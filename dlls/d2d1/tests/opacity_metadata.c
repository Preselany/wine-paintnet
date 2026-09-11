/* Opacity metadata must preserve input pixels and drawing coordinates.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <math.h>
#include "d2d1_1.h"
#include "d2d1effects.h"
#include "d3d11.h"
#include "effect_test.h"
#include "wine/test.h"

static HRESULT draw_image(ID2D1DeviceContext *context, ID2D1Image *image, ID2D1Bitmap1 *target,
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

static void read_pixels(ID2D1Bitmap1 *bitmap, ID2D1Bitmap1 *readback, UINT32 *data)
{
    D2D1_MAPPED_RECT mapped;
    HRESULT hr;
    unsigned int y;
    hr = ID2D1Bitmap1_CopyFromBitmap(readback, NULL, (ID2D1Bitmap *)bitmap, NULL);
    ok(hr == S_OK, "Copy returned %#lx.\n", hr);
    hr = ID2D1Bitmap1_Map(readback, D2D1_MAP_OPTIONS_READ, &mapped);
    ok(hr == S_OK, "Map returned %#lx.\n", hr);
    if (FAILED(hr)) return;
    for (y = 0; y < 2; ++y) memcpy(data + y * 3, mapped.bits + y * mapped.pitch, 3 * sizeof(UINT32));
    ID2D1Bitmap1_Unmap(readback);
}

START_TEST(opacity_metadata)
{
    static const UINT32 pixels[] = {0xff0000ff, 0x80004000, 0x40000020, 0, 0xff804020, 0xffffffff};
    D2D1_BITMAP_PROPERTIES1 desc = {{DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED},
            96, 96, D2D1_BITMAP_OPTIONS_NONE, NULL};
    D2D1_VECTOR_4F opaque_rect = {0, 0, 1, 1}, value;
    D2D1_POINT_2F offset = {1, 0};
    D2D1_RECT_F rect = {0, 0, 2, 2}, bounds;
    D2D1_SIZE_U size = {3, 2};
    ID2D1Factory1 *factory;
    ID3D11Device *d3d_device;
    IDXGIDevice *dxgi_device;
    ID2D1Device *device;
    ID2D1DeviceContext *context;
    ID2D1Bitmap1 *source = NULL, *direct = NULL, *through = NULL, *readback = NULL;
    ID2D1Effect *effect = NULL, *outer = NULL;
    ID2D1Image *image = NULL, *outer_image = NULL;
    UINT32 direct_pixels[6], through_pixels[6];
    unsigned int i, pass;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = D3D11CreateDevice(NULL, effect_test_driver(), NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            NULL, 0, D3D11_SDK_VERSION, &d3d_device, NULL, NULL);
    if (FAILED(hr)) { win_skip("No D3D11 device.\n"); goto uninit; }
    hr = ID3D11Device_QueryInterface(d3d_device, &IID_IDXGIDevice, (void **)&dxgi_device);
    ok(hr == S_OK, "DXGI device returned %#lx.\n", hr);
    if (FAILED(hr)) goto release_d3d;
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL, (void **)&factory);
    ok(hr == S_OK, "Factory returned %#lx.\n", hr);
    if (FAILED(hr)) goto release_dxgi;
    hr = ID2D1Factory1_CreateDevice(factory, dxgi_device, &device);
    ok(hr == S_OK, "Device returned %#lx.\n", hr);
    if (FAILED(hr)) goto release_factory;
    hr = ID2D1Device_CreateDeviceContext(device, 0, &context);
    ok(hr == S_OK, "Context returned %#lx.\n", hr);
    if (FAILED(hr)) goto release_device;
    hr = ID2D1DeviceContext_CreateEffect(context, &CLSID_D2D1OpacityMetadata, &effect);
    ok(hr == S_OK, "Opacity Metadata creation returned %#lx.\n", hr);
    if (FAILED(hr)) goto cleanup;
    hr = ID2D1DeviceContext_CreateEffect(context, &CLSID_D2D1OpacityMetadata, &outer);
    ok(hr == S_OK, "Outer metadata creation returned %#lx.\n", hr);
    if (FAILED(hr)) goto cleanup;
    hr = ID2D1Effect_GetValue(effect, D2D1_OPACITYMETADATA_PROP_INPUT_OPAQUE_RECT,
            D2D1_PROPERTY_TYPE_VECTOR4, (BYTE *)&value, sizeof(value));
    ok(hr == S_OK && value.x == -INFINITY && value.y == -INFINITY && value.z == INFINITY && value.w == INFINITY,
            "Default opaque rectangle {%g,%g,%g,%g} differs, hr %#lx.\n",value.x,value.y,value.z,value.w,hr);
    hr = ID2D1Effect_SetValue(effect, D2D1_OPACITYMETADATA_PROP_INPUT_OPAQUE_RECT,
            D2D1_PROPERTY_TYPE_VECTOR4, (const BYTE *)&opaque_rect, sizeof(opaque_rect));
    ok(hr == S_OK, "Set rectangle returned %#lx.\n", hr);
    hr = ID2D1Effect_GetValue(effect, D2D1_OPACITYMETADATA_PROP_INPUT_OPAQUE_RECT,
            D2D1_PROPERTY_TYPE_VECTOR4, (BYTE *)&value, sizeof(value));
    ok(hr == S_OK && !memcmp(&value, &opaque_rect, sizeof(value)), "Rectangle round trip failed.\n");
    hr = ID2D1DeviceContext_CreateBitmap(context, size, pixels, 3 * sizeof(UINT32), &desc, &source);
    ok(hr == S_OK, "Source bitmap returned %#lx.\n", hr);
    if (FAILED(hr)) goto cleanup;
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
    hr = ID2D1DeviceContext_CreateBitmap(context, size, NULL, 0, &desc, &direct);
    ok(hr == S_OK, "Direct target returned %#lx.\n", hr);
    if (FAILED(hr)) goto cleanup;
    hr = ID2D1DeviceContext_CreateBitmap(context, size, NULL, 0, &desc, &through);
    ok(hr == S_OK, "Effect target returned %#lx.\n", hr);
    if (FAILED(hr)) goto cleanup;
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_CANNOT_DRAW | D2D1_BITMAP_OPTIONS_CPU_READ;
    hr = ID2D1DeviceContext_CreateBitmap(context, size, NULL, 0, &desc, &readback);
    ok(hr == S_OK, "Readback returned %#lx.\n", hr);
    if (FAILED(hr)) goto cleanup;
    ID2D1Effect_SetInput(effect, 0, (ID2D1Image *)source, TRUE);
    ID2D1Effect_GetOutput(effect, &image);
    ID2D1Effect_SetInput(outer, 0, image, TRUE);
    hr = ID2D1Effect_SetValue(outer, D2D1_OPACITYMETADATA_PROP_INPUT_OPAQUE_RECT,
            D2D1_PROPERTY_TYPE_VECTOR4, (const BYTE *)&opaque_rect, sizeof(opaque_rect));
    ok(hr == S_OK, "Set outer opaque region returned %#lx.\n", hr);
    ID2D1Effect_GetOutput(outer, &outer_image);
    hr = ID2D1DeviceContext_GetImageLocalBounds(context, outer_image, &bounds);
    ok(hr == S_OK && bounds.left == 0 && bounds.top == 0 && bounds.right == 3 && bounds.bottom == 2,
            "Metadata input bounds differ, hr %#lx.\n", hr);

    for (pass = 0; pass < 2; ++pass)
    {
        winetest_push_context("crop/offset %u", pass);
        hr = draw_image(context, (ID2D1Image *)source, direct, pass ? &offset : NULL, pass ? &rect : NULL);
        ok(hr == S_OK, "Direct draw returned %#lx.\n", hr);
        hr = draw_image(context, outer_image, through, pass ? &offset : NULL, pass ? &rect : NULL);
        ok(hr == S_OK, "Metadata draw returned %#lx.\n", hr);
        read_pixels(direct, readback, direct_pixels);
        read_pixels(through, readback, through_pixels);
        for (i = 0; i < 6; ++i)
        {
            ok(direct_pixels[i] == through_pixels[i], "Pixel %u: direct %#x, metadata %#x.\n",
                    i, direct_pixels[i], through_pixels[i]);
            if (!pass) ok(through_pixels[i] == pixels[i], "Source pixel %u changed from %#x to %#x.\n",
                    i, pixels[i], through_pixels[i]);
        }
        winetest_pop_context();
    }
    /* A two-node cycle must report an error instead of recursing indefinitely. */
    ID2D1Effect_SetInput(effect, 0, outer_image, TRUE);
    hr = draw_image(context, outer_image, through, NULL, NULL);
    ok(hr == D2DERR_CYCLIC_GRAPH, "Cycle returned %#lx.\n", hr);
    ID2D1Effect_SetInput(effect, 0, (ID2D1Image *)source, TRUE);
    hr = draw_image(context, outer_image, through, NULL, NULL);
    ok(hr == S_OK, "Draw after breaking cycle returned %#lx.\n", hr);
cleanup:
    ID2D1DeviceContext_SetTarget(context, NULL);
    if (effect) ID2D1Effect_SetInput(effect, 0, NULL, TRUE);
    if (outer) ID2D1Effect_SetInput(outer, 0, NULL, TRUE);
    if (outer_image) ID2D1Image_Release(outer_image);
    if (image) ID2D1Image_Release(image);
    if (outer) ID2D1Effect_Release(outer);
    if (effect) ID2D1Effect_Release(effect);
    if (readback) ID2D1Bitmap1_Release(readback);
    if (through) ID2D1Bitmap1_Release(through);
    if (direct) ID2D1Bitmap1_Release(direct);
    if (source) ID2D1Bitmap1_Release(source);
    ID2D1DeviceContext_Release(context);
release_device:
    ID2D1Device_Release(device);
release_factory:
    ID2D1Factory1_Release(factory);
release_dxgi:
    IDXGIDevice_Release(dxgi_device);
release_d3d:
    ID3D11Device_Release(d3d_device);
uninit:
    CoUninitialize();
}
