/* Direct2D white-level adjustment.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "d2d1_private.h"
#include "d3dcompiler.h"

WINE_DEFAULT_DEBUG_CHANNEL(d2d);

struct white_level_effect
{
    ID2D1EffectImpl ID2D1EffectImpl_iface;
    LONG refcount;
    ID3D11VertexShader *vs;
    ID3D11PixelShader *ps;
    float input_level, output_level;
};

static struct white_level_effect *impl_from_ID2D1EffectImpl(ID2D1EffectImpl *iface)
{
    return CONTAINING_RECORD(iface, struct white_level_effect, ID2D1EffectImpl_iface);
}

static HRESULT STDMETHODCALLTYPE white_level_QueryInterface(ID2D1EffectImpl *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_ID2D1EffectImpl) && !IsEqualGUID(iid, &IID_IUnknown)) return E_NOINTERFACE;
    ID2D1EffectImpl_AddRef(iface);
    *out = iface;
    return S_OK;
}

static ULONG STDMETHODCALLTYPE white_level_AddRef(ID2D1EffectImpl *iface)
{
    return InterlockedIncrement(&impl_from_ID2D1EffectImpl(iface)->refcount);
}

static ULONG STDMETHODCALLTYPE white_level_Release(ID2D1EffectImpl *iface)
{
    struct white_level_effect *effect = impl_from_ID2D1EffectImpl(iface);
    ULONG refs = InterlockedDecrement(&effect->refcount);
    if (!refs)
    {
        if (effect->vs) ID3D11VertexShader_Release(effect->vs);
        if (effect->ps) ID3D11PixelShader_Release(effect->ps);
        free(effect);
    }
    return refs;
}

static HRESULT STDMETHODCALLTYPE white_level_Initialize(ID2D1EffectImpl *iface,
        ID2D1EffectContext *context, ID2D1TransformGraph *graph)
{
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE white_level_PrepareForRender(ID2D1EffectImpl *iface, D2D1_CHANGE_TYPE change)
{
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE white_level_SetGraph(ID2D1EffectImpl *iface, ID2D1TransformGraph *graph)
{
    return E_NOTIMPL;
}

static const ID2D1EffectImplVtbl white_level_vtbl =
{
    white_level_QueryInterface, white_level_AddRef, white_level_Release,
    white_level_Initialize, white_level_PrepareForRender, white_level_SetGraph,
};

static HRESULT CALLBACK white_level_factory(IUnknown **out)
{
    struct white_level_effect *effect = calloc(1, sizeof(*effect));
    if (!effect) return E_OUTOFMEMORY;
    effect->ID2D1EffectImpl_iface.lpVtbl = &white_level_vtbl;
    effect->refcount = 1;
    effect->input_level = effect->output_level = 80.0f;
    *out = (IUnknown *)&effect->ID2D1EffectImpl_iface;
    return S_OK;
}

static HRESULT level_get(float level, BYTE *data, UINT size, UINT *actual)
{
    if (actual) *actual = sizeof(level);
    if (!data) return S_OK;
    if (size < sizeof(level)) return E_NOT_SUFFICIENT_BUFFER;
    memcpy(data, &level, sizeof(level));
    return S_OK;
}

static HRESULT CALLBACK input_level_get(const IUnknown *iface, BYTE *data, UINT size, UINT *actual)
{
    return level_get(impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface)->input_level, data, size, actual);
}

static HRESULT CALLBACK output_level_get(const IUnknown *iface, BYTE *data, UINT size, UINT *actual)
{
    return level_get(impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface)->output_level, data, size, actual);
}

static HRESULT CALLBACK input_level_set(IUnknown *iface, const BYTE *data, UINT size)
{
    struct white_level_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    if (!data || size != sizeof(effect->input_level)) return E_INVALIDARG;
    memcpy(&effect->input_level, data, size);
    return S_OK;
}

static HRESULT CALLBACK output_level_set(IUnknown *iface, const BYTE *data, UINT size)
{
    struct white_level_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    if (!data || size != sizeof(effect->output_level)) return E_INVALIDARG;
    memcpy(&effect->output_level, data, size);
    return S_OK;
}

void d2d_white_level_init_builtin(struct d2d_factory *factory)
{
    static const WCHAR description[] =
        L"<?xml version='1.0'?><Effect><Property name='DisplayName' type='string' value='WhiteLevelAdjustment'/>"
        L"<Property name='Author' type='string' value='The Wine Project'/>"
        L"<Property name='Category' type='string' value='Color'/>"
        L"<Property name='Description' type='string' value='Scales color between white levels.'/>"
        L"<Inputs minimum='1' maximum='1'><Input name='Source'/></Inputs>"
        L"<Property name='InputWhiteLevel' type='float' value='80'><Property name='Default' type='float' value='80'/></Property>"
        L"<Property name='OutputWhiteLevel' type='float' value='80'><Property name='Default' type='float' value='80'/></Property>"
        L"</Effect>";
    static const D2D1_PROPERTY_BINDING bindings[] =
    {
        {L"InputWhiteLevel", input_level_set, input_level_get},
        {L"OutputWhiteLevel", output_level_set, output_level_get},
    };
    struct d2d_effect_registration *registration;
    unsigned int i;
    HRESULT hr;
    if (FAILED(hr = d2d_factory_register_builtin_effect(factory, &CLSID_D2D1WhiteLevelAdjustment,
            description, bindings, ARRAY_SIZE(bindings), white_level_factory)))
    {
        WARN("Failed to register WhiteLevelAdjustment, hr %#lx.\n", hr);
        return;
    }

    /* The XML reader does not yet retain property children. Install the native
     * default metadata explicitly while builtin registration is still private. */
    LIST_FOR_EACH_ENTRY(registration, &factory->effects, struct d2d_effect_registration, entry)
    {
        if (!IsEqualGUID(&registration->id, &CLSID_D2D1WhiteLevelAdjustment)) continue;
        for (i = 0; i < ARRAY_SIZE(bindings); ++i)
        {
            struct d2d_effect_property *property = d2d_effect_properties_get_property_by_name(
                    registration->properties, bindings[i].propertyName);
            if (!(property->subproperties = calloc(1, sizeof(*property->subproperties)))) return;
            d2d_effect_init_properties(NULL, property->subproperties);
            if (FAILED(hr = d2d_effect_subproperties_add(property->subproperties, L"Default",
                    D2D1_SUBPROPERTY_DEFAULT, D2D1_PROPERTY_TYPE_FLOAT, L"80")))
                WARN("Failed to add white-level default metadata, hr %#lx.\n", hr);
        }
        break;
    }
}

