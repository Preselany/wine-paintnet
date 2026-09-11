/* Windows-derived UIAnimation timeline fixtures.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <math.h>
#include "windows.h"
#include "uianimation.h"
#include "wine/test.h"

static char events[64];
static unsigned int event_count;
static void record_event(char event)
{
    ok(event_count + 1 < sizeof(events), "Too many callback events.\n");
    if (event_count + 1 < sizeof(events)) events[event_count++] = event;
    events[event_count] = 0;
}
static void check_events(const char *expected)
{
    ok(!strcmp(events, expected), "Events '%s', expected '%s'.\n", events, expected);
    event_count = 0; events[0] = 0;
}

static HRESULT WINAPI variable_qi(IUIAnimationVariableChangeHandler *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out=NULL;
    if (!IsEqualIID(iid,&IID_IUnknown) && !IsEqualIID(iid,&IID_IUIAnimationVariableChangeHandler)) return E_NOINTERFACE;
    IUIAnimationVariableChangeHandler_AddRef(iface); *out=iface; return S_OK;
}
static ULONG WINAPI variable_addref(IUIAnimationVariableChangeHandler *iface) { (void)iface; return 2; }
static ULONG WINAPI variable_release(IUIAnimationVariableChangeHandler *iface) { (void)iface; return 1; }
static HRESULT WINAPI variable_changed(IUIAnimationVariableChangeHandler *iface, IUIAnimationStoryboard *storyboard,
        IUIAnimationVariable *variable, double value, double previous)
{
    (void)iface; (void)storyboard; (void)variable;
    (void)value; (void)previous; record_event('v');
    return S_OK;
}
static const IUIAnimationVariableChangeHandlerVtbl variable_vtbl =
{ variable_qi, variable_addref, variable_release, variable_changed };
static IUIAnimationVariableChangeHandler variable_handler = { &variable_vtbl };

static HRESULT WINAPI storyboard_qi(IUIAnimationStoryboardEventHandler *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out=NULL;
    if (!IsEqualIID(iid,&IID_IUnknown) && !IsEqualIID(iid,&IID_IUIAnimationStoryboardEventHandler)) return E_NOINTERFACE;
    IUIAnimationStoryboardEventHandler_AddRef(iface); *out=iface; return S_OK;
}
static ULONG WINAPI storyboard_addref(IUIAnimationStoryboardEventHandler *iface) { (void)iface; return 2; }
static ULONG WINAPI storyboard_release(IUIAnimationStoryboardEventHandler *iface) { (void)iface; return 1; }
static HRESULT WINAPI storyboard_changed(IUIAnimationStoryboardEventHandler *iface, IUIAnimationStoryboard *storyboard,
        UI_ANIMATION_STORYBOARD_STATUS status, UI_ANIMATION_STORYBOARD_STATUS previous)
{
    (void)iface; (void)storyboard;
    (void)previous; record_event(status == 1 ? 's' : status == 3 ? 'p' : status == 5 ? 'f' : status == 6 ? 'r' : '?');
    return S_OK;
}
static HRESULT WINAPI storyboard_updated(IUIAnimationStoryboardEventHandler *iface, IUIAnimationStoryboard *storyboard)
{
    (void)iface; (void)storyboard;
    record_event('u');
    return S_OK;
}
static const IUIAnimationStoryboardEventHandlerVtbl storyboard_vtbl =
{ storyboard_qi, storyboard_addref, storyboard_release, storyboard_changed, storyboard_updated };
static IUIAnimationStoryboardEventHandler storyboard_handler = { &storyboard_vtbl };

static HRESULT WINAPI manager_qi(IUIAnimationManagerEventHandler *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out=NULL;
    if (!IsEqualIID(iid,&IID_IUnknown) && !IsEqualIID(iid,&IID_IUIAnimationManagerEventHandler)) return E_NOINTERFACE;
    IUIAnimationManagerEventHandler_AddRef(iface); *out=iface; return S_OK;
}
static ULONG WINAPI manager_addref(IUIAnimationManagerEventHandler *iface) { (void)iface; return 2; }
static ULONG WINAPI manager_release(IUIAnimationManagerEventHandler *iface) { (void)iface; return 1; }
static HRESULT WINAPI manager_changed(IUIAnimationManagerEventHandler *iface,
        UI_ANIMATION_MANAGER_STATUS status, UI_ANIMATION_MANAGER_STATUS previous)
{
    (void)iface;
    (void)previous; record_event(status ? 'b' : 'i');
    return S_OK;
}
static const IUIAnimationManagerEventHandlerVtbl manager_vtbl =
{ manager_qi, manager_addref, manager_release, manager_changed };
static IUIAnimationManagerEventHandler manager_handler = { &manager_vtbl };

static const struct timeline_case
{
    unsigned int kind;
    double initial, final, velocity;
    double values[13];
    unsigned int states[13];
    const char *events[13];
} cases[] =
{
    {0,0,1,-4,
        {0,-0.29599999999999926,-0.40799999999999975,-0.37199999999999933,-0.22399999999999931,0,0.26399999999999935,0.53199999999999825,0.76800000000000157,0.93600000000000083,1,1,1},
        {1,3,3,3,3,3,3,3,3,3,6,6,6},
        {"","puv","uv","uv","uv","uv","uv","uv","uv","uv","ufrvi","",""}},
    {0,0,1,0,
        {0,0.02799999999999981,0.10399999999999932,0.21600000000000091,0.35200000000000053,0.5,0.64799999999999947,0.78399999999999914,0.8960000000000008,0.9720000000000002,1,1,1},
        {1,3,3,3,3,3,3,3,3,3,6,6,6},
        {"","puv","uv","uv","uv","uv","uv","uv","uv","uv","ufrvi","",""}},
    {0,0,1,1,
        {0,0.10899999999999958,0.2319999999999991,0.36300000000000093,0.49600000000000044,0.625,0.74399999999999955,0.84699999999999931,0.92800000000000049,0.98100000000000009,1,1,1},
        {1,3,3,3,3,3,3,3,3,3,6,6,6},
        {"","puv","uv","uv","uv","uv","uv","uv","uv","uv","ufrvi","",""}},
    {0,0,1,2,
        {0,0.18999999999999936,0.35999999999999888,0.51000000000000101,0.64000000000000046,0.75,0.83999999999999964,0.90999999999999959,0.9600000000000003,0.9900000000000001,1,1,1},
        {1,3,3,3,3,3,3,3,3,3,6,6,6},
        {"","puv","uv","uv","uv","uv","uv","uv","uv","uv","ufrvi","",""}},
    {0,0,1,4,
        {0,0.35999999999999888,0.63999999999999835,0.84000000000000119,0.9600000000000003,1,1,1,1,1,1,1,1},
        {1,3,3,3,3,6,6,6,6,6,6,6,6},
        {"","puv","uv","uv","uv","ufrvi","","","","","","",""}},
    {0,0,1,10,
        {0,0.74999999999999822,1,1,1,1,1,1,1,1,1,1,1},
        {1,3,6,6,6,6,6,6,6,6,6,6,6},
        {"","puv","ufrvi","","","","","","","","","",""}},
    {0,1,0,0,
        {1,0.9720000000000002,0.89600000000000068,0.78399999999999914,0.64799999999999947,0.5,0.35200000000000053,0.21600000000000086,0.1039999999999992,0.027999999999999803,0,0,0},
        {1,3,3,3,3,3,3,3,3,3,6,6,6},
        {"","puv","uv","uv","uv","uv","uv","uv","uv","uv","ufrvi","",""}},
    {0,1,1,0,
        {1,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,6,6,6,6,6,6,6,6,6,6,6,6},
        {"","fri","","","","","","","","","","",""}},
    {0,1,1,2,
        {1,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,6,6,6,6,6,6,6,6,6,6,6,6},
        {"","fri","","","","","","","","","","",""}},
    {1,-0.5,2.5,0,
        {-0.5,-0.20000000000000107,0.099999999999997868,0.40000000000000213,0.70000000000000107,1,1.2999999999999989,1.5999999999999979,1.9000000000000021,2.2000000000000011,2.5,2.5,2.5},
        {1,3,3,3,3,3,3,3,3,3,6,6,6},
        {"","puv","uv","uv","uv","uv","uv","uv","uv","uv","ufrvi","",""}},
    {2,0,1,0,
        {0,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,6,6,6,6,6,6,6,6,6,6,6,6},
        {"","ufrvi","","","","","","","","","","",""}},
};

static void check_value(IUIAnimationVariable *variable, double expected, double previous, double final)
{
    double value;
    HRESULT hr;
    hr = IUIAnimationVariable_GetValue(variable, &value);
    ok(hr == S_OK && fabs(value - expected) < 1e-12, "Value %.17g, expected %.17g, hr %#lx.\n", value,expected,hr);
    hr = IUIAnimationVariable_GetPreviousValue(variable, &value);
    ok(hr == S_OK && fabs(value - previous) < 1e-12, "Previous %.17g, expected %.17g, hr %#lx.\n",value,previous,hr);
    hr = IUIAnimationVariable_GetFinalValue(variable, &value);
    ok(hr == S_OK && fabs(value - final) < 1e-12, "Final %.17g, expected %.17g, hr %#lx.\n",value,final,hr);
}

static void test_timeline(IUIAnimationTransitionLibrary *library, const struct timeline_case *test)
{
    IUIAnimationManager *manager = NULL;
    IUIAnimationStoryboard *storyboard = NULL, *current = NULL;
    IUIAnimationVariable *variable = NULL;
    IUIAnimationTransition *transition = NULL;
    UI_ANIMATION_SCHEDULING_RESULT scheduling;
    UI_ANIMATION_UPDATE_RESULT changed;
    UI_ANIMATION_STORYBOARD_STATUS status;
    UI_ANIMATION_MANAGER_STATUS manager_status;
    double previous = test->initial, value, duration, elapsed;
    unsigned int i;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_UIAnimationManager,NULL,CLSCTX_INPROC_SERVER,&IID_IUIAnimationManager,(void **)&manager);
    ok(hr == S_OK, "Manager returned %#lx.\n",hr);
    if (FAILED(hr)) goto done;
    hr = IUIAnimationManager_SetAnimationMode(manager, UI_ANIMATION_MODE_ENABLED);
    ok(hr == S_OK, "Enabling animation returned %#lx.\n",hr);
    hr = IUIAnimationManager_CreateAnimationVariable(manager,test->initial,&variable);
    ok(hr == S_OK, "Variable returned %#lx.\n",hr);
    if (FAILED(hr)) goto done;
    hr = IUIAnimationManager_CreateStoryboard(manager,&storyboard);
    ok(hr == S_OK, "Storyboard returned %#lx.\n",hr);
    if (FAILED(hr)) goto done;
    if (!test->kind) hr = IUIAnimationTransitionLibrary_CreateSmoothStopTransition(library,1,test->final,&transition);
    else if (test->kind == 1) hr = IUIAnimationTransitionLibrary_CreateLinearTransition(library,1,test->final,&transition);
    else hr = IUIAnimationTransitionLibrary_CreateInstantaneousTransition(library,test->final,&transition);
    ok(hr == S_OK, "Transition returned %#lx.\n",hr);
    if (FAILED(hr)) goto done;
    hr = IUIAnimationManager_SetManagerEventHandler(manager,&manager_handler);
    ok(hr == S_OK, "Manager handler returned %#lx.\n",hr);
    hr = IUIAnimationVariable_SetVariableChangeHandler(variable,&variable_handler);
    ok(hr == S_OK, "Variable handler returned %#lx.\n",hr);
    hr = IUIAnimationStoryboard_SetStoryboardEventHandler(storyboard,&storyboard_handler);
    ok(hr == S_OK, "Storyboard handler returned %#lx.\n",hr);
    event_count = 0; events[0] = 0;
    hr = IUIAnimationTransition_SetInitialVelocity(transition,test->velocity);
    ok(hr == S_OK, "Initial velocity returned %#lx.\n",hr);
    check_value(variable,test->initial,test->initial,test->initial);
    hr = IUIAnimationTransition_IsDurationKnown(transition);
    ok(hr == (test->kind ? S_OK : S_FALSE), "Duration-known returned %#lx.\n",hr);
    hr = IUIAnimationTransition_GetDuration(transition,&duration);
    ok(hr == (test->kind ? S_OK : UI_E_VALUE_NOT_DETERMINED), "Duration returned %#lx.\n",hr);
    hr = IUIAnimationStoryboard_AddTransition(storyboard,variable,transition);
    ok(hr == S_OK, "Add transition returned %#lx.\n",hr);
    hr = IUIAnimationManager_Update(manager,10,NULL);
    ok(hr == S_OK, "Initial update returned %#lx.\n",hr);
    hr = IUIAnimationStoryboard_Schedule(storyboard,10,&scheduling);
    ok(hr == S_OK && scheduling == UI_ANIMATION_SCHEDULING_SUCCEEDED, "Schedule %#lx, result %u.\n",hr,scheduling);
    check_events("sb");
    check_value(variable,test->initial,test->initial,test->final);
    for (i = 0; i < ARRAY_SIZE(test->values); ++i)
    {
        winetest_push_context("sample %u",i);
        hr = IUIAnimationManager_Update(manager,10+i*.1,&changed);
        ok(hr == S_OK, "Update returned %#lx.\n",hr);
        check_events(test->events[i]);
        ok(changed == (test->values[i] != previous ? UI_ANIMATION_UPDATE_VARIABLES_CHANGED : UI_ANIMATION_UPDATE_NO_CHANGE),
                "Unexpected update result %u.\n",changed);
        check_value(variable,test->values[i],previous,test->final);
        previous = test->values[i];
        hr = IUIAnimationStoryboard_GetStatus(storyboard,&status);
        ok(hr == S_OK && status == test->states[i], "State %u, expected %u, hr %#lx.\n",status,test->states[i],hr);
        hr = IUIAnimationManager_GetStatus(manager,&manager_status);
        ok(hr == S_OK && manager_status == (status == UI_ANIMATION_STORYBOARD_READY ? UI_ANIMATION_MANAGER_IDLE : UI_ANIMATION_MANAGER_BUSY),
                "Manager status %u, hr %#lx.\n",manager_status,hr);
        hr = IUIAnimationVariable_GetCurrentStoryboard(variable,&current);
        ok(hr == S_OK && current == (status == UI_ANIMATION_STORYBOARD_READY ? NULL : storyboard),
                "Current storyboard %p, hr %#lx.\n",current,hr);
        if (current) IUIAnimationStoryboard_Release(current);
        current = NULL;
        hr = IUIAnimationStoryboard_GetElapsedTime(storyboard,&elapsed);
        if (status == UI_ANIMATION_STORYBOARD_PLAYING)
            ok(hr == S_OK && fabs(elapsed-i*.1) < 1e-12, "Elapsed %g, hr %#lx.\n",elapsed,hr);
        else ok(hr == UI_E_STORYBOARD_NOT_PLAYING && elapsed == 0, "Non-playing elapsed %g, hr %#lx.\n",elapsed,hr);
        hr = IUIAnimationTransition_GetDuration(transition,&duration);
        if (status != UI_ANIMATION_STORYBOARD_READY)
            ok(hr == UI_E_STORYBOARD_ACTIVE && duration == 0, "Active duration %g, hr %#lx.\n",duration,hr);
        else ok(hr == (test->kind ? S_OK : UI_E_VALUE_NOT_DETERMINED), "Ready duration returned %#lx.\n",hr);
        winetest_pop_context();
    }
    hr = IUIAnimationStoryboard_Schedule(storyboard,12,&scheduling);
    ok(hr == S_OK && scheduling == UI_ANIMATION_SCHEDULING_SUCCEEDED,"Reschedule %#lx, result %u.\n",hr,scheduling);
    check_events("sb");
    hr = IUIAnimationManager_Shutdown(manager);
    ok(hr == S_OK,"Shutdown returned %#lx.\n",hr);
    check_events("ir");
    value = 123;
    hr = IUIAnimationVariable_GetValue(variable,&value);
    ok(hr == UI_E_SHUTDOWN_CALLED && value == 0, "After shutdown value %g, hr %#lx.\n",value,hr);
    hr = IUIAnimationTransition_IsDurationKnown(transition);
    ok(hr == UI_E_SHUTDOWN_CALLED,"After shutdown duration returned %#lx.\n",hr);
done:
    if (transition) IUIAnimationTransition_Release(transition);
    if (storyboard) IUIAnimationStoryboard_Release(storyboard);
    if (variable) IUIAnimationVariable_Release(variable);
    if (manager) IUIAnimationManager_Release(manager);
}

START_TEST(timeline)
{
    IUIAnimationTransitionLibrary *library;
    unsigned int i;
    HRESULT hr;
    CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
    hr = CoCreateInstance(&CLSID_UIAnimationTransitionLibrary,NULL,CLSCTX_INPROC_SERVER,
            &IID_IUIAnimationTransitionLibrary,(void **)&library);
    ok(hr == S_OK,"Transition library returned %#lx.\n",hr);
    if (SUCCEEDED(hr))
    {
        for (i = 0; i < ARRAY_SIZE(cases); ++i)
        {
            winetest_push_context("timeline %u",i);
            test_timeline(library,&cases[i]);
            winetest_pop_context();
        }
        IUIAnimationTransitionLibrary_Release(library);
    }
    CoUninitialize();
}
