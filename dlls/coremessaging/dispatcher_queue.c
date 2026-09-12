/* Windows.System dispatcher queues.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */
#include "private.h"
#include "dispatcherqueue.h"
#include "winuser.h"
#include "wine/list.h"

WINE_DEFAULT_DEBUG_CHANNEL(messaging);

#define WM_DISPATCH_QUEUE (WM_USER + 1)

struct queue_callback
{
    struct list entry;
    IDispatcherQueueHandler *handler;
    DispatcherQueuePriority priority;
};

struct queue_event
{
    struct list entry;
    ITypedEventHandler_IInspectable_IInspectable *handler;
    EventRegistrationToken token;
};

struct dispatcher_queue_controller;
struct dispatcher_queue
{
    IDispatcherQueue IDispatcherQueue_iface;
    IDispatcherQueue2 IDispatcherQueue2_iface;
    LONG ref;
    CRITICAL_SECTION cs;
    struct list registry_entry, callbacks, starting_handlers, completed_handlers;
    struct dispatcher_queue_controller *controller;
    IAsyncAction *shutdown;
    HWND window;
    DWORD thread_id;
    enum { QUEUE_RUNNING, QUEUE_DRAINING, QUEUE_STARTING, QUEUE_STOPPED } state;
    unsigned int deferrals;
    BOOL dedicated, first_callback, dispatching, posted, window_ref;
};

struct dispatcher_queue_controller
{
    IDispatcherQueueController IDispatcherQueueController_iface;
    LONG ref;
    struct dispatcher_queue *queue;
};

static SRWLOCK registry_lock = SRWLOCK_INIT;
static struct list queues = LIST_INIT(queues);
static LONG64 next_token;
static INIT_ONCE window_class_once = INIT_ONCE_STATIC_INIT;

static struct dispatcher_queue *impl_from_IDispatcherQueue( IDispatcherQueue *iface )
{
    return CONTAINING_RECORD( iface, struct dispatcher_queue, IDispatcherQueue_iface );
}

static struct dispatcher_queue_controller *impl_from_IDispatcherQueueController( IDispatcherQueueController *iface )
{
    return CONTAINING_RECORD( iface, struct dispatcher_queue_controller, IDispatcherQueueController_iface );
}

static HRESULT copy_iids( const IID *ids, unsigned int count, ULONG *iid_count, IID **iids )
{
    if (!iid_count || !iids) return E_POINTER;
    *iid_count = 0;
    if (!(*iids = CoTaskMemAlloc( count * sizeof(**iids) ))) return E_OUTOFMEMORY;
    memcpy( *iids, ids, count * sizeof(**iids) );
    *iid_count = count;
    return S_OK;
}

static HRESULT get_class_name( const WCHAR *name, HSTRING *value )
{
    if (!value) return E_POINTER;
    return WindowsCreateString( name, wcslen(name), value );
}

static HRESULT get_trust_level( TrustLevel *value )
{
    if (!value) return E_POINTER;
    *value = BaseTrust;
    return S_OK;
}

/* The message window owns a controller reference. This keeps an orphaned queue
 * alive until its owning thread exits without inflating the queue's COM count. */
static HRESULT WINAPI queue_QueryInterface( IDispatcherQueue *iface, REFIID iid, void **out )
{
    struct dispatcher_queue *queue = impl_from_IDispatcherQueue( iface );
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable )
            || IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IDispatcherQueue ))
        *out = iface;
    else if (IsEqualGUID( iid, &IID_IDispatcherQueue2 )) *out = &queue->IDispatcherQueue2_iface;
    else return E_NOINTERFACE;
    IDispatcherQueue_AddRef( iface );
    return S_OK;
}

static ULONG WINAPI queue_AddRef( IDispatcherQueue *iface )
{
    return InterlockedIncrement( &impl_from_IDispatcherQueue( iface )->ref );
}

static void clear_events( struct list *events )
{
    struct queue_event *event, *next;
    LIST_FOR_EACH_ENTRY_SAFE( event, next, events, struct queue_event, entry )
    {
        list_remove( &event->entry );
        ITypedEventHandler_IInspectable_IInspectable_Release( event->handler );
        free( event );
    }
}

