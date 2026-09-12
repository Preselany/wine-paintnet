/* Tests for effects embedded in custom Direct2D transform graphs.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <stdlib.h>
#include <string.h>
#include "wine/test.h"
#include "d2d1effectauthor.h"
#include "d2d1effects_2.h"
#include "d3d11.h"
#include "effect_test.h"
#include "initguid.h"

#define REQUIRE(call) do { hr = (call); ok(hr == S_OK, "%s returned %#lx.\n", #call, hr); if (FAILED(hr)) return; } while (0)

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
    (void)change;
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
    ok(ID2D1TransformNode_GetInputCount(node) == 1, "Unexpected input count.\n");
    ID2D1Effect_AddRef(effect);
    ok(ID2D1Effect_Release(effect) == 2, "Node must retain its effect.\n");
    for (i = 0; i < ARRAY_SIZE(iids); ++i)
    {
        obj = (void *)0xdeadbeef;
        hr = ID2D1TransformNode_QueryInterface(node,iids[i],(void **)&obj);
        ok(hr == (i < 2 ? S_OK : E_NOINTERFACE), "Interface %u returned %#lx.\n", i, hr);
        ok(obj == (i < 2 ? (IUnknown *)node : (IUnknown *)0xdeadbeef), "Unexpected interface output %p.\n", obj);
        if (SUCCEEDED(hr)) IUnknown_Release(obj);
    }
}

static const float pixels[4][4] = {{.1,.2,.3,.5},{.5,1,-.25,1},{.2,.3,.4,0},{0,0,0,0}};

static void render(ID2D1DeviceContext *dc, ID2D1Effect *effect, ID2D1Bitmap1 *target,
        ID2D1Bitmap1 *readback, unsigned int label)
{
    static const float factors[] = {.5,.25,.25,0,.25,.125,.125,0,1};
    HRESULT expected = label == 3 ? D2DERR_INVALID_GRAPH_CONFIGURATION : label == 7 ? D2DERR_CYCLIC_GRAPH : S_OK;
    ID2D1Image *image;
    D2D1_RECT_F bounds = {-99,-99,-99,-99};
    D2D1_MAPPED_RECT map;
    D2D1_COLOR_F clear = {0,0,0,0};
    HRESULT hr;
    unsigned int i;
    winetest_push_context("case %u",label);
    ID2D1Effect_GetOutput(effect,&image);
    hr = ID2D1DeviceContext_GetImageLocalBounds(dc,image,&bounds);
    ok(hr == expected,"Bounds returned %#lx, expected %#lx.\n",hr,expected);
    if (SUCCEEDED(hr))
        ok(bounds.left == 0 && bounds.top == 0 && bounds.right == 4 && bounds.bottom == 1,
                "Unexpected bounds %.9g %.9g %.9g %.9g.\n",bounds.left,bounds.top,bounds.right,bounds.bottom);
    else ok(bounds.left == -99 && bounds.top == -99 && bounds.right == -99 && bounds.bottom == -99,
            "Failed bounds call modified output.\n");
    ID2D1DeviceContext_BeginDraw(dc);
    ID2D1DeviceContext_Clear(dc,&clear);
    ID2D1DeviceContext_DrawImage(dc,image,NULL,NULL,D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,D2D1_COMPOSITE_MODE_SOURCE_OVER);
    hr = ID2D1DeviceContext_EndDraw(dc,NULL,NULL);
    ok(hr == expected,"Draw returned %#lx, expected %#lx.\n",hr,expected);
    if (SUCCEEDED(hr))
    {
        hr = ID2D1Bitmap1_CopyFromBitmap(readback,NULL,(ID2D1Bitmap *)target,NULL);
        ok(hr == S_OK,"Copy returned %#lx.\n",hr);
        hr = ID2D1Bitmap1_Map(readback,D2D1_MAP_OPTIONS_READ,&map);
        ok(hr == S_OK,"Map returned %#lx.\n",hr);
        if (SUCCEEDED(hr))
        {
            for (i = 0; i < 16; ++i)
            {
                float actual, value = pixels[i/4][i%4] * factors[label];
                memcpy(&actual,map.bits+4*i,4);
                ok(actual == value,"Channel %u: %.9g, expected %.9g.\n",i,actual,value);
            }
            hr = ID2D1Bitmap1_Unmap(readback);
            ok(hr == S_OK,"Unmap returned %#lx.\n",hr);
        }
    }
    ID2D1Image_Release(image);
    winetest_pop_context();
}

START_TEST(effect_node)
{
    ID3D11Device *d3d;
    IDXGIDevice *dxgi;
    ID2D1Factory1 *factory;
    ID2D1Device *device;
    ID2D1DeviceContext *dc, *dc2;
    ID2D1Effect *outer, *inner, *nested, *composite, *other;
    ID2D1TransformNode *node, *node2;
    ID2D1Bitmap1 *source, *target, *readback;
    ID2D1Image *input;
    struct wrapper *w, *w2;
    D2D1_SIZE_U size = {4,1};
    D2D1_BITMAP_PROPERTIES1 desc = {{DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},
            96,96,D2D1_BITMAP_OPTIONS_NONE,NULL};
    float opacity = .5f;
    HRESULT hr;
    unsigned int i;

    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    REQUIRE(D3D11CreateDevice(NULL,effect_test_driver(),
            NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,NULL,0,D3D11_SDK_VERSION,&d3d,NULL,NULL));
    REQUIRE(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    REQUIRE(ID2D1Factory1_RegisterEffectFromString(factory,&CLSID_ProbeWrapper,wrapper_xml,NULL,0,wrapper_factory));
    REQUIRE(ID2D1Factory1_CreateDevice(factory,dxgi,&device));
    REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&dc));
    REQUIRE(ID2D1DeviceContext_CreateEffect(dc,&CLSID_ProbeWrapper,&outer)); w = latest;
    REQUIRE(ID2D1EffectContext_CreateEffect(w->context,&CLSID_D2D1Opacity,&inner));
    REQUIRE(ID2D1Effect_SetValue(inner,0,D2D1_PROPERTY_TYPE_FLOAT,(BYTE *)&opacity,sizeof(opacity)));
    hr = ID2D1EffectContext_CreateTransformNodeFromEffect(w->context,inner,&node);
    ok(hr == S_OK,"Node creation returned %#lx.\n",hr);
    if (FAILED(hr)) return;
    inspect_node(node,inner);
    REQUIRE(ID2D1EffectContext_CreateTransformNodeFromEffect(w->context,inner,&node2));
    ok(node != node2,"Repeated creation returned the same node.\n");
    ID2D1TransformNode_Release(node2);
    REQUIRE(ID2D1TransformGraph_SetSingleTransformNode(w->graph,node));
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,pixels,sizeof(pixels),&desc,&source));
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,NULL,0,&desc,&target));
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,NULL,0,&desc,&readback));
    ID2D1DeviceContext_SetTarget(dc,(ID2D1Image *)target);
    ID2D1Effect_SetInput(outer,0,(ID2D1Image *)source,TRUE);
    render(dc,outer,target,readback,0);
    ID2D1Effect_GetInput(inner,0,&input); ok(!input,"Wrapped effect input changed.\n"); if(input) ID2D1Image_Release(input);
    opacity = .25f;
    REQUIRE(ID2D1Effect_SetValue(inner,0,D2D1_PROPERTY_TYPE_FLOAT,(BYTE *)&opacity,sizeof(opacity)));
    render(dc,outer,target,readback,1);
    ID2D1Effect_SetInput(inner,0,(ID2D1Image *)source,TRUE);
    render(dc,outer,target,readback,2);
    ID2D1Effect_SetInput(outer,0,NULL,TRUE);
    render(dc,outer,target,readback,3);
    ID2D1Effect_SetInput(outer,0,(ID2D1Image *)source,TRUE);
    REQUIRE(ID2D1DeviceContext_CreateEffect(dc,&CLSID_ProbeWrapper,&nested)); w2 = latest;
    REQUIRE(ID2D1EffectContext_CreateTransformNodeFromEffect(w2->context,outer,&node2));
    REQUIRE(ID2D1TransformGraph_SetSingleTransformNode(w2->graph,node2));
    ID2D1Effect_SetInput(nested,0,(ID2D1Image *)source,TRUE);
    render(dc,nested,target,readback,4);
    ID2D1TransformNode_Release(node2);
    REQUIRE(ID2D1DeviceContext_CreateEffect(dc,&CLSID_D2D1Composite,&composite));
    REQUIRE(ID2D1EffectContext_CreateTransformNodeFromEffect(w->context,composite,&node2));
    for (i = 0; i < 4; ++i)
    {
        hr = ID2D1Effect_SetInputCount(composite,i);
        ok(hr == (i ? S_OK : E_INVALIDARG),"SetInputCount(%u) returned %#lx.\n",i,hr);
        ok(ID2D1Effect_GetInputCount(composite) == (i ? i : 2),"Unexpected effect input count.\n");
        ok(ID2D1TransformNode_GetInputCount(node2) == (i ? i : 2),"Node input count did not track the effect.\n");
    }
    ID2D1TransformNode_Release(node2); ID2D1Effect_Release(composite);
    REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&dc2));
    REQUIRE(ID2D1DeviceContext_CreateEffect(dc2,&CLSID_D2D1Opacity,&other));
    hr = ID2D1EffectContext_CreateTransformNodeFromEffect(w->context,other,&node2);
    ok(hr == S_OK,"Other context returned %#lx.\n",hr);
    if (SUCCEEDED(hr)) ID2D1TransformNode_Release(node2);
    ID2D1Effect_Release(other); ID2D1DeviceContext_Release(dc2);
    REQUIRE(ID2D1DeviceContext_CreateEffect(dc,&CLSID_D2D1Opacity,&other));
    opacity = .5f;
    REQUIRE(ID2D1Effect_SetValue(other,0,D2D1_PROPERTY_TYPE_FLOAT,(BYTE *)&opacity,sizeof(opacity)));
    REQUIRE(ID2D1EffectContext_CreateTransformNodeFromEffect(w->context,other,&node2));
    REQUIRE(ID2D1TransformGraph_AddNode(w->graph,node2));
    REQUIRE(ID2D1TransformGraph_ConnectNode(w->graph,node,node2,0));
    REQUIRE(ID2D1TransformGraph_SetOutputNode(w->graph,node2));
    render(dc,outer,target,readback,5);
    render(dc,nested,target,readback,6);
    REQUIRE(ID2D1TransformGraph_ConnectNode(w->graph,node2,node,0));
    render(dc,outer,target,readback,7);
    ID2D1TransformGraph_Clear(w->graph);
    ID2D1TransformNode_Release(node2); ID2D1Effect_Release(other);
    REQUIRE(ID2D1TransformGraph_SetPassthroughGraph(w->graph,0));
    render(dc,outer,target,readback,8);
    ID2D1DeviceContext_SetTarget(dc,NULL);
    ID2D1Bitmap1_Release(readback); ID2D1Bitmap1_Release(target); ID2D1Bitmap1_Release(source);
    ID2D1Effect_Release(nested); ID2D1Effect_Release(outer);
    ID2D1TransformNode_Release(node); ID2D1Effect_Release(inner);
    ID2D1DeviceContext_Release(dc); ID2D1Device_Release(device); ID2D1Factory1_Release(factory);
    IDXGIDevice_Release(dxgi); ID3D11Device_Release(d3d); CoUninitialize();

}
