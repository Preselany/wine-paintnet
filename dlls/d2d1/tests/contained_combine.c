/* Contained Direct2D geometry combinations measured on native Windows/WARP.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <math.h>
#include "wine/test.h"
#include "d2d1_3.h"
#include "d3d11.h"
#include "effect_test.h"
#define REQUIRE(call) do { hr=(call); ok(hr == S_OK,"%s returned %#lx.\n",#call,hr); if (FAILED(hr)) return; } while(0)

static const struct {float bounds[4], area; unsigned short rows[12];} expected[] = {
    {{0.0f,0.0f,11.0f,11.0f},121.0f,{0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x0}},
    {{2.0f,2.0f,9.0f,9.0f},49.0f,{0x0,0x0,0x1fc,0x1fc,0x1fc,0x1fc,0x1fc,0x1fc,0x1fc,0x0,0x0,0x0}},
    {{0.0f,0.0f,11.0f,11.0f},72.0f,{0x7ff,0x7ff,0x603,0x603,0x603,0x603,0x603,0x603,0x603,0x7ff,0x7ff,0x0}},
    {{0.0f,0.0f,11.0f,11.0f},72.0f,{0x7ff,0x7ff,0x603,0x603,0x603,0x603,0x603,0x603,0x603,0x7ff,0x7ff,0x0}},
    {{0.0f,0.0f,11.0f,11.0f},121.0f,{0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x0}},
    {{3.0f,2.0f,6.5f,5.5f},12.25f,{0x0,0x0,0x38,0x38,0x38,0x0,0x0,0x0,0x0,0x0,0x0,0x0}},
    {{0.0f,0.0f,11.0f,11.0f},108.75f,{0x7ff,0x7ff,0x7c7,0x7c7,0x7c7,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x0}},
    {{0.0f,0.0f,11.0f,11.0f},108.75f,{0x7ff,0x7ff,0x7c7,0x7c7,0x7c7,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x0}},
    {{0.0f,0.0f,11.0f,11.0f},121.0f,{0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x0}},
    {{2.0f,2.0f,9.0f,9.0f},37.0f,{0x0,0x0,0x1fc,0x1fc,0x1fc,0x3c,0x3c,0x3c,0x3c,0x0,0x0,0x0}},
    {{0.0f,0.0f,11.0f,11.0f},84.0f,{0x7ff,0x7ff,0x603,0x603,0x603,0x7c3,0x7c3,0x7c3,0x7c3,0x7ff,0x7ff,0x0}},
    {{0.0f,0.0f,11.0f,11.0f},84.0f,{0x7ff,0x7ff,0x603,0x603,0x603,0x7c3,0x7c3,0x7c3,0x7c3,0x7ff,0x7ff,0x0}},
    {{0.0f,0.0f,11.0f,11.0f},121.0f,{0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x0}},
    {{3.0f,2.0f,6.5f,5.5f},9.25f,{0x0,0x0,0x38,0x18,0x18,0x0,0x0,0x0,0x0,0x0,0x0,0x0}},
    {{0.0f,0.0f,11.0f,11.0f},111.75f,{0x7ff,0x7ff,0x7c7,0x7e7,0x7e7,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x0}},
    {{0.0f,0.0f,11.0f,11.0f},111.75f,{0x7ff,0x7ff,0x7c7,0x7e7,0x7e7,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x0}},
    {{0.0f,0.0f,11.0f,11.0f},121.0f,{0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x0}},
    {{2.0f,3.0f,8.0f,7.0f},17.6718311f,{0x0,0x0,0x0,0x78,0xfc,0xfc,0x78,0x0,0x0,0x0,0x0,0x0}},
    {{0.0f,0.0f,11.0f,11.0f},103.328171f,{0x7ff,0x7ff,0x7ff,0x787,0x703,0x703,0x787,0x7ff,0x7ff,0x7ff,0x7ff,0x0}},
    {{0.0f,0.0f,11.0f,11.0f},103.328171f,{0x7ff,0x7ff,0x7ff,0x787,0x703,0x703,0x787,0x7ff,0x7ff,0x7ff,0x7ff,0x0}},
    {{0.0f,0.0f,11.0f,11.0f},121.0f,{0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x0}},
    {{3.0f,2.5f,6.0f,4.5f},4.2426405f,{0x0,0x0,0x0,0x38,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0}},
    {{0.0f,0.0f,11.0f,11.0f},116.757362f,{0x7ff,0x7ff,0x7ff,0x7c7,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x0}},
    {{0.0f,0.0f,11.0f,11.0f},116.757362f,{0x7ff,0x7ff,0x7ff,0x7c7,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x0}},
};

START_TEST(contained_combine)
{
    ID3D11Device *d3d;
    IDXGIDevice *dxgi;
    ID2D1Factory1 *factory;
    ID2D1Device *device;
    ID2D1DeviceContext *dc;
    ID2D1RectangleGeometry *rectangle, *inner_rectangle;
    ID2D1EllipseGeometry *ellipse;
    ID2D1PathGeometry *path, *result;
    ID2D1GeometrySink *sink;
    ID2D1Geometry *input;
    ID2D1SolidColorBrush *brush;
    ID2D1Bitmap1 *target,*readback;
    D2D1_SIZE_U size = {12,12};
    D2D1_BITMAP_PROPERTIES1 desc = {{DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},
            96,96,D2D1_BITMAP_OPTIONS_TARGET,NULL};
    const D2D1_RECT_F outer = {0,0,11,11}, inner = {2,2,9,9};
    const D2D1_POINT_2F points[] = {{2,2},{9,2},{9,5},{6,5},{6,9},{2,9}};
    const D2D1_ELLIPSE ellipse_desc = {{5,5},3,2};
    const D2D1_COLOR_F color = {.25,.5,.75,1}, clear = {0};
    const D2D1_MATRIX_3X2_F transform = {.m11=.5,.m22=.5,.dx=2,.dy=1};
    D2D1_MAPPED_RECT map;
    D2D1_RECT_F bounds;
    float area;
    HRESULT hr, close_hr;
    unsigned int shape,mode,transformed,x,y;
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    REQUIRE(D3D11CreateDevice(NULL,effect_test_driver(),
            NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,NULL,0,D3D11_SDK_VERSION,&d3d,NULL,NULL));
    REQUIRE(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    REQUIRE(ID2D1Factory1_CreateDevice(factory,dxgi,&device));
    REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&dc));
    REQUIRE(ID2D1DeviceContext_CreateSolidColorBrush(dc,&color,NULL,&brush));
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,NULL,0,&desc,&target));
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,NULL,0,&desc,&readback));
    REQUIRE(ID2D1Factory1_CreateRectangleGeometry(factory,&outer,&rectangle));
    REQUIRE(ID2D1Factory1_CreateRectangleGeometry(factory,&inner,&inner_rectangle));
    REQUIRE(ID2D1Factory1_CreateEllipseGeometry(factory,&ellipse_desc,&ellipse));
    REQUIRE(ID2D1Factory_CreatePathGeometry((ID2D1Factory *)factory,&path));
    REQUIRE(ID2D1PathGeometry_Open(path,&sink));
    ID2D1GeometrySink_SetFillMode(sink,D2D1_FILL_MODE_ALTERNATE);
    ID2D1GeometrySink_BeginFigure(sink,points[0],D2D1_FIGURE_BEGIN_FILLED);
    ID2D1GeometrySink_AddLines(sink,&points[1],5);
    ID2D1GeometrySink_EndFigure(sink,D2D1_FIGURE_END_CLOSED);
    REQUIRE(ID2D1GeometrySink_Close(sink));
    ID2D1GeometrySink_Release(sink);
    ID2D1DeviceContext_SetTarget(dc,(ID2D1Image *)target);
    ID2D1DeviceContext_SetAntialiasMode(dc,D2D1_ANTIALIAS_MODE_ALIASED);
    for(shape=0;shape<3;++shape)for(transformed=0;transformed<2;++transformed)for(mode=0;mode<4;++mode)
    {
        unsigned int index = (shape*2+transformed)*4+mode;
        winetest_push_context("shape %u transform %u mode %u",shape,transformed,mode);
        input = shape == 0 ? (ID2D1Geometry *)inner_rectangle : shape == 1 ? (ID2D1Geometry *)path : (ID2D1Geometry *)ellipse;
        REQUIRE(ID2D1Factory_CreatePathGeometry((ID2D1Factory *)factory,&result));
        REQUIRE(ID2D1PathGeometry_Open(result,&sink));
        hr=ID2D1RectangleGeometry_CombineWithGeometry(rectangle,input,mode,transformed ? &transform : NULL,
                .25f,(ID2D1SimplifiedGeometrySink *)sink);
        ok(hr == S_OK,"Combine returned %#lx.\n",hr);
        close_hr=ID2D1GeometrySink_Close(sink); ok(close_hr == S_OK,"Close returned %#lx.\n",close_hr);
        ID2D1GeometrySink_Release(sink);
        if (SUCCEEDED(hr) && SUCCEEDED(close_hr))
        {
            hr=ID2D1PathGeometry_GetBounds(result,NULL,&bounds);
            ok(hr == S_OK,"GetBounds returned %#lx.\n",hr);
            ok(fabsf(bounds.left-expected[index].bounds[0])<1e-6f
                    && fabsf(bounds.top-expected[index].bounds[1])<1e-6f
                    && fabsf(bounds.right-expected[index].bounds[2])<1e-6f
                    && fabsf(bounds.bottom-expected[index].bounds[3])<1e-6f,"Unexpected bounds.\n");
            hr=ID2D1PathGeometry_ComputeArea(result,NULL,.25f,&area);
            ok(hr == S_OK,"ComputeArea returned %#lx.\n",hr);
            /* Curve flattening differs despite matching sampled pixels. */
            todo_wine_if(shape == 2 && !transformed && mode != D2D1_COMBINE_MODE_UNION)
                ok(fabsf(area-expected[index].area)<1e-5f,"Area %.9g, expected %.9g.\n",area,expected[index].area);
            ID2D1DeviceContext_BeginDraw(dc);
            ID2D1DeviceContext_Clear(dc,&clear);
            ID2D1DeviceContext_FillGeometry(dc,(ID2D1Geometry *)result,(ID2D1Brush *)brush,NULL);
            hr=ID2D1DeviceContext_EndDraw(dc,NULL,NULL);ok(hr == S_OK,"EndDraw returned %#lx.\n",hr);
            if(SUCCEEDED(hr))
            {
                REQUIRE(ID2D1Bitmap1_CopyFromBitmap(readback,NULL,(ID2D1Bitmap *)target,NULL));
                REQUIRE(ID2D1Bitmap1_Map(readback,D2D1_MAP_OPTIONS_READ,&map));
                for(y=0;y<size.height;++y)for(x=0;x<size.width*4;++x)
                {
                    static const float rgba[] = {.25f,.5f,.75f,1};
                    float actual, value = (expected[index].rows[y] & (1u << (x/4))) ? rgba[x%4] : 0;
                    memcpy(&actual,map.bits+y*map.pitch+x*4,4);
                    ok(actual == value,"Pixel %u,%u channel %u: %.9g, expected %.9g.\n",x/4,y,x%4,actual,value);
                }
                REQUIRE(ID2D1Bitmap1_Unmap(readback));
            }
        }
        ID2D1PathGeometry_Release(result);
        winetest_pop_context();
    }
    ID2D1DeviceContext_SetTarget(dc,NULL);
    ID2D1RectangleGeometry_Release(rectangle);ID2D1RectangleGeometry_Release(inner_rectangle);
    ID2D1EllipseGeometry_Release(ellipse);ID2D1PathGeometry_Release(path);
    ID2D1SolidColorBrush_Release(brush);ID2D1Bitmap1_Release(readback);ID2D1Bitmap1_Release(target);
    ID2D1DeviceContext_Release(dc);ID2D1Device_Release(device);ID2D1Factory1_Release(factory);
    IDXGIDevice_Release(dxgi);ID3D11Device_Release(d3d);CoUninitialize();
}
