/* Public measurements for effects embedded in custom transform graphs.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define main color_context_base_main
#include "color-context-reference.c"
#undef main
#include "d2d1effectauthor.h"
#include "d2d1effects_2.h"

DEFINE_GUID(CLSID_ProbeWrapper,0x93703917,0x56b3,0x4c9c,0x99,0x51,0x38,0x48,0x29,0xfe,0x64,0x21);

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

static void inspect_node(ID2D1TransformNode *node, ID2D1Effect *effect)
{
    const GUID *iids[] = {&IID_IUnknown,&IID_ID2D1TransformNode,&IID_ID2D1Transform,
            &IID_ID2D1DrawTransform,&IID_ID2D1ComputeTransform,&IID_ID2D1SourceTransform,&IID_ID2D1Effect};
    IUnknown *obj;
    HRESULT hr;
    unsigned int i;
    printf("count %u\n",ID2D1TransformNode_GetInputCount(node));
    ID2D1Effect_AddRef(effect);
    printf("effect_refs %lu\n",ID2D1Effect_Release(effect));
    for (i = 0; i < ARRAY_SIZE(iids); ++i)
    {
        obj = (void *)0xdeadbeef;
        hr = ID2D1TransformNode_QueryInterface(node,iids[i],(void **)&obj);
        printf("qi %u %08lx %u %u\n",i,hr,obj == (IUnknown *)node,obj == (void *)0xdeadbeef ? 2 : !!obj);
        if (SUCCEEDED(hr)) IUnknown_Release(obj);
    }
}

static void render(ID2D1DeviceContext *dc, ID2D1Effect *effect, ID2D1Bitmap1 *target,
        ID2D1Bitmap1 *readback, unsigned int label)
{
    ID2D1Image *image;
    D2D1_RECT_F bounds = {-99,-99,-99,-99};
    D2D1_MAPPED_RECT map;
    D2D1_COLOR_F clear = {0,0,0,0};
    HRESULT hr;
    unsigned int i;
    if (getenv("FANOUT_INVALIDATE"))
    {
        ID2D1Image *input;
        ID2D1Effect_GetInput(effect,0,&input);
        ID2D1Effect_SetInput(effect,0,input,TRUE);
        if (input) ID2D1Image_Release(input);
    }
    printf("case %u\n",label);
    ID2D1Effect_GetOutput(effect,&image);
    hr = ID2D1DeviceContext_GetImageLocalBounds(dc,image,&bounds);
    printf("bounds %08lx %.9g %.9g %.9g %.9g\n",hr,bounds.left,bounds.top,bounds.right,bounds.bottom);
    ID2D1DeviceContext_BeginDraw(dc);
    ID2D1DeviceContext_Clear(dc,&clear);
    ID2D1DeviceContext_DrawImage(dc,image,NULL,NULL,D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,D2D1_COMPOSITE_MODE_SOURCE_OVER);
    hr = ID2D1DeviceContext_EndDraw(dc,NULL,NULL);
    printf("draw %08lx\n",hr);
    if (SUCCEEDED(hr) && SUCCEEDED(ID2D1Bitmap1_CopyFromBitmap(readback,NULL,(ID2D1Bitmap *)target,NULL))
            && SUCCEEDED(ID2D1Bitmap1_Map(readback,D2D1_MAP_OPTIONS_READ,&map)))
    {
        printf("pixels");
        for (i = 0; i < 16; ++i) { float f; memcpy(&f,map.bits+4*i,4); printf(" %.9g",f); }
        puts("");
        ID2D1Bitmap1_Unmap(readback);
    }
    ID2D1Image_Release(image);
}

int main(void)
{
    static const float pixels[4][4] = {{.125,.25,.5,.25},{.25,.5,1,.5},{1,2,-.25,1},{.2,.3,.4,0}};
    const GUID *ids[] = {&CLSID_D2D1Premultiply,&CLSID_D2D1UnPremultiply,&CLSID_D2D1Premultiply};
    ID3D11Device *d3d;
    IDXGIDevice *dxgi;
    ID2D1Factory1 *factory;
    ID2D1Device *device;
    ID2D1DeviceContext *dc;
    ID2D1Effect *outer, *effects[3];
    ID2D1TransformNode *nodes[3];
    ID2D1Bitmap1 *source, *target, *readback;
    struct wrapper *w;
    D2D1_SIZE_U size = {4,1};
    D2D1_BITMAP_PROPERTIES1 desc = {{DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},
            96,96,D2D1_BITMAP_OPTIONS_NONE,NULL};
    HRESULT hr;
    unsigned int i;
    setvbuf(stdout,NULL,_IOFBF,65536);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    REQUIRE(D3D11CreateDevice(NULL,getenv("D2D1_TEST_WARP") ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE,
            NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,NULL,0,D3D11_SDK_VERSION,&d3d,NULL,NULL));
    REQUIRE(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    REQUIRE(ID2D1Factory1_RegisterEffectFromString(factory,&CLSID_ProbeWrapper,wrapper_xml,NULL,0,wrapper_factory));
    REQUIRE(ID2D1Factory1_CreateDevice(factory,dxgi,&device));
    REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&dc));
    REQUIRE(ID2D1DeviceContext_CreateEffect(dc,&CLSID_ProbeWrapper,&outer)); w = latest;
    ID2D1TransformGraph_Clear(w->graph);
    for (i=0;i<3;++i)
    {
        REQUIRE(ID2D1EffectContext_CreateEffect(w->context,ids[i],&effects[i]));
        REQUIRE(ID2D1EffectContext_CreateTransformNodeFromEffect(w->context,effects[i],&nodes[i]));
        REQUIRE(ID2D1TransformGraph_AddNode(w->graph,nodes[i]));
    }
    inspect_node(nodes[0],effects[0]);
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,pixels,sizeof(pixels),&desc,&source));
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,NULL,0,&desc,&target));
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,NULL,0,&desc,&readback));
    ID2D1DeviceContext_SetTarget(dc,(ID2D1Image *)target);
    ID2D1Effect_SetInput(outer,0,(ID2D1Image *)source,TRUE);
#define CALL(label,call) do { hr=(call); printf("connect %u %08lx\n",label,hr); } while(0)
#define RENDER(label,node) do { REQUIRE(ID2D1TransformGraph_SetOutputNode(w->graph,nodes[node])); render(dc,outer,target,readback,label); } while(0)
    CALL(0,ID2D1TransformGraph_ConnectToEffectInput(w->graph,0,nodes[0],0));
    CALL(1,ID2D1TransformGraph_ConnectToEffectInput(w->graph,0,nodes[1],0));
    RENDER(0,0);
    RENDER(1,1);
    RENDER(2,0);
    CALL(2,ID2D1TransformGraph_ConnectNode(w->graph,nodes[1],nodes[0],0));
    RENDER(3,0);
    CALL(3,ID2D1TransformGraph_ConnectToEffectInput(w->graph,0,nodes[0],0));
    RENDER(4,0);
    CALL(4,ID2D1TransformGraph_RemoveNode(w->graph,nodes[1]));
    RENDER(5,0);
    ID2D1TransformGraph_Clear(w->graph);
    CALL(5,ID2D1TransformGraph_SetSingleTransformNode(w->graph,nodes[0]));
    RENDER(6,0);
    ID2D1TransformGraph_Clear(w->graph);
    for (i=0;i<3;++i) REQUIRE(ID2D1TransformGraph_AddNode(w->graph,nodes[i]));
    CALL(6,ID2D1TransformGraph_ConnectToEffectInput(w->graph,0,nodes[1],0));
    CALL(7,ID2D1TransformGraph_ConnectNode(w->graph,nodes[1],nodes[0],0));
    CALL(8,ID2D1TransformGraph_ConnectNode(w->graph,nodes[1],nodes[2],0));
    RENDER(7,0);
    RENDER(8,2);
    CALL(9,ID2D1TransformGraph_RemoveNode(w->graph,nodes[2]));
    RENDER(9,0);
    CALL(10,ID2D1TransformGraph_RemoveNode(w->graph,nodes[1]));
    RENDER(10,0);
    CALL(11,ID2D1TransformGraph_ConnectToEffectInput(w->graph,0,nodes[0],0));
    RENDER(11,0);
    ID2D1TransformGraph_Clear(w->graph);
    ID2D1DeviceContext_SetTarget(dc,NULL);
    ID2D1Bitmap1_Release(readback); ID2D1Bitmap1_Release(target); ID2D1Bitmap1_Release(source);
    for (i=0;i<3;++i) { ID2D1TransformNode_Release(nodes[i]); ID2D1Effect_Release(effects[i]); }
    ID2D1Effect_Release(outer);
    ID2D1DeviceContext_Release(dc); ID2D1Device_Release(device); ID2D1Factory1_Release(factory);
    IDXGIDevice_Release(dxgi); ID3D11Device_Release(d3d); CoUninitialize();
    puts("done");
    return 0;
}
