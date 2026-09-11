/* Direct2D histogram analysis effect.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "d2d1_private.h"
#include "d3dcompiler.h"

WINE_DEFAULT_DEBUG_CHANNEL(d2d);

struct histogram_effect
{
    ID2D1EffectImpl ID2D1EffectImpl_iface;
    LONG refcount;
    ID3D11ComputeShader *shader;
    UINT32 num_bins, channel;
    float output[1024];
    BOOL valid;
};

static const char histogram_shader[] =
    "Texture2D<float4> image : register(t0);\n"
    "RWStructuredBuffer<uint> histogram : register(u0);\n"
    "cbuffer params : register(b0) { uint2 origin; uint2 extent; uint bins; uint channel; uint alpha_mode; uint pad; };\n"
    "[numthreads(16,16,1)] void main(uint3 tid : SV_DispatchThreadID) {\n"
    " if (tid.x >= extent.x || tid.y >= extent.y) return;\n"
    " float4 pixel = image.Load(int3(tid.xy + origin, 0));\n"
    " if (alpha_mode == 3) pixel.a = 1;\n"
    " if (alpha_mode == 1) pixel.rgb = pixel.a > 0 ? pixel.rgb / pixel.a : 0;\n"
    " uint bin = min((uint)(saturate(pixel[channel]) * bins), bins - 1);\n"
    " InterlockedAdd(histogram[bin], 1);\n"
    "}\n";

static inline struct histogram_effect *impl_from_ID2D1EffectImpl(ID2D1EffectImpl *iface)
{
    return CONTAINING_RECORD(iface, struct histogram_effect, ID2D1EffectImpl_iface);
}

static HRESULT STDMETHODCALLTYPE histogram_QueryInterface(ID2D1EffectImpl *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_ID2D1EffectImpl) && !IsEqualGUID(iid, &IID_IUnknown))
        return E_NOINTERFACE;
    *out = iface;
    ID2D1EffectImpl_AddRef(iface);
    return S_OK;
}

static ULONG STDMETHODCALLTYPE histogram_AddRef(ID2D1EffectImpl *iface)
{
    struct histogram_effect *effect = impl_from_ID2D1EffectImpl(iface);
    return InterlockedIncrement(&effect->refcount);
}

static ULONG STDMETHODCALLTYPE histogram_Release(ID2D1EffectImpl *iface)
{
    struct histogram_effect *effect = impl_from_ID2D1EffectImpl(iface);
    ULONG refcount = InterlockedDecrement(&effect->refcount);
    if (!refcount)
    {
        if (effect->shader) ID3D11ComputeShader_Release(effect->shader);
        free(effect);
    }
    return refcount;
}

static HRESULT STDMETHODCALLTYPE histogram_Initialize(ID2D1EffectImpl *iface, ID2D1EffectContext *context,
        ID2D1TransformGraph *graph)
{
    struct histogram_effect *effect = impl_from_ID2D1EffectImpl(iface);
    struct d2d_effect_context *effect_context = CONTAINING_RECORD(context,
            struct d2d_effect_context, ID2D1EffectContext1_iface);
    ID3D11Device1 *device = effect_context->device_context->d3d_device;
    ID3DBlob *code = NULL, *errors = NULL;
    HRESULT hr;

    /* The current implementation uses SM5 atomic operations. */
    if (ID3D11Device1_GetFeatureLevel(device) < D3D_FEATURE_LEVEL_11_0)
        return D2DERR_INSUFFICIENT_DEVICE_CAPABILITIES;
    hr = D3DCompile(histogram_shader, sizeof(histogram_shader), NULL, NULL, NULL,
            "main", "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
    if (errors)
    {
        WARN("%s\n", (char *)ID3D10Blob_GetBufferPointer(errors));
        ID3D10Blob_Release(errors);
    }
    if (FAILED(hr)) return hr;
    hr = ID3D11Device1_CreateComputeShader(device, ID3D10Blob_GetBufferPointer(code),
            ID3D10Blob_GetBufferSize(code), NULL, &effect->shader);
    ID3D10Blob_Release(code);
    return hr;
}