static ULONG WINAPI queue_Release( IDispatcherQueue *iface )
{
    struct dispatcher_queue *queue = impl_from_IDispatcherQueue( iface );
    ULONG ref = InterlockedDecrement( &queue->ref );
    if (!ref)
    {
        clear_events( &queue->starting_handlers );
        clear_events( &queue->completed_handlers );
        if (queue->shutdown) IAsyncAction_Release( queue->shutdown );
        DeleteCriticalSection( &queue->cs );
        free( queue );
    }
    return ref;
}

static HRESULT WINAPI queue_GetIids( IDispatcherQueue *iface, ULONG *count, IID **iids )
{
    const IID ids[] = {IID_IDispatcherQueue, IID_IDispatcherQueue2};
    return copy_iids( ids, ARRAY_SIZE(ids), count, iids );
}

static HRESULT WINAPI queue_GetRuntimeClassName( IDispatcherQueue *iface, HSTRING *value )
{
    return get_class_name( RuntimeClass_Windows_System_DispatcherQueue, value );
}

static HRESULT WINAPI queue_GetTrustLevel( IDispatcherQueue *iface, TrustLevel *value )
{
    return get_trust_level( value );
}

static HRESULT WINAPI queue_CreateTimer( IDispatcherQueue *iface, IDispatcherQueueTimer **result )
{
    FIXME( "iface %p, result %p stub!\n", iface, result );
    if (!result) return E_POINTER;
    *result = NULL;
    return E_NOTIMPL;
}

/* Called with the queue lock held. Only a single wakeup is needed even when
 * producers enqueue concurrently or a callback runs a nested message loop. */
static BOOL queue_wake( struct dispatcher_queue *queue )
{
    if (queue->posted || queue->dispatching) return TRUE;
    if (!queue->window || !PostMessageW( queue->window, WM_DISPATCH_QUEUE, 0, 0 )) return FALSE;
    queue->posted = TRUE;
    return TRUE;
}

static HRESULT WINAPI queue_TryEnqueueWithPriority( IDispatcherQueue *iface, DispatcherQueuePriority priority,
        IDispatcherQueueHandler *handler, boolean *result )
{
    struct dispatcher_queue *queue = impl_from_IDispatcherQueue( iface );
    struct queue_callback *callback;
    HRESULT hr = S_OK;

    TRACE( "iface %p, priority %d, handler %p, result %p.\n", iface, priority, handler, result );
    if (!result) return E_POINTER;
    *result = FALSE;
    if (!handler) return E_INVALIDARG;
    if (priority != DispatcherQueuePriority_Low && priority != DispatcherQueuePriority_Normal
            && priority != DispatcherQueuePriority_High) return E_INVALIDARG;
    if (!(callback = malloc( sizeof(*callback) ))) return E_OUTOFMEMORY;
    callback->handler = handler;
    callback->priority = priority;
    IDispatcherQueueHandler_AddRef( handler );
    EnterCriticalSection( &queue->cs );
    if (queue->state != QUEUE_STOPPED)
    {
        list_add_tail( &queue->callbacks, &callback->entry );
        if (!queue_wake( queue ))
        {
            list_remove( &callback->entry );
            hr = HRESULT_FROM_WIN32( GetLastError() );
        }
        else *result = TRUE;
    }
    LeaveCriticalSection( &queue->cs );
    if (!*result)
    {
        IDispatcherQueueHandler_Release( handler );
        free( callback );
    }
    return hr;
}

static HRESULT WINAPI queue_TryEnqueue( IDispatcherQueue *iface, IDispatcherQueueHandler *handler, boolean *result )
{
    return queue_TryEnqueueWithPriority( iface, DispatcherQueuePriority_Normal, handler, result );
}

static HRESULT add_event( struct dispatcher_queue *queue, struct list *events,
        ITypedEventHandler_IInspectable_IInspectable *handler, EventRegistrationToken *token )
{
    struct queue_event *event;
    if (!token) return E_POINTER;
    token->value = 0;
    if (!handler) return E_INVALIDARG;
    if (!(event = malloc( sizeof(*event) ))) return E_OUTOFMEMORY;
    event->handler = handler;
    event->token.value = InterlockedIncrement64( &next_token );
    ITypedEventHandler_IInspectable_IInspectable_AddRef( handler );
    EnterCriticalSection( &queue->cs );
    list_add_tail( events, &event->entry );
    *token = event->token;
    LeaveCriticalSection( &queue->cs );
    return S_OK;
}

