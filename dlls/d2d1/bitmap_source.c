/* Direct2D image sources backed by Windows Imaging Component pixels.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "d2d1_private.h"
#include "wincodec.h"

WINE_DEFAULT_DEBUG_CHANNEL(d2d);

struct bitmap_source_effect
{
    ID2D1EffectImpl ID2D1EffectImpl_iface;
    LONG refcount;
    IWICBitmapSource *source;
    D2D1_VECTOR_2F scale;
    UINT interpolation, alpha_mode, orientation;
    BOOL dpi_correction;
    ID2D1Bitmap *cached;
    float cached_dpi_x, cached_dpi_y;
};

static struct bitmap_source_effect *impl_from_ID2D1EffectImpl(ID2D1EffectImpl *iface)
{
    return CONTAINING_RECORD(iface, struct bitmap_source_effect, ID2D1EffectImpl_iface);
}

static void invalidate(struct bitmap_source_effect *effect)
{
    if (effect->cached) ID2D1Bitmap_Release(effect->cached);
    effect->cached = NULL;
}

static HRESULT STDMETHODCALLTYPE bitmap_source_QueryInterface(ID2D1EffectImpl *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_ID2D1EffectImpl) && !IsEqualGUID(iid, &IID_IUnknown)) return E_NOINTERFACE;
    *out = iface;
    ID2D1EffectImpl_AddRef(iface);
    return S_OK;
}

static ULONG STDMETHODCALLTYPE bitmap_source_AddRef(ID2D1EffectImpl *iface)
{
    return InterlockedIncrement(&impl_from_ID2D1EffectImpl(iface)->refcount);
}

static ULONG STDMETHODCALLTYPE bitmap_source_Release(ID2D1EffectImpl *iface)
{
    struct bitmap_source_effect *effect = impl_from_ID2D1EffectImpl(iface);
    ULONG refcount = InterlockedDecrement(&effect->refcount);
    if (!refcount)
    {
        invalidate(effect);
        if (effect->source) IWICBitmapSource_Release(effect->source);
        free(effect);
    }
    return refcount;
}

static HRESULT STDMETHODCALLTYPE bitmap_source_Initialize(ID2D1EffectImpl *iface,
        ID2D1EffectContext *context, ID2D1TransformGraph *graph)
{
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE bitmap_source_PrepareForRender(ID2D1EffectImpl *iface, D2D1_CHANGE_TYPE change)
{
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE bitmap_source_SetGraph(ID2D1EffectImpl *iface, ID2D1TransformGraph *graph)
{
    return E_NOTIMPL;
}

static const ID2D1EffectImplVtbl bitmap_source_vtbl =
{
    bitmap_source_QueryInterface, bitmap_source_AddRef, bitmap_source_Release,
    bitmap_source_Initialize, bitmap_source_PrepareForRender, bitmap_source_SetGraph,
};

static HRESULT CALLBACK bitmap_source_factory(IUnknown **out)
{
    struct bitmap_source_effect *effect = calloc(1, sizeof(*effect));
    if (!effect) return E_OUTOFMEMORY;
    effect->ID2D1EffectImpl_iface.lpVtbl = &bitmap_source_vtbl;
    effect->refcount = 1;
    effect->scale.x = effect->scale.y = 1;
    effect->interpolation = D2D1_BITMAPSOURCE_INTERPOLATION_MODE_LINEAR;
    effect->alpha_mode = D2D1_BITMAPSOURCE_ALPHA_MODE_PREMULTIPLIED;
    effect->orientation = D2D1_BITMAPSOURCE_ORIENTATION_DEFAULT;
    *out = (IUnknown *)&effect->ID2D1EffectImpl_iface;
    return S_OK;
}

static HRESULT property_get(const void *value, UINT size, BYTE *data, UINT data_size, UINT *actual)
{
    if (actual) *actual = size;
    if (!data) return S_OK;
    if (data_size < size) return E_NOT_SUFFICIENT_BUFFER;
    memcpy(data, value, size);
    return S_OK;
}

#define FIELD_PROPERTY(name, field, type, valid) \
static HRESULT CALLBACK name##_get(const IUnknown *iface, BYTE *data, UINT size, UINT *actual) \
{ \
    const struct bitmap_source_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface); \
    return property_get(&effect->field, sizeof(effect->field), data, size, actual); \
} \
static HRESULT CALLBACK name##_set(IUnknown *iface, const BYTE *data, UINT size) \
{ \
    struct bitmap_source_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface); \
    type value; \
    if (!data || size != sizeof(value)) return E_INVALIDARG; \
    memcpy(&value, data, size); \
    if (!(valid)) return E_INVALIDARG; \
    effect->field = value; \
    invalidate(effect); \
    return S_OK; \
}

FIELD_PROPERTY(scale, scale, D2D1_VECTOR_2F, isfinite(value.x) && isfinite(value.y) && value.x >= 0 && value.y >= 0)
FIELD_PROPERTY(interpolation, interpolation, UINT, value <= 2 || value == 6 || value == 7)
FIELD_PROPERTY(dpi, dpi_correction, BOOL, TRUE)
FIELD_PROPERTY(alpha, alpha_mode, UINT, value == 1 || value == 2)
FIELD_PROPERTY(orientation, orientation, UINT, value >= 1 && value <= 8)
#undef FIELD_PROPERTY

static HRESULT CALLBACK source_get(const IUnknown *iface, BYTE *data, UINT size, UINT *actual)
{
    const struct bitmap_source_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    HRESULT hr = property_get(&effect->source, sizeof(effect->source), data, size, actual);
    if (SUCCEEDED(hr) && data && effect->source) IWICBitmapSource_AddRef(effect->source);
    return hr;
}

static HRESULT CALLBACK source_set(IUnknown *iface, const BYTE *data, UINT size)
{
    struct bitmap_source_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    IWICBitmapSource *source = NULL;
    IUnknown *value;
    HRESULT hr;
    if (!data || size != sizeof(value)) return E_INVALIDARG;
    memcpy(&value, data, size);
    if (value && FAILED(hr = IUnknown_QueryInterface(value, &IID_IWICBitmapSource, (void **)&source))) return hr;
    if (effect->source) IWICBitmapSource_Release(effect->source);
    effect->source = source;
    invalidate(effect);
    return S_OK;
}

void d2d_bitmap_source_init_builtin(struct d2d_factory *factory)
{
    static const WCHAR description[] = L"<?xml version='1.0'?><Effect>"
        L"<Property name='DisplayName' type='string' value='Bitmap Source'/>"
        L"<Property name='Author' type='string' value='The Wine Project'/>"
        L"<Property name='Category' type='string' value='Generators'/>"
        L"<Property name='Description' type='string' value='Loads and transforms a WIC bitmap source'/>"
        L"<Inputs minimum='0' maximum='0'/>"
        L"<Property name='WicBitmapSource' type='iunknown'/>"
        L"<Property name='Scale' type='vector2' value='(1,1)'/>"
        L"<Property name='InterpolationMode' type='enum' value='1'/>"
        L"<Property name='EnableDPICorrection' type='bool' value='false'/>"
        L"<Property name='AlphaMode' type='enum' value='1'/>"
        L"<Property name='Orientation' type='enum' value='1'/></Effect>";
    static const D2D1_PROPERTY_BINDING bindings[] =
    {
        {L"WicBitmapSource", source_set, source_get}, {L"Scale", scale_set, scale_get},
        {L"InterpolationMode", interpolation_set, interpolation_get},
        {L"EnableDPICorrection", dpi_set, dpi_get}, {L"AlphaMode", alpha_set, alpha_get},
        {L"Orientation", orientation_set, orientation_get},
    };
    HRESULT hr;
    if (FAILED(hr = d2d_factory_register_builtin_effect(factory, &CLSID_D2D1BitmapSource,
            description, bindings, ARRAY_SIZE(bindings), bitmap_source_factory)))
        WARN("Failed to register Bitmap Source, hr %#lx.\n", hr);
}

/* Preserve encoded channel values and native precision. Converting an 8-bit WIC
 * image to a float WIC format would also apply a color-space conversion. */