static HRESULT STDMETHODCALLTYPE histogram_PrepareForRender(ID2D1EffectImpl *iface, D2D1_CHANGE_TYPE type)
{
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE histogram_SetGraph(ID2D1EffectImpl *iface, ID2D1TransformGraph *graph)
{
    return E_NOTIMPL;
}

static const ID2D1EffectImplVtbl histogram_vtbl =
{
    histogram_QueryInterface, histogram_AddRef, histogram_Release,
    histogram_Initialize, histogram_PrepareForRender, histogram_SetGraph,
};

static HRESULT CALLBACK histogram_factory(IUnknown **out)
{
    struct histogram_effect *effect = calloc(1, sizeof(*effect));
    if (!effect) return E_OUTOFMEMORY;
    effect->ID2D1EffectImpl_iface.lpVtbl = &histogram_vtbl;
    effect->refcount = 1;
    effect->num_bins = 256;
    *out = (IUnknown *)&effect->ID2D1EffectImpl_iface;
    return S_OK;
}

static HRESULT histogram_get_uint32(UINT32 value, BYTE *data, UINT32 size, UINT32 *actual)
{
    if (actual) *actual = sizeof(value);
    if (!data) return S_OK;
    if (size < sizeof(value)) return E_NOT_SUFFICIENT_BUFFER;
    memcpy(data, &value, sizeof(value));
    return S_OK;
}

static HRESULT CALLBACK histogram_bins_get(const IUnknown *iface, BYTE *data, UINT32 size, UINT32 *actual)
{
    const struct histogram_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    return histogram_get_uint32(effect->num_bins, data, size, actual);
}

static HRESULT CALLBACK histogram_bins_set(IUnknown *iface, const BYTE *data, UINT32 size)
{
    struct histogram_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    UINT32 value;
    if (!data || size != sizeof(value)) return E_INVALIDARG;
    memcpy(&value, data, sizeof(value));
    if (value < 2 || value > 1024) return E_INVALIDARG;
    effect->num_bins = value;
    effect->valid = FALSE;
    return S_OK;
}

static HRESULT CALLBACK histogram_channel_get(const IUnknown *iface, BYTE *data, UINT32 size, UINT32 *actual)
{
    const struct histogram_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    return histogram_get_uint32(effect->channel, data, size, actual);
}

static HRESULT CALLBACK histogram_channel_set(IUnknown *iface, const BYTE *data, UINT32 size)
{
    struct histogram_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    UINT32 value;
    if (!data || size != sizeof(value)) return E_INVALIDARG;
    memcpy(&value, data, sizeof(value));
    if (value > D2D1_CHANNEL_SELECTOR_A) return E_INVALIDARG;
    effect->channel = value;
    effect->valid = FALSE;
    return S_OK;
}

static HRESULT CALLBACK histogram_output_get(const IUnknown *iface, BYTE *data, UINT32 size, UINT32 *actual)
{
    const struct histogram_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface);
    UINT32 required = effect->num_bins * sizeof(float);
    if (actual) *actual = required;
    if (!data) return S_OK;
    if (size < required) return E_NOT_SUFFICIENT_BUFFER;
    if (!effect->valid) return D2DERR_WRONG_STATE;
    memcpy(data, effect->output, required);
    return S_OK;
}

void d2d_histogram_init_builtin(struct d2d_factory *factory)
{
    static const WCHAR description[] = L"<?xml version='1.0'?><Effect>"
        L"<Property name='DisplayName' type='string' value='Histogram'/>"
        L"<Property name='Author' type='string' value='The Wine Project'/>"
        L"<Property name='Category' type='string' value='Analysis'/>"
        L"<Property name='Description' type='string' value='Computes a normalized color-channel histogram'/>"
        L"<Inputs minimum='1' maximum='1'><Input name='Source'/></Inputs>"
        L"<Property name='NumBins' type='uint32' value='256'/>"
        L"<Property name='ChannelSelect' type='enum' value='0'/>"
        L"<Property name='HistogramOutput' type='blob'/></Effect>";
    static const D2D1_PROPERTY_BINDING bindings[] =
    {
        {L"NumBins", histogram_bins_set, histogram_bins_get},
        {L"ChannelSelect", histogram_channel_set, histogram_channel_get},
        {L"HistogramOutput", NULL, histogram_output_get},
    };
    HRESULT hr;

    if (FAILED(hr = d2d_factory_register_builtin_effect(factory, &CLSID_D2D1Histogram,
            description, bindings, ARRAY_SIZE(bindings), histogram_factory)))
        WARN("Failed to register Histogram, hr %#lx.\n", hr);
}