static HRESULT remove_event( struct dispatcher_queue *queue, struct list *events, EventRegistrationToken token )
{
    struct queue_event *event, *found = NULL;
    EnterCriticalSection( &queue->cs );
    LIST_FOR_EACH_ENTRY( event, events, struct queue_event, entry )
        if (event->token.value == token.value) { found = event; list_remove( &event->entry ); break; }
    LeaveCriticalSection( &queue->cs );
    if (found)
    {
        ITypedEventHandler_IInspectable_IInspectable_Release( found->handler );
        free( found );
    }
    return S_OK;
}

static HRESULT notify_events( struct dispatcher_queue *queue, struct list *events, IInspectable *args )
{
    struct list copies = LIST_INIT(copies);
    struct queue_event *event, *copy;
    HRESULT hr = S_OK;
    EnterCriticalSection( &queue->cs );
    LIST_FOR_EACH_ENTRY( event, events, struct queue_event, entry )
    {
        if (!(copy = malloc( sizeof(*copy) ))) { hr = E_OUTOFMEMORY; break; }
        copy->handler = event->handler;
        ITypedEventHandler_IInspectable_IInspectable_AddRef( copy->handler );
        list_add_tail( &copies, &copy->entry );
    }
    LeaveCriticalSection( &queue->cs );
    if (SUCCEEDED(hr)) LIST_FOR_EACH_ENTRY( copy, &copies, struct queue_event, entry )
        ITypedEventHandler_IInspectable_IInspectable_Invoke( copy->handler,
                (IInspectable *)&queue->IDispatcherQueue_iface, args );
    clear_events( &copies );
    return hr;
}

static HRESULT WINAPI queue_add_ShutdownStarting( IDispatcherQueue *iface,
        ITypedEventHandler_DispatcherQueue_DispatcherQueueShutdownStartingEventArgs *handler, EventRegistrationToken *token )
{
    struct dispatcher_queue *queue = impl_from_IDispatcherQueue( iface );
    return add_event( queue, &queue->starting_handlers, (ITypedEventHandler_IInspectable_IInspectable *)handler, token );
}

static HRESULT WINAPI queue_remove_ShutdownStarting( IDispatcherQueue *iface, EventRegistrationToken token )
{
    struct dispatcher_queue *queue = impl_from_IDispatcherQueue( iface );
    return remove_event( queue, &queue->starting_handlers, token );
}

static HRESULT WINAPI queue_add_ShutdownCompleted( IDispatcherQueue *iface,
        ITypedEventHandler_DispatcherQueue_IInspectable *handler, EventRegistrationToken *token )
{
    struct dispatcher_queue *queue = impl_from_IDispatcherQueue( iface );
    return add_event( queue, &queue->completed_handlers, (ITypedEventHandler_IInspectable_IInspectable *)handler, token );
}

static HRESULT WINAPI queue_remove_ShutdownCompleted( IDispatcherQueue *iface, EventRegistrationToken token )
{
    struct dispatcher_queue *queue = impl_from_IDispatcherQueue( iface );
    return remove_event( queue, &queue->completed_handlers, token );
}

static const IDispatcherQueueVtbl queue_vtbl =
{
    queue_QueryInterface, queue_AddRef, queue_Release,
    queue_GetIids, queue_GetRuntimeClassName, queue_GetTrustLevel,
    queue_CreateTimer, queue_TryEnqueue, queue_TryEnqueueWithPriority,
    queue_add_ShutdownStarting, queue_remove_ShutdownStarting,
    queue_add_ShutdownCompleted, queue_remove_ShutdownCompleted,
};

DEFINE_IINSPECTABLE( queue2, IDispatcherQueue2, struct dispatcher_queue, IDispatcherQueue_iface )

static HRESULT WINAPI queue2_get_HasThreadAccess( IDispatcherQueue2 *iface, boolean *value )
{
    struct dispatcher_queue *queue = impl_from_IDispatcherQueue2( iface );
    if (!value) return E_POINTER;
    *value = GetCurrentThreadId() == queue->thread_id;
    return S_OK;
}

static const IDispatcherQueue2Vtbl queue2_vtbl =
{
    queue2_QueryInterface, queue2_AddRef, queue2_Release,
    queue2_GetIids, queue2_GetRuntimeClassName, queue2_GetTrustLevel,
    queue2_get_HasThreadAccess,
};

