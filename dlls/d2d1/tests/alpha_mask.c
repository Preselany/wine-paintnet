/* Alpha-mask rendering and supported effect-graph evaluation.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <math.h>
#include "d2d1_1.h"
#include "d2d1effects_2.h"
#include "d3d11.h"
#include "wine/test.h"

static HRESULT draw(ID2D1DeviceContext *context, ID2D1Image *image, ID2D1Bitmap1 *target,
        const D2D1_POINT_2F *offset, const D2D1_RECT_F *rect)
{
    D2D1_COLOR_F clear = {0};
    ID2D1DeviceContext_SetTarget(context, (ID2D1Image *)target);
    ID2D1DeviceContext_BeginDraw(context);
    ID2D1DeviceContext_Clear(context, &clear);
    ID2D1DeviceContext_DrawImage(context, image, offset, rect, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
            D2D1_COMPOSITE_MODE_SOURCE_OVER);
    return ID2D1DeviceContext_EndDraw(context, NULL, NULL);
}

static void check_pixels(ID2D1Bitmap1 *bitmap, ID2D1Bitmap1 *readback, const float expected[8][4])
{
    D2D1_MAPPED_RECT mapped;
    unsigned int x, y, c;
    float value;
    HRESULT hr;

    hr = ID2D1Bitmap1_CopyFromBitmap(readback, NULL, (ID2D1Bitmap *)bitmap, NULL);
    ok(hr == S_OK, "Copy returned %#lx.\n", hr);
    hr = ID2D1Bitmap1_Map(readback, D2D1_MAP_OPTIONS_READ, &mapped);
    ok(hr == S_OK, "Map returned %#lx.\n", hr);
    if (FAILED(hr)) return;
    for (y = 0; y < 2; ++y)
        for (x = 0; x < 4; ++x)
            for (c = 0; c < 4; ++c)
            {
                memcpy(&value, mapped.bits + y * mapped.pitch + (x * 4 + c) * sizeof(float), sizeof(value));
                ok(fabsf(value - expected[y * 4 + x][c]) < 0.0001f,
                        "Pixel %u,%u channel %u: %.8g, expected %.8g.\n", x, y, c, value, expected[y * 4 + x][c]);
            }
    ID2D1Bitmap1_Unmap(readback);
}

START_TEST(alpha_mask)
{
    static const float colors[8][4] =
    {
        {1, 0.5f, 0.25f, 1}, {0.2f, 0.4f, 0.1f, 0.5f}, {0, 0, 0, 0}, {0.1f, 0.2f, 0.3f, 1},
        {2, 1.5f, -0.5f, 1}, {0.1f, 0, 0.2f, 0.25f}, {0.3f, 0.4f, 0.5f, 0.75f}, {0.8f, 0.1f, 0.6f, 1},
    };
    static const float masks[8][4] =
    {
        {0, 1, 0.5f, 0}, {0.1f, 0.2f, 0.3f, 0.5f}, {4, -2, 7, 1}, {0, 0, 0, 1},
        {0, 0, 0, 0.5f}, {1, 0, 1, 0.25f}, {0, 0.1f, 0, 0.75f}, {0, 0, 0, 1},
    };
    D2D1_BITMAP_PROPERTIES1 desc = {{DXGI_FORMAT_R32G32B32A32_FLOAT, D2D1_ALPHA_MODE_PREMULTIPLIED},
            96, 96, D2D1_BITMAP_OPTIONS_NONE, NULL};
    D2D1_SIZE_U size = {4, 2}, small_size = {2, 1};
    D2D1_RECT_F bounds, rect = {1, 0, 3, 2};
    D2D1_POINT_2F offset = {2, 0};
    ID3D11Device *d3d_device = NULL;
    IDXGIDevice *dxgi_device = NULL;
    ID2D1Factory1 *factory = NULL;
    ID2D1Device *device = NULL;
    ID2D1DeviceContext *context = NULL;
    ID2D1Effect *effect = NULL, *outer = NULL, *metadata = NULL, *histogram = NULL;
    ID2D1Image *image = NULL, *outer_image = NULL, *metadata_image = NULL, *histogram_image = NULL;
    ID2D1Bitmap1 *source = NULL, *mask = NULL, *small_mask = NULL, *ignored = NULL, *target = NULL, *readback = NULL;
    ID2D1Bitmap1 *scaled_source = NULL, *scaled_mask = NULL;
    ID2D1Bitmap *target_alias = NULL;
    ID2D1Properties *properties = NULL;
    float expected[8][4], masked[8][4], bins[4];
    UINT32 value;
    unsigned int i, c, pass;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            NULL, 0, D3D11_SDK_VERSION, &d3d_device, NULL, NULL);
    if (FAILED(hr)) { win_skip("No D3D11 device.\n"); goto cleanup; }
    hr = ID3D11Device_QueryInterface(d3d_device, &IID_IDXGIDevice, (void **)&dxgi_device);
    ok(hr == S_OK, "DXGI device returned %#lx.\n", hr);
    if (FAILED(hr)) goto cleanup;
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL, (void **)&factory);
    ok(hr == S_OK, "Factory returned %#lx.\n", hr);
    if (FAILED(hr)) goto cleanup;
    hr = ID2D1Factory1_GetEffectProperties(factory, &CLSID_D2D1AlphaMask, &properties);
    ok(hr == S_OK, "Alpha Mask metadata returned %#lx.\n", hr);
    if (FAILED(hr)) goto cleanup;
    ok(!ID2D1Properties_GetPropertyCount(properties), "Unexpected custom properties.\n");
    hr = ID2D1Factory1_CreateDevice(factory, dxgi_device, &device);
    ok(hr == S_OK, "Device returned %#lx.\n", hr);
    if (FAILED(hr)) goto cleanup;
    hr = ID2D1Device_CreateDeviceContext(device, 0, &context);
    ok(hr == S_OK, "Context returned %#lx.\n", hr);
    if (FAILED(hr)) goto cleanup;
    hr = ID2D1DeviceContext_CreateEffect(context, &CLSID_D2D1AlphaMask, &effect);
    ok(hr == S_OK, "Alpha Mask creation returned %#lx.\n", hr);
    if (FAILED(hr)) goto cleanup;
    ok(ID2D1Effect_GetInputCount(effect) == 2, "Unexpected input count.\n");
    hr = ID2D1Effect_SetInputCount(effect, 1);
    ok(hr == E_INVALIDARG, "Changing fixed input count returned %#lx.\n", hr);
    hr = ID2D1DeviceContext_CreateEffect(context, &CLSID_D2D1AlphaMask, &outer);
    ok(hr == S_OK, "Outer mask creation returned %#lx.\n", hr);
    if (FAILED(hr)) goto cleanup;
    hr = ID2D1DeviceContext_CreateEffect(context, &CLSID_D2D1OpacityMetadata, &metadata);
    ok(hr == S_OK, "Metadata creation returned %#lx.\n", hr);
    if (FAILED(hr)) goto cleanup;
    hr = ID2D1DeviceContext_CreateBitmap(context, size, colors, sizeof(colors[0]) * 4, &desc, &source);
    ok(hr == S_OK, "Source returned %#lx.\n", hr);
    if (FAILED(hr)) goto cleanup;
    hr = ID2D1DeviceContext_CreateBitmap(context, size, masks, sizeof(masks[0]) * 4, &desc, &mask);
    ok(hr == S_OK, "Mask returned %#lx.\n", hr);
    if (FAILED(hr)) goto cleanup;
    hr = ID2D1DeviceContext_CreateBitmap(context, small_size, masks, sizeof(masks[0]) * 4, &desc, &small_mask);
    ok(hr == S_OK, "Small mask returned %#lx.\n", hr);
    if (FAILED(hr)) goto cleanup;
    desc.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
    hr = ID2D1DeviceContext_CreateBitmap(context, size, masks, sizeof(masks[0]) * 4, &desc, &ignored);
    ok(hr == S_OK, "Ignore-alpha bitmap returned %#lx.\n", hr);
    if (FAILED(hr)) goto cleanup;
    desc.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
    hr = ID2D1DeviceContext_CreateBitmap(context, size, NULL, 0, &desc, &target);
    ok(hr == S_OK, "Target returned %#lx.\n", hr);
    if (FAILED(hr)) goto cleanup;
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_CANNOT_DRAW | D2D1_BITMAP_OPTIONS_CPU_READ;
    hr = ID2D1DeviceContext_CreateBitmap(context, size, NULL, 0, &desc, &readback);
    ok(hr == S_OK, "Readback returned %#lx.\n", hr);
    if (FAILED(hr)) goto cleanup;

    ID2D1Effect_SetInput(metadata, 0, (ID2D1Image *)source, TRUE);
    ID2D1Effect_GetOutput(metadata, &metadata_image);
    ID2D1Effect_SetInput(effect, 0, metadata_image, TRUE);
    ID2D1Effect_SetInput(effect, 1, (ID2D1Image *)mask, TRUE);
    ID2D1Effect_GetOutput(effect, &image);
    ID2D1Effect_SetInput(outer, 0, image, TRUE);
    ID2D1Effect_SetInput(outer, 1, image, TRUE);
    ID2D1Effect_GetOutput(outer, &outer_image);
    for (i = 0; i < 8; ++i)
        for (c = 0; c < 4; ++c) masked[i][c] = colors[i][c] * masks[i][3];

    for (pass = 0; pass < 4; ++pass)
    {
        winetest_push_context("draw %u", pass);
        memset(expected, 0, sizeof(expected));
        if (pass == 3)
        {
            for (i = 0; i < 2; ++i)
                for (c = 0; c < 4; ++c)
                {
                    expected[i * 4 + 2][c] = masked[i * 4 + 1][c];
                    expected[i * 4 + 3][c] = masked[i * 4 + 2][c];
                }
        }
        else
            for (i = 0; i < 8; ++i)
                for (c = 0; c < 4; ++c)
                    expected[i][c] = pass == 2 ? masked[i][c] * masked[i][3] : masked[i][c];
        hr = draw(context, pass == 2 ? outer_image : image, target, pass == 3 ? &offset : NULL, pass == 3 ? &rect : NULL);
        ok(hr == S_OK, "Draw returned %#lx.\n", hr);
        check_pixels(target, readback, expected);
        winetest_pop_context();
    }
    /* Content changes must be visible without reattaching the input. */
    for (i = 0; i < 8; ++i)
    {
        memcpy(expected[i], masks[i], sizeof(expected[i]));
        expected[i][3] = 1;
    }
    hr = ID2D1Bitmap1_CopyFromMemory(mask, NULL, expected, sizeof(expected[0]) * 4);
    ok(hr == S_OK, "Updating mask content returned %#lx.\n", hr);
    hr = draw(context, image, target, NULL, NULL);
    ok(hr == S_OK, "Updated mask draw returned %#lx.\n", hr);
    check_pixels(target, readback, colors);
    hr = ID2D1Bitmap1_CopyFromMemory(mask, NULL, masks, sizeof(masks[0]) * 4);
    ok(hr == S_OK, "Restoring mask content returned %#lx.\n", hr);

    /* Analysis consumes the real intermediate image produced by Alpha Mask. */
    hr = ID2D1DeviceContext_CreateEffect(context, &CLSID_D2D1Histogram, &histogram);
    ok(hr == S_OK, "Histogram creation returned %#lx.\n", hr);
    if (SUCCEEDED(hr))
    {
        value = 4;
        ID2D1Effect_SetValue(histogram, D2D1_HISTOGRAM_PROP_NUM_BINS, D2D1_PROPERTY_TYPE_UINT32,
                (const BYTE *)&value, sizeof(value));
        value = D2D1_CHANNEL_SELECTOR_A;
        ID2D1Effect_SetValue(histogram, D2D1_HISTOGRAM_PROP_CHANNEL_SELECT, D2D1_PROPERTY_TYPE_ENUM,
                (const BYTE *)&value, sizeof(value));
        ID2D1Effect_SetInput(histogram, 0, image, TRUE);
        ID2D1Effect_GetOutput(histogram, &histogram_image);
        hr = draw(context, histogram_image, target, NULL, NULL);
        ok(hr == S_OK, "Histogram draw returned %#lx.\n", hr);
        hr = ID2D1Effect_GetValue(histogram, D2D1_HISTOGRAM_PROP_HISTOGRAM_OUTPUT, D2D1_PROPERTY_TYPE_BLOB,
                (BYTE *)bins, sizeof(bins));
        ok(hr == S_OK, "Histogram output returned %#lx.\n", hr);
        ok(bins[0] == 0.375f && bins[1] == 0.125f && bins[2] == 0.25f && bins[3] == 0.25f,
                "Unexpected masked alpha histogram: %g, %g, %g, %g.\n", bins[0], bins[1], bins[2], bins[3]);
    }

    ID2D1Effect_SetInput(effect, 1, (ID2D1Image *)small_mask, TRUE);
    hr = ID2D1DeviceContext_GetImageLocalBounds(context, image, &bounds);
    ok(hr == S_OK && !bounds.left && !bounds.top && bounds.right == 2 && bounds.bottom == 1,
            "Mask intersection bounds differ, hr %#lx.\n", hr);
    memset(expected, 0, sizeof(expected));
    memcpy(expected, masked, sizeof(masked[0]) * 2);
    hr = draw(context, image, target, NULL, NULL);
    ok(hr == S_OK, "Smaller mask draw returned %#lx.\n", hr);
    check_pixels(target, readback, expected);

    ID2D1Effect_SetInput(effect, 1, (ID2D1Image *)ignored, TRUE);
    hr = draw(context, image, target, NULL, NULL);
    ok(hr == S_OK, "Ignore-alpha mask draw returned %#lx.\n", hr);
    check_pixels(target, readback, colors);
    ID2D1Effect_SetInput(effect, 0, (ID2D1Image *)ignored, TRUE);
    ID2D1Effect_SetInput(effect, 1, (ID2D1Image *)mask, TRUE);
    for (i = 0; i < 8; ++i)
        for (c = 0; c < 4; ++c) expected[i][c] = (c == 3 ? 1 : masks[i][c]) * masks[i][3];
    hr = draw(context, image, target, NULL, NULL);
    ok(hr == S_OK, "Ignore-alpha destination draw returned %#lx.\n", hr);
    check_pixels(target, readback, expected);

    ID2D1Effect_SetInput(effect, 0, metadata_image, TRUE);
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_NONE;
    desc.dpiX = desc.dpiY = 192;
    hr = ID2D1DeviceContext_CreateBitmap(context, size, colors, sizeof(colors[0]) * 4, &desc, &scaled_source);
    ok(hr == S_OK, "Scaled source returned %#lx.\n", hr);
    if (FAILED(hr)) goto cleanup;
    hr = ID2D1DeviceContext_CreateBitmap(context, size, masks, sizeof(masks[0]) * 4, &desc, &scaled_mask);
    ok(hr == S_OK, "Scaled mask returned %#lx.\n", hr);
    if (FAILED(hr)) goto cleanup;
    ID2D1Effect_SetInput(metadata, 0, (ID2D1Image *)scaled_source, TRUE);
    ID2D1Effect_SetInput(effect, 1, (ID2D1Image *)scaled_mask, TRUE);
    ID2D1DeviceContext_SetDpi(context, 192, 192);
    hr = ID2D1DeviceContext_GetImageLocalBounds(context, outer_image, &bounds);
    ok(hr == S_OK && bounds.right == 2 && bounds.bottom == 1, "DIP bounds differ, hr %#lx.\n", hr);
    hr = draw(context, image, target, NULL, NULL);
    ok(hr == S_OK, "High-DPI effect draw returned %#lx.\n", hr);
    check_pixels(target, readback, masked);
    ID2D1DeviceContext_SetUnitMode(context, D2D1_UNIT_MODE_PIXELS);
    hr = ID2D1DeviceContext_GetImageLocalBounds(context, outer_image, &bounds);
    ok(hr == S_OK && bounds.right == 4 && bounds.bottom == 2, "Pixel bounds differ, hr %#lx.\n", hr);
    hr = draw(context, image, target, NULL, NULL);
    ok(hr == S_OK, "Pixel-mode draw returned %#lx.\n", hr);
    check_pixels(target, readback, masked);
    ID2D1DeviceContext_SetUnitMode(context, D2D1_UNIT_MODE_DIPS);
    ID2D1DeviceContext_SetDpi(context, 96, 96);
    ID2D1Effect_SetInput(metadata, 0, (ID2D1Image *)source, TRUE);
    ID2D1Effect_SetInput(effect, 1, (ID2D1Image *)mask, TRUE);

    /* A cycle through the second input must also be detected and recoverable. */
    ID2D1Effect_SetInput(effect, 1, outer_image, TRUE);
    hr = draw(context, image, target, NULL, NULL);
    ok(hr == D2DERR_CYCLIC_GRAPH, "Cycle draw returned %#lx.\n", hr);
    hr = ID2D1DeviceContext_GetImageLocalBounds(context, image, &bounds);
    ok(hr == D2DERR_CYCLIC_GRAPH, "Cycle bounds returned %#lx.\n", hr);
    ID2D1Effect_SetInput(effect, 1, NULL, TRUE);
    hr = draw(context, image, target, NULL, NULL);
    ok(hr == D2DERR_WRONG_STATE, "Missing input returned %#lx.\n", hr);
    ID2D1Effect_SetInput(effect, 1, (ID2D1Image *)readback, TRUE);
    hr = draw(context, image, target, NULL, NULL);
    ok(hr == D2DERR_BITMAP_CANNOT_DRAW, "Unreadable input returned %#lx.\n", hr);
    ID2D1Effect_SetInput(effect, 1, (ID2D1Image *)target, TRUE);
    hr = draw(context, image, target, NULL, NULL);
    ok(hr == D2DERR_BITMAP_BOUND_AS_TARGET, "Target input returned %#lx.\n", hr);
    hr = ID2D1DeviceContext_CreateSharedBitmap(context, &IID_ID2D1Bitmap, target, NULL, &target_alias);
    ok(hr == S_OK, "Shared target returned %#lx.\n", hr);
    if (SUCCEEDED(hr))
    {
        ID2D1Effect_SetInput(effect, 1, (ID2D1Image *)target_alias, TRUE);
        hr = draw(context, image, target, NULL, NULL);
        ok(hr == D2DERR_BITMAP_BOUND_AS_TARGET, "Aliased target input returned %#lx.\n", hr);
    }
    ID2D1Effect_SetInput(effect, 1, (ID2D1Image *)mask, TRUE);
    hr = draw(context, image, target, NULL, NULL);
    ok(hr == S_OK, "Draw after breaking cycle returned %#lx.\n", hr);
    check_pixels(target, readback, masked);
    check_pixels(source, readback, colors);
    check_pixels(mask, readback, masks);

