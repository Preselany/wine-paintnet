/* Native Crop rectangle, border, bounds and pixel measurements.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define main color_context_base_main
#include "color-context-reference.c"
#undef main
#include "d2d1effects.h"
#include <float.h>
#include "d2d1effectauthor.h"
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




int main(void)
{
    ID3D11Device *d3d;
    IDXGIDevice *dxgi;
    ID2D1Factory1 *factory;
    ID2D1Device *device;
    ID2D1DeviceContext *dc;
    ID2D1Bitmap1 *source,*target,*readback;
    ID2D1Effect *effect, *outer;
    ID2D1OffsetTransform *node;
    ID2D1Image *input_image;
    static const D2D1_POINT_2L offsets[]={{0,0},{-2,-1},{2,1},{-5,2}};
    ID2D1Image *image;
    D2D1_BITMAP_PROPERTIES1 desc={{DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},96,96,0,NULL};
    D2D1_COLOR_F clear={0};
    D2D1_POINT_2F offset={4,4};
    D2D1_RECT_F bounds;
    D2D1_MAPPED_RECT mapped;
    D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_10_0;
    BOOL fl10=getenv("D2D1_TEST_FL10")!=NULL;
    float pixels[3][5][4];
    static const D2D1_RECT_F rectangles[]={
        {-FLT_MAX,-FLT_MAX,FLT_MAX,FLT_MAX},{-1.75f,-.75f,2.6f,1.6f},
        {1.2f,.3f,3.8f,2.7f},{5,0,8,2},
        {-1.75f,-.75f,-.25f,.25f},{-1.5f,-.5f,1.5f,1.5f},
        {-2.25f,-1.25f,-.75f,.75f},{-1.999f,-.999f,2.999f,1.999f}};
    unsigned int rect,dpi,unit,border,x,y,origin,alpha;
    HRESULT hr;
    setvbuf(stdout,NULL,_IOFBF,65536);
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    REQUIRE(D3D11CreateDevice(NULL,getenv("D2D1_TEST_WARP")?D3D_DRIVER_TYPE_WARP:D3D_DRIVER_TYPE_HARDWARE,
            NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,fl10?&level:NULL,fl10?1:0,D3D11_SDK_VERSION,&d3d,NULL,NULL));
    REQUIRE(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    REQUIRE(ID2D1Factory1_CreateDevice(factory,dxgi,&device));
    REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&dc));
    for(y=0;y<3;++y)for(x=0;x<5;++x)
    {
        pixels[y][x][0]=x*.125f;
        pixels[y][x][1]=y*.25f;
        pixels[y][x][2]=(x==2 && y==1)?1:0;
        pixels[y][x][3]=.5f+((x+y)%2)*.25f;
    }
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,(D2D1_SIZE_U){5,3},pixels,sizeof(pixels[0]),&desc,&source));
    desc.bitmapOptions=D2D1_BITMAP_OPTIONS_TARGET;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,(D2D1_SIZE_U){12,10},NULL,0,&desc,&target));
    desc.bitmapOptions=D2D1_BITMAP_OPTIONS_CPU_READ|D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,(D2D1_SIZE_U){12,10},NULL,0,&desc,&readback));
    REQUIRE(ID2D1DeviceContext_CreateEffect(dc,&CLSID_D2D1Crop,&effect));
    REQUIRE(ID2D1Factory1_RegisterEffectFromString(factory,&CLSID_OffsetProbe,wrapper_xml,NULL,0,wrapper_factory));
    REQUIRE(ID2D1DeviceContext_CreateEffect(dc,&CLSID_OffsetProbe,&outer));
    REQUIRE(ID2D1EffectContext_CreateOffsetTransform(latest->context,(D2D1_POINT_2L){0,0},&node));
    REQUIRE(ID2D1TransformGraph_SetSingleTransformNode(latest->graph,(ID2D1TransformNode *)node));
    ID2D1Effect_GetOutput(outer,&input_image);
    ID2D1Effect_SetInput(effect,0,input_image,TRUE);
    ID2D1Image_Release(input_image);
    ID2D1Bitmap1_Release(source);
    ID2D1Effect_GetOutput(effect,&image);
    ID2D1DeviceContext_SetTarget(dc,(ID2D1Image *)target);
    for(alpha=0;alpha<2;++alpha)
    {
    desc.bitmapOptions=D2D1_BITMAP_OPTIONS_NONE;
    desc.pixelFormat.alphaMode=alpha ? D2D1_ALPHA_MODE_IGNORE : D2D1_ALPHA_MODE_PREMULTIPLIED;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,(D2D1_SIZE_U){5,3},pixels,sizeof(pixels[0]),&desc,&source));
    ID2D1Effect_SetInput(outer,0,(ID2D1Image *)source,TRUE);
    for(origin=0;origin<ARRAY_SIZE(offsets);++origin)
    for(dpi=96;dpi<=192;dpi+=48)for(unit=0;unit<2;++unit)for(rect=0;rect<ARRAY_SIZE(rectangles);++rect)for(border=0;border<2;++border)
    {
        printf("case %u %u %u %u %u %u\n",alpha,origin,dpi,unit,rect,border);
        ID2D1OffsetTransform_SetOffset(node,offsets[origin]);
        ID2D1DeviceContext_SetDpi(dc,dpi,dpi);ID2D1DeviceContext_SetUnitMode(dc,unit);
        REQUIRE(ID2D1Effect_SetValue(effect,D2D1_CROP_PROP_RECT,D2D1_PROPERTY_TYPE_VECTOR4,(BYTE *)&rectangles[rect],sizeof(D2D1_RECT_F)));
        REQUIRE(ID2D1Effect_SetValue(effect,D2D1_CROP_PROP_BORDER_MODE,D2D1_PROPERTY_TYPE_ENUM,(BYTE *)&border,sizeof(border)));
        bounds=(D2D1_RECT_F){-99,-99,-99,-99};
        hr=ID2D1DeviceContext_GetImageLocalBounds(dc,image,&bounds);
        printf("bounds %08lx %.9g %.9g %.9g %.9g\n",hr,bounds.left,bounds.top,bounds.right,bounds.bottom);
        ID2D1DeviceContext_BeginDraw(dc);ID2D1DeviceContext_Clear(dc,&clear);
        ID2D1DeviceContext_DrawImage(dc,image,&offset,NULL,D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,D2D1_COMPOSITE_MODE_SOURCE_OVER);
        hr=ID2D1DeviceContext_EndDraw(dc,NULL,NULL);printf("draw %08lx\n",hr);
        if(SUCCEEDED(hr))
        {
            REQUIRE(ID2D1Bitmap1_CopyFromBitmap(readback,NULL,(ID2D1Bitmap *)target,NULL));
            REQUIRE(ID2D1Bitmap1_Map(readback,D2D1_MAP_OPTIONS_READ,&mapped));printf("pixels");
            for(y=0;y<10;++y)for(x=0;x<48;++x){float v;memcpy(&v,mapped.bits+y*mapped.pitch+x*4,4);printf(" %.9g",v);}
            puts("");REQUIRE(ID2D1Bitmap1_Unmap(readback));
        }
        fflush(stdout);
    }
    ID2D1Bitmap1_Release(source);
    }
    ID2D1OffsetTransform_Release(node);ID2D1Effect_Release(outer);
    ID2D1Factory1_UnregisterEffect(factory,&CLSID_OffsetProbe);
    ID2D1DeviceContext_SetTarget(dc,NULL);ID2D1Image_Release(image);ID2D1Effect_Release(effect);
    ID2D1Bitmap1_Release(readback);ID2D1Bitmap1_Release(target);
    ID2D1DeviceContext_Release(dc);ID2D1Device_Release(device);ID2D1Factory1_Release(factory);
    IDXGIDevice_Release(dxgi);ID3D11Device_Release(d3d);CoUninitialize();puts("done");return 0;
}
