/* UIAnimation transitions and time-driven storyboards.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <stdarg.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include "windef.h"
#include "winbase.h"
#include "objbase.h"
#include "uianimation.h"
#include "wine/list.h"
#include "wine/debug.h"
#include "animation_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(uianimation);

struct animation_manager;
struct animation_storyboard;
struct animation_variable
{
    IUIAnimationVariable IUIAnimationVariable_iface;
    LONG ref;
    struct animation_manager *manager;
    struct list entry;
    double value, previous, final, velocity, lower, upper;
    UI_ANIMATION_ROUNDING_MODE rounding;
    struct animation_storyboard *current, *changed_storyboard;
    IUIAnimationVariableChangeHandler *handler;
    IUIAnimationVariableIntegerChangeHandler *integer_handler;
};

struct animation_transition
{
    IUIAnimationTransition IUIAnimationTransition_iface;
    LONG ref;
    enum animation_transition_kind kind;
    double duration, final, initial, velocity;
    BOOL initial_set, velocity_set, used, active, shutdown;
};

struct storyboard_keyframe;

struct storyboard_transition
{
    struct list entry;
    struct animation_variable *variable;
    struct animation_transition *transition;
    double start, duration, initial, velocity;
    struct storyboard_keyframe *start_key, *end_key;
    BOOL at_keyframe, between_keyframes;
};

struct storyboard_keyframe
{
    struct list entry;
    struct storyboard_keyframe *parent;
    struct storyboard_transition *after;
    double offset;
    UINT_PTR index;
};

struct animation_storyboard
{
    IUIAnimationStoryboard IUIAnimationStoryboard_iface;
    LONG ref;
    struct animation_manager *manager;
    struct list entry, transitions, keyframes;
    UINT_PTR keyframe_count;
    struct storyboard_keyframe *loop_start_key, *loop_end_key;
    double base_duration, first_start, loop_start, loop_end;
    INT32 loop_count;
    BOOL has_loop;
    UI_ANIMATION_STORYBOARD_STATUS status;
    double start, duration, elapsed, delay;
    BOOL active;
    IUIAnimationStoryboardEventHandler *handler;
};

struct animation_manager
{
    IUIAnimationManager IUIAnimationManager_iface;
    LONG ref;
    struct list variables, storyboards;
    double time, delay;
    BOOL have_time, paused, shutdown, updating;
    UI_ANIMATION_MODE mode;
    UI_ANIMATION_MANAGER_STATUS status;
    IUIAnimationManagerEventHandler *handler;
};

static void manager_destroy(struct animation_manager *manager);
static void storyboard_destroy(struct animation_storyboard *storyboard);
static void variable_destroy(struct animation_variable *variable);

static struct animation_manager *manager_from_iface(IUIAnimationManager *iface)
{
    return CONTAINING_RECORD(iface, struct animation_manager, IUIAnimationManager_iface);
}

static HRESULT WINAPI manager_QueryInterface(IUIAnimationManager *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualIID(iid, &IID_IUnknown) && !IsEqualIID(iid, &IID_IUIAnimationManager)) return E_NOINTERFACE;
    IUIAnimationManager_AddRef(iface);
    *out = iface;
    return S_OK;
}

static ULONG WINAPI manager_AddRef(IUIAnimationManager *iface)
{
    return InterlockedIncrement(&manager_from_iface(iface)->ref);
}

static ULONG WINAPI manager_Release(IUIAnimationManager *iface)
{
    struct animation_manager *object = manager_from_iface(iface);
    ULONG ref = InterlockedDecrement(&object->ref);
    if (!ref) manager_destroy(object);
    return ref;
}

static struct animation_storyboard *storyboard_from_iface(IUIAnimationStoryboard *iface)
{
    return CONTAINING_RECORD(iface, struct animation_storyboard, IUIAnimationStoryboard_iface);
}

static HRESULT WINAPI storyboard_QueryInterface(IUIAnimationStoryboard *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualIID(iid, &IID_IUnknown) && !IsEqualIID(iid, &IID_IUIAnimationStoryboard)) return E_NOINTERFACE;
    IUIAnimationStoryboard_AddRef(iface);
    *out = iface;
    return S_OK;
}

static ULONG WINAPI storyboard_AddRef(IUIAnimationStoryboard *iface)
{
    return InterlockedIncrement(&storyboard_from_iface(iface)->ref);
}

static ULONG WINAPI storyboard_Release(IUIAnimationStoryboard *iface)
{
    struct animation_storyboard *object = storyboard_from_iface(iface);
    ULONG ref = InterlockedDecrement(&object->ref);
    if (!ref) storyboard_destroy(object);
    return ref;
}

static struct animation_variable *variable_from_iface(IUIAnimationVariable *iface)
{
    return CONTAINING_RECORD(iface, struct animation_variable, IUIAnimationVariable_iface);
}

static HRESULT WINAPI variable_QueryInterface(IUIAnimationVariable *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualIID(iid, &IID_IUnknown) && !IsEqualIID(iid, &IID_IUIAnimationVariable)) return E_NOINTERFACE;
    IUIAnimationVariable_AddRef(iface);
    *out = iface;
    return S_OK;
}

static ULONG WINAPI variable_AddRef(IUIAnimationVariable *iface)
{
    return InterlockedIncrement(&variable_from_iface(iface)->ref);
}

static ULONG WINAPI variable_Release(IUIAnimationVariable *iface)
{
    struct animation_variable *object = variable_from_iface(iface);
    ULONG ref = InterlockedDecrement(&object->ref);
    if (!ref) variable_destroy(object);
    return ref;
}

static struct animation_transition *transition_from_iface(IUIAnimationTransition *iface)
{
    return CONTAINING_RECORD(iface, struct animation_transition, IUIAnimationTransition_iface);
}

static HRESULT WINAPI transition_QueryInterface(IUIAnimationTransition *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualIID(iid, &IID_IUnknown) && !IsEqualIID(iid, &IID_IUIAnimationTransition)) return E_NOINTERFACE;
    IUIAnimationTransition_AddRef(iface);
    *out = iface;
    return S_OK;
}

static ULONG WINAPI transition_AddRef(IUIAnimationTransition *iface)
{
    return InterlockedIncrement(&transition_from_iface(iface)->ref);
}

static ULONG WINAPI transition_Release(IUIAnimationTransition *iface)
{
    struct animation_transition *object = transition_from_iface(iface);
    ULONG ref = InterlockedDecrement(&object->ref);
    if (!ref) free(object);
    return ref;
}

static BOOL variable_shutdown(struct animation_variable *variable)
{
    return !variable->manager || variable->manager->shutdown;
}

static BOOL storyboard_shutdown(struct animation_storyboard *storyboard)
{
    return !storyboard->manager || storyboard->manager->shutdown;
}

static INT32 variable_round(struct animation_variable *variable, double value)
{
    switch (variable->rounding)
    {
        case UI_ANIMATION_ROUNDING_FLOOR: value = floor(value); break;
        case UI_ANIMATION_ROUNDING_CEILING: value = ceil(value); break;
        default: value = floor(value + .5); break;
    }
    return min(max(value, INT_MIN), INT_MAX);
}

static void variable_destroy(struct animation_variable *variable)
{
    if (variable->manager) list_remove(&variable->entry);
    if (variable->handler) IUIAnimationVariableChangeHandler_Release(variable->handler);
    if (variable->integer_handler) IUIAnimationVariableIntegerChangeHandler_Release(variable->integer_handler);
    free(variable);
}

static void storyboard_destroy(struct animation_storyboard *storyboard)
{
    struct storyboard_transition *entry, *next;
    struct storyboard_keyframe *keyframe, *next_keyframe;
    if (storyboard->manager) list_remove(&storyboard->entry);
    LIST_FOR_EACH_ENTRY_SAFE(keyframe, next_keyframe, &storyboard->keyframes, struct storyboard_keyframe, entry)
        free(keyframe);
    LIST_FOR_EACH_ENTRY_SAFE(entry, next, &storyboard->transitions, struct storyboard_transition, entry)
    {
        IUIAnimationVariable_Release(&entry->variable->IUIAnimationVariable_iface);
        IUIAnimationTransition_Release(&entry->transition->IUIAnimationTransition_iface);
        free(entry);
    }
    if (storyboard->handler) IUIAnimationStoryboardEventHandler_Release(storyboard->handler);
    free(storyboard);
}

static void storyboard_set_status(struct animation_storyboard *storyboard, UI_ANIMATION_STORYBOARD_STATUS status)
{
    UI_ANIMATION_STORYBOARD_STATUS previous = storyboard->status;
    IUIAnimationStoryboardEventHandler *handler = storyboard->handler;
    if (status == previous) return;
    storyboard->status = status;
    if (handler)
    {
        IUIAnimationStoryboardEventHandler_AddRef(handler);
        IUIAnimationStoryboardEventHandler_OnStoryboardStatusChanged(handler,
                &storyboard->IUIAnimationStoryboard_iface, status, previous);
        IUIAnimationStoryboardEventHandler_Release(handler);
    }
}

static void manager_refresh_status(struct animation_manager *manager)
{
    struct animation_storyboard *storyboard;
    IUIAnimationManagerEventHandler *handler = manager->handler;
    UI_ANIMATION_MANAGER_STATUS status = UI_ANIMATION_MANAGER_IDLE, previous = manager->status;
    LIST_FOR_EACH_ENTRY(storyboard, &manager->storyboards, struct animation_storyboard, entry)
        if (storyboard->active) { status = UI_ANIMATION_MANAGER_BUSY; break; }
    if (previous == status) return;
    manager->status = status;
    if (handler)
    {
        IUIAnimationManagerEventHandler_AddRef(handler);
        IUIAnimationManagerEventHandler_OnManagerStatusChanged(handler, status, previous);
        IUIAnimationManagerEventHandler_Release(handler);
    }
}

static void storyboard_deactivate(struct animation_storyboard *storyboard, UI_ANIMATION_STORYBOARD_STATUS status)
{
    struct storyboard_transition *entry;
    if (!storyboard->active) return;
    storyboard->active = FALSE;
    LIST_FOR_EACH_ENTRY(entry, &storyboard->transitions, struct storyboard_transition, entry)
    {
        entry->transition->active = FALSE;
        if (entry->variable->current == storyboard)
        {
            entry->variable->current = NULL;
            entry->variable->velocity = 0;
            entry->variable->final = entry->variable->value;
        }
    }
    storyboard_set_status(storyboard, status);
    IUIAnimationStoryboard_Release(&storyboard->IUIAnimationStoryboard_iface);
}

static void manager_detach_objects(struct animation_manager *manager)
{
    struct animation_storyboard *storyboard;
    struct animation_variable *variable;
    struct storyboard_transition *entry;
    while (!list_empty(&manager->storyboards))
    {
        storyboard = LIST_ENTRY(list_head(&manager->storyboards), struct animation_storyboard, entry);
        list_remove(&storyboard->entry);
        storyboard->manager = NULL;
        LIST_FOR_EACH_ENTRY(entry, &storyboard->transitions, struct storyboard_transition, entry)
        {
            entry->transition->shutdown = TRUE;
            entry->transition->active = FALSE;
            entry->variable->current = NULL;
        }
        if (storyboard->active)
        {
            storyboard->active = FALSE;
            storyboard_set_status(storyboard, UI_ANIMATION_STORYBOARD_READY);
            IUIAnimationStoryboard_Release(&storyboard->IUIAnimationStoryboard_iface);
        }
    }
    while (!list_empty(&manager->variables))
    {
        variable = LIST_ENTRY(list_head(&manager->variables), struct animation_variable, entry);
        list_remove(&variable->entry);
        variable->manager = NULL;
    }
}

static void manager_destroy(struct animation_manager *manager)
{
    manager_detach_objects(manager);
    if (manager->handler) IUIAnimationManagerEventHandler_Release(manager->handler);
    free(manager);
}

static HRESULT WINAPI transition_SetInitialValue(IUIAnimationTransition *iface, double value)
{
    struct animation_transition *transition = transition_from_iface(iface);
    if (transition->shutdown) return UI_E_SHUTDOWN_CALLED;
    if (transition->active) return UI_E_STORYBOARD_ACTIVE;
    if (!isfinite(value)) return E_INVALIDARG;
    transition->initial = value;
    transition->initial_set = TRUE;
    return S_OK;
}

static HRESULT WINAPI transition_SetInitialVelocity(IUIAnimationTransition *iface, double velocity)
{
    struct animation_transition *transition = transition_from_iface(iface);
    if (transition->shutdown) return UI_E_SHUTDOWN_CALLED;
    if (transition->active) return UI_E_STORYBOARD_ACTIVE;
    if (!isfinite(velocity)) return E_INVALIDARG;
    transition->velocity = velocity;
    transition->velocity_set = TRUE;
    return S_OK;
}

static HRESULT WINAPI transition_IsDurationKnown(IUIAnimationTransition *iface)
{
    struct animation_transition *transition = transition_from_iface(iface);
    if (transition->shutdown) return UI_E_SHUTDOWN_CALLED;
    if (transition->active) return UI_E_STORYBOARD_ACTIVE;
    return transition->kind == TRANSITION_SMOOTH_STOP ? S_FALSE : S_OK;
}

static HRESULT WINAPI transition_GetDuration(IUIAnimationTransition *iface, double *duration)
{
    HRESULT hr;
    if (!duration) return E_POINTER;
    *duration = 0;
    if (FAILED(hr = transition_IsDurationKnown(iface))) return hr;
    if (hr == S_FALSE) return UI_E_VALUE_NOT_DETERMINED;
    *duration = transition_from_iface(iface)->duration;
    return S_OK;
}

static const IUIAnimationTransitionVtbl transition_vtbl =
{
    transition_QueryInterface, transition_AddRef, transition_Release,
    transition_SetInitialValue, transition_SetInitialVelocity,
    transition_IsDurationKnown, transition_GetDuration,
};

HRESULT animation_transition_create(enum animation_transition_kind kind, double duration,
        double final_value, IUIAnimationTransition **out)
{
    struct animation_transition *transition;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!isfinite(duration) || duration < 0 || !isfinite(final_value)) return E_INVALIDARG;
    if (!(transition = calloc(1, sizeof(*transition)))) return E_OUTOFMEMORY;
    transition->IUIAnimationTransition_iface.lpVtbl = &transition_vtbl;
    transition->ref = 1;
    transition->kind = kind;
    transition->duration = duration;
    transition->final = final_value;
    *out = &transition->IUIAnimationTransition_iface;
    return S_OK;
}

static HRESULT WINAPI variable_GetValue(IUIAnimationVariable *iface, double *value)
{
    struct animation_variable *variable = variable_from_iface(iface);
    if (!value) return E_POINTER;
    *value = 0;
    if (variable_shutdown(variable)) return UI_E_SHUTDOWN_CALLED;
    *value = variable->value;
    return S_OK;
}

static HRESULT WINAPI variable_GetFinalValue(IUIAnimationVariable *iface, double *value)
{
    struct animation_variable *variable = variable_from_iface(iface);
    if (!value) return E_POINTER;
    *value = 0;
    if (variable_shutdown(variable)) return UI_E_SHUTDOWN_CALLED;
    *value = variable->final;
    return S_OK;
}

static HRESULT WINAPI variable_GetPreviousValue(IUIAnimationVariable *iface, double *value)
{
    struct animation_variable *variable = variable_from_iface(iface);
    if (!value) return E_POINTER;
    *value = 0;
    if (variable_shutdown(variable)) return UI_E_SHUTDOWN_CALLED;
    *value = variable->previous;
    return S_OK;
}

static HRESULT WINAPI variable_GetIntegerValue(IUIAnimationVariable *iface, INT32 *value)
{
    struct animation_variable *variable = variable_from_iface(iface);
    if (!value) return E_POINTER;
    *value = 0;
    if (variable_shutdown(variable)) return UI_E_SHUTDOWN_CALLED;
    *value = variable_round(variable, variable->value);
    return S_OK;
}

static HRESULT WINAPI variable_GetFinalIntegerValue(IUIAnimationVariable *iface, INT32 *value)
{
    struct animation_variable *variable = variable_from_iface(iface);
    if (!value) return E_POINTER;
    *value = 0;
    if (variable_shutdown(variable)) return UI_E_SHUTDOWN_CALLED;
    *value = variable_round(variable, variable->final);
    return S_OK;
}

static HRESULT WINAPI variable_GetPreviousIntegerValue(IUIAnimationVariable *iface, INT32 *value)
{
    struct animation_variable *variable = variable_from_iface(iface);
    if (!value) return E_POINTER;
    *value = 0;
    if (variable_shutdown(variable)) return UI_E_SHUTDOWN_CALLED;
    *value = variable_round(variable, variable->previous);
    return S_OK;
}

static HRESULT WINAPI variable_GetCurrentStoryboard(IUIAnimationVariable *iface, IUIAnimationStoryboard **out)
{
    struct animation_variable *variable = variable_from_iface(iface);
    if (!out) return E_POINTER;
    *out = NULL;
    if (variable_shutdown(variable)) return UI_E_SHUTDOWN_CALLED;
    if (variable->current) IUIAnimationStoryboard_AddRef(*out = &variable->current->IUIAnimationStoryboard_iface);
    return S_OK;
}

static HRESULT WINAPI variable_SetLowerBound(IUIAnimationVariable *iface, double bound)
{
    struct animation_variable *variable = variable_from_iface(iface);
    if (variable_shutdown(variable)) return UI_E_SHUTDOWN_CALLED;
    if (!isfinite(bound) || bound > variable->upper) return E_INVALIDARG;
    variable->lower = bound;
    return S_OK;
}

static HRESULT WINAPI variable_SetUpperBound(IUIAnimationVariable *iface, double bound)
{
    struct animation_variable *variable = variable_from_iface(iface);
    if (variable_shutdown(variable)) return UI_E_SHUTDOWN_CALLED;
    if (!isfinite(bound) || bound < variable->lower) return E_INVALIDARG;
    variable->upper = bound;
    return S_OK;
}

static HRESULT WINAPI variable_SetRoundingMode(IUIAnimationVariable *iface, UI_ANIMATION_ROUNDING_MODE mode)
{
    struct animation_variable *variable = variable_from_iface(iface);
    if (variable_shutdown(variable)) return UI_E_SHUTDOWN_CALLED;
    if (mode > UI_ANIMATION_ROUNDING_CEILING) return E_INVALIDARG;
    variable->rounding = mode;
    return S_OK;
}

static HRESULT WINAPI variable_SetTag(IUIAnimationVariable *iface, IUnknown *object, UINT32 id)
{
    FIXME("iface %p, object %p, id %u: not implemented.\n", iface, object, id);
    return E_NOTIMPL;
}

static HRESULT WINAPI variable_GetTag(IUIAnimationVariable *iface, IUnknown **object, UINT32 *id)
{
    FIXME("iface %p, object %p, id %p: not implemented.\n", iface, object, id);
    return E_NOTIMPL;
}

static HRESULT WINAPI storyboard_SetTag(IUIAnimationStoryboard *iface, IUnknown *object, UINT32 id)
{
    FIXME("iface %p, object %p, id %u: not implemented.\n", iface, object, id);
    return E_NOTIMPL;
}

static HRESULT WINAPI storyboard_GetTag(IUIAnimationStoryboard *iface, IUnknown **object, UINT32 *id)
{
    FIXME("iface %p, object %p, id %p: not implemented.\n", iface, object, id);
    return E_NOTIMPL;
}

static HRESULT WINAPI variable_SetVariableChangeHandler(IUIAnimationVariable *iface, IUIAnimationVariableChangeHandler *handler)
{
    struct animation_variable *object = variable_from_iface(iface);
    IUIAnimationVariableChangeHandler *previous = object->handler;
    if (variable_shutdown(object)) return UI_E_SHUTDOWN_CALLED;
    if (handler) IUIAnimationVariableChangeHandler_AddRef(handler);
    object->handler = handler;
    if (previous) IUIAnimationVariableChangeHandler_Release(previous);
    return S_OK;
}

static HRESULT WINAPI variable_SetVariableIntegerChangeHandler(IUIAnimationVariable *iface, IUIAnimationVariableIntegerChangeHandler *handler)
{
    struct animation_variable *object = variable_from_iface(iface);
    IUIAnimationVariableIntegerChangeHandler *previous = object->integer_handler;
    if (variable_shutdown(object)) return UI_E_SHUTDOWN_CALLED;
    if (handler) IUIAnimationVariableIntegerChangeHandler_AddRef(handler);
    object->integer_handler = handler;
    if (previous) IUIAnimationVariableIntegerChangeHandler_Release(previous);
    return S_OK;
}

static HRESULT WINAPI storyboard_SetStoryboardEventHandler(IUIAnimationStoryboard *iface, IUIAnimationStoryboardEventHandler *handler)
{
    struct animation_storyboard *object = storyboard_from_iface(iface);
    IUIAnimationStoryboardEventHandler *previous = object->handler;
    if (storyboard_shutdown(object)) return UI_E_SHUTDOWN_CALLED;
    if (handler) IUIAnimationStoryboardEventHandler_AddRef(handler);
    object->handler = handler;
    if (previous) IUIAnimationStoryboardEventHandler_Release(previous);
    return S_OK;
}

static HRESULT WINAPI manager_SetManagerEventHandler(IUIAnimationManager *iface, IUIAnimationManagerEventHandler *handler)
{
    struct animation_manager *object = manager_from_iface(iface);
    IUIAnimationManagerEventHandler *previous = object->handler;
    if (object->shutdown) return UI_E_SHUTDOWN_CALLED;
    if (handler) IUIAnimationManagerEventHandler_AddRef(handler);
    object->handler = handler;
    if (previous) IUIAnimationManagerEventHandler_Release(previous);
    return S_OK;
}

static const IUIAnimationVariableVtbl variable_vtbl =
{
    variable_QueryInterface, variable_AddRef, variable_Release,
    variable_GetValue, variable_GetFinalValue, variable_GetPreviousValue,
    variable_GetIntegerValue, variable_GetFinalIntegerValue, variable_GetPreviousIntegerValue,
    variable_GetCurrentStoryboard, variable_SetLowerBound, variable_SetUpperBound,
    variable_SetRoundingMode, variable_SetTag, variable_GetTag,
    variable_SetVariableChangeHandler, variable_SetVariableIntegerChangeHandler,
};

static HRESULT WINAPI storyboard_AddTransition(IUIAnimationStoryboard *iface, IUIAnimationVariable *variable_iface,
        IUIAnimationTransition *transition_iface)
{
    struct animation_storyboard *storyboard = storyboard_from_iface(iface);
    struct animation_variable *variable;
    struct animation_transition *transition;
    struct storyboard_transition *entry;
    if (storyboard_shutdown(storyboard)) return UI_E_SHUTDOWN_CALLED;
    if (storyboard->status != UI_ANIMATION_STORYBOARD_BUILDING) return UI_E_OBJECT_SEALED;
    if (!variable_iface || !transition_iface) return E_POINTER;
    if (variable_iface->lpVtbl != &variable_vtbl || transition_iface->lpVtbl != &transition_vtbl) return E_NOTIMPL;
    variable = variable_from_iface(variable_iface);
    transition = transition_from_iface(transition_iface);
    if (variable->manager != storyboard->manager) return UI_E_DIFFERENT_OWNER;
    if (transition->shutdown) return UI_E_SHUTDOWN_CALLED;
    if (transition->used) return UI_E_TRANSITION_ALREADY_USED;
    if (!(entry = calloc(1, sizeof(*entry)))) return E_OUTOFMEMORY;
    entry->variable = variable;
    entry->transition = transition;
    IUIAnimationVariable_AddRef(variable_iface);
    IUIAnimationTransition_AddRef(transition_iface);
    transition->used = TRUE;
    list_add_tail(&storyboard->transitions, &entry->entry);
    return S_OK;
}

static HRESULT storyboard_get_keyframe(struct animation_storyboard *storyboard,
        UI_ANIMATION_KEYFRAME handle, struct storyboard_keyframe **result)
{
    struct storyboard_keyframe *keyframe;
    *result = NULL;
    if (handle == UI_ANIMATION_KEYFRAME_STORYBOARD_START) return S_OK;
    LIST_FOR_EACH_ENTRY(keyframe, &storyboard->keyframes, struct storyboard_keyframe, entry)
        if (keyframe->index == (UINT_PTR)handle) { *result = keyframe; return S_OK; }
    return E_INVALIDARG;
}

static double keyframe_time(struct storyboard_keyframe *keyframe)
{
    double time = 0;
    /* Keyframes only reference previously added keys/transitions. Follow offset
     * chains iteratively so long chains do not consume the C call stack. */
    while (keyframe)
    {
        time += keyframe->offset;
        if (keyframe->after) return time + keyframe->after->start + keyframe->after->duration;
        keyframe = keyframe->parent;
    }
    return time;
}

