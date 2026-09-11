/* WIC source ownership, pixel conversion, transforms, and source graph leaves.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <math.h>
#include "d2d1_1.h"
#include "d2d1effects_2.h"
#include "d3d11.h"
#include "wincodec.h"
#include "wine/test.h"

struct source
{
    IWICBitmapSource IWICBitmapSource_iface;
    LONG refs;
    UINT copies, width, height, bpp;
    double dpi_x, dpi_y;
    const GUID *format;
    const void *pixels;
};

static struct source *impl(IWICBitmapSource *iface)
{
    return CONTAINING_RECORD(iface, struct source, IWICBitmapSource_iface);
}

static HRESULT WINAPI source_query(IWICBitmapSource *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IWICBitmapSource)) return E_NOINTERFACE;
    IWICBitmapSource_AddRef(iface);
    *out = iface;
    return S_OK;
}

static ULONG WINAPI source_addref(IWICBitmapSource *iface) { return ++impl(iface)->refs; }
static ULONG WINAPI source_release(IWICBitmapSource *iface) { return --impl(iface)->refs; }
static HRESULT WINAPI source_size(IWICBitmapSource *iface, UINT *width, UINT *height)
{
    *width = impl(iface)->width; *height = impl(iface)->height;
    return S_OK;
}
static HRESULT WINAPI source_format(IWICBitmapSource *iface, GUID *format)
{
    *format = *impl(iface)->format;
    return S_OK;
}
static HRESULT WINAPI source_resolution(IWICBitmapSource *iface, double *x, double *y)
{
    *x = impl(iface)->dpi_x; *y = impl(iface)->dpi_y;
    return S_OK;
}
static HRESULT WINAPI source_palette(IWICBitmapSource *iface, IWICPalette *palette)
{
    return WINCODEC_ERR_PALETTEUNAVAILABLE;
}
static HRESULT WINAPI source_pixels(IWICBitmapSource *iface, const WICRect *rect, UINT stride, UINT size, BYTE *data)
{
    struct source *source = impl(iface);
    WICRect full = {0, 0, source->width, source->height};
    UINT y;
    ++source->copies;
    if (!rect) rect = &full;
    if (rect->X < 0 || rect->Y < 0 || rect->Width <= 0 || rect->Height <= 0
            || rect->X + rect->Width > source->width || rect->Y + rect->Height > source->height)
        return E_INVALIDARG;
    if (stride < rect->Width * source->bpp || size < (rect->Height-1)*stride + rect->Width*source->bpp)
        return E_INVALIDARG;
    for (y = 0; y < rect->Height; ++y)
        memcpy(data + y*stride, (const BYTE *)source->pixels
                + ((y+rect->Y)*source->width+rect->X)*source->bpp, rect->Width*source->bpp);
    return S_OK;
}
static const IWICBitmapSourceVtbl source_vtbl =
{
    source_query, source_addref, source_release, source_size, source_format,
    source_resolution, source_palette, source_pixels,
};

static HRESULT CALLBACK unused_factory(IUnknown **out)
{
    *out = NULL;
    ok(0, "Metadata inspection instantiated an effect.\n");
    return E_NOTIMPL;
}

static void test_empty_inputs(ID2D1Factory1 *factory)
{
    static const CLSID clsid = {0x28f768d9,0x1a6f,0x42da,{0xaa,0x62,0xb0,0x31,0x65,0x74,0xc0,0x80}};
    static const WCHAR *inputs[] =
    {
        L"<Inputs/>", L"<Inputs minimum='0'/>", L"<Inputs maximum='0'/>",
        L"<Inputs minimum='0' maximum='0'/>", L"<Inputs minimum='0' maximum='0'></Inputs>",
    };
    static const WCHAR prefix[] = L"<?xml version='1.0'?><Effect>"
        L"<Property name='DisplayName' type='string' value='Source metadata test'/>"
        L"<Property name='Author' type='string' value='Wine'/>"
        L"<Property name='Category' type='string' value='Test'/>"
        L"<Property name='Description' type='string' value='Empty input list'/>";
    ID2D1Properties *properties;
    WCHAR xml[1024];
    UINT i, count;
    HRESULT hr;
    for (i = 0; i < ARRAY_SIZE(inputs); ++i)
    {
        winetest_push_context("empty Inputs %u",i);
        swprintf(xml,ARRAY_SIZE(xml),L"%ls%ls<Property name='Value' type='float' value='1'/></Effect>",prefix,inputs[i]);
        hr = ID2D1Factory1_RegisterEffectFromString(factory,&clsid,xml,NULL,0,unused_factory);
        ok(hr == S_OK, "Empty input registration returned %#lx.\n", hr);
        if (SUCCEEDED(hr))
        {
            hr = ID2D1Factory1_GetEffectProperties(factory,&clsid,&properties);
            ok(hr == S_OK, "Metadata returned %#lx.\n", hr);
            if (SUCCEEDED(hr))
            {
                hr = ID2D1Properties_GetValue(properties,D2D1_PROPERTY_INPUTS,D2D1_PROPERTY_TYPE_ARRAY,
                        (BYTE *)&count,sizeof(count));
                ok(hr == S_OK && !count, "Empty input count differs, hr %#lx.\n", hr);
                ID2D1Properties_Release(properties);
            }
            ID2D1Factory1_UnregisterEffect(factory,&clsid);
        }
        winetest_pop_context();
    }
}

static HRESULT set_source(ID2D1Effect *effect, IWICBitmapSource *source)
{
    return ID2D1Effect_SetValue(effect, D2D1_BITMAPSOURCE_PROP_WIC_BITMAP_SOURCE,
            D2D1_PROPERTY_TYPE_IUNKNOWN, (const BYTE *)&source, sizeof(source));
}

static void set_uint(ID2D1Effect *effect, UINT prop, UINT value)
{
    HRESULT hr = ID2D1Effect_SetValue(effect, prop, prop == D2D1_BITMAPSOURCE_PROP_ENABLE_DPI_CORRECTION
            ? D2D1_PROPERTY_TYPE_BOOL : D2D1_PROPERTY_TYPE_ENUM, (const BYTE *)&value, sizeof(value));
    ok(hr == S_OK, "Setting property %u returned %#lx.\n", prop, hr);
}

static void set_scale(ID2D1Effect *effect, float x, float y)
{
    D2D1_VECTOR_2F value = {x, y};
    HRESULT hr = ID2D1Effect_SetValue(effect, D2D1_BITMAPSOURCE_PROP_SCALE,
            D2D1_PROPERTY_TYPE_VECTOR2, (const BYTE *)&value, sizeof(value));
    ok(hr == S_OK, "Setting scale returned %#lx.\n", hr);
}

static HRESULT draw(ID2D1DeviceContext *context, ID2D1Image *image, const D2D1_POINT_2F *offset,
        const D2D1_RECT_F *crop)
{
    const D2D1_COLOR_F clear = {0};
    ID2D1DeviceContext_BeginDraw(context);
    ID2D1DeviceContext_Clear(context, &clear);
    ID2D1DeviceContext_DrawImage(context, image, offset, crop, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
            D2D1_COMPOSITE_MODE_SOURCE_OVER);
    return ID2D1DeviceContext_EndDraw(context, NULL, NULL);
}

/* The entire 8x8 target is checked, including pixels outside the source extent. */
static void compare(ID2D1Bitmap1 *target, ID2D1Bitmap1 *readback, const float expected[64][4])
{
    D2D1_MAPPED_RECT mapped;
    UINT x, y, c;
    float value;
    HRESULT hr = ID2D1Bitmap1_CopyFromBitmap(readback, NULL, (ID2D1Bitmap *)target, NULL);
    ok(hr == S_OK, "Readback copy returned %#lx.\n", hr);
    hr = ID2D1Bitmap1_Map(readback, D2D1_MAP_OPTIONS_READ, &mapped);
    ok(hr == S_OK, "Readback map returned %#lx.\n", hr);
    if (FAILED(hr)) return;
    for (y = 0; y < 8; ++y)
        for (x = 0; x < 8; ++x)
            for (c = 0; c < 4; ++c)
            {
                memcpy(&value, mapped.bits + y*mapped.pitch + (x*4+c)*sizeof(float), sizeof(value));
                ok(fabsf(value-expected[y*8+x][c]) < .0001f, "Pixel %u,%u channel %u: %g, expected %g.\n",
                        x,y,c,value,expected[y*8+x][c]);
            }
    ID2D1Bitmap1_Unmap(readback);
}