struct shutdown_args
{
    IDispatcherQueueShutdownStartingEventArgs IDispatcherQueueShutdownStartingEventArgs_iface;
    LONG ref;
    struct dispatcher_queue *queue;
};

struct queue_deferral
{
    IDeferral IDeferral_iface;
    LONG ref, completed;
    struct dispatcher_queue *queue;
};

static struct shutdown_args *impl_from_shutdown_args( IDispatcherQueueShutdownStartingEventArgs *iface )
{
    return CONTAINING_RECORD( iface, struct shutdown_args, IDispatcherQueueShutdownStartingEventArgs_iface );
}

static struct queue_deferral *impl_from_deferral( IDeferral *iface )
{
    return CONTAINING_RECORD( iface, struct queue_deferral, IDeferral_iface );
}

static HRESULT WINAPI deferral_QueryInterface( IDeferral *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID( iid, &IID_IUnknown ) && !IsEqualGUID( iid, &IID_IInspectable )
            && !IsEqualGUID( iid, &IID_IAgileObject ) && !IsEqualGUID( iid, &IID_IDeferral )) return E_NOINTERFACE;
    *out = iface;
    IDeferral_AddRef( iface );
    return S_OK;
}

static ULONG WINAPI deferral_AddRef( IDeferral *iface )
{
    return InterlockedIncrement( &impl_from_deferral( iface )->ref );
}

static ULONG WINAPI deferral_Release( IDeferral *iface )
{
    struct queue_deferral *deferral = impl_from_deferral( iface );
    ULONG ref = InterlockedDecrement( &deferral->ref );
    if (!ref)
    {
        IDispatcherQueue_Release( &deferral->queue->IDispatcherQueue_iface );
        free( deferral );
    }
    return ref;
}

static HRESULT WINAPI deferral_GetIids( IDeferral *iface, ULONG *count, IID **iids )
{
    return copy_iids( &IID_IDeferral, 1, count, iids );
}

static HRESULT WINAPI deferral_GetRuntimeClassName( IDeferral *iface, HSTRING *value )
{
    return get_class_name( L"Windows.Foundation.Deferral", value );
}

static HRESULT WINAPI deferral_GetTrustLevel( IDeferral *iface, TrustLevel *value )
{
    return get_trust_level( value );
}

static HRESULT WINAPI deferral_Complete( IDeferral *iface )
{
    struct queue_deferral *deferral = impl_from_deferral( iface );
    struct dispatcher_queue *queue = deferral->queue;
    if (!InterlockedExchange( &deferral->completed, TRUE ))
    {
        EnterCriticalSection( &queue->cs );
        --queue->deferrals;
        if (!queue->deferrals && queue->state == QUEUE_STARTING) queue_wake( queue );
        LeaveCriticalSection( &queue->cs );
    }
    return S_OK;
}

static const IDeferralVtbl deferral_vtbl =
{
    deferral_QueryInterface, deferral_AddRef, deferral_Release,
    deferral_GetIids, deferral_GetRuntimeClassName, deferral_GetTrustLevel, deferral_Complete,
};

static HRESULT WINAPI shutdown_args_QueryInterface( IDispatcherQueueShutdownStartingEventArgs *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID( iid, &IID_IUnknown ) && !IsEqualGUID( iid, &IID_IInspectable )
            && !IsEqualGUID( iid, &IID_IAgileObject )
            && !IsEqualGUID( iid, &IID_IDispatcherQueueShutdownStartingEventArgs )) return E_NOINTERFACE;
    *out = iface;
    IDispatcherQueueShutdownStartingEventArgs_AddRef( iface );
    return S_OK;
}

static ULONG WINAPI shutdown_args_AddRef( IDispatcherQueueShutdownStartingEventArgs *iface )
{
    return InterlockedIncrement( &impl_from_shutdown_args( iface )->ref );
}

static ULONG WINAPI shutdown_args_Release( IDispatcherQueueShutdownStartingEventArgs *iface )
{
    struct shutdown_args *args = impl_from_shutdown_args( iface );
    ULONG ref = InterlockedDecrement( &args->ref );
    if (!ref)
    {
        IDispatcherQueue_Release( &args->queue->IDispatcherQueue_iface );
        free( args );
    }
    return ref;
}

