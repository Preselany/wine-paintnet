/* Native composition controller lifetime and commit reference.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include "initguid.h"
#include <stdio.h>
#include <stdlib.h>
#include "roapi.h"
#include "winstring.h"
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_System
#define WIDL_using_Windows_UI_Composition
#define WIDL_using_Windows_UI_Composition_Core
#include "windows.ui.composition.core.h"
#include "windows.system.h"
#include "dispatcherqueue.h"
#define REPORT(name,call) do {hr=(call);printf(name " %08lx\n",hr);} while(0)

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
{struct handler *h=(struct handler *)iface;++h->calls;printf("event %u %u %u\n",h->calls,!!sender,!!args);return S_OK;}
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
static void inspect(const char *label,IInspectable *object)
{
    HSTRING str=NULL;IInspectable *agile=NULL;ULONG count=0;IID *iids=NULL;TrustLevel level=99;HRESULT hr;
    hr=IInspectable_GetRuntimeClassName(object,&str);printf("%s name %08lx %ls\n",label,hr,WindowsGetStringRawBuffer(str,NULL));WindowsDeleteString(str);
    hr=IInspectable_GetTrustLevel(object,&level);printf("%s trust %08lx %u\n",label,hr,level);
    hr=IInspectable_GetIids(object,&count,&iids);printf("%s iids %08lx %lu\n",label,hr,count);CoTaskMemFree(iids);
    hr=IInspectable_QueryInterface(object,&IID_IAgileObject,(void **)&agile);printf("%s agile %08lx\n",label,hr);if(agile)IInspectable_Release(agile);
}
int main(void)
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
    setvbuf(stdout,NULL,_IONBF,0);
    REPORT("init",RoInitialize(RO_INIT_SINGLETHREADED));
    if(!getenv("COMPOSITOR_NO_QUEUE"))
    {
        HMODULE module=LoadLibraryW(L"coremessaging.dll");
        create_queue=(void *)GetProcAddress(module,"CreateDispatcherQueueController");
        REPORT("queue",create_queue(options,&queue));
    }
    WindowsCreateString(L"Windows.UI.Composition.Core.CompositorController",48,&name);
    REPORT("factory",RoGetActivationFactory(name,&IID_IActivationFactory,(void **)&factory));
    WindowsDeleteString(name);if(FAILED(hr))goto done;
    inspect("factory",(IInspectable *)factory);
    REPORT("activate",IActivationFactory_ActivateInstance(factory,&instance));if(FAILED(hr))goto done;
    REPORT("controller",IInspectable_QueryInterface(instance,&IID_ICompositorController,(void **)&controller));if(FAILED(hr))goto done;
    inspect("controller",(IInspectable *)controller);
    REPORT("get compositor",ICompositorController_get_Compositor(controller,&compositor));if(FAILED(hr))goto done;
    inspect("compositor",(IInspectable *)compositor);
    REPORT("get compositor again",ICompositorController_get_Compositor(controller,&second));
    printf("same compositor %u\n",second==compositor);if(second)ICompositor_Release(second);second=NULL;
    REPORT("add event",ICompositorController_add_CommitNeeded(controller,&h.iface,&token));printf("event refs %ld token %u\n",h.ref,!!token.value);
    REPORT("empty commit",ICompositorController_Commit(controller));pump();printf("empty events %u\n",h.calls);
    REPORT("create brush",ICompositor_CreateColorBrush(compositor,&brush));
    pump();printf("create events %u\n",h.calls);
    if(brush)
    {
        struct __x_ABI_CWindows_CUI_CColor color={255,32,64,128};
        REPORT("set color",ICompositionColorBrush_put_Color(brush,color));pump();printf("color events %u\n",h.calls);
    }
    REPORT("commit",ICompositorController_Commit(controller));
    REPORT("ensure",ICompositorController_EnsurePreviousCommitCompletedAsync(controller,&action));
    if(action)
    {
        REPORT("async info",IAsyncAction_QueryInterface(action,&IID_IAsyncInfo,(void **)&info));
        if(info){REPORT("async status",IAsyncInfo_get_Status(info,&status));printf("status %u\n",status);pump();REPORT("async status later",IAsyncInfo_get_Status(info,&status));printf("status later %u\n",status);IAsyncInfo_Release(info);}
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
    printf("final events %u refs %ld\n",h.calls,h.ref);
done:
    if(compositor)ICompositor_Release(compositor);
    if(controller)ICompositorController_Release(controller);
    if(instance)IInspectable_Release(instance);
    if(factory)IActivationFactory_Release(factory);
    if(queue)IDispatcherQueueController_Release(queue);
    printf("released refs %ld\n",h.ref);RoUninitialize();return 0;
}
