/* Command payload lifetime across recording-buffer growth.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <string.h>
#include "d2d1_1.h"
#include "dwrite.h"
#include "d3d11.h"
#include "wine/test.h"
#include "effect_test.h"
static unsigned int glyphs,bitmaps,images,masks;
static UINT16 indices[3],clusters[]={0,1,2};
static const float advances[]={3,4,5};
static const DWRITE_GLYPH_OFFSET offsets[]={{1,2},{3,4},{5,6}};
static const D2D1_RECT_F destination={4,5,14,15},source={0,0,1,1};
static const D2D1_POINT_2F origin={7,9};
static const D2D1_MATRIX_4X4_F perspective={.m={{1,0,0,0},{0,1,0,0},{0,0,1,0},{2,3,0,1}}};
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
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_EndDraw(ID2D1CommandSink *This)
{
    (void)This;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_SetAntialiasMode(ID2D1CommandSink *This, D2D1_ANTIALIAS_MODE antialias_mode)
{
    (void)This;
    (void)antialias_mode;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_SetTags(ID2D1CommandSink *This, D2D1_TAG tag1, D2D1_TAG tag2)
{
    (void)This;
    (void)tag1;
    (void)tag2;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_SetTextAntialiasMode(ID2D1CommandSink *This, D2D1_TEXT_ANTIALIAS_MODE antialias_mode)
{
    (void)This;
    (void)antialias_mode;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_SetTextRenderingParams(ID2D1CommandSink *This, IDWriteRenderingParams *text_rendering_params)
{
    (void)This;
    (void)text_rendering_params;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_SetTransform(ID2D1CommandSink *This, const D2D1_MATRIX_3X2_F *transform)
{
    (void)This;
    (void)transform;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_SetPrimitiveBlend(ID2D1CommandSink *This, D2D1_PRIMITIVE_BLEND primitive_blend)
{
    (void)This;
    (void)primitive_blend;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_SetUnitMode(ID2D1CommandSink *This, D2D1_UNIT_MODE unit_mode)
{
    (void)This;
    (void)unit_mode;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_Clear(ID2D1CommandSink *This, const D2D1_COLOR_F *color)
{
    (void)This;
    (void)color;
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
    ++glyphs;
    ok(glyph_run->glyphCount==3,"Unexpected glyph count.\n");
    ok(!memcmp(glyph_run->glyphIndices,indices,sizeof(indices)),"Glyph indices changed.\n");
    ok(!memcmp(glyph_run->glyphAdvances,advances,sizeof(advances)),"Glyph advances changed.\n");
    ok(!memcmp(glyph_run->glyphOffsets,offsets,sizeof(offsets)),"Glyph offsets changed.\n");
    ok(glyph_run_desc && glyph_run_desc->stringLength==3,"Missing glyph description.\n");
    if(glyph_run_desc)
    {
        ok(!wcscmp(glyph_run_desc->localeName,L"en-us"),"Locale changed.\n");
        ok(!memcmp(glyph_run_desc->string,L"ABC",6),"Text changed.\n");
        ok(!memcmp(glyph_run_desc->clusterMap,clusters,sizeof(clusters)),"Clusters changed.\n");
        ok(glyph_run_desc->textPosition==7,"Text position changed.\n");
    }
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
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_DrawGeometry(ID2D1CommandSink *This, ID2D1Geometry *geometry, ID2D1Brush *brush, float stroke_width, ID2D1StrokeStyle *stroke_style)
{
    (void)This;
    (void)geometry;
    (void)brush;
    (void)stroke_width;
    (void)stroke_style;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_DrawRectangle(ID2D1CommandSink *This, const D2D1_RECT_F *rect, ID2D1Brush *brush, float stroke_width, ID2D1StrokeStyle *stroke_style)
{
    (void)This;
    (void)rect;
    (void)brush;
    (void)stroke_width;
    (void)stroke_style;
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
    ++bitmaps;
    ok(dst_rect && !memcmp(dst_rect,&destination,sizeof(destination)),"Bitmap destination changed.\n");
    ok(src_rect && !memcmp(src_rect,&source,sizeof(source)),"Bitmap source changed.\n");
    ok(perspective_transform && !memcmp(perspective_transform,&perspective,sizeof(perspective)),"Perspective changed.\n");
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
    ++images;
    ok(target_offset && !memcmp(target_offset,&origin,sizeof(origin)),"Image offset changed.\n");
    ok(image_rect && !memcmp(image_rect,&source,sizeof(source)),"Image rectangle changed.\n");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_DrawGdiMetafile(ID2D1CommandSink *This, ID2D1GdiMetafile *metafile, const D2D1_POINT_2F *target_offset)
{
    (void)This;
    (void)metafile;
    (void)target_offset;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_FillMesh(ID2D1CommandSink *This, ID2D1Mesh *mesh, ID2D1Brush *brush)
{
    (void)This;
    (void)mesh;
    (void)brush;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_FillOpacityMask(ID2D1CommandSink *This, ID2D1Bitmap *bitmap, ID2D1Brush *brush, const D2D1_RECT_F *dst_rect, const D2D1_RECT_F *src_rect)
{
    (void)This;
    (void)bitmap;
    (void)brush;
    (void)dst_rect;
    (void)src_rect;
    ++masks;
    ok(dst_rect && !memcmp(dst_rect,&destination,sizeof(destination)),"Mask destination changed.\n");
    ok(src_rect && !memcmp(src_rect,&source,sizeof(source)),"Mask source changed.\n");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_FillGeometry(ID2D1CommandSink *This, ID2D1Geometry *geometry, ID2D1Brush *brush, ID2D1Brush *opacity_brush)
{
    (void)This;
    (void)geometry;
    (void)brush;
    (void)opacity_brush;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_FillRectangle(ID2D1CommandSink *This, const D2D1_RECT_F *rect, ID2D1Brush *brush)
{
    (void)This;
    (void)rect;
    (void)brush;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_PushAxisAlignedClip(ID2D1CommandSink *This, const D2D1_RECT_F *clip_rect, D2D1_ANTIALIAS_MODE antialias_mode)
{
    (void)This;
    (void)clip_rect;
    (void)antialias_mode;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_PushLayer(ID2D1CommandSink *This, const D2D1_LAYER_PARAMETERS1 *layer_parameters, ID2D1Layer *layer)
{
    (void)This;
    (void)layer_parameters;
    (void)layer;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_PopAxisAlignedClip(ID2D1CommandSink *This)
{
    (void)This;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_PopLayer(ID2D1CommandSink *This)
{
    (void)This;
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


#define REQUIRE(call) do {hr=(call);ok(hr==S_OK,"%s returned %#lx.\n",#call,hr);if(FAILED(hr))return;}while(0)
static void test_payloads(void)
{
    ID3D11Device *d3d;IDXGIDevice *dxgi;ID2D1Factory1 *factory;ID2D1Device *device;ID2D1DeviceContext *dc;
    ID2D1CommandList *list;ID2D1SolidColorBrush *brush;ID2D1Bitmap1 *bitmap,*mask;
    IDWriteFactory *dw;IDWriteFontCollection *fonts;IDWriteFontFamily *family;IDWriteFont *font;IDWriteFontFace *face;
    DWRITE_GLYPH_RUN run={0};DWRITE_GLYPH_RUN_DESCRIPTION desc={L"en-us",L"ABC",3,clusters,7};
    D2D1_BITMAP_PROPERTIES1 props={{DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED},96,96,0,NULL};
    D2D1_COLOR_F color={.25,.5,.75,.5};UINT32 pixel=0xffffffff,characters[]={'A','B','C'};
    ID2D1CommandSink sink={&sink_vtbl};void *allocations[128];UINT i;HRESULT hr;
    REQUIRE(D3D11CreateDevice(NULL,effect_test_driver(),NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,NULL,0,D3D11_SDK_VERSION,&d3d,NULL,NULL));
    REQUIRE(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    REQUIRE(ID2D1Factory1_CreateDevice(factory,dxgi,&device));REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&dc));
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,(D2D1_SIZE_U){1,1},&pixel,4,&props,&bitmap));
    props.pixelFormat.format=DXGI_FORMAT_A8_UNORM;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,(D2D1_SIZE_U){1,1},&pixel,1,&props,&mask));
    REQUIRE(ID2D1DeviceContext_CreateSolidColorBrush(dc,&color,NULL,&brush));
    REQUIRE(DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED,&IID_IDWriteFactory,(IUnknown **)&dw));
    REQUIRE(IDWriteFactory_GetSystemFontCollection(dw,&fonts,FALSE));
    REQUIRE(IDWriteFontCollection_GetFontFamily(fonts,0,&family));REQUIRE(IDWriteFontFamily_GetFirstMatchingFont(family,400,5,0,&font));
    REQUIRE(IDWriteFont_CreateFontFace(font,&face));REQUIRE(IDWriteFontFace_GetGlyphIndices(face,characters,3,indices));
    run.fontFace=face;run.fontEmSize=12;run.glyphCount=3;run.glyphIndices=indices;run.glyphAdvances=advances;run.glyphOffsets=offsets;
    REQUIRE(ID2D1DeviceContext_CreateCommandList(dc,&list));ID2D1DeviceContext_SetTarget(dc,(ID2D1Image *)list);
    ID2D1DeviceContext_SetAntialiasMode(dc,D2D1_ANTIALIAS_MODE_ALIASED);ID2D1DeviceContext_BeginDraw(dc);
    for(i=0;i<128;++i)
    {
        ID2D1DeviceContext_DrawGlyphRun(dc,origin,&run,&desc,(ID2D1Brush *)brush,DWRITE_MEASURING_MODE_NATURAL);
        ID2D1DeviceContext_DrawBitmap(dc,(ID2D1Bitmap *)bitmap,&destination,.5,D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,&source,&perspective);
        ID2D1DeviceContext_DrawImage(dc,(ID2D1Image *)bitmap,&origin,&source,D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,D2D1_COMPOSITE_MODE_SOURCE_OVER);
        ID2D1DeviceContext_FillOpacityMask(dc,(ID2D1Bitmap *)mask,(ID2D1Brush *)brush,&destination,&source);
        allocations[i]=HeapAlloc(GetProcessHeap(),0,512+(i%16)*256);
        if(allocations[i])memset(allocations[i],0x5a,512+(i%16)*256);
    }
    REQUIRE(ID2D1DeviceContext_EndDraw(dc,NULL,NULL));ID2D1DeviceContext_SetTarget(dc,NULL);REQUIRE(ID2D1CommandList_Close(list));
    REQUIRE(ID2D1CommandList_Stream(list,&sink));
    ok(glyphs==128 && bitmaps==128 && images==128 && masks==128,"Incomplete stream: %u %u %u %u.\n",glyphs,bitmaps,images,masks);
    for(i=0;i<128;++i)HeapFree(GetProcessHeap(),0,allocations[i]);
    ID2D1CommandList_Release(list);IDWriteFontFace_Release(face);IDWriteFont_Release(font);IDWriteFontFamily_Release(family);
    IDWriteFontCollection_Release(fonts);IDWriteFactory_Release(dw);ID2D1SolidColorBrush_Release(brush);
    ID2D1Bitmap1_Release(mask);ID2D1Bitmap1_Release(bitmap);ID2D1DeviceContext_Release(dc);ID2D1Device_Release(device);
    ID2D1Factory1_Release(factory);IDXGIDevice_Release(dxgi);ID3D11Device_Release(d3d);
}
START_TEST(command_data)
{
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    test_payloads();
    CoUninitialize();
}
