/* Direct2D alpha conversion and pointwise color inversion.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "d2d1_private.h"
#include "d3dcompiler.h"

WINE_DEFAULT_DEBUG_CHANNEL(d2d);

struct alpha_conversion_effect
{
    ID2D1EffectImpl ID2D1EffectImpl_iface;
    LONG refcount;
    ID3D11VertexShader *vs;
    ID3D11PixelShader *ps;
};

static struct alpha_conversion_effect *impl_from_ID2D1EffectImpl(ID2D1EffectImpl *iface)
{
    return CONTAINING_RECORD(iface, struct alpha_conversion_effect, ID2D1EffectImpl_iface);
}

static HRESULT STDMETHODCALLTYPE alpha_conversion_QueryInterface(ID2D1EffectImpl *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_ID2D1EffectImpl) && !IsEqualGUID(iid, &IID_IUnknown)) return E_NOINTERFACE;
    ID2D1EffectImpl_AddRef(iface);
    *out = iface;
    return S_OK;
}

static ULONG STDMETHODCALLTYPE alpha_conversion_AddRef(ID2D1EffectImpl *iface)
{
    return InterlockedIncrement(&impl_from_ID2D1EffectImpl(iface)->refcount);
}

static ULONG STDMETHODCALLTYPE alpha_conversion_Release(ID2D1EffectImpl *iface)
{
    struct alpha_conversion_effect *effect = impl_from_ID2D1EffectImpl(iface);
    ULONG refs = InterlockedDecrement(&effect->refcount);
    if (!refs)
    {
        if (effect->vs) ID3D11VertexShader_Release(effect->vs);
        if (effect->ps) ID3D11PixelShader_Release(effect->ps);
        free(effect);
    }
    return refs;
}

static HRESULT STDMETHODCALLTYPE alpha_conversion_Initialize(ID2D1EffectImpl *iface,
        ID2D1EffectContext *context, ID2D1TransformGraph *graph)
{
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE alpha_conversion_PrepareForRender(ID2D1EffectImpl *iface, D2D1_CHANGE_TYPE change)
{
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE alpha_conversion_SetGraph(ID2D1EffectImpl *iface, ID2D1TransformGraph *graph)
{
    return E_NOTIMPL;
}

static const ID2D1EffectImplVtbl alpha_conversion_vtbl =
{
    alpha_conversion_QueryInterface, alpha_conversion_AddRef, alpha_conversion_Release,
    alpha_conversion_Initialize, alpha_conversion_PrepareForRender, alpha_conversion_SetGraph,
};

static HRESULT CALLBACK alpha_conversion_factory(IUnknown **out)
{
    struct alpha_conversion_effect *effect = calloc(1, sizeof(*effect));
    if (!effect) return E_OUTOFMEMORY;
    effect->ID2D1EffectImpl_iface.lpVtbl = &alpha_conversion_vtbl;
    effect->refcount = 1;
    *out = (IUnknown *)&effect->ID2D1EffectImpl_iface;
    return S_OK;
}

void d2d_alpha_conversion_init_builtin(struct d2d_factory *factory)
{
    static const WCHAR premultiply[] =
        L"<?xml version='1.0'?><Effect><Property name='DisplayName' type='string' value='Premultiply'/>"
        L"<Property name='Author' type='string' value='The Wine Project'/>"
        L"<Property name='Category' type='string' value='Color'/>"
        L"<Property name='Description' type='string' value='Premultiplies color by alpha.'/>"
        L"<Inputs minimum='1' maximum='1'><Input name='Source'/></Inputs></Effect>";
    static const WCHAR unpremultiply[] =
        L"<?xml version='1.0'?><Effect><Property name='DisplayName' type='string' value='UnPremultiply'/>"
        L"<Property name='Author' type='string' value='The Wine Project'/>"
        L"<Property name='Category' type='string' value='Color'/>"
        L"<Property name='Description' type='string' value='Divides color by alpha.'/>"
        L"<Inputs minimum='1' maximum='1'><Input name='Source'/></Inputs></Effect>";
    static const WCHAR invert[] =
        L"<?xml version='1.0'?><Effect><Property name='DisplayName' type='string' value='Invert'/>"
        L"<Property name='Author' type='string' value='The Wine Project'/>"
        L"<Property name='Category' type='string' value='Color'/>"
        L"<Property name='Description' type='string' value='Inverts color while preserving alpha.'/>"
        L"<Inputs minimum='1' maximum='1'><Input name='Source'/></Inputs></Effect>";
    HRESULT hr;
    if (FAILED(hr = d2d_factory_register_builtin_effect(factory, &CLSID_D2D1Premultiply,
            premultiply, NULL, 0, alpha_conversion_factory)))
        WARN("Failed to register Premultiply, hr %#lx.\n", hr);
    if (FAILED(hr = d2d_factory_register_builtin_effect(factory, &CLSID_D2D1UnPremultiply,
            unpremultiply, NULL, 0, alpha_conversion_factory)))
        WARN("Failed to register UnPremultiply, hr %#lx.\n", hr);
    if (FAILED(hr = d2d_factory_register_builtin_effect(factory, &CLSID_D2D1Invert,
            invert, NULL, 0, alpha_conversion_factory)))
        WARN("Failed to register Invert, hr %#lx.\n", hr);
}

static HRESULT create_shaders(struct alpha_conversion_effect *effect, ID3D11Device1 *device)
{
    static const char vertex_source[] =
        "float4 main(uint id:SV_VertexID):SV_POSITION {"
        "float2 uv=float2((id<<1)&2,id&2);"
        "return float4(uv*float2(2,-2)+float2(-1,1),0,1);}";
    static const char pixel_source[] =
        "Texture2D<float4> image:register(t0);"
        "cbuffer C:register(b0) {uint operation;uint3 padding;}"
        "float4 main(float4 pos:SV_POSITION):SV_TARGET {"
        "float4 color=image.Load(int3(pos.xy,0));"
        "if (operation == 2) color.rgb=color.a == 0 ? 0 : color.a-color.rgb;"
        "else if (operation == 1) color.rgb=color.a == 0 ? 0 : color.rgb/color.a;"
        "else color.rgb*=color.a; return color;}";
    const char *sources[] = {vertex_source, pixel_source}, *profiles[] = {"vs_4_0", "ps_4_0"};
    ID3DBlob *code, *errors;
    unsigned int i;
    HRESULT hr;

    for (i = 0; i < ARRAY_SIZE(sources); ++i)
    {
        if (i ? !!effect->ps : !!effect->vs) continue;
        code = errors = NULL;
        hr = D3DCompile(sources[i], strlen(sources[i]), NULL, NULL, NULL, "main", profiles[i],
                D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
        if (errors)
        {
            WARN("%s\n", (char *)ID3D10Blob_GetBufferPointer(errors));
            ID3D10Blob_Release(errors);
        }
        if (FAILED(hr)) return hr;
        if (i)
            hr = ID3D11Device1_CreatePixelShader(device, ID3D10Blob_GetBufferPointer(code),
                    ID3D10Blob_GetBufferSize(code), NULL, &effect->ps);
        else
            hr = ID3D11Device1_CreateVertexShader(device, ID3D10Blob_GetBufferPointer(code),
                    ID3D10Blob_GetBufferSize(code), NULL, &effect->vs);
        ID3D10Blob_Release(code);
        if (FAILED(hr)) return hr;
    }
    return S_OK;
}

static HRESULT render_color(struct d2d_effect *effect, struct d2d_device_context *context,
        const struct d2d_effect_image *input, UINT operation, struct d2d_effect_image *output)
{
    struct alpha_conversion_effect *conversion = impl_from_ID2D1EffectImpl(effect->impl);
    UINT params[4] = {operation, 0, 0, 0};
    HRESULT hr;
    if (effect->impl->lpVtbl != &alpha_conversion_vtbl) return E_UNEXPECTED;
    if (FAILED(hr = create_shaders(conversion, context->d3d_device))) return hr;
    return d2d_effect_render_pixels(context, conversion->vs, conversion->ps, input, 1,
            params, sizeof(params), NULL, output);
}

HRESULT d2d_alpha_conversion_render(struct d2d_effect *effect, struct d2d_device_context *context,
        const struct d2d_effect_image *input, BOOL unpremultiply, struct d2d_effect_image *output)
{
    return render_color(effect, context, input, unpremultiply ? 1 : 0, output);
}

HRESULT d2d_invert_render(struct d2d_effect *effect, struct d2d_device_context *context,
        const struct d2d_effect_image *input, struct d2d_effect_image *output)
{
    return render_color(effect, context, input, 2, output);
}
