/* Direct2D Crop effect.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "d2d1_private.h"
#include "d3dcompiler.h"

WINE_DEFAULT_DEBUG_CHANNEL(d2d);

struct crop_effect
{
    ID2D1EffectImpl ID2D1EffectImpl_iface;
    LONG refcount;
    ID3D11VertexShader *vs;
    ID3D11PixelShader *ps;
    D2D1_VECTOR_4F rect;
    D2D1_BORDER_MODE border;
};

static struct crop_effect *impl_from_ID2D1EffectImpl(ID2D1EffectImpl *iface)
{
    return CONTAINING_RECORD(iface, struct crop_effect, ID2D1EffectImpl_iface);
}

static HRESULT STDMETHODCALLTYPE crop_QueryInterface(ID2D1EffectImpl *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_ID2D1EffectImpl) && !IsEqualGUID(iid, &IID_IUnknown)) return E_NOINTERFACE;
    ID2D1EffectImpl_AddRef(iface);
    *out = iface;
    return S_OK;
}

static ULONG STDMETHODCALLTYPE crop_AddRef(ID2D1EffectImpl *iface)
{
    return InterlockedIncrement(&impl_from_ID2D1EffectImpl(iface)->refcount);
}

static ULONG STDMETHODCALLTYPE crop_Release(ID2D1EffectImpl *iface)
{
    struct crop_effect *effect = impl_from_ID2D1EffectImpl(iface);
    ULONG refs = InterlockedDecrement(&effect->refcount);
    if (!refs)
    {
        if (effect->vs) ID3D11VertexShader_Release(effect->vs);
        if (effect->ps) ID3D11PixelShader_Release(effect->ps);
        free(effect);
    }
    return refs;
}

static HRESULT STDMETHODCALLTYPE crop_Initialize(ID2D1EffectImpl *iface,
        ID2D1EffectContext *context, ID2D1TransformGraph *graph)
{
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE crop_PrepareForRender(ID2D1EffectImpl *iface, D2D1_CHANGE_TYPE change)
{
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE crop_SetGraph(ID2D1EffectImpl *iface, ID2D1TransformGraph *graph)
{
    return E_NOTIMPL;
}

static const ID2D1EffectImplVtbl crop_vtbl =
{
    crop_QueryInterface, crop_AddRef, crop_Release,
    crop_Initialize, crop_PrepareForRender, crop_SetGraph,
};

static HRESULT CALLBACK crop_factory(IUnknown **out)
{
    struct crop_effect *effect = calloc(1, sizeof(*effect));
    if (!effect) return E_OUTOFMEMORY;
    effect->ID2D1EffectImpl_iface.lpVtbl = &crop_vtbl;
    effect->refcount = 1;
    effect->rect = (D2D1_VECTOR_4F){-INFINITY, -INFINITY, INFINITY, INFINITY};
    *out = (IUnknown *)&effect->ID2D1EffectImpl_iface;
    return S_OK;
}

static HRESULT create_shaders(struct crop_effect *effect, ID3D11Device1 *device)
{
    static const char vertex_source[] =
        "float4 main(uint id:SV_VertexID):SV_POSITION {"
        "float2 uv=float2((id<<1)&2,id&2);"
        "return float4(uv*float2(2,-2)+float2(-1,1),0,1);}";
    static const char pixel_source[] =
        "Texture2D<float4> image:register(t0);"
        "cbuffer C:register(b0) {int2 shift;uint border;uint nan_edge;float4 edges;float4 weights;}"
        "float4 main(float4 pos:SV_POSITION):SV_TARGET {"
        "float2 p=floor(pos.xy);float4 color=image.Load(int3(int2(p)+shift,0));"
        "if(!border){if(nan_edge)return weights.xxxx;"
        "if(p.x==edges.x)color*=weights.x;if(p.y==edges.y)color*=weights.y;"
        "if(p.x==edges.z)color*=weights.z;if(p.y==edges.w)color*=weights.w;}return color;}";
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

static HRESULT crop_property_get(const void *value, UINT value_size, BYTE *data, UINT size, UINT *actual)
{
    if (actual) *actual = value_size;
    if (!data) return S_OK;
    if (size < value_size) return E_NOT_SUFFICIENT_BUFFER;
    memcpy(data, value, value_size);
    return S_OK;
}

static HRESULT CALLBACK rect_get(const IUnknown *iface, BYTE *data, UINT size, UINT *actual)
{
    const struct crop_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    return crop_property_get(&effect->rect, sizeof(effect->rect), data, size, actual);
}

static HRESULT CALLBACK rect_set(IUnknown *iface, const BYTE *data, UINT size)
{
    struct crop_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    float temporary;
    if (!data || size != sizeof(effect->rect)) return E_INVALIDARG;
    memcpy(&effect->rect, data, sizeof(effect->rect));
    if (effect->rect.x > effect->rect.z)
    {
        temporary = effect->rect.x;
        effect->rect.x = effect->rect.z;
        effect->rect.z = temporary;
    }
    if (effect->rect.y > effect->rect.w)
    {
        temporary = effect->rect.y;
        effect->rect.y = effect->rect.w;
        effect->rect.w = temporary;
    }
    return S_OK;
}

static HRESULT CALLBACK border_get(const IUnknown *iface, BYTE *data, UINT size, UINT *actual)
{
    const struct crop_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    return crop_property_get(&effect->border, sizeof(effect->border), data, size, actual);
}

static HRESULT CALLBACK border_set(IUnknown *iface, const BYTE *data, UINT size)
{
    struct crop_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    D2D1_BORDER_MODE value;
    if (!data || size != sizeof(value)) return E_INVALIDARG;
    memcpy(&value, data, sizeof(value));
    if (value > D2D1_BORDER_MODE_HARD) return E_INVALIDARG;
    effect->border = value;
    return S_OK;
}

void d2d_crop_init_builtin(struct d2d_factory *factory)
{
    static const WCHAR description[] =
        L"<?xml version='1.0'?><Effect><Property name='DisplayName' type='string' value='Crop'/>"
        L"<Property name='Author' type='string' value='The Wine Project'/>"
        L"<Property name='Category' type='string' value='Transform'/>"
        L"<Property name='Description' type='string' value='Crops an image to a rectangle.'/>"
        L"<Inputs minimum='1' maximum='1'><Input name='Source'/></Inputs>"
        L"<Property name='Rect' type='vector4'/>"
        L"<Property name='BorderMode' type='enum'/></Effect>";
    static const D2D1_PROPERTY_BINDING bindings[] =
    {
        {L"Rect", rect_set, rect_get},
        {L"BorderMode", border_set, border_get},
    };
    HRESULT hr;
    if (FAILED(hr = d2d_factory_register_builtin_effect(factory, &CLSID_D2D1Crop,
            description, bindings, ARRAY_SIZE(bindings), crop_factory)))
        WARN("Failed to register Crop, hr %#lx.\n", hr);
}

static D2D1_RECT_F crop_pixel_rect(const struct crop_effect *effect, const struct d2d_device_context *context)
{
    float sx = context->drawing_state.unitMode == D2D1_UNIT_MODE_PIXELS ? 1 : context->desc.dpiX / 96;
    float sy = context->drawing_state.unitMode == D2D1_UNIT_MODE_PIXELS ? 1 : context->desc.dpiY / 96;
    const D2D1_VECTOR_4F *r = &effect->rect;
    return (D2D1_RECT_F){r->x * sx, r->y * sy, r->z * sx, r->w * sy};
}

static LONG crop_round_coordinate(float value)
{
    double rounded = floor((double)value + .5);
    if (isnan(value)) return INT_MIN;
    if (rounded <= INT_MIN) return INT_MIN;
    if (rounded >= INT_MAX) return INT_MAX;
    return rounded;
}

HRESULT d2d_crop_bounds(struct d2d_effect *effect, struct d2d_device_context *context,
        const D2D1_RECT_L *input, D2D1_RECT_L *output)
{
    struct crop_effect *crop = impl_from_ID2D1EffectImpl(effect->impl);
    D2D1_RECT_F rect;
    if (effect->impl->lpVtbl != &crop_vtbl) return E_UNEXPECTED;
    rect = crop_pixel_rect(crop, context);
    output->left = max(input->left, crop_round_coordinate(rect.left));
    output->top = max(input->top, crop_round_coordinate(rect.top));
    output->right = min(input->right, crop_round_coordinate(rect.right));
    output->bottom = min(input->bottom, crop_round_coordinate(rect.bottom));
    if (output->left >= output->right || output->top >= output->bottom)
        memset(output, 0, sizeof(*output));
    return S_OK;
}

HRESULT d2d_crop_render(struct d2d_effect *effect, struct d2d_device_context *context,
        const struct d2d_effect_image *input, struct d2d_effect_image *output)
{
    struct crop_effect *crop = impl_from_ID2D1EffectImpl(effect->impl);
    struct
    {
        LONG shift[2];
        UINT border, nan_edge;
        D2D1_RECT_F edges;
        D2D1_VECTOR_4F weights;
    } constants;
    D2D1_RECT_F rect;
    HRESULT hr;
    if (effect->impl->lpVtbl != &crop_vtbl) return E_UNEXPECTED;
    if (FAILED(hr = create_shaders(crop, context->d3d_device))) return hr;
    constants.shift[0] = output->rect.left < output->rect.right ? output->rect.left - input->rect.left : 0;
    constants.shift[1] = output->rect.top < output->rect.bottom ? output->rect.top - input->rect.top : 0;
    constants.border = crop->border;
    rect = crop_pixel_rect(crop, context);
    constants.nan_edge = isnan(rect.left) || isnan(rect.top);
    constants.edges = (D2D1_RECT_F){floorf(rect.left) - output->rect.left, floorf(rect.top) - output->rect.top,
            floorf(rect.right) - output->rect.left, floorf(rect.bottom) - output->rect.top};
    /* Native Crop uses signed fractional parts, including for negative edges. */
    constants.weights = (D2D1_VECTOR_4F){1 - fmodf(rect.left, 1), 1 - fmodf(rect.top, 1),
            fmodf(rect.right, 1), fmodf(rect.bottom, 1)};
    if (constants.nan_edge) constants.weights.x = NAN;
    return d2d_effect_render_pixels(context, crop->vs, crop->ps, input, 1,
            &constants, sizeof(constants), NULL, output);
}