static HRESULT storyboard_add_keyframe(struct animation_storyboard *storyboard,
        struct storyboard_keyframe *parent, struct storyboard_transition *after,
        double offset, UI_ANIMATION_KEYFRAME *result)
{
    struct storyboard_keyframe *keyframe;
    if (!(keyframe = calloc(1, sizeof(*keyframe)))) return E_OUTOFMEMORY;
    keyframe->parent = parent;
    keyframe->after = after;
    keyframe->offset = offset;
    keyframe->index = storyboard->keyframe_count++;
    list_add_tail(&storyboard->keyframes, &keyframe->entry);
    *result = (UI_ANIMATION_KEYFRAME)keyframe->index;
    return S_OK;
}

static HRESULT WINAPI storyboard_AddKeyframeAtOffset(IUIAnimationStoryboard *iface, UI_ANIMATION_KEYFRAME existing,
        double offset, UI_ANIMATION_KEYFRAME *keyframe)
{
    struct animation_storyboard *storyboard = storyboard_from_iface(iface);
    struct storyboard_keyframe *parent;
    HRESULT hr;
    if (!keyframe) return E_POINTER;
    *keyframe = UI_ANIMATION_KEYFRAME_STORYBOARD_START;
    if (storyboard_shutdown(storyboard)) return UI_E_SHUTDOWN_CALLED;
    if (storyboard->status != UI_ANIMATION_STORYBOARD_BUILDING) return UI_E_OBJECT_SEALED;
    if (FAILED(hr = storyboard_get_keyframe(storyboard, existing, &parent))) return hr;
    return storyboard_add_keyframe(storyboard, parent, NULL, offset, keyframe);
}

