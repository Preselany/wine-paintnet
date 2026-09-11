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
    "cbuffer params : register(b0) { uint2 extent; uint destination_alpha; uint mask_alpha; };\n"
    "[numthreads(16,16,1)] void main(uint3 tid : SV_DispatchThreadID) {\n"
    " if (tid.x >= extent.x || tid.y >= extent.y) return;\n"
    " float4 color = destination.Load(int3(tid.xy, 0));\n"
    " if (destination_alpha == 3) color.a = 1;\n"
    " else if (destination_alpha == 2) color.rgb *= color.a;\n"
    " float alpha = mask_alpha == 3 ? 1 : mask.Load(int3(tid.xy, 0)).a;\n"
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
    ID3DBlob *code = NULL, *errors = NULL;
    HRESULT hr;

    if (ID3D11Device1_GetFeatureLevel(device) < D3D_FEATURE_LEVEL_11_0)
        return D2DERR_INSUFFICIENT_DEVICE_CAPABILITIES;
    hr = D3DCompile(alpha_mask_shader, sizeof(alpha_mask_shader), NULL, NULL, NULL,
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
        ID2D1Bitmap *destination, ID2D1Bitmap *mask, ID2D1Bitmap **output)
{
    struct d2d_bitmap *inputs[] = {unsafe_impl_from_ID2D1Bitmap(destination), unsafe_impl_from_ID2D1Bitmap(mask)};
    struct alpha_mask_effect *alpha_mask = impl_from_ID2D1EffectImpl(effect->impl);
    ID3D11Device1 *device = context->d3d_device;
    ID3D11Device *input_device;
    ID3D11DeviceContext1 *immediate;
    ID3DDeviceContextState *previous;
    ID3D11Texture2D *texture = NULL;
    ID3D11UnorderedAccessView *uav = NULL, *null_uav = NULL;
    ID3D11ShaderResourceView *srvs[2], *null_srvs[2] = {NULL, NULL};
    ID3D11Buffer *constants = NULL;
    IDXGISurface *surface = NULL;
    D3D11_TEXTURE2D_DESC texture_desc = {0};
    D3D11_BUFFER_DESC buffer_desc = {0};
    D3D11_SUBRESOURCE_DATA initial = {0};
    D2D1_BITMAP_PROPERTIES1 bitmap_desc = {{DXGI_FORMAT_R32G32B32A32_FLOAT, D2D1_ALPHA_MODE_PREMULTIPLIED},
            96, 96, D2D1_BITMAP_OPTIONS_NONE, NULL};
    struct d2d_bitmap *bitmap;
    UINT params[4], i;
    HRESULT hr;

    *output = NULL;
    if (effect->impl->lpVtbl != &alpha_mask_vtbl) return E_UNEXPECTED;
    ID3D11ComputeShader_GetDevice(alpha_mask->shader, &input_device);
    hr = input_device == (ID3D11Device *)device ? S_OK : D2DERR_WRONG_RESOURCE_DOMAIN;
    ID3D11Device_Release(input_device);
    if (FAILED(hr)) return hr;
    for (i = 0; i < 2; ++i)
    {
        if (!inputs[i]->srv) return D2DERR_BITMAP_CANNOT_DRAW;
        if (context->target.type == D2D_TARGET_BITMAP && inputs[i]->resource == context->target.bitmap->resource)
            return D2DERR_BITMAP_BOUND_AS_TARGET;
        ID3D11Resource_GetDevice(inputs[i]->resource, &input_device);
        hr = input_device == (ID3D11Device *)device ? S_OK : D2DERR_WRONG_RESOURCE_DOMAIN;
        ID3D11Device_Release(input_device);
        if (FAILED(hr)) return hr;
        srvs[i] = inputs[i]->srv;
    }
    params[0] = min(inputs[0]->pixel_size.width, inputs[1]->pixel_size.width);
    params[1] = min(inputs[0]->pixel_size.height, inputs[1]->pixel_size.height);
    params[2] = inputs[0]->format.alphaMode;
    params[3] = inputs[1]->format.alphaMode;
    /* Effect transforms operate in pixels. Bitmap DPI is compensated by an
     * explicit DPI Compensation effect, not by a pixelwise transform. */
    bitmap_desc.dpiX = context->drawing_state.unitMode == D2D1_UNIT_MODE_PIXELS ? 96 : context->desc.dpiX;
    bitmap_desc.dpiY = context->drawing_state.unitMode == D2D1_UNIT_MODE_PIXELS ? 96 : context->desc.dpiY;
    if (!params[0] || !params[1])
    {
        D2D1_SIZE_U size = {params[0], params[1]};
        if (SUCCEEDED(hr = d2d_bitmap_create(context, size, NULL, 0, &bitmap_desc, &bitmap)))
            *output = (ID2D1Bitmap *)&bitmap->ID2D1Bitmap1_iface;
        return hr;
    }
    texture_desc.Width = params[0];
    texture_desc.Height = params[1];
    texture_desc.MipLevels = texture_desc.ArraySize = texture_desc.SampleDesc.Count = 1;
    texture_desc.Format = bitmap_desc.pixelFormat.format;
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    if (FAILED(hr = ID3D11Device1_CreateTexture2D(device, &texture_desc, NULL, &texture))) goto done;
    if (FAILED(hr = ID3D11Device1_CreateUnorderedAccessView(device, (ID3D11Resource *)texture, NULL, &uav))) goto done;
    buffer_desc.ByteWidth = sizeof(params);
    buffer_desc.Usage = D3D11_USAGE_IMMUTABLE;
    buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    initial.pSysMem = params;
    if (FAILED(hr = ID3D11Device1_CreateBuffer(device, &buffer_desc, &initial, &constants))) goto done;

    if (context->cs) EnterCriticalSection(context->cs);
    ID3D11Device1_GetImmediateContext1(device, &immediate);
    ID3D11DeviceContext1_SwapDeviceContextState(immediate, context->d3d_state, &previous);
    ID3D11DeviceContext1_OMSetRenderTargets(immediate, 0, NULL, NULL);
    ID3D11DeviceContext1_CSSetShader(immediate, alpha_mask->shader, NULL, 0);
    ID3D11DeviceContext1_CSSetShaderResources(immediate, 0, 2, srvs);
    ID3D11DeviceContext1_CSSetUnorderedAccessViews(immediate, 0, 1, &uav, NULL);
    ID3D11DeviceContext1_CSSetConstantBuffers(immediate, 0, 1, &constants);
    ID3D11DeviceContext1_Dispatch(immediate, (params[0] + 15) / 16, (params[1] + 15) / 16, 1);
    ID3D11DeviceContext1_CSSetUnorderedAccessViews(immediate, 0, 1, &null_uav, NULL);
    ID3D11DeviceContext1_CSSetShaderResources(immediate, 0, 2, null_srvs);
    ID3D11DeviceContext1_SwapDeviceContextState(immediate, previous, NULL);
    ID3DDeviceContextState_Release(previous);
    ID3D11DeviceContext1_Release(immediate);
    if (context->cs) LeaveCriticalSection(context->cs);

    if (FAILED(hr = ID3D11Texture2D_QueryInterface(texture, &IID_IDXGISurface, (void **)&surface))) goto done;
    if (SUCCEEDED(hr = d2d_bitmap_create_shared(context, &IID_IDXGISurface, surface, &bitmap_desc, &bitmap)))
        *output = (ID2D1Bitmap *)&bitmap->ID2D1Bitmap1_iface;
done:
    if (surface) IDXGISurface_Release(surface);
    if (constants) ID3D11Buffer_Release(constants);
    if (uav) ID3D11UnorderedAccessView_Release(uav);
    if (texture) ID3D11Texture2D_Release(texture);
    return hr;
}
