/* WIC floating-point RGB quantization thresholds.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <stdio.h>
#include <stdlib.h>
#include "windows.h"
#include "initguid.h"
#include "wincodec.h"
#define N 262144
#define REQUIRE(call) do {hr=(call);if(FAILED(hr)){printf("FAILED %u %08lx\n",__LINE__,hr);return 1;}}while(0)
int main(void)
{
    IWICImagingFactory *f;IWICBitmap *bitmap;IWICFormatConverter *c;
    float *pixels=malloc((N+1)*16);BYTE *output=malloc((N+1)*4);
    unsigned int i,phase;HRESULT hr;
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    REQUIRE(CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void **)&f));
    for(phase=0;phase<2;++phase)
    {
        for(i=0;i<=N;++i){pixels[4*i]=pixels[4*i+1]=pixels[4*i+2]=(float)i/N/(phase?50:1);pixels[4*i+3]=1;}
        REQUIRE(IWICImagingFactory_CreateBitmapFromMemory(f,N+1,1,&GUID_WICPixelFormat128bppRGBAFloat,(N+1)*16,(N+1)*16,(BYTE *)pixels,&bitmap));
        REQUIRE(IWICImagingFactory_CreateFormatConverter(f,&c));
        REQUIRE(IWICFormatConverter_Initialize(c,(IWICBitmapSource *)bitmap,&GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,NULL,0,WICBitmapPaletteTypeCustom));
        REQUIRE(IWICFormatConverter_CopyPixels(c,NULL,(N+1)*4,(N+1)*4,output));
        for(i=0;i<=N;++i)if(!i || output[4*i]!=output[4*i-4])printf("threshold %u %u %.9g %u\n",phase,i,pixels[4*i],output[4*i]);
        IWICFormatConverter_Release(c);IWICBitmap_Release(bitmap);
    }
    IWICImagingFactory_Release(f);CoUninitialize();free(pixels);free(output);puts("done");return 0;
}