static HRESULT WINAPI shutdown_args_GetIids( IDispatcherQueueShutdownStartingEventArgs *iface, ULONG *count, IID **iids )
{
    return copy_iids( &IID_IDispatcherQueueShutdownStartingEventArgs, 1, count, iids );
}

static HRESULT WINAPI shutdown_args_GetRuntimeClassName( IDispatcherQueueShutdownStartingEventArgs *iface, HSTRING *value )
{
    return get_class_name( RuntimeClass_Windows_System_DispatcherQueueShutdownStartingEventArgs, value );
}

static HRESULT WINAPI shutdown_args_GetTrustLevel( IDispatcherQueueShutdownStartingEventArgs *iface, TrustLevel *value )
{
    return get_trust_level( value );
}

static HRESULT WINAPI shutdown_args_GetDeferral( IDispatcherQueueShutdownStartingEventArgs *iface, IDeferral **result )
{
    struct dispatcher_queue *queue = impl_from_shutdown_args( iface )->queue;
    struct queue_deferral *deferral;
    if (!result) return E_POINTER;
    *result = NULL;
    if (!(deferral = calloc( 1, sizeof(*deferral) ))) return E_OUTOFMEMORY;
    EnterCriticalSection( &queue->cs );
    if (queue->state != QUEUE_STARTING)
    {
        LeaveCriticalSection( &queue->cs );
        free( deferral );
        return E_ILLEGAL_METHOD_CALL;
    }
    ++queue->deferrals;
    LeaveCriticalSection( &queue->cs );
    deferral->IDeferral_iface.lpVtbl = &deferral_vtbl;
    deferral->ref = 1;
    deferral->queue = queue;
    IDispatcherQueue_AddRef( &queue->IDispatcherQueue_iface );
    *result = &deferral->IDeferral_iface;
    return S_OK;
}

static const IDispatcherQueueShutdownStartingEventArgsVtbl shutdown_args_vtbl =
{
    shutdown_args_QueryInterface, shutdown_args_AddRef, shutdown_args_Release,
    shutdown_args_GetIids, shutdown_args_GetRuntimeClassName, shutdown_args_GetTrustLevel,
    shutdown_args_GetDeferral,
};

static void queue_finish( struct dispatcher_queue *queue, HRESULT hr )
{
    EnterCriticalSection( &queue->cs );
    queue->state = QUEUE_STOPPED;
    LeaveCriticalSection( &queue->cs );
    notify_events( queue, &queue->completed_handlers, NULL );
    if (queue->shutdown) async_action_complete( queue->shutdown, hr );
    DestroyWindow( queue->window );
    if (queue->dedicated) PostQuitMessage( 0 );
}

static void queue_dispatch( struct dispatcher_queue *queue )
{
    struct queue_callback *callback = NULL, *candidate;
    struct shutdown_args *args;
    HRESULT hr;

    IDispatcherQueue_AddRef( &queue->IDispatcherQueue_iface );
    EnterCriticalSection( &queue->cs );
    queue->posted = FALSE;
    if (queue->dispatching || queue->state == QUEUE_STOPPED) goto done;
    queue->dispatching = TRUE;
    LIST_FOR_EACH_ENTRY( candidate, &queue->callbacks, struct queue_callback, entry )
    {
        if (!callback || candidate->priority > callback->priority) callback = candidate;
        if (queue->dedicated && queue->first_callback) break;
    }
    if (callback && callback->handler)
    {
        list_remove( &callback->entry );
        queue->first_callback = FALSE;
        LeaveCriticalSection( &queue->cs );
        IDispatcherQueueHandler_Invoke( callback->handler );
        IDispatcherQueueHandler_Release( callback->handler );
        free( callback );
        EnterCriticalSection( &queue->cs );
    }
    else if (callback)
    {
        list_remove( &callback->entry );
        free( callback );
        queue->state = QUEUE_STARTING;
        ++queue->deferrals; /* Guard against an inline Complete() inside an event. */
        LeaveCriticalSection( &queue->cs );
        if ((args = calloc( 1, sizeof(*args) )))
        {
            args->IDispatcherQueueShutdownStartingEventArgs_iface.lpVtbl = &shutdown_args_vtbl;
            args->ref = 1;
            args->queue = queue;
            IDispatcherQueue_AddRef( &queue->IDispatcherQueue_iface );
            hr = notify_events( queue, &queue->starting_handlers,
                    (IInspectable *)&args->IDispatcherQueueShutdownStartingEventArgs_iface );
            IDispatcherQueueShutdownStartingEventArgs_Release( &args->IDispatcherQueueShutdownStartingEventArgs_iface );
        }
        else hr = E_OUTOFMEMORY;
        if (FAILED(hr)) queue_finish( queue, hr );
        EnterCriticalSection( &queue->cs );
        --queue->deferrals;
    }
    else if (queue->state == QUEUE_STARTING && !queue->deferrals)
    {
        /* Stop accepting work atomically with observing an empty queue. */
        queue->state = QUEUE_STOPPED;
        LeaveCriticalSection( &queue->cs );
        queue_finish( queue, S_OK );
        EnterCriticalSection( &queue->cs );
    }
    queue->dispatching = FALSE;
    if (queue->state != QUEUE_STOPPED && (!list_empty( &queue->callbacks ) || queue->state == QUEUE_DRAINING
            || (queue->state == QUEUE_STARTING && !queue->deferrals))) queue_wake( queue );
 done:
    LeaveCriticalSection( &queue->cs );
    IDispatcherQueue_Release( &queue->IDispatcherQueue_iface );
}

