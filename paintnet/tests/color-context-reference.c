/* Public-API color-context measurements.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "initguid.h"
#include "d2d1_3.h"
#include "d3d11.h"
#include "wincodec.h"

#define REQUIRE(call) do { hr = (call); if (FAILED(hr)) { printf("setup %u %08lx\n", __LINE__, hr); return 1; } } while (0)

static void inspect(ID2D1ColorContext *context, ID2D1Factory1 *expected)
{
    ID2D1ColorContext1 *context1 = NULL;
    ID2D1Factory *factory;
    IUnknown *unknown;
    D2D1_SIMPLE_COLOR_PROFILE simple;
    BYTE *buffer;
    UINT size, i, capacities[4];
    HRESULT hr;
    size = ID2D1ColorContext_GetProfileSize(context);
    printf("space_size %u %u\n", ID2D1ColorContext_GetColorSpace(context), size);
    ID2D1ColorContext_GetFactory(context, &factory);
    printf("factory %u\n", factory == (ID2D1Factory *)expected);
    ID2D1Factory_Release(factory);
    hr = ID2D1ColorContext_QueryInterface(context, &IID_IUnknown, (void **)&unknown);
    printf("identity %08lx %u\n", hr, unknown == (IUnknown *)context);
    if (SUCCEEDED(hr)) IUnknown_Release(unknown);
    hr = ID2D1ColorContext_QueryInterface(context, &IID_ID2D1ColorContext1, (void **)&context1);
    printf("qi1 %08lx %u\n", hr, context1 == (ID2D1ColorContext1 *)context);
    if (context1)
    {
        printf("type_dxgi %u %u\n", ID2D1ColorContext1_GetColorContextType(context1),
                ID2D1ColorContext1_GetDXGIColorSpace(context1));
        memset(&simple, 0xcc, sizeof(simple));
        hr = ID2D1ColorContext1_GetSimpleColorProfile(context1, &simple);
        printf("simple %08lx", hr);
        for (i = 0; i < sizeof(simple); ++i) printf("%s%02x", i ? "" : " ", ((BYTE *)&simple)[i]);
        puts("");
        ID2D1ColorContext1_Release(context1);
    }
    if (size > 100000) { puts("oversized"); return; }
    buffer = malloc(size + 16);
    capacities[0] = 0; capacities[1] = size ? size - 1 : 1; capacities[2] = size; capacities[3] = size + 8;
    for (i = 0; i < 4; ++i)
    {
        UINT j, changed = 0, nonzero = 0;
        unsigned int hash = 2166136261u;
        memset(buffer, 0xcc, size + 16);
        hr = ID2D1ColorContext_GetProfile(context, buffer, capacities[i]);
        for (j = 0; j < size + 16; ++j)
        {
            if (buffer[j] != 0xcc) ++changed;
            if (buffer[j]) ++nonzero;
            hash = (hash ^ buffer[j]) * 16777619u;
        }
        printf("get %u %08lx %u %u %08x\n", capacities[i], hr, changed, nonzero, hash);
    }
    free(buffer);
}

int main(void)
{
    ID3D11Device *d3d;
    IDXGIDevice *dxgi;
    ID2D1Factory1 *factory;
    ID2D1Device *device;
    ID2D1DeviceContext *dc;
    ID2D1DeviceContext5 *dc5;
    ID2D1ColorContext *context, *returned;
    ID2D1ColorContext1 *context1;
    IWICImagingFactory *wic;
    IWICColorContext *wic_context;
    ID2D1Bitmap1 *bitmap;
    D2D1_BITMAP_PROPERTIES1 desc = {{DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED},96,96,0,NULL};
    D2D1_SIZE_U bitmap_size = {2,2};
    D2D1_SIMPLE_COLOR_PROFILE simple = {{0.64f,0.33f},{0.3f,0.6f},{0.15f,0.06f},{0.95047f,1.08883f},0};
    BYTE profile[160];
    const unsigned int lengths[] = {0,1,4,48,127,128,132,160};
    const unsigned int exif[] = {0,1,2,0xffff,0xffffffff};
    unsigned int i, j;
    HRESULT hr;
    FILE *file;
    setvbuf(stdout,NULL,_IONBF,0);
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    REQUIRE(D3D11CreateDevice(NULL,getenv("D2D1_TEST_WARP") ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE,
            NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,NULL,0,D3D11_SDK_VERSION,&d3d,NULL,NULL));
    REQUIRE(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    REQUIRE(ID2D1Factory1_CreateDevice(factory,dxgi,&device));
    REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&dc));
    REQUIRE(ID2D1DeviceContext_QueryInterface(dc,&IID_ID2D1DeviceContext5,(void **)&dc5));
    REQUIRE(CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void **)&wic));
    memset(profile,0,sizeof(profile));
    for (i = 0; i < 5; ++i)
    {
        for (j = 0; j < 2; ++j)
        {
            if (!i && !j) continue; /* Native dereferences a NULL custom profile. */
            context = (void *)0xdeadbeef;
            printf("base %u %u\n",i,j);
            hr = ID2D1DeviceContext_CreateColorContext(dc,i,j ? profile : NULL,j ? sizeof(profile) : 0,&context);
            printf("create %08lx %u\n",hr, context == (void *)0xdeadbeef ? 2 : !!context);
            if (SUCCEEDED(hr)) { inspect(context,factory); ID2D1ColorContext_Release(context); }
        }
    }
    for (i = 0; i < sizeof(lengths)/sizeof(*lengths); ++i)
    {
        context = (void *)0xdeadbeef;
        printf("length %u\n",lengths[i]);
        hr = ID2D1DeviceContext_CreateColorContext(dc,D2D1_COLOR_SPACE_CUSTOM,profile,lengths[i],&context);
        printf("create %08lx %u\n",hr, context == (void *)0xdeadbeef ? 2 : !!context);
        if (SUCCEEDED(hr)) { inspect(context,factory); ID2D1ColorContext_Release(context); }
    }
    for (i = 0; i < 4; ++i)
    {
        memset(profile,0,sizeof(profile));
        profile[3] = sizeof(profile);
        memcpy(profile+36,"acsp",4);
        memcpy(profile+52,i == 0 ? "sRGB" : i == 1 ? "scRG" : i == 2 ? "bscR" : "test",4);
        context = NULL;
        printf("model %u\n",i);
        hr = ID2D1DeviceContext_CreateColorContext(dc,D2D1_COLOR_SPACE_CUSTOM,profile,sizeof(profile),&context);
        printf("create %08lx %u\n",hr,!!context);
        if (context) { inspect(context,factory); ID2D1ColorContext_Release(context); }
    }
    for (i = 0; i < 27; ++i)
    {
        context1 = (void *)0xdeadbeef;
        printf("dxgi %u\n",i == 26 ? 0xffffffff : i);
        hr = ID2D1DeviceContext5_CreateColorContextFromDxgiColorSpace(dc5,i == 26 ? 0xffffffff : i,&context1);
        printf("create %08lx %u\n",hr, context1 == (void *)0xdeadbeef ? 2 : !!context1);
        if (SUCCEEDED(hr)) { inspect((ID2D1ColorContext *)context1,factory); ID2D1ColorContext1_Release(context1); }
    }
    for (i = 0; i < 5; ++i)
    {
        context1 = (void *)0xdeadbeef;
        simple.gamma = i == 4 ? 0xffffffff : i;
        printf("simple_create %u\n",simple.gamma);
        hr = ID2D1DeviceContext5_CreateColorContextFromSimpleColorProfile(dc5,&simple,&context1);
        printf("create %08lx %u\n",hr, context1 == (void *)0xdeadbeef ? 2 : !!context1);
        if (SUCCEEDED(hr)) { inspect((ID2D1ColorContext *)context1,factory); ID2D1ColorContext1_Release(context1); }
    }
    for (i = 0; i < 7; ++i)
    {
        REQUIRE(IWICImagingFactory_CreateColorContext(wic,&wic_context));
        printf("wic %u\n",i);
        if (i < 5) REQUIRE(IWICColorContext_InitializeFromExifColorSpace(wic_context,exif[i]));
        if (i == 6) REQUIRE(IWICColorContext_InitializeFromMemory(wic_context,profile,sizeof(profile)));
        context = (void *)0xdeadbeef;
        hr = ID2D1DeviceContext_CreateColorContextFromWicColorContext(dc,wic_context,&context);
        printf("create %08lx %u\n",hr, context == (void *)0xdeadbeef ? 2 : !!context);
        if (SUCCEEDED(hr)) { inspect(context,factory); ID2D1ColorContext_Release(context); }
        IWICColorContext_Release(wic_context);
    }
    file = fopen("profile.icc","wb");
    if (!file) return 2;
    fwrite(profile,1,sizeof(profile),file); fclose(file);
    for (i = 0; i < 2; ++i)
    {
        printf("file %u\n",i);
        context = (void *)0xdeadbeef;
        hr = ID2D1DeviceContext_CreateColorContextFromFilename(dc,i ? L"missing.icc" : L"profile.icc",&context);
        printf("create %08lx %u\n",hr, context == (void *)0xdeadbeef ? 2 : !!context);
        if (SUCCEEDED(hr)) { inspect(context,factory); ID2D1ColorContext_Release(context); }
    }
    REQUIRE(ID2D1DeviceContext_CreateColorContext(dc,D2D1_COLOR_SPACE_SRGB,NULL,0,&context));
    for (i = 0; i < 2; ++i)
    {
        desc.colorContext = i ? context : NULL;
        printf("bitmap %u\n",i);
        hr = ID2D1DeviceContext_CreateBitmap(dc,bitmap_size,NULL,0,&desc,&bitmap);
        printf("create_bitmap %08lx\n",hr);
        if (SUCCEEDED(hr))
        {
            returned = (void *)0xdeadbeef;
            ID2D1Bitmap1_GetColorContext(bitmap,&returned);
            printf("bitmap_context %u\n",returned == context ? 1 : returned ? 2 : 0);
            if (returned && returned != (void *)0xdeadbeef) ID2D1ColorContext_Release(returned);
            ID2D1Bitmap1_Release(bitmap);
        }
    }
    ID2D1ColorContext_Release(context);
    IWICImagingFactory_Release(wic);
    ID2D1DeviceContext5_Release(dc5);
    ID2D1DeviceContext_Release(dc);
    ID2D1Device_Release(device);
    ID2D1Factory1_Release(factory);
    IDXGIDevice_Release(dxgi);
    ID3D11Device_Release(d3d);
    CoUninitialize();
    puts("done");
    return 0;
}
