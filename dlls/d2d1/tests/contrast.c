/* Contrast transfer values, alpha, and effect input coordinates.
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

START_TEST(contrast)
{
    static const float pixels[5][4] =
    {
        {0,.25f,.5f,1}, {.75f,1,.25f,1}, {-.25f,1.25f,.5f,1}, {.125f,.25f,.375f,.5f}, {0,0,0,0},
    };
    static const UINT channels[5][3] = {{1,2,3},{4,5,2},{0,6,3},{2,3,4},{1,1,1}};
    /* Values of the two quadratics at x = {-1/4,0,1/4,1/2,3/4,1,5/4}.
     * The amount interpolation and extrapolation need native conformance testing. */
    static const float amounts[] = {-1,0,.5f,1};
    static const float transfer[4][7] =
    {
        {-.625f,0,.375f,.5f,.625f,1,1.625f},
        {-.25f,0,.25f,.5f,.75f,1,1.25f},
        {-.0625f,0,.1875f,.5f,.8125f,1,1.0625f},
        {.125f,0,.125f,.5f,.875f,1,.875f},
    };
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
    ID2D1Effect *effect = NULL, *convolution = NULL;
    ID2D1Image *image = NULL, *convolved_image = NULL;
    ID2D1Bitmap1 *source = NULL, *ignored = NULL, *target = NULL, *readback = NULL;
    float expected[5][4], amount;
    UINT clamp;
    unsigned int pass, x,c,index;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL, (void **)&factory);
    ok(hr == S_OK, "Factory returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    hr = ID2D1Factory1_GetEffectProperties(factory, &CLSID_D2D1Contrast, &properties);
    ok(hr == S_OK, "Contrast metadata returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    ok(ID2D1Properties_GetPropertyCount(properties) == 2, "Wrong property count.\n");
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
    hr = ID2D1DeviceContext_CreateEffect(context,&CLSID_D2D1Contrast,&effect);
    ok(hr == S_OK, "Contrast returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    hr = ID2D1Effect_GetValue(effect,D2D1_CONTRAST_PROP_CONTRAST,D2D1_PROPERTY_TYPE_FLOAT,(BYTE *)&amount,sizeof(amount));
    ok(hr == S_OK && amount == 0, "Default amount differs.\n");
    hr = ID2D1Effect_GetValue(effect,D2D1_CONTRAST_PROP_CLAMP_INPUT,D2D1_PROPERTY_TYPE_BOOL,(BYTE *)&clamp,sizeof(clamp));
    ok(hr == S_OK && !clamp, "Default clamping differs.\n");
    amount = 2;
    hr = ID2D1Effect_SetValue(effect,D2D1_CONTRAST_PROP_CONTRAST,D2D1_PROPERTY_TYPE_FLOAT,(BYTE *)&amount,sizeof(amount));
    ok(FAILED(hr), "Out-of-range amount accepted.\n");
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
    for (pass = 0; pass < 8; ++pass)
    {
        winetest_push_context("amount/clamp %u",pass);
        amount = amounts[pass/2]; clamp = pass%2;
        hr = ID2D1Effect_SetValue(effect,D2D1_CONTRAST_PROP_CONTRAST,D2D1_PROPERTY_TYPE_FLOAT,(BYTE *)&amount,sizeof(amount));
        ok(hr == S_OK, "Setting amount returned %#lx.\n", hr);
        hr = ID2D1Effect_SetValue(effect,D2D1_CONTRAST_PROP_CLAMP_INPUT,D2D1_PROPERTY_TYPE_BOOL,(BYTE *)&clamp,sizeof(clamp));
        ok(hr == S_OK, "Setting clamp returned %#lx.\n", hr);
        for (x = 0; x < 5; ++x)
        {
            for (c = 0; c < 3; ++c)
            {
                index = channels[x][c];
                if (clamp && index == 0) index = 1;
                if (clamp && index == 6) index = 5;
                expected[x][c] = transfer[pass/2][index]*pixels[x][3];
            }
            expected[x][3] = pixels[x][3];
        }
        hr = draw(context,image,NULL);
        ok(hr == S_OK, "Contrast draw returned %#lx.\n", hr);
        compare(target,readback,expected);
        winetest_pop_context();
    }
    hr = ID2D1DeviceContext_CreateEffect(context,&CLSID_D2D1ConvolveMatrix,&convolution);
    ok(hr == S_OK, "Convolution returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    ID2D1Effect_SetInput(convolution,0,(ID2D1Image *)source,TRUE);
    ID2D1Effect_GetOutput(convolution,&convolved_image);
    ID2D1Effect_SetInput(effect,0,convolved_image,TRUE);
    hr = ID2D1DeviceContext_GetImageLocalBounds(context,image,&bounds);
    ok(hr == S_OK && bounds.left == -1 && bounds.top == -1 && bounds.right == 6 && bounds.bottom == 2,
            "Contrast lost the input origin, hr %#lx.\n", hr);
    hr = draw(context,image,&crop);
    ok(hr == S_OK, "Contrast after convolution returned %#lx.\n", hr);
    compare(target,readback,expected);

    ID2D1Effect_SetInput(effect,0,(ID2D1Image *)source,TRUE);
    ID2D1DeviceContext_SetDpi(context,192,192);
    hr = ID2D1DeviceContext_GetImageLocalBounds(context,image,&bounds);
    ok(hr == S_OK && bounds.right == 2.5f && bounds.bottom == .5f, "DIP bounds differ.\n");
    hr = draw(context,image,NULL);
    ok(hr == S_OK, "192-DPI contrast returned %#lx.\n", hr);
    compare(target,readback,expected);
    ID2D1DeviceContext_SetUnitMode(context,D2D1_UNIT_MODE_PIXELS);
    hr = ID2D1DeviceContext_GetImageLocalBounds(context,image,&bounds);
    ok(hr == S_OK && bounds.right == 5 && bounds.bottom == 1, "Pixel bounds differ.\n");
    hr = draw(context,image,NULL);
    ok(hr == S_OK, "Pixel-unit contrast returned %#lx.\n", hr);
    compare(target,readback,expected);

    amount = 0; clamp = FALSE;
    ID2D1Effect_SetValue(effect,D2D1_CONTRAST_PROP_CONTRAST,D2D1_PROPERTY_TYPE_FLOAT,(BYTE *)&amount,sizeof(amount));
    ID2D1Effect_SetValue(effect,D2D1_CONTRAST_PROP_CLAMP_INPUT,D2D1_PROPERTY_TYPE_BOOL,(BYTE *)&clamp,sizeof(clamp));
    ID2D1Effect_SetInput(effect,0,(ID2D1Image *)ignored,TRUE);
    memcpy(expected,pixels,sizeof(expected));
    for (x = 0; x < 5; ++x) expected[x][3] = 1;
    hr = draw(context,image,NULL);
    ok(hr == S_OK, "Ignored-alpha contrast returned %#lx.\n", hr);
    compare(target,readback,expected);
    compare(source,readback,pixels);
done:
    if (context) ID2D1DeviceContext_SetTarget(context,NULL);
    if (image) ID2D1Image_Release(image);
    if (effect) ID2D1Effect_Release(effect);
    if (convolved_image) ID2D1Image_Release(convolved_image);
    if (convolution) ID2D1Effect_Release(convolution);
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