static HRESULT WINAPI storyboard_AddKeyframeAfterTransition(IUIAnimationStoryboard *iface,
        IUIAnimationTransition *transition, UI_ANIMATION_KEYFRAME *keyframe)
{
    struct animation_storyboard *storyboard = storyboard_from_iface(iface);
    struct storyboard_transition *entry;
    if (!keyframe) return E_POINTER;
    *keyframe = UI_ANIMATION_KEYFRAME_STORYBOARD_START;
    if (storyboard_shutdown(storyboard)) return UI_E_SHUTDOWN_CALLED;
    if (storyboard->status != UI_ANIMATION_STORYBOARD_BUILDING) return UI_E_OBJECT_SEALED;
    if (!transition) return E_POINTER;
    LIST_FOR_EACH_ENTRY(entry, &storyboard->transitions, struct storyboard_transition, entry)
        if (&entry->transition->IUIAnimationTransition_iface == transition)
            return storyboard_add_keyframe(storyboard, NULL, entry, 0, keyframe);
    return UI_E_TRANSITION_NOT_IN_STORYBOARD;
}

static HRESULT WINAPI storyboard_AddTransitionAtKeyframe(IUIAnimationStoryboard *iface, IUIAnimationVariable *variable,
        IUIAnimationTransition *transition, UI_ANIMATION_KEYFRAME keyframe)
{
    struct animation_storyboard *storyboard = storyboard_from_iface(iface);
    struct storyboard_keyframe *start;
    struct storyboard_transition *entry;
    HRESULT hr;
    if (storyboard_shutdown(storyboard)) return UI_E_SHUTDOWN_CALLED;
    if (storyboard->status != UI_ANIMATION_STORYBOARD_BUILDING) return UI_E_OBJECT_SEALED;
    if (FAILED(hr = storyboard_get_keyframe(storyboard, keyframe, &start))) return hr;
    if (FAILED(hr = storyboard_AddTransition(iface, variable, transition))) return hr;
    entry = LIST_ENTRY(list_tail(&storyboard->transitions), struct storyboard_transition, entry);
    entry->start_key = start;
    entry->at_keyframe = TRUE;
    return S_OK;
}

