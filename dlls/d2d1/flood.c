/* Direct2D solid-color Flood sources.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "d2d1_private.h"
#include "d3dcompiler.h"

WINE_DEFAULT_DEBUG_CHANNEL(d2d);

struct flood_effect
{
    ID2D1EffectImpl ID2D1EffectImpl_iface;
    LONG refcount;
    ID3D11VertexShader *vs;
    ID3D11PixelShader *ps;
    D2D1_VECTOR_4F color;
};

static struct flood_effect *impl_from_ID2D1EffectImpl(ID2D1EffectImpl *iface)
{
    return CONTAINING_RECORD(iface, struct flood_effect, ID2D1EffectImpl_iface);
}

static HRESULT STDMETHODCALLTYPE flood_QueryInterface(ID2D1EffectImpl *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_ID2D1EffectImpl) && !IsEqualGUID(iid, &IID_IUnknown)) return E_NOINTERFACE;
    ID2D1EffectImpl_AddRef(iface);
    *out = iface;
    return S_OK;
}

static ULONG STDMETHODCALLTYPE flood_AddRef(ID2D1EffectImpl *iface)
{
    return InterlockedIncrement(&impl_from_ID2D1EffectImpl(iface)->refcount);
}

static ULONG STDMETHODCALLTYPE flood_Release(ID2D1EffectImpl *iface)
{
    struct flood_effect *effect = impl_from_ID2D1EffectImpl(iface);
    ULONG refs = InterlockedDecrement(&effect->refcount);
    if (!refs)
    {
        if (effect->vs) ID3D11VertexShader_Release(effect->vs);
        if (effect->ps) ID3D11PixelShader_Release(effect->ps);
        free(effect);
    }
    return refs;
}

static HRESULT STDMETHODCALLTYPE flood_Initialize(ID2D1EffectImpl *iface,
        ID2D1EffectContext *context, ID2D1TransformGraph *graph)
{
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE flood_PrepareForRender(ID2D1EffectImpl *iface, D2D1_CHANGE_TYPE change)
{
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE flood_SetGraph(ID2D1EffectImpl *iface, ID2D1TransformGraph *graph)
{
    return E_NOTIMPL;
}

static const ID2D1EffectImplVtbl flood_vtbl =
{
    flood_QueryInterface, flood_AddRef, flood_Release,
    flood_Initialize, flood_PrepareForRender, flood_SetGraph,
};

static HRESULT CALLBACK flood_factory(IUnknown **out)
{
    struct flood_effect *effect = calloc(1, sizeof(*effect));
    if (!effect) return E_OUTOFMEMORY;
    effect->ID2D1EffectImpl_iface.lpVtbl = &flood_vtbl;
    effect->refcount = 1;
    effect->color.w = 1;
    *out = (IUnknown *)&effect->ID2D1EffectImpl_iface;
    return S_OK;
}

static HRESULT create_shaders(struct flood_effect *effect, ID3D11Device1 *device)
{
    static const char vertex_source[] =
        "float4 main(uint id:SV_VertexID):SV_POSITION {"
        "float2 uv=float2((id<<1)&2,id&2);"
        "return float4(uv*float2(2,-2)+float2(-1,1),0,1);}";
    static const char pixel_source[] =
        "cbuffer C:register(b0) {float4 color;}"
        "float4 main(float4 pos:SV_POSITION):SV_TARGET {return color;}";
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

static HRESULT CALLBACK color_get(const IUnknown *iface, BYTE *data, UINT size, UINT *actual)
{
    struct flood_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    if (actual) *actual = sizeof(effect->color);
    if (!data) return S_OK;
    if (size < sizeof(effect->color)) return E_NOT_SUFFICIENT_BUFFER;
    memcpy(data, &effect->color, sizeof(effect->color));
    return S_OK;
}

static HRESULT CALLBACK color_set(IUnknown *iface, const BYTE *data, UINT size)
{
    struct flood_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    if (!data || size != sizeof(effect->color)) return E_INVALIDARG;
    memcpy(&effect->color, data, sizeof(effect->color));
    return S_OK;
}

void d2d_flood_init_builtin(struct d2d_factory *factory)
{
    static const WCHAR description[] =
        L"<?xml version='1.0'?><Effect><Property name='DisplayName' type='string' value='Flood'/>"
        L"<Property name='Author' type='string' value='The Wine Project'/>"
        L"<Property name='Category' type='string' value='Generator'/>"
        L"<Property name='Description' type='string' value='Creates a uniform color source.'/>"
        L"<Inputs minimum='0' maximum='0'/><Property name='Color' type='vector4'/></Effect>";
    static const D2D1_PROPERTY_BINDING bindings[] = {{L"Color", color_set, color_get}};
    HRESULT hr;
    if (FAILED(hr = d2d_factory_register_builtin_effect(factory, &CLSID_D2D1Flood,
            description, bindings, ARRAY_SIZE(bindings), flood_factory)))
        WARN("Failed to register Flood, hr %#lx.\n", hr);
}

HRESULT d2d_flood_evaluate(struct d2d_effect *effect, struct d2d_device_context *context,
        const D2D1_RECT_L *region, BOOL bounds_only, struct d2d_effect_image *output)
{
    struct flood_effect *flood = impl_from_ID2D1EffectImpl(effect->impl);
    HRESULT hr;
    if (effect->impl->lpVtbl != &flood_vtbl) return E_UNEXPECTED;
    output->rect = (D2D1_RECT_L){INT_MIN, INT_MIN, INT_MAX, INT_MAX};
    if (bounds_only) return S_OK;
    if (!region) return D2DERR_EXCEEDS_MAX_BITMAP_SIZE;
    output->rect = *region;
    if (FAILED(hr = create_shaders(flood, context->d3d_device))) return hr;
    return d2d_effect_render_pixels(context, flood->vs, flood->ps, NULL, 0,
            &flood->color, sizeof(flood->color), NULL, output);
}