static HRESULT load_pixels(IWICBitmapSource *source, D2D1_SIZE_U size, float **pixels)
{
    static const struct
    {
        const GUID *format;
        UINT component_size;
        BOOL bgra, premultiplied, opaque;
    } formats[] =
    {
        {&GUID_WICPixelFormat32bppPBGRA, 1, TRUE, TRUE},
        {&GUID_WICPixelFormat32bppPRGBA, 1, FALSE, TRUE},
        {&GUID_WICPixelFormat32bppBGRA, 1, TRUE},
        {&GUID_WICPixelFormat32bppRGBA, 1},
        {&GUID_WICPixelFormat32bppBGR, 1, TRUE, FALSE, TRUE},
        {&GUID_WICPixelFormat32bppRGB, 1, FALSE, FALSE, TRUE},
        {&GUID_WICPixelFormat64bppRGBA, 2},
        {&GUID_WICPixelFormat64bppPRGBA, 2, FALSE, TRUE},
        {&GUID_WICPixelFormat128bppRGBAFloat, 4},
        {&GUID_WICPixelFormat128bppPRGBAFloat, 4, FALSE, TRUE},
    };
    IWICImagingFactory *factory = NULL;
    IWICFormatConverter *converter = NULL;
    GUID format;
    BYTE *data = NULL;
    float *result = NULL, value;
    size_t count = (size_t)size.width * size.height, i;
    UINT f, c, channel, stride;
    HRESULT hr;

    *pixels = NULL;
    if (FAILED(hr = IWICBitmapSource_GetPixelFormat(source, &format))) return hr;
    for (f = 0; f < ARRAY_SIZE(formats); ++f)
        if (IsEqualGUID(&format, formats[f].format)) break;
    if (f == ARRAY_SIZE(formats))
    {
        /* These common decoder formats have no component above eight bits. */
        if (!IsEqualGUID(&format, &GUID_WICPixelFormat24bppBGR)
                && !IsEqualGUID(&format, &GUID_WICPixelFormat24bppRGB)
                && !IsEqualGUID(&format, &GUID_WICPixelFormat8bppGray)
                && !IsEqualGUID(&format, &GUID_WICPixelFormat8bppIndexed)
                && !IsEqualGUID(&format, &GUID_WICPixelFormat4bppIndexed)
                && !IsEqualGUID(&format, &GUID_WICPixelFormat2bppIndexed)
                && !IsEqualGUID(&format, &GUID_WICPixelFormat1bppIndexed))
            return D2DERR_UNSUPPORTED_PIXEL_FORMAT;
        if (FAILED(hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                &IID_IWICImagingFactory, (void **)&factory))) goto done;
        if (FAILED(hr = IWICImagingFactory_CreateFormatConverter(factory, &converter))) goto done;
        if (FAILED(hr = IWICFormatConverter_Initialize(converter, source, &GUID_WICPixelFormat32bppPBGRA,
                WICBitmapDitherTypeNone, NULL, 0, WICBitmapPaletteTypeCustom))) goto done;
        source = (IWICBitmapSource *)converter;
        f = 0;
    }
    if (count > UINT_MAX / (4 * formats[f].component_size) || count > SIZE_MAX / (4 * sizeof(float)))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    if (!(data = malloc(count * 4 * formats[f].component_size)) || !(result = malloc(count * 4 * sizeof(float))))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    stride = size.width * 4 * formats[f].component_size;
    if (FAILED(hr = IWICBitmapSource_CopyPixels(source, NULL, stride, stride * size.height, data))) goto done;
    for (i = 0; i < count; ++i)
    {
        for (c = 0; c < 4; ++c)
        {
            channel = formats[f].bgra && !(c & 1) ? 2 - c : c;
            if (formats[f].component_size == 1) value = data[4*i+channel] / 255.0f;
            else if (formats[f].component_size == 2) value = ((WORD *)data)[4*i+channel] / 65535.0f;
            else value = ((float *)data)[4*i+channel];
            result[4*i+c] = value;
        }
        if (formats[f].opaque) result[4*i+3] = 1;
        if (!formats[f].premultiplied)
            for (c = 0; c < 3; ++c) result[4*i+c] *= result[4*i+3];
    }
    *pixels = result;
    result = NULL;