static HRESULT WINAPI storyboard_AddTransitionBetweenKeyframes(IUIAnimationStoryboard *iface, IUIAnimationVariable *variable,
        IUIAnimationTransition *transition, UI_ANIMATION_KEYFRAME start_handle, UI_ANIMATION_KEYFRAME end_handle)
{
    struct animation_storyboard *storyboard = storyboard_from_iface(iface);
    struct storyboard_keyframe *end;
    struct storyboard_transition *entry;
    HRESULT hr;
    if (storyboard_shutdown(storyboard)) return UI_E_SHUTDOWN_CALLED;
    if (storyboard->status != UI_ANIMATION_STORYBOARD_BUILDING) return UI_E_OBJECT_SEALED;
    if (FAILED(hr = storyboard_get_keyframe(storyboard, end_handle, &end))) return hr;
    if (FAILED(hr = storyboard_AddTransitionAtKeyframe(iface, variable, transition, start_handle))) return hr;
    entry = LIST_ENTRY(list_tail(&storyboard->transitions), struct storyboard_transition, entry);
    entry->end_key = end;
    entry->between_keyframes = TRUE;
    return S_OK;
}

static HRESULT storyboard_resolve_timing(struct animation_storyboard *storyboard);

static HRESULT WINAPI storyboard_RepeatBetweenKeyframes(IUIAnimationStoryboard *iface, UI_ANIMATION_KEYFRAME start,
        UI_ANIMATION_KEYFRAME end, INT32 count)
{
    struct animation_storyboard *storyboard = storyboard_from_iface(iface);
    struct storyboard_keyframe *start_key, *end_key;
    double start_time, end_time;
    HRESULT hr;
    if (storyboard_shutdown(storyboard)) return UI_E_SHUTDOWN_CALLED;
    if (storyboard->status != UI_ANIMATION_STORYBOARD_BUILDING) return UI_E_OBJECT_SEALED;
    if (count < -1) return E_INVALIDARG;
    if (FAILED(hr = storyboard_get_keyframe(storyboard, start, &start_key))) return hr;
    if (FAILED(hr = storyboard_get_keyframe(storyboard, end, &end_key))) return hr;
    if (FAILED(hr = storyboard_resolve_timing(storyboard))) return hr;
    start_time = keyframe_time(start_key);
    end_time = keyframe_time(end_key);
    if (start_time > end_time) return UI_E_START_KEYFRAME_AFTER_END;
    if (start_key == end_key) return S_OK;
    if (storyboard->has_loop)
    {
        if (start_time == keyframe_time(storyboard->loop_start_key)
                && end_time == keyframe_time(storyboard->loop_end_key)) return UI_E_LOOPS_OVERLAP;
        FIXME("Multiple animation loops are not implemented.\n");
        return E_NOTIMPL;
    }
    storyboard->loop_start_key = start_key;
    storyboard->loop_end_key = end_key;
    storyboard->loop_count = count;
    storyboard->has_loop = TRUE;
    return S_OK;
}