static LRESULT CALLBACK queue_window_proc( HWND window, UINT message, WPARAM wparam, LPARAM lparam )
{
    struct dispatcher_queue *queue = (void *)GetWindowLongPtrW( window, GWLP_USERDATA );
    if (message == WM_NCCREATE)
    {
        queue = ((CREATESTRUCTW *)lparam)->lpCreateParams;
        SetWindowLongPtrW( window, GWLP_USERDATA, (LONG_PTR)queue );
    }
    else if (message == WM_DISPATCH_QUEUE && queue) queue_dispatch( queue );
    else if (message == WM_NCDESTROY && queue)
    {
        struct queue_callback *callback, *next;
        struct list abandoned = LIST_INIT(abandoned);
        AcquireSRWLockExclusive( &registry_lock );
        list_remove( &queue->registry_entry );
        ReleaseSRWLockExclusive( &registry_lock );
        EnterCriticalSection( &queue->cs );
        queue->window = NULL;
        queue->state = QUEUE_STOPPED;
        list_move_tail( &abandoned, &queue->callbacks );
        LeaveCriticalSection( &queue->cs );
        LIST_FOR_EACH_ENTRY_SAFE( callback, next, &abandoned, struct queue_callback, entry )
        {
            list_remove( &callback->entry );
            if (callback->handler) IDispatcherQueueHandler_Release( callback->handler );
            free( callback );
        }
        SetWindowLongPtrW( window, GWLP_USERDATA, 0 );
        queue->window_ref = FALSE;
        IDispatcherQueueController_Release( &queue->controller->IDispatcherQueueController_iface );
    }
    return DefWindowProcW( window, message, wparam, lparam );
}

static BOOL CALLBACK register_queue_window( INIT_ONCE *once, void *param, void **context )
{
    WNDCLASSW cls = {0};
    cls.lpfnWndProc = queue_window_proc;
    cls.hInstance = GetModuleHandleW( L"coremessaging.dll" );
    cls.lpszClassName = L"WineDispatcherQueue";
    return RegisterClassW( &cls ) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

static HRESULT queue_create_window( struct dispatcher_queue *queue )
{
    if (!InitOnceExecuteOnce( &window_class_once, register_queue_window, NULL, NULL ))
        return HRESULT_FROM_WIN32( GetLastError() );
    queue->thread_id = GetCurrentThreadId();
    queue->window_ref = TRUE;
    IDispatcherQueueController_AddRef( &queue->controller->IDispatcherQueueController_iface );
    queue->window = CreateWindowExW( 0, L"WineDispatcherQueue", NULL, 0, 0, 0, 0, 0,
            HWND_MESSAGE, NULL, GetModuleHandleW( L"coremessaging.dll" ), queue );
    if (!queue->window)
    {
        if (queue->window_ref)
        {
            queue->window_ref = FALSE;
            IDispatcherQueueController_Release( &queue->controller->IDispatcherQueueController_iface );
        }
        return HRESULT_FROM_WIN32( GetLastError() );
    }
    AcquireSRWLockExclusive( &registry_lock );
    list_add_tail( &queues, &queue->registry_entry );
    ReleaseSRWLockExclusive( &registry_lock );
    return S_OK;
}

HRESULT dispatcher_queue_get_for_current_thread( IDispatcherQueue **result )
{
    struct dispatcher_queue *queue;
    if (!result) return E_POINTER;
    *result = NULL;
    AcquireSRWLockShared( &registry_lock );
    LIST_FOR_EACH_ENTRY( queue, &queues, struct dispatcher_queue, registry_entry )
    {
        if (queue->thread_id != GetCurrentThreadId()) continue;
        IDispatcherQueue_AddRef( (*result = &queue->IDispatcherQueue_iface) );
        break;
    }
    ReleaseSRWLockShared( &registry_lock );
    return S_OK;
}

static HRESULT WINAPI controller_QueryInterface( IDispatcherQueueController *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID( iid, &IID_IUnknown ) && !IsEqualGUID( iid, &IID_IInspectable )
            && !IsEqualGUID( iid, &IID_IAgileObject ) && !IsEqualGUID( iid, &IID_IDispatcherQueueController )) return E_NOINTERFACE;
    *out = iface;
    IDispatcherQueueController_AddRef( iface );
    return S_OK;
}