done:
    free(result);
    free(data);
    if (converter) IWICFormatConverter_Release(converter);
    if (factory) IWICImagingFactory_Release(factory);
    return hr;
}

static void sample(const float *pixels, D2D1_SIZE_U size, double x, double y, UINT mode, float *out)
{
    UINT x0, x1, y0, y1, c;
    double fx, fy;
    x = min(max(x, 0), size.width - 1);
    y = min(max(y, 0), size.height - 1);
    if (mode == D2D1_BITMAPSOURCE_INTERPOLATION_MODE_NEAREST_NEIGHBOR)
    {
        memcpy(out, pixels + 4 * ((size_t)(UINT)(y + .5) * size.width + (UINT)(x + .5)), 4 * sizeof(float));
        return;
    }
    x0 = x; y0 = y;
    x1 = min(x0 + 1, size.width - 1); y1 = min(y0 + 1, size.height - 1);
    fx = x - x0; fy = y - y0;
    for (c = 0; c < 4; ++c)
        out[c] = (1-fy) * ((1-fx)*pixels[4*((size_t)y0*size.width+x0)+c] + fx*pixels[4*((size_t)y0*size.width+x1)+c])
                + fy * ((1-fx)*pixels[4*((size_t)y1*size.width+x0)+c] + fx*pixels[4*((size_t)y1*size.width+x1)+c]);
}

HRESULT d2d_bitmap_source_evaluate(struct d2d_effect *base, struct d2d_device_context *context,
        struct d2d_effect_image *output, BOOL bounds_only)
{
    struct bitmap_source_effect *effect;
    D2D1_BITMAP_PROPERTIES1 desc = {{DXGI_FORMAT_R32G32B32A32_FLOAT, D2D1_ALPHA_MODE_PREMULTIPLIED},
            96, 96, D2D1_BITMAP_OPTIONS_NONE, NULL};
    D2D1_SIZE_U source_size, scaled_size, size;
    struct d2d_bitmap *bitmap;
    double width, height, source_dpi_x, source_dpi_y;
    float *input = NULL, *pixels = NULL;
    UINT x, y, sx, sy;
    HRESULT hr;