static HRESULT WINAPI storyboard_HoldVariable(IUIAnimationStoryboard *iface, IUIAnimationVariable *variable)
{
    FIXME("iface %p, variable %p: not implemented.\n", iface, variable);
    return E_NOTIMPL;
}

static HRESULT WINAPI storyboard_SetLongestAcceptableDelay(IUIAnimationStoryboard *iface, double delay)
{
    struct animation_storyboard *storyboard = storyboard_from_iface(iface);
    if (storyboard_shutdown(storyboard)) return UI_E_SHUTDOWN_CALLED;
    if (!isfinite(delay) || (delay < 0 && delay != UI_ANIMATION_SECONDS_EVENTUALLY)) return E_INVALIDARG;
    if (storyboard->active) return UI_E_STORYBOARD_ACTIVE;
    storyboard->delay = delay;
    return S_OK;
}

static double transition_duration(struct animation_transition *transition, double initial, double velocity)
{
    double duration = transition->duration, distance = transition->final - initial, stop_time;
    if (transition->kind != TRANSITION_SMOOTH_STOP) return duration;
    if (!distance) return 0;
    if (velocity && (stop_time = 2 * distance / velocity) > 0) duration = min(duration, stop_time);
    return duration;
}

static HRESULT storyboard_resolve_timing(struct animation_storyboard *storyboard)
{
    struct storyboard_transition *entry, *previous;
    storyboard->duration = 0;
    storyboard->first_start = DBL_MAX;
    LIST_FOR_EACH_ENTRY(entry, &storyboard->transitions, struct storyboard_transition, entry)
    {
        entry->start = 0;
        entry->initial = entry->variable->value;
        entry->velocity = entry->variable->velocity;
        LIST_FOR_EACH_ENTRY(previous, &storyboard->transitions, struct storyboard_transition, entry)
        {
            if (previous == entry) break;
            if (previous->variable != entry->variable) continue;
            entry->start = previous->start + previous->duration;
            entry->initial = previous->transition->final;
            entry->velocity = previous->transition->kind == TRANSITION_LINEAR && previous->duration
                    ? (previous->transition->final - previous->initial) / previous->duration : 0;
        }
        if (entry->at_keyframe) entry->start = keyframe_time(entry->start_key);
        if (!isfinite(entry->start)) return E_INVALIDARG;
        if (entry->transition->initial_set) entry->initial = entry->transition->initial;
        if (entry->transition->velocity_set) entry->velocity = entry->transition->velocity;
        entry->duration = transition_duration(entry->transition, entry->initial, entry->velocity);
        if (entry->between_keyframes) entry->duration = keyframe_time(entry->end_key) - entry->start;
        if (!isfinite(entry->duration)) return E_INVALIDARG;
        if (entry->duration < 0) return UI_E_START_KEYFRAME_AFTER_END;
        storyboard->duration = max(storyboard->duration, entry->start + entry->duration);
        storyboard->first_start = min(storyboard->first_start, entry->start);
    }
    storyboard->base_duration = storyboard->duration;
    return S_OK;
}

static HRESULT WINAPI storyboard_Schedule(IUIAnimationStoryboard *iface, double now, UI_ANIMATION_SCHEDULING_RESULT *result)
{
    struct animation_storyboard *storyboard = storyboard_from_iface(iface);
    struct animation_manager *manager = storyboard->manager;
    struct storyboard_transition *entry;
    HRESULT hr;
    if (result) *result = UI_ANIMATION_SCHEDULING_UNEXPECTED_FAILURE;
    if (storyboard_shutdown(storyboard)) return UI_E_SHUTDOWN_CALLED;
    if (!isfinite(now) || now < 0) return E_INVALIDARG;
    if (manager->updating) return UI_E_ILLEGAL_REENTRANCY;
    if (storyboard->active)
    {
        if (result) *result = UI_ANIMATION_SCHEDULING_ALREADY_SCHEDULED;
        return S_OK;
    }
    if (manager->have_time && now < manager->time) return UI_E_TIME_BEFORE_LAST_UPDATE;
    /* Conflict arbitration requires the application's priority comparisons. */
    LIST_FOR_EACH_ENTRY(entry, &storyboard->transitions, struct storyboard_transition, entry)
        if (entry->variable->current) return E_NOTIMPL;
    if (FAILED(hr = storyboard_resolve_timing(storyboard))) return hr;
    if (storyboard->has_loop)
    {
        double period;
        storyboard->loop_start = keyframe_time(storyboard->loop_start_key);
        storyboard->loop_end = keyframe_time(storyboard->loop_end_key);
        period = storyboard->loop_end - storyboard->loop_start;
        if (!isfinite(period)) return E_INVALIDARG;
        if (period < 0) return UI_E_START_KEYFRAME_AFTER_END;
        if (period && storyboard->loop_count == -1) storyboard->duration = DBL_MAX;
        else storyboard->duration += period * (storyboard->loop_count - 1.0);
        if (!isfinite(storyboard->duration)) return UI_E_FP_OVERFLOW;
    }
    LIST_FOR_EACH_ENTRY(entry, &storyboard->transitions, struct storyboard_transition, entry)
    {
        if (entry->between_keyframes) entry->transition->duration = entry->duration;
        entry->transition->active = TRUE;
        entry->variable->final = min(max(entry->transition->final,entry->variable->lower),entry->variable->upper);
        entry->variable->current = storyboard;
    }
    storyboard->start = now;
    storyboard->elapsed = 0;
    storyboard->active = TRUE;
    IUIAnimationStoryboard_AddRef(iface); /* Scheduling retains the storyboard until completion. */
    IUIAnimationStoryboard_AddRef(iface); /* Callbacks may release or abandon the caller's storyboard. */
    IUIAnimationManager_AddRef(&manager->IUIAnimationManager_iface);
    storyboard_set_status(storyboard, UI_ANIMATION_STORYBOARD_SCHEDULED);
    if (result) *result = UI_ANIMATION_SCHEDULING_SUCCEEDED;
    manager_refresh_status(manager);
    IUIAnimationStoryboard_Release(iface);
    IUIAnimationManager_Release(&manager->IUIAnimationManager_iface);
    return S_OK;
}

