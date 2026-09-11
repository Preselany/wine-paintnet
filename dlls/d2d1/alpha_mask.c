/* Direct2D alpha-mask effect.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "d2d1_private.h"
#include "d3dcompiler.h"

WINE_DEFAULT_DEBUG_CHANNEL(d2d);

struct alpha_mask_effect
{
    ID2D1EffectImpl ID2D1EffectImpl_iface;
    LONG refcount;
    ID3D11ComputeShader *shader;
};

static const char alpha_mask_shader[] =
    "Texture2D<float4> destination : register(t0);\n"
    "Texture2D<float4> mask : register(t1);\n"
    "RWTexture2D<float4> output_image : register(u0);\n"
    "cbuffer params : register(b0) { uint2 extent; uint destination_alpha; uint mask_alpha; int2 destination_offset; int2 mask_offset; };\n"
    "[numthreads(16,16,1)] void main(uint3 tid : SV_DispatchThreadID) {\n"
    " if (tid.x >= extent.x || tid.y >= extent.y) return;\n"
    " float4 color = destination.Load(int3(int2(tid.xy) + destination_offset, 0));\n"
    " if (destination_alpha == 3) color.a = 1;\n"
    " else if (destination_alpha == 2) color.rgb *= color.a;\n"
    " float alpha = mask_alpha == 3 ? 1 : mask.Load(int3(int2(tid.xy) + mask_offset, 0)).a;\n"
    " output_image[tid.xy] = color * alpha;\n"
    "}\n";

static struct alpha_mask_effect *impl_from_ID2D1EffectImpl(ID2D1EffectImpl *iface)
{
    return CONTAINING_RECORD(iface, struct alpha_mask_effect, ID2D1EffectImpl_iface);
}

static HRESULT STDMETHODCALLTYPE alpha_mask_QueryInterface(ID2D1EffectImpl *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_ID2D1EffectImpl) && !IsEqualGUID(iid, &IID_IUnknown)) return E_NOINTERFACE;
    *out = iface;
    ID2D1EffectImpl_AddRef(iface);
    return S_OK;
}

static ULONG STDMETHODCALLTYPE alpha_mask_AddRef(ID2D1EffectImpl *iface)
{
    return InterlockedIncrement(&impl_from_ID2D1EffectImpl(iface)->refcount);
}

static ULONG STDMETHODCALLTYPE alpha_mask_Release(ID2D1EffectImpl *iface)
{
    struct alpha_mask_effect *effect = impl_from_ID2D1EffectImpl(iface);
    ULONG refcount = InterlockedDecrement(&effect->refcount);
    if (!refcount)
    {
        if (effect->shader) ID3D11ComputeShader_Release(effect->shader);
        free(effect);
    }
    return refcount;
}

static HRESULT STDMETHODCALLTYPE alpha_mask_Initialize(ID2D1EffectImpl *iface, ID2D1EffectContext *context,
        ID2D1TransformGraph *graph)
{
    struct alpha_mask_effect *effect = impl_from_ID2D1EffectImpl(iface);
    struct d2d_effect_context *effect_context = CONTAINING_RECORD(context,
            struct d2d_effect_context, ID2D1EffectContext1_iface);
    ID3D11Device1 *device = effect_context->device_context->d3d_device;

    return d2d_effect_compile_compute_shader(device, alpha_mask_shader, &effect->shader);
}

static HRESULT STDMETHODCALLTYPE alpha_mask_PrepareForRender(ID2D1EffectImpl *iface, D2D1_CHANGE_TYPE change)
{
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE alpha_mask_SetGraph(ID2D1EffectImpl *iface, ID2D1TransformGraph *graph)
{
    return E_NOTIMPL;
}

static const ID2D1EffectImplVtbl alpha_mask_vtbl =
{
    alpha_mask_QueryInterface, alpha_mask_AddRef, alpha_mask_Release,
    alpha_mask_Initialize, alpha_mask_PrepareForRender, alpha_mask_SetGraph,
};

static HRESULT CALLBACK alpha_mask_factory(IUnknown **out)
{
    struct alpha_mask_effect *effect = calloc(1, sizeof(*effect));
    if (!effect) return E_OUTOFMEMORY;
    effect->ID2D1EffectImpl_iface.lpVtbl = &alpha_mask_vtbl;
    effect->refcount = 1;
    *out = (IUnknown *)&effect->ID2D1EffectImpl_iface;
    return S_OK;
}

void d2d_alpha_mask_init_builtin(struct d2d_factory *factory)
{
    static const WCHAR description[] = L"<?xml version='1.0'?><Effect>"
        L"<Property name='DisplayName' type='string' value='Alpha Mask'/>"
        L"<Property name='Author' type='string' value='The Wine Project'/>"
        L"<Property name='Category' type='string' value='Compositing'/>"
        L"<Property name='Description' type='string' value='Multiplies an image by a mask alpha channel'/>"
        L"<Inputs minimum='2' maximum='2'><Input name='Destination'/><Input name='Mask'/></Inputs></Effect>";
    HRESULT hr;
    if (FAILED(hr = d2d_factory_register_builtin_effect(factory, &CLSID_D2D1AlphaMask,
            description, NULL, 0, alpha_mask_factory)))
        WARN("Failed to register Alpha Mask, hr %#lx.\n", hr);
}

HRESULT d2d_alpha_mask_render(struct d2d_effect *effect, struct d2d_device_context *context,
        const struct d2d_effect_image *inputs, struct d2d_effect_image *output)
{
    struct alpha_mask_effect *alpha_mask = impl_from_ID2D1EffectImpl(effect->impl);
    struct d2d_bitmap *destination = unsafe_impl_from_ID2D1Bitmap(inputs[0].bitmap);
    struct d2d_bitmap *mask = unsafe_impl_from_ID2D1Bitmap(inputs[1].bitmap);
    struct
    {
        UINT width, height, destination_alpha, mask_alpha;
        INT destination_offset[2], mask_offset[2];
    } params =
    {
        output->rect.right - output->rect.left, output->rect.bottom - output->rect.top,
        destination->format.alphaMode, mask->format.alphaMode,
        {output->rect.left - inputs[0].rect.left, output->rect.top - inputs[0].rect.top},
        {output->rect.left - inputs[1].rect.left, output->rect.top - inputs[1].rect.top},
    };

    if (effect->impl->lpVtbl != &alpha_mask_vtbl) return E_UNEXPECTED;
    return d2d_effect_dispatch(context, alpha_mask->shader, inputs, 2, &params, sizeof(params), NULL, output);
}
