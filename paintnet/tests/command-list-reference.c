/* Public-API command-list recording and lifecycle measurements.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <stdio.h>
#include <stdlib.h>
#include "initguid.h"
#include "d2d1_1.h"
#include "d3d11.h"

static ID2D1DeviceContext *replay;

static HRESULT STDMETHODCALLTYPE sink_QueryInterface(ID2D1CommandSink *This, REFIID riid, void **ppvObject)
{
    *ppvObject = NULL;
    if (!IsEqualGUID(riid,&IID_IUnknown) && !IsEqualGUID(riid,&IID_ID2D1CommandSink)) return E_NOINTERFACE;
    *ppvObject = This;
    return S_OK;
}
static ULONG STDMETHODCALLTYPE sink_AddRef(ID2D1CommandSink *This)
{
    (void)This; return 2;
}
static ULONG STDMETHODCALLTYPE sink_Release(ID2D1CommandSink *This)
{
    (void)This; return 2;
}
static HRESULT STDMETHODCALLTYPE sink_BeginDraw(ID2D1CommandSink *This)
{
    (void)This;
    puts("cmd BeginDraw");
    if (replay) { ID2D1DeviceContext_BeginDraw(replay); }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_EndDraw(ID2D1CommandSink *This)
{
    (void)This;
    puts("cmd EndDraw");
    if (replay) { return ID2D1DeviceContext_EndDraw(replay,NULL,NULL); }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_SetAntialiasMode(ID2D1CommandSink *This, D2D1_ANTIALIAS_MODE antialias_mode)
{
    (void)This;
    (void)antialias_mode;
    puts("cmd SetAntialiasMode");
    if (replay) { ID2D1DeviceContext_SetAntialiasMode(replay,antialias_mode); }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_SetTags(ID2D1CommandSink *This, D2D1_TAG tag1, D2D1_TAG tag2)
{
    (void)This;
    (void)tag1;
    (void)tag2;
    printf("cmd SetTags %llu %llu\n",tag1,tag2);
    if (replay) { ID2D1DeviceContext_SetTags(replay,tag1,tag2); }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_SetTextAntialiasMode(ID2D1CommandSink *This, D2D1_TEXT_ANTIALIAS_MODE antialias_mode)
{
    (void)This;
    (void)antialias_mode;
    puts("cmd SetTextAntialiasMode");
    if (replay) { ID2D1DeviceContext_SetTextAntialiasMode(replay,antialias_mode); }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_SetTextRenderingParams(ID2D1CommandSink *This, IDWriteRenderingParams *text_rendering_params)
{
    (void)This;
    (void)text_rendering_params;
    puts("cmd SetTextRenderingParams");
    if (replay) { ID2D1DeviceContext_SetTextRenderingParams(replay,text_rendering_params); }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_SetTransform(ID2D1CommandSink *This, const D2D1_MATRIX_3X2_F *transform)
{
    (void)This;
    (void)transform;
    printf("cmd SetTransform %g %g %g %g %g %g\n",transform->m11,transform->m12,transform->m21,transform->m22,transform->dx,transform->dy);
    if (replay) { ID2D1DeviceContext_SetTransform(replay,transform); }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_SetPrimitiveBlend(ID2D1CommandSink *This, D2D1_PRIMITIVE_BLEND primitive_blend)
{
    (void)This;
    (void)primitive_blend;
    puts("cmd SetPrimitiveBlend");
    if (replay) { ID2D1DeviceContext_SetPrimitiveBlend(replay,primitive_blend); }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_SetUnitMode(ID2D1CommandSink *This, D2D1_UNIT_MODE unit_mode)
{
    (void)This;
    (void)unit_mode;
    puts("cmd SetUnitMode");
    if (replay) { ID2D1DeviceContext_SetUnitMode(replay,unit_mode); }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_Clear(ID2D1CommandSink *This, const D2D1_COLOR_F *color)
{
    (void)This;
    (void)color;
    puts("cmd Clear");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_DrawGlyphRun(ID2D1CommandSink *This, D2D1_POINT_2F baseline_origin, const DWRITE_GLYPH_RUN *glyph_run, const DWRITE_GLYPH_RUN_DESCRIPTION *glyph_run_desc, ID2D1Brush *brush, DWRITE_MEASURING_MODE measuring_mode)
{
    (void)This;
    (void)baseline_origin;
    (void)glyph_run;
    (void)glyph_run_desc;
    (void)brush;
    (void)measuring_mode;
    puts("cmd DrawGlyphRun");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_DrawLine(ID2D1CommandSink *This, D2D1_POINT_2F p0, D2D1_POINT_2F p1, ID2D1Brush *brush, float stroke_width, ID2D1StrokeStyle *stroke_style)
{
    (void)This;
    (void)p0;
    (void)p1;
    (void)brush;
    (void)stroke_width;
    (void)stroke_style;
    puts("cmd DrawLine");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_DrawGeometry(ID2D1CommandSink *This, ID2D1Geometry *geometry, ID2D1Brush *brush, float stroke_width, ID2D1StrokeStyle *stroke_style)
{
    (void)This;
    (void)geometry;
    (void)brush;
    (void)stroke_width;
    (void)stroke_style;
    puts("cmd DrawGeometry");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_DrawRectangle(ID2D1CommandSink *This, const D2D1_RECT_F *rect, ID2D1Brush *brush, float stroke_width, ID2D1StrokeStyle *stroke_style)
{
    (void)This;
    (void)rect;
    (void)brush;
    (void)stroke_width;
    (void)stroke_style;
    puts("cmd DrawRectangle");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_DrawBitmap(ID2D1CommandSink *This, ID2D1Bitmap *bitmap, const D2D1_RECT_F *dst_rect, float opacity, D2D1_INTERPOLATION_MODE interpolation_mode, const D2D1_RECT_F *src_rect, const D2D1_MATRIX_4X4_F *perspective_transform)
{
    (void)This;
    (void)bitmap;
    (void)dst_rect;
    (void)opacity;
    (void)interpolation_mode;
    (void)src_rect;
    (void)perspective_transform;
    puts("cmd DrawBitmap");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_DrawImage(ID2D1CommandSink *This, ID2D1Image *image, const D2D1_POINT_2F *target_offset, const D2D1_RECT_F *image_rect, D2D1_INTERPOLATION_MODE interpolation_mode, D2D1_COMPOSITE_MODE composite_mode)
{
    (void)This;
    (void)image;
    (void)target_offset;
    (void)image_rect;
    (void)interpolation_mode;
    (void)composite_mode;
    puts("cmd DrawImage");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_DrawGdiMetafile(ID2D1CommandSink *This, ID2D1GdiMetafile *metafile, const D2D1_POINT_2F *target_offset)
{
    (void)This;
    (void)metafile;
    (void)target_offset;
    puts("cmd DrawGdiMetafile");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_FillMesh(ID2D1CommandSink *This, ID2D1Mesh *mesh, ID2D1Brush *brush)
{
    (void)This;
    (void)mesh;
    (void)brush;
    puts("cmd FillMesh");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_FillOpacityMask(ID2D1CommandSink *This, ID2D1Bitmap *bitmap, ID2D1Brush *brush, const D2D1_RECT_F *dst_rect, const D2D1_RECT_F *src_rect)
{
    (void)This;
    (void)bitmap;
    (void)brush;
    (void)dst_rect;
    (void)src_rect;
    puts("cmd FillOpacityMask");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_FillGeometry(ID2D1CommandSink *This, ID2D1Geometry *geometry, ID2D1Brush *brush, ID2D1Brush *opacity_brush)
{
    (void)This;
    (void)geometry;
    (void)brush;
    (void)opacity_brush;
    puts("cmd FillGeometry");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_FillRectangle(ID2D1CommandSink *This, const D2D1_RECT_F *rect, ID2D1Brush *brush)
{
    (void)This;
    (void)rect;
    (void)brush;
    ID2D1SolidColorBrush *solid;
    D2D1_COLOR_F c = {0};
    if (SUCCEEDED(ID2D1Brush_QueryInterface(brush,&IID_ID2D1SolidColorBrush,(void **)&solid)))
    {
        c = ID2D1SolidColorBrush_GetColor(solid);
        ID2D1SolidColorBrush_Release(solid);
    }
    printf("cmd FillRectangle %g %g %g %g %g %g %g %g\n",rect->left,rect->top,rect->right,rect->bottom,c.r,c.g,c.b,c.a);
    if (replay) { ID2D1DeviceContext_FillRectangle(replay,rect,brush); }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_PushAxisAlignedClip(ID2D1CommandSink *This, const D2D1_RECT_F *clip_rect, D2D1_ANTIALIAS_MODE antialias_mode)
{
    (void)This;
    (void)clip_rect;
    (void)antialias_mode;
    puts("cmd PushAxisAlignedClip");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_PushLayer(ID2D1CommandSink *This, const D2D1_LAYER_PARAMETERS1 *layer_parameters, ID2D1Layer *layer)
{
    (void)This;
    (void)layer_parameters;
    (void)layer;
    puts("cmd PushLayer");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_PopAxisAlignedClip(ID2D1CommandSink *This)
{
    (void)This;
    puts("cmd PopAxisAlignedClip");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_PopLayer(ID2D1CommandSink *This)
{
    (void)This;
    puts("cmd PopLayer");
    return S_OK;
}
static const ID2D1CommandSinkVtbl sink_vtbl = {
    sink_QueryInterface,
    sink_AddRef,
    sink_Release,
    sink_BeginDraw,
    sink_EndDraw,
    sink_SetAntialiasMode,
    sink_SetTags,
    sink_SetTextAntialiasMode,
    sink_SetTextRenderingParams,
    sink_SetTransform,
    sink_SetPrimitiveBlend,
    sink_SetUnitMode,
    sink_Clear,
    sink_DrawGlyphRun,
    sink_DrawLine,
    sink_DrawGeometry,
    sink_DrawRectangle,
    sink_DrawBitmap,
    sink_DrawImage,
    sink_DrawGdiMetafile,
    sink_FillMesh,
    sink_FillOpacityMask,
    sink_FillGeometry,
    sink_FillRectangle,
    sink_PushAxisAlignedClip,
    sink_PushLayer,
    sink_PopAxisAlignedClip,
    sink_PopLayer
};

int main(void)
{
    static const char *sequences[] = {
        "CSC S", "LCS", "LBFECS", "bBLFbCSE", "NBLFNCSE", "LFCS",
        "LBFEBFECS", "bBLFbXLFbCSE", "LBF CSES", "LBF CFES",
        "LBFE CBFES", "bBLFNLFNCSE", "LBXFECS", "LSBFSECS", "BLFECS",
        "LBFE X BFECS", "LX BFECS", "LBFE FCS", "LBF C LBFES", "LBFE CLS",
        "bBLFbNF CSE", "bBLFYZFbCSE", "LBFYFECS",
    };
    ID3D11Device *d3d = NULL;
    IDXGIDevice *dxgi = NULL;
    ID2D1Factory1 *factory = NULL;
    ID2D1Device *device = NULL;
    ID2D1DeviceContext *dc = NULL;
    ID2D1CommandList *list = NULL;
    ID2D1Image *bound;
    ID2D1Bitmap1 *target = NULL, *replay_target = NULL, *readback = NULL;
    ID2D1SolidColorBrush *brush = NULL;
    ID2D1CommandSink sink = {&sink_vtbl};
    D2D1_BITMAP_PROPERTIES1 props = {{DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},96,96,D2D1_BITMAP_OPTIONS_TARGET,NULL};
    D2D1_SIZE_U size = {8,8};
    D2D1_COLOR_F color = {0.25,0.5,0.75,0.5}, red = {1,0,0,1}, clear = {0};
    D2D1_MAPPED_RECT mapped;
    D2D1_MATRIX_3X2_F identity = {.m11=1,.m22=1}, translated = {.m11=1,.m22=1,.dx=2,.dy=1};
    D2D1_RECT_F rect = {1,1,3,3};
    D2D1_MATRIX_3X2_F transform = {.m11=1,.m22=1,.dx=5,.dy=7};
    D2D1_TAG tag1,tag2;
    HRESULT hr = S_OK;
    unsigned int i,j,x,y;
    D3D_DRIVER_TYPE driver = getenv("D2D1_TEST_WARP") ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE;
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
#define REQUIRE(call) do { hr = (call); if (FAILED(hr)) {printf("FAILED %s %08lx\n",#call,hr); goto done;} } while(0)
    REQUIRE(D3D11CreateDevice(NULL,driver,NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,NULL,0,D3D11_SDK_VERSION,&d3d,NULL,NULL));
    REQUIRE(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    REQUIRE(ID2D1Factory1_CreateDevice(factory,dxgi,&device));
    REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&replay));
    REQUIRE(ID2D1DeviceContext_CreateBitmap(replay,size,NULL,0,&props,&replay_target));
    props.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(replay,size,NULL,0,&props,&readback));
    props.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
    ID2D1DeviceContext_SetTarget(replay,(ID2D1Image *)replay_target);
    for (i = 0; i < sizeof(sequences)/sizeof(*sequences); ++i)
    {
        REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&dc));
        REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,NULL,0,&props,&target));
        REQUIRE(ID2D1DeviceContext_CreateCommandList(dc,&list));
        REQUIRE(ID2D1DeviceContext_CreateSolidColorBrush(dc,&color,NULL,&brush));
        ID2D1DeviceContext_SetAntialiasMode(dc,D2D1_ANTIALIAS_MODE_ALIASED);
        ID2D1DeviceContext_SetTags(dc,10,20);
        printf("case %u %s\n",i,sequences[i]);
        for (j = 0; sequences[i][j]; ++j)
        {
            switch (sequences[i][j])
            {
                case 'B': ID2D1DeviceContext_BeginDraw(dc); break;
                case 'b': ID2D1DeviceContext_SetTarget(dc,(ID2D1Image *)target); break;
                case 'L': ID2D1DeviceContext_SetTarget(dc,(ID2D1Image *)list); break;
                case 'N': ID2D1DeviceContext_SetTarget(dc,NULL); break;
                case 'X': ID2D1DeviceContext_SetTransform(dc,&transform); break;
                case 'Y': ID2D1SolidColorBrush_SetColor(brush,&red); break;
                case 'Z': ID2D1DeviceContext_SetTransform(dc,&translated); break;
                case 'F': ID2D1DeviceContext_FillRectangle(dc,&rect,(ID2D1Brush *)brush); break;
                case 'E':
                    tag1 = tag2 = 0xdeadbeef;
                    hr = ID2D1DeviceContext_EndDraw(dc,&tag1,&tag2);
                    printf("end %08lx %llu %llu\n",hr,tag1,tag2); break;
                case 'C':
                    printf("close %08lx\n",ID2D1CommandList_Close(list));
                    ID2D1DeviceContext_GetTarget(dc,&bound);
                    printf("target %u\n",bound == (ID2D1Image *)list ? 1 : bound == (ID2D1Image *)target ? 2 : bound ? 3 : 0);
                    if (bound) ID2D1Image_Release(bound);
                    break;
                case 'S':
                    ID2D1DeviceContext_BeginDraw(replay);
                    ID2D1DeviceContext_SetTransform(replay,&identity);
                    ID2D1DeviceContext_Clear(replay,&clear);
                    REQUIRE(ID2D1DeviceContext_EndDraw(replay,NULL,NULL));
                    puts("stream_begin");
                    hr = ID2D1CommandList_Stream(list,&sink);
                    printf("stream_end %08lx\n",hr);
                    REQUIRE(ID2D1Bitmap1_CopyFromBitmap(readback,NULL,(ID2D1Bitmap *)replay_target,NULL));
                    REQUIRE(ID2D1Bitmap1_Map(readback,D2D1_MAP_OPTIONS_READ,&mapped));
                    printf("pixels %u %u",i,j);
                    for (y = 0; y < size.height; ++y) for (x = 0; x < size.width; ++x)
                    {
                        const float *v = (float *)(mapped.bits+y*mapped.pitch)+4*x;
                        printf(" %.9g %.9g %.9g %.9g",v[0],v[1],v[2],v[3]);
                    }
                    puts("");
                    ID2D1Bitmap1_Unmap(readback);
                    break;
            }
        }
        ID2D1DeviceContext_SetTarget(dc,NULL);
        ID2D1SolidColorBrush_Release(brush); brush = NULL;
        ID2D1CommandList_Release(list); list = NULL;
        ID2D1Bitmap1_Release(target); target = NULL;
        ID2D1DeviceContext_Release(dc); dc = NULL;
    }
done:
    if (replay) ID2D1DeviceContext_SetTarget(replay,NULL);
    if (readback) ID2D1Bitmap1_Release(readback);
    if (replay_target) ID2D1Bitmap1_Release(replay_target);
    if (replay) ID2D1DeviceContext_Release(replay);
    if (dc) ID2D1DeviceContext_SetTarget(dc,NULL);
    if (brush) ID2D1SolidColorBrush_Release(brush);
    if (list) ID2D1CommandList_Release(list);
    if (target) ID2D1Bitmap1_Release(target);
    if (dc) ID2D1DeviceContext_Release(dc);
    if (device) ID2D1Device_Release(device);
    if (factory) ID2D1Factory1_Release(factory);
    if (dxgi) IDXGIDevice_Release(dxgi);
    if (d3d) ID3D11Device_Release(d3d);
    CoUninitialize();
    return 0;
}