static ULONG WINAPI controller_AddRef( IDispatcherQueueController *iface )
{
    return InterlockedIncrement( &impl_from_IDispatcherQueueController( iface )->ref );
}

static ULONG WINAPI controller_Release( IDispatcherQueueController *iface )
{
    struct dispatcher_queue_controller *controller = impl_from_IDispatcherQueueController( iface );
    ULONG ref = InterlockedDecrement( &controller->ref );
    if (!ref)
    {
        IDispatcherQueue_Release( &controller->queue->IDispatcherQueue_iface );
        free( controller );
    }
    return ref;
}

static HRESULT WINAPI controller_GetIids( IDispatcherQueueController *iface, ULONG *count, IID **iids )
{
    return copy_iids( &IID_IDispatcherQueueController, 1, count, iids );
}

static HRESULT WINAPI controller_GetRuntimeClassName( IDispatcherQueueController *iface, HSTRING *value )
{
    return get_class_name( RuntimeClass_Windows_System_DispatcherQueueController, value );
}

static HRESULT WINAPI controller_GetTrustLevel( IDispatcherQueueController *iface, TrustLevel *value )
{
    return get_trust_level( value );
}

static HRESULT WINAPI controller_get_DispatcherQueue( IDispatcherQueueController *iface, IDispatcherQueue **value )
{
    struct dispatcher_queue *queue = impl_from_IDispatcherQueueController( iface )->queue;
    if (!value) return E_POINTER;
    IDispatcherQueue_AddRef( (*value = &queue->IDispatcherQueue_iface) );
    return S_OK;
}

static HRESULT WINAPI controller_ShutdownQueueAsync( IDispatcherQueueController *iface, IAsyncAction **operation )
{
    struct dispatcher_queue *queue = impl_from_IDispatcherQueueController( iface )->queue;
    struct queue_callback *marker;
    HRESULT hr = S_OK;
    if (!operation) return E_POINTER;
    *operation = NULL;
    if (!(marker = calloc( 1, sizeof(*marker) ))) return E_OUTOFMEMORY;
    marker->priority = DispatcherQueuePriority_High;
    EnterCriticalSection( &queue->cs );
    if (queue->state != QUEUE_RUNNING) hr = E_UNEXPECTED;
    else if (SUCCEEDED(hr = async_action_create( NULL, NULL, &queue->shutdown )))
    {
        queue->state = QUEUE_DRAINING;
        /* Windows queues ShutdownStarting after existing high-priority work,
         * ahead of pending normal/low callbacks, and drains everything after it. */
        list_add_tail( &queue->callbacks, &marker->entry );
        if (!queue_wake( queue ))
        {
            list_remove( &marker->entry );
            hr = HRESULT_FROM_WIN32( GetLastError() );
            queue->state = QUEUE_RUNNING;
            IAsyncAction_Release( queue->shutdown );
            queue->shutdown = NULL;
        }
        else
        {
            marker = NULL;
            IAsyncAction_AddRef( (*operation = queue->shutdown) );
        }
    }
    LeaveCriticalSection( &queue->cs );
    free( marker );
    return hr;
}