HRESULT d2d_histogram_draw(struct d2d_effect *effect, struct d2d_device_context *context,
        const D2D1_RECT_F *image_rect)
{
    struct histogram_effect *histogram;
    struct d2d_bitmap *bitmap;
    struct d2d_effect_image input;
    ID3D11Device *input_device;
    ID3D11Device1 *device = context->d3d_device;
    ID3D11DeviceContext1 *immediate;
    ID3DDeviceContextState *previous;
    ID3D11Buffer *counts = NULL, *staging = NULL, *constants = NULL;
    ID3D11UnorderedAccessView *uav = NULL, *null_uav = NULL;
    ID3D11ShaderResourceView *null_srv = NULL;
    D3D11_BUFFER_DESC desc = {0};
    D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {0};
    D3D11_SUBRESOURCE_DATA initial;
    D3D11_MAPPED_SUBRESOURCE mapped;
    const UINT clear[] = {0, 0, 0, 0};
    UINT params[8], i, total;
    HRESULT hr;

    if (effect->impl->lpVtbl != &histogram_vtbl) return E_UNEXPECTED;
    histogram = impl_from_ID2D1EffectImpl(effect->impl);
    histogram->valid = FALSE;
    if (!effect->input_count || !effect->inputs[0]) return D2DERR_WRONG_STATE;
    if ((hr = d2d_effect_resolve_image(context, effect->inputs[0], &input)) != S_OK)
    {
        if (hr == S_FALSE) FIXME("Histogram input effect graph is not implemented.\n");
        return hr == S_FALSE ? E_NOTIMPL : hr;
    }
    bitmap = unsafe_impl_from_ID2D1Bitmap(input.bitmap);
    if (!bitmap->srv)
    {
        hr = D2DERR_BITMAP_CANNOT_DRAW;
        goto done;
    }
    if (context->target.type == D2D_TARGET_BITMAP && bitmap->resource == context->target.bitmap->resource)
    {
        hr = D2DERR_BITMAP_BOUND_AS_TARGET;
        goto done;
    }
    ID3D11ComputeShader_GetDevice(histogram->shader, &input_device);
    hr = input_device == (ID3D11Device *)device ? S_OK : D2DERR_WRONG_RESOURCE_DOMAIN;
    ID3D11Device_Release(input_device);
    if (FAILED(hr)) goto done;
    ID3D11Resource_GetDevice(bitmap->resource, &input_device);
    hr = input_device == (ID3D11Device *)device ? S_OK : D2DERR_WRONG_RESOURCE_DOMAIN;
    ID3D11Device_Release(input_device);
    if (FAILED(hr)) goto done;

    params[0] = params[1] = 0;
    params[2] = bitmap->pixel_size.width;
    params[3] = bitmap->pixel_size.height;
    if (image_rect)
    {
        float scale_x = context->drawing_state.unitMode == D2D1_UNIT_MODE_PIXELS ? 1.0f : context->desc.dpiX / 96.0f;
        float scale_y = context->drawing_state.unitMode == D2D1_UNIT_MODE_PIXELS ? 1.0f : context->desc.dpiY / 96.0f;
        float left = ceilf(image_rect->left * scale_x - 0.5f) - input.rect.left;
        float top = ceilf(image_rect->top * scale_y - 0.5f) - input.rect.top;
        float right = ceilf(image_rect->right * scale_x - 0.5f) - input.rect.left;
        float bottom = ceilf(image_rect->bottom * scale_y - 0.5f) - input.rect.top;
        if (!isfinite(left) || !isfinite(top) || !isfinite(right) || !isfinite(bottom))
        {
            hr = E_INVALIDARG;
            goto done;
        }
        params[0] = min(max(left, 0), bitmap->pixel_size.width);
        params[1] = min(max(top, 0), bitmap->pixel_size.height);
        params[2] = max(min(right, bitmap->pixel_size.width), params[0]) - params[0];
        params[3] = max(min(bottom, bitmap->pixel_size.height), params[1]) - params[1];
    }
    params[4] = histogram->num_bins;
    params[5] = histogram->channel;
    params[6] = bitmap->format.alphaMode;
    params[7] = 0;
    total = params[2] * params[3];
    if (!total)
    {
        memset(histogram->output, 0, sizeof(histogram->output));
        histogram->valid = TRUE;
        hr = S_OK;
        goto done;
    }

    desc.ByteWidth = histogram->num_bins * sizeof(UINT);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = sizeof(UINT);
    if (FAILED(hr = ID3D11Device1_CreateBuffer(device, &desc, NULL, &counts))) goto done;
    uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uav_desc.Buffer.NumElements = histogram->num_bins;
    if (FAILED(hr = ID3D11Device1_CreateUnorderedAccessView(device, (ID3D11Resource *)counts, &uav_desc, &uav))) goto done;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = desc.MiscFlags = desc.StructureByteStride = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(hr = ID3D11Device1_CreateBuffer(device, &desc, NULL, &staging))) goto done;
    desc.ByteWidth = sizeof(params);
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = 0;
    initial.pSysMem = params;
    initial.SysMemPitch = initial.SysMemSlicePitch = 0;
    if (FAILED(hr = ID3D11Device1_CreateBuffer(device, &desc, &initial, &constants))) goto done;

    if (context->cs) EnterCriticalSection(context->cs);
    ID3D11Device1_GetImmediateContext1(device, &immediate);
    ID3D11DeviceContext1_SwapDeviceContextState(immediate, context->d3d_state, &previous);
    ID3D11DeviceContext1_OMSetRenderTargets(immediate, 0, NULL, NULL);
    ID3D11DeviceContext1_ClearUnorderedAccessViewUint(immediate, uav, clear);
    ID3D11DeviceContext1_CSSetShader(immediate, histogram->shader, NULL, 0);
    ID3D11DeviceContext1_CSSetShaderResources(immediate, 0, 1, &bitmap->srv);
    ID3D11DeviceContext1_CSSetUnorderedAccessViews(immediate, 0, 1, &uav, NULL);
    ID3D11DeviceContext1_CSSetConstantBuffers(immediate, 0, 1, &constants);
    ID3D11DeviceContext1_Dispatch(immediate, (params[2] + 15) / 16, (params[3] + 15) / 16, 1);
    ID3D11DeviceContext1_CSSetUnorderedAccessViews(immediate, 0, 1, &null_uav, NULL);
    ID3D11DeviceContext1_CSSetShaderResources(immediate, 0, 1, &null_srv);
    ID3D11DeviceContext1_CopyResource(immediate, (ID3D11Resource *)staging, (ID3D11Resource *)counts);
    hr = ID3D11DeviceContext1_Map(immediate, (ID3D11Resource *)staging, 0, D3D11_MAP_READ, 0, &mapped);
    if (SUCCEEDED(hr))
    {
        for (i = 0; i < histogram->num_bins; ++i)
            histogram->output[i] = ((UINT *)mapped.pData)[i] / (float)total;
        ID3D11DeviceContext1_Unmap(immediate, (ID3D11Resource *)staging, 0);
        histogram->valid = TRUE;
    }
    ID3D11DeviceContext1_SwapDeviceContextState(immediate, previous, NULL);
    ID3DDeviceContextState_Release(previous);
    ID3D11DeviceContext1_Release(immediate);
    if (context->cs) LeaveCriticalSection(context->cs);
done:
    if (constants) ID3D11Buffer_Release(constants);
    if (staging) ID3D11Buffer_Release(staging);
    if (uav) ID3D11UnorderedAccessView_Release(uav);
    if (counts) ID3D11Buffer_Release(counts);
    ID2D1Bitmap_Release(input.bitmap);
    return hr;
}
