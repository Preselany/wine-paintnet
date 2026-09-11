/* Rendering for custom Direct2D draw transforms.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "d2d1_private.h"
#include "d3dcompiler.h"

WINE_DEFAULT_DEBUG_CHANNEL(d2d);

static HRESULT create_buffer(ID3D11Device1 *device, const void *data, UINT size, ID3D11Buffer **buffer)
{
    D3D11_BUFFER_DESC desc = {0};
    D3D11_SUBRESOURCE_DATA initial = {0};
    BYTE *padded;
    HRESULT hr;

    *buffer = NULL;
    if (!size) return S_OK;
    if (size > D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16) return E_INVALIDARG;
    desc.ByteWidth = (size + 15) & ~15;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (!(padded = calloc(1, desc.ByteWidth))) return E_OUTOFMEMORY;
    memcpy(padded, data, size);
    initial.pSysMem = padded;
    hr = ID3D11Device1_CreateBuffer(device, &desc, &initial, buffer);
    free(padded);
    return hr;
}

static HRESULT create_vertex_shader(ID3D11Device1 *device, ID3D11VertexShader **shader)
{
    static const char source[] =
        "cbuffer C : register(b0) {float4 region;}"
        "struct O {float4 pos:SV_POSITION; float4 scene:SCENE_POSITION;};"
        "O main(uint id:SV_VertexID) {O o; float2 uv=float2((id<<1)&2,id&2);"
        "o.pos=float4(uv*float2(2,-2)+float2(-1,1),0,1);"
        "o.scene=float4(region.xy+uv*region.zw,0,1); return o;}";
    ID3DBlob *code = NULL, *errors = NULL;
    HRESULT hr;

    hr = D3DCompile(source, strlen(source), NULL, NULL, NULL, "main", "vs_4_0", 0, 0, &code, &errors);
    if (errors)
    {
        WARN("%s\n", (char *)ID3D10Blob_GetBufferPointer(errors));
        ID3D10Blob_Release(errors);
    }
    if (FAILED(hr)) return hr;
    hr = ID3D11Device1_CreateVertexShader(device, ID3D10Blob_GetBufferPointer(code),
            ID3D10Blob_GetBufferSize(code), NULL, shader);
    ID3D10Blob_Release(code);
    return hr;
}

HRESULT d2d_custom_effect_render(struct d2d_device_context *context, struct d2d_render_info *info,
        BOOL linkable_output, struct d2d_effect_image *output)
{
    ID3D11Device1 *device = context->d3d_device;
    ID3D11DeviceContext1 *immediate;
    ID3DDeviceContextState *previous;
    ID3D11Texture2D *texture = NULL;
    ID3D11RenderTargetView *rtv = NULL;
    ID3D11BlendState *blend = NULL;
    ID3D11Buffer *vs_constants = NULL, *ps_constants = NULL;
    IDXGISurface *surface = NULL;
    D3D11_TEXTURE2D_DESC texture_desc = {0};
    D3D11_BLEND_DESC blend_desc = {0};
    D3D11_VIEWPORT viewport = {0};
    D2D1_BITMAP_PROPERTIES1 bitmap_desc = {{DXGI_FORMAT_R32G32B32A32_FLOAT, D2D1_ALPHA_MODE_PREMULTIPLIED},
            96,96,D2D1_BITMAP_OPTIONS_NONE,NULL};
    struct d2d_bitmap *bitmap;
    D2D1_SIZE_U size;
    D2D1_BUFFER_PRECISION precision = info->precision;
    float region[4], clear[4] = {0,0,0,1};
    BOOL single_channel = info->depth == D2D1_CHANNEL_DEPTH_1 && (!linkable_output || info->cached);
    HRESULT hr;

    output->bitmap = NULL;
    if (!info->ps) return D2DERR_INVALID_GRAPH_CONFIGURATION;
    if ((INT64)output->rect.right - output->rect.left > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION
            || (INT64)output->rect.bottom - output->rect.top > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
        return D2DERR_EXCEEDS_MAX_BITMAP_SIZE;
    size.width = output->rect.right - output->rect.left;
    size.height = output->rect.bottom - output->rect.top;
    /* A final draw inherits target precision. The declared precision is a minimum
     * for intermediate buffers, not a quantization operation on the shader result. */
    if (context->target.type == D2D_TARGET_BITMAP)
    {
        switch (context->target.bitmap->format.format)
        {
            case DXGI_FORMAT_R32G32B32A32_FLOAT: precision = D2D1_BUFFER_PRECISION_32BPC_FLOAT; break;
            case DXGI_FORMAT_R16G16B16A16_FLOAT:
                if (precision == D2D1_BUFFER_PRECISION_16BPC_UNORM) precision = D2D1_BUFFER_PRECISION_32BPC_FLOAT;
                else precision = max(precision, D2D1_BUFFER_PRECISION_16BPC_FLOAT);
                break;
            case DXGI_FORMAT_R16G16B16A16_UNORM:
                if (precision == D2D1_BUFFER_PRECISION_16BPC_FLOAT) precision = D2D1_BUFFER_PRECISION_32BPC_FLOAT;
                else precision = max(precision, D2D1_BUFFER_PRECISION_16BPC_UNORM);
                break;
            default: break;
        }
    }
    switch (precision)
    {
        case D2D1_BUFFER_PRECISION_UNKNOWN:
        case D2D1_BUFFER_PRECISION_8BPC_UNORM: bitmap_desc.pixelFormat.format = DXGI_FORMAT_R8G8B8A8_UNORM; break;
        case D2D1_BUFFER_PRECISION_8BPC_UNORM_SRGB: bitmap_desc.pixelFormat.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; break;
        case D2D1_BUFFER_PRECISION_16BPC_UNORM: bitmap_desc.pixelFormat.format = DXGI_FORMAT_R16G16B16A16_UNORM; break;
        case D2D1_BUFFER_PRECISION_16BPC_FLOAT: bitmap_desc.pixelFormat.format = DXGI_FORMAT_R16G16B16A16_FLOAT; break;
        case D2D1_BUFFER_PRECISION_32BPC_FLOAT: break;
        default: return E_INVALIDARG;
    }
    bitmap_desc.dpiX = context->drawing_state.unitMode == D2D1_UNIT_MODE_PIXELS ? 96 : context->desc.dpiX;
    bitmap_desc.dpiY = context->drawing_state.unitMode == D2D1_UNIT_MODE_PIXELS ? 96 : context->desc.dpiY;
    if (!size.width || !size.height)
    {
        if (SUCCEEDED(hr = d2d_bitmap_create(context, size, NULL, 0, &bitmap_desc, &bitmap)))
            output->bitmap = (ID2D1Bitmap *)&bitmap->ID2D1Bitmap1_iface;
        return hr;
    }
    if (!info->vs && FAILED(hr = create_vertex_shader(device, &info->vs))) return hr;
    texture_desc.Width = size.width;
    texture_desc.Height = size.height;
    texture_desc.MipLevels = texture_desc.ArraySize = texture_desc.SampleDesc.Count = 1;
    texture_desc.Format = bitmap_desc.pixelFormat.format;
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    if (FAILED(hr = ID3D11Device1_CreateTexture2D(device, &texture_desc, NULL, &texture))) goto done;
    if (FAILED(hr = ID3D11Device1_CreateRenderTargetView(device, (ID3D11Resource *)texture, NULL, &rtv))) goto done;
    region[0] = output->rect.left; region[1] = output->rect.top;
    region[2] = size.width; region[3] = size.height;
    if (FAILED(hr = create_buffer(device, region, sizeof(region), &vs_constants))) goto done;
    if (FAILED(hr = create_buffer(device, info->constants, info->constants_size, &ps_constants))) goto done;
    blend_desc.RenderTarget[0].RenderTargetWriteMask = single_channel ? D3D11_COLOR_WRITE_ENABLE_RED : D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(hr = ID3D11Device1_CreateBlendState(device, &blend_desc, &blend))) goto done;
    viewport.Width = size.width; viewport.Height = size.height; viewport.MaxDepth = 1;

    if (context->cs) EnterCriticalSection(context->cs);
    ID3D11Device1_GetImmediateContext1(device, &immediate);
    ID3D11DeviceContext1_SwapDeviceContextState(immediate, context->d3d_state, &previous);
    ID3D11DeviceContext1_ClearState(immediate);
    ID3D11DeviceContext1_ClearRenderTargetView(immediate, rtv, clear);
    ID3D11DeviceContext1_OMSetRenderTargets(immediate, 1, &rtv, NULL);
    ID3D11DeviceContext1_OMSetBlendState(immediate, blend, NULL, ~0u);
    ID3D11DeviceContext1_RSSetViewports(immediate, 1, &viewport);
    ID3D11DeviceContext1_IASetPrimitiveTopology(immediate, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext1_VSSetShader(immediate, info->vs, NULL, 0);
    ID3D11DeviceContext1_VSSetConstantBuffers(immediate, 0, 1, &vs_constants);
    ID3D11DeviceContext1_PSSetShader(immediate, info->ps, NULL, 0);
    ID3D11DeviceContext1_PSSetConstantBuffers(immediate, 0, 1, &ps_constants);
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
    if (blend) ID3D11BlendState_Release(blend);
    if (vs_constants) ID3D11Buffer_Release(vs_constants);
    if (ps_constants) ID3D11Buffer_Release(ps_constants);
    if (rtv) ID3D11RenderTargetView_Release(rtv);
    if (texture) ID3D11Texture2D_Release(texture);
    return hr;
}
