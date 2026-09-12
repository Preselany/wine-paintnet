/* Windows composition controller and compositor state.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "initguid.h"
#include "private.h"
#include "wine/list.h"

WINE_DEFAULT_DEBUG_CHANNEL(dcomp);

typedef ITypedEventHandler_CompositorController_IInspectable commit_handler;

struct event_entry
{
    struct list entry;
    EventRegistrationToken token;
    commit_handler *handler;
};

struct composition_state
{
    ICompositorController controller;
    ICompositor compositor;
    IClosable controller_closable, compositor_closable;
    LONG ref;
    CRITICAL_SECTION cs;
    IDispatcherQueue *queue;
    struct list handlers;
    INT64 token;
    BOOL closed, dirty;
};

struct composition_callback
{
    IDispatcherQueueHandler iface;
    LONG ref;
    struct composition_state *state;
    IAsyncAction *action;
    BOOL notify, action_completed;
};

struct color_brush
{
    ICompositionColorBrush iface;
    LONG ref;
    struct composition_state *state;
    struct __x_ABI_CWindows_CUI_CColor color;
};

static const ICompositorControllerVtbl controller_vtbl;
static const ICompositorVtbl compositor_vtbl;
static const IClosableVtbl controller_closable_vtbl, compositor_closable_vtbl;
static const ICompositionColorBrushVtbl color_brush_vtbl;

static struct composition_state *state_from_controller(ICompositorController *iface)
{ return CONTAINING_RECORD(iface, struct composition_state, controller); }
static struct composition_state *state_from_compositor(ICompositor *iface)
{ return CONTAINING_RECORD(iface, struct composition_state, compositor); }

static ULONG state_addref(struct composition_state *state)
{ return InterlockedIncrement(&state->ref); }

static void state_clear_handlers(struct composition_state *state)
{
    struct event_entry *entry, *next;
    LIST_FOR_EACH_ENTRY_SAFE(entry, next, &state->handlers, struct event_entry, entry)
    {
        list_remove(&entry->entry);
        ITypedEventHandler_CompositorController_IInspectable_Release(entry->handler);
        free(entry);
    }
}

static ULONG state_release(struct composition_state *state)
{
    ULONG ref = InterlockedDecrement(&state->ref);
    if (!ref)
    {
        state_clear_handlers(state);
        IDispatcherQueue_Release(state->queue);
        DeleteCriticalSection(&state->cs);
        free(state);
    }
    return ref;
}

static HRESULT state_status(struct composition_state *state)
{
    HRESULT hr;
    EnterCriticalSection(&state->cs);
    hr = state->closed ? RO_E_CLOSED : S_OK;
    LeaveCriticalSection(&state->cs);
    return hr;
}

static HRESULT inspect_iids(const IID **source, ULONG count, ULONG *ret_count, IID **ret)
{
    ULONG i;
    if (!ret_count || !ret) return E_POINTER;
    *ret_count = 0; *ret = NULL;
    if (!(*ret = CoTaskMemAlloc(count * sizeof(**ret)))) return E_OUTOFMEMORY;
    for (i = 0; i < count; ++i) (*ret)[i] = *source[i];
    *ret_count = count;
    return S_OK;
}

static HRESULT WINAPI controller_QueryInterface(ICompositorController *iface, REFIID iid, void **out)
{
    struct composition_state *state = state_from_controller(iface);
    TRACE("iface %p, iid %s.\n", iface, debugstr_guid(iid));
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable)
            || IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_ICompositorController))
        *out = iface;
    else if (IsEqualGUID(iid, &IID_IClosable)) *out = &state->controller_closable;
    else return E_NOINTERFACE;
    state_addref(state);
    return S_OK;
}
static ULONG WINAPI controller_AddRef(ICompositorController *iface)
{ return state_addref(state_from_controller(iface)); }
static ULONG WINAPI controller_Release(ICompositorController *iface)
{ return state_release(state_from_controller(iface)); }
static HRESULT WINAPI controller_GetIids(ICompositorController *iface, ULONG *count, IID **iids)
{
    const IID *ids[] = {&IID_ICompositorController, &IID_IClosable};
    return inspect_iids(ids, ARRAY_SIZE(ids), count, iids);
}
static HRESULT WINAPI controller_GetRuntimeClassName(ICompositorController *iface, HSTRING *name)
{
    static const WCHAR str[] = L"Windows.UI.Composition.Core.CompositorController";
    return WindowsCreateString(str, ARRAY_SIZE(str) - 1, name);
}
static HRESULT WINAPI controller_GetTrustLevel(ICompositorController *iface, TrustLevel *level)
{ if (!level) return E_POINTER; *level = BaseTrust; return S_OK; }
static HRESULT WINAPI controller_get_Compositor(ICompositorController *iface, ICompositor **result)
{
    struct composition_state *state = state_from_controller(iface);
    HRESULT hr;
    if (!result) return E_POINTER;
    *result = NULL;
    EnterCriticalSection(&state->cs);
    if (SUCCEEDED(hr = state->closed ? RO_E_CLOSED : S_OK))
    { *result = &state->compositor; state_addref(state); }
    LeaveCriticalSection(&state->cs);
    return hr;
}

static HRESULT WINAPI callback_QueryInterface(IDispatcherQueueHandler *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IAgileObject)
            && !IsEqualGUID(iid, &IID_IDispatcherQueueHandler)) return E_NOINTERFACE;
    *out = iface; IDispatcherQueueHandler_AddRef(iface); return S_OK;
}
static ULONG WINAPI callback_AddRef(IDispatcherQueueHandler *iface)
{ return InterlockedIncrement(&((struct composition_callback *)iface)->ref); }
static ULONG WINAPI callback_Release(IDispatcherQueueHandler *iface)
{
    struct composition_callback *callback = (struct composition_callback *)iface;
    ULONG ref = InterlockedDecrement(&callback->ref);
    if (!ref)
    {
        if (callback->action)
        {
            if (!callback->action_completed) async_action_complete(callback->action, RO_E_CLOSED);
            IAsyncAction_Release(callback->action);
        }
        state_release(callback->state); free(callback);
    }
    return ref;
}
static HRESULT WINAPI callback_Invoke(IDispatcherQueueHandler *iface)
{
    struct composition_callback *callback = (struct composition_callback *)iface;
    struct composition_state *state = callback->state;
    struct event_entry *entry;
    commit_handler **handlers = NULL;
    size_t count = 0, i;
    HRESULT hr = S_OK;
    EnterCriticalSection(&state->cs);
    if (state->closed) hr = RO_E_CLOSED;
    else if (callback->notify && state->dirty)
    {
        LIST_FOR_EACH_ENTRY(entry, &state->handlers, struct event_entry, entry) ++count;
        if (count && !(handlers = calloc(count, sizeof(*handlers)))) hr = E_OUTOFMEMORY;
        else
        {
            i = 0;
            LIST_FOR_EACH_ENTRY(entry, &state->handlers, struct event_entry, entry)
            {
                handlers[i++] = entry->handler;
                ITypedEventHandler_CompositorController_IInspectable_AddRef(entry->handler);
            }
        }
    }
    LeaveCriticalSection(&state->cs);
    if (SUCCEEDED(hr))
        for (i = 0; i < count; ++i)
        {
            ITypedEventHandler_CompositorController_IInspectable_Invoke(handlers[i], &state->controller, NULL);
            ITypedEventHandler_CompositorController_IInspectable_Release(handlers[i]);
        }
    free(handlers);
    if (callback->action)
    {
        callback->action_completed = TRUE;
        async_action_complete(callback->action, hr);
    }
    return S_OK;
}
static const IDispatcherQueueHandlerVtbl callback_vtbl =
{ callback_QueryInterface, callback_AddRef, callback_Release, callback_Invoke };

/* Queued callbacks own the controller and action until the associated UI queue
 * has observed the state transition. They never call event handlers under cs. */
