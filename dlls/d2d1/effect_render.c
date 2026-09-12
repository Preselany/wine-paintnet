/* Shared compute rendering for builtin Direct2D image effects.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "d2d1_private.h"
#include "d3dcompiler.h"

WINE_DEFAULT_DEBUG_CHANNEL(d2d);

HRESULT d2d_effect_compile_compute_shader(ID3D11Device1 *device, const char *source,
        ID3D11ComputeShader **shader)
{
    ID3DBlob *code = NULL, *errors = NULL;
    HRESULT hr;

    if (ID3D11Device1_GetFeatureLevel(device) < D3D_FEATURE_LEVEL_11_0)
        return D2DERR_INSUFFICIENT_DEVICE_CAPABILITIES;
    hr = D3DCompile(source, strlen(source), NULL, NULL, NULL,
            "main", "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
    if (errors)
    {
        WARN("%s\n", (char *)ID3D10Blob_GetBufferPointer(errors));
        ID3D10Blob_Release(errors);
    }
    if (FAILED(hr)) return hr;
    hr = ID3D11Device1_CreateComputeShader(device, ID3D10Blob_GetBufferPointer(code),
            ID3D10Blob_GetBufferSize(code), NULL, shader);
    ID3D10Blob_Release(code);
    return hr;
}

HRESULT d2d_effect_dispatch(struct d2d_device_context *context, ID3D11ComputeShader *shader,
        const struct d2d_effect_image *inputs, unsigned int count, const void *data, UINT data_size,
        ID3D11ShaderResourceView *extra, struct d2d_effect_image *output)
{
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
    D2D1_SIZE_U size;
    unsigned int i;
    HRESULT hr;

    if (count > ARRAY_SIZE(srvs)) return E_INVALIDARG;
    output->bitmap = NULL;
    if ((INT64)output->rect.right - output->rect.left > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION
            || (INT64)output->rect.bottom - output->rect.top > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
        return D2DERR_EXCEEDS_MAX_BITMAP_SIZE;
    size.width = output->rect.right - output->rect.left;
    size.height = output->rect.bottom - output->rect.top;
    ID3D11ComputeShader_GetDevice(shader, &input_device);
    hr = input_device == (ID3D11Device *)device ? S_OK : D2DERR_WRONG_RESOURCE_DOMAIN;
    ID3D11Device_Release(input_device);
    if (FAILED(hr)) return hr;
    for (i = 0; i < count; ++i)
    {
        struct d2d_bitmap *input = unsafe_impl_from_ID2D1Bitmap(inputs[i].bitmap);
        if (!input->srv) return D2DERR_BITMAP_CANNOT_DRAW;
        if (context->target.type == D2D_TARGET_BITMAP && input->resource == context->target.bitmap->resource)
            return D2DERR_BITMAP_BOUND_AS_TARGET;
        ID3D11Resource_GetDevice(input->resource, &input_device);
        hr = input_device == (ID3D11Device *)device ? S_OK : D2DERR_WRONG_RESOURCE_DOMAIN;
        ID3D11Device_Release(input_device);
        if (FAILED(hr)) return hr;
        srvs[i] = input->srv;
    }
    bitmap_desc.dpiX = context->drawing_state.unitMode == D2D1_UNIT_MODE_PIXELS ? 96 : context->desc.dpiX;
    bitmap_desc.dpiY = context->drawing_state.unitMode == D2D1_UNIT_MODE_PIXELS ? 96 : context->desc.dpiY;
    if (!size.width || !size.height)
    {
        if (SUCCEEDED(hr = d2d_bitmap_create(context, size, NULL, 0, &bitmap_desc, &bitmap)))
            output->bitmap = (ID2D1Bitmap *)&bitmap->ID2D1Bitmap1_iface;
        return hr;
    }
    texture_desc.Width = size.width;
    texture_desc.Height = size.height;
    texture_desc.MipLevels = texture_desc.ArraySize = texture_desc.SampleDesc.Count = 1;
    texture_desc.Format = bitmap_desc.pixelFormat.format;
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    if (FAILED(hr = ID3D11Device1_CreateTexture2D(device, &texture_desc, NULL, &texture))) goto done;
    if (FAILED(hr = ID3D11Device1_CreateUnorderedAccessView(device, (ID3D11Resource *)texture, NULL, &uav))) goto done;
    buffer_desc.ByteWidth = data_size;
    buffer_desc.Usage = D3D11_USAGE_IMMUTABLE;
    buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    initial.pSysMem = data;
    if (FAILED(hr = ID3D11Device1_CreateBuffer(device, &buffer_desc, &initial, &constants))) goto done;

    if (context->cs) EnterCriticalSection(context->cs);
    ID3D11Device1_GetImmediateContext1(device, &immediate);
    ID3D11DeviceContext1_SwapDeviceContextState(immediate, context->d3d_state, &previous);
    ID3D11DeviceContext1_OMSetRenderTargets(immediate, 0, NULL, NULL);
    ID3D11DeviceContext1_CSSetShader(immediate, shader, NULL, 0);
    ID3D11DeviceContext1_CSSetShaderResources(immediate, 0, count, srvs);
    ID3D11DeviceContext1_CSSetShaderResources(immediate, 2, 1, &extra);
    ID3D11DeviceContext1_CSSetUnorderedAccessViews(immediate, 0, 1, &uav, NULL);
    ID3D11DeviceContext1_CSSetConstantBuffers(immediate, 0, 1, &constants);
    ID3D11DeviceContext1_Dispatch(immediate, (size.width + 15) / 16, (size.height + 15) / 16, 1);
    ID3D11DeviceContext1_CSSetUnorderedAccessViews(immediate, 0, 1, &null_uav, NULL);
    ID3D11DeviceContext1_CSSetShaderResources(immediate, 0, 2, null_srvs);
    ID3D11DeviceContext1_CSSetShaderResources(immediate, 2, 1, null_srvs);
    ID3D11DeviceContext1_SwapDeviceContextState(immediate, previous, NULL);
    ID3DDeviceContextState_Release(previous);
    ID3D11DeviceContext1_Release(immediate);
    if (context->cs) LeaveCriticalSection(context->cs);

    if (FAILED(hr = ID3D11Texture2D_QueryInterface(texture, &IID_IDXGISurface, (void **)&surface))) goto done;
    if (SUCCEEDED(hr = d2d_bitmap_create_shared(context, &IID_IDXGISurface, surface, &bitmap_desc, &bitmap)))
        output->bitmap = (ID2D1Bitmap *)&bitmap->ID2D1Bitmap1_iface;
done:
    if (surface) IDXGISurface_Release(surface);
    if (constants) ID3D11Buffer_Release(constants);
    if (uav) ID3D11UnorderedAccessView_Release(uav);
    if (texture) ID3D11Texture2D_Release(texture);
    return hr;
}

HRESULT d2d_effect_render_pixels(struct d2d_device_context *context, ID3D11VertexShader *vs,
        ID3D11PixelShader *ps,
        const struct d2d_effect_image *inputs, unsigned int count, const void *data, UINT data_size,
        ID3D11ShaderResourceView *extra, struct d2d_effect_image *output)
{
    ID3D11Device1 *device = context->d3d_device;
    ID3D11Device *input_device;
    ID3D11DeviceContext1 *immediate;
    ID3DDeviceContextState *previous;
    ID3D11Texture2D *texture = NULL;
    ID3D11RenderTargetView *rtv = NULL;
    D3D11_VIEWPORT viewport = {0};
    ID3D11ShaderResourceView *srvs[2];
    ID3D11Buffer *constants = NULL;
    IDXGISurface *surface = NULL;
    D3D11_TEXTURE2D_DESC texture_desc = {0};
    D3D11_BUFFER_DESC buffer_desc = {0};
    D3D11_SUBRESOURCE_DATA initial = {0};
    D2D1_BITMAP_PROPERTIES1 bitmap_desc = {{DXGI_FORMAT_R32G32B32A32_FLOAT, D2D1_ALPHA_MODE_PREMULTIPLIED},
            96, 96, D2D1_BITMAP_OPTIONS_NONE, NULL};
    struct d2d_bitmap *bitmap;
    D2D1_SIZE_U size;
    unsigned int i;
    HRESULT hr;

    if (count > ARRAY_SIZE(srvs)) return E_INVALIDARG;
    output->bitmap = NULL;
    if ((INT64)output->rect.right - output->rect.left > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION
            || (INT64)output->rect.bottom - output->rect.top > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
        return D2DERR_EXCEEDS_MAX_BITMAP_SIZE;
    size.width = output->rect.right - output->rect.left;
    size.height = output->rect.bottom - output->rect.top;
    ID3D11PixelShader_GetDevice(ps, &input_device);
    hr = input_device == (ID3D11Device *)device ? S_OK : D2DERR_WRONG_RESOURCE_DOMAIN;
    ID3D11Device_Release(input_device);
    if (FAILED(hr)) return hr;
    for (i = 0; i < count; ++i)
    {
        struct d2d_bitmap *input = unsafe_impl_from_ID2D1Bitmap(inputs[i].bitmap);
        if (!input->srv) return D2DERR_BITMAP_CANNOT_DRAW;
        if (context->target.type == D2D_TARGET_BITMAP && input->resource == context->target.bitmap->resource)
            return D2DERR_BITMAP_BOUND_AS_TARGET;
        ID3D11Resource_GetDevice(input->resource, &input_device);
        hr = input_device == (ID3D11Device *)device ? S_OK : D2DERR_WRONG_RESOURCE_DOMAIN;
        ID3D11Device_Release(input_device);
        if (FAILED(hr)) return hr;
        srvs[i] = input->srv;
    }
    bitmap_desc.dpiX = context->drawing_state.unitMode == D2D1_UNIT_MODE_PIXELS ? 96 : context->desc.dpiX;
    bitmap_desc.dpiY = context->drawing_state.unitMode == D2D1_UNIT_MODE_PIXELS ? 96 : context->desc.dpiY;
    if (!size.width || !size.height)
    {
        if (SUCCEEDED(hr = d2d_bitmap_create(context, size, NULL, 0, &bitmap_desc, &bitmap)))
            output->bitmap = (ID2D1Bitmap *)&bitmap->ID2D1Bitmap1_iface;
        return hr;
    }
    texture_desc.Width = size.width;
    texture_desc.Height = size.height;
    texture_desc.MipLevels = texture_desc.ArraySize = texture_desc.SampleDesc.Count = 1;
    texture_desc.Format = bitmap_desc.pixelFormat.format;
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    if (FAILED(hr = ID3D11Device1_CreateTexture2D(device, &texture_desc, NULL, &texture))) goto done;
    if (FAILED(hr = ID3D11Device1_CreateRenderTargetView(device, (ID3D11Resource *)texture, NULL, &rtv))) goto done;
    buffer_desc.ByteWidth = data_size;
    buffer_desc.Usage = D3D11_USAGE_IMMUTABLE;
    buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    initial.pSysMem = data;
    if (FAILED(hr = ID3D11Device1_CreateBuffer(device, &buffer_desc, &initial, &constants))) goto done;

    viewport.Width = size.width;
    viewport.Height = size.height;
    viewport.MaxDepth = 1;
    if (context->cs) EnterCriticalSection(context->cs);
    ID3D11Device1_GetImmediateContext1(device, &immediate);
    ID3D11DeviceContext1_SwapDeviceContextState(immediate, context->d3d_state, &previous);
    ID3D11DeviceContext1_ClearState(immediate);
    ID3D11DeviceContext1_OMSetRenderTargets(immediate, 1, &rtv, NULL);
    ID3D11DeviceContext1_RSSetViewports(immediate, 1, &viewport);
    ID3D11DeviceContext1_IASetPrimitiveTopology(immediate, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext1_VSSetShader(immediate, vs, NULL, 0);
    ID3D11DeviceContext1_PSSetShader(immediate, ps, NULL, 0);
    ID3D11DeviceContext1_PSSetShaderResources(immediate, 0, count, srvs);
    ID3D11DeviceContext1_PSSetShaderResources(immediate, 2, 1, &extra);
    ID3D11DeviceContext1_PSSetConstantBuffers(immediate, 0, 1, &constants);
    ID3D11DeviceContext1_Draw(immediate, 3, 0);
    ID3D11DeviceContext1_ClearState(immediate);
    ID3D11DeviceContext1_SwapDeviceContextState(immediate, previous, NULL);
    ID3DDeviceContextState_Release(previous);
    ID3D11DeviceContext1_Release(immediate);
    if (context->cs) LeaveCriticalSection(context->cs);

    if (FAILED(hr = ID3D11Texture2D_QueryInterface(texture, &IID_IDXGISurface, (void **)&surface))) goto done;
    if (SUCCEEDED(hr = d2d_bitmap_create_shared(context, &IID_IDXGISurface, surface, &bitmap_desc, &bitmap)))
        output->bitmap = (ID2D1Bitmap *)&bitmap->ID2D1Bitmap1_iface;
done:
    if (surface) IDXGISurface_Release(surface);
    if (constants) ID3D11Buffer_Release(constants);
    if (rtv) ID3D11RenderTargetView_Release(rtv);
    if (texture) ID3D11Texture2D_Release(texture);
    return hr;
}
