/* Native Composite input ordering, bounds and pixel measurements.
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
    ID3D11Device *d3d;IDXGIDevice *dxgi;ID2D1Factory1 *factory;ID2D1Device *device;ID2D1DeviceContext *dc;
    ID2D1Effect *outer[3],*effect;ID2D1OffsetTransform *nodes[3];ID2D1Image *image,*inputs[3];
    ID2D1Bitmap1 *sources[3],*target,*readback;
    D2D1_BITMAP_PROPERTIES1 desc={{DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},96,96,0,NULL};
    const D2D1_SIZE_U sizes[]={{3,2},{2,3},{2,2}};
    const D2D1_POINT_2L positions[][3]={{{0,0},{0,0},{0,0}},{{-1,1},{1,0},{2,2}},{{-3,-2},{2,1},{5,-1}}};
    static const float alphas[]={-.25f,0,.5f,.75f,1,1.5f};
    D2D1_POINT_2F offset={4,4};D2D1_COLOR_F clear={0};D2D1_RECT_F bounds;D2D1_MAPPED_RECT map;
    float pixels[3][3][4];UINT i,x,y,count,mode,origin,dpi,unit,value;HRESULT hr;
    setvbuf(stdout,NULL,_IOFBF,65536);CoInitializeEx(NULL,COINIT_MULTITHREADED);
    REQUIRE(D3D11CreateDevice(NULL,getenv("D2D1_TEST_WARP")?D3D_DRIVER_TYPE_WARP:D3D_DRIVER_TYPE_HARDWARE,NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,NULL,0,D3D11_SDK_VERSION,&d3d,NULL,NULL));
    REQUIRE(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    REQUIRE(ID2D1Factory1_CreateDevice(factory,dxgi,&device));REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&dc));
    REQUIRE(ID2D1Factory1_RegisterEffectFromString(factory,&CLSID_OffsetProbe,wrapper_xml,NULL,0,wrapper_factory));
    for(i=0;i<3;++i)
    {
        for(y=0;y<3;++y)for(x=0;x<3;++x)
        {
            pixels[y][x][0]=(x+1)*.25f+i*.125f;
            pixels[y][x][1]=(y+1)*.125f-i*.125f;
            pixels[y][x][2]=i==0?1.25f:-.25f;
            pixels[y][x][3]=alphas[(y*sizes[i].width+x+i)%6];
        }
        REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,sizes[i],pixels,sizeof(pixels[0]),&desc,&sources[i]));
        REQUIRE(ID2D1DeviceContext_CreateEffect(dc,&CLSID_OffsetProbe,&outer[i]));
        REQUIRE(ID2D1EffectContext_CreateOffsetTransform(latest->context,(D2D1_POINT_2L){0,0},&nodes[i]));
        REQUIRE(ID2D1TransformGraph_SetSingleTransformNode(latest->graph,(ID2D1TransformNode *)nodes[i]));
        ID2D1Effect_SetInput(outer[i],0,(ID2D1Image *)sources[i],TRUE);ID2D1Effect_GetOutput(outer[i],&inputs[i]);
    }
    desc.bitmapOptions=D2D1_BITMAP_OPTIONS_TARGET;REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,(D2D1_SIZE_U){8,8},NULL,0,&desc,&target));
    desc.bitmapOptions=D2D1_BITMAP_OPTIONS_CPU_READ|D2D1_BITMAP_OPTIONS_CANNOT_DRAW;REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,(D2D1_SIZE_U){8,8},NULL,0,&desc,&readback));
    REQUIRE(ID2D1DeviceContext_CreateEffect(dc,&CLSID_D2D1Composite,&effect));
    REQUIRE(ID2D1Effect_GetValue(effect,0,D2D1_PROPERTY_TYPE_ENUM,(BYTE *)&value,sizeof(value)));
    printf("default %u inputs %u properties %u\n",value,ID2D1Effect_GetInputCount(effect),ID2D1Effect_GetPropertyCount(effect));
    for(mode=0;mode<15;++mode)
    {
        value=0;REQUIRE(ID2D1Effect_SetValue(effect,0,D2D1_PROPERTY_TYPE_ENUM,(BYTE *)&value,sizeof(value)));
        HRESULT set_hr=ID2D1Effect_SetValue(effect,0,D2D1_PROPERTY_TYPE_ENUM,(BYTE *)&mode,sizeof(mode));
        REQUIRE(ID2D1Effect_GetValue(effect,0,D2D1_PROPERTY_TYPE_ENUM,(BYTE *)&value,sizeof(value)));
        printf("enum %u %08lx %u\n",mode,set_hr,value);
    }
    hr=ID2D1Effect_SetInputCount(effect,0);printf("count_zero %08lx %u\n",hr,ID2D1Effect_GetInputCount(effect));
    ID2D1Effect_GetOutput(effect,&image);ID2D1DeviceContext_SetTarget(dc,(ID2D1Image *)target);
    for(count=1;count<=3;++count)
    {
        REQUIRE(ID2D1Effect_SetInputCount(effect,count));for(i=0;i<count;++i)ID2D1Effect_SetInput(effect,i,inputs[i],TRUE);
        for(origin=0;origin<3;++origin)for(mode=0;mode<13;++mode)for(dpi=96;dpi<=192;dpi+=48)for(unit=0;unit<2;++unit)
        {
            printf("case %u %u %u %u %u\n",count,origin,mode,dpi,unit);
            for(i=0;i<count;++i)ID2D1OffsetTransform_SetOffset(nodes[i],positions[origin][i]);
            REQUIRE(ID2D1Effect_SetValue(effect,0,D2D1_PROPERTY_TYPE_ENUM,(BYTE *)&mode,sizeof(mode)));
            ID2D1DeviceContext_SetDpi(dc,dpi,dpi);ID2D1DeviceContext_SetUnitMode(dc,unit);
            bounds=(D2D1_RECT_F){-99,-99,-99,-99};hr=ID2D1DeviceContext_GetImageLocalBounds(dc,image,&bounds);
            printf("bounds %08lx %.9g %.9g %.9g %.9g\n",hr,bounds.left,bounds.top,bounds.right,bounds.bottom);
            ID2D1DeviceContext_BeginDraw(dc);ID2D1DeviceContext_Clear(dc,&clear);
            ID2D1DeviceContext_DrawImage(dc,image,&offset,NULL,D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,D2D1_COMPOSITE_MODE_SOURCE_OVER);
            hr=ID2D1DeviceContext_EndDraw(dc,NULL,NULL);printf("draw %08lx\n",hr);
            if(SUCCEEDED(hr))
            {
                REQUIRE(ID2D1Bitmap1_CopyFromBitmap(readback,NULL,(ID2D1Bitmap *)target,NULL));REQUIRE(ID2D1Bitmap1_Map(readback,D2D1_MAP_OPTIONS_READ,&map));
                printf("pixels");for(y=0;y<8;++y)for(x=0;x<32;++x){float v;memcpy(&v,map.bits+y*map.pitch+x*4,4);printf(" %.9g",v);}puts("");REQUIRE(ID2D1Bitmap1_Unmap(readback));
            }
            fflush(stdout);
        }
    }
    ID2D1DeviceContext_SetTarget(dc,NULL);ID2D1Image_Release(image);ID2D1Effect_Release(effect);
    for(i=0;i<3;++i){ID2D1Image_Release(inputs[i]);ID2D1Effect_Release(outer[i]);ID2D1OffsetTransform_Release(nodes[i]);ID2D1Bitmap1_Release(sources[i]);}
    ID2D1Factory1_UnregisterEffect(factory,&CLSID_OffsetProbe);ID2D1Bitmap1_Release(readback);ID2D1Bitmap1_Release(target);
    ID2D1DeviceContext_Release(dc);ID2D1Device_Release(device);ID2D1Factory1_Release(factory);IDXGIDevice_Release(dxgi);ID3D11Device_Release(d3d);CoUninitialize();puts("done");return 0;
}
