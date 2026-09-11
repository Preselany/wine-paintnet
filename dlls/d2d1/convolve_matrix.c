/* Direct2D convolution with a caller-supplied matrix.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "d2d1_private.h"
#include <float.h>

WINE_DEFAULT_DEBUG_CHANNEL(d2d);

struct convolve_matrix_effect
{
    ID2D1EffectImpl ID2D1EffectImpl_iface;
    LONG refcount;
    ID3D11ComputeShader *shader;
    D2D1_VECTOR_2F unit_length, offset;
    UINT32 size_x, size_y, scale_mode, border_mode, kernel_count;
    float divisor, bias, *kernel;
    BOOL preserve_alpha, clamp_output;
};

static const char convolve_matrix_shader[] =
    "Texture2D<float4> image : register(t0);\n"
    "StructuredBuffer<float> weights : register(t2);\n"
    "RWTexture2D<float4> output_image : register(u0);\n"
    "cbuffer params : register(b0) { uint2 extent; int2 origin; uint2 input_size; uint2 kernel_size;\n"
    " float2 step_size; float2 kernel_offset; float divisor; float bias; uint preserve_alpha; uint clamp_output;\n"
    " uint border_mode; uint alpha_mode; uint scale_mode; uint pad; };\n"
    "int mirror(int p, int size) { p = p % (size * 2); if (p < 0) p += size * 2; return p < size ? p : size * 2 - 1 - p; }\n"
    "float4 fetch(int2 p) {\n"
    " if (border_mode == 1) p = int2(mirror(p.x, input_size.x), mirror(p.y, input_size.y));\n"
    " else if (p.x < 0 || p.y < 0 || p.x >= (int)input_size.x || p.y >= (int)input_size.y) return 0;\n"
    " float4 color = image.Load(int3(p, 0));\n"
    " if (preserve_alpha) color.rgb = color.a != 0 ? color.rgb / color.a : 0;\n"
    " return color; }\n"
    "float4 sample_image(float2 p) {\n"
    " int2 base = int2(floor(p)); float2 f = frac(p);\n"
    " return lerp(lerp(fetch(base), fetch(base + int2(1,0)), f.x),\n"
    "             lerp(fetch(base + int2(0,1)), fetch(base + int2(1,1)), f.x), f.y); }\n"
    "[numthreads(16,16,1)] void main(uint3 tid : SV_DispatchThreadID) {\n"
    " if (tid.x >= extent.x || tid.y >= extent.y) return;\n"
    " float2 center = int2(tid.xy) + origin; float4 color = 0;\n"
    " for (uint y = 0; y < kernel_size.y; ++y) for (uint x = 0; x < kernel_size.x; ++x) {\n"
    "  float2 delta = ((float2(kernel_size) - 1) * 0.5 - float2(x,y) + kernel_offset) * step_size;\n"
    "  color += sample_image(center + delta) * weights[y * kernel_size.x + x]; }\n"
    " color = color / divisor + bias;\n"
    " if (preserve_alpha) color.a = fetch(int2(center)).a;\n"
    " if (clamp_output) color = saturate(color);\n"
    " if (preserve_alpha) color.rgb *= color.a; output_image[tid.xy] = color; }\n";

static struct convolve_matrix_effect *impl_from_ID2D1EffectImpl(ID2D1EffectImpl *iface)
{
    return CONTAINING_RECORD(iface, struct convolve_matrix_effect, ID2D1EffectImpl_iface);
}

static HRESULT STDMETHODCALLTYPE convolve_matrix_QueryInterface(ID2D1EffectImpl *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_ID2D1EffectImpl) && !IsEqualGUID(iid, &IID_IUnknown)) return E_NOINTERFACE;
    *out = iface;
    ID2D1EffectImpl_AddRef(iface);
    return S_OK;
}

static ULONG STDMETHODCALLTYPE convolve_matrix_AddRef(ID2D1EffectImpl *iface)
{
    return InterlockedIncrement(&impl_from_ID2D1EffectImpl(iface)->refcount);
}

static ULONG STDMETHODCALLTYPE convolve_matrix_Release(ID2D1EffectImpl *iface)
{
    struct convolve_matrix_effect *effect = impl_from_ID2D1EffectImpl(iface);
    ULONG refcount = InterlockedDecrement(&effect->refcount);
    if (!refcount)
    {
        if (effect->shader) ID3D11ComputeShader_Release(effect->shader);
        free(effect->kernel);
        free(effect);
    }
    return refcount;
}

static HRESULT STDMETHODCALLTYPE convolve_matrix_Initialize(ID2D1EffectImpl *iface, ID2D1EffectContext *context,
        ID2D1TransformGraph *graph)
{
    struct convolve_matrix_effect *effect = impl_from_ID2D1EffectImpl(iface);
    struct d2d_effect_context *effect_context = CONTAINING_RECORD(context,
            struct d2d_effect_context, ID2D1EffectContext1_iface);
    ID3D11Device1 *device = effect_context->device_context->d3d_device;

    return d2d_effect_compile_compute_shader(device, convolve_matrix_shader, &effect->shader);
}

static HRESULT STDMETHODCALLTYPE convolve_matrix_PrepareForRender(ID2D1EffectImpl *iface, D2D1_CHANGE_TYPE change)
{
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE convolve_matrix_SetGraph(ID2D1EffectImpl *iface, ID2D1TransformGraph *graph)
{
    return E_NOTIMPL;
}

static const ID2D1EffectImplVtbl convolve_matrix_vtbl =
{
    convolve_matrix_QueryInterface, convolve_matrix_AddRef, convolve_matrix_Release,
    convolve_matrix_Initialize, convolve_matrix_PrepareForRender, convolve_matrix_SetGraph,
};

static HRESULT CALLBACK convolve_matrix_factory(IUnknown **out)
{
    struct convolve_matrix_effect *effect = calloc(1, sizeof(*effect));
    if (!effect) return E_OUTOFMEMORY;
    effect->ID2D1EffectImpl_iface.lpVtbl = &convolve_matrix_vtbl;
    effect->refcount = 1;
    effect->unit_length.x = effect->unit_length.y = 1;
    effect->size_x = effect->size_y = 3;
    effect->scale_mode = D2D1_CONVOLVEMATRIX_SCALE_MODE_LINEAR;
    effect->divisor = 1;
    effect->kernel_count = 9;
    if (!(effect->kernel = calloc(9, sizeof(float))))
    {
        free(effect);
        return E_OUTOFMEMORY;
    }
    effect->kernel[4] = 1;
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

#define PROPERTY(name, type, valid) \
static HRESULT CALLBACK name##_get(const IUnknown *iface, BYTE *data, UINT size, UINT *actual) \
{ \
    const struct convolve_matrix_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface); \
    return property_get(&effect->name, sizeof(effect->name), data, size, actual); \
} \
static HRESULT CALLBACK name##_set(IUnknown *iface, const BYTE *data, UINT size) \
{ \
    struct convolve_matrix_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface); \
    type value; \
    if (!data || size != sizeof(value)) return E_INVALIDARG; \
    memcpy(&value, data, size); \
    if (!(valid)) return E_INVALIDARG; \
    effect->name = value; \
    return S_OK; \
}

PROPERTY(unit_length, D2D1_VECTOR_2F, value.x >= 0.01f && value.x <= 100 && value.y >= 0.01f && value.y <= 100)
PROPERTY(offset, D2D1_VECTOR_2F, value.x >= -50 && value.x <= 50 && value.y >= -50 && value.y <= 50)
PROPERTY(size_x, UINT32, (value = min(max(value, 1), 100), TRUE))
PROPERTY(size_y, UINT32, (value = min(max(value, 1), 100), TRUE))
PROPERTY(scale_mode, UINT32, value <= D2D1_CONVOLVEMATRIX_SCALE_MODE_HIGH_QUALITY_CUBIC)
PROPERTY(border_mode, UINT32, value <= D2D1_BORDER_MODE_HARD)
PROPERTY(divisor, float, isfinite(value))
PROPERTY(bias, float, isfinite(value))
PROPERTY(preserve_alpha, BOOL, TRUE)
PROPERTY(clamp_output, BOOL, TRUE)
#undef PROPERTY

static HRESULT CALLBACK kernel_get(const IUnknown *iface, BYTE *data, UINT size, UINT *actual)
{
    const struct convolve_matrix_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    return property_get(effect->kernel, effect->kernel_count * sizeof(float), data, size, actual);
}

static HRESULT CALLBACK kernel_set(IUnknown *iface, const BYTE *data, UINT size)
{
    struct convolve_matrix_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    float *kernel;
    UINT count = size / sizeof(float), i;
    if (!data || !size || size % sizeof(float)) return E_INVALIDARG;
    if (!(kernel = malloc(size))) return E_OUTOFMEMORY;
    memcpy(kernel, data, size);
    for (i = 0; i < count; ++i)
        if (!isfinite(kernel[i])) { free(kernel); return E_INVALIDARG; }
    free(effect->kernel);
    effect->kernel = kernel;
    effect->kernel_count = count;
    return S_OK;
}

void d2d_convolve_matrix_init_builtin(struct d2d_factory *factory)
{
    static const WCHAR description[] = L"<?xml version='1.0'?><Effect>"
        L"<Property name='DisplayName' type='string' value='Convolve Matrix'/>"
        L"<Property name='Author' type='string' value='The Wine Project'/>"
        L"<Property name='Category' type='string' value='Filter'/>"
        L"<Property name='Description' type='string' value='Applies a two-dimensional convolution kernel'/>"
        L"<Inputs minimum='1' maximum='1'><Input name='Source'/></Inputs>"
        L"<Property name='KernelUnitLength' type='vector2' value='(1,1)'/>"
        L"<Property name='ScaleMode' type='enum' value='1'/>"
        L"<Property name='KernelSizeX' type='uint32' value='3'/>"
        L"<Property name='KernelSizeY' type='uint32' value='3'/>"
        L"<Property name='KernelMatrix' type='blob'/>"
        L"<Property name='Divisor' type='float' value='1'/>"
        L"<Property name='Bias' type='float' value='0'/>"
        L"<Property name='KernelOffset' type='vector2' value='(0,0)'/>"
        L"<Property name='PreserveAlpha' type='bool' value='false'/>"
        L"<Property name='BorderMode' type='enum' value='0'/>"
        L"<Property name='ClampOutput' type='bool' value='false'/></Effect>";
    static const D2D1_PROPERTY_BINDING bindings[] =
    {
        {L"KernelUnitLength", unit_length_set, unit_length_get},
        {L"ScaleMode", scale_mode_set, scale_mode_get},
        {L"KernelSizeX", size_x_set, size_x_get},
        {L"KernelSizeY", size_y_set, size_y_get},
        {L"KernelMatrix", kernel_set, kernel_get},
        {L"Divisor", divisor_set, divisor_get},
        {L"Bias", bias_set, bias_get},
        {L"KernelOffset", offset_set, offset_get},
        {L"PreserveAlpha", preserve_alpha_set, preserve_alpha_get},
        {L"BorderMode", border_mode_set, border_mode_get},
        {L"ClampOutput", clamp_output_set, clamp_output_get},
    };
    HRESULT hr;
    if (FAILED(hr = d2d_factory_register_builtin_effect(factory, &CLSID_D2D1ConvolveMatrix,
            description, bindings, ARRAY_SIZE(bindings), convolve_matrix_factory)))
        WARN("Failed to register Convolve Matrix, hr %#lx.\n", hr);
}

HRESULT d2d_convolve_matrix_bounds(struct d2d_effect *effect, struct d2d_device_context *context,
        const D2D1_RECT_L *input, D2D1_RECT_L *output)
{
    struct convolve_matrix_effect *convolve = impl_from_ID2D1EffectImpl(effect->impl);
    double step_x, step_y, half_x, half_y, left, top, right, bottom;
    UINT x, y, min_x, min_y, max_x = 0, max_y = 0;
    BOOL any = FALSE;

    if (effect->impl->lpVtbl != &convolve_matrix_vtbl) return E_UNEXPECTED;
    *output = *input;
    if (convolve->border_mode == D2D1_BORDER_MODE_HARD || input->left == input->right || input->top == input->bottom)
        return S_OK;
    step_x = convolve->unit_length.x * (context->drawing_state.unitMode == D2D1_UNIT_MODE_PIXELS ? 1 : context->desc.dpiX / 96.0);
    step_y = convolve->unit_length.y * (context->drawing_state.unitMode == D2D1_UNIT_MODE_PIXELS ? 1 : context->desc.dpiY / 96.0);
    half_x = (convolve->size_x - 1) * 0.5;
    half_y = (convolve->size_y - 1) * 0.5;
    min_x = convolve->size_x;
    min_y = convolve->size_y;
    for (y = 0; y < convolve->size_y; ++y)
        for (x = 0; x < convolve->size_x; ++x)
            if (y * convolve->size_x + x < convolve->kernel_count && convolve->kernel[y * convolve->size_x + x] != 0)
            {
                min_x = min(min_x, x); min_y = min(min_y, y);
                max_x = max(max_x, x); max_y = max(max_y, y);
                any = TRUE;
            }
    /* Windows trims unused rows and columns before computing the output bounds. */
    left = floor(input->left + (any ? min_x - half_x - convolve->offset.x : 0) * step_x);
    top = floor(input->top + (any ? min_y - half_y - convolve->offset.y : 0) * step_y);
    right = ceil(input->right + (any ? max_x - half_x - convolve->offset.x : half_x) * step_x);
    bottom = ceil(input->bottom + (any ? max_y - half_y - convolve->offset.y : half_y) * step_y);
    if (left < INT_MIN || top < INT_MIN || right > INT_MAX || bottom > INT_MAX)
        return D2DERR_EXCEEDS_MAX_BITMAP_SIZE;
    output->left = left;
    output->top = top;
    output->right = right;
    output->bottom = bottom;
    return S_OK;
}

