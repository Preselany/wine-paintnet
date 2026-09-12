/* Native WIC render target formats, retained contents and drawing.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "initguid.h"
#include "d2d1_1.h"
#include "wincodec.h"
#define REQUIRE(call) do {hr=(call);if(FAILED(hr)){printf("setup %u %08lx\n",__LINE__,hr);return 1;}}while(0)
struct format {const GUID *id;unsigned int bpp;};
int main(void)
{
    static const struct format formats[]={
        {&GUID_WICPixelFormat8bppAlpha,1},{&GUID_WICPixelFormat8bppGray,1},
        {&GUID_WICPixelFormat32bppPBGRA,4},{&GUID_WICPixelFormat32bppBGRA,4},{&GUID_WICPixelFormat32bppBGR,4},
        {&GUID_WICPixelFormat32bppPRGBA,4},{&GUID_WICPixelFormat32bppRGBA,4},{&GUID_WICPixelFormat32bppRGB,4},
        {&GUID_WICPixelFormat64bppPRGBAHalf,8},{&GUID_WICPixelFormat64bppRGBAHalf,8},
        {&GUID_WICPixelFormat128bppPRGBAFloat,16},{&GUID_WICPixelFormat128bppRGBAFloat,16},
    };
    static const DXGI_FORMAT dxgis[]={DXGI_FORMAT_UNKNOWN,DXGI_FORMAT_A8_UNORM,DXGI_FORMAT_B8G8R8A8_UNORM,
        DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R16G16B16A16_FLOAT,DXGI_FORMAT_R32G32B32A32_FLOAT};
    ID2D1Factory *factory;IWICImagingFactory *wic;IWICBitmap *bitmap;ID2D1RenderTarget *target;ID2D1SolidColorBrush *brush;
    D2D1_RENDER_TARGET_PROPERTIES desc={0};D2D1_PIXEL_FORMAT actual;
    D2D1_COLOR_F clear={.2f,.4f,.6f,.25f},color={.75f,.5f,.25f,.5f};
    D2D1_RECT_F rectangle={1,1,4,3};BYTE pixels[8*4*16],out[8*4*16];
    unsigned int f,d,a,t,i,phase,stride;HRESULT hr;
    setvbuf(stdout,NULL,_IOFBF,65536);CoInitializeEx(NULL,COINIT_MULTITHREADED);
    REQUIRE(CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void **)&wic));
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED,&IID_ID2D1Factory,NULL,(void **)&factory));
    for(f=0;f<ARRAY_SIZE(formats);++f)for(d=0;d<1;++d)for(a=0;a<1;++a)for(t=0;t<1;++t)
    {
        printf("case %u %u %u %u\n",f,d,a,t);
        stride=8*formats[f].bpp;for(i=0;i<stride*4;++i)pixels[i]=(BYTE)(i*7+51);
        REQUIRE(IWICImagingFactory_CreateBitmapFromMemory(wic,8,4,formats[f].id,stride,stride*4,pixels,&bitmap));
        desc.type=t;desc.pixelFormat=(D2D1_PIXEL_FORMAT){dxgis[d],a};
        target=NULL;hr=ID2D1Factory_CreateWicBitmapRenderTarget(factory,bitmap,&desc,&target);printf("create %08lx\n",hr);
        if(SUCCEEDED(hr))
        {
            actual=ID2D1RenderTarget_GetPixelFormat(target);printf("format %u %u\n",actual.format,actual.alphaMode);
            REQUIRE(ID2D1RenderTarget_CreateSolidColorBrush(target,&color,NULL,&brush));
            ID2D1RenderTarget_SetAntialiasMode(target,D2D1_ANTIALIAS_MODE_ALIASED);
            for(phase=0;phase<6;++phase)
            {
                if(phase==4)
                {
                    IWICBitmapLock *lock;BYTE *data;UINT pitch,length;
                    REQUIRE(IWICBitmap_Lock(bitmap,NULL,WICBitmapLockWrite,&lock));
                    REQUIRE(IWICBitmapLock_GetDataPointer(lock,&length,&data));
                    REQUIRE(IWICBitmapLock_GetStride(lock,&pitch));
                    for(i=0;i<4;++i)memset(data+i*pitch,0x33,stride);
                    IWICBitmapLock_Release(lock);
                }
                ID2D1RenderTarget_BeginDraw(target);
                if(phase==1)ID2D1RenderTarget_Clear(target,&clear);
                if(phase==2 || phase==3 || phase==5)ID2D1RenderTarget_FillRectangle(target,&rectangle,(ID2D1Brush *)brush);
                hr=ID2D1RenderTarget_EndDraw(target,NULL,NULL);printf("draw%u %08lx\n",phase,hr);
                memset(out,0xcc,sizeof(out));REQUIRE(IWICBitmap_CopyPixels(bitmap,NULL,stride,stride*4,out));
                printf("pixels%u",phase);for(i=0;i<stride*4;++i)printf(" %02x",out[i]);puts("");
            }
            ID2D1SolidColorBrush_Release(brush);ID2D1RenderTarget_Release(target);
        }
        IWICBitmap_Release(bitmap);fflush(stdout);
    }
    ID2D1Factory_Release(factory);IWICImagingFactory_Release(wic);CoUninitialize();puts("done");return 0;
}
