/* Direct2D opacity adjustment.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "d2d1_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d2d);

struct opacity_effect
{
    ID2D1EffectImpl ID2D1EffectImpl_iface;
    LONG refcount;
    ID3D11ComputeShader *shader;
    float amount;
};

static const char opacity_shader[] =
    "Texture2D<float4> image : register(t0);\n"
    "RWTexture2D<float4> output_image : register(u0);\n"
    "cbuffer params : register(b0) { uint2 extent; float amount; uint alpha_mode; };\n"
    "[numthreads(16,16,1)] void main(uint3 tid : SV_DispatchThreadID) {\n"
    " if (tid.x >= extent.x || tid.y >= extent.y) return;\n"
    " float4 color = image.Load(int3(tid.xy,0));\n"
    " if (alpha_mode == 2) color.rgb *= color.a;\n"
    " output_image[tid.xy] = color * amount; }\n";

static struct opacity_effect *impl_from_ID2D1EffectImpl(ID2D1EffectImpl *iface)
{
    return CONTAINING_RECORD(iface, struct opacity_effect, ID2D1EffectImpl_iface);
}

static HRESULT STDMETHODCALLTYPE opacity_QueryInterface(ID2D1EffectImpl *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_ID2D1EffectImpl) && !IsEqualGUID(iid, &IID_IUnknown)) return E_NOINTERFACE;
    *out = iface;
    ID2D1EffectImpl_AddRef(iface);
    return S_OK;
}

static ULONG STDMETHODCALLTYPE opacity_AddRef(ID2D1EffectImpl *iface)
{
    return InterlockedIncrement(&impl_from_ID2D1EffectImpl(iface)->refcount);
}

static ULONG STDMETHODCALLTYPE opacity_Release(ID2D1EffectImpl *iface)
{
    struct opacity_effect *effect = impl_from_ID2D1EffectImpl(iface);
    ULONG refcount = InterlockedDecrement(&effect->refcount);
    if (!refcount)
    {
        if (effect->shader) ID3D11ComputeShader_Release(effect->shader);
        free(effect);
    }
    return refcount;
}

static HRESULT STDMETHODCALLTYPE opacity_Initialize(ID2D1EffectImpl *iface, ID2D1EffectContext *context,
        ID2D1TransformGraph *graph)
{
    struct opacity_effect *effect = impl_from_ID2D1EffectImpl(iface);
    struct d2d_effect_context *effect_context = CONTAINING_RECORD(context,
            struct d2d_effect_context, ID2D1EffectContext1_iface);
    ID3D11Device1 *device = effect_context->device_context->d3d_device;

    return d2d_effect_compile_compute_shader(device, opacity_shader, &effect->shader);
}

static HRESULT STDMETHODCALLTYPE opacity_PrepareForRender(ID2D1EffectImpl *iface, D2D1_CHANGE_TYPE change)
{
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE opacity_SetGraph(ID2D1EffectImpl *iface, ID2D1TransformGraph *graph)
{
    return E_NOTIMPL;
}

static const ID2D1EffectImplVtbl opacity_vtbl =
{
    opacity_QueryInterface, opacity_AddRef, opacity_Release,
    opacity_Initialize, opacity_PrepareForRender, opacity_SetGraph,
};

static HRESULT CALLBACK opacity_factory(IUnknown **out)
{
    struct opacity_effect *effect = calloc(1, sizeof(*effect));
    if (!effect) return E_OUTOFMEMORY;
    effect->ID2D1EffectImpl_iface.lpVtbl = &opacity_vtbl;
    effect->refcount = 1;
    effect->amount = 1;
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
    const struct opacity_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    return property_get(&effect->amount, sizeof(effect->amount), data, size, actual);
}

static HRESULT CALLBACK amount_set(IUnknown *iface, const BYTE *data, UINT size)
{
    struct opacity_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    float value;
    if (!data || size != sizeof(value)) return E_INVALIDARG;
    memcpy(&value, data, size);
    if (isnan(value)) return E_INVALIDARG;
    effect->amount = min(max(value, 0), 1);
    return S_OK;
}

void d2d_opacity_init_builtin(struct d2d_factory *factory)
{
    static const WCHAR description[] = L"<?xml version='1.0'?><Effect>"
        L"<Property name='DisplayName' type='string' value='Opacity'/>"
        L"<Property name='Author' type='string' value='The Wine Project'/>"
        L"<Property name='Category' type='string' value='Transparency'/>"
        L"<Property name='Description' type='string' value='Multiplies image opacity by a constant'/>"
        L"<Inputs minimum='1' maximum='1'><Input name='Source'/></Inputs>"
        L"<Property name='Opacity' type='float' value='1'/></Effect>";
    static const D2D1_PROPERTY_BINDING bindings[] =
    {
        {L"Opacity", amount_set, amount_get},
    };
    HRESULT hr;
    if (FAILED(hr = d2d_factory_register_builtin_effect(factory, &CLSID_D2D1Opacity,
            description, bindings, ARRAY_SIZE(bindings), opacity_factory)))
        WARN("Failed to register Opacity, hr %#lx.\n", hr);
}

HRESULT d2d_opacity_render(struct d2d_effect *effect, struct d2d_device_context *context,
        const struct d2d_effect_image *input, struct d2d_effect_image *output)
{
    struct opacity_effect *opacity = impl_from_ID2D1EffectImpl(effect->impl);
    struct d2d_bitmap *bitmap = unsafe_impl_from_ID2D1Bitmap(input->bitmap);
    struct
    {
        UINT extent[2]; float amount; UINT alpha_mode;
    } params =
    {
        {bitmap->pixel_size.width, bitmap->pixel_size.height}, opacity->amount,
        bitmap->format.alphaMode,
    };
    if (effect->impl->lpVtbl != &opacity_vtbl) return E_UNEXPECTED;
    return d2d_effect_dispatch(context, opacity->shader, input, 1, &params, sizeof(params), NULL, output);
}