    if (base->impl->lpVtbl != &bitmap_source_vtbl) return E_UNEXPECTED;
    effect = impl_from_ID2D1EffectImpl(base->impl);
    if (base->input_count || !effect->source) return D2DERR_WRONG_STATE;
    if (context->drawing_state.unitMode != D2D1_UNIT_MODE_PIXELS)
    {
        desc.dpiX = context->desc.dpiX;
        desc.dpiY = context->desc.dpiY;
    }
    if (!bounds_only && effect->cached && effect->cached_dpi_x == desc.dpiX && effect->cached_dpi_y == desc.dpiY)
    {
        size = ID2D1Bitmap_GetPixelSize(effect->cached);
        output->rect = (D2D1_RECT_L){0, 0, size.width, size.height};
        ID2D1Bitmap_AddRef(output->bitmap = effect->cached);
        return S_OK;
    }
    if (FAILED(hr = IWICBitmapSource_GetSize(effect->source, &source_size.width, &source_size.height))) return hr;
    width = source_size.width * (double)effect->scale.x;
    height = source_size.height * (double)effect->scale.y;
    if (effect->dpi_correction)
    {
        if (FAILED(hr = IWICBitmapSource_GetResolution(effect->source, &source_dpi_x, &source_dpi_y))) return hr;
        if (!isfinite(source_dpi_x) || !isfinite(source_dpi_y) || source_dpi_x <= 0 || source_dpi_y <= 0)
            return E_INVALIDARG;
        width *= desc.dpiX / source_dpi_x;
        height *= desc.dpiY / source_dpi_y;
    }
    if (ceil(width) > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION || ceil(height) > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
        return D2DERR_EXCEEDS_MAX_BITMAP_SIZE;
    scaled_size.width = ceil(width); scaled_size.height = ceil(height);
    size = scaled_size;
    if (effect->orientation >= 5) { size.width = scaled_size.height; size.height = scaled_size.width; }
    output->rect = (D2D1_RECT_L){0, 0, size.width, size.height};
    if (bounds_only) return S_OK;
    if (effect->alpha_mode != D2D1_BITMAPSOURCE_ALPHA_MODE_PREMULTIPLIED) return E_NOTIMPL;
    if (effect->interpolation != D2D1_BITMAPSOURCE_INTERPOLATION_MODE_NEAREST_NEIGHBOR
            && effect->interpolation != D2D1_BITMAPSOURCE_INTERPOLATION_MODE_LINEAR) return E_NOTIMPL;
    if (size.width && size.height)
    {
        if ((size_t)size.width * size.height > SIZE_MAX / (4 * sizeof(float))) return E_OUTOFMEMORY;
        if (FAILED(hr = load_pixels(effect->source, source_size, &input))) return hr;
        if (!(pixels = malloc((size_t)size.width * size.height * 4 * sizeof(float))))
        {
            free(input);
            return E_OUTOFMEMORY;
        }
        for (y = 0; y < size.height; ++y)
            for (x = 0; x < size.width; ++x)
            {
                switch (effect->orientation)
                {
                    default: sx = x; sy = y; break;
                    case 2: sx = scaled_size.width-1-x; sy = y; break;
                    case 3: sx = scaled_size.width-1-x; sy = scaled_size.height-1-y; break;
                    case 4: sx = x; sy = scaled_size.height-1-y; break;
                    case 5: sx = scaled_size.width-1-y; sy = scaled_size.height-1-x; break;
                    case 6: sx = y; sy = scaled_size.height-1-x; break;
                    case 7: sx = y; sy = x; break;
                    case 8: sx = scaled_size.width-1-y; sy = x; break;
                }
                sample(input, source_size, (sx+.5)*source_size.width/scaled_size.width-.5,
                        (sy+.5)*source_size.height/scaled_size.height-.5, effect->interpolation,
                        pixels + 4*((size_t)y*size.width+x));
            }
    }
    hr = d2d_bitmap_create(context, size, pixels, size.width * 4 * sizeof(float), &desc, &bitmap);
    free(input);
    free(pixels);
    if (FAILED(hr)) return hr;
    invalidate(effect);
    effect->cached = (ID2D1Bitmap *)&bitmap->ID2D1Bitmap1_iface;
    effect->cached_dpi_x = desc.dpiX;
    effect->cached_dpi_y = desc.dpiY;
    ID2D1Bitmap_AddRef(output->bitmap = effect->cached);
    return S_OK;
}