HRESULT d2d_convolve_matrix_render(struct d2d_effect *effect, struct d2d_device_context *context,
        const struct d2d_effect_image *input, struct d2d_effect_image *output)
{
    struct convolve_matrix_effect *convolve = impl_from_ID2D1EffectImpl(effect->impl);
    struct d2d_bitmap *bitmap = unsafe_impl_from_ID2D1Bitmap(input->bitmap);
    D3D11_BUFFER_DESC desc = {0};
    D3D11_SUBRESOURCE_DATA initial = {0};
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {0};
    ID3D11Buffer *weights;
    ID3D11ShaderResourceView *srv;
    struct
    {
        UINT extent[2]; INT origin[2];
        UINT input_size[2], kernel_size[2];
        float step[2], offset[2], divisor, bias;
        UINT preserve_alpha, clamp_output, border_mode, alpha_mode, scale_mode, pad;
    } params =
    {
        {output->rect.right - output->rect.left, output->rect.bottom - output->rect.top},
        {output->rect.left - input->rect.left, output->rect.top - input->rect.top},
        {bitmap->pixel_size.width, bitmap->pixel_size.height}, {convolve->size_x, convolve->size_y},
        {convolve->unit_length.x, convolve->unit_length.y}, {convolve->offset.x, convolve->offset.y},
        convolve->divisor ? convolve->divisor : FLT_EPSILON, convolve->bias,
        !!convolve->preserve_alpha, !!convolve->clamp_output, convolve->border_mode,
        bitmap->format.alphaMode, convolve->scale_mode, 0,
    };
    HRESULT hr;

    if (effect->impl->lpVtbl != &convolve_matrix_vtbl) return E_UNEXPECTED;
    if (convolve->kernel_count != convolve->size_x * convolve->size_y) return E_INVALIDARG;
    if (context->drawing_state.unitMode != D2D1_UNIT_MODE_PIXELS)
    {
        params.step[0] *= context->desc.dpiX / 96.0f;
        params.step[1] *= context->desc.dpiY / 96.0f;
    }
    /* Scaled kernels need a pre/post-resampling pass for the selected scale
     * mode. One-pixel unit spacing avoids that pass; fractional offsets still
     * use the convolution's specified bilinear sample interpolation. */
    if (params.step[0] != 1 || params.step[1] != 1)
    {
        FIXME("Scaled convolution kernels are not implemented.\n");
        return E_NOTIMPL;
    }
    desc.ByteWidth = convolve->kernel_count * sizeof(float);
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = sizeof(float);
    initial.pSysMem = convolve->kernel;
    if (FAILED(hr = ID3D11Device1_CreateBuffer(context->d3d_device, &desc, &initial, &weights))) return hr;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srv_desc.Buffer.NumElements = convolve->kernel_count;
    hr = ID3D11Device1_CreateShaderResourceView(context->d3d_device, (ID3D11Resource *)weights, &srv_desc, &srv);
    ID3D11Buffer_Release(weights);
    if (FAILED(hr)) return hr;
    hr = d2d_effect_dispatch(context, convolve->shader, input, 1, &params, sizeof(params), srv, output);
    ID3D11ShaderResourceView_Release(srv);
    return hr;
}
