/* Direct2D lookup table resources.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "d2d1_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d2d);

static inline struct d2d_lookup_table *impl_from_ID2D1LookupTable3D(ID2D1LookupTable3D *iface)
{
    return CONTAINING_RECORD(iface, struct d2d_lookup_table, ID2D1LookupTable3D_iface);
}

static HRESULT STDMETHODCALLTYPE d2d_lookup_table_QueryInterface(ID2D1LookupTable3D *iface, REFIID iid, void **out)
{
    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_ID2D1LookupTable3D)
            || IsEqualGUID(iid, &IID_ID2D1Resource)
            || IsEqualGUID(iid, &IID_IUnknown))
    {
        ID2D1LookupTable3D_AddRef(iface);
        *out = iface;
        return S_OK;
    }

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE d2d_lookup_table_AddRef(ID2D1LookupTable3D *iface)
{
    struct d2d_lookup_table *table = impl_from_ID2D1LookupTable3D(iface);
    return InterlockedIncrement(&table->refcount);
}

static ULONG STDMETHODCALLTYPE d2d_lookup_table_Release(ID2D1LookupTable3D *iface)
{
    struct d2d_lookup_table *table = impl_from_ID2D1LookupTable3D(iface);
    ULONG refcount = InterlockedDecrement(&table->refcount);

    if (!refcount)
    {
        ID3D11ShaderResourceView_Release(table->view);
        ID3D11Texture3D_Release(table->texture);
        ID2D1Factory_Release(table->factory);
        free(table);
    }

    return refcount;
}

static void STDMETHODCALLTYPE d2d_lookup_table_GetFactory(ID2D1LookupTable3D *iface, ID2D1Factory **factory)
{
    struct d2d_lookup_table *table = impl_from_ID2D1LookupTable3D(iface);
    *factory = table->factory;
    ID2D1Factory_AddRef(*factory);
}

static const ID2D1LookupTable3DVtbl d2d_lookup_table_vtbl =
{
    d2d_lookup_table_QueryInterface,
    d2d_lookup_table_AddRef,
    d2d_lookup_table_Release,
    d2d_lookup_table_GetFactory,
};

HRESULT d2d_lookup_table_create(struct d2d_device_context *context, D2D1_BUFFER_PRECISION precision,
        const UINT32 *extents, const BYTE *data, UINT32 data_count, const UINT32 *strides,
        ID2D1LookupTable3D **lookup_table)
{
    D3D11_SUBRESOURCE_DATA initial_data;
    D3D11_TEXTURE3D_DESC desc = {0};
    struct d2d_lookup_table *table;
    UINT bytes_per_pixel;
    UINT64 row_size, plane_size, required_size;
    HRESULT hr;

    if (!lookup_table)
        return E_INVALIDARG;
    *lookup_table = NULL;
    if (!extents || !data || !strides)
        return E_INVALIDARG;

    switch (precision)
    {
        case D2D1_BUFFER_PRECISION_8BPC_UNORM:
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            bytes_per_pixel = 4;
            break;
        case D2D1_BUFFER_PRECISION_8BPC_UNORM_SRGB:
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            bytes_per_pixel = 4;
            break;
        case D2D1_BUFFER_PRECISION_16BPC_UNORM:
            desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
            bytes_per_pixel = 8;
            break;
        case D2D1_BUFFER_PRECISION_16BPC_FLOAT:
            desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            bytes_per_pixel = 8;
            break;
        case D2D1_BUFFER_PRECISION_32BPC_FLOAT:
            desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            bytes_per_pixel = 16;
            break;
        default:
            return E_INVALIDARG;
    }

    if (!extents[0] || !extents[1] || !extents[2])
        return E_INVALIDARG;

    /* Validate the last addressed texel, allowing padding between rows and
     * planes. Use wide arithmetic before passing pitches to Direct3D. */
    row_size = (UINT64)extents[0] * bytes_per_pixel;
    if (strides[0] < row_size)
        return E_INVALIDARG;
    plane_size = (UINT64)(extents[1] - 1) * strides[0] + row_size;
    if (strides[1] < plane_size)
        return E_INVALIDARG;
    required_size = (UINT64)(extents[2] - 1) * strides[1] + plane_size;
    if (data_count < required_size)
        return E_INVALIDARG;

    if (!(table = calloc(1, sizeof(*table))))
        return E_OUTOFMEMORY;

    desc.Width = extents[0];
    desc.Height = extents[1];
    desc.Depth = extents[2];
    desc.MipLevels = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    initial_data.pSysMem = data;
    initial_data.SysMemPitch = strides[0];
    initial_data.SysMemSlicePitch = strides[1];

    if (FAILED(hr = ID3D11Device1_CreateTexture3D(context->d3d_device, &desc, &initial_data, &table->texture)))
    {
        WARN("Failed to create lookup table texture, hr %#lx.\n", hr);
        free(table);
        return hr;
    }
    if (FAILED(hr = ID3D11Device1_CreateShaderResourceView(context->d3d_device,
            (ID3D11Resource *)table->texture, NULL, &table->view)))
    {
        ID3D11Texture3D_Release(table->texture);
        free(table);
        return hr;
    }

    table->ID2D1LookupTable3D_iface.lpVtbl = &d2d_lookup_table_vtbl;
    table->refcount = 1;
    table->factory = context->factory;
    ID2D1Factory_AddRef(table->factory);
    *lookup_table = &table->ID2D1LookupTable3D_iface;
    return S_OK;
}
