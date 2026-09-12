/* Measure Direct2D format capability queries on an independent Windows host.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <stdio.h>
#include "initguid.h"
#include "d2d1_3.h"
#include "d3d11.h"

int main(void)
{
    static const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_9_1, D3D_FEATURE_LEVEL_9_3,
            D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_12_1};
    ID2D1Factory1 *factory;
    ID3D11Device *d3d;
    IDXGIDevice *dxgi;
    ID2D1Device *device;
    ID2D1DeviceContext *context;
    ID2D1Bitmap1 *bitmap;
    D2D1_BITMAP_PROPERTIES1 props = {{DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED}, 96, 96, 0, NULL};
    D2D1_SIZE_U size = {4, 4};
    HRESULT hr, bitmap_hr;
    UINT l, f, support;
    BOOL supported;

    setvbuf(stdout, NULL, _IOLBF, 0);
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL, (void **)&factory);
    if (FAILED(hr)) return 1;
    for (l = 0; l < sizeof(levels) / sizeof(levels[0]); ++l)
    {
        hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                &levels[l], 1, D3D11_SDK_VERSION, &d3d, NULL, NULL);
        printf("device %04x %08lx\n", levels[l], hr);
        if (FAILED(hr)) continue;
        hr = ID3D11Device_QueryInterface(d3d, &IID_IDXGIDevice, (void **)&dxgi);
        if (FAILED(hr)) return 2;
        hr = ID2D1Factory1_CreateDevice(factory, dxgi, &device);
        if (FAILED(hr)) return 3;
        hr = ID2D1Device_CreateDeviceContext(device, 0, &context);
        if (FAILED(hr)) return 4;
        for (f = 0; f < 192; ++f)
        {
            supported = ID2D1DeviceContext_IsDxgiFormatSupported(context, f);
            support = 0;
            hr = ID3D11Device_CheckFormatSupport(d3d, f, &support);
            props.pixelFormat.format = f;
            props.pixelFormat.alphaMode = f == DXGI_FORMAT_B8G8R8X8_UNORM
                    || f == DXGI_FORMAT_B8G8R8X8_UNORM_SRGB ? D2D1_ALPHA_MODE_IGNORE : D2D1_ALPHA_MODE_PREMULTIPLIED;
            bitmap_hr = ID2D1DeviceContext_CreateBitmap(context, size, NULL, 0, &props, &bitmap);
            if (SUCCEEDED(bitmap_hr)) ID2D1Bitmap1_Release(bitmap);
            printf("format %04x %u %u d3d %08lx %08x bitmap %08lx\n", levels[l], f, supported, hr, support, bitmap_hr);
        }
        ID2D1DeviceContext_Release(context);
        ID2D1Device_Release(device);
        IDXGIDevice_Release(dxgi);
        ID3D11Device_Release(d3d);
    }
    ID2D1Factory1_Release(factory);
    return 0;
}
