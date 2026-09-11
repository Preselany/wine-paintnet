/* Opacity, alpha modes, bounds, and effect graph rendering.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <math.h>
#include "d2d1_1.h"
#include "d2d1effects_2.h"
#include "d3d11.h"
#include "effect_test.h"
#include "wine/test.h"

static HRESULT draw(ID2D1DeviceContext *context, ID2D1Image *image, const D2D1_RECT_F *rect)
{
    D2D1_COLOR_F clear = {0};
    ID2D1DeviceContext_BeginDraw(context);
    ID2D1DeviceContext_Clear(context, &clear);
    ID2D1DeviceContext_DrawImage(context, image, NULL, rect, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
            D2D1_COMPOSITE_MODE_SOURCE_OVER);
    return ID2D1DeviceContext_EndDraw(context, NULL, NULL);
}

static void compare(ID2D1Bitmap1 *target, ID2D1Bitmap1 *readback, const float expected[5][4])
{
    D2D1_MAPPED_RECT mapped;
    unsigned int x,c;
    float value;
    HRESULT hr;
    hr = ID2D1Bitmap1_CopyFromBitmap(readback, NULL, (ID2D1Bitmap *)target, NULL);
    ok(hr == S_OK, "Copy returned %#lx.\n", hr);
    hr = ID2D1Bitmap1_Map(readback, D2D1_MAP_OPTIONS_READ, &mapped);
    ok(hr == S_OK, "Map returned %#lx.\n", hr);
    if (FAILED(hr)) return;
    for (x = 0; x < 5; ++x)
        for (c = 0; c < 4; ++c)
        {
            memcpy(&value, mapped.bits + (x*4+c)*sizeof(float), sizeof(value));
            ok(fabsf(value-expected[x][c]) < .0001f, "Pixel %u channel %u: %g, expected %g.\n",
                    x,c,value,expected[x][c]);
        }
    ID2D1Bitmap1_Unmap(readback);
}

START_TEST(opacity)
{
    static const float pixels[5][4] =
    {
        {0,.25f,.5f,1}, {.75f,1,.25f,1}, {-.25f,1.25f,.5f,1}, {.125f,.25f,.375f,.5f}, {.1f,.2f,.3f,0},
    };
    static const float amounts[] = {-1,0,.25f,.5f,1,2};
    D2D1_BITMAP_PROPERTIES1 desc = {{DXGI_FORMAT_R32G32B32A32_FLOAT, D2D1_ALPHA_MODE_PREMULTIPLIED},
            96,96,D2D1_BITMAP_OPTIONS_NONE,NULL};
    D2D1_SIZE_U size = {5,1};
    D2D1_RECT_F bounds, crop = {0,0,5,1};
    ID2D1Factory1 *factory = NULL;
    ID2D1Properties *properties = NULL;
    ID3D11Device *d3d = NULL;
    IDXGIDevice *dxgi = NULL;
    ID2D1Device *device = NULL;
    ID2D1DeviceContext *context = NULL;
    ID2D1Effect *effect = NULL, *outer = NULL;
    ID2D1Image *image = NULL, *outer_image = NULL;
    ID2D1Bitmap1 *source = NULL, *ignored = NULL, *target = NULL, *readback = NULL;
    float expected[5][4], amount;
    unsigned int pass, mode, x, c;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL, (void **)&factory);
    ok(hr == S_OK, "Factory returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    hr = ID2D1Factory1_GetEffectProperties(factory, &CLSID_D2D1Opacity, &properties);
    ok(hr == S_OK, "Opacity metadata returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    ok(ID2D1Properties_GetPropertyCount(properties) == 1, "Wrong property count.\n");
    hr = D3D11CreateDevice(NULL, effect_test_driver(), NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            NULL,0,D3D11_SDK_VERSION,&d3d,NULL,NULL);
    if (FAILED(hr)) { win_skip("No D3D11 device.\n"); goto done; }
    hr = ID3D11Device_QueryInterface(d3d, &IID_IDXGIDevice, (void **)&dxgi);
    ok(hr == S_OK, "DXGI returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    hr = ID2D1Factory1_CreateDevice(factory,dxgi,&device);
    ok(hr == S_OK, "Device returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    hr = ID2D1Device_CreateDeviceContext(device,0,&context);
    ok(hr == S_OK, "Context returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    hr = ID2D1DeviceContext_CreateEffect(context,&CLSID_D2D1Opacity,&effect);
    ok(hr == S_OK, "Opacity returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    hr = ID2D1Effect_GetValue(effect,D2D1_OPACITY_PROP_OPACITY,D2D1_PROPERTY_TYPE_FLOAT,(BYTE *)&amount,sizeof(amount));
    ok(hr == S_OK && amount == 1, "Default amount differs: %g, hr %#lx.\n",amount,hr);
    hr = ID2D1DeviceContext_CreateBitmap(context,size,pixels,sizeof(pixels),&desc,&source);
    ok(hr == S_OK, "Source returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    desc.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
    hr = ID2D1DeviceContext_CreateBitmap(context,size,pixels,sizeof(pixels),&desc,&ignored);
    ok(hr == S_OK, "Ignore-alpha source returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    desc.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
    hr = ID2D1DeviceContext_CreateBitmap(context,size,NULL,0,&desc,&target);
    ok(hr == S_OK, "Target returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_CANNOT_DRAW | D2D1_BITMAP_OPTIONS_CPU_READ;
    hr = ID2D1DeviceContext_CreateBitmap(context,size,NULL,0,&desc,&readback);
    ok(hr == S_OK, "Readback returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    ID2D1DeviceContext_SetTarget(context,(ID2D1Image *)target);
    ID2D1Effect_SetInput(effect,0,(ID2D1Image *)source,TRUE);
    ID2D1Effect_GetOutput(effect,&image);
    for (mode = 0; mode < 2; ++mode)
    {
        ID2D1Effect_SetInput(effect,0,(ID2D1Image *)(mode ? ignored : source),TRUE);
        for (pass = 0; pass < ARRAY_SIZE(amounts); ++pass)
        {
            winetest_push_context("alpha mode %u, opacity %g",mode,amounts[pass]);
            amount = amounts[pass];
            hr = ID2D1Effect_SetValue(effect,D2D1_OPACITY_PROP_OPACITY,D2D1_PROPERTY_TYPE_FLOAT,
                    (BYTE *)&amount,sizeof(amount));
            ok(hr == S_OK, "Setting amount returned %#lx.\n",hr);
            hr = ID2D1Effect_GetValue(effect,D2D1_OPACITY_PROP_OPACITY,D2D1_PROPERTY_TYPE_FLOAT,
                    (BYTE *)&amount,sizeof(amount));
            ok(hr == S_OK && amount == min(max(amounts[pass],0),1),
                    "Amount %g was not clamped, hr %#lx.\n",amount,hr);
            for (x = 0; x < 5; ++x)
                for (c = 0; c < 4; ++c) expected[x][c] = pixels[x][c]*amount;
            hr = ID2D1DeviceContext_GetImageLocalBounds(context,image,&bounds);
            ok(hr == S_OK && bounds.left == 0 && bounds.top == 0 && bounds.right == 5 && bounds.bottom == 1,
                    "Bounds differ: {%g,%g,%g,%g}, hr %#lx.\n",bounds.left,bounds.top,bounds.right,bounds.bottom,hr);
            hr = draw(context,image,NULL);
            ok(hr == S_OK, "Opacity draw returned %#lx.\n",hr);
            compare(target,readback,expected);
            winetest_pop_context();
        }
    }
    ID2D1Effect_SetInput(effect,0,(ID2D1Image *)source,TRUE);
    amount = .5f;
    hr = ID2D1Effect_SetValue(effect,D2D1_OPACITY_PROP_OPACITY,D2D1_PROPERTY_TYPE_FLOAT,(BYTE *)&amount,sizeof(amount));
    ok(hr == S_OK, "Setting amount returned %#lx.\n",hr);
    for (x = 0; x < 5; ++x)
        for (c = 0; c < 4; ++c) expected[x][c] = pixels[x][c]*amount;
    ID2D1DeviceContext_SetDpi(context,192,192);
    hr = ID2D1DeviceContext_GetImageLocalBounds(context,image,&bounds);
    ok(hr == S_OK && bounds.right == 2.5f && bounds.bottom == .5f, "DIP bounds differ.\n");
    hr = draw(context,image,NULL);
    ok(hr == S_OK, "192-DPI opacity returned %#lx.\n",hr);
    compare(target,readback,expected);
    ID2D1DeviceContext_SetUnitMode(context,D2D1_UNIT_MODE_PIXELS);
    hr = ID2D1DeviceContext_GetImageLocalBounds(context,image,&bounds);
    ok(hr == S_OK && bounds.right == 5 && bounds.bottom == 1, "Pixel bounds differ.\n");
    hr = draw(context,image,NULL);
    ok(hr == S_OK, "Pixel-unit opacity returned %#lx.\n",hr);
    compare(target,readback,expected);
    ID2D1DeviceContext_SetDpi(context,96,96);
    ID2D1DeviceContext_SetUnitMode(context,D2D1_UNIT_MODE_DIPS);

    /* A second opacity must multiply the first output and retain its bounds. */
    hr = ID2D1DeviceContext_CreateEffect(context,&CLSID_D2D1Opacity,&outer);
    ok(hr == S_OK, "Second opacity returned %#lx.\n",hr);
    if (FAILED(hr)) goto done;
    ID2D1Effect_SetInput(outer,0,image,TRUE);
    amount = .25f;
    hr = ID2D1Effect_SetValue(outer,D2D1_OPACITY_PROP_OPACITY,D2D1_PROPERTY_TYPE_FLOAT,(BYTE *)&amount,sizeof(amount));
    ok(hr == S_OK, "Setting outer amount returned %#lx.\n",hr);
    ID2D1Effect_GetOutput(outer,&outer_image);
    for (x = 0; x < 5; ++x)
        for (c = 0; c < 4; ++c) expected[x][c] = pixels[x][c]*.125f;
    hr = draw(context,outer_image,NULL);
    ok(hr == S_OK, "Nested opacity returned %#lx.\n",hr);
    compare(target,readback,expected);

    crop.left = 1; crop.right = 4;
    for (x = 0; x < 5; ++x)
        for (c = 0; c < 4; ++c) expected[x][c] = x < 3 ? pixels[x+1][c]*.125f : 0;
    hr = draw(context,outer_image,&crop);
    ok(hr == S_OK, "Cropped opacity returned %#lx.\n",hr);
    compare(target,readback,expected);

    ID2D1Effect_SetInput(effect,0,outer_image,TRUE);
    hr = draw(context,image,NULL);
    ok(hr == D2DERR_CYCLIC_GRAPH, "Cyclic graph returned %#lx.\n",hr);
    ID2D1Effect_SetInput(effect,0,NULL,TRUE);
    hr = draw(context,image,NULL);
    ok(hr == D2DERR_INVALID_GRAPH_CONFIGURATION, "Missing input returned %#lx.\n",hr);
    amount = 0;
    hr = ID2D1Effect_SetValue(effect,D2D1_OPACITY_PROP_OPACITY,D2D1_PROPERTY_TYPE_FLOAT,(BYTE *)&amount,sizeof(amount));
    ok(hr == S_OK, "Zero amount returned %#lx.\n",hr);
    hr = draw(context,image,NULL);
    ok(hr == D2DERR_INVALID_GRAPH_CONFIGURATION, "Missing input at zero returned %#lx.\n",hr);
    ID2D1Effect_SetInput(effect,0,(ID2D1Image *)source,TRUE);
    hr = draw(context,image,NULL);
    ok(hr == S_OK, "Recovered graph returned %#lx.\n",hr);
    memset(expected,0,sizeof(expected));
    compare(target,readback,expected);
    compare(source,readback,pixels);
done:
    if (context) ID2D1DeviceContext_SetTarget(context,NULL);
    if (image) ID2D1Image_Release(image);
    if (effect) ID2D1Effect_Release(effect);
    if (outer_image) ID2D1Image_Release(outer_image);
    if (outer) ID2D1Effect_Release(outer);
    if (readback) ID2D1Bitmap1_Release(readback);
    if (target) ID2D1Bitmap1_Release(target);
    if (ignored) ID2D1Bitmap1_Release(ignored);
    if (source) ID2D1Bitmap1_Release(source);
    if (properties) ID2D1Properties_Release(properties);
    if (context) ID2D1DeviceContext_Release(context);
    if (device) ID2D1Device_Release(device);
    if (dxgi) IDXGIDevice_Release(dxgi);
    if (d3d) ID3D11Device_Release(d3d);
    if (factory) ID2D1Factory1_Release(factory);
    CoUninitialize();
}
