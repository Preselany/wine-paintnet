/* Convolution pixels, extended bounds, and graph composition.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <math.h>
#include "d2d1_1.h"
#include "d2d1effects_2.h"
#include "d3d11.h"
#include "wine/test.h"

static void set_uint(ID2D1Effect *effect, UINT index, D2D1_PROPERTY_TYPE type, UINT value)
{
    HRESULT hr = ID2D1Effect_SetValue(effect, index, type, (const BYTE *)&value, sizeof(value));
    ok(hr == S_OK, "Property %u returned %#lx.\n", index, hr);
}

static void set_float(ID2D1Effect *effect, UINT index, float value)
{
    HRESULT hr = ID2D1Effect_SetValue(effect, index, D2D1_PROPERTY_TYPE_FLOAT, (const BYTE *)&value, sizeof(value));
    ok(hr == S_OK, "Property %u returned %#lx.\n", index, hr);
}

static void set_kernel(ID2D1Effect *effect, UINT width, UINT height, const float *kernel)
{
    HRESULT hr;
    set_uint(effect, D2D1_CONVOLVEMATRIX_PROP_KERNEL_SIZE_X, D2D1_PROPERTY_TYPE_UINT32, width);
    set_uint(effect, D2D1_CONVOLVEMATRIX_PROP_KERNEL_SIZE_Y, D2D1_PROPERTY_TYPE_UINT32, height);
    hr = ID2D1Effect_SetValue(effect, D2D1_CONVOLVEMATRIX_PROP_KERNEL_MATRIX, D2D1_PROPERTY_TYPE_BLOB,
            (const BYTE *)kernel, width * height * sizeof(float));
    ok(hr == S_OK, "Kernel returned %#lx.\n", hr);
}

static HRESULT draw(ID2D1DeviceContext *context, ID2D1Image *image, const D2D1_POINT_2F *offset,
        const D2D1_RECT_F *rect)
{
    D2D1_COLOR_F clear = {0};
    ID2D1DeviceContext_BeginDraw(context);
    ID2D1DeviceContext_Clear(context, &clear);
    ID2D1DeviceContext_DrawImage(context, image, offset, rect, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
            D2D1_COMPOSITE_MODE_SOURCE_OVER);
    return ID2D1DeviceContext_EndDraw(context, NULL, NULL);
}

static void compare(ID2D1Bitmap1 *target, ID2D1Bitmap1 *readback, const float expected[20][4])
{
    D2D1_MAPPED_RECT mapped;
    unsigned int x, y, c;
    float value;
    HRESULT hr;
    hr = ID2D1Bitmap1_CopyFromBitmap(readback, NULL, (ID2D1Bitmap *)target, NULL);
    ok(hr == S_OK, "Copy returned %#lx.\n", hr);
    hr = ID2D1Bitmap1_Map(readback, D2D1_MAP_OPTIONS_READ, &mapped);
    ok(hr == S_OK, "Map returned %#lx.\n", hr);
    if (FAILED(hr)) return;
    for (y = 0; y < 4; ++y)
        for (x = 0; x < 5; ++x)
            for (c = 0; c < 4; ++c)
            {
                memcpy(&value, mapped.bits + y * mapped.pitch + (x * 4 + c) * sizeof(float), sizeof(value));
                ok(fabsf(value - expected[y * 5 + x][c]) < 0.0001f, "Pixel %u,%u channel %u: %g, expected %g.\n",
                        x, y, c, value, expected[y * 5 + x][c]);
            }
    ID2D1Bitmap1_Unmap(readback);
}

static void check_bounds(ID2D1DeviceContext *context, ID2D1Image *image, float left, float top, float right, float bottom)
{
    D2D1_RECT_F bounds = {0};
    HRESULT hr = ID2D1DeviceContext_GetImageLocalBounds(context, image, &bounds);
    ok(hr == S_OK && bounds.left == left && bounds.top == top && bounds.right == right && bounds.bottom == bottom,
            "Bounds {%g,%g,%g,%g}, hr %#lx; expected {%g,%g,%g,%g}.\n",
            bounds.left, bounds.top, bounds.right, bounds.bottom, hr, left, top, right, bottom);
}

START_TEST(convolve_matrix)
{
    static const float values[] = {.2f, .4f, .8f};
    static const float identity[] = {0,0,0,0,1,0,0,0,0};
    static const float blur[] = {1,1,1}, edge[] = {-1,0,1}, single = 1;
    D2D1_BITMAP_PROPERTIES1 desc = {{DXGI_FORMAT_R32G32B32A32_FLOAT, D2D1_ALPHA_MODE_PREMULTIPLIED},
            96, 96, D2D1_BITMAP_OPTIONS_NONE, NULL};
    D2D1_SIZE_U size = {3,2}, target_size = {5,4};
    D2D1_RECT_F crop = {0,0,3,2}, inverted = {3,2,0,0};
    D2D1_POINT_2F target_offset = {1,1};
    D2D1_VECTOR_2F vector;
    ID2D1Factory1 *factory = NULL;
    ID3D11Device *d3d = NULL;
    IDXGIDevice *dxgi = NULL;
    ID2D1Device *device = NULL;
    ID2D1DeviceContext *context = NULL;
    ID2D1Properties *properties = NULL;
    ID2D1Effect *effect = NULL, *mask_effect = NULL, *histogram = NULL, *outer = NULL;
    ID2D1Image *image = NULL, *masked_image = NULL, *outer_image = NULL, *histogram_image = NULL;
    ID2D1Bitmap1 *source = NULL, *target = NULL, *readback = NULL;
    float pixels[6][4], expected[20][4], matrix[9], bins[4];
    unsigned int i, x, y, c, pass;
    UINT value;
    float scalar;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL, (void **)&factory);
    ok(hr == S_OK, "Factory returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    hr = ID2D1Factory1_GetEffectProperties(factory, &CLSID_D2D1ConvolveMatrix, &properties);
    ok(hr == S_OK, "Convolve Matrix metadata returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    ok(ID2D1Properties_GetPropertyCount(properties) == 11, "Wrong property count.\n");
    ok(ID2D1Properties_GetType(properties, D2D1_CONVOLVEMATRIX_PROP_KERNEL_UNIT_LENGTH) == D2D1_PROPERTY_TYPE_VECTOR2,
            "Kernel unit length must be a vector, as declared in the SDK.\n");
    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            NULL, 0, D3D11_SDK_VERSION, &d3d, NULL, NULL);
    if (FAILED(hr)) { win_skip("No D3D11 device.\n"); goto done; }
    hr = ID3D11Device_QueryInterface(d3d, &IID_IDXGIDevice, (void **)&dxgi);
    ok(hr == S_OK, "DXGI device returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    hr = ID2D1Factory1_CreateDevice(factory, dxgi, &device);
    ok(hr == S_OK, "Device returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    hr = ID2D1Device_CreateDeviceContext(device, 0, &context);
    ok(hr == S_OK, "Context returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    hr = ID2D1DeviceContext_CreateEffect(context, &CLSID_D2D1ConvolveMatrix, &effect);
    ok(hr == S_OK, "Effect returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    hr = ID2D1Effect_GetValue(effect, D2D1_CONVOLVEMATRIX_PROP_KERNEL_MATRIX, D2D1_PROPERTY_TYPE_BLOB,
            (BYTE *)matrix, sizeof(matrix));
    ok(hr == S_OK && !memcmp(matrix, identity, sizeof(matrix)), "Default kernel differs, hr %#lx.\n", hr);
    hr = ID2D1Effect_GetValue(effect, D2D1_CONVOLVEMATRIX_PROP_KERNEL_UNIT_LENGTH, D2D1_PROPERTY_TYPE_VECTOR2,
            (BYTE *)&vector, sizeof(vector));
    ok(hr == S_OK && vector.x == 1 && vector.y == 1, "Default unit length differs.\n");
    hr = ID2D1Effect_GetValue(effect, D2D1_CONVOLVEMATRIX_PROP_DIVISOR, D2D1_PROPERTY_TYPE_FLOAT,
            (BYTE *)&scalar, sizeof(scalar));
    ok(hr == S_OK && scalar == 1, "Default divisor differs.\n");
    value = 0;
    hr = ID2D1Effect_SetValue(effect, D2D1_CONVOLVEMATRIX_PROP_KERNEL_SIZE_X, D2D1_PROPERTY_TYPE_UINT32,
            (const BYTE *)&value, sizeof(value));
    ok(FAILED(hr), "Zero-sized kernel accepted.\n");
    hr = ID2D1Effect_SetValue(effect, D2D1_CONVOLVEMATRIX_PROP_KERNEL_MATRIX, D2D1_PROPERTY_TYPE_BLOB,
            (const BYTE *)&single, sizeof(single));
    ok(FAILED(hr), "Truncated kernel accepted.\n");
    for (i = 0; i < 6; ++i)
    {
        pixels[i][0] = pixels[i][1] = pixels[i][2] = values[i % 3];
        pixels[i][3] = 1;
    }
    hr = ID2D1DeviceContext_CreateBitmap(context, size, pixels, sizeof(pixels[0]) * 3, &desc, &source);
    ok(hr == S_OK, "Source returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
    hr = ID2D1DeviceContext_CreateBitmap(context, target_size, NULL, 0, &desc, &target);
    ok(hr == S_OK, "Target returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_CANNOT_DRAW | D2D1_BITMAP_OPTIONS_CPU_READ;
    hr = ID2D1DeviceContext_CreateBitmap(context, target_size, NULL, 0, &desc, &readback);
    ok(hr == S_OK, "Readback returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    ID2D1DeviceContext_SetTarget(context, (ID2D1Image *)target);
    ID2D1Effect_SetInput(effect, 0, (ID2D1Image *)source, TRUE);
    ID2D1Effect_GetOutput(effect, &image);
    check_bounds(context, image, -1,-1,4,3);
    for (pass = 0; pass < 3; ++pass)
    {
        winetest_push_context("identity/crop %u", pass);
        memset(expected, 0, sizeof(expected));
        for (y = 0; y < 2; ++y)
            for (x = 0; x < 3; ++x) memcpy(expected[(y+1)*5+x+1], pixels[y*3+x], sizeof(pixels[0]));
        hr = draw(context, image, pass == 1 ? &target_offset : NULL, pass == 2 ? &inverted : pass == 1 ? &crop : NULL);
        ok(hr == S_OK, "Identity draw returned %#lx.\n", hr);
        compare(target, readback, expected);
        winetest_pop_context();
    }
    /* A two-dimensional box filter gives the same horizontal averages for
     * these identical input rows, including mirrored top/bottom borders. */
    for (i = 0; i < 9; ++i) matrix[i] = 1;
    set_kernel(effect, 3,3,matrix);
    set_float(effect, D2D1_CONVOLVEMATRIX_PROP_DIVISOR, 9);
    set_uint(effect, D2D1_CONVOLVEMATRIX_PROP_BORDER_MODE, D2D1_PROPERTY_TYPE_ENUM, D2D1_BORDER_MODE_HARD);
    memset(matrix, 0, sizeof(matrix));
    ok(ID2D1Effect_GetValueSize(effect, D2D1_CONVOLVEMATRIX_PROP_KERNEL_MATRIX) == sizeof(matrix),
            "Kernel byte size differs.\n");
    hr = ID2D1Effect_GetValue(effect, D2D1_CONVOLVEMATRIX_PROP_KERNEL_MATRIX, D2D1_PROPERTY_TYPE_BLOB,
            (BYTE *)matrix, sizeof(matrix));
    ok(hr == S_OK, "Kernel readback returned %#lx.\n", hr);
    for (i = 0; i < 9; ++i) ok(matrix[i] == 1, "Kernel value %u changed after the caller buffer was overwritten.\n", i);
    memset(expected, 0, sizeof(expected));
    for (y = 0; y < 2; ++y)
        for (x = 0; x < 3; ++x)
        {
            static const float averages[] = {.8f/3,1.4f/3,2.0f/3};
            for (c = 0; c < 3; ++c) expected[y*5+x][c] = averages[x];
            expected[y*5+x][3] = 1;
        }
    hr = draw(context, image, NULL, NULL);
    ok(hr == S_OK, "Two-dimensional blur returned %#lx.\n", hr);
    compare(target, readback, expected);
    set_kernel(effect, 3,1,blur);
    set_float(effect, D2D1_CONVOLVEMATRIX_PROP_DIVISOR, 3);
    for (pass = 0; pass < 2; ++pass)
    {
        static const float soft_color[] = {.2f,.6f,1.4f,1.2f,.8f}, soft_alpha[] = {1,2,3,2,1};
        static const float hard_color[] = {.8f,1.4f,2.0f};
        winetest_push_context("blur border %u", pass);
        set_uint(effect, D2D1_CONVOLVEMATRIX_PROP_BORDER_MODE, D2D1_PROPERTY_TYPE_ENUM, pass);
        check_bounds(context, image, pass ? 0 : -1,0,pass ? 3 : 4,2);
        memset(expected, 0, sizeof(expected));
        for (y = 0; y < 2; ++y)
            for (x = 0; x < (pass ? 3 : 5); ++x)
            {
                float alpha = pass ? 1 : soft_alpha[x]/3;
                for (c = 0; c < 3; ++c) expected[y*5+x][c] = (pass ? hard_color[x] : soft_color[x])/3 * alpha;
                expected[y*5+x][3] = alpha;
            }
        hr = draw(context, image, NULL, NULL);
        ok(hr == S_OK, "Blur draw returned %#lx.\n", hr);
        compare(target, readback, expected);
        winetest_pop_context();
    }
    set_kernel(effect, 3,1,edge);
    set_float(effect, D2D1_CONVOLVEMATRIX_PROP_DIVISOR, 1);
    set_uint(effect, D2D1_CONVOLVEMATRIX_PROP_PRESERVE_ALPHA, D2D1_PROPERTY_TYPE_BOOL, TRUE);
    memset(expected, 0, sizeof(expected));
    for (y = 0; y < 2; ++y)
        for (x = 0; x < 3; ++x)
        {
            static const float difference[] = {.2f,.6f,.4f};
            for (c = 0; c < 3; ++c) expected[y*5+x][c] = difference[x];
            expected[y*5+x][3] = 1;
        }
    hr = draw(context, image, NULL, NULL);
    ok(hr == S_OK, "Directional kernel returned %#lx.\n", hr);
    compare(target, readback, expected);

    set_kernel(effect, 1,1,&single);
    vector.x = .5f; vector.y = 0;
    hr = ID2D1Effect_SetValue(effect, D2D1_CONVOLVEMATRIX_PROP_KERNEL_OFFSET, D2D1_PROPERTY_TYPE_VECTOR2,
            (const BYTE *)&vector, sizeof(vector));
    ok(hr == S_OK, "Offset returned %#lx.\n", hr);
    for (y = 0; y < 2; ++y)
        for (x = 0; x < 3; ++x)
            for (c = 0; c < 3; ++c) expected[y*5+x][c] = x == 2 ? .8f : (values[x]+values[x+1])/2;
    hr = draw(context, image, NULL, NULL);
    ok(hr == S_OK, "Half-pixel kernel offset returned %#lx.\n", hr);
    compare(target, readback, expected);
    vector.x = 0;
    ID2D1Effect_SetValue(effect, D2D1_CONVOLVEMATRIX_PROP_KERNEL_OFFSET, D2D1_PROPERTY_TYPE_VECTOR2,
            (const BYTE *)&vector, sizeof(vector));

    /* Filter straight RGB, preserve or filter alpha, then clamp before premultiplication. */
    for (i = 0; i < 6; ++i)
    {
        pixels[i][3] = (i+1)/8.0f;
        pixels[i][0] = pixels[i][1] = pixels[i][2] = .75f*pixels[i][3];
    }
    hr = ID2D1Bitmap1_CopyFromMemory(source, NULL, pixels, sizeof(pixels[0])*3);
    ok(hr == S_OK, "Source update returned %#lx.\n", hr);
    scalar = 2;
    set_kernel(effect, 1,1,&scalar);
    set_float(effect, D2D1_CONVOLVEMATRIX_PROP_BIAS, .25f);
    for (pass = 0; pass < 3; ++pass)
    {
        winetest_push_context("alpha/clamp %u", pass);
        set_uint(effect, D2D1_CONVOLVEMATRIX_PROP_PRESERVE_ALPHA, D2D1_PROPERTY_TYPE_BOOL, pass == 0);
        set_uint(effect, D2D1_CONVOLVEMATRIX_PROP_CLAMP_OUTPUT, D2D1_PROPERTY_TYPE_BOOL, pass == 2);
        memset(expected, 0, sizeof(expected));
        for (y = 0; y < 2; ++y)
            for (x = 0; x < 3; ++x)
            {
                float alpha = pass ? 2*pixels[y*3+x][3]+.25f : pixels[y*3+x][3];
                if (pass == 2 && alpha > 1) alpha = 1;
                for (c = 0; c < 3; ++c) expected[y*5+x][c] = (pass == 2 ? 1 : 1.75f)*alpha;
                expected[y*5+x][3] = alpha;
            }
        hr = draw(context, image, NULL, NULL);
        ok(hr == S_OK, "Alpha draw returned %#lx.\n", hr);
        compare(target, readback, expected);
        winetest_pop_context();
    }

    /* A negative-origin convolution feeding a mask must stay aligned with its source. */
    set_kernel(effect, 3,3,identity);
    set_float(effect, D2D1_CONVOLVEMATRIX_PROP_BIAS, 0);
    set_uint(effect, D2D1_CONVOLVEMATRIX_PROP_CLAMP_OUTPUT, D2D1_PROPERTY_TYPE_BOOL, FALSE);
    set_uint(effect, D2D1_CONVOLVEMATRIX_PROP_BORDER_MODE, D2D1_PROPERTY_TYPE_ENUM, D2D1_BORDER_MODE_SOFT);
    hr = ID2D1DeviceContext_CreateEffect(context, &CLSID_D2D1AlphaMask, &mask_effect);
    ok(hr == S_OK, "Mask effect returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    ID2D1Effect_SetInput(mask_effect, 0, image, TRUE);
    ID2D1Effect_SetInput(mask_effect, 1, (ID2D1Image *)source, TRUE);
    ID2D1Effect_GetOutput(mask_effect, &masked_image);
    check_bounds(context, masked_image, 0,0,3,2);
    memset(expected, 0, sizeof(expected));
    for (y = 0; y < 2; ++y)
        for (x = 0; x < 3; ++x)
            for (c = 0; c < 4; ++c) expected[y*5+x][c] = pixels[y*3+x][c]*pixels[y*3+x][3];
    hr = draw(context, masked_image, NULL, NULL);
    ok(hr == S_OK, "Convolution to mask returned %#lx.\n", hr);
    compare(target, readback, expected);

    hr = ID2D1DeviceContext_CreateEffect(context, &CLSID_D2D1ConvolveMatrix, &outer);
    ok(hr == S_OK, "Outer convolution returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    ID2D1Effect_SetInput(outer, 0, image, TRUE);
    ID2D1Effect_GetOutput(outer, &outer_image);
    check_bounds(context, outer_image, -2,-2,5,4);
    memset(expected, 0, sizeof(expected));
    for (y = 0; y < 2; ++y)
        for (x = 0; x < 3; ++x) memcpy(expected[y*5+x], pixels[y*3+x], sizeof(pixels[0]));
    hr = draw(context, outer_image, NULL, &crop);
    ok(hr == S_OK, "Nested convolution crop returned %#lx.\n", hr);
    compare(target, readback, expected);

    hr = ID2D1DeviceContext_CreateEffect(context, &CLSID_D2D1Histogram, &histogram);
    ok(hr == S_OK, "Histogram returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    set_uint(histogram, D2D1_HISTOGRAM_PROP_NUM_BINS, D2D1_PROPERTY_TYPE_UINT32, 4);
    set_uint(histogram, D2D1_HISTOGRAM_PROP_CHANNEL_SELECT, D2D1_PROPERTY_TYPE_ENUM, D2D1_CHANNEL_SELECTOR_A);
    ID2D1Effect_SetInput(histogram, 0, outer_image, TRUE);
    ID2D1Effect_GetOutput(histogram, &histogram_image);
    hr = draw(context, histogram_image, NULL, &crop);
    ok(hr == S_OK, "Histogram crop through negative origins returned %#lx.\n", hr);
    hr = ID2D1Effect_GetValue(histogram, D2D1_HISTOGRAM_PROP_HISTOGRAM_OUTPUT, D2D1_PROPERTY_TYPE_BLOB,
            (BYTE *)bins, sizeof(bins));
    ok(hr == S_OK, "Histogram bins returned %#lx.\n", hr);
    ok(fabsf(bins[0]-1/6.0f)<.0001f && fabsf(bins[1]-2/6.0f)<.0001f
            && fabsf(bins[2]-2/6.0f)<.0001f && fabsf(bins[3]-1/6.0f)<.0001f,
            "Histogram bins differ: %g,%g,%g,%g.\n", bins[0],bins[1],bins[2],bins[3]);

    ID2D1Effect_SetInput(effect, 0, outer_image, TRUE);
    hr = draw(context, image, NULL, NULL);
    ok(hr == D2DERR_CYCLIC_GRAPH, "Cycle returned %#lx.\n", hr);
    ID2D1Effect_SetInput(effect, 0, (ID2D1Image *)source, TRUE);
    hr = draw(context, image, NULL, &crop);
    ok(hr == S_OK, "Recovery returned %#lx.\n", hr);
    compare(target, readback, expected);

    vector.x = vector.y = .5f;
    hr = ID2D1Effect_SetValue(effect, D2D1_CONVOLVEMATRIX_PROP_KERNEL_UNIT_LENGTH, D2D1_PROPERTY_TYPE_VECTOR2,
            (const BYTE *)&vector, sizeof(vector));
    ok(hr == S_OK, "Half-DIP kernel unit returned %#lx.\n", hr);
    ID2D1DeviceContext_SetDpi(context, 192,192);
    check_bounds(context, image, -.5f,-.5f,2,1.5f);
    crop.right = 1.5f; crop.bottom = 1;
    hr = draw(context, image, NULL, &crop);
    ok(hr == S_OK, "192-DPI convolution returned %#lx.\n", hr);
    compare(target, readback, expected);
    ID2D1DeviceContext_SetUnitMode(context, D2D1_UNIT_MODE_PIXELS);
    vector.x = vector.y = 1;
    ID2D1Effect_SetValue(effect, D2D1_CONVOLVEMATRIX_PROP_KERNEL_UNIT_LENGTH, D2D1_PROPERTY_TYPE_VECTOR2,
            (const BYTE *)&vector, sizeof(vector));
    check_bounds(context, image, -1,-1,4,3);
    crop.right = 3; crop.bottom = 2;
    hr = draw(context, image, NULL, &crop);
    ok(hr == S_OK, "Pixel-unit convolution returned %#lx.\n", hr);
    compare(target, readback, expected);

    /* Zero divisor must not turn a zero-valued kernel into NaNs. */
    scalar = 0;
    set_kernel(effect, 1,1,&scalar);
    set_float(effect, D2D1_CONVOLVEMATRIX_PROP_DIVISOR, 0);
    memset(expected, 0, sizeof(expected));
    hr = draw(context, image, NULL, NULL);
    ok(hr == S_OK, "Zero divisor returned %#lx.\n", hr);
    compare(target, readback, expected);
done:
    if (context) ID2D1DeviceContext_SetTarget(context, NULL);
    if (effect) ID2D1Effect_SetInput(effect, 0, NULL, TRUE);
    if (histogram_image) ID2D1Image_Release(histogram_image);
    if (histogram) ID2D1Effect_Release(histogram);
    if (outer_image) ID2D1Image_Release(outer_image);
    if (outer) ID2D1Effect_Release(outer);
    if (masked_image) ID2D1Image_Release(masked_image);
    if (mask_effect) ID2D1Effect_Release(mask_effect);
    if (image) ID2D1Image_Release(image);
    if (effect) ID2D1Effect_Release(effect);
    if (readback) ID2D1Bitmap1_Release(readback);
    if (target) ID2D1Bitmap1_Release(target);
    if (source) ID2D1Bitmap1_Release(source);
    if (properties) ID2D1Properties_Release(properties);
    if (context) ID2D1DeviceContext_Release(context);
    if (device) ID2D1Device_Release(device);
    if (dxgi) IDXGIDevice_Release(dxgi);
    if (d3d) ID3D11Device_Release(d3d);
    if (factory) ID2D1Factory1_Release(factory);
    CoUninitialize();
}