static HRESULT WINAPI storyboard_Conclude(IUIAnimationStoryboard *iface)
{
    struct animation_storyboard *storyboard = storyboard_from_iface(iface);
    double period, count;
    if (storyboard_shutdown(storyboard)) return UI_E_SHUTDOWN_CALLED;
    if (!storyboard->active || !storyboard->has_loop || storyboard->loop_count != -1) return S_OK;
    period = storyboard->loop_end - storyboard->loop_start;
    if (!period) return S_OK;
    count = max(0, ceil((storyboard->elapsed - storyboard->loop_start) / period));
    if (count > INT_MAX) return UI_E_FP_OVERFLOW;
    storyboard->loop_count = count;
    storyboard->duration = storyboard->base_duration + period * (count - 1);
    return S_OK;
}

static HRESULT WINAPI storyboard_Finish(IUIAnimationStoryboard *iface, double deadline)
{
    FIXME("iface %p, deadline %g: not implemented.\n", iface, deadline);
    return E_NOTIMPL;
}

static HRESULT WINAPI storyboard_Abandon(IUIAnimationStoryboard *iface)
{
    struct animation_storyboard *storyboard = storyboard_from_iface(iface);
    struct animation_manager *manager = storyboard->manager;
    if (storyboard_shutdown(storyboard)) return UI_E_SHUTDOWN_CALLED;
    if (manager->updating) return UI_E_ILLEGAL_REENTRANCY;
    IUIAnimationStoryboard_AddRef(iface);
    IUIAnimationManager_AddRef(&manager->IUIAnimationManager_iface);
    storyboard_deactivate(storyboard, UI_ANIMATION_STORYBOARD_CANCELLED);
    manager_refresh_status(manager);
    IUIAnimationStoryboard_Release(iface);
    IUIAnimationManager_Release(&manager->IUIAnimationManager_iface);
    return S_OK;
}

static HRESULT WINAPI storyboard_GetStatus(IUIAnimationStoryboard *iface, UI_ANIMATION_STORYBOARD_STATUS *status)
{
    struct animation_storyboard *storyboard = storyboard_from_iface(iface);
    if (!status) return E_POINTER;
    *status = UI_ANIMATION_STORYBOARD_BUILDING;
    if (storyboard_shutdown(storyboard)) return UI_E_SHUTDOWN_CALLED;
    *status = storyboard->status;
    return S_OK;
}

static HRESULT WINAPI storyboard_GetElapsedTime(IUIAnimationStoryboard *iface, double *elapsed)
{
    struct animation_storyboard *storyboard = storyboard_from_iface(iface);
    if (!elapsed) return E_POINTER;
    *elapsed = 0;
    if (storyboard_shutdown(storyboard)) return UI_E_SHUTDOWN_CALLED;
    if (storyboard->status != UI_ANIMATION_STORYBOARD_PLAYING) return UI_E_STORYBOARD_NOT_PLAYING;
    *elapsed = storyboard->elapsed;
    return S_OK;
}

static const IUIAnimationStoryboardVtbl storyboard_vtbl =
{
    storyboard_QueryInterface, storyboard_AddRef, storyboard_Release, storyboard_AddTransition,
    storyboard_AddKeyframeAtOffset, storyboard_AddKeyframeAfterTransition, storyboard_AddTransitionAtKeyframe,
    storyboard_AddTransitionBetweenKeyframes, storyboard_RepeatBetweenKeyframes, storyboard_HoldVariable,
    storyboard_SetLongestAcceptableDelay, storyboard_Schedule, storyboard_Conclude, storyboard_Finish,
    storyboard_Abandon, storyboard_SetTag, storyboard_GetTag, storyboard_GetStatus,
    storyboard_GetElapsedTime, storyboard_SetStoryboardEventHandler,
};

static HRESULT WINAPI manager_CreateAnimationVariable(IUIAnimationManager *iface, double initial, IUIAnimationVariable **out)
{
    struct animation_manager *manager = manager_from_iface(iface);
    struct animation_variable *variable;
    if (!out) return E_POINTER;
    *out = NULL;
    if (manager->shutdown) return UI_E_SHUTDOWN_CALLED;
    if (!isfinite(initial)) return E_INVALIDARG;
    if (!(variable = calloc(1, sizeof(*variable)))) return E_OUTOFMEMORY;
    variable->IUIAnimationVariable_iface.lpVtbl = &variable_vtbl;
    variable->ref = 1;
    variable->manager = manager;
    variable->value = variable->previous = variable->final = initial;
    variable->lower = -DBL_MAX;
    variable->upper = DBL_MAX;
    list_add_tail(&manager->variables, &variable->entry);
    *out = &variable->IUIAnimationVariable_iface;
    return S_OK;
}

static HRESULT WINAPI manager_CreateStoryboard(IUIAnimationManager *iface, IUIAnimationStoryboard **out)
{
    struct animation_manager *manager = manager_from_iface(iface);
    struct animation_storyboard *storyboard;
    if (!out) return E_POINTER;
    *out = NULL;
    if (manager->shutdown) return UI_E_SHUTDOWN_CALLED;
    if (!(storyboard = calloc(1, sizeof(*storyboard)))) return E_OUTOFMEMORY;
    storyboard->IUIAnimationStoryboard_iface.lpVtbl = &storyboard_vtbl;
    storyboard->ref = 1;
    storyboard->manager = manager;
    storyboard->delay = manager->delay;
    list_init(&storyboard->transitions);
    list_init(&storyboard->keyframes);
    list_add_tail(&manager->storyboards, &storyboard->entry);
    *out = &storyboard->IUIAnimationStoryboard_iface;
    return S_OK;
}

static HRESULT WINAPI manager_ScheduleTransition(IUIAnimationManager *iface, IUIAnimationVariable *variable,
        IUIAnimationTransition *transition, double now)
{
    IUIAnimationStoryboard *storyboard;
    HRESULT hr;
    if (FAILED(hr = manager_CreateStoryboard(iface, &storyboard))) return hr;
    if (SUCCEEDED(hr = storyboard_AddTransition(storyboard, variable, transition)))
        hr = storyboard_Schedule(storyboard, now, NULL);
    IUIAnimationStoryboard_Release(storyboard);
    return hr;
}

static HRESULT WINAPI manager_FinishAllStoryboards(IUIAnimationManager *iface, double deadline)
{
    FIXME("iface %p, deadline %g: not implemented.\n", iface, deadline);
    return E_NOTIMPL;
}

