/* Device-context format capabilities and observable rendering support.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include "d2d1_1.h"
#include "d3d11.h"
#include "effect_test.h"
#include "wine/test.h"

static BOOL native_format_supported(DXGI_FORMAT format)
{
    switch (format)
    {
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_A8_UNORM:
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
            return TRUE;
        default:
            return FALSE;
    }
}

static BOOL missing_format(DXGI_FORMAT format)
{
    return format == DXGI_FORMAT_R10G10B10A2_UNORM || format == DXGI_FORMAT_R8G8_UNORM
            || format == DXGI_FORMAT_R8_UNORM || format == DXGI_FORMAT_BC1_UNORM
            || format == DXGI_FORMAT_BC2_UNORM || format == DXGI_FORMAT_BC3_UNORM;
}

static void test_queries(ID2D1DeviceContext *context, ID3D11Device *d3d)
{
    UINT format, support;
    BOOL expected, actual;
    HRESULT hr;

    for (format = 0; format < 192; ++format)
    {
        support = 0;
        hr = ID3D11Device_CheckFormatSupport(d3d, format, &support);
        expected = native_format_supported(format) && SUCCEEDED(hr)
                && (support & D3D11_FORMAT_SUPPORT_TEXTURE2D) && (support & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE);
        actual = ID2D1DeviceContext_IsDxgiFormatSupported(context, format);
        todo_wine_if(missing_format(format) && expected)
        ok(actual == expected, "Format %u: supported %d, expected %d (D3D flags %#x).\n",
                format, actual, expected, support);
    }
    ok(!ID2D1DeviceContext_IsDxgiFormatSupported(context, 0xffffffff), "Invalid format supported.\n");
    ok(!ID2D1DeviceContext_IsDxgiFormatSupported(context, 0x7fffffff), "Invalid format supported.\n");
}

static void test_rendering(ID2D1DeviceContext *context)
{
    static const struct
    {
        DXGI_FORMAT format;
        UINT bpp;
        BYTE green[16];
    }
    formats[] =
    {
        {DXGI_FORMAT_R32G32B32A32_FLOAT, 16, {0,0,0,0, 0,0,128,63, 0,0,0,0, 0,0,128,63}},
        {DXGI_FORMAT_R16G16B16A16_FLOAT, 8, {0,0, 0,60, 0,0, 0,60}},
        {DXGI_FORMAT_R16G16B16A16_UNORM, 8, {0,0, 255,255, 0,0, 255,255}},
        {DXGI_FORMAT_R8G8B8A8_UNORM, 4, {0,255,0,255}},
        {DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 4, {0,255,0,255}},
        {DXGI_FORMAT_A8_UNORM, 1, {255}},
        {DXGI_FORMAT_B8G8R8A8_UNORM, 4, {0,255,0,255}},
        {DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, 4, {0,255,0,255}},
    };
    D2D1_BITMAP_PROPERTIES1 props = {{DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED}, 96, 96, 0, NULL};
    D2D1_SIZE_U size = {4, 4};
    D2D1_COLOR_F green = {0, 1, 0, 1};
    D2D1_MAPPED_RECT map;
    ID2D1Bitmap1 *target, *readback;
    UINT f, x, y;
    HRESULT hr;

    for (f = 0; f < ARRAY_SIZE(formats); ++f)
    {
        winetest_push_context("format %u", formats[f].format);
        props.pixelFormat.format = formats[f].format;
        props.pixelFormat.alphaMode = formats[f].format == DXGI_FORMAT_B8G8R8X8_UNORM
                ? D2D1_ALPHA_MODE_IGNORE : D2D1_ALPHA_MODE_PREMULTIPLIED;
        props.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
        hr = ID2D1DeviceContext_CreateBitmap(context, size, NULL, 0, &props, &target);
        ok(hr == S_OK, "Create target returned %#lx.\n", hr);
        if (FAILED(hr)) { winetest_pop_context(); continue; }
        props.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
        hr = ID2D1DeviceContext_CreateBitmap(context, size, NULL, 0, &props, &readback);
        ok(hr == S_OK, "Create readback returned %#lx.\n", hr);
        if (SUCCEEDED(hr))
        {
            ID2D1DeviceContext_SetTarget(context, (ID2D1Image *)target);
            ID2D1DeviceContext_BeginDraw(context);
            ID2D1DeviceContext_Clear(context, &green);
            hr = ID2D1DeviceContext_EndDraw(context, NULL, NULL);
            ok(hr == S_OK, "EndDraw returned %#lx.\n", hr);
            ID2D1DeviceContext_SetTarget(context, NULL);
            hr = ID2D1Bitmap1_CopyFromBitmap(readback, NULL, (ID2D1Bitmap *)target, NULL);
            ok(hr == S_OK, "Copy returned %#lx.\n", hr);
            hr = ID2D1Bitmap1_Map(readback, D2D1_MAP_OPTIONS_READ, &map);
            ok(hr == S_OK, "Map returned %#lx.\n", hr);
            if (SUCCEEDED(hr))
            {
                for (y = 0; y < size.height; ++y)
                for (x = 0; x < size.width; ++x)
                    ok(!memcmp(map.bits + y * map.pitch + x * formats[f].bpp,
                            formats[f].green, formats[f].bpp), "Incorrect green pixel at %u,%u.\n", x, y);
                hr = ID2D1Bitmap1_Unmap(readback);
                ok(hr == S_OK, "Unmap returned %#lx.\n", hr);
            }
            ID2D1Bitmap1_Release(readback);
        }
        ID2D1Bitmap1_Release(target);
        winetest_pop_context();
    }
}

static void test_bgrx_source(ID2D1DeviceContext *context)
{
    D2D1_BITMAP_PROPERTIES1 props = {{DXGI_FORMAT_B8G8R8X8_UNORM, D2D1_ALPHA_MODE_IGNORE},
            96, 96, D2D1_BITMAP_OPTIONS_NONE, NULL};
    D2D1_SIZE_U size = {4, 4};
    D2D1_RECT_F rect = {0, 0, 4, 4};
    D2D1_MAPPED_RECT map;
    ID2D1Bitmap1 *source, *target, *readback;
    DWORD pixels[16];
    HRESULT hr;
    UINT i, x, y;

    /* BGRX is supported as an input, although it is not a valid render target. */
    for (i = 0; i < ARRAY_SIZE(pixels); ++i) pixels[i] = 0x0000ff00;
    hr = ID2D1DeviceContext_CreateBitmap(context, size, pixels, 16, &props, &source);
    ok(hr == S_OK, "Create BGRX source returned %#lx.\n", hr);
    if (FAILED(hr)) return;
    props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    props.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
    hr = ID2D1DeviceContext_CreateBitmap(context, size, NULL, 0, &props, &target);
    ok(hr == S_OK, "Create BGRX destination returned %#lx.\n", hr);
    if (SUCCEEDED(hr))
    {
        props.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
        hr = ID2D1DeviceContext_CreateBitmap(context, size, NULL, 0, &props, &readback);
        ok(hr == S_OK, "Create BGRX readback returned %#lx.\n", hr);
        if (SUCCEEDED(hr))
        {
            ID2D1DeviceContext_SetTarget(context, (ID2D1Image *)target);
            ID2D1DeviceContext_BeginDraw(context);
            ID2D1DeviceContext_Clear(context, NULL);
            ID2D1DeviceContext_DrawBitmap(context, (ID2D1Bitmap *)source, &rect, 1.0f,
                    D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR, &rect, NULL);
            hr = ID2D1DeviceContext_EndDraw(context, NULL, NULL);
            ok(hr == S_OK, "Draw BGRX returned %#lx.\n", hr);
            ID2D1DeviceContext_SetTarget(context, NULL);
            hr = ID2D1Bitmap1_CopyFromBitmap(readback, NULL, (ID2D1Bitmap *)target, NULL);
            ok(hr == S_OK, "Copy BGRX result returned %#lx.\n", hr);
            hr = ID2D1Bitmap1_Map(readback, D2D1_MAP_OPTIONS_READ, &map);
            ok(hr == S_OK, "Map BGRX result returned %#lx.\n", hr);
            if (SUCCEEDED(hr))
            {
                for (y = 0; y < 4; ++y)
                for (x = 0; x < 4; ++x)
                    ok(((DWORD *)(map.bits + y * map.pitch))[x] == 0xff00ff00,
                            "Unexpected BGRX output at %u,%u: %#lx.\n", x, y,
                            ((DWORD *)(map.bits + y * map.pitch))[x]);
                hr = ID2D1Bitmap1_Unmap(readback);
                ok(hr == S_OK, "Unmap BGRX result returned %#lx.\n", hr);
            }
            ID2D1Bitmap1_Release(readback);
        }
        ID2D1Bitmap1_Release(target);
    }
    ID2D1Bitmap1_Release(source);
}

