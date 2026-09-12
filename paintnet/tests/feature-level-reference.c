/* Native measurements for effect-context and D3D context-state feature levels.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define main color_context_base_main
#include "color-context-reference.c"
#undef main
#include "d2d1effectauthor.h"
#include "d3d11_1.h"
#include "d2d1effects_2.h"

DEFINE_GUID(CLSID_FeatureProbe,0x93703917,0x56b3,0x4c9c,0x99,0x51,0x38,0x48,0x29,0xfe,0x64,0x21);

struct wrapper
{
    ID2D1EffectImpl iface;
    LONG refs;
    ID2D1EffectContext *context;
    ID2D1TransformGraph *graph;
    unsigned int prepared;
};
static struct wrapper *latest;

static HRESULT STDMETHODCALLTYPE wrapper_qi(ID2D1EffectImpl *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid,&IID_IUnknown) && !IsEqualGUID(iid,&IID_ID2D1EffectImpl)) return E_NOINTERFACE;
    *out = iface;
    ID2D1EffectImpl_AddRef(iface);
    return S_OK;
}
static ULONG STDMETHODCALLTYPE wrapper_addref(ID2D1EffectImpl *iface)
{
    return InterlockedIncrement(&CONTAINING_RECORD(iface,struct wrapper,iface)->refs);
}
static ULONG STDMETHODCALLTYPE wrapper_release(ID2D1EffectImpl *iface)
{
    struct wrapper *w = CONTAINING_RECORD(iface,struct wrapper,iface);
    ULONG refs = InterlockedDecrement(&w->refs);
    if (!refs)
    {
        if (w->graph) ID2D1TransformGraph_Release(w->graph);
        if (w->context) ID2D1EffectContext_Release(w->context);
        free(w);
    }
    return refs;
}
static HRESULT STDMETHODCALLTYPE wrapper_init(ID2D1EffectImpl *iface, ID2D1EffectContext *context,
        ID2D1TransformGraph *graph)
{
    struct wrapper *w = CONTAINING_RECORD(iface,struct wrapper,iface);
    ID2D1EffectContext_AddRef(w->context = context);
    ID2D1TransformGraph_AddRef(w->graph = graph);
    return ID2D1TransformGraph_SetPassthroughGraph(graph,0);
}
static HRESULT STDMETHODCALLTYPE wrapper_prepare(ID2D1EffectImpl *iface, D2D1_CHANGE_TYPE change)
{
    struct wrapper *w = CONTAINING_RECORD(iface,struct wrapper,iface);
    ++w->prepared;
    printf("prepare %u\n",change);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE wrapper_graph(ID2D1EffectImpl *iface, ID2D1TransformGraph *graph)
{
    (void)iface; (void)graph;
    return E_NOTIMPL;
}
static const ID2D1EffectImplVtbl wrapper_vtbl =
{
    wrapper_qi,wrapper_addref,wrapper_release,wrapper_init,wrapper_prepare,wrapper_graph
};
static HRESULT CALLBACK wrapper_factory(IUnknown **out)
{
    struct wrapper *w = calloc(1,sizeof(*w));
    if (!w) return E_OUTOFMEMORY;
    w->iface.lpVtbl = &wrapper_vtbl;
    w->refs = 1;
    latest = w;
    *out = (IUnknown *)&w->iface;
    return S_OK;
}
static const WCHAR wrapper_xml[] =
    L"<?xml version='1.0'?><Effect>"
    L"<Property name='DisplayName' type='string' value='Wrapper probe'/>"
    L"<Property name='Author' type='string' value='Wine'/>"
    L"<Property name='Category' type='string' value='Test'/>"
    L"<Property name='Description' type='string' value='Effect node'/>"
    L"<Inputs><Input name='Source'/></Inputs></Effect>";

static ID3D11Device1 *probe_device;

static void probe(ID2D1EffectContext *context,unsigned int index,const D3D_FEATURE_LEVEL *levels,UINT32 count)
{
    D3D_FEATURE_LEVEL result=0xdeadbeef;HRESULT hr;
    hr=ID2D1EffectContext_GetMaximumSupportedFeatureLevel(context,levels,count,&result);
    printf("level %u %08lx %08x\n",index,hr,result);
    result=0xdeadbeef;
    hr=ID3D11Device1_CreateDeviceContextState(probe_device,0,levels,count,D3D11_SDK_VERSION,
            &IID_ID3D11Device1,&result,NULL);
    printf("context %u %08lx %08x\n",index,hr,result);
}
int main(void)
{
    ID3D11Device *d3d;IDXGIDevice *dxgi;ID2D1Factory1 *factory;ID2D1Device *device;
    ID2D1DeviceContext *dc;ID2D1Effect *effect;
    D3D_FEATURE_LEVEL requested=getenv("FEATURE_LEVEL_10") ? D3D_FEATURE_LEVEL_10_0 : D3D_FEATURE_LEVEL_11_0;
    static const D3D_FEATURE_LEVEL arrays[][8]={
        {0x9100},{0x9200},{0x9300},{0xa000},{0xa100},{0xb000},{0xb100},{0xc000},{0xc100},{0xc200},
        {0x9100,0xa000,0xb000},{0xb100,0xb000,0xa000},{0xa000,0xb000,0x9300},
        {0},{1},{0xffffffff},{0xafff},{0xa001},{0,0xb000},{0xb000,0},{0xffff,0xa000},
    };
    static const UINT counts[]={1,1,1,1,1,1,1,1,1,1,3,3,3,1,1,1,1,1,2,2,2};
    unsigned int i;HRESULT hr;
    setvbuf(stdout,NULL,_IONBF,0);CoInitializeEx(NULL,COINIT_MULTITHREADED);
    REQUIRE(D3D11CreateDevice(NULL,getenv("D2D1_TEST_WARP") ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE,
            NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,&requested,1,D3D11_SDK_VERSION,&d3d,NULL,NULL));
    printf("device %08x\n",ID3D11Device_GetFeatureLevel(d3d));
    REQUIRE(ID3D11Device_QueryInterface(d3d,&IID_ID3D11Device1,(void **)&probe_device));
    REQUIRE(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    REQUIRE(ID2D1Factory1_CreateDevice(factory,dxgi,&device));
    REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&dc));
    {
        ID2D1Device2 *device2;IDXGIDevice *actual_dxgi;ID3D11Device *actual;
        REQUIRE(ID2D1Device_QueryInterface(device,&IID_ID2D1Device2,(void **)&device2));
        REQUIRE(ID2D1Device2_GetDxgiDevice(device2,&actual_dxgi));
        REQUIRE(IDXGIDevice_QueryInterface(actual_dxgi,&IID_ID3D11Device,(void **)&actual));
        printf("d2d device %08x same %u\n",ID3D11Device_GetFeatureLevel(actual),actual==d3d);
        ID3D11Device_Release(actual);IDXGIDevice_Release(actual_dxgi);ID2D1Device2_Release(device2);
    }
    REQUIRE(ID2D1Factory1_RegisterEffectFromString(factory,&CLSID_FeatureProbe,wrapper_xml,NULL,0,wrapper_factory));
    REQUIRE(ID2D1DeviceContext_CreateEffect(dc,&CLSID_FeatureProbe,&effect));
    for(i=0;i<ARRAY_SIZE(arrays);++i)probe(latest->context,i,arrays[i],counts[i]);
    probe(latest->context,i++,arrays[0],0);
    {
        static const D3D_FEATURE_LEVEL candidates[]={0x9300,0xa000,0xb000,0xb100,0xc000};
        unsigned int j,k;D3D_FEATURE_LEVEL pair[2];
        for(j=0;j<5;++j)for(k=0;k<5;++k)
        {pair[0]=candidates[j];pair[1]=candidates[k];probe(latest->context,100+j*5+k,pair,2);}
    }
    ID2D1Effect_Release(effect);ID2D1Factory1_UnregisterEffect(factory,&CLSID_FeatureProbe);
    ID2D1DeviceContext_Release(dc);ID2D1Device_Release(device);ID2D1Factory1_Release(factory);
    ID3D11Device1_Release(probe_device);IDXGIDevice_Release(dxgi);ID3D11Device_Release(d3d);CoUninitialize();return 0;
}
