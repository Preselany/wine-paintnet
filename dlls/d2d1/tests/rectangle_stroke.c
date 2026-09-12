/* Explicit solid miter styles on closed rectangles.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <math.h>
#include "wine/test.h"
#include "d2d1_1.h"
#include "wincodec.h"

#define REQUIRE(call) do { hr = (call); ok(hr == S_OK, "%s returned %#lx.\n", #call, hr); if (FAILED(hr)) return; } while (0)
#define SIDE 48

static void render(ID2D1RenderTarget *target, ID2D1SolidColorBrush *brush, IWICBitmap *bitmap,
        ID2D1PathGeometry *path, DWORD *pixels)
{
    const D2D1_COLOR_F clear = {0};
    HRESULT hr;
    ID2D1RenderTarget_BeginDraw(target);
    ID2D1RenderTarget_Clear(target, &clear);
    ID2D1RenderTarget_FillGeometry(target, (ID2D1Geometry *)path, (ID2D1Brush *)brush, NULL);
    REQUIRE(ID2D1RenderTarget_EndDraw(target, NULL, NULL));
    REQUIRE(IWICBitmap_CopyPixels(bitmap, NULL, SIDE * 4, SIDE * SIDE * 4, (BYTE *)pixels));
}

static void test_rectangle_styles(void)
{
    static const D2D1_MATRIX_3X2_F transforms[] =
    {
        {1, 0, 0, 1, 24, 24}, {.8f, .3f, -.2f, 1.1f, 24, 24}, {-1, .2f, .1f, .7f, 24, 24},
    };
    static const float widths[] = {1, 4, 20};
    const D2D1_RECT_F rect = {-6.5f, -6.5f, 6.5f, 6.5f};
    const D2D1_COLOR_F color = {.25f, .5f, .75f, 1};
    D2D1_RENDER_TARGET_PROPERTIES props = {D2D1_RENDER_TARGET_TYPE_DEFAULT,
            {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED}, 96, 96, 0, D2D1_FEATURE_LEVEL_DEFAULT};
    ID2D1Factory1 *factory;
    IWICImagingFactory *wic;
    IWICBitmap *bitmap;
    ID2D1RenderTarget *target;
    ID2D1SolidColorBrush *brush;
    ID2D1RectangleGeometry *rectangle;
    ID2D1StrokeStyle1 *style;
    ID2D1TransformedGeometry *reference_geometry;
    ID2D1PathGeometry *paths[2];
    ID2D1GeometrySink *sink;
    D2D1_STROKE_STYLE_PROPERTIES1 desc;
    D2D1_RECT_F bounds[2];
    DWORD pixels[2][SIDE * SIDE];
    float area[2], width;
    UINT c, bits, transform, aa, i, different, covered, n;
    HRESULT hr, widen_hr;

    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL, (void **)&factory));
    REQUIRE(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&wic));
    REQUIRE(IWICImagingFactory_CreateBitmap(wic, SIDE, SIDE, &GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &bitmap));
    REQUIRE(ID2D1Factory1_CreateWicBitmapRenderTarget(factory, bitmap, &props, &target));
    REQUIRE(ID2D1RenderTarget_CreateSolidColorBrush(target, &color, NULL, &brush));
    REQUIRE(ID2D1Factory1_CreateRectangleGeometry(factory, &rect, &rectangle));

    /* All cap combinations, then widths, transforms, miter limits and stroke
     * transform modes. Caps have no endpoints to act on in a solid closed ring. */
    for (c = 0; c < 64 + 108; ++c)
    {
        memset(&desc, 0, sizeof(desc));
        bits = c % 64;
        desc.startCap = bits & 3;
        desc.endCap = (bits >> 2) & 3;
        desc.dashCap = (bits >> 4) & 3;
        desc.miterLimit = 10;
        desc.dashStyle = D2D1_DASH_STYLE_SOLID;
        desc.dashOffset = -3.75f;
        width = 1;
        transform = 0;
        if (c >= 64)
        {
            n = c - 64;
            desc.transformType = n % 3; n /= 3;
            desc.lineJoin = n % 2 ? D2D1_LINE_JOIN_MITER_OR_BEVEL : D2D1_LINE_JOIN_MITER; n /= 2;
            desc.miterLimit = n % 2 ? 2 : 10; n /= 2;
            width = widths[n % 3]; n /= 3;
            transform = n % 3;
        }
        winetest_push_context("case %u caps %u/%u/%u join %u limit %g type %u width %g matrix %u", c,
                desc.startCap, desc.endCap, desc.dashCap, desc.lineJoin, desc.miterLimit, desc.transformType, width, transform);
        REQUIRE(ID2D1Factory1_CreateStrokeStyle(factory, &desc, NULL, 0, &style));
        REQUIRE(ID2D1Factory1_CreateTransformedGeometry(factory, (ID2D1Geometry *)rectangle,
                &transforms[transform], &reference_geometry));
        memset(paths, 0, sizeof(paths));
        for (i = 0; i < 2; ++i)
        {
            REQUIRE(ID2D1Factory1_CreatePathGeometry(factory, &paths[i]));
            REQUIRE(ID2D1PathGeometry_Open(paths[i], &sink));
            if (!i && desc.transformType != D2D1_STROKE_TRANSFORM_TYPE_NORMAL)
                widen_hr = ID2D1TransformedGeometry_Widen(reference_geometry,
                        desc.transformType == D2D1_STROKE_TRANSFORM_TYPE_HAIRLINE ? 1 : width,
                        NULL, NULL, .01f, (ID2D1SimplifiedGeometrySink *)sink);
            else
                widen_hr = ID2D1RectangleGeometry_Widen(rectangle, width, i ? (ID2D1StrokeStyle *)style : NULL,
                        &transforms[transform], .01f, (ID2D1SimplifiedGeometrySink *)sink);
            ok(widen_hr == S_OK, "Widen (%u) returned %#lx.\n", i, widen_hr);
            hr = ID2D1GeometrySink_Close(sink);
            ok(hr == S_OK, "Sink close returned %#lx.\n", hr);
            ID2D1GeometrySink_Release(sink);
            if (FAILED(widen_hr) || FAILED(hr)) break;
            REQUIRE(ID2D1PathGeometry_GetBounds(paths[i], NULL, &bounds[i]));
            REQUIRE(ID2D1PathGeometry_ComputeArea(paths[i], NULL, .01f, &area[i]));
        }
        if (i == 2)
        {
            for (n = 0; n < 4; ++n)
                ok(fabsf(((float *)&bounds[0])[n] - ((float *)&bounds[1])[n]) < .0001f,
                        "Bounds coordinate %u differs: %g / %g.\n", n, ((float *)&bounds[0])[n], ((float *)&bounds[1])[n]);
            ok(fabsf(area[0] - area[1]) < .01f, "Area differs: %g / %g.\n", area[0], area[1]);
            for (aa = 0; aa < 2; ++aa)
            {
                ID2D1RenderTarget_SetAntialiasMode(target, aa);
                for (i = 0; i < 2; ++i) render(target, brush, bitmap, paths[i], pixels[i]);
                different = covered = 0;
                for (n = 0; n < SIDE * SIDE; ++n)
                {
                    if (pixels[0][n]) ++covered;
                    if (pixels[0][n] != pixels[1][n]) ++different;
                }
                ok(covered, "Empty reference rendering (AA %u).\n", aa);
                ok(!different, "%u pixels differ (AA %u).\n", different, aa);
            }
        }
        for (i = 0; i < 2; ++i) if (paths[i]) ID2D1PathGeometry_Release(paths[i]);
        ID2D1TransformedGeometry_Release(reference_geometry);
        ID2D1StrokeStyle1_Release(style);
        winetest_pop_context();
    }
    ID2D1RectangleGeometry_Release(rectangle);
    ID2D1SolidColorBrush_Release(brush);
    ID2D1RenderTarget_Release(target);
    IWICBitmap_Release(bitmap);
    IWICImagingFactory_Release(wic);
    ID2D1Factory1_Release(factory);
}

START_TEST(rectangle_stroke)
{
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    test_rectangle_styles();
    CoUninitialize();
}
