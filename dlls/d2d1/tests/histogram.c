/* Histogram analysis from known pixels.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <math.h>
#include "d2d1_1.h"
#include "d2d1effects.h"
#include "d3d11.h"
#include "wine/test.h"

static void draw_histogram(ID2D1DeviceContext *context, ID2D1Effect *effect, const D2D1_RECT_F *rect,
        const float *expected, unsigned int bins)
{
    ID2D1Image *image;
    float output[1024], sum = 0;
    unsigned int i;
    HRESULT hr;

    ID2D1Effect_GetOutput(effect, &image);
    ID2D1DeviceContext_BeginDraw(context);
    ID2D1DeviceContext_DrawImage(context, image, NULL, rect, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
            D2D1_COMPOSITE_MODE_SOURCE_OVER);
    hr = ID2D1DeviceContext_EndDraw(context, NULL, NULL);
    ok(hr == S_OK, "Drawing Histogram returned %#lx.\n", hr);
    ID2D1Image_Release(image);
    hr = ID2D1Effect_GetValue(effect, D2D1_HISTOGRAM_PROP_HISTOGRAM_OUTPUT, D2D1_PROPERTY_TYPE_BLOB,
            (BYTE *)output, bins * sizeof(float));
    ok(hr == S_OK, "Reading output returned %#lx.\n", hr);
    if (FAILED(hr)) return;
    for (i = 0; i < bins; ++i)
    {
        ok(fabsf(output[i] - expected[i]) < 0.000001f,
                "Bin %u is %.8f, expected %.8f.\n", i, output[i], expected[i]);
        sum += output[i];
    }
    ok(fabsf(sum - 1.0f) < 0.000001f, "Histogram sum is %.8f.\n", sum);
}

START_TEST(histogram)
{
    static const float pixels[][4] =
    {
        {0, 0, 1, 1}, {.25f, .5f, .75f, 1}, {.5f, .25f, 0, .5f}, {.75f, .75f, .75f, 1},
        {-1, .125f, 3, 1}, {2, .25f, .5f, 1}, {.125f, 0, .125f, .25f}, {0, 0, 0, 0},
    };
    static const float expected[][4] =
    {
        {3/8.0f, 1/8.0f, 1/8.0f, 3/8.0f},
        {4/8.0f, 1/8.0f, 2/8.0f, 1/8.0f},
        {2/8.0f, 0, 2/8.0f, 4/8.0f},
        {1/8.0f, 1/8.0f, 1/8.0f, 5/8.0f},
    };
    static const float cropped[] = {0, 0, 0, 1};
    static const float eight_bins[] = {3/8.0f, 0, 1/8.0f, 0, 1/8.0f, 0, 1/8.0f, 2/8.0f};
    static const float replaced[] = {0, 0, 1, 0};
    static const UINT32 replacement_pixels[] = {0x00008000, 0x00008000};
    D2D1_COLOR_F target_color = {0.2f, 0.4f, 0.6f, 1};
    D2D1_BITMAP_PROPERTIES1 desc = {{DXGI_FORMAT_R32G32B32A32_FLOAT, D2D1_ALPHA_MODE_PREMULTIPLIED},
            192, 192, D2D1_BITMAP_OPTIONS_NONE, NULL};
    D2D1_RECT_F crop = {2, 0, 4, 1}, dip_crop = {1, 0, 2, .5f};
    D2D1_SIZE_U size = {4, 2};
    ID2D1Factory1 *factory;
    ID2D1Properties *metadata;
    ID3D11Device *d3d_device;
    IDXGIDevice *dxgi_device;
    ID2D1Device *device;
    ID2D1DeviceContext *context;
    ID2D1Bitmap1 *source = NULL, *target = NULL, *readback = NULL, *replacement = NULL;
    ID2D1Effect *effect, *opacity = NULL;
    ID2D1Image *opacity_image;
    D2D1_MAPPED_RECT mapped;
    D3D_FEATURE_LEVEL level;
    UINT32 value, channel, x, y;
    WCHAR category[32];
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL, (void **)&factory);
    ok(hr == S_OK, "Factory returned %#lx.\n", hr);
    if (FAILED(hr)) goto uninit;
    hr = ID2D1Factory1_GetEffectProperties(factory, &CLSID_D2D1Histogram, &metadata);
    ok(hr == S_OK, "Histogram registration missing, hr %#lx.\n", hr);
    if (FAILED(hr)) goto release_factory;
    hr = ID2D1Properties_GetValue(metadata, D2D1_PROPERTY_CATEGORY, D2D1_PROPERTY_TYPE_STRING,
            (BYTE *)category, sizeof(category));
    ok(hr == S_OK && !wcscmp(category, L"Analysis"), "Category returned %#lx, %s.\n", hr, debugstr_w(category));
    value = ID2D1Properties_GetValueSize(metadata, D2D1_HISTOGRAM_PROP_NUM_BINS);
    ok(value == sizeof(UINT32), "Metadata scalar size is %u.\n", value);
    ID2D1Properties_Release(metadata);
    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            NULL, 0, D3D11_SDK_VERSION, &d3d_device, &level, NULL);
    if (FAILED(hr)) { win_skip("No D3D11 device.\n"); goto release_factory; }
    hr = ID3D11Device_QueryInterface(d3d_device, &IID_IDXGIDevice, (void **)&dxgi_device);
    ok(hr == S_OK, "DXGI device returned %#lx.\n", hr);
    if (FAILED(hr)) goto release_d3d;
    hr = ID2D1Factory1_CreateDevice(factory, dxgi_device, &device);
    ok(hr == S_OK, "D2D device returned %#lx.\n", hr);
    if (FAILED(hr)) goto release_dxgi;
    hr = ID2D1Device_CreateDeviceContext(device, 0, &context);
    ok(hr == S_OK, "D2D context returned %#lx.\n", hr);
    if (FAILED(hr)) goto release_device;
    hr = ID2D1DeviceContext_CreateEffect(context, &CLSID_D2D1Histogram, &effect);
    if (hr == D2DERR_INSUFFICIENT_DEVICE_CAPABILITIES && level < D3D_FEATURE_LEVEL_11_0)
    {
        win_skip("Device lacks the required compute support.\n");
        goto release_context;
    }
    ok(hr == S_OK, "Histogram creation returned %#lx.\n", hr);
    if (FAILED(hr)) goto release_context;
    hr = ID2D1Effect_GetValue(effect, D2D1_HISTOGRAM_PROP_NUM_BINS, D2D1_PROPERTY_TYPE_UINT32,
            (BYTE *)&value, sizeof(value));
    ok(hr == S_OK && value == 256, "Default bins returned %#lx, %u.\n", hr, value);
    value = 4;
    hr = ID2D1Effect_SetValue(effect, D2D1_HISTOGRAM_PROP_NUM_BINS, D2D1_PROPERTY_TYPE_UINT32,
            (BYTE *)&value, sizeof(value));
    ok(hr == S_OK, "Setting bin count returned %#lx.\n", hr);
    value = ID2D1Effect_GetValueSize(effect, D2D1_HISTOGRAM_PROP_HISTOGRAM_OUTPUT);
    ok(value == 4 * sizeof(float), "Output size is %u.\n", value);
    value = 0;
    hr = ID2D1Effect_SetValue(effect, D2D1_HISTOGRAM_PROP_NUM_BINS, D2D1_PROPERTY_TYPE_UINT32,
            (BYTE *)&value, sizeof(value));
    ok(FAILED(hr), "Zero bins accepted.\n");
    value = 4;
    hr = ID2D1Effect_SetValue(effect, D2D1_HISTOGRAM_PROP_CHANNEL_SELECT, D2D1_PROPERTY_TYPE_ENUM,
            (BYTE *)&value, sizeof(value));
    ok(FAILED(hr), "Invalid channel accepted.\n");
    hr = ID2D1Effect_SetValue(effect, D2D1_HISTOGRAM_PROP_HISTOGRAM_OUTPUT, D2D1_PROPERTY_TYPE_BLOB,
            (const BYTE *)expected[0], sizeof(expected[0]));
    ok(FAILED(hr), "Output property is writable.\n");
    hr = ID2D1DeviceContext_CreateBitmap(context, size, pixels, 4 * sizeof(pixels[0]), &desc, &source);
    ok(hr == S_OK, "Source bitmap returned %#lx.\n", hr);
    if (FAILED(hr)) goto release_bitmaps;
    ID2D1Effect_SetInput(effect, 0, (ID2D1Image *)source, TRUE);
    desc.pixelFormat.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
    hr = ID2D1DeviceContext_CreateBitmap(context, size, NULL, 0, &desc, &target);
    ok(hr == S_OK, "Target bitmap returned %#lx.\n", hr);
    if (FAILED(hr)) goto release_bitmaps;
    ID2D1DeviceContext_SetTarget(context, (ID2D1Image *)target);
    ID2D1DeviceContext_BeginDraw(context);
    ID2D1DeviceContext_Clear(context, &target_color);
    hr = ID2D1DeviceContext_EndDraw(context, NULL, NULL);
    ok(hr == S_OK, "Clear returned %#lx.\n", hr);
    for (channel = 0; channel < 4; ++channel)
    {
        winetest_push_context("channel %u", channel);
        hr = ID2D1Effect_SetValue(effect, D2D1_HISTOGRAM_PROP_CHANNEL_SELECT, D2D1_PROPERTY_TYPE_ENUM,
                (BYTE *)&channel, sizeof(channel));
        ok(hr == S_OK, "Set channel returned %#lx.\n", hr);
        draw_histogram(context, effect, NULL, expected[channel], 4);
        draw_histogram(context, effect, NULL, expected[channel], 4);
        winetest_pop_context();
    }
    hr = ID2D1DeviceContext_CreateEffect(context, &CLSID_D2D1OpacityMetadata, &opacity);
    ok(hr == S_OK, "Metadata input creation returned %#lx.\n", hr);
    if (SUCCEEDED(hr))
    {
        ID2D1Effect_SetInput(opacity, 0, (ID2D1Image *)source, TRUE);
        ID2D1Effect_GetOutput(opacity, &opacity_image);
        ID2D1Effect_SetInput(effect, 0, opacity_image, TRUE);
        ID2D1Image_Release(opacity_image);
    }
    channel = D2D1_CHANNEL_SELECTOR_R;
    ID2D1Effect_SetValue(effect, D2D1_HISTOGRAM_PROP_CHANNEL_SELECT, D2D1_PROPERTY_TYPE_ENUM,
            (BYTE *)&channel, sizeof(channel));
    /* Effect coordinates use context DPI, independently of bitmap DPI. */
    ID2D1DeviceContext_SetDpi(context, 192, 192);
    draw_histogram(context, effect, &dip_crop, cropped, 4);
    ID2D1DeviceContext_SetDpi(context, 96, 96);
    draw_histogram(context, effect, &crop, cropped, 4);
    ID2D1DeviceContext_SetUnitMode(context, D2D1_UNIT_MODE_PIXELS);
    draw_histogram(context, effect, &crop, cropped, 4);
    value = 8;
    ID2D1Effect_SetValue(effect, D2D1_HISTOGRAM_PROP_NUM_BINS, D2D1_PROPERTY_TYPE_UINT32,
            (BYTE *)&value, sizeof(value));
    ok(ID2D1Effect_GetValueSize(effect, D2D1_HISTOGRAM_PROP_HISTOGRAM_OUTPUT) == 8 * sizeof(float),
            "Output size did not change with bins.\n");
    draw_histogram(context, effect, NULL, eight_bins, 8);

    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_CANNOT_DRAW | D2D1_BITMAP_OPTIONS_CPU_READ;
    hr = ID2D1DeviceContext_CreateBitmap(context, size, NULL, 0, &desc, &readback);
    ok(hr == S_OK, "Readback bitmap returned %#lx.\n", hr);
    if (FAILED(hr)) goto release_bitmaps;
    hr = ID2D1Bitmap1_CopyFromBitmap(readback, NULL, (ID2D1Bitmap *)target, NULL);
    ok(hr == S_OK, "Copy target returned %#lx.\n", hr);
    hr = ID2D1Bitmap1_Map(readback, D2D1_MAP_OPTIONS_READ, &mapped);
    ok(hr == S_OK, "Map target returned %#lx.\n", hr);
    if (SUCCEEDED(hr))
    {
        for (y = 0; y < size.height; ++y)
            for (x = 0; x < size.width; ++x)
                ok(((UINT32 *)(mapped.bits + y * mapped.pitch))[x] == 0xff996633,
                        "Histogram changed target pixel %u, %u.\n", x, y);
        ID2D1Bitmap1_Unmap(readback);
    }
    desc.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_NONE;
    size.width = 2;
    size.height = 1;
    hr = ID2D1DeviceContext_CreateBitmap(context, size, replacement_pixels, sizeof(replacement_pixels), &desc, &replacement);
    ok(hr == S_OK, "Replacement source returned %#lx.\n", hr);
    if (SUCCEEDED(hr))
    {
        value = 4;
        ID2D1Effect_SetValue(effect, D2D1_HISTOGRAM_PROP_NUM_BINS, D2D1_PROPERTY_TYPE_UINT32,
                (BYTE *)&value, sizeof(value));
        channel = D2D1_CHANNEL_SELECTOR_G;
        ID2D1Effect_SetValue(effect, D2D1_HISTOGRAM_PROP_CHANNEL_SELECT, D2D1_PROPERTY_TYPE_ENUM,
                (BYTE *)&channel, sizeof(channel));
        ID2D1Effect_SetInput(effect, 0, (ID2D1Image *)replacement, TRUE);
        draw_histogram(context, effect, NULL, replaced, 4);
    }
release_bitmaps:
    ID2D1DeviceContext_SetTarget(context, NULL);
    if (replacement) ID2D1Bitmap1_Release(replacement);
    if (readback) ID2D1Bitmap1_Release(readback);
    if (target) ID2D1Bitmap1_Release(target);
    if (source) ID2D1Bitmap1_Release(source);
    ID2D1Effect_Release(effect);
    if (opacity) ID2D1Effect_Release(opacity);
release_context:
    ID2D1DeviceContext_Release(context);
release_device:
    ID2D1Device_Release(device);
release_dxgi:
    IDXGIDevice_Release(dxgi_device);
release_d3d:
    ID3D11Device_Release(d3d_device);
release_factory:
    ID2D1Factory1_Release(factory);
uninit:
    CoUninitialize();
}