static const IDispatcherQueueControllerVtbl controller_vtbl =
{
    controller_QueryInterface, controller_AddRef, controller_Release,
    controller_GetIids, controller_GetRuntimeClassName, controller_GetTrustLevel,
    controller_get_DispatcherQueue, controller_ShutdownQueueAsync,
};

struct queue_thread_start
{
    struct dispatcher_queue *queue;
    HANDLE ready;
    HRESULT hr;
};

static DWORD WINAPI queue_thread_proc( void *param )
{
    struct queue_thread_start *start = param;
    struct dispatcher_queue *queue = start->queue;
    HRESULT hr;
    MSG message;

    IDispatcherQueue_AddRef( &queue->IDispatcherQueue_iface );
    hr = RoInitialize( RO_INIT_SINGLETHREADED );
    start->hr = SUCCEEDED(hr) ? queue_create_window( queue ) : hr;
    SetEvent( start->ready );
    if (queue->window)
    {
        while (GetMessageW( &message, NULL, 0, 0 ) > 0)
        {
            TranslateMessage( &message );
            DispatchMessageW( &message );
        }
        if (queue->window) DestroyWindow( queue->window );
    }
    if (SUCCEEDED(hr)) RoUninitialize();
    IDispatcherQueue_Release( &queue->IDispatcherQueue_iface );
    return 0;
}

HRESULT WINAPI CreateDispatcherQueueController( DispatcherQueueOptions options, PDISPATCHERQUEUECONTROLLER *result )
{
    struct dispatcher_queue_controller *controller;
    struct dispatcher_queue *queue;
    struct queue_thread_start start;
    IDispatcherQueue *existing;
    HANDLE thread;
    HRESULT hr;

    TRACE( "size %lu, thread type %d, apartment type %d, result %p.\n",
            options.dwSize, options.threadType, options.apartmentType, result );
    if (!result) return E_POINTER;
    if (options.dwSize != sizeof(options)) return E_INVALIDARG;
    if (options.threadType != DQTYPE_THREAD_DEDICATED && options.threadType != DQTYPE_THREAD_CURRENT) return E_INVALIDARG;
    *result = NULL;
    if (options.threadType == DQTYPE_THREAD_CURRENT)
    {
        dispatcher_queue_get_for_current_thread( &existing );
        if (existing)
        {
            IDispatcherQueue_Release( existing );
            return RPC_E_WRONG_THREAD;
        }
    }
    else if (options.apartmentType != DQTAT_COM_STA && options.apartmentType != DQTAT_COM_ASTA) return E_INVALIDARG;
    if (!(controller = calloc( 1, sizeof(*controller) ))) return E_OUTOFMEMORY;
    if (!(queue = calloc( 1, sizeof(*queue) ))) { free( controller ); return E_OUTOFMEMORY; }
    controller->IDispatcherQueueController_iface.lpVtbl = &controller_vtbl;
    controller->ref = 1;
    controller->queue = queue;
    queue->IDispatcherQueue_iface.lpVtbl = &queue_vtbl;
    queue->IDispatcherQueue2_iface.lpVtbl = &queue2_vtbl;
    queue->ref = 1;
    queue->controller = controller;
    queue->dedicated = options.threadType == DQTYPE_THREAD_DEDICATED;
    queue->first_callback = TRUE;
    InitializeCriticalSection( &queue->cs );
    list_init( &queue->registry_entry );
    list_init( &queue->callbacks );
    list_init( &queue->starting_handlers );
    list_init( &queue->completed_handlers );
    if (!queue->dedicated) hr = queue_create_window( queue );
    else
    {
        start.queue = queue;
        if (!(start.ready = CreateEventW( NULL, TRUE, FALSE, NULL ))) hr = HRESULT_FROM_WIN32( GetLastError() );
        else
        {
            if (!(thread = CreateThread( NULL, 0, queue_thread_proc, &start, 0, NULL ))) hr = HRESULT_FROM_WIN32( GetLastError() );
            else
            {
                WaitForSingleObject( start.ready, INFINITE );
                hr = start.hr;
                CloseHandle( thread );
            }
            CloseHandle( start.ready );
        }
    }
    if (FAILED(hr)) IDispatcherQueueController_Release( &controller->IDispatcherQueueController_iface );
    else *result = &controller->IDispatcherQueueController_iface;
    return hr;
}