START_TEST(format_support)
{
    static const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_11_0};
    ID2D1Factory1 *factory;
    ID3D11Device *d3d;
    IDXGIDevice *dxgi;
    ID2D1Device *device;
    ID2D1DeviceContext *context;
    HRESULT hr;
    UINT l;

    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL, (void **)&factory);
    ok(hr == S_OK, "CreateFactory returned %#lx.\n", hr);
    if (FAILED(hr)) return;
    for (l = 0; l < ARRAY_SIZE(levels); ++l)
    {
        winetest_push_context("level %#x", levels[l]);
        hr = D3D11CreateDevice(NULL, effect_test_driver(), NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                &levels[l], 1, D3D11_SDK_VERSION, &d3d, NULL, NULL);
        if (FAILED(hr))
        {
            skip("D3D device unavailable, hr %#lx.\n", hr);
            winetest_pop_context();
            continue;
        }
        hr = ID3D11Device_QueryInterface(d3d, &IID_IDXGIDevice, (void **)&dxgi);
        ok(hr == S_OK, "QueryInterface returned %#lx.\n", hr);
        if (SUCCEEDED(hr))
        {
            hr = ID2D1Factory1_CreateDevice(factory, dxgi, &device);
            ok(hr == S_OK, "CreateDevice returned %#lx.\n", hr);
            if (SUCCEEDED(hr))
            {
                hr = ID2D1Device_CreateDeviceContext(device, 0, &context);
                ok(hr == S_OK, "CreateDeviceContext returned %#lx.\n", hr);
                if (SUCCEEDED(hr))
                {
                    test_queries(context, d3d);
                    test_rendering(context);
                    test_bgrx_source(context);
                    ID2D1DeviceContext_Release(context);
                }
                ID2D1Device_Release(device);
            }
            IDXGIDevice_Release(dxgi);
        }
        ID3D11Device_Release(d3d);
        winetest_pop_context();
    }
    ID2D1Factory1_Release(factory);
}