static HRESULT state_enqueue(struct composition_state *state, BOOL notify, IAsyncAction *action)
{
    struct composition_callback *callback;
    boolean accepted = FALSE;
    HRESULT hr;
    if (!(callback = calloc(1, sizeof(*callback)))) return E_OUTOFMEMORY;
    callback->iface.lpVtbl = &callback_vtbl; callback->ref = 1;
    callback->state = state; state_addref(state);
    callback->notify = notify; callback->action = action;
    if (action) IAsyncAction_AddRef(action);
    hr = IDispatcherQueue_TryEnqueue(state->queue, &callback->iface, &accepted);
    IDispatcherQueueHandler_Release(&callback->iface);
    return FAILED(hr) ? hr : accepted ? S_OK : RO_E_CLOSED;
}

static HRESULT state_dirty(struct composition_state *state)
{
    HRESULT hr = S_OK;
    EnterCriticalSection(&state->cs);
    if (state->closed) hr = RO_E_CLOSED;
    else if (!state->dirty)
    {
        state->dirty = TRUE;
        if (FAILED(hr = state_enqueue(state, TRUE, NULL))) state->dirty = FALSE;
    }
    LeaveCriticalSection(&state->cs);
    return hr;
}

static HRESULT WINAPI controller_Commit(ICompositorController *iface)
{
    struct composition_state *state = state_from_controller(iface);
    HRESULT hr;
    EnterCriticalSection(&state->cs);
    /* No visual targets are supported yet. Empty commits consume real state
     * changes; visual creation methods continue to report E_NOTIMPL. */
    if (SUCCEEDED(hr = state->closed ? RO_E_CLOSED : S_OK)) state->dirty = FALSE;
    LeaveCriticalSection(&state->cs);
    return hr;
}
static HRESULT WINAPI controller_EnsurePreviousCommitCompletedAsync(ICompositorController *iface, IAsyncAction **result)
{
    struct composition_state *state = state_from_controller(iface);
    HRESULT hr;
    if (!result) return E_POINTER;
    *result = NULL;
    if (FAILED(hr = state_status(state))) return hr;
    if (FAILED(hr = async_action_create(NULL, NULL, result))) return hr;
    if (SUCCEEDED(hr = state_dirty(state))) hr = state_enqueue(state, FALSE, *result);
    if (FAILED(hr)) {IAsyncAction_Release(*result); *result = NULL;}
    return hr;
}
static HRESULT WINAPI controller_add_CommitNeeded(ICompositorController *iface, commit_handler *handler, EventRegistrationToken *token)
{
    struct composition_state *state = state_from_controller(iface);
    struct event_entry *entry;
    HRESULT hr;
    if (FAILED(hr = state_status(state))) return hr;
    if (!handler || !token) return E_POINTER;
    if (!(entry = calloc(1, sizeof(*entry)))) return E_OUTOFMEMORY;
    entry->handler = handler; ITypedEventHandler_CompositorController_IInspectable_AddRef(handler);
    EnterCriticalSection(&state->cs);
    if (SUCCEEDED(hr = state->closed ? RO_E_CLOSED : S_OK))
    { entry->token.value = ++state->token; *token = entry->token; list_add_tail(&state->handlers, &entry->entry); }
    LeaveCriticalSection(&state->cs);
    if (FAILED(hr)) {ITypedEventHandler_CompositorController_IInspectable_Release(handler); free(entry);}
    return hr;
}
static HRESULT WINAPI controller_remove_CommitNeeded(ICompositorController *iface, EventRegistrationToken token)
{
    struct composition_state *state = state_from_controller(iface);
    struct event_entry *entry, *found = NULL;
    HRESULT hr;
    EnterCriticalSection(&state->cs);
    hr = state->closed ? RO_E_CLOSED : S_OK;
    if (SUCCEEDED(hr)) LIST_FOR_EACH_ENTRY(entry, &state->handlers, struct event_entry, entry)
        if (entry->token.value == token.value) {list_remove(&entry->entry); found = entry; break;}
    LeaveCriticalSection(&state->cs);
    if (found) {ITypedEventHandler_CompositorController_IInspectable_Release(found->handler); free(found);}
    return hr;
}
static const ICompositorControllerVtbl controller_vtbl =
{
    controller_QueryInterface, controller_AddRef, controller_Release, controller_GetIids,
    controller_GetRuntimeClassName, controller_GetTrustLevel, controller_get_Compositor,
    controller_Commit, controller_EnsurePreviousCommitCompletedAsync,
    controller_add_CommitNeeded, controller_remove_CommitNeeded,
};