static HRESULT create_shaders(struct white_level_effect *effect, ID3D11Device1 *device)
{
    static const char vertex_source[] =
        "float4 main(uint id:SV_VertexID):SV_POSITION {"
        "float2 uv=float2((id<<1)&2,id&2);"
        "return float4(uv*float2(2,-2)+float2(-1,1),0,1);}";
    static const char pixel_source[] =
        "Texture2D<float4> image:register(t0);"
        "cbuffer C:register(b0) {float amount;float3 padding;}"
        "float4 main(float4 pos:SV_POSITION):SV_TARGET {"
        "float4 color=image.Load(int3(pos.xy,0));"
        "color.rgb*=amount; return color;}";
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

HRESULT d2d_white_level_render(struct d2d_effect *effect, struct d2d_device_context *context,
        const struct d2d_effect_image *input, struct d2d_effect_image *output)
{
    struct white_level_effect *conversion = impl_from_ID2D1EffectImpl(effect->impl);
    float params[4] = {conversion->input_level / conversion->output_level, 0, 0, 0};
    HRESULT hr;
    if (effect->impl->lpVtbl != &white_level_vtbl) return E_UNEXPECTED;
    if (FAILED(hr = create_shaders(conversion, context->d3d_device))) return hr;
    return d2d_effect_render_pixels(context, conversion->vs, conversion->ps, input, 1,
            params, sizeof(params), NULL, output);
}