START_TEST(bitmap_source)
{
    static const float pixels[6][4] =
    {
        {1,0,0,1}, {0,.5f,0,.5f}, {0,0,.25f,.25f},
        {1.5f,-.25f,.5f,1}, {.125f,.25f,.375f,.5f}, {0,0,0,0},
    };
    /* Rotation is followed by a horizontal flip for the combined modes. */
    static const UINT orientations[8][6] =
    {
        {0,1,2,3,4,5}, {2,1,0,5,4,3}, {5,4,3,2,1,0}, {3,4,5,0,1,2},
        {5,2,4,1,3,0}, {3,0,4,1,5,2}, {0,3,1,4,2,5}, {2,5,1,4,0,3},
    };
    static const BYTE bgra[3][4] = {{0,0,255,255}, {0,255,0,128}, {255,0,0,64}};
    static const BYTE bgr[2][3] = {{0,128,255}, {255,64,0}};
    static const WORD rgba16[2][4] = {{65535,32768,0,65535}, {0,65535,16384,32768}};
    struct source source = {{&source_vtbl},1,0,3,2,16,96,96,&GUID_WICPixelFormat128bppPRGBAFloat,pixels};
    D2D1_BITMAP_PROPERTIES1 desc = {{DXGI_FORMAT_R32G32B32A32_FLOAT, D2D1_ALPHA_MODE_PREMULTIPLIED},
            96,96,D2D1_BITMAP_OPTIONS_TARGET,NULL};
    ID2D1Factory1 *factory = NULL;
    ID2D1Properties *properties = NULL;
    ID3D11Device *d3d = NULL;
    IDXGIDevice *dxgi = NULL;
    ID2D1Device *device = NULL;
    ID2D1DeviceContext *context = NULL;
    ID2D1Effect *effect = NULL, *contrast = NULL;
    ID2D1Image *image = NULL, *adjusted = NULL;
    ID2D1Bitmap1 *target = NULL, *readback = NULL;
    IWICBitmapSource *returned = NULL;
    IUnknown *wrong;
    D2D1_SIZE_U size = {8,8};
    D2D1_RECT_F bounds, crop = {1,0,3,2};
    D2D1_POINT_2F offset = {2,3};
    D2D1_VECTOR_2F scale;
    float expected[64][4], amount = 0;
    UINT orientation, x,y,c,value,width,height,copies;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL, (void **)&factory);
    ok(hr == S_OK, "Factory returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    test_empty_inputs(factory);
    hr = ID2D1Factory1_GetEffectProperties(factory, &CLSID_D2D1BitmapSource, &properties);
    ok(hr == S_OK, "Bitmap Source metadata returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    ok(ID2D1Properties_GetPropertyCount(properties) == 6, "Wrong property count.\n");
    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
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
    hr = ID2D1DeviceContext_CreateEffect(context,&CLSID_D2D1BitmapSource,&effect);
    ok(hr == S_OK, "Bitmap Source returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    ok(!ID2D1Effect_GetInputCount(effect), "Source has graph inputs.\n");
    hr = ID2D1Effect_GetValue(effect, D2D1_BITMAPSOURCE_PROP_WIC_BITMAP_SOURCE,
            D2D1_PROPERTY_TYPE_IUNKNOWN, (BYTE *)&returned, sizeof(returned));
    ok(hr == S_OK && !returned, "Default source differs.\n");
    hr = ID2D1Effect_GetValue(effect, D2D1_BITMAPSOURCE_PROP_SCALE,
            D2D1_PROPERTY_TYPE_VECTOR2, (BYTE *)&scale, sizeof(scale));
    ok(hr == S_OK && scale.x == 1 && scale.y == 1, "Default scale differs.\n");
    for (x = 2; x < 6; ++x)
    {
        hr = ID2D1Effect_GetValue(effect,x,x == 3 ? D2D1_PROPERTY_TYPE_BOOL : D2D1_PROPERTY_TYPE_ENUM,
                (BYTE *)&value,sizeof(value));
        ok(hr == S_OK && value == (x == 3 ? 0 : 1), "Default property %u differs, hr %#lx.\n", x,hr);
    }
    hr = ID2D1DeviceContext_CreateBitmap(context,size,NULL,0,&desc,&target);
    ok(hr == S_OK, "Target returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_CANNOT_DRAW | D2D1_BITMAP_OPTIONS_CPU_READ;
    hr = ID2D1DeviceContext_CreateBitmap(context,size,NULL,0,&desc,&readback);
    ok(hr == S_OK, "Readback returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    ID2D1DeviceContext_SetTarget(context,(ID2D1Image *)target);
    ID2D1Effect_GetOutput(effect,&image);
    hr = draw(context,image,NULL,NULL);
    ok(FAILED(hr), "Missing source draw returned %#lx.\n", hr);
    hr = set_source(effect,&source.IWICBitmapSource_iface);
    ok(hr == S_OK && source.refs > 1, "Source not retained, hr %#lx, refs %ld.\n", hr,source.refs);
    hr = ID2D1Effect_GetValue(effect,D2D1_BITMAPSOURCE_PROP_WIC_BITMAP_SOURCE,
            D2D1_PROPERTY_TYPE_IUNKNOWN,(BYTE *)&returned,sizeof(returned));
    ok(hr == S_OK && returned == &source.IWICBitmapSource_iface, "Wrong property source.\n");
    ok(source.refs > 2, "GetValue did not retain the returned source.\n");
    if (returned) IWICBitmapSource_Release(returned);
    wrong = (IUnknown *)factory;
    hr = ID2D1Effect_SetValue(effect,D2D1_BITMAPSOURCE_PROP_WIC_BITMAP_SOURCE,
            D2D1_PROPERTY_TYPE_IUNKNOWN,(BYTE *)&wrong,sizeof(wrong));
    ok(FAILED(hr), "Non-WIC property accepted.\n");
    scale = (D2D1_VECTOR_2F){-1,1};
    hr = ID2D1Effect_SetValue(effect,D2D1_BITMAPSOURCE_PROP_SCALE,D2D1_PROPERTY_TYPE_VECTOR2,(BYTE *)&scale,sizeof(scale));
    ok(FAILED(hr), "Negative scale accepted.\n");
    value = 0;
    hr = ID2D1Effect_SetValue(effect,D2D1_BITMAPSOURCE_PROP_ORIENTATION,D2D1_PROPERTY_TYPE_ENUM,(BYTE *)&value,sizeof(value));
    ok(FAILED(hr), "Invalid orientation accepted.\n");
    for (orientation = 1; orientation <= 8; ++orientation)
    {
        winetest_push_context("orientation %u",orientation);
        set_uint(effect,D2D1_BITMAPSOURCE_PROP_ORIENTATION,orientation);
        width = orientation >= 5 ? 2 : 3; height = orientation >= 5 ? 3 : 2;
        copies = source.copies;
        hr = ID2D1DeviceContext_GetImageLocalBounds(context,image,&bounds);
        ok(hr == S_OK && bounds.left == 0 && bounds.top == 0 && bounds.right == width && bounds.bottom == height,
                "Wrong bounds, hr %#lx.\n", hr);
        ok(source.copies == copies, "Bounds decoded pixels.\n");
        memset(expected,0,sizeof(expected));
        for (y = 0; y < height; ++y)
            for (x = 0; x < width; ++x)
                memcpy(expected[y*8+x],pixels[orientations[orientation-1][y*width+x]],sizeof(pixels[0]));
        hr = draw(context,image,NULL,NULL);
        ok(hr == S_OK, "Source draw returned %#lx.\n", hr);
        compare(target,readback,expected);
        copies = source.copies;
        hr = draw(context,image,NULL,NULL);
        ok(hr == S_OK, "Cached source draw returned %#lx.\n", hr);
        ok(source.copies == copies, "Repeated draw decoded the source again.\n");
        compare(target,readback,expected);
        winetest_pop_context();
    }
    set_uint(effect,D2D1_BITMAPSOURCE_PROP_ORIENTATION,1);
    set_uint(effect,D2D1_BITMAPSOURCE_PROP_INTERPOLATION_MODE,0);
    set_scale(effect,2,2);
    memset(expected,0,sizeof(expected));
    for (y = 0; y < 4; ++y)
        for (x = 0; x < 6; ++x)
            memcpy(expected[y*8+x],pixels[(y/2)*3+x/2],sizeof(pixels[0]));
    hr = draw(context,image,NULL,NULL);
    ok(hr == S_OK, "Nearest scaling returned %#lx.\n", hr);
    compare(target,readback,expected);

    /* Half-pixel sample centers: doubling two opaque pixels gives 0,1/4,3/4,1. */
    source.width = 2; source.height = 1;
    source.pixels = (const float[2][4]){{0,0,0,1},{1,1,1,1}};
    set_source(effect,&source.IWICBitmapSource_iface);
    set_uint(effect,D2D1_BITMAPSOURCE_PROP_INTERPOLATION_MODE,1);
    set_scale(effect,2,1);
    memset(expected,0,sizeof(expected));
    for (x = 0; x < 4; ++x)
    {
        for (c = 0; c < 3; ++c) expected[x][c] = x == 0 ? 0 : x == 1 ? .25f : x == 2 ? .75f : 1;
        expected[x][3] = 1;
    }
    hr = draw(context,image,NULL,NULL);
    ok(hr == S_OK, "Linear scaling returned %#lx.\n", hr);
    compare(target,readback,expected);
    set_scale(effect,.5f,1);
    memset(expected,0,sizeof(expected));
    expected[0][0] = expected[0][1] = expected[0][2] = .5f; expected[0][3] = 1;
    hr = draw(context,image,NULL,NULL);
    ok(hr == S_OK, "Linear reduction returned %#lx.\n", hr);
    compare(target,readback,expected);

    source.width = 3; source.height = 2; source.pixels = pixels;
    set_source(effect,&source.IWICBitmapSource_iface);
    set_scale(effect,1,1);
    memset(expected,0,sizeof(expected));
    for (y = 0; y < 2; ++y)
        for (x = 0; x < 2; ++x) memcpy(expected[(y+3)*8+x+2],pixels[y*3+x+1],sizeof(pixels[0]));
    hr = draw(context,image,&offset,&crop);
    ok(hr == S_OK, "Cropped source draw returned %#lx.\n", hr);
    compare(target,readback,expected);

    /* A property-backed source can be nested inside an image-input effect. */
    hr = ID2D1DeviceContext_CreateEffect(context,&CLSID_D2D1Contrast,&contrast);
    ok(hr == S_OK, "Contrast returned %#lx.\n", hr);
    if (FAILED(hr)) goto done;
    ID2D1Effect_SetValue(contrast,D2D1_CONTRAST_PROP_CONTRAST,D2D1_PROPERTY_TYPE_FLOAT,(BYTE *)&amount,sizeof(amount));
    ID2D1Effect_SetInput(contrast,0,image,TRUE);
    ID2D1Effect_GetOutput(contrast,&adjusted);
    memset(expected,0,sizeof(expected));
    for (y = 0; y < 2; ++y)
        for (x = 0; x < 3; ++x) memcpy(expected[y*8+x],pixels[y*3+x],sizeof(pixels[0]));
    hr = draw(context,adjusted,NULL,NULL);
    ok(hr == S_OK, "Nested source draw returned %#lx.\n", hr);
    compare(target,readback,expected);

    ID2D1DeviceContext_SetDpi(context,192,192);
    hr = ID2D1DeviceContext_GetImageLocalBounds(context,image,&bounds);
    ok(hr == S_OK && bounds.right == 1.5f && bounds.bottom == 1, "Uncorrected DIP bounds differ.\n");
    hr = draw(context,image,NULL,NULL);
    ok(hr == S_OK, "Uncorrected 192-DPI draw returned %#lx.\n", hr);
    compare(target,readback,expected);
    set_uint(effect,D2D1_BITMAPSOURCE_PROP_ENABLE_DPI_CORRECTION,TRUE);
    set_uint(effect,D2D1_BITMAPSOURCE_PROP_INTERPOLATION_MODE,0);
    hr = ID2D1DeviceContext_GetImageLocalBounds(context,image,&bounds);
    ok(hr == S_OK && bounds.right == 3 && bounds.bottom == 2, "Corrected DIP bounds differ.\n");
    memset(expected,0,sizeof(expected));
    for (y = 0; y < 4; ++y)
        for (x = 0; x < 6; ++x) memcpy(expected[y*8+x],pixels[(y/2)*3+x/2],sizeof(pixels[0]));
    hr = draw(context,image,NULL,NULL);
    ok(hr == S_OK, "DPI-corrected draw returned %#lx.\n", hr);
    compare(target,readback,expected);
    ID2D1DeviceContext_SetUnitMode(context,D2D1_UNIT_MODE_PIXELS);
    memset(expected,0,sizeof(expected));
    for (y = 0; y < 2; ++y)
        for (x = 0; x < 3; ++x) memcpy(expected[y*8+x],pixels[y*3+x],sizeof(pixels[0]));
    hr = draw(context,image,NULL,NULL);
    ok(hr == S_OK, "Pixel-unit draw returned %#lx.\n", hr);
    compare(target,readback,expected);

    source.format = &GUID_WICPixelFormat32bppBGRA; source.bpp = 4; source.height = 1; source.pixels = bgra;
    set_source(effect,&source.IWICBitmapSource_iface);
    memset(expected,0,sizeof(expected));
    for (x = 0; x < 3; ++x)
    {
        expected[x][3] = bgra[x][3]/255.0f;
        for (c = 0; c < 3; ++c) expected[x][c] = bgra[x][2-c]/255.0f * expected[x][3];
    }
    hr = draw(context,image,NULL,NULL);
    ok(hr == S_OK, "BGRA conversion returned %#lx.\n", hr);
    compare(target,readback,expected);
    source.format = &GUID_WICPixelFormat24bppBGR; source.bpp = 3; source.width = 2; source.pixels = bgr;
    set_source(effect,&source.IWICBitmapSource_iface);
    memset(expected,0,sizeof(expected));
    for (x = 0; x < 2; ++x)
    {
        for (c = 0; c < 3; ++c) expected[x][c] = bgr[x][2-c]/255.0f;
        expected[x][3] = 1;
    }
    hr = draw(context,image,NULL,NULL);
    ok(hr == S_OK, "WIC BGR conversion returned %#lx.\n", hr);
    compare(target,readback,expected);
    source.format = &GUID_WICPixelFormat64bppRGBA; source.bpp = 8; source.pixels = rgba16;
    set_source(effect,&source.IWICBitmapSource_iface);
    memset(expected,0,sizeof(expected));
    for (x = 0; x < 2; ++x)
    {
        expected[x][3] = rgba16[x][3]/65535.0f;
        for (c = 0; c < 3; ++c) expected[x][c] = rgba16[x][c]/65535.0f * expected[x][3];
    }
    hr = draw(context,image,NULL,NULL);
    ok(hr == S_OK, "16-bit conversion returned %#lx.\n", hr);
    compare(target,readback,expected);
    set_scale(effect,0,1);
    hr = draw(context,image,NULL,NULL);
    ok(hr == S_OK, "Zero-width draw returned %#lx.\n", hr);
    memset(expected,0,sizeof(expected));
    compare(target,readback,expected);
    hr = set_source(effect,NULL);
    ok(hr == S_OK && source.refs == 1, "Source was not released, hr %#lx, refs %ld.\n", hr,source.refs);
done:
    if (context) ID2D1DeviceContext_SetTarget(context,NULL);
    if (adjusted) ID2D1Image_Release(adjusted);
    if (contrast) ID2D1Effect_Release(contrast);
    if (image) ID2D1Image_Release(image);
    if (effect) ID2D1Effect_Release(effect);
    if (readback) ID2D1Bitmap1_Release(readback);
    if (target) ID2D1Bitmap1_Release(target);
    if (properties) ID2D1Properties_Release(properties);
    if (context) ID2D1DeviceContext_Release(context);
    if (device) ID2D1Device_Release(device);
    if (dxgi) IDXGIDevice_Release(dxgi);
    if (d3d) ID3D11Device_Release(d3d);
    if (factory) ID2D1Factory1_Release(factory);
    ok(source.refs == 1, "Leaked source, refs %ld.\n", source.refs);
    CoUninitialize();
}
