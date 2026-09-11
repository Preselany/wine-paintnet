/* Fork-internal diagnostic: reads the actual Wine lookup-table texture back.
 * This deliberately uses private structure layout and MUST only run against
 * d2d1.dll built from the same checkout. It is not a Windows conformance test.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "initguid.h"
#include "d2d1_private.h"
#include <stdio.h>

static unsigned int failures, rows;

static void check_upload(ID2D1DeviceContext2 *context, ID3D11Device *device,
        ID3D11DeviceContext *immediate, D2D1_BUFFER_PRECISION precision,
        unsigned int bytes_per_pixel, BOOL padded)
{
    UINT32 extents[] = {2, 3, 4}, strides[2];
    unsigned int x, y, z, c, size;
    BYTE data[1024], expected[1024];
    ID2D1LookupTable3D *resource;
    struct d2d_lookup_table *table;
    D3D11_TEXTURE3D_DESC desc;
    D3D11_MAPPED_SUBRESOURCE mapped;
    ID3D11Texture3D *staging;
    HRESULT hr;

    strides[0] = extents[0] * bytes_per_pixel + (padded ? 16 : 0);
    strides[1] = extents[1] * strides[0] + (padded ? 32 : 0);
    size = (extents[2] - 1) * strides[1] + (extents[1] - 1) * strides[0] + extents[0] * bytes_per_pixel;
    memset(data, 0xcd, sizeof(data));
    for (z = 0; z < extents[2]; ++z)
        for (y = 0; y < extents[1]; ++y)
            for (x = 0; x < extents[0]; ++x)
                for (c = 0; c < bytes_per_pixel; ++c)
                    data[z * strides[1] + y * strides[0] + x * bytes_per_pixel + c] = 1 + z * 37 + y * 11 + x * 5 + c;
    memcpy(expected, data, sizeof(data));
    hr = ID2D1DeviceContext2_CreateLookupTable3D(context, precision, extents, data, size, strides, &resource);
    if (FAILED(hr))
    {
        printf("FAIL: create precision %u, padded %u: %#lx\n", precision, padded, hr);
        ++failures;
        return;
    }
    /* Mutate the caller buffer to verify creation took its own copy. */
    memset(data, 0, sizeof(data));
    table = CONTAINING_RECORD(resource, struct d2d_lookup_table, ID2D1LookupTable3D_iface);
    ID3D11Texture3D_GetDesc(table->texture, &desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    hr = ID3D11Device_CreateTexture3D(device, &desc, NULL, &staging);
    if (FAILED(hr))
    {
        printf("FAIL: staging texture: %#lx\n", hr);
        ++failures;
        goto release_table;
    }
    ID3D11DeviceContext_CopyResource(immediate, (ID3D11Resource *)staging, (ID3D11Resource *)table->texture);
    hr = ID3D11DeviceContext_Map(immediate, (ID3D11Resource *)staging, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr))
    {
        printf("FAIL: map: %#lx\n", hr);
        ++failures;
    }
    else
    {
        for (z = 0; z < extents[2]; ++z)
            for (y = 0; y < extents[1]; ++y)
            {
                ++rows;
                if (memcmp((BYTE *)mapped.pData + z * mapped.DepthPitch + y * mapped.RowPitch,
                        expected + z * strides[1] + y * strides[0], extents[0] * bytes_per_pixel))
                {
                    printf("FAIL: precision %u, padded %u, plane %u, row %u\n", precision, padded, z, y);
                    ++failures;
                }
            }
        ID3D11DeviceContext_Unmap(immediate, (ID3D11Resource *)staging, 0);
    }
    ID3D11Texture3D_Release(staging);
release_table:
    ID2D1LookupTable3D_Release(resource);
}

int main(void)
{
    static const unsigned int bytes_per_pixel[] = {4, 4, 8, 8, 16};
    ID3D11Device *d3d_device;
    ID3D11DeviceContext *immediate;
    IDXGIDevice *dxgi_device;
    ID2D1Factory1 *factory;
    ID2D1Device *device;
    ID2D1DeviceContext *context;
    ID2D1DeviceContext2 *context2;
    unsigned int i, padded;
    HRESULT hr;

    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            NULL, 0, D3D11_SDK_VERSION, &d3d_device, NULL, &immediate);
    if (FAILED(hr)) return 2;
    hr = ID3D11Device_QueryInterface(d3d_device, &IID_IDXGIDevice, (void **)&dxgi_device);
    if (FAILED(hr)) return 2;
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL, (void **)&factory);
    if (FAILED(hr)) return 2;
    hr = ID2D1Factory1_CreateDevice(factory, dxgi_device, &device);
    if (FAILED(hr)) return 2;
    hr = ID2D1Device_CreateDeviceContext(device, D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &context);
    if (FAILED(hr)) return 2;
    hr = ID2D1DeviceContext_QueryInterface(context, &IID_ID2D1DeviceContext2, (void **)&context2);
    if (FAILED(hr)) return 2;
    for (i = 0; i < ARRAY_SIZE(bytes_per_pixel); ++i)
        for (padded = 0; padded < 2; ++padded)
            check_upload(context2, d3d_device, immediate, D2D1_BUFFER_PRECISION_8BPC_UNORM + i,
                    bytes_per_pixel[i], padded);
    ID2D1DeviceContext2_Release(context2);
    ID2D1DeviceContext_Release(context);
    ID2D1Device_Release(device);
    ID2D1Factory1_Release(factory);
    IDXGIDevice_Release(dxgi_device);
    ID3D11DeviceContext_Release(immediate);
    ID3D11Device_Release(d3d_device);
    printf("Lookup upload: %u rows compared, %u failures.\n", rows, failures);
    return failures ? 1 : 0;
}