static HRESULT WINAPI manager_AbandonAllStoryboards(IUIAnimationManager *iface)
{
    struct animation_manager *manager = manager_from_iface(iface);
    struct animation_storyboard *storyboard;
    if (manager->shutdown) return UI_E_SHUTDOWN_CALLED;
    if (manager->updating) return UI_E_ILLEGAL_REENTRANCY;
    for (;;)
    {
        struct animation_storyboard *active = NULL;
        LIST_FOR_EACH_ENTRY(storyboard, &manager->storyboards, struct animation_storyboard, entry)
            if (storyboard->active) { active = storyboard; break; }
        if (!active) break;
        storyboard_deactivate(active, UI_ANIMATION_STORYBOARD_CANCELLED);
    }
    manager_refresh_status(manager);
    return S_OK;
}

static double transition_value(struct storyboard_transition *entry, double time, double *velocity)
{
    struct animation_transition *transition = entry->transition;
    double duration = entry->duration, delta = transition->final - entry->initial, t, a, b;
    if (!duration || time >= duration)
    {
        *velocity = transition->kind == TRANSITION_LINEAR && duration ? delta / duration : 0;
        return transition->final;
    }
    if (transition->kind == TRANSITION_LINEAR)
    {
        *velocity = delta / duration;
        return entry->initial + time * *velocity;
    }
    /* A cubic Hermite curve degenerates to the parabolic stop when T=2*dx/v. */
    t = time / duration;
    a = entry->velocity * duration;
    b = 3 * delta - 2 * a;
    *velocity = (a + t * (2 * b + t * 3 * (a - 2 * delta))) / duration;
    return entry->initial + t * (a + t * (b + t * (a - 2 * delta)));
}

static void variable_notify(struct animation_variable *variable, struct animation_storyboard *storyboard)
{
    IUIAnimationVariableChangeHandler *handler = variable->handler;
    IUIAnimationVariableIntegerChangeHandler *integer_handler = variable->integer_handler;
    INT32 previous_int = variable_round(variable, variable->previous), new_int;
    double value = variable->value;
    if (variable->previous == value) return;
    new_int = variable_round(variable, value);
    if (handler) IUIAnimationVariableChangeHandler_AddRef(handler);
    if (integer_handler) IUIAnimationVariableIntegerChangeHandler_AddRef(integer_handler);
    if (handler)
    {
        IUIAnimationVariableChangeHandler_OnValueChanged(handler, &storyboard->IUIAnimationStoryboard_iface,
                &variable->IUIAnimationVariable_iface, value, variable->previous);
        IUIAnimationVariableChangeHandler_Release(handler);
    }
    if (integer_handler)
    {
        if (new_int != previous_int)
            IUIAnimationVariableIntegerChangeHandler_OnIntegerValueChanged(integer_handler,
                    &storyboard->IUIAnimationStoryboard_iface, &variable->IUIAnimationVariable_iface, new_int, previous_int);
        IUIAnimationVariableIntegerChangeHandler_Release(integer_handler);
    }
}

static HRESULT WINAPI manager_Update(IUIAnimationManager *iface, double now, UI_ANIMATION_UPDATE_RESULT *result)
{
    struct animation_manager *manager = manager_from_iface(iface);
    struct animation_storyboard *storyboard, **active;
    struct animation_variable *variable;
    struct storyboard_transition *entry, *candidate;
    size_t count = 0, i;
    double elapsed, sample_time, value, velocity;
    BOOL changed = FALSE, later, storyboard_changed;
    if (result) *result = UI_ANIMATION_UPDATE_NO_CHANGE;
    if (manager->shutdown) return UI_E_SHUTDOWN_CALLED;
    if (manager->updating) return UI_E_ILLEGAL_REENTRANCY;
    if (!isfinite(now) || now < 0) return E_INVALIDARG;
    if (manager->have_time && now < manager->time) return UI_E_TIME_BEFORE_LAST_UPDATE;
    if (manager->paused || (manager->have_time && now == manager->time)) return S_OK;
    LIST_FOR_EACH_ENTRY(storyboard, &manager->storyboards, struct animation_storyboard, entry)
        if (storyboard->active) ++count;
    if (!(active = calloc(count ? count : 1, sizeof(*active)))) return E_OUTOFMEMORY;
    i = 0;
    LIST_FOR_EACH_ENTRY(storyboard, &manager->storyboards, struct animation_storyboard, entry)
    {
        if (!storyboard->active) continue;
        IUIAnimationStoryboard_AddRef(&storyboard->IUIAnimationStoryboard_iface);
        active[i++] = storyboard;
    }
    manager->updating = TRUE;
    manager->have_time = TRUE;
    manager->time = now;
    IUIAnimationManager_AddRef(iface);
    LIST_FOR_EACH_ENTRY(variable, &manager->variables, struct animation_variable, entry)
        variable->previous = variable->value;
    for (i = 0; i < count; ++i)
    {
        storyboard = active[i];
        elapsed = manager->mode == UI_ANIMATION_MODE_DISABLED || now >= storyboard->start + storyboard->duration
                ? storyboard->duration : now - storyboard->start;
        if (elapsed < 0) goto next;
        storyboard->elapsed = min(elapsed, storyboard->duration);
        storyboard_changed = FALSE;
        sample_time = elapsed;
        if (storyboard->has_loop && sample_time >= storyboard->loop_start)
        {
            double period = storyboard->loop_end - storyboard->loop_start;
            if (period)
            {
                if (storyboard->loop_count == -1 || sample_time < storyboard->loop_start + period * storyboard->loop_count)
                    sample_time = storyboard->loop_start + fmod(sample_time - storyboard->loop_start, period);
                else sample_time -= period * (storyboard->loop_count - 1.0);
            }
        }
        if (elapsed < storyboard->duration && sample_time >= storyboard->first_start)
            storyboard_set_status(storyboard, UI_ANIMATION_STORYBOARD_PLAYING);
        LIST_FOR_EACH_ENTRY(entry, &storyboard->transitions, struct storyboard_transition, entry)
        {
            if (sample_time < entry->start) continue;
            /* Only the last begun transition on each variable supplies this frame. */
            later = FALSE;
            LIST_FOR_EACH_ENTRY(candidate, &storyboard->transitions, struct storyboard_transition, entry)
            {
                if (candidate == entry) { later = TRUE; continue; }
                if (later && candidate->variable == entry->variable && sample_time >= candidate->start) break;
            }
            if (&candidate->entry != &storyboard->transitions) continue;
            value = transition_value(entry, sample_time - entry->start, &velocity);
            variable = entry->variable;
            variable->value = min(max(value, variable->lower), variable->upper);
            variable->velocity = velocity;
            if (variable->value != variable->previous)
            {
                changed = TRUE;
                storyboard_changed = TRUE;
                variable->changed_storyboard = storyboard;
            }
        }
        if (storyboard->handler && (storyboard_changed || storyboard->status == UI_ANIMATION_STORYBOARD_PLAYING))
        {
            IUIAnimationStoryboardEventHandler *handler = storyboard->handler;
            IUIAnimationStoryboardEventHandler_AddRef(handler);
            IUIAnimationStoryboardEventHandler_OnStoryboardUpdated(handler, &storyboard->IUIAnimationStoryboard_iface);
            IUIAnimationStoryboardEventHandler_Release(handler);
        }
        if (elapsed >= storyboard->duration)
        {
            storyboard_set_status(storyboard, UI_ANIMATION_STORYBOARD_FINISHED);
            storyboard_deactivate(storyboard, UI_ANIMATION_STORYBOARD_READY);
        }
next:
        ;
    }
    /* Windows dispatches variable changes after storyboard status/update callbacks. */
    for (i = 0; i < count; ++i)
    {
        storyboard = active[i];
        LIST_FOR_EACH_ENTRY(entry, &storyboard->transitions, struct storyboard_transition, entry)
        {
            variable = entry->variable;
            if (variable->changed_storyboard != storyboard) continue;
            variable->changed_storyboard = NULL;
            variable_notify(variable, storyboard);
        }
        IUIAnimationStoryboard_Release(&storyboard->IUIAnimationStoryboard_iface);
    }
    free(active);
    manager_refresh_status(manager);
    manager->updating = FALSE;
    if (result) *result = changed ? UI_ANIMATION_UPDATE_VARIABLES_CHANGED : UI_ANIMATION_UPDATE_NO_CHANGE;
    IUIAnimationManager_Release(iface);
    return S_OK;
}

