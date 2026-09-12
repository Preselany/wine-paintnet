/* Native Direct2D default-stroke widening regressions.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <math.h>
#include <float.h>
#include <stdint.h>
#include "wine/test.h"
#include "d2d1_3.h"
#include "d3d11.h"
#include "effect_test.h"
#define REQUIRE(call) do { hr=(call); ok(hr == S_OK,"%s returned %#lx.\n",#call,hr); if (FAILED(hr)) return; } while(0)
static const struct {float bounds[4], area; uint64_t rows[36];} expected[2][24] = {
    {
        {{2.0f,2.0f,9.0f,9.0f},0.0f,{0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL}},
        {{1.5f,1.5f,9.5f,9.5f},28.0f,{0x0ULL,0x1feULL,0x102ULL,0x102ULL,0x102ULL,0x102ULL,0x102ULL,0x102ULL,0x1feULL,0x0ULL,0x0ULL,0x0ULL}},
        {{1.0f,1.0f,10.0f,10.0f},56.0f,{0x0ULL,0x3feULL,0x3feULL,0x306ULL,0x306ULL,0x306ULL,0x306ULL,0x306ULL,0x3feULL,0x3feULL,0x0ULL,0x0ULL}},
        {{-2.0f,-2.0f,13.0f,13.0f},225.0f,{0xfffULL,0xfffULL,0xfffULL,0xfffULL,0xfffULL,0xfffULL,0xfffULL,0xfffULL,0xfffULL,0xfffULL,0xfffULL,0xfffULL}},
        {{3.0f,2.0f,6.5f,5.5f},0.0f,{0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL}},
        {{2.75f,1.75f,6.75f,5.75f},7.0f,{0x0ULL,0x0ULL,0x40ULL,0x40ULL,0x40ULL,0x78ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL}},
        {{2.5f,1.5f,7.0f,6.0f},14.0f,{0x0ULL,0x7cULL,0x44ULL,0x44ULL,0x44ULL,0x7cULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL}},
        {{1.0f,0.0f,8.5f,7.5f},56.25f,{0xfeULL,0xfeULL,0xfeULL,0xfeULL,0xfeULL,0xfeULL,0xfeULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL}},
        {{INFINITY,INFINITY,3.40282347e+38f,3.40282347e+38f},0.0f,{0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL}},
        {{1.5f,1.5f,9.5f,9.5f},28.0f,{0x0ULL,0x1feULL,0x102ULL,0x102ULL,0x1e2ULL,0x22ULL,0x22ULL,0x22ULL,0x3eULL,0x0ULL,0x0ULL,0x0ULL}},
        {{1.0f,1.0f,10.0f,10.0f},56.0f,{0x0ULL,0x3feULL,0x3feULL,0x306ULL,0x3e6ULL,0x3e6ULL,0x66ULL,0x66ULL,0x7eULL,0x7eULL,0x0ULL,0x0ULL}},
        {{-2.0f,-2.0f,13.0f,13.0f},213.0f,{0xfffULL,0xfffULL,0xfffULL,0xfffULL,0xfffULL,0xfffULL,0xfffULL,0xfffULL,0xfffULL,0x3ffULL,0x3ffULL,0x3ffULL}},
        {{INFINITY,INFINITY,3.40282347e+38f,3.40282347e+38f},0.0f,{0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL}},
        {{2.75f,1.75f,6.75f,5.75f},7.0f,{0x0ULL,0x0ULL,0x40ULL,0x60ULL,0x0ULL,0x18ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL}},
        {{2.5f,1.5f,7.0f,6.0f},14.0f,{0x0ULL,0x7cULL,0x44ULL,0x74ULL,0x14ULL,0x1cULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL}},
        {{1.0f,0.0f,8.5f,7.5f},53.25f,{0xfeULL,0xfeULL,0xfeULL,0xfeULL,0xfeULL,0x7eULL,0x7eULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL}},
        {{INFINITY,INFINITY,3.40282347e+38f,3.40282347e+38f},0.0f,{0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL}},
        {{1.5f,2.5f,8.5f,7.5f},15.0845108f,{0x0ULL,0x0ULL,0x0ULL,0xfcULL,0x84ULL,0x84ULL,0xfcULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL}},
        {{1.0f,2.0f,9.0f,8.0f},30.1690216f,{0x0ULL,0x0ULL,0x78ULL,0xfcULL,0x186ULL,0x186ULL,0xfcULL,0x78ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL}},
        {{-2.0f,-1.0f,12.0f,11.0f},129.044449f,{0x1feULL,0x7ffULL,0x7ffULL,0xfffULL,0xfffULL,0xfffULL,0xfffULL,0x7ffULL,0x7ffULL,0x3ffULL,0x78ULL,0x0ULL}},
        {{INFINITY,INFINITY,3.40282347e+38f,3.40282347e+38f},0.0f,{0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL}},
        {{2.75f,2.25f,6.25f,4.75f},3.5705421f,{0x0ULL,0x0ULL,0x10ULL,0x0ULL,0x10ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL}},
        {{2.5f,2.0f,6.5f,5.0f},7.14108515f,{0x0ULL,0x0ULL,0x38ULL,0x24ULL,0x38ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL}},
        {{1.0f,0.5f,8.0f,6.5f},31.4830017f,{0x0ULL,0x7cULL,0xfeULL,0xfeULL,0xfeULL,0x7cULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL}},
    },
    {
        {{0},0,{0}},
        {{0},0,{0}},
        {{0},0,{0}},
        {{0},0,{0}},
        {{0},0,{0}},
        {{0},0,{0}},
        {{0},0,{0}},
        {{0},0,{0}},
        {{INFINITY,INFINITY,3.40282347e+38f,3.40282347e+38f},0.0f,{0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL}},
        {{2.5f,2.5f,32.5f,32.5f},123.224976f,{0x0ULL,0x0ULL,0xfffffffcULL,0x80000004ULL,0x80000004ULL,0x80000004ULL,0x80000004ULL,0x80000004ULL,0x80000004ULL,0x80000004ULL,0x80000004ULL,0x80000004ULL,0x80000004ULL,0x80000004ULL,0x80000004ULL,0x80000004ULL,0x80000004ULL,0x80000004ULL,0x80000004ULL,0x80000004ULL,0x80000004ULL,0x80000004ULL,0x80000004ULL,0x80000004ULL,0x80020004ULL,0x80020004ULL,0x80050004ULL,0x800d8004ULL,0x80088004ULL,0x80104004ULL,0x80306004ULL,0xffe03ffcULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL}},
        {{2.0f,2.0f,33.0f,33.0f},246.449966f,{0x0ULL,0x0ULL,0x1fffffffcULL,0x1fffffffcULL,0x18000000cULL,0x18000000cULL,0x18000000cULL,0x18000000cULL,0x18000000cULL,0x18000000cULL,0x18000000cULL,0x18000000cULL,0x18000000cULL,0x18000000cULL,0x18000000cULL,0x18000000cULL,0x18000000cULL,0x18000000cULL,0x18000000cULL,0x18000000cULL,0x18000000cULL,0x18000000cULL,0x18000000cULL,0x18000000cULL,0x18002000cULL,0x18007000cULL,0x1800f000cULL,0x1800d800cULL,0x1801dc00cULL,0x18038c00cULL,0x18030600cULL,0x1fff07ffcULL,0x1ffe03ffcULL,0x0ULL,0x0ULL,0x0ULL}},
        {{-1.0f,-1.0f,36.0f,36.0f},985.799866f,{0xfffffffffULL,0xfffffffffULL,0xfffffffffULL,0xfffffffffULL,0xfffffffffULL,0xfffffffffULL,0xfffffffffULL,0xff000007fULL,0xff000007fULL,0xff000007fULL,0xff000007fULL,0xff000007fULL,0xff000007fULL,0xff000007fULL,0xff000007fULL,0xff000007fULL,0xff000007fULL,0xff000007fULL,0xff002007fULL,0xff002007fULL,0xff007007fULL,0xff00f807fULL,0xff00f807fULL,0xff01fc07fULL,0xff03fe07fULL,0xff03fe07fULL,0xff07ff07fULL,0xff0fff87fULL,0xfffffffffULL,0xfffffffffULL,0xfffffffffULL,0xfffffffffULL,0xfffffffffULL,0xffffdffffULL,0xffff8ffffULL,0xffff0ffffULL}},
        {{INFINITY,INFINITY,3.40282347e+38f,3.40282347e+38f},0.0f,{0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL}},
        {{3.25f,2.25f,18.25f,17.25f},30.8062477f,{0x0ULL,0x0ULL,0x3fff8ULL,0x8ULL,0x8ULL,0x8ULL,0x8ULL,0x8ULL,0x8ULL,0x8ULL,0x8ULL,0x8ULL,0x8ULL,0x8ULL,0xc08ULL,0x208ULL,0x1008ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL}},
        {{3.0f,2.0f,18.5f,17.5f},61.6124878f,{0x0ULL,0x0ULL,0x3fff8ULL,0x20008ULL,0x20008ULL,0x20008ULL,0x20008ULL,0x20008ULL,0x20008ULL,0x20008ULL,0x20008ULL,0x20008ULL,0x20008ULL,0x20408ULL,0x20c08ULL,0x21a08ULL,0x3f1f8ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL}},
        {{1.5f,0.5f,20.0f,19.0f},246.449966f,{0xffffeULL,0xffffeULL,0xffffeULL,0xffffeULL,0xf001eULL,0xf001eULL,0xf001eULL,0xf001eULL,0xf001eULL,0xf001eULL,0xf041eULL,0xf0c1eULL,0xf0e1eULL,0xf1f1eULL,0xf3f1eULL,0xffffeULL,0xffffeULL,0xffffeULL,0xff3feULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL,0x0ULL}},
        {{0},0,{0}},
        {{0},0,{0}},
        {{0},0,{0}},
        {{0},0,{0}},
        {{0},0,{0}},
        {{0},0,{0}},
        {{0},0,{0}},
        {{0},0,{0}},
    },
};

static void test_widen(BOOL app_shape)
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
    const float widths[] = {0,1,2,8};
    const D2D1_RECT_F outer = {0,0,11,11}, inner = {2,2,9,9};
    const D2D1_POINT_2F points[] = {{2,2},{9,2},{9,5},{6,5},{6,9},{2,9}};
    const D2D1_POINT_2F app_points[] = {{3,3},{32,3},{32,32},{22,32},{17.5f,25.25f},{13,32},{3,32}};
    const D2D1_POINT_2F *selected = app_shape ? app_points : points;
    const UINT point_count = app_shape ? ARRAY_SIZE(app_points) : ARRAY_SIZE(points);
    const D2D1_ELLIPSE ellipse_desc = {{5,5},3,2};
    const D2D1_COLOR_F color = {.25,.5,.75,1}, clear = {0};
    const D2D1_MATRIX_3X2_F transform = {.m11=.5,.m22=.5,.dx=2,.dy=1};
    D2D1_MAPPED_RECT map;
    D2D1_RECT_F bounds;
    float area;
    HRESULT hr, close_hr;
    unsigned int shape,mode,transformed,x,y;
    if (app_shape) size = (D2D1_SIZE_U){36,36};
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
    ID2D1GeometrySink_BeginFigure(sink,selected[0],D2D1_FIGURE_BEGIN_FILLED);
    ID2D1GeometrySink_AddLines(sink,&selected[1],point_count-1);
    ID2D1GeometrySink_EndFigure(sink,D2D1_FIGURE_END_CLOSED);
    REQUIRE(ID2D1GeometrySink_Close(sink));
    ID2D1GeometrySink_Release(sink);
    ID2D1DeviceContext_SetTarget(dc,(ID2D1Image *)target);
    ID2D1DeviceContext_SetAntialiasMode(dc,D2D1_ANTIALIAS_MODE_ALIASED);
    for(shape=app_shape ? 1 : 0;shape<2;++shape)for(transformed=0;transformed<2;++transformed)for(mode=0;mode<4;++mode)
    {
        unsigned int index = (shape*2+transformed)*4+mode;
        winetest_push_context("app %u shape %u transform %u width %g",app_shape,shape,transformed,widths[mode]);
        input = shape == 0 ? (ID2D1Geometry *)inner_rectangle : shape == 1 ? (ID2D1Geometry *)path : (ID2D1Geometry *)ellipse;
        REQUIRE(ID2D1Factory_CreatePathGeometry((ID2D1Factory *)factory,&result));
        REQUIRE(ID2D1PathGeometry_Open(result,&sink));
        hr=ID2D1Geometry_Widen(input,widths[mode],NULL,transformed ? &transform : NULL,
                .25f,(ID2D1SimplifiedGeometrySink *)sink);
        todo_wine_if(!app_shape && shape == 1 && mode == 3)
            ok(hr == S_OK,"Widen returned %#lx.\n",hr);
        close_hr=ID2D1GeometrySink_Close(sink); ok(close_hr == S_OK,"Close returned %#lx.\n",close_hr);
        ID2D1GeometrySink_Release(sink);
        if (SUCCEEDED(hr) && SUCCEEDED(close_hr))
        {
            hr=ID2D1PathGeometry_GetBounds(result,NULL,&bounds);
            ok(hr == S_OK,"Bounds returned %#lx.\n",hr);
            ok(bounds.left == expected[app_shape][index].bounds[0]
                    && bounds.top == expected[app_shape][index].bounds[1]
                    && bounds.right == expected[app_shape][index].bounds[2]
                    && bounds.bottom == expected[app_shape][index].bounds[3],"Unexpected bounds.\n");
            hr=ID2D1PathGeometry_ComputeArea(result,NULL,.25f,&area);
            ok(hr == S_OK,"Area returned %#lx.\n",hr);
            ok(fabsf(area-expected[app_shape][index].area) <= 8*FLT_EPSILON*max(1,fabsf(area)),
                    "Area %.9g, expected %.9g.\n",area,expected[app_shape][index].area);
            ID2D1DeviceContext_BeginDraw(dc);
            ID2D1DeviceContext_Clear(dc,&clear);
            ID2D1DeviceContext_FillGeometry(dc,(ID2D1Geometry *)result,(ID2D1Brush *)brush,NULL);
            hr=ID2D1DeviceContext_EndDraw(dc,NULL,NULL);ok(hr == S_OK,"Draw returned %#lx.\n",hr);
            if(SUCCEEDED(hr))
            {
                unsigned int mismatches = 0;
                REQUIRE(ID2D1Bitmap1_CopyFromBitmap(readback,NULL,(ID2D1Bitmap *)target,NULL));
                REQUIRE(ID2D1Bitmap1_Map(readback,D2D1_MAP_OPTIONS_READ,&map));
                for(y=0;y<size.height;++y)for(x=0;x<size.width*4;++x)
                {
                    static const float rgba[] = {.25f,.5f,.75f,1};
                    float actual, value = expected[app_shape][index].rows[y] & (UINT64_C(1) << (x/4)) ? rgba[x%4] : 0;
                    memcpy(&actual,map.bits+y*map.pitch+x*4,4);
                    if (actual != value) ++mismatches;
                }
                /* The actual sloping contour still differs at 1–3 aliased edge pixels. */
                todo_wine_if(app_shape && ((!transformed && (mode == 2 || mode == 3)) || (transformed && mode == 1)))
                    ok(!mismatches,"%u RGBA channels differ from native edge coverage.\n",mismatches);
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

START_TEST(widen)
{
    test_widen(FALSE);
    test_widen(TRUE);
}
