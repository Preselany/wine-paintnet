/* ICC profile classification, copying and filename measurements.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define main color_context_base_main
#include "color-context-reference.c"
#undef main

int main(void)
{
    ID3D11Device *d3d;
    IDXGIDevice *dxgi;
    ID2D1Factory1 *factory;
    ID2D1Device *device;
    ID2D1DeviceContext *dc;
    ID2D1ColorContext *context, *copy;
    IWICImagingFactory *wic;
    IWICColorContext *wic_context;
    BYTE *profile, *returned;
    WCHAR filename[MAX_PATH];
    UINT size, i, j;
    HRESULT hr;
    FILE *file;
    setvbuf(stdout,NULL,_IONBF,0);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    REQUIRE(D3D11CreateDevice(NULL,getenv("D2D1_TEST_WARP") ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE,
            NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,NULL,0,D3D11_SDK_VERSION,&d3d,NULL,NULL));
    REQUIRE(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    REQUIRE(ID2D1Factory1_CreateDevice(factory,dxgi,&device));
    REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&dc));
    REQUIRE(CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void **)&wic));
    for (i = 1; i <= 3; ++i)
    {
        if (i <= 2) REQUIRE(ID2D1DeviceContext_CreateColorContext(dc,i,NULL,0,&context));
        else
        {
            REQUIRE(IWICImagingFactory_CreateColorContext(wic,&wic_context));
            REQUIRE(IWICColorContext_InitializeFromExifColorSpace(wic_context,2));
            REQUIRE(ID2D1DeviceContext_CreateColorContextFromWicColorContext(dc,wic_context,&context));
            IWICColorContext_Release(wic_context);
        }
        size = ID2D1ColorContext_GetProfileSize(context);
        profile = malloc(size+16); returned = malloc(size+16);
        REQUIRE(ID2D1ColorContext_GetProfile(context,profile,size));
        printf("builtin %u %u\nheader ",i,size);
        for (j = 0; j < 128 && j < size; ++j) printf("%02x",profile[j]);
        puts("");
        printf("profile_data ");
        for (j = 0; j < size; ++j) printf("%02x",profile[j]);
        puts("");
        for (j = 0; j < 9; ++j)
        {
            REQUIRE(ID2D1ColorContext_GetProfile(context,profile,size));
            if (j == 1) memset(profile+52,0,4);
            if (j == 2) memcpy(profile+52,"sRGB",4);
            if (j == 3) memcpy(profile+52,"scRG",4);
            if (j == 4) memcpy(profile+52,"bscR",4);
            if (j == 5) profile[3] ^= 1;
            if (j == 6) memset(profile+size,0xcc,16);
            if (j == 7) memset(profile+84,0,16);
            if (j == 8) memset(profile+size-12,0,12);
            REQUIRE(IWICImagingFactory_CreateColorContext(wic,&wic_context));
            REQUIRE(IWICColorContext_InitializeFromMemory(wic_context,profile,size));
            copy = NULL;
            hr = ID2D1DeviceContext_CreateColorContextFromWicColorContext(dc,wic_context,&copy);
            printf("wic_memory %u %u %08lx %u\n",i,j,hr, copy ? ID2D1ColorContext_GetColorSpace(copy) : 99);
            if (copy) ID2D1ColorContext_Release(copy);
            IWICColorContext_Release(wic_context);
            copy = (void *)0xdeadbeef;
            printf("custom %u %u\n",i,j);
            hr = ID2D1DeviceContext_CreateColorContext(dc,0,profile,size+(j == 6 ? 16 : 0),&copy);
            printf("create %08lx %u\n",hr, copy == (void *)0xdeadbeef ? 2 : !!copy);
            if (SUCCEEDED(hr))
            {
                printf("space_size %u %u\n",ID2D1ColorContext_GetColorSpace(copy),ID2D1ColorContext_GetProfileSize(copy));
                REQUIRE(ID2D1ColorContext_GetProfile(copy,returned,size+(j == 6 ? 16 : 0)));
                printf("copy_equal %u\n",!memcmp(profile,returned,size+(j == 6 ? 16 : 0)));
                memset(profile,0xcc,size);
                REQUIRE(ID2D1ColorContext_GetProfile(copy,profile,size+(j == 6 ? 16 : 0)));
                printf("owned %u\n",!memcmp(profile,returned,size+(j == 6 ? 16 : 0)));
                ID2D1ColorContext_Release(copy);
            }
        }
        REQUIRE(ID2D1ColorContext_GetProfile(context,profile,size));
        file = fopen("valid.icc","wb"); if (!file) return 2;
        fwrite(profile,1,size,file); fclose(file);
        GetFullPathNameW(L"valid.icc",MAX_PATH,filename,NULL);
        copy = NULL;
        hr = ID2D1DeviceContext_CreateColorContextFromFilename(dc,filename,&copy);
        printf("file_absolute %u %08lx\n",i,hr);
        if (copy)
        {
            printf("space_size %u %u\n",ID2D1ColorContext_GetColorSpace(copy),ID2D1ColorContext_GetProfileSize(copy));
            REQUIRE(ID2D1ColorContext_GetProfile(copy,returned,size));
            printf("file_equal %u\n",!memcmp(profile,returned,size));
            ID2D1ColorContext_Release(copy);
        }
        free(returned); free(profile);
        ID2D1ColorContext_Release(context);
    }
    IWICImagingFactory_Release(wic);
    ID2D1DeviceContext_Release(dc); ID2D1Device_Release(device); ID2D1Factory1_Release(factory);
    IDXGIDevice_Release(dxgi); ID3D11Device_Release(d3d); CoUninitialize();
    puts("done");
    return 0;
}
