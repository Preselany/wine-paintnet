/* Direct2D piecewise-quadratic contrast adjustment.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "d2d1_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d2d);

struct contrast_effect
{
    ID2D1EffectImpl ID2D1EffectImpl_iface;
    LONG refcount;
    ID3D11ComputeShader *shader;
    float amount;
    BOOL clamp_input;
};

static const char contrast_shader[] =
    "Texture2D<float4> image : register(t0);\n"
    "RWTexture2D<float4> output_image : register(u0);\n"
    "cbuffer params : register(b0) { uint2 extent; float amount; uint clamp_input; uint alpha_mode; uint3 pad; };\n"
    "float adjust(float x) {\n"
    " if (amount == 0) return x;\n"
    " float delta = x < 0.5 ? 2*x*x-x : 3*x-2*x*x-1;\n"
    " return x + amount * delta; }\n"
    "[numthreads(16,16,1)] void main(uint3 tid : SV_DispatchThreadID) {\n"
    " if (tid.x >= extent.x || tid.y >= extent.y) return;\n"
    " float4 color = image.Load(int3(tid.xy,0));\n"
    " if (alpha_mode == 3) color.a = 1;\n"
    " else if (alpha_mode == 1) color.rgb = color.a != 0 ? color.rgb / color.a : 0;\n"
    " if (clamp_input) color.rgb = saturate(color.rgb);\n"
    " color.rgb = float3(adjust(color.r), adjust(color.g), adjust(color.b)) * color.a;\n"
    " output_image[tid.xy] = color; }\n";

static struct contrast_effect *impl_from_ID2D1EffectImpl(ID2D1EffectImpl *iface)
{
    return CONTAINING_RECORD(iface, struct contrast_effect, ID2D1EffectImpl_iface);
}

static HRESULT STDMETHODCALLTYPE contrast_QueryInterface(ID2D1EffectImpl *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_ID2D1EffectImpl) && !IsEqualGUID(iid, &IID_IUnknown)) return E_NOINTERFACE;
    *out = iface;
    ID2D1EffectImpl_AddRef(iface);
    return S_OK;
}

static ULONG STDMETHODCALLTYPE contrast_AddRef(ID2D1EffectImpl *iface)
{
    return InterlockedIncrement(&impl_from_ID2D1EffectImpl(iface)->refcount);
}

static ULONG STDMETHODCALLTYPE contrast_Release(ID2D1EffectImpl *iface)
{
    struct contrast_effect *effect = impl_from_ID2D1EffectImpl(iface);
    ULONG refcount = InterlockedDecrement(&effect->refcount);
    if (!refcount)
    {
        if (effect->shader) ID3D11ComputeShader_Release(effect->shader);
        free(effect);
    }
    return refcount;
}

static HRESULT STDMETHODCALLTYPE contrast_Initialize(ID2D1EffectImpl *iface, ID2D1EffectContext *context,
        ID2D1TransformGraph *graph)
{
    struct contrast_effect *effect = impl_from_ID2D1EffectImpl(iface);
    struct d2d_effect_context *effect_context = CONTAINING_RECORD(context,
            struct d2d_effect_context, ID2D1EffectContext1_iface);
    ID3D11Device1 *device = effect_context->device_context->d3d_device;

    return d2d_effect_compile_compute_shader(device, contrast_shader, &effect->shader);
}

static HRESULT STDMETHODCALLTYPE contrast_PrepareForRender(ID2D1EffectImpl *iface, D2D1_CHANGE_TYPE change)
{
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE contrast_SetGraph(ID2D1EffectImpl *iface, ID2D1TransformGraph *graph)
{
    return E_NOTIMPL;
}

static const ID2D1EffectImplVtbl contrast_vtbl =
{
    contrast_QueryInterface, contrast_AddRef, contrast_Release,
    contrast_Initialize, contrast_PrepareForRender, contrast_SetGraph,
};

static HRESULT CALLBACK contrast_factory(IUnknown **out)
{
    struct contrast_effect *effect = calloc(1, sizeof(*effect));
    if (!effect) return E_OUTOFMEMORY;
    effect->ID2D1EffectImpl_iface.lpVtbl = &contrast_vtbl;
    effect->refcount = 1;
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

static HRESULT CALLBACK amount_get(const IUnknown *iface, BYTE *data, UINT size, UINT *actual)
{
    const struct contrast_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    return property_get(&effect->amount, sizeof(effect->amount), data, size, actual);
}

static HRESULT CALLBACK amount_set(IUnknown *iface, const BYTE *data, UINT size)
{
    struct contrast_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    float value;
    if (!data || size != sizeof(value)) return E_INVALIDARG;
    memcpy(&value, data, size);
    if (!(value >= -1 && value <= 1)) return E_INVALIDARG;
    effect->amount = value;
    return S_OK;
}

static HRESULT CALLBACK clamp_get(const IUnknown *iface, BYTE *data, UINT size, UINT *actual)
{
    const struct contrast_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    return property_get(&effect->clamp_input, sizeof(effect->clamp_input), data, size, actual);
}

static HRESULT CALLBACK clamp_set(IUnknown *iface, const BYTE *data, UINT size)
{
    struct contrast_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    if (!data || size != sizeof(effect->clamp_input)) return E_INVALIDARG;
    memcpy(&effect->clamp_input, data, size);
    return S_OK;
}

void d2d_contrast_init_builtin(struct d2d_factory *factory)
{
    static const WCHAR description[] = L"<?xml version='1.0'?><Effect>"
        L"<Property name='DisplayName' type='string' value='Contrast'/>"
        L"<Property name='Author' type='string' value='The Wine Project'/>"
        L"<Property name='Category' type='string' value='Color'/>"
        L"<Property name='Description' type='string' value='Adjusts image contrast with a piecewise quadratic curve'/>"
        L"<Inputs minimum='1' maximum='1'><Input name='Source'/></Inputs>"
        L"<Property name='Contrast' type='float' value='0'/>"
        L"<Property name='ClampInput' type='bool' value='false'/></Effect>";
    static const D2D1_PROPERTY_BINDING bindings[] =
    {
        {L"Contrast", amount_set, amount_get},
        {L"ClampInput", clamp_set, clamp_get},
    };
    HRESULT hr;
    if (FAILED(hr = d2d_factory_register_builtin_effect(factory, &CLSID_D2D1Contrast,
            description, bindings, ARRAY_SIZE(bindings), contrast_factory)))
        WARN("Failed to register Contrast, hr %#lx.\n", hr);
}

HRESULT d2d_contrast_render(struct d2d_effect *effect, struct d2d_device_context *context,
        const struct d2d_effect_image *input, struct d2d_effect_image *output)
{
    struct contrast_effect *contrast = impl_from_ID2D1EffectImpl(effect->impl);
    struct d2d_bitmap *bitmap = unsafe_impl_from_ID2D1Bitmap(input->bitmap);
    struct
    {
        UINT extent[2]; float amount; UINT clamp_input, alpha_mode, pad[3];
    } params =
    {
        {bitmap->pixel_size.width, bitmap->pixel_size.height}, contrast->amount,
        !!contrast->clamp_input, bitmap->format.alphaMode, {0},
    };
    if (effect->impl->lpVtbl != &contrast_vtbl) return E_UNEXPECTED;
    return d2d_effect_dispatch(context, contrast->shader, input, 1, &params, sizeof(params), NULL, output);
}
