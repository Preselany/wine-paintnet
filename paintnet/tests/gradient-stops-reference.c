/* Legacy gradient collections, versioned metadata, and gradient pixels.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define main color_context_base_main
#include "color-context-reference.c"
#undef main

static void dump_stops(const char *name, const D2D1_GRADIENT_STOP *stops, unsigned int count)
{
    unsigned int i;
    printf("%s", name);
    for (i = 0; i < count; ++i)
        printf(" %.9g %.9g %.9g %.9g %.9g", stops[i].position,
                stops[i].color.r, stops[i].color.g, stops[i].color.b, stops[i].color.a);
    putchar('\n');
}

int main(void)
{
    ID3D11Device *d3d;
    IDXGIDevice *dxgi;
    ID2D1Factory1 *factory;
    ID2D1Device *device;
    ID2D1DeviceContext *dc;
    ID2D1GradientStopCollection *gradient;
    ID2D1GradientStopCollection1 *gradient1;
    ID2D1Brush *brush;
    ID2D1Bitmap1 *target, *readback;
    IUnknown *unknown;
    D2D1_SIZE_U size = {16,1};
    D2D1_BITMAP_PROPERTIES1 desc = {{DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},
            96,96,D2D1_BITMAP_OPTIONS_TARGET,NULL};
    D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES brush_desc = {{4,0},{12,0}};
    D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES radial_desc = {{4,.5f},{0,0},8,8};
    D2D1_GRADIENT_STOP stops[3], copy[5];
    D2D1_COLOR_F clear = {0};
    D2D1_RECT_F rect = {0,0,16,1};
    D2D1_MAPPED_RECT map;
    HRESULT hr;
    unsigned int gamma, extend, colors, i;
    if (getenv("GRADIENT_DETAIL"))
    {
        size.width = 1024;
        rect.right = 1024;
        brush_desc.startPoint.x = 0;
        brush_desc.endPoint.x = 1024;
    }
    if (getenv("GRADIENT_LENGTH"))
    {
        brush_desc.startPoint.x = 0;
        brush_desc.endPoint.x = strtof(getenv("GRADIENT_LENGTH"), NULL);
        size.width = 128;
        rect.right = 128;
    }
    setvbuf(stdout, NULL, _IOFBF, 65536);
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    REQUIRE(D3D11CreateDevice(NULL, getenv("D2D1_TEST_WARP") ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE,
            NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0, D3D11_SDK_VERSION, &d3d, NULL, NULL));
    REQUIRE(ID3D11Device_QueryInterface(d3d, &IID_IDXGIDevice, (void **)&dxgi));
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL, (void **)&factory));
    REQUIRE(ID2D1Factory1_CreateDevice(factory, dxgi, &device));
    REQUIRE(ID2D1Device_CreateDeviceContext(device, 0, &dc));
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc, size, NULL, 0, &desc, &target));
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc, size, NULL, 0, &desc, &readback));
    ID2D1DeviceContext_SetTarget(dc, (ID2D1Image *)target);
    if (getenv("GRADIENT_DPI"))
        ID2D1DeviceContext_SetDpi(dc, strtof(getenv("GRADIENT_DPI"), NULL), 96);
    if (getenv("GRADIENT_SCALE"))
    {
        D2D1_MATRIX_3X2_F transform = {.m11 = 1, .m22 = 1};
        transform.m11 = strtof(getenv("GRADIENT_SCALE"), NULL);
        ID2D1DeviceContext_SetTransform(dc, &transform);
    }
    for (gamma = 0; gamma < 2; ++gamma)
    for (extend = 0; extend < 3; ++extend)
    for (colors = 0; colors < 2; ++colors)
    {
        stops[0] = (D2D1_GRADIENT_STOP){0, {.1f,.2f,.9f,.2f}};
        stops[1] = (D2D1_GRADIENT_STOP){.5f, {.8f,.4f,.2f,.8f}};
        stops[2] = (D2D1_GRADIENT_STOP){1, {.3f,.7f,.5f,1}};
        if (colors)
        {
            stops[0].color = (D2D1_COLOR_F){-1, 2, .4f, 0};
            stops[1].color = (D2D1_COLOR_F){.25f, -.5f, 1.5f, .5f};
            stops[2].color = (D2D1_COLOR_F){2, .5f, -2, 2};
        }
        printf("CASE %u %u %u\n", gamma, extend, colors);
        hr = ID2D1RenderTarget_CreateGradientStopCollection((ID2D1RenderTarget *)dc, stops, 3, gamma, extend, &gradient);
        printf("CREATE %08lx\n", hr);
        if (FAILED(hr)) continue;
        printf("BASE count=%u gamma=%u extend=%u\n", ID2D1GradientStopCollection_GetGradientStopCount(gradient),
                ID2D1GradientStopCollection_GetColorInterpolationGamma(gradient),
                ID2D1GradientStopCollection_GetExtendMode(gradient));
        for (i = 0; i < 5; ++i) copy[i] = (D2D1_GRADIENT_STOP){99,{99,99,99,99}};
        ID2D1GradientStopCollection_GetGradientStops(gradient, copy, 5);
        dump_stops("STOPS", copy, 5);
        hr = ID2D1GradientStopCollection_QueryInterface(gradient, &IID_ID2D1GradientStopCollection1, (void **)&gradient1);
        printf("QI1 %08lx\n", hr);
        if (SUCCEEDED(hr))
        {
            printf("V1 pre=%u post=%u precision=%u interpolation=%u\n",
                    ID2D1GradientStopCollection1_GetPreInterpolationSpace(gradient1),
                    ID2D1GradientStopCollection1_GetPostInterpolationSpace(gradient1),
                    ID2D1GradientStopCollection1_GetBufferPrecision(gradient1),
                    ID2D1GradientStopCollection1_GetColorInterpolationMode(gradient1));
            for (i = 0; i < 5; ++i) copy[i] = (D2D1_GRADIENT_STOP){99,{99,99,99,99}};
            ID2D1GradientStopCollection1_GetGradientStops1(gradient1, copy, 5);
            dump_stops("STOPS1", copy, 5);
            REQUIRE(ID2D1GradientStopCollection1_QueryInterface(gradient1, &IID_IUnknown, (void **)&unknown));
            printf("IDENTITY %d\n", unknown == (IUnknown *)gradient);
            IUnknown_Release(unknown);
            ID2D1GradientStopCollection1_Release(gradient1);
        }
        if (getenv("GRADIENT_RADIAL"))
            REQUIRE(ID2D1DeviceContext_CreateRadialGradientBrush(dc, &radial_desc, NULL, gradient,
                    (ID2D1RadialGradientBrush **)&brush));
        else
            REQUIRE(ID2D1DeviceContext_CreateLinearGradientBrush(dc, &brush_desc, NULL, gradient,
                    (ID2D1LinearGradientBrush **)&brush));
        ID2D1DeviceContext_BeginDraw(dc);
        ID2D1DeviceContext_Clear(dc, &clear);
        ID2D1DeviceContext_FillRectangle(dc, &rect, (ID2D1Brush *)brush);
        hr = ID2D1DeviceContext_EndDraw(dc, NULL, NULL);
        printf("DRAW %08lx\n", hr);
        if (SUCCEEDED(hr))
        {
            REQUIRE(ID2D1Bitmap1_CopyFromBitmap(readback, NULL, (ID2D1Bitmap *)target, NULL));
            REQUIRE(ID2D1Bitmap1_Map(readback, D2D1_MAP_OPTIONS_READ, &map));
            printf("PIXELS");
            for (i = 0; i < size.width * 4; ++i) printf(" %.9g", ((float *)map.bits)[i]);
            putchar('\n');
            REQUIRE(ID2D1Bitmap1_Unmap(readback));
        }
        ID2D1Brush_Release(brush);
        ID2D1GradientStopCollection_Release(gradient);
        fflush(stdout);
    }
    ID2D1DeviceContext_SetTarget(dc, NULL);
    ID2D1Bitmap1_Release(readback);
    ID2D1Bitmap1_Release(target);
    ID2D1DeviceContext_Release(dc);
    ID2D1Device_Release(device);
    ID2D1Factory1_Release(factory);
    IDXGIDevice_Release(dxgi);
    ID3D11Device_Release(d3d);
    CoUninitialize();
    return 0;
}
