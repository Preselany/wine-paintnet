/* Public Direct2D WIC bitmap upload measurements.
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
#define REQUIRE(call) do {hr=(call);if(FAILED(hr)){printf("setup %u %08lx\n",__LINE__,hr);return 1;}}while(0)

int main(void)
{
    static const struct {const GUID *wic;UINT bytes;DXGI_FORMAT dxgi;} formats[]={
        {&GUID_WICPixelFormat64bppPRGBAHalf,8,DXGI_FORMAT_R16G16B16A16_FLOAT},
        {&GUID_WICPixelFormat64bppRGBAHalf,8,DXGI_FORMAT_R16G16B16A16_FLOAT},
        {&GUID_WICPixelFormat48bppRGBHalf,6,DXGI_FORMAT_R16G16B16A16_FLOAT},
        {&GUID_WICPixelFormat16bppGrayHalf,2,DXGI_FORMAT_R16_FLOAT},
        {&GUID_WICPixelFormat128bppPRGBAFloat,16,DXGI_FORMAT_R32G32B32A32_FLOAT},
        {&GUID_WICPixelFormat128bppRGBAFloat,16,DXGI_FORMAT_R32G32B32A32_FLOAT},
        {&GUID_WICPixelFormat64bppPRGBA,8,DXGI_FORMAT_R16G16B16A16_UNORM},
        {&GUID_WICPixelFormat64bppRGBA,8,DXGI_FORMAT_R16G16B16A16_UNORM},
        {&GUID_WICPixelFormat32bppPBGRA,4,DXGI_FORMAT_B8G8R8A8_UNORM},
        {&GUID_WICPixelFormat32bppPRGBA,4,DXGI_FORMAT_R8G8B8A8_UNORM},
        {&GUID_WICPixelFormat32bppBGR,4,DXGI_FORMAT_B8G8R8A8_UNORM},
        {&GUID_WICPixelFormat32bppRGB,4,DXGI_FORMAT_R8G8B8A8_UNORM},
    };
    static const WORD half[]={0x3000,0x3400,0x3800,0x3a00,0xbc00,0x4000,0x0400,0x3c00,
            0x0001,0x8000,0x3c00,0x3800};
    ID3D11Device *d3d;IDXGIDevice *dxgi;ID2D1Factory1 *factory;ID2D1Device *device;
    ID2D1DeviceContext *dc;IWICImagingFactory *wic;IWICBitmap *source;
    ID2D1Bitmap1 *bitmap,*readback,*target,*float_readback;
    D2D1_SIZE_U size={3,2};D2D1_PIXEL_FORMAT actual;
    D2D1_BITMAP_PROPERTIES1 desc={{DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},
            96,96,D2D1_BITMAP_OPTIONS_TARGET,NULL},read_desc;
    D2D1_MAPPED_RECT mapped;BYTE data[128];UINT f,mode,api,i,y,stride;FLOAT xdpi,ydpi;
    D2D1_COLOR_F clear={0,0,0,0};D2D1_RECT_F dest={0,0,3,2};
    HRESULT hr;D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_10_0;
    BOOL fl10=getenv("D2D1_TEST_FL10")!=NULL;
    setvbuf(stdout,NULL,_IOFBF,65536);SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    REQUIRE(D3D11CreateDevice(NULL,getenv("D2D1_TEST_WARP")?D3D_DRIVER_TYPE_WARP:D3D_DRIVER_TYPE_HARDWARE,
            NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,fl10?&level:NULL,fl10?1:0,D3D11_SDK_VERSION,&d3d,NULL,NULL));
    printf("feature_level %x\n",ID3D11Device_GetFeatureLevel(d3d));
    REQUIRE(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    REQUIRE(ID2D1Factory1_CreateDevice(factory,dxgi,&device));
    REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&dc));
    REQUIRE(CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void **)&wic));
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,NULL,0,&desc,&target));
    desc.bitmapOptions=D2D1_BITMAP_OPTIONS_CPU_READ|D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,NULL,0,&desc,&float_readback));
    ID2D1DeviceContext_SetTarget(dc,(ID2D1Image *)target);
    for(f=0;f<ARRAYSIZE(formats);++f)
    {
        stride=3*formats[f].bytes+4;memset(data,0xcd,sizeof(data));
        for(y=0;y<2;++y)for(i=0;i<3*formats[f].bytes;++i)
            data[y*stride+i]=((const BYTE *)half)[(i+y*6)%sizeof(half)];
        REQUIRE(IWICImagingFactory_CreateBitmapFromMemory(wic,3,2,formats[f].wic,stride,2*stride,data,&source));
        REQUIRE(IWICBitmap_SetResolution(source,144,192));
        for(api=0;api<2;++api)for(mode=0;mode<13;++mode)
        {
            memset(&desc,0,sizeof(desc));
            if(mode>=2&&mode<=5){desc.pixelFormat.format=formats[f].dxgi;desc.pixelFormat.alphaMode=mode-2;}
            if(mode==6){desc.pixelFormat.format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.pixelFormat.alphaMode=D2D1_ALPHA_MODE_PREMULTIPLIED;}
            if(mode==7){desc.pixelFormat.format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.pixelFormat.alphaMode=D2D1_ALPHA_MODE_PREMULTIPLIED;}
            if(mode==8){desc.pixelFormat.format=DXGI_FORMAT_R16G16B16A16_FLOAT;desc.pixelFormat.alphaMode=D2D1_ALPHA_MODE_PREMULTIPLIED;}
            if(mode==9){desc.pixelFormat.alphaMode=D2D1_ALPHA_MODE_IGNORE;desc.dpiX=120;desc.dpiY=160;}
            if(mode==10){desc.pixelFormat.format=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;desc.pixelFormat.alphaMode=D2D1_ALPHA_MODE_PREMULTIPLIED;}
            if(mode==11){desc.pixelFormat.format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;desc.pixelFormat.alphaMode=D2D1_ALPHA_MODE_PREMULTIPLIED;}
            if(mode==12){desc.pixelFormat.format=DXGI_FORMAT_B8G8R8X8_UNORM;desc.pixelFormat.alphaMode=D2D1_ALPHA_MODE_IGNORE;}
            bitmap=NULL;
            if(api) hr=ID2D1RenderTarget_CreateBitmapFromWicBitmap((ID2D1RenderTarget *)dc,(IWICBitmapSource *)source,
                    mode?(D2D1_BITMAP_PROPERTIES *)&desc:NULL,(ID2D1Bitmap **)&bitmap);
            else hr=ID2D1DeviceContext_CreateBitmapFromWicBitmap(dc,(IWICBitmapSource *)source,mode?&desc:NULL,&bitmap);
            printf("case %u %u %u %08lx\n",f,api,mode,hr);
            if(FAILED(hr))continue;
            actual=ID2D1Bitmap1_GetPixelFormat(bitmap);ID2D1Bitmap1_GetDpi(bitmap,&xdpi,&ydpi);
            printf("format %u %u dpi %.9g %.9g\n",actual.format,actual.alphaMode,xdpi,ydpi);
            read_desc=desc;read_desc.pixelFormat=actual;read_desc.dpiX=read_desc.dpiY=96;
            read_desc.bitmapOptions=D2D1_BITMAP_OPTIONS_CPU_READ|D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
            REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,NULL,0,&read_desc,&readback));
            REQUIRE(ID2D1Bitmap1_CopyFromBitmap(readback,NULL,(ID2D1Bitmap *)bitmap,NULL));
            REQUIRE(ID2D1Bitmap1_Map(readback,D2D1_MAP_OPTIONS_READ,&mapped));
            printf("raw");for(y=0;y<2;++y)for(i=0;i<3*formats[f].bytes;++i)printf(" %02x",mapped.bits[y*mapped.pitch+i]);puts("");
            REQUIRE(ID2D1Bitmap1_Unmap(readback));ID2D1Bitmap1_Release(readback);
            ID2D1DeviceContext_BeginDraw(dc);ID2D1DeviceContext_Clear(dc,&clear);
            ID2D1DeviceContext_DrawBitmap(dc,(ID2D1Bitmap *)bitmap,&dest,1,D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,NULL,NULL);
            hr=ID2D1DeviceContext_EndDraw(dc,NULL,NULL);printf("draw %08lx\n",hr);
            if(SUCCEEDED(hr))
            {
                REQUIRE(ID2D1Bitmap1_CopyFromBitmap(float_readback,NULL,(ID2D1Bitmap *)target,NULL));
                REQUIRE(ID2D1Bitmap1_Map(float_readback,D2D1_MAP_OPTIONS_READ,&mapped));
                printf("pixels");for(y=0;y<2;++y)for(i=0;i<12;++i){float v;memcpy(&v,mapped.bits+y*mapped.pitch+i*4,4);printf(" %.9g",v);}puts("");
                REQUIRE(ID2D1Bitmap1_Unmap(float_readback));
            }
            ID2D1Bitmap1_Release(bitmap);fflush(stdout);
        }
        IWICBitmap_Release(source);
    }
    ID2D1DeviceContext_SetTarget(dc,NULL);ID2D1Bitmap1_Release(target);ID2D1Bitmap1_Release(float_readback);
    IWICImagingFactory_Release(wic);ID2D1DeviceContext_Release(dc);ID2D1Device_Release(device);
    ID2D1Factory1_Release(factory);IDXGIDevice_Release(dxgi);ID3D11Device_Release(d3d);CoUninitialize();
    puts("done");return 0;
}
