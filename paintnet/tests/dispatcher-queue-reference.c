/* Native Windows reference for dispatcher queue identity, ordering and shutdown.
 * This file is free software under the GNU LGPL, version 2.1 or later. */
#define COBJMACROS
#include <stdio.h>
#include "initguid.h"
#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winstring.h"
#include "roapi.h"
#define WIDL_using_Windows_Foundation
#include "windows.foundation.h"
#define WIDL_using_Windows_System
#include "windows.system.h"
#include "dispatcherqueue.h"

static IDispatcherQueue *queue;
static IDispatcherQueueStatics *statics;
static IAsyncInfo *shutdown_info;
static DWORD owner;
static unsigned int calls;
static IDeferral *deferral;
struct callback { IDispatcherQueueHandler iface; LONG ref; int id; };
static HRESULT WINAPI callback_qi(IDispatcherQueueHandler *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IAgileObject)
            || IsEqualGUID(iid, &IID_IDispatcherQueueHandler))
    { *out = iface; IDispatcherQueueHandler_AddRef(iface); return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI callback_addref(IDispatcherQueueHandler *iface)
{ return InterlockedIncrement(&CONTAINING_RECORD(iface, struct callback, iface)->ref); }
static ULONG WINAPI callback_release(IDispatcherQueueHandler *iface)
{ return InterlockedDecrement(&CONTAINING_RECORD(iface, struct callback, iface)->ref); }
static HRESULT WINAPI callback_invoke(IDispatcherQueueHandler *iface)
{
    struct callback *cb = CONTAINING_RECORD(iface, struct callback, iface);
    IDispatcherQueue *current = NULL;
    HRESULT hr = IDispatcherQueueStatics_GetForCurrentThread(statics, &current);
    printf("CALL %d thread=%d current=%08lx same=%d\n", cb->id,
            GetCurrentThreadId() == owner, hr, current == queue);
    if (current) IDispatcherQueue_Release(current);
    ++calls;
    return S_OK;
}
static const IDispatcherQueueHandlerVtbl callback_vtbl =
{callback_qi, callback_addref, callback_release, callback_invoke};

struct event_handler { ITypedEventHandler_DispatcherQueue_IInspectable iface; LONG ref; BOOL starting; };
static struct callback extra = {{&callback_vtbl}, 1, 10};
static struct callback cross_thread = {{&callback_vtbl}, 1, 11};
static HRESULT WINAPI event_qi(ITypedEventHandler_DispatcherQueue_IInspectable *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IAgileObject)
            || IsEqualGUID(iid, &IID_ITypedEventHandler_DispatcherQueue_IInspectable)
            || IsEqualGUID(iid, &IID_ITypedEventHandler_DispatcherQueue_DispatcherQueueShutdownStartingEventArgs))
    { *out = iface; ITypedEventHandler_DispatcherQueue_IInspectable_AddRef(iface); return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI event_addref(ITypedEventHandler_DispatcherQueue_IInspectable *iface)
{ return InterlockedIncrement(&CONTAINING_RECORD(iface, struct event_handler, iface)->ref); }
static ULONG WINAPI event_release(ITypedEventHandler_DispatcherQueue_IInspectable *iface)
{ return InterlockedDecrement(&CONTAINING_RECORD(iface, struct event_handler, iface)->ref); }
static HRESULT WINAPI event_invoke(ITypedEventHandler_DispatcherQueue_IInspectable *iface,
        IDispatcherQueue *sender, IInspectable *args)
{
    struct event_handler *event = CONTAINING_RECORD(iface, struct event_handler, iface);
    IDispatcherQueue *current = NULL;
    AsyncStatus status = -1;
    boolean accepted;
    if (event->starting)
    {
        HRESULT hr = IDispatcherQueueShutdownStartingEventArgs_GetDeferral(
                (IDispatcherQueueShutdownStartingEventArgs *)args, &deferral);
        printf("STARTING thread=%d same=%d deferral=%08lx present=%d\n",
                GetCurrentThreadId() == owner, sender == queue, hr, deferral != NULL);
        hr = IDispatcherQueue_TryEnqueue(queue, &extra.iface, &accepted);
        printf("STARTING_ENQUEUE %08lx accepted=%d\n", hr, accepted);
        return S_OK;
    }
    HRESULT hr = IDispatcherQueueStatics_GetForCurrentThread(statics, &current);
    if (shutdown_info) IAsyncInfo_get_Status(shutdown_info, &status);
    printf("COMPLETED thread=%d same=%d args=%d current=%08lx present=%d status=%d calls=%u\n",
            GetCurrentThreadId() == owner, sender == queue, args != NULL, hr,
            current != NULL, status, calls);
    if (current) IDispatcherQueue_Release(current);
    return S_OK;
}
static const ITypedEventHandler_DispatcherQueue_IInspectableVtbl event_vtbl =
{event_qi, event_addref, event_release, event_invoke};

static DWORD WINAPI finish_deferral(void *unused)
{
    IDispatcherQueue *current = NULL;
    boolean accepted;
    HRESULT hr = IDispatcherQueueStatics_GetForCurrentThread(statics, &current);
    printf("OTHER_THREAD current=%08lx present=%d\n", hr, current != NULL);
    if (current) IDispatcherQueue_Release(current);
    hr = IDispatcherQueue_TryEnqueue(queue, &cross_thread.iface, &accepted);
    printf("OTHER_ENQUEUE %08lx accepted=%d\n", hr, accepted);
    hr = IDeferral_Complete(deferral);
    printf("DEFERRAL_COMPLETE %08lx\n", hr);
    return 0;
}

int main(void)
{
    const int priorities[] = {0, -10, 10, 0, 10, -10, -1, 1, -11, 11};
    struct callback callbacks[ARRAY_SIZE(priorities)];
    struct event_handler event = {{&event_vtbl}, 1, FALSE};
    struct event_handler starting = {{&event_vtbl}, 1, TRUE};
    BOOL test_deferral = getenv("QUEUE_DEFERRAL") != NULL, deferral_completed = FALSE;
    DispatcherQueueOptions options = {sizeof(options), DQTYPE_THREAD_CURRENT, DQTAT_COM_NONE};
    IDispatcherQueueController *controller = NULL, *duplicate = (void *)0xdeadbeef;
    IDispatcherQueue *current = (void *)0xdeadbeef;
    IAsyncAction *action = NULL, *second = NULL;
    EventRegistrationToken token, starting_token;
    AsyncStatus status = Started;
    HSTRING name;
    HRESULT hr;
    boolean accepted;
    unsigned int i;
    DWORD end;
    HANDLE thread;
    MSG msg;

    setvbuf(stdout, NULL, _IOFBF, 65536);
    owner = GetCurrentThreadId();
    hr = RoInitialize(RO_INIT_MULTITHREADED);
    printf("INIT %08lx\n", hr);
    WindowsCreateString(RuntimeClass_Windows_System_DispatcherQueue,
            wcslen(RuntimeClass_Windows_System_DispatcherQueue), &name);
    hr = RoGetActivationFactory(name, &IID_IDispatcherQueueStatics, (void **)&statics);
    WindowsDeleteString(name);
    printf("FACTORY %08lx\n", hr);
    if (FAILED(hr)) return 1;
    hr = IDispatcherQueueStatics_GetForCurrentThread(statics, &current);
    printf("BEFORE %08lx present=%d\n", hr, current != NULL);
    if (current) IDispatcherQueue_Release(current);
    hr = CreateDispatcherQueueController(options, &controller);
    printf("CREATE %08lx\n", hr);
    if (FAILED(hr)) return 2;
    hr = IDispatcherQueueController_get_DispatcherQueue(controller, &queue);
    printf("QUEUE %08lx\n", hr);
    hr = IDispatcherQueueStatics_GetForCurrentThread(statics, &current);
    printf("CURRENT %08lx same=%d\n", hr, current == queue);
    if (current) printf("CURRENT_RELEASE %lu\n", IDispatcherQueue_Release(current));
    hr = CreateDispatcherQueueController(options, &duplicate);
    printf("DUPLICATE %08lx unchanged=%d null=%d\n", hr, duplicate == (void *)0xdeadbeef, duplicate == NULL);
    if (SUCCEEDED(hr)) IDispatcherQueueController_Release(duplicate);
    for (i = 0; i < ARRAY_SIZE(priorities); ++i)
    {
        callbacks[i] = (struct callback){{&callback_vtbl}, 1, i};
        accepted = 0xcc;
        hr = IDispatcherQueue_TryEnqueueWithPriority(queue, priorities[i], &callbacks[i].iface, &accepted);
        printf("ENQUEUE %u priority=%d hr=%08lx accepted=%d ref=%ld\n",
                i, priorities[i], hr, accepted, callbacks[i].ref);
    }
    hr = IDispatcherQueue_add_ShutdownCompleted(queue, &event.iface, &token);
    printf("ADD_EVENT %08lx ref=%ld\n", hr, event.ref);
    if (test_deferral)
    {
        hr = IDispatcherQueue_add_ShutdownStarting(queue,
                (ITypedEventHandler_DispatcherQueue_DispatcherQueueShutdownStartingEventArgs *)&starting.iface, &starting_token);
        printf("ADD_STARTING %08lx ref=%ld\n", hr, starting.ref);
    }
    hr = IDispatcherQueueController_ShutdownQueueAsync(controller, &action);
    printf("SHUTDOWN %08lx\n", hr);
    IAsyncAction_QueryInterface(action, &IID_IAsyncInfo, (void **)&shutdown_info);
    if (getenv("QUEUE_REPEAT_SHUTDOWN"))
    {
        hr = IDispatcherQueueController_ShutdownQueueAsync(controller, &second);
        printf("SHUTDOWN_AGAIN %08lx same=%d null=%d\n", hr, second == action, second == NULL);
        if (second) IAsyncAction_Release(second);
    }
    Sleep(20);
    IAsyncInfo_get_Status(shutdown_info, &status);
    printf("BEFORE_PUMP status=%d calls=%u\n", status, calls);
    end = GetTickCount() + 5000;
    while (status == Started && GetTickCount() < end)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        IAsyncInfo_get_Status(shutdown_info, &status);
        if (deferral && !deferral_completed)
        {
            printf("DEFERRAL_PENDING status=%d calls=%u\n", status, calls);
            thread = CreateThread(NULL, 0, finish_deferral, NULL, 0, NULL);
            WaitForSingleObject(thread, 5000);
            CloseHandle(thread);
            deferral_completed = TRUE;
        }
        if (status == Started) MsgWaitForMultipleObjects(0, NULL, FALSE, 10, QS_ALLINPUT);
    }
    printf("AFTER_PUMP status=%d calls=%u\n", status, calls);
    hr = IDispatcherQueueStatics_GetForCurrentThread(statics, &current);
    printf("AFTER %08lx present=%d\n", hr, current != NULL);
    if (current) IDispatcherQueue_Release(current);
    accepted = 0xcc;
    hr = IDispatcherQueue_TryEnqueue(queue, &callbacks[0].iface, &accepted);
    printf("AFTER_ENQUEUE %08lx accepted=%d\n", hr, accepted);
    for (i = 0; i < ARRAY_SIZE(priorities); ++i) printf("REF %u %ld\n", i, callbacks[i].ref);
    IDispatcherQueue_remove_ShutdownCompleted(queue, token);
    printf("EVENT_REF %ld\n", event.ref);
    if (test_deferral)
    {
        IDispatcherQueue_remove_ShutdownStarting(queue, starting_token);
        printf("STARTING_REF %ld EXTRA_REF %ld CROSS_REF %ld\n", starting.ref, extra.ref, cross_thread.ref);
        if (deferral) IDeferral_Release(deferral);
    }
    IAsyncInfo_Release(shutdown_info);
    IAsyncAction_Release(action);
    IDispatcherQueue_Release(queue);
    IDispatcherQueueController_Release(controller);
    IDispatcherQueueStatics_Release(statics);
    RoUninitialize();
    return 0;
}