static HRESULT state_close(struct composition_state *state)
{
    struct event_entry *entry, *next;
    struct list removed;
    list_init(&removed);
    EnterCriticalSection(&state->cs);
    state->closed = TRUE;
    LIST_FOR_EACH_ENTRY_SAFE(entry, next, &state->handlers, struct event_entry, entry)
    {list_remove(&entry->entry); list_add_tail(&removed, &entry->entry);}
    LeaveCriticalSection(&state->cs);
    LIST_FOR_EACH_ENTRY_SAFE(entry, next, &removed, struct event_entry, entry)
    {list_remove(&entry->entry); ITypedEventHandler_CompositorController_IInspectable_Release(entry->handler); free(entry);}
    return S_OK;
}

#define CLOSABLE_METHODS(prefix, member, main_type, main_member) \
static HRESULT WINAPI prefix##_QueryInterface(IClosable *iface, REFIID iid, void **out) \
{ struct composition_state *state = CONTAINING_RECORD(iface, struct composition_state, member); \
  return main_type##_QueryInterface(&state->main_member, iid, out); } \
static ULONG WINAPI prefix##_AddRef(IClosable *iface) \
{ return state_addref(CONTAINING_RECORD(iface, struct composition_state, member)); } \
static ULONG WINAPI prefix##_Release(IClosable *iface) \
{ return state_release(CONTAINING_RECORD(iface, struct composition_state, member)); } \
static HRESULT WINAPI prefix##_GetIids(IClosable *iface, ULONG *count, IID **iids) \
{ struct composition_state *state = CONTAINING_RECORD(iface, struct composition_state, member); \
  return main_type##_GetIids(&state->main_member, count, iids); } \
static HRESULT WINAPI prefix##_GetRuntimeClassName(IClosable *iface, HSTRING *name) \
{ struct composition_state *state = CONTAINING_RECORD(iface, struct composition_state, member); \
  return main_type##_GetRuntimeClassName(&state->main_member, name); } \
static HRESULT WINAPI prefix##_GetTrustLevel(IClosable *iface, TrustLevel *level) \
{ if (!level) return E_POINTER; *level = BaseTrust; return S_OK; } \
static HRESULT WINAPI prefix##_Close(IClosable *iface) \
{ return state_close(CONTAINING_RECORD(iface, struct composition_state, member)); } \
static const IClosableVtbl prefix##_vtbl = \
{ prefix##_QueryInterface, prefix##_AddRef, prefix##_Release, prefix##_GetIids, \
  prefix##_GetRuntimeClassName, prefix##_GetTrustLevel, prefix##_Close };

CLOSABLE_METHODS(controller_closable, controller_closable, ICompositorController, controller)

static HRESULT WINAPI compositor_QueryInterface(ICompositor *iface, REFIID iid, void **out)
{
    struct composition_state *state = state_from_compositor(iface);
    TRACE("iface %p, iid %s.\n", iface, debugstr_guid(iid));
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable)
            || IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_ICompositor)) *out = iface;
    else if (IsEqualGUID(iid, &IID_IClosable)) *out = &state->compositor_closable;
    else return E_NOINTERFACE;
    state_addref(state); return S_OK;
}
static ULONG WINAPI compositor_AddRef(ICompositor *iface)
{ return state_addref(state_from_compositor(iface)); }
static ULONG WINAPI compositor_Release(ICompositor *iface)
{ return state_release(state_from_compositor(iface)); }
static HRESULT WINAPI compositor_GetIids(ICompositor *iface, ULONG *count, IID **iids)
{
    const IID *ids[] = {&IID_ICompositor, &IID_IClosable};
    return inspect_iids(ids, ARRAY_SIZE(ids), count, iids);
}
static HRESULT WINAPI compositor_GetRuntimeClassName(ICompositor *iface, HSTRING *name)
{
    static const WCHAR str[] = L"Windows.UI.Composition.Compositor";
    return WindowsCreateString(str, ARRAY_SIZE(str) - 1, name);
}
static HRESULT WINAPI compositor_GetTrustLevel(ICompositor *iface, TrustLevel *level)
{ if (!level) return E_POINTER; *level = BaseTrust; return S_OK; }

CLOSABLE_METHODS(compositor_closable, compositor_closable, ICompositor, compositor)

static HRESULT WINAPI brush_QueryInterface(ICompositionColorBrush *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IInspectable)
            && !IsEqualGUID(iid, &IID_IAgileObject) && !IsEqualGUID(iid, &IID_ICompositionColorBrush)) return E_NOINTERFACE;
    *out = iface; ICompositionColorBrush_AddRef(iface); return S_OK;
}
static ULONG WINAPI brush_AddRef(ICompositionColorBrush *iface)
{ return InterlockedIncrement(&((struct color_brush *)iface)->ref); }
static ULONG WINAPI brush_Release(ICompositionColorBrush *iface)
{
    struct color_brush *brush = (struct color_brush *)iface;
    ULONG ref = InterlockedDecrement(&brush->ref);
    if (!ref) {state_release(brush->state); free(brush);}
    return ref;
}
static HRESULT WINAPI brush_GetIids(ICompositionColorBrush *iface, ULONG *count, IID **iids)
{
    const IID *ids[] = {&IID_ICompositionColorBrush};
    return inspect_iids(ids, ARRAY_SIZE(ids), count, iids);
}
static HRESULT WINAPI brush_GetRuntimeClassName(ICompositionColorBrush *iface, HSTRING *name)
{
    static const WCHAR str[] = L"Windows.UI.Composition.CompositionColorBrush";
    return WindowsCreateString(str, ARRAY_SIZE(str) - 1, name);
}
static HRESULT WINAPI brush_GetTrustLevel(ICompositionColorBrush *iface, TrustLevel *level)
{ if (!level) return E_POINTER; *level = BaseTrust; return S_OK; }
static HRESULT WINAPI brush_get_Color(ICompositionColorBrush *iface, struct __x_ABI_CWindows_CUI_CColor *color)
{
    struct color_brush *brush = (struct color_brush *)iface;
    HRESULT hr;
    if (!color) return E_POINTER;
    EnterCriticalSection(&brush->state->cs);
    if (SUCCEEDED(hr = brush->state->closed ? RO_E_CLOSED : S_OK)) *color = brush->color;
    LeaveCriticalSection(&brush->state->cs);
    return hr;
}
static HRESULT WINAPI brush_put_Color(ICompositionColorBrush *iface, struct __x_ABI_CWindows_CUI_CColor color)
{
    struct color_brush *brush = (struct color_brush *)iface;
    HRESULT hr;
    EnterCriticalSection(&brush->state->cs);
    if (SUCCEEDED(hr = state_dirty(brush->state))) brush->color = color;
    LeaveCriticalSection(&brush->state->cs);
    return hr;
}
static const ICompositionColorBrushVtbl color_brush_vtbl =
{
    brush_QueryInterface, brush_AddRef, brush_Release, brush_GetIids,
    brush_GetRuntimeClassName, brush_GetTrustLevel, brush_get_Color, brush_put_Color,
};
static HRESULT WINAPI compositor_CreateColorBrushWithColor(ICompositor *iface,
        struct __x_ABI_CWindows_CUI_CColor color, ICompositionColorBrush **result)
{
    struct composition_state *state = state_from_compositor(iface);
    struct color_brush *brush;
    HRESULT hr;
    if (!result) return E_POINTER;
    *result = NULL;
    if (FAILED(hr = state_status(state))) return hr;
    if (!(brush = calloc(1, sizeof(*brush)))) return E_OUTOFMEMORY;
    brush->iface.lpVtbl = &color_brush_vtbl; brush->ref = 1;
    brush->state = state; state_addref(state); brush->color = color;
    if (FAILED(hr = state_dirty(state))) {ICompositionColorBrush_Release(&brush->iface); return hr;}
    *result = &brush->iface;
    return S_OK;
}
static HRESULT WINAPI compositor_CreateColorBrush(ICompositor *iface, ICompositionColorBrush **result)
{
    struct __x_ABI_CWindows_CUI_CColor color = {0};
    return compositor_CreateColorBrushWithColor(iface, color, result);
}