cleanup:
    if (context) ID2D1DeviceContext_SetTarget(context, NULL);
    if (effect) { ID2D1Effect_SetInput(effect, 0, NULL, TRUE); ID2D1Effect_SetInput(effect, 1, NULL, TRUE); }
    if (outer) { ID2D1Effect_SetInput(outer, 0, NULL, TRUE); ID2D1Effect_SetInput(outer, 1, NULL, TRUE); }
    if (histogram_image) ID2D1Image_Release(histogram_image);
    if (histogram) ID2D1Effect_Release(histogram);
    if (image) ID2D1Image_Release(image);
    if (outer_image) ID2D1Image_Release(outer_image);
    if (metadata_image) ID2D1Image_Release(metadata_image);
    if (metadata) ID2D1Effect_Release(metadata);
    if (outer) ID2D1Effect_Release(outer);
    if (effect) ID2D1Effect_Release(effect);
    if (target_alias) ID2D1Bitmap_Release(target_alias);
    if (readback) ID2D1Bitmap1_Release(readback);
    if (target) ID2D1Bitmap1_Release(target);
    if (ignored) ID2D1Bitmap1_Release(ignored);
    if (small_mask) ID2D1Bitmap1_Release(small_mask);
    if (mask) ID2D1Bitmap1_Release(mask);
    if (scaled_source) ID2D1Bitmap1_Release(scaled_source);
    if (scaled_mask) ID2D1Bitmap1_Release(scaled_mask);
    if (source) ID2D1Bitmap1_Release(source);
    if (properties) ID2D1Properties_Release(properties);
    if (context) ID2D1DeviceContext_Release(context);
    if (device) ID2D1Device_Release(device);
    if (factory) ID2D1Factory1_Release(factory);
    if (dxgi_device) IDXGIDevice_Release(dxgi_device);
    if (d3d_device) ID3D11Device_Release(d3d_device);
    CoUninitialize();
}
