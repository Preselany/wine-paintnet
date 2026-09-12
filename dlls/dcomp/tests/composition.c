/* Native composition controller lifetime and commit reference.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include "initguid.h"
#include "wine/test.h"

#include "roapi.h"
#include "winstring.h"
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_System
#define WIDL_using_Windows_UI_Composition
#define WIDL_using_Windows_UI_Composition_Core
#include "windows.ui.composition.core.h"
#include "windows.system.h"
#include "dispatcherqueue.h"
#define REPORT(name,call) do { hr=(call); ok(hr == (strncmp(name,"closed",6) ? S_OK : RO_E_CLOSED), \
        "%s returned %#lx.\n",name,hr); } while(0)

struct handler {ITypedEventHandler_CompositorController_IInspectable iface; LONG ref; UINT calls;};
static HRESULT WINAPI handler_qi(ITypedEventHandler_CompositorController_IInspectable *iface,REFIID iid,void **out)
{
    if(IsEqualGUID(iid,&IID_IUnknown)||IsEqualGUID(iid,&IID_IAgileObject)
            ||IsEqualGUID(iid,&IID_ITypedEventHandler_CompositorController_IInspectable))
    { *out=iface;IUnknown_AddRef((IUnknown *)iface);return S_OK; }
    *out=NULL;return E_NOINTERFACE;
}
static ULONG WINAPI handler_addref(ITypedEventHandler_CompositorController_IInspectable *iface)
{return InterlockedIncrement(&((struct handler *)iface)->ref);}
static ULONG WINAPI handler_release(ITypedEventHandler_CompositorController_IInspectable *iface)
{return InterlockedDecrement(&((struct handler *)iface)->ref);}
static HRESULT WINAPI handler_invoke(ITypedEventHandler_CompositorController_IInspectable *iface,ICompositorController *sender,IInspectable *args)
{struct handler *h=(struct handler *)iface;++h->calls;ok(sender != NULL && args == NULL,"Unexpected event arguments.\n");return S_OK;}
static const ITypedEventHandler_CompositorController_IInspectableVtbl handler_vtbl={handler_qi,handler_addref,handler_release,handler_invoke};
static void pump(void)
{
    MSG msg;DWORD start=GetTickCount();
    while(GetTickCount()-start<120)
    {
        while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
        Sleep(2);
    }
}
static void inspect(const WCHAR *name,IInspectable *object)
{
    HSTRING str=NULL;IInspectable *agile=NULL,*item=NULL;ULONG count=0,i;IID *iids=NULL;TrustLevel level=99;HRESULT hr;
    hr=IInspectable_GetRuntimeClassName(object,&str);
    ok(hr == (name ? S_OK : E_ILLEGAL_METHOD_CALL),"Class name returned %#lx.\n",hr);
    if(name)ok(!wcscmp(name,WindowsGetStringRawBuffer(str,NULL)),"Unexpected runtime class name.\n");
    WindowsDeleteString(str);
    REPORT("trust",IInspectable_GetTrustLevel(object,&level));ok(level==BaseTrust,"Trust level %u.\n",level);
    REPORT("iids",IInspectable_GetIids(object,&count,&iids));ok(count>0,"No interfaces reported.\n");
    for(i=0;i<count;++i)
    {
        hr=IInspectable_QueryInterface(object,&iids[i],(void **)&item);
        ok(hr==S_OK,"Advertised interface %u unavailable: %#lx.\n",i,hr);
        if(item)IInspectable_Release(item);
    }
    CoTaskMemFree(iids);
    REPORT("agile",IInspectable_QueryInterface(object,&IID_IAgileObject,(void **)&agile));if(agile)IInspectable_Release(agile);
}
START_TEST(composition)
{
    IDispatcherQueueController *queue=NULL;
    DispatcherQueueOptions options={sizeof(options),DQTYPE_THREAD_CURRENT,DQTAT_COM_STA};
    HRESULT (WINAPI *create_queue)(DispatcherQueueOptions,IDispatcherQueueController **);
    IActivationFactory *factory=NULL;IInspectable *instance=NULL;
    ICompositorController *controller=NULL;ICompositor *compositor=NULL,*second=NULL;
    ICompositionColorBrush *brush=NULL;IClosable *closable=NULL,*compositor_closable=NULL;
    IAsyncAction *action=NULL;IAsyncInfo *info=NULL;AsyncStatus status=99;
    struct handler h={{&handler_vtbl},1,0};EventRegistrationToken token={0};
    HSTRING name;HRESULT hr;
    REPORT("init",RoInitialize(RO_INIT_SINGLETHREADED));
    WindowsCreateString(L"Windows.UI.Composition.Core.CompositorController",48,&name);
    REPORT("factory",RoGetActivationFactory(name,&IID_IActivationFactory,(void **)&factory));
    WindowsDeleteString(name);if(FAILED(hr))goto done;
    inspect(NULL,(IInspectable *)factory);
    hr=IActivationFactory_ActivateInstance(factory,&instance);
    ok(hr==E_ACCESSDENIED,"Activation without a dispatcher returned %#lx.\n",hr);
    if(instance){IInspectable_Release(instance);instance=NULL;}
    {
        HMODULE module=LoadLibraryW(L"coremessaging.dll");
        create_queue=(void *)GetProcAddress(module,"CreateDispatcherQueueController");
        ok(create_queue != NULL,"Missing dispatcher API.\n");
        if(!create_queue)goto done;
        REPORT("queue",create_queue(options,&queue));
    }
    REPORT("activate",IActivationFactory_ActivateInstance(factory,&instance));if(FAILED(hr))goto done;
    REPORT("controller",IInspectable_QueryInterface(instance,&IID_ICompositorController,(void **)&controller));if(FAILED(hr))goto done;
    inspect(L"Windows.UI.Composition.Core.CompositorController",(IInspectable *)controller);
    REPORT("get compositor",ICompositorController_get_Compositor(controller,&compositor));if(FAILED(hr))goto done;
    inspect(L"Windows.UI.Composition.Compositor",(IInspectable *)compositor);
    REPORT("get compositor again",ICompositorController_get_Compositor(controller,&second));
    ok(second==compositor,"Compositor identity changed.\n");if(second)ICompositor_Release(second);second=NULL;
    REPORT("add event",ICompositorController_add_CommitNeeded(controller,&h.iface,&token));ok(h.ref==2 && token.value,"Event subscription not retained.\n");
    REPORT("empty commit",ICompositorController_Commit(controller));pump();ok(!h.calls,"Empty commit raised %u events.\n",h.calls);
    REPORT("create brush",ICompositor_CreateColorBrush(compositor,&brush));
    pump();ok(h.calls==1,"Creation raised %u events.\n",h.calls);
    if(brush)
    {
        struct __x_ABI_CWindows_CUI_CColor color={255,32,64,128};
        struct __x_ABI_CWindows_CUI_CColor actual;
        REPORT("set color",ICompositionColorBrush_put_Color(brush,color));REPORT("get color",ICompositionColorBrush_get_Color(brush,&actual));
        ok(!memcmp(&actual,&color,sizeof(color)),"Color did not round trip.\n");
        pump();ok(h.calls==1,"Changes should be coalesced until commit: %u events.\n",h.calls);
    }
    REPORT("commit",ICompositorController_Commit(controller));
    REPORT("ensure",ICompositorController_EnsurePreviousCommitCompletedAsync(controller,&action));
    if(action)
    {
        REPORT("async info",IAsyncAction_QueryInterface(action,&IID_IAsyncInfo,(void **)&info));
        if(info){REPORT("async status",IAsyncInfo_get_Status(info,&status));ok(status==Started || status==Completed,"Unexpected async status %u.\n",status);pump();REPORT("async status later",IAsyncInfo_get_Status(info,&status));ok(status==Completed,"Async operation did not complete: %u.\n",status);IAsyncInfo_Release(info);}
        REPORT("async results",IAsyncAction_GetResults(action));IAsyncAction_Release(action);
    }
    REPORT("controller closable",ICompositorController_QueryInterface(controller,&IID_IClosable,(void **)&closable));
    REPORT("compositor closable",ICompositor_QueryInterface(compositor,&IID_IClosable,(void **)&compositor_closable));
    if(closable)
    {
        REPORT("close",IClosable_Close(closable));REPORT("close again",IClosable_Close(closable));
        REPORT("closed commit",ICompositorController_Commit(controller));
        REPORT("closed get",ICompositorController_get_Compositor(controller,&second));if(second)ICompositor_Release(second);
        REPORT("closed remove",ICompositorController_remove_CommitNeeded(controller,token));
        REPORT("closed add",ICompositorController_add_CommitNeeded(controller,&h.iface,&token));
        IClosable_Release(closable);
    }
    if(compositor_closable){REPORT("compositor close",IClosable_Close(compositor_closable));IClosable_Release(compositor_closable);}
    if(brush)ICompositionColorBrush_Release(brush);
    ok(h.calls==2 && h.ref==1,"Unexpected final event state: %u calls, %ld refs.\n",h.calls,h.ref);
done:
    if(compositor)ICompositor_Release(compositor);
    if(controller)ICompositorController_Release(controller);
    if(instance)IInspectable_Release(instance);
    if(factory)IActivationFactory_Release(factory);
    if(queue)IDispatcherQueueController_Release(queue);
    ok(h.ref==1,"Leaked handler reference: %ld.\n",h.ref);RoUninitialize();
}
