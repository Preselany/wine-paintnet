/* Native Direct2D path combination regressions.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <math.h>
#include <float.h>
#include "wine/test.h"
#include "d2d1_3.h"
#include "d3d11.h"
#include "effect_test.h"
#define REQUIRE(call) do { hr=(call); ok(hr == S_OK,"%s returned %#lx.\n",#call,hr); if (FAILED(hr)) return; } while(0)
static const struct {float bounds[4], area; unsigned short rows[12];} expected[2][24] = {
    {
        {{0.0f,0.0f,11.0f,11.0f},121.0f,{0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x0}},
        {{2.0f,2.0f,9.0f,9.0f},49.0f,{0x0,0x0,0x1fc,0x1fc,0x1fc,0x1fc,0x1fc,0x1fc,0x1fc,0x0,0x0,0x0}},
        {{0.0f,0.0f,11.0f,11.0f},72.0f,{0x7ff,0x7ff,0x603,0x603,0x603,0x603,0x603,0x603,0x603,0x7ff,0x7ff,0x0}},
        {{0.0f,0.0f,11.0f,11.0f},72.0f,{0x7ff,0x7ff,0x603,0x603,0x603,0x603,0x603,0x603,0x603,0x7ff,0x7ff,0x0}},
        {{0.0f,0.0f,15.0f,14.25f},153.1875f,{0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0xfff,0xfff,0xfff,0xfff,0xfff,0xfe0}},
        {{5.25f,5.5f,11.0f,11.0f},22.9375f,{0x0,0x0,0x0,0x0,0x0,0x0,0x780,0x780,0x7c0,0x7c0,0x7e0,0x0}},
        {{0.0f,0.0f,15.0f,14.25f},130.25f,{0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x87f,0x87f,0x83f,0x83f,0x81f,0xfe0}},
        {{0.0f,0.0f,11.0f,11.0f},98.0625f,{0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7f,0x7f,0x3f,0x3f,0x1f,0x0}},
        {{0.0f,0.0f,11.0f,11.0f},121.0f,{0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x0}},
        {{2.0f,2.0f,9.0f,9.0f},37.0f,{0x0,0x0,0x1fc,0x1fc,0x1fc,0x3c,0x3c,0x3c,0x3c,0x0,0x0,0x0}},
        {{0.0f,0.0f,11.0f,11.0f},84.0f,{0x7ff,0x7ff,0x603,0x603,0x603,0x7c3,0x7c3,0x7c3,0x7c3,0x7ff,0x7ff,0x0}},
        {{0.0f,0.0f,11.0f,11.0f},84.0f,{0x7ff,0x7ff,0x603,0x603,0x603,0x7c3,0x7c3,0x7c3,0x7c3,0x7ff,0x7ff,0x0}},
        {{0.0f,0.0f,15.0f,13.5f},140.96875f,{0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0xfff,0xfff,0xfff,0xfff,0x7ff,0x1e0}},
        {{5.25f,5.5f,11.0f,11.0f},21.65625f,{0x0,0x0,0x0,0x0,0x0,0x0,0x780,0x780,0x7c0,0x7c0,0x3e0,0x0}},
        {{0.0f,0.0f,15.0f,13.5f},119.3125f,{0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x87f,0x87f,0x83f,0x83f,0x41f,0x1e0}},
        {{0.0f,0.0f,11.0f,11.0f},99.34375f,{0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7f,0x7f,0x3f,0x3f,0x41f,0x0}},
        {{0.0f,0.0f,11.0f,11.0f},121.0f,{0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x0}},
        {{2.0f,3.0f,8.0f,7.0f},17.6718311f,{0x0,0x0,0x0,0x78,0xfc,0xfc,0x78,0x0,0x0,0x0,0x0,0x0}},
        {{0.0f,0.0f,11.0f,11.0f},103.328171f,{0x7ff,0x7ff,0x7ff,0x787,0x703,0x703,0x787,0x7ff,0x7ff,0x7ff,0x7ff,0x0}},
        {{0.0f,0.0f,11.0f,11.0f},103.328171f,{0x7ff,0x7ff,0x7ff,0x787,0x703,0x703,0x787,0x7ff,0x7ff,0x7ff,0x7ff,0x0}},
        {{0.0f,0.0f,12.6631336f,11.3865795f},125.767403f,{0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0xfff,0xfff,0xfff,0x0}},
        {{6.33686495f,7.11342001f,11.0f,11.0f},15.262742f,{0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x700,0x7c0,0x7c0,0x780,0x0}},
        {{0.0f,0.0f,12.6631336f,11.3865795f},110.504662f,{0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0xff,0x83f,0x83f,0x87f,0x0}},
        {{0.0f,0.0f,11.0f,11.0f},105.737259f,{0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0xff,0x3f,0x3f,0x7f,0x0}},
    },
    {
        {{0.0f,0.0f,11.0f,11.0f},121.0f,{0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x0}},
        {{2.0f,2.0f,9.0f,9.0f},24.0f,{0x0,0x0,0x1fc,0x104,0x104,0x104,0x104,0x104,0x1fc,0x0,0x0,0x0}},
        {{0.0f,0.0f,11.0f,11.0f},97.0f,{0x7ff,0x7ff,0x603,0x6fb,0x6fb,0x6fb,0x6fb,0x6fb,0x603,0x7ff,0x7ff,0x0}},
        {{0.0f,0.0f,11.0f,11.0f},72.0f,{0x7ff,0x7ff,0x603,0x603,0x603,0x603,0x603,0x603,0x603,0x7ff,0x7ff,0x0}},
        {{0.0f,0.0f,15.0f,14.25f},129.75f,{0x7ff,0x7ff,0x7ff,0x707,0x707,0x707,0xf87,0xf87,0xfff,0xfff,0xfff,0xfe0}},
        {{5.25f,5.5f,11.0f,11.0f},21.375f,{0x0,0x0,0x0,0x0,0x0,0x0,0x700,0x700,0x7c0,0x7c0,0x7e0,0x0}},
        {{0.0f,0.0f,15.0f,14.25f},108.375f,{0x7ff,0x7ff,0x7ff,0x707,0x707,0x707,0x887,0x887,0x83f,0x83f,0x81f,0xfe0}},
        {{0.0f,0.0f,11.0f,11.0f},74.625f,{0x7ff,0x7ff,0x7ff,0x707,0x707,0x707,0x7,0x7,0x3f,0x3f,0x1f,0x0}},
        {{0.0f,0.0f,11.0f,11.0f},115.0f,{0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x73f,0x73f,0x73f,0x7ff,0x7ff,0x7ff,0x0}},
        {{2.0f,2.0f,9.0f,9.0f},18.0f,{0x0,0x0,0x1fc,0x104,0x104,0x4,0x4,0x4,0x3c,0x0,0x0,0x0}},
        {{0.0f,0.0f,11.0f,11.0f},97.0f,{0x7ff,0x7ff,0x603,0x6fb,0x6fb,0x73b,0x73b,0x73b,0x7c3,0x7ff,0x7ff,0x0}},
        {{0.0f,0.0f,11.0f,11.0f},78.0f,{0x7ff,0x7ff,0x603,0x603,0x603,0x703,0x703,0x703,0x7c3,0x7ff,0x7ff,0x0}},
        {{0.0f,0.0f,15.0f,13.5f},117.53125f,{0x7ff,0x7ff,0x7ff,0x707,0x707,0x707,0xf87,0xf87,0xfff,0xfff,0x7ff,0x1e0}},
        {{5.25f,5.5f,11.0f,11.0f},20.09375f,{0x0,0x0,0x0,0x0,0x0,0x0,0x700,0x700,0x7c0,0x7c0,0x3e0,0x0}},
        {{0.0f,0.0f,15.0f,13.5f},97.4375f,{0x7ff,0x7ff,0x7ff,0x707,0x707,0x707,0x887,0x887,0x83f,0x83f,0x41f,0x1e0}},
        {{0.0f,0.0f,11.0f,11.0f},75.90625f,{0x7ff,0x7ff,0x7ff,0x707,0x707,0x707,0x7,0x7,0x3f,0x3f,0x41f,0x0}},
        {{0.0f,0.0f,11.0f,11.0f},111.731346f,{0x7ff,0x7ff,0x7ff,0x77f,0x7ff,0x7ff,0x77f,0x707,0x7ff,0x7ff,0x7ff,0x0}},
        {{2.0f,3.55228519f,3.0f,6.44771481f},1.94048607f,{0x0,0x0,0x0,0x0,0x4,0x4,0x0,0x0,0x0,0x0,0x0,0x0}},
        {{0.0f,0.0f,11.0f,11.0f},109.790863f,{0x7ff,0x7ff,0x7ff,0x77f,0x7fb,0x7fb,0x77f,0x707,0x7ff,0x7ff,0x7ff,0x0}},
        {{0.0f,0.0f,11.0f,11.0f},94.0595169f,{0x7ff,0x7ff,0x7ff,0x707,0x703,0x703,0x707,0x707,0x7ff,0x7ff,0x7ff,0x0}},
        {{0.0f,0.0f,12.6631336f,11.3865795f},101.169693f,{0x7ff,0x7ff,0x7ff,0x707,0x707,0x707,0x707,0x707,0xfff,0xfff,0xfff,0x0}},
        {{6.33686495f,7.11342096f,11.0f,11.0f},14.860446f,{0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x700,0x7c0,0x7c0,0x780,0x0}},
        {{0.0f,0.0f,12.6631336f,11.3865795f},86.3092499f,{0x7ff,0x7ff,0x7ff,0x707,0x707,0x707,0x707,0x7,0x83f,0x83f,0x87f,0x0}},
        {{0.0f,0.0f,11.0f,11.0f},81.1395569f,{0x7ff,0x7ff,0x7ff,0x707,0x707,0x707,0x707,0x7,0x3f,0x3f,0x7f,0x0}},
    },
};

static void test_path_combine(BOOL hole)
{
    ID3D11Device *d3d;
    IDXGIDevice *dxgi;
    ID2D1Factory1 *factory;
    ID2D1Device *device;
    ID2D1DeviceContext *dc;
    ID2D1RectangleGeometry *rectangle, *inner_rectangle;
    ID2D1EllipseGeometry *ellipse;
    ID2D1PathGeometry *path, *result, *primary;
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
    const D2D1_MATRIX_3X2_F transform = {.m11=1,.m12=.25,.m21=-.5,.m22=1,.dx=7,.dy=3};
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
    REQUIRE(ID2D1Factory_CreatePathGeometry((ID2D1Factory *)factory,&primary));
    REQUIRE(ID2D1PathGeometry_Open(primary,&sink));
    REQUIRE(ID2D1RectangleGeometry_Simplify(rectangle,D2D1_GEOMETRY_SIMPLIFICATION_OPTION_LINES,NULL,.25f,
            (ID2D1SimplifiedGeometrySink *)sink));
    if(hole)
    {
        const D2D1_POINT_2F hole[] = {{3,3},{8,3},{8,8},{3,8}};
        ID2D1GeometrySink_BeginFigure(sink,hole[0],D2D1_FIGURE_BEGIN_FILLED);
        ID2D1GeometrySink_AddLines(sink,&hole[1],3);
        ID2D1GeometrySink_EndFigure(sink,D2D1_FIGURE_END_CLOSED);
    }
    REQUIRE(ID2D1GeometrySink_Close(sink));
    ID2D1GeometrySink_Release(sink);
    ID2D1DeviceContext_SetTarget(dc,(ID2D1Image *)target);
    ID2D1DeviceContext_SetAntialiasMode(dc,D2D1_ANTIALIAS_MODE_ALIASED);
    for(shape=0;shape<3;++shape)for(transformed=0;transformed<2;++transformed)for(mode=0;mode<4;++mode)
    {
        unsigned int index = (shape*2+transformed)*4+mode;
        winetest_push_context("hole %u shape %u transform %u mode %u",hole,shape,transformed,mode);
        input = shape == 0 ? (ID2D1Geometry *)inner_rectangle : shape == 1 ? (ID2D1Geometry *)path : (ID2D1Geometry *)ellipse;
        REQUIRE(ID2D1Factory_CreatePathGeometry((ID2D1Factory *)factory,&result));
        REQUIRE(ID2D1PathGeometry_Open(result,&sink));
        hr=ID2D1PathGeometry_CombineWithGeometry(primary,input,mode,transformed ? &transform : NULL,
                .25f,(ID2D1SimplifiedGeometrySink *)sink);
        ok(hr == S_OK,"Combine returned %#lx.\n",hr);
        close_hr=ID2D1GeometrySink_Close(sink); ok(close_hr == S_OK,"Close returned %#lx.\n",close_hr);
        ID2D1GeometrySink_Release(sink);
        if (SUCCEEDED(hr) && SUCCEEDED(close_hr))
        {
            hr=ID2D1PathGeometry_GetBounds(result,NULL,&bounds);
            ok(hr == S_OK,"Bounds returned %#lx.\n",hr);
            /* Ellipse curve flattening still differs from native. */
            todo_wine_if(shape == 2 && ((transformed && mode != D2D1_COMBINE_MODE_EXCLUDE)
                    || (hole && !transformed && mode == D2D1_COMBINE_MODE_INTERSECT)))
                ok(fabsf(bounds.left-expected[hole][index].bounds[0])<1e-6f
                        && fabsf(bounds.top-expected[hole][index].bounds[1])<1e-6f
                        && fabsf(bounds.right-expected[hole][index].bounds[2])<1e-6f
                        && fabsf(bounds.bottom-expected[hole][index].bounds[3])<1e-6f,"Unexpected bounds.\n");
            hr=ID2D1PathGeometry_ComputeArea(result,NULL,.25f,&area);
            ok(hr == S_OK,"Area returned %#lx.\n",hr);
            todo_wine_if(shape == 2 && ((transformed && mode != D2D1_COMBINE_MODE_XOR)
                    || (!transformed && (hole || mode != D2D1_COMBINE_MODE_UNION))))
                ok(fabsf(area-expected[hole][index].area) <= 8*FLT_EPSILON*max(1,fabsf(area)),
                        "Area %.9g, expected %.9g.\n",area,expected[hole][index].area);
            ID2D1DeviceContext_BeginDraw(dc);
            ID2D1DeviceContext_Clear(dc,&clear);
            ID2D1DeviceContext_FillGeometry(dc,(ID2D1Geometry *)result,(ID2D1Brush *)brush,NULL);
            hr=ID2D1DeviceContext_EndDraw(dc,NULL,NULL);ok(hr == S_OK,"Draw returned %#lx.\n",hr);
            if(SUCCEEDED(hr))
            {
                REQUIRE(ID2D1Bitmap1_CopyFromBitmap(readback,NULL,(ID2D1Bitmap *)target,NULL));
                REQUIRE(ID2D1Bitmap1_Map(readback,D2D1_MAP_OPTIONS_READ,&map));
                for(y=0;y<size.height;++y)for(x=0;x<size.width*4;++x)
                {
                    static const float rgba[] = {.25f,.5f,.75f,1};
                    float actual, value = expected[hole][index].rows[y] & (1u << (x/4)) ? rgba[x%4] : 0;
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
    ID2D1PathGeometry_Release(primary);
    ID2D1EllipseGeometry_Release(ellipse);ID2D1PathGeometry_Release(path);
    ID2D1SolidColorBrush_Release(brush);ID2D1Bitmap1_Release(readback);ID2D1Bitmap1_Release(target);
    ID2D1DeviceContext_Release(dc);ID2D1Device_Release(device);ID2D1Factory1_Release(factory);
    IDXGIDevice_Release(dxgi);ID3D11Device_Release(d3d);CoUninitialize();
}

START_TEST(path_combine)
{
    test_path_combine(FALSE);
    test_path_combine(TRUE);
}
