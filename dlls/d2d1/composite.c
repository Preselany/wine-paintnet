/* Direct2D multi-input Composite rendering.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "d2d1_private.h"
#include "d3dcompiler.h"

WINE_DEFAULT_DEBUG_CHANNEL(d2d);

struct composite_effect
{
    ID2D1EffectImpl ID2D1EffectImpl_iface;
    LONG refcount;
    ID3D11VertexShader *vs;
    ID3D11PixelShader *ps;
    D2D1_COMPOSITE_MODE mode;
};

static struct composite_effect *impl_from_ID2D1EffectImpl(ID2D1EffectImpl *iface)
{
    return CONTAINING_RECORD(iface, struct composite_effect, ID2D1EffectImpl_iface);
}

static HRESULT STDMETHODCALLTYPE composite_QueryInterface(ID2D1EffectImpl *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_ID2D1EffectImpl) && !IsEqualGUID(iid, &IID_IUnknown)) return E_NOINTERFACE;
    ID2D1EffectImpl_AddRef(iface);
    *out = iface;
    return S_OK;
}

static ULONG STDMETHODCALLTYPE composite_AddRef(ID2D1EffectImpl *iface)
{
    return InterlockedIncrement(&impl_from_ID2D1EffectImpl(iface)->refcount);
}

static ULONG STDMETHODCALLTYPE composite_Release(ID2D1EffectImpl *iface)
{
    struct composite_effect *effect = impl_from_ID2D1EffectImpl(iface);
    ULONG refs = InterlockedDecrement(&effect->refcount);
    if (!refs)
    {
        if (effect->vs) ID3D11VertexShader_Release(effect->vs);
        if (effect->ps) ID3D11PixelShader_Release(effect->ps);
        free(effect);
    }
    return refs;
}

static HRESULT STDMETHODCALLTYPE composite_Initialize(ID2D1EffectImpl *iface,
        ID2D1EffectContext *context, ID2D1TransformGraph *graph)
{
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE composite_PrepareForRender(ID2D1EffectImpl *iface, D2D1_CHANGE_TYPE change)
{
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE composite_SetGraph(ID2D1EffectImpl *iface, ID2D1TransformGraph *graph)
{
    /* The builtin evaluator folds the current effect inputs itself. There are
     * no authoring nodes to reconnect when SetInputCount replaces this graph. */
    ID2D1TransformGraph_Clear(graph);
    return S_OK;
}

static const ID2D1EffectImplVtbl composite_vtbl =
{
    composite_QueryInterface, composite_AddRef, composite_Release,
    composite_Initialize, composite_PrepareForRender, composite_SetGraph,
};

static HRESULT CALLBACK composite_factory(IUnknown **out)
{
    struct composite_effect *effect = calloc(1, sizeof(*effect));
    if (!effect) return E_OUTOFMEMORY;
    effect->ID2D1EffectImpl_iface.lpVtbl = &composite_vtbl;
    effect->refcount = 1;
    *out = (IUnknown *)&effect->ID2D1EffectImpl_iface;
    return S_OK;
}