static HRESULT WINAPI compositor_CreateColorKeyFrameAnimation(__x_ABI_CWindows_CUI_CComposition_CICompositor *iface, __x_ABI_CWindows_CUI_CComposition_CIColorKeyFrameAnimation **result)
{
    HRESULT hr = state_status(state_from_compositor(iface));
    FIXME("CreateColorKeyFrameAnimation is not implemented.\n");
    if (result) *result = NULL;
    return FAILED(hr) ? hr : E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateContainerVisual(__x_ABI_CWindows_CUI_CComposition_CICompositor *iface, __x_ABI_CWindows_CUI_CComposition_CIContainerVisual **result)
{
    HRESULT hr = state_status(state_from_compositor(iface));
    FIXME("CreateContainerVisual is not implemented.\n");
    if (result) *result = NULL;
    return FAILED(hr) ? hr : E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateCubicBezierEasingFunction(__x_ABI_CWindows_CUI_CComposition_CICompositor *iface, __x_ABI_CWindows_CFoundation_CNumerics_CVector2 point1, __x_ABI_CWindows_CFoundation_CNumerics_CVector2 point2, __x_ABI_CWindows_CUI_CComposition_CICubicBezierEasingFunction **result)
{
    HRESULT hr = state_status(state_from_compositor(iface));
    FIXME("CreateCubicBezierEasingFunction is not implemented.\n");
    if (result) *result = NULL;
    return FAILED(hr) ? hr : E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateEffectFactory(__x_ABI_CWindows_CUI_CComposition_CICompositor *iface, __x_ABI_CWindows_CGraphics_CEffects_CIGraphicsEffect *effect, __x_ABI_CWindows_CUI_CComposition_CICompositionEffectFactory **result)
{
    HRESULT hr = state_status(state_from_compositor(iface));
    FIXME("CreateEffectFactory is not implemented.\n");
    if (result) *result = NULL;
    return FAILED(hr) ? hr : E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateEffectFactoryWithProperties(__x_ABI_CWindows_CUI_CComposition_CICompositor *iface, __x_ABI_CWindows_CGraphics_CEffects_CIGraphicsEffect *effect, __FIIterable_1_HSTRING *animatable, __x_ABI_CWindows_CUI_CComposition_CICompositionEffectFactory **result)
{
    HRESULT hr = state_status(state_from_compositor(iface));
    FIXME("CreateEffectFactoryWithProperties is not implemented.\n");
    if (result) *result = NULL;
    return FAILED(hr) ? hr : E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateExpressionAnimation(__x_ABI_CWindows_CUI_CComposition_CICompositor *iface, __x_ABI_CWindows_CUI_CComposition_CIExpressionAnimation **result)
{
    HRESULT hr = state_status(state_from_compositor(iface));
    FIXME("CreateExpressionAnimation is not implemented.\n");
    if (result) *result = NULL;
    return FAILED(hr) ? hr : E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateExpressionAnimationWithExpression(__x_ABI_CWindows_CUI_CComposition_CICompositor *iface, HSTRING expression, __x_ABI_CWindows_CUI_CComposition_CIExpressionAnimation **result)
{
    HRESULT hr = state_status(state_from_compositor(iface));
    FIXME("CreateExpressionAnimationWithExpression is not implemented.\n");
    if (result) *result = NULL;
    return FAILED(hr) ? hr : E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateInsetClip(__x_ABI_CWindows_CUI_CComposition_CICompositor *iface, __x_ABI_CWindows_CUI_CComposition_CIInsetClip **result)
{
    HRESULT hr = state_status(state_from_compositor(iface));
    FIXME("CreateInsetClip is not implemented.\n");
    if (result) *result = NULL;
    return FAILED(hr) ? hr : E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateInsetClipWithInsets(__x_ABI_CWindows_CUI_CComposition_CICompositor *iface, FLOAT left, FLOAT top, FLOAT right, FLOAT bottom, __x_ABI_CWindows_CUI_CComposition_CIInsetClip **result)
{
    HRESULT hr = state_status(state_from_compositor(iface));
    FIXME("CreateInsetClipWithInsets is not implemented.\n");
    if (result) *result = NULL;
    return FAILED(hr) ? hr : E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateLinearEasingFunction(__x_ABI_CWindows_CUI_CComposition_CICompositor *iface, __x_ABI_CWindows_CUI_CComposition_CILinearEasingFunction **result)
{
    HRESULT hr = state_status(state_from_compositor(iface));
    FIXME("CreateLinearEasingFunction is not implemented.\n");
    if (result) *result = NULL;
    return FAILED(hr) ? hr : E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreatePropertySet(__x_ABI_CWindows_CUI_CComposition_CICompositor *iface, __x_ABI_CWindows_CUI_CComposition_CICompositionPropertySet **result)
{
    HRESULT hr = state_status(state_from_compositor(iface));
    FIXME("CreatePropertySet is not implemented.\n");
    if (result) *result = NULL;
    return FAILED(hr) ? hr : E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateQuaternionKeyFrameAnimation(__x_ABI_CWindows_CUI_CComposition_CICompositor *iface, __x_ABI_CWindows_CUI_CComposition_CIQuaternionKeyFrameAnimation **result)
{
    HRESULT hr = state_status(state_from_compositor(iface));
    FIXME("CreateQuaternionKeyFrameAnimation is not implemented.\n");
    if (result) *result = NULL;
    return FAILED(hr) ? hr : E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateScalarKeyFrameAnimation(__x_ABI_CWindows_CUI_CComposition_CICompositor *iface, __x_ABI_CWindows_CUI_CComposition_CIScalarKeyFrameAnimation **result)
{
    HRESULT hr = state_status(state_from_compositor(iface));
    FIXME("CreateScalarKeyFrameAnimation is not implemented.\n");
    if (result) *result = NULL;
    return FAILED(hr) ? hr : E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateScopedBatch(__x_ABI_CWindows_CUI_CComposition_CICompositor *iface, __x_ABI_CWindows_CUI_CComposition_CCompositionBatchTypes type, __x_ABI_CWindows_CUI_CComposition_CICompositionScopedBatch **result)
{
    HRESULT hr = state_status(state_from_compositor(iface));
    FIXME("CreateScopedBatch is not implemented.\n");
    if (result) *result = NULL;
    return FAILED(hr) ? hr : E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateSpriteVisual(__x_ABI_CWindows_CUI_CComposition_CICompositor *iface, __x_ABI_CWindows_CUI_CComposition_CISpriteVisual **result)
{
    HRESULT hr = state_status(state_from_compositor(iface));
    FIXME("CreateSpriteVisual is not implemented.\n");
    if (result) *result = NULL;
    return FAILED(hr) ? hr : E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateSurfaceBrush(__x_ABI_CWindows_CUI_CComposition_CICompositor *iface, __x_ABI_CWindows_CUI_CComposition_CICompositionSurfaceBrush **result)
{
    HRESULT hr = state_status(state_from_compositor(iface));
    FIXME("CreateSurfaceBrush is not implemented.\n");
    if (result) *result = NULL;
    return FAILED(hr) ? hr : E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateSurfaceBrushWithSurface(__x_ABI_CWindows_CUI_CComposition_CICompositor *iface, __x_ABI_CWindows_CUI_CComposition_CICompositionSurface *surface, __x_ABI_CWindows_CUI_CComposition_CICompositionSurfaceBrush **result)
{
    HRESULT hr = state_status(state_from_compositor(iface));
    FIXME("CreateSurfaceBrushWithSurface is not implemented.\n");
    if (result) *result = NULL;
    return FAILED(hr) ? hr : E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateTargetForCurrentView(__x_ABI_CWindows_CUI_CComposition_CICompositor *iface, __x_ABI_CWindows_CUI_CComposition_CICompositionTarget **result)
{
    HRESULT hr = state_status(state_from_compositor(iface));
    FIXME("CreateTargetForCurrentView is not implemented.\n");
    if (result) *result = NULL;
    return FAILED(hr) ? hr : E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateVector2KeyFrameAnimation(__x_ABI_CWindows_CUI_CComposition_CICompositor *iface, __x_ABI_CWindows_CUI_CComposition_CIVector2KeyFrameAnimation **result)
{
    HRESULT hr = state_status(state_from_compositor(iface));
    FIXME("CreateVector2KeyFrameAnimation is not implemented.\n");
    if (result) *result = NULL;
    return FAILED(hr) ? hr : E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateVector3KeyFrameAnimation(__x_ABI_CWindows_CUI_CComposition_CICompositor *iface, __x_ABI_CWindows_CUI_CComposition_CIVector3KeyFrameAnimation **result)
{
    HRESULT hr = state_status(state_from_compositor(iface));
    FIXME("CreateVector3KeyFrameAnimation is not implemented.\n");
    if (result) *result = NULL;
    return FAILED(hr) ? hr : E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateVector4KeyFrameAnimation(__x_ABI_CWindows_CUI_CComposition_CICompositor *iface, __x_ABI_CWindows_CUI_CComposition_CIVector4KeyFrameAnimation **result)
{
    HRESULT hr = state_status(state_from_compositor(iface));
    FIXME("CreateVector4KeyFrameAnimation is not implemented.\n");
    if (result) *result = NULL;
    return FAILED(hr) ? hr : E_NOTIMPL;
}

static HRESULT WINAPI compositor_GetCommitBatch(__x_ABI_CWindows_CUI_CComposition_CICompositor *iface, __x_ABI_CWindows_CUI_CComposition_CCompositionBatchTypes type, __x_ABI_CWindows_CUI_CComposition_CICompositionCommitBatch **result)
{
    HRESULT hr = state_status(state_from_compositor(iface));
    FIXME("GetCommitBatch is not implemented.\n");
    if (result) *result = NULL;
    return FAILED(hr) ? hr : E_NOTIMPL;
}

static const ICompositorVtbl compositor_vtbl =
{
    compositor_QueryInterface,
    compositor_AddRef,
    compositor_Release,
    compositor_GetIids,
    compositor_GetRuntimeClassName,
    compositor_GetTrustLevel,
    compositor_CreateColorKeyFrameAnimation,
    compositor_CreateColorBrush,
    compositor_CreateColorBrushWithColor,
    compositor_CreateContainerVisual,
    compositor_CreateCubicBezierEasingFunction,
    compositor_CreateEffectFactory,
    compositor_CreateEffectFactoryWithProperties,
    compositor_CreateExpressionAnimation,
    compositor_CreateExpressionAnimationWithExpression,
    compositor_CreateInsetClip,
    compositor_CreateInsetClipWithInsets,
    compositor_CreateLinearEasingFunction,
    compositor_CreatePropertySet,
    compositor_CreateQuaternionKeyFrameAnimation,
    compositor_CreateScalarKeyFrameAnimation,
    compositor_CreateScopedBatch,
    compositor_CreateSpriteVisual,
    compositor_CreateSurfaceBrush,
    compositor_CreateSurfaceBrushWithSurface,
    compositor_CreateTargetForCurrentView,
    compositor_CreateVector2KeyFrameAnimation,
    compositor_CreateVector3KeyFrameAnimation,
    compositor_CreateVector4KeyFrameAnimation,
    compositor_GetCommitBatch,
};

static HRESULT controller_create(IInspectable **result)
{
    static const WCHAR queue_class[] = L"Windows.System.DispatcherQueue";
    IDispatcherQueueStatics *statics;
    IDispatcherQueue *queue = NULL;
    struct composition_state *state;
    HSTRING name;
    HRESULT hr;
    if (!result) return E_POINTER;
    *result = NULL;
    if (FAILED(hr = WindowsCreateString(queue_class, ARRAY_SIZE(queue_class) - 1, &name))) return hr;
    hr = RoGetActivationFactory(name, &IID_IDispatcherQueueStatics, (void **)&statics);
    WindowsDeleteString(name);
    if (FAILED(hr)) return hr;
    hr = IDispatcherQueueStatics_GetForCurrentThread(statics, &queue);
    IDispatcherQueueStatics_Release(statics);
    if (FAILED(hr)) return hr;
    if (!queue) return E_ACCESSDENIED;
    if (!(state = calloc(1, sizeof(*state)))) {IDispatcherQueue_Release(queue); return E_OUTOFMEMORY;}
    state->controller.lpVtbl = &controller_vtbl;
    state->compositor.lpVtbl = &compositor_vtbl;
    state->controller_closable.lpVtbl = &controller_closable_vtbl;
    state->compositor_closable.lpVtbl = &compositor_closable_vtbl;
    state->ref = 1; state->queue = queue;
    InitializeCriticalSection(&state->cs);
    list_init(&state->handlers);
    *result = (IInspectable *)&state->controller;
    return S_OK;
}

static HRESULT WINAPI factory_QueryInterface(IActivationFactory *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IInspectable)
            && !IsEqualGUID(iid, &IID_IAgileObject) && !IsEqualGUID(iid, &IID_IActivationFactory)) return E_NOINTERFACE;
    *out = iface; IActivationFactory_AddRef(iface); return S_OK;
}
static ULONG WINAPI factory_AddRef(IActivationFactory *iface) { return 2; }
static ULONG WINAPI factory_Release(IActivationFactory *iface) { return 1; }
static HRESULT WINAPI factory_GetIids(IActivationFactory *iface, ULONG *count, IID **iids)
{
    const IID *ids[] = {&IID_IActivationFactory};
    return inspect_iids(ids, ARRAY_SIZE(ids), count, iids);
}
static HRESULT WINAPI factory_GetRuntimeClassName(IActivationFactory *iface, HSTRING *name)
{ if (!name) return E_POINTER; *name = NULL; return E_ILLEGAL_METHOD_CALL; }
static HRESULT WINAPI factory_GetTrustLevel(IActivationFactory *iface, TrustLevel *level)
{ if (!level) return E_POINTER; *level = BaseTrust; return S_OK; }
static HRESULT WINAPI factory_ActivateInstance(IActivationFactory *iface, IInspectable **instance)
{ return controller_create(instance); }
static const IActivationFactoryVtbl factory_vtbl =
{
    factory_QueryInterface, factory_AddRef, factory_Release, factory_GetIids,
    factory_GetRuntimeClassName, factory_GetTrustLevel, factory_ActivateInstance,
};
static IActivationFactory controller_factory = {&factory_vtbl};

HRESULT WINAPI DllGetActivationFactory(HSTRING classid, IActivationFactory **factory)
{
    const WCHAR *name = WindowsGetStringRawBuffer(classid, NULL);
    TRACE("class %s.\n", debugstr_hstring(classid));
    if (!factory) return E_POINTER;
    *factory = NULL;
    if (wcscmp(name, L"Windows.UI.Composition.Core.CompositorController")) return CLASS_E_CLASSNOTAVAILABLE;
    return IActivationFactory_QueryInterface(&controller_factory, &IID_IActivationFactory, (void **)factory);
}
