/* Floating-point converter output bounds and source rectangles.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <stdio.h>
#include <string.h>
#include "windows.h"
#include "initguid.h"
#include "wincodec.h"
#define REQUIRE(call) do {hr=(call);if(FAILED(hr)){printf("FAILED %u %08lx\n",__LINE__,hr);return 1;}}while(0)
int main(void)
{
    static const struct {const GUID *id;UINT bpp;} formats[]={
        {&GUID_WICPixelFormat128bppRGBAFloat,16},{&GUID_WICPixelFormat128bppPRGBAFloat,16},
        {&GUID_WICPixelFormat32bppBGRA,4},{&GUID_WICPixelFormat32bppPBGRA,4}};
    static const WICRect rects[]={{0,0,8,12},{0,0,0,1},{0,0,1,0},{-1,0,1,1},{7,0,2,1},{0,-1,1,1},{0,11,1,2}};
    IWICImagingFactory *f;IWICBitmap *bitmap;IWICFormatConverter *c;
    BYTE input[1536],output[4096];UINT src,dst,test,stride,size,i,changed;HRESULT hr;
    CoInitializeEx(NULL,COINIT_MULTITHREADED);memset(input,0,sizeof(input));
    REQUIRE(CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void **)&f));
    for(src=0;src<4;++src)for(dst=0;dst<4;++dst)
    {
        REQUIRE(IWICImagingFactory_CreateBitmapFromMemory(f,8,12,formats[src].id,8*formats[src].bpp,sizeof(input),input,&bitmap));
        REQUIRE(IWICImagingFactory_CreateFormatConverter(f,&c));
        REQUIRE(IWICFormatConverter_Initialize(c,(IWICBitmapSource *)bitmap,formats[dst].id,WICBitmapDitherTypeNone,NULL,0,WICBitmapPaletteTypeCustom));
        for(test=0;test<10;++test)
        {
            const WICRect *rect=&rects[test<7?test:0];stride=8*formats[dst].bpp;size=12*stride;
            if(test==7)stride=0;
            if(test==8)--stride;
            if(test==9)--size;
            memset(output,0xcc,sizeof(output));hr=IWICFormatConverter_CopyPixels(c,rect,stride,size,output);
            for(i=0,changed=0;i<sizeof(output);++i)changed+=output[i]!=0xcc;
            printf("case %u %u %u %08lx %u\n",src,dst,test,hr,changed);
        }
        IWICFormatConverter_Release(c);IWICBitmap_Release(bitmap);
    }
    IWICImagingFactory_Release(f);CoUninitialize();puts("done");return 0;
}
