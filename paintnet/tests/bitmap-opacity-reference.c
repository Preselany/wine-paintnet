/* GPU bitmap opacity brush sampling, independently of font rasterization.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <stdio.h>
#include <stdlib.h>
#include "windows.h"
#include "initguid.h"
#include "d2d1_1.h"
#include "d3d11.h"
#define REQUIRE(call) do {hr=(call);if(FAILED(hr)){printf("FAILED %u %08lx\n",__LINE__,hr);return 1;}}while(0)

int main(void)
{
    ID3D11Device *d3d;
    IDXGIDevice *dxgi;
    ID2D1Factory1 *factory;
    ID2D1Device *device;
    ID2D1DeviceContext *dc;
    ID2D1Bitmap1 *target, *readback, *source, *mask;
    ID2D1BitmapBrush *brush, *opacity;
    ID2D1RectangleGeometry *geometry;
    D2D1_BITMAP_PROPERTIES1 desc = {{DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},96,96,D2D1_BITMAP_OPTIONS_TARGET,NULL};
    D2D1_BITMAP_BRUSH_PROPERTIES brush_desc = {D2D1_EXTEND_MODE_CLAMP,D2D1_EXTEND_MODE_CLAMP,D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR};
    D2D1_RECT_F rect = {0,0,8,8};
    D2D1_COLOR_F clear = {0};
    D2D1_MAPPED_RECT mapped;
    DWORD colours[64], bgra[64];
    BYTE alpha[64];
    UINT x, y, kind;
    HRESULT hr;

    setvbuf(stdout,NULL,_IOLBF,0);
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    REQUIRE(D3D11CreateDevice(NULL,getenv("D2D1_TEST_WARP")?D3D_DRIVER_TYPE_WARP:D3D_DRIVER_TYPE_HARDWARE,
            NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,NULL,0,D3D11_SDK_VERSION,&d3d,NULL,NULL));
    REQUIRE(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    REQUIRE(ID2D1Factory1_CreateDevice(factory,dxgi,&device));
    REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&dc));
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,(D2D1_SIZE_U){8,8},NULL,0,&desc,&target));
    desc.bitmapOptions=D2D1_BITMAP_OPTIONS_CPU_READ|D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,(D2D1_SIZE_U){8,8},NULL,0,&desc,&readback));
    desc.bitmapOptions=0; desc.pixelFormat.format=DXGI_FORMAT_B8G8R8A8_UNORM;
    for(y=0;y<8;++y)for(x=0;x<8;++x)
    {
        colours[y*8+x]=0xff4080c0;
        alpha[y*8+x]=(x+y)%4*85;
        bgra[y*8+x]=(DWORD)alpha[y*8+x]<<24;
    }
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,(D2D1_SIZE_U){8,8},colours,32,&desc,&source));
    REQUIRE(ID2D1RenderTarget_CreateBitmapBrush((ID2D1RenderTarget *)dc,(ID2D1Bitmap *)source,&brush_desc,NULL,&brush));
    REQUIRE(ID2D1Factory1_CreateRectangleGeometry(factory,&rect,&geometry));
    ID2D1DeviceContext_SetTarget(dc,(ID2D1Image *)target);
    ID2D1DeviceContext_SetAntialiasMode(dc,D2D1_ANTIALIAS_MODE_ALIASED);
    for(kind=0;kind<2;++kind)
    {
        desc.pixelFormat.format=kind?DXGI_FORMAT_B8G8R8A8_UNORM:DXGI_FORMAT_A8_UNORM;
        REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,(D2D1_SIZE_U){8,8},kind?(void *)bgra:alpha,kind?32:8,&desc,&mask));
        REQUIRE(ID2D1RenderTarget_CreateBitmapBrush((ID2D1RenderTarget *)dc,(ID2D1Bitmap *)mask,&brush_desc,NULL,&opacity));
        ID2D1DeviceContext_BeginDraw(dc);
        ID2D1DeviceContext_Clear(dc,&clear);
        ID2D1DeviceContext_FillGeometry(dc,(ID2D1Geometry *)geometry,(ID2D1Brush *)brush,(ID2D1Brush *)opacity);
        hr=ID2D1DeviceContext_EndDraw(dc,NULL,NULL);printf("case %u end %08lx\n",kind,hr);
        REQUIRE(ID2D1Bitmap1_CopyFromBitmap(readback,NULL,(ID2D1Bitmap *)target,NULL));
        REQUIRE(ID2D1Bitmap1_Map(readback,D2D1_MAP_OPTIONS_READ,&mapped));
        for(y=0;y<8;++y)for(x=0;x<8;++x)
        {
            float *p=(float *)(mapped.bits+y*mapped.pitch)+x*4;
            printf("pixel %u %u %.9g %.9g %.9g %.9g\n",x,y,p[0],p[1],p[2],p[3]);
        }
        ID2D1Bitmap1_Unmap(readback);
        ID2D1BitmapBrush_Release(opacity);
        ID2D1Bitmap1_Release(mask);
    }
    ID2D1RectangleGeometry_Release(geometry);
    ID2D1BitmapBrush_Release(brush);
    ID2D1Bitmap1_Release(source);
    ID2D1Bitmap1_Release(readback);
    ID2D1Bitmap1_Release(target);
    ID2D1DeviceContext_Release(dc);
    ID2D1Device_Release(device);
    ID2D1Factory1_Release(factory);
    IDXGIDevice_Release(dxgi);
    ID3D11Device_Release(d3d);
    CoUninitialize();
    return 0;
}
