/* Public measurements for offset nodes in custom transform graphs.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define main color_context_base_main
#include "color-context-reference.c"
#undef main
#include "d2d1effectauthor.h"
#include "d2d1effects_2.h"

DEFINE_GUID(CLSID_OffsetProbe,0x93703918,0x56b3,0x4c9c,0x99,0x51,0x38,0x48,0x29,0xfe,0x64,0x21);

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


static int render(ID2D1DeviceContext *dc,ID2D1Effect *effect,ID2D1Bitmap1 *target,ID2D1Bitmap1 *readback)
{
    ID2D1Image *image;D2D1_RECT_F bounds={-99,-99,-99,-99};D2D1_MAPPED_RECT map;
    const D2D1_COLOR_F clear={0};HRESULT hr;unsigned int x,y;
    ID2D1Effect_GetOutput(effect,&image);
    hr=ID2D1DeviceContext_GetImageLocalBounds(dc,image,&bounds);
    printf("bounds %08lx %.9g %.9g %.9g %.9g\n",hr,bounds.left,bounds.top,bounds.right,bounds.bottom);
    ID2D1DeviceContext_BeginDraw(dc);ID2D1DeviceContext_Clear(dc,&clear);
    ID2D1DeviceContext_DrawImage(dc,image,NULL,NULL,D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,D2D1_COMPOSITE_MODE_SOURCE_OVER);
    hr=ID2D1DeviceContext_EndDraw(dc,NULL,NULL);printf("draw %08lx\n",hr);
    if (SUCCEEDED(hr))
    {
        REQUIRE(ID2D1Bitmap1_CopyFromBitmap(readback,NULL,(ID2D1Bitmap *)target,NULL));
        REQUIRE(ID2D1Bitmap1_Map(readback,D2D1_MAP_OPTIONS_READ,&map));
        printf("pixels");
        for(y=0;y<8;++y)for(x=0;x<12*4;++x){float f;memcpy(&f,map.bits+y*map.pitch+x*4,4);printf(" %.9g",f);}
        puts("");REQUIRE(ID2D1Bitmap1_Unmap(readback));
    }
    ID2D1Image_Release(image);
    return 0;
}
int main(void)
{
    static const float pixels[2][4][4]={{{.1,.2,.3,.5},{.5,1,-.25,1},{.2,.3,.4,0},{0,0,0,0}},
            {{1,.5,.25,1},{.25,.125,.0625,.5},{-1,.5,2,1},{.3,.2,.1,.75}}};
    static const D2D1_POINT_2L offsets[]={{0,0},{3,2},{-1,1},{6,-1}};
    static const float dpis[]={96,144,192};
    ID3D11Device *d3d;IDXGIDevice *dxgi;ID2D1Factory1 *factory;ID2D1Device *device;ID2D1DeviceContext *dc;
    ID2D1Bitmap1 *source,*target,*readback;ID2D1Effect *outer,*opacity;
    ID2D1OffsetTransform *offset,*second;ID2D1TransformNode *opacity_node;
    D2D1_POINT_2L zero={0},extra={-1,1},actual;
    D3D_FEATURE_LEVEL requested=getenv("FEATURE_LEVEL_10") ? D3D_FEATURE_LEVEL_10_0 : D3D_FEATURE_LEVEL_11_0;
    D2D1_BITMAP_PROPERTIES1 desc={{DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},96,96,0,NULL};
    struct wrapper *w;HRESULT hr;unsigned int dpi,units,path,i;float alpha=.5;
    setvbuf(stdout,NULL,_IOFBF,65536);CoInitializeEx(NULL,COINIT_MULTITHREADED);
    REQUIRE(D3D11CreateDevice(NULL,getenv("D2D1_TEST_WARP") ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE,
            NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,&requested,1,D3D11_SDK_VERSION,&d3d,NULL,NULL));
    REQUIRE(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    REQUIRE(ID2D1Factory1_RegisterEffectFromString(factory,&CLSID_OffsetProbe,wrapper_xml,NULL,0,wrapper_factory));
    REQUIRE(ID2D1Factory1_CreateDevice(factory,dxgi,&device));
    REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&dc));
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,(D2D1_SIZE_U){4,2},pixels,sizeof(pixels[0]),&desc,&source));
    desc.bitmapOptions=D2D1_BITMAP_OPTIONS_TARGET;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,(D2D1_SIZE_U){12,8},NULL,0,&desc,&target));
    desc.bitmapOptions=D2D1_BITMAP_OPTIONS_CPU_READ|D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,(D2D1_SIZE_U){12,8},NULL,0,&desc,&readback));
    ID2D1DeviceContext_SetTarget(dc,(ID2D1Image *)target);
    REQUIRE(ID2D1DeviceContext_CreateEffect(dc,&CLSID_OffsetProbe,&outer));w=latest;
    REQUIRE(ID2D1EffectContext_CreateOffsetTransform(w->context,zero,&offset));
    REQUIRE(ID2D1EffectContext_CreateOffsetTransform(w->context,extra,&second));
    opacity=NULL;opacity_node=NULL;
    if(requested!=D3D_FEATURE_LEVEL_10_0)
    {
    REQUIRE(ID2D1EffectContext_CreateEffect(w->context,&CLSID_D2D1Opacity,&opacity));
    REQUIRE(ID2D1Effect_SetValue(opacity,0,D2D1_PROPERTY_TYPE_FLOAT,(BYTE *)&alpha,sizeof(alpha)));
    REQUIRE(ID2D1EffectContext_CreateTransformNodeFromEffect(w->context,opacity,&opacity_node));
    }
    ID2D1Effect_SetInput(outer,0,(ID2D1Image *)source,TRUE);
    for(dpi=0;dpi<ARRAY_SIZE(dpis);++dpi)for(units=0;units<2;++units)for(path=0;path<3;++path)
    {
        if(path==1 && !opacity)continue; /* Opacity uses a separate feature-11 renderer in Wine. */
        ID2D1DeviceContext_SetDpi(dc,dpis[dpi],dpis[dpi]);ID2D1DeviceContext_SetUnitMode(dc,units);
        REQUIRE(ID2D1TransformGraph_SetSingleTransformNode(w->graph,(ID2D1TransformNode *)offset));
        if(path)
        {
            ID2D1TransformNode *next=path==1 ? opacity_node : (ID2D1TransformNode *)second;
            REQUIRE(ID2D1TransformGraph_AddNode(w->graph,next));
            REQUIRE(ID2D1TransformGraph_ConnectNode(w->graph,(ID2D1TransformNode *)offset,next,0));
            REQUIRE(ID2D1TransformGraph_SetOutputNode(w->graph,next));
        }
        for(i=0;i<ARRAY_SIZE(offsets);++i)
        {
            ID2D1OffsetTransform_SetOffset(offset,offsets[i]);actual=ID2D1OffsetTransform_GetOffset(offset);
            printf("case %u %u %u %u\noffset %ld %ld\n",dpi,units,path,i,actual.x,actual.y);
            if (render(dc,outer,target,readback)) return 1;
            fflush(stdout);
        }
    }
    {
        static const D2D1_POINT_2L extremes[]={{0x7ffffffe,0},{(-0x7fffffff-1),0},{0x1000000,-0x1000000}};
        ID2D1Image *image;D2D1_RECT_F bounds;
        REQUIRE(ID2D1TransformGraph_SetSingleTransformNode(w->graph,(ID2D1TransformNode *)offset));
        ID2D1Effect_GetOutput(outer,&image);
        for(i=0;i<ARRAY_SIZE(extremes);++i)
        {
            ID2D1OffsetTransform_SetOffset(offset,extremes[i]);
            bounds=(D2D1_RECT_F){-99,-99,-99,-99};
            hr=ID2D1DeviceContext_GetImageLocalBounds(dc,image,&bounds);
            printf("extreme %u %08lx %.9g %.9g %.9g %.9g\n",i,hr,bounds.left,bounds.top,bounds.right,bounds.bottom);
        }
        ID2D1Image_Release(image);
    }
    ID2D1TransformGraph_Clear(w->graph);if(opacity_node)ID2D1TransformNode_Release(opacity_node);
    if(opacity)ID2D1Effect_Release(opacity);
    ID2D1OffsetTransform_Release(second);ID2D1OffsetTransform_Release(offset);
    ID2D1Effect_Release(outer);ID2D1Factory1_UnregisterEffect(factory,&CLSID_OffsetProbe);
    ID2D1DeviceContext_SetTarget(dc,NULL);ID2D1Bitmap1_Release(readback);ID2D1Bitmap1_Release(target);ID2D1Bitmap1_Release(source);
    ID2D1DeviceContext_Release(dc);ID2D1Device_Release(device);ID2D1Factory1_Release(factory);
    IDXGIDevice_Release(dxgi);ID3D11Device_Release(d3d);CoUninitialize();puts("done");return 0;
}