static HRESULT WINAPI manager_GetVariableFromTag(IUIAnimationManager *iface, IUnknown *object, UINT32 id,
        IUIAnimationVariable **variable)
{
    FIXME("iface %p, object %p, id %u: not implemented.\n", iface, object, id);
    if (variable) *variable = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI manager_GetStoryboardFromTag(IUIAnimationManager *iface, IUnknown *object, UINT32 id,
        IUIAnimationStoryboard **storyboard)
{
    FIXME("iface %p, object %p, id %u: not implemented.\n", iface, object, id);
    if (storyboard) *storyboard = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI manager_GetStatus(IUIAnimationManager *iface, UI_ANIMATION_MANAGER_STATUS *status)
{
    struct animation_manager *manager = manager_from_iface(iface);
    if (!status) return E_POINTER;
    *status = UI_ANIMATION_MANAGER_IDLE;
    if (manager->shutdown) return UI_E_SHUTDOWN_CALLED;
    *status = manager->status;
    return S_OK;
}

static HRESULT WINAPI manager_SetAnimationMode(IUIAnimationManager *iface, UI_ANIMATION_MODE mode)
{
    struct animation_manager *manager = manager_from_iface(iface);
    if (manager->shutdown) return UI_E_SHUTDOWN_CALLED;
    if (mode > UI_ANIMATION_MODE_ENABLED) return E_INVALIDARG;
    manager->mode = mode;
    return S_OK;
}

static HRESULT WINAPI manager_Pause(IUIAnimationManager *iface)
{
    struct animation_manager *manager = manager_from_iface(iface);
    if (manager->shutdown) return UI_E_SHUTDOWN_CALLED;
    if (manager->paused) return S_FALSE;
    manager->paused = TRUE;
    return S_OK;
}

static HRESULT WINAPI manager_Resume(IUIAnimationManager *iface)
{
    struct animation_manager *manager = manager_from_iface(iface);
    if (manager->shutdown) return UI_E_SHUTDOWN_CALLED;
    if (!manager->paused) return S_FALSE;
    manager->paused = FALSE;
    return S_OK;
}

static HRESULT WINAPI manager_SetCancelPriorityComparison(IUIAnimationManager *iface, IUIAnimationPriorityComparison *comparison)
{
    FIXME("iface %p, comparison %p: not implemented.\n", iface, comparison);
    return E_NOTIMPL;
}

static HRESULT WINAPI manager_SetTrimPriorityComparison(IUIAnimationManager *iface, IUIAnimationPriorityComparison *comparison)
{
    FIXME("iface %p, comparison %p: not implemented.\n", iface, comparison);
    return E_NOTIMPL;
}

static HRESULT WINAPI manager_SetCompressPriorityComparison(IUIAnimationManager *iface, IUIAnimationPriorityComparison *comparison)
{
    FIXME("iface %p, comparison %p: not implemented.\n", iface, comparison);
    return E_NOTIMPL;
}

static HRESULT WINAPI manager_SetConcludePriorityComparison(IUIAnimationManager *iface, IUIAnimationPriorityComparison *comparison)
{
    FIXME("iface %p, comparison %p: not implemented.\n", iface, comparison);
    return E_NOTIMPL;
}

static HRESULT WINAPI manager_SetDefaultLongestAcceptableDelay(IUIAnimationManager *iface, double delay)
{
    struct animation_manager *manager = manager_from_iface(iface);
    if (manager->shutdown) return UI_E_SHUTDOWN_CALLED;
    if (!isfinite(delay) || (delay < 0 && delay != UI_ANIMATION_SECONDS_EVENTUALLY)) return E_INVALIDARG;
    manager->delay = delay;
    return S_OK;
}

static HRESULT WINAPI manager_Shutdown(IUIAnimationManager *iface)
{
    struct animation_manager *manager = manager_from_iface(iface);
    if (manager->shutdown) return UI_E_SHUTDOWN_CALLED;
    if (manager->updating) return UI_E_ILLEGAL_REENTRANCY;
    IUIAnimationManager_AddRef(iface);
    manager->shutdown = TRUE;
    if (manager->status == UI_ANIMATION_MANAGER_BUSY)
    {
        manager->status = UI_ANIMATION_MANAGER_IDLE;
        if (manager->handler)
            IUIAnimationManagerEventHandler_OnManagerStatusChanged(manager->handler,
                    UI_ANIMATION_MANAGER_IDLE, UI_ANIMATION_MANAGER_BUSY);
    }
    manager_detach_objects(manager);
    if (manager->handler)
    {
        IUIAnimationManagerEventHandler *handler = manager->handler;
        manager->handler = NULL;
        IUIAnimationManagerEventHandler_Release(handler);
    }
    IUIAnimationManager_Release(iface);
    return S_OK;
}

static const IUIAnimationManagerVtbl manager_vtbl =
{
    manager_QueryInterface, manager_AddRef, manager_Release, manager_CreateAnimationVariable,
    manager_ScheduleTransition, manager_CreateStoryboard, manager_FinishAllStoryboards,
    manager_AbandonAllStoryboards, manager_Update, manager_GetVariableFromTag, manager_GetStoryboardFromTag,
    manager_GetStatus, manager_SetAnimationMode, manager_Pause, manager_Resume, manager_SetManagerEventHandler,
    manager_SetCancelPriorityComparison, manager_SetTrimPriorityComparison, manager_SetCompressPriorityComparison,
    manager_SetConcludePriorityComparison, manager_SetDefaultLongestAcceptableDelay, manager_Shutdown,
};

HRESULT animation_manager_create(IUnknown *outer, REFIID iid, void **out)
{
    struct animation_manager *manager;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    if (outer) return CLASS_E_NOAGGREGATION;
    if (!(manager = calloc(1, sizeof(*manager)))) return E_OUTOFMEMORY;
    manager->IUIAnimationManager_iface.lpVtbl = &manager_vtbl;
    manager->ref = 1;
    manager->mode = UI_ANIMATION_MODE_SYSTEM_DEFAULT;
    manager->delay = UI_ANIMATION_SECONDS_EVENTUALLY;
    list_init(&manager->variables);
    list_init(&manager->storyboards);
    hr = IUIAnimationManager_QueryInterface(&manager->IUIAnimationManager_iface, iid, out);
    IUIAnimationManager_Release(&manager->IUIAnimationManager_iface);
    return hr;
}