static HRESULT create_shaders(struct composite_effect *effect, ID3D11Device1 *device)
{
    static const char vertex_source[] =
        "float4 main(uint id:SV_VertexID):SV_POSITION {"
        "float2 uv=float2((id<<1)&2,id&2);"
        "return float4(uv*float2(2,-2)+float2(-1,1),0,1);}";
    static const char pixel_source[] =
        "Texture2D<float4> destination:register(t0);Texture2D<float4> source:register(t1);"
        "cbuffer C:register(b0) {int2 dst_shift;int2 src_shift;int2 src_size;uint mode;uint pad;}"
        "float4 main(float4 pos:SV_POSITION):SV_TARGET {"
        "int2 p=int2(pos.xy);float4 d=destination.Load(int3(p+dst_shift,0));"
        "if(mode==13)return d;int2 q=p+src_shift;float4 s=source.Load(int3(q,0));"
        "if(mode==0)return s+(1-s.a)*d;"
        "if(mode==1)return (1-d.a)*s+d;"
        "if(mode==2)return d.a*s;if(mode==3)return s.a*d;"
        "if(mode==4)return (1-d.a)*s;if(mode==5)return (1-s.a)*d;"
        "if(mode==6)return d.a*s+(1-s.a)*d;if(mode==7)return (1-d.a)*s+s.a*d;"
        "if(mode==8)return (1-d.a)*s+(1-s.a)*d;if(mode==9)return s+d;"
        "if(mode==10)return s;if(mode==11)return all(q>=0)&&all(q<src_size)?s:d;"
        "return float4((1-d.rgb)*s.rgb+(1-s.a)*d.rgb,d.a);}";
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

static HRESULT CALLBACK mode_get(const IUnknown *iface, BYTE *data, UINT size, UINT *actual)
{
    struct composite_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    if (actual) *actual = sizeof(effect->mode);
    if (!data) return S_OK;
    if (size < sizeof(effect->mode)) return E_NOT_SUFFICIENT_BUFFER;
    memcpy(data, &effect->mode, sizeof(effect->mode));
    return S_OK;
}

static HRESULT CALLBACK mode_set(IUnknown *iface, const BYTE *data, UINT size)
{
    struct composite_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    D2D1_COMPOSITE_MODE mode;
    if (!data || size != sizeof(mode)) return E_INVALIDARG;
    memcpy(&mode, data, sizeof(mode));
    if (mode > D2D1_COMPOSITE_MODE_MASK_INVERT) return E_INVALIDARG;
    effect->mode = mode;
    return S_OK;
}

void d2d_composite_init_builtin(struct d2d_factory *factory)
{
    static const WCHAR description[] =
        L"<?xml version='1.0'?><Effect><Property name='DisplayName' type='string' value='Composite'/>"
        L"<Property name='Author' type='string' value='The Wine Project'/>"
        L"<Property name='Category' type='string' value='Composite'/>"
        L"<Property name='Description' type='string' value='Composites images in input order.'/>"
        L"<Inputs minimum='1' maximum='0xffffffff'><Input name='Destination'/><Input name='Source'/></Inputs>"
        L"<Property name='Mode' type='enum'/></Effect>";
    static const D2D1_PROPERTY_BINDING bindings[] = {{L"Mode", mode_set, mode_get}};
    HRESULT hr;
    if (FAILED(hr = d2d_factory_register_builtin_effect(factory, &CLSID_D2D1Composite,
            description, bindings, ARRAY_SIZE(bindings), composite_factory)))
        WARN("Failed to register Composite, hr %#lx.\n", hr);
}

static void composite_rect(D2D1_COMPOSITE_MODE mode, const D2D1_RECT_L *source, D2D1_RECT_L *destination)
{
    BOOL source_empty = source->left >= source->right || source->top >= source->bottom;
    BOOL destination_empty = destination->left >= destination->right || destination->top >= destination->bottom;
    switch (mode)
    {
        case D2D1_COMPOSITE_MODE_SOURCE_IN:
        case D2D1_COMPOSITE_MODE_DESTINATION_IN:
            destination->left = max(destination->left, source->left);
            destination->top = max(destination->top, source->top);
            destination->right = min(destination->right, source->right);
            destination->bottom = min(destination->bottom, source->bottom);
            break;
        case D2D1_COMPOSITE_MODE_SOURCE_OUT:
        case D2D1_COMPOSITE_MODE_DESTINATION_ATOP:
        case D2D1_COMPOSITE_MODE_SOURCE_COPY:
            *destination = *source;
            break;
        case D2D1_COMPOSITE_MODE_DESTINATION_OUT:
        case D2D1_COMPOSITE_MODE_SOURCE_ATOP:
            break;
        default:
            if (source_empty) break;
            if (destination_empty) {*destination = *source; break;}
            destination->left = min(destination->left, source->left);
            destination->top = min(destination->top, source->top);
            destination->right = max(destination->right, source->right);
            destination->bottom = max(destination->bottom, source->bottom);
            break;
    }
    if (destination->left >= destination->right || destination->top >= destination->bottom)
        memset(destination, 0, sizeof(*destination));
}

HRESULT d2d_composite_bounds(struct d2d_effect *effect, const struct d2d_effect_image *inputs,
        UINT count, D2D1_RECT_L *output)
{
    struct composite_effect *composite = impl_from_ID2D1EffectImpl(effect->impl);
    unsigned int i;
    if (effect->impl->lpVtbl != &composite_vtbl) return E_UNEXPECTED;
    if (!count) return D2DERR_INVALID_GRAPH_CONFIGURATION;
    *output = inputs[0].rect;
    for (i = 1; i < count; ++i) composite_rect(composite->mode, &inputs[i].rect, output);
    return S_OK;
}

HRESULT d2d_composite_render(struct d2d_effect *effect, struct d2d_device_context *context,
        const struct d2d_effect_image *inputs, UINT count, struct d2d_effect_image *output)
{
    struct composite_effect *composite = impl_from_ID2D1EffectImpl(effect->impl);
    struct d2d_effect_image pair[2], result = {0};
    struct
    {
        LONG dst_shift[2], src_shift[2], src_size[2];
        UINT mode, pad;
    } constants = {0};
    unsigned int i;
    HRESULT hr;
    if (effect->impl->lpVtbl != &composite_vtbl) return E_UNEXPECTED;
    if (!count) return D2DERR_INVALID_GRAPH_CONFIGURATION;
    if (FAILED(hr = create_shaders(composite, context->d3d_device))) return hr;
    pair[0] = inputs[0];
    ID2D1Bitmap_AddRef(pair[0].bitmap);
    for (i = count == 1 ? 0 : 1; i < count; ++i)
    {
        pair[1] = inputs[i];
        result.rect = pair[0].rect;
        if (count > 1) composite_rect(composite->mode, &pair[1].rect, &result.rect);
        constants.mode = count == 1 ? 13 : composite->mode;
        if (result.rect.left < result.rect.right && result.rect.top < result.rect.bottom)
        {
            constants.dst_shift[0] = result.rect.left - pair[0].rect.left;
            constants.dst_shift[1] = result.rect.top - pair[0].rect.top;
            constants.src_shift[0] = result.rect.left - pair[1].rect.left;
            constants.src_shift[1] = result.rect.top - pair[1].rect.top;
        }
        constants.src_size[0] = pair[1].rect.right - pair[1].rect.left;
        constants.src_size[1] = pair[1].rect.bottom - pair[1].rect.top;
        hr = d2d_effect_render_pixels(context, composite->vs, composite->ps, pair, count == 1 ? 1 : 2,
                &constants, sizeof(constants), NULL, &result);
        ID2D1Bitmap_Release(pair[0].bitmap);
        if (FAILED(hr)) return hr;
        pair[0] = result;
    }
    *output = result;
    return S_OK;
}
