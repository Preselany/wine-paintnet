/* Public-API UIAnimation timing and status measurements.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <stdio.h>
#include "windows.h"
#include "initguid.h"
#include "uianimation.h"

static void call(const char *name, HRESULT hr)
{
    printf("{\"call\":\"%s\",\"hr\":%lu}\n",name,(unsigned long)hr);
}

static void snapshot(IUIAnimationManager *m,IUIAnimationVariable *v,IUIAnimationStoryboard *s,
        IUIAnimationTransition *tr,double time)
{
    double value=-123,previous=-123,final=-123,elapsed=-123,duration=-123;
    INT integer=-123;
    UI_ANIMATION_STORYBOARD_STATUS status=99;
    UI_ANIMATION_MANAGER_STATUS busy=99;
    IUIAnimationStoryboard *current=NULL;
    HRESULT vh,ph,fh,eh,dh,kh,sh,mh,ih,ch;
    vh=IUIAnimationVariable_GetValue(v,&value);
    ph=IUIAnimationVariable_GetPreviousValue(v,&previous);
    fh=IUIAnimationVariable_GetFinalValue(v,&final);
    ih=IUIAnimationVariable_GetIntegerValue(v,&integer);
    eh=IUIAnimationStoryboard_GetElapsedTime(s,&elapsed);
    dh=IUIAnimationTransition_GetDuration(tr,&duration);
    kh=IUIAnimationTransition_IsDurationKnown(tr);
    sh=IUIAnimationStoryboard_GetStatus(s,&status);
    mh=IUIAnimationManager_GetStatus(m,&busy);
    ch=IUIAnimationVariable_GetCurrentStoryboard(v,&current);
    printf("{\"time\":%.17g,\"value\":%.17g,\"previous\":%.17g,\"final\":%.17g,\"integer\":%d,\"elapsed\":%.17g,\"duration\":%.17g,\"known_hr\":%lu,\"status\":%u,\"busy\":%u,\"current\":%d,\"hr\":[%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu]}\n",
            time,value,previous,final,integer,elapsed,duration,(unsigned long)kh,status,busy,current==s,
            (unsigned long)vh,(unsigned long)ph,(unsigned long)fh,(unsigned long)ih,(unsigned long)eh,
            (unsigned long)dh,(unsigned long)sh,(unsigned long)mh,(unsigned long)ch);
    if(current) IUIAnimationStoryboard_Release(current);
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
    printf("{\"event\":\"value\",\"value\":%.17g,\"previous\":%.17g}\n",value,previous);
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
    printf("{\"event\":\"storyboard\",\"status\":%u,\"previous\":%u}\n",status,previous);
    return S_OK;
}
static HRESULT WINAPI storyboard_updated(IUIAnimationStoryboardEventHandler *iface, IUIAnimationStoryboard *storyboard)
{
    (void)iface; (void)storyboard;
    printf("{\"event\":\"updated\"}\n");
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
    printf("{\"event\":\"manager\",\"status\":%u,\"previous\":%u}\n",status,previous);
    return S_OK;
}
static const IUIAnimationManagerEventHandlerVtbl manager_vtbl =
{ manager_qi, manager_addref, manager_release, manager_changed };
static IUIAnimationManagerEventHandler manager_handler = { &manager_vtbl };

static void run(IUIAnimationTransitionLibrary *lib,unsigned int kind,double initial,double final,double velocity)
{
    IUIAnimationManager *m=NULL;
    IUIAnimationVariable *v=NULL;
    IUIAnimationStoryboard *s=NULL;
    IUIAnimationTransition *tr=NULL;
    UI_ANIMATION_SCHEDULING_RESULT result=99;
    UI_ANIMATION_UPDATE_RESULT changed;
    unsigned int i;
    HRESULT hr;
    printf("{\"case\":%u,\"initial\":%g,\"final\":%g,\"velocity\":%g}\n",kind,initial,final,velocity);
    hr=CoCreateInstance(&CLSID_UIAnimationManager,NULL,CLSCTX_INPROC_SERVER,&IID_IUIAnimationManager,(void **)&m);
    if(FAILED(hr)){call("manager",hr);goto done;}
    call("enable",IUIAnimationManager_SetAnimationMode(m,UI_ANIMATION_MODE_ENABLED));
    hr=IUIAnimationManager_CreateAnimationVariable(m,initial,&v);
    if(FAILED(hr)){call("variable",hr);goto done;}
    hr=IUIAnimationManager_CreateStoryboard(m,&s);
    if(FAILED(hr)){call("storyboard",hr);goto done;}
    if(kind==0) hr=IUIAnimationTransitionLibrary_CreateSmoothStopTransition(lib,1,final,&tr);
    else if(kind==1) hr=IUIAnimationTransitionLibrary_CreateLinearTransition(lib,1,final,&tr);
    else hr=IUIAnimationTransitionLibrary_CreateInstantaneousTransition(lib,final,&tr);
    call("transition",hr);
    if(FAILED(hr)) goto done;
    call("manager_handler",IUIAnimationManager_SetManagerEventHandler(m,&manager_handler));
    call("variable_handler",IUIAnimationVariable_SetVariableChangeHandler(v,&variable_handler));
    call("storyboard_handler",IUIAnimationStoryboard_SetStoryboardEventHandler(s,&storyboard_handler));
    call("velocity",IUIAnimationTransition_SetInitialVelocity(tr,velocity));
    snapshot(m,v,s,tr,-2);
    call("add",IUIAnimationStoryboard_AddTransition(s,v,tr));
    snapshot(m,v,s,tr,-1);
    call("update_before",IUIAnimationManager_Update(m,10,NULL));
    call("schedule",IUIAnimationStoryboard_Schedule(s,10,&result));
    printf("{\"scheduling_result\":%u}\n",result);
    snapshot(m,v,s,tr,-.5);
    for(i=0;i<=12;++i)
    {
        changed=99;
        hr=IUIAnimationManager_Update(m,10+i*.1,&changed);
        printf("{\"update_hr\":%lu,\"changed\":%u}\n",(unsigned long)hr,changed);
        snapshot(m,v,s,tr,i*.1);
    }
    call("update_repeat",IUIAnimationManager_Update(m,11.2,&changed));
    printf("{\"repeat_changed\":%u}\n",changed);
    call("reschedule",IUIAnimationStoryboard_Schedule(s,12,&result));
    printf("{\"rescheduling_result\":%u}\n",result);
    call("shutdown",IUIAnimationManager_Shutdown(m));
    snapshot(m,v,s,tr,99);
done:
    if(tr) IUIAnimationTransition_Release(tr);
    if(s) IUIAnimationStoryboard_Release(s);
    if(v) IUIAnimationVariable_Release(v);
    if(m) IUIAnimationManager_Release(m);
}

int main(void)
{
    IUIAnimationTransitionLibrary *lib=NULL;
    HRESULT hr;
    unsigned int i;
    static const double velocities[]={-4,0,1,2,4,10};
    CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
    hr=CoCreateInstance(&CLSID_UIAnimationTransitionLibrary,NULL,CLSCTX_INPROC_SERVER,&IID_IUIAnimationTransitionLibrary,(void **)&lib);
    if(FAILED(hr)){call("library",hr);return 1;}
    for(i=0;i<sizeof(velocities)/sizeof(*velocities);++i) run(lib,0,0,1,velocities[i]);
    run(lib,0,1,0,0);
    run(lib,0,1,1,0);
    run(lib,0,1,1,2);
    run(lib,1,-.5,2.5,0);
    run(lib,2,0,1,0);
    {
        IUIAnimationTimer *timer=NULL;
        double time=-1;
        LARGE_INTEGER counter,frequency;
        hr=CoCreateInstance(&CLSID_UIAnimationTimer,NULL,CLSCTX_INPROC_SERVER,&IID_IUIAnimationTimer,(void **)&timer);
        call("timer",hr);
        if(SUCCEEDED(hr))
        {
            call("timer_initial",IUIAnimationTimer_IsEnabled(timer));
            call("time_before",IUIAnimationTimer_GetTime(timer,&time));
            QueryPerformanceCounter(&counter); QueryPerformanceFrequency(&frequency);
            printf("{\"timer_time\":%.17g,\"qpc\":%.17g}\n",time,(double)counter.QuadPart/frequency.QuadPart);
            call("timer_enable",IUIAnimationTimer_Enable(timer));
            call("timer_enable_again",IUIAnimationTimer_Enable(timer));
            Sleep(20);
            call("time_after",IUIAnimationTimer_GetTime(timer,&time));
            printf("{\"timer_time\":%.17g}\n",time);
            call("timer_disable",IUIAnimationTimer_Disable(timer));
            call("timer_disable_again",IUIAnimationTimer_Disable(timer));
            IUIAnimationTimer_Release(timer);
        }
    }
    IUIAnimationTransitionLibrary_Release(lib);
    CoUninitialize();
    return 0;
}
