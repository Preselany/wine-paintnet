/* Exhaustive 8-bit premultiplied input channel/alpha combinations.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "windows.h"
#include "initguid.h"
#include "wincodec.h"
#define REQUIRE(call) do {hr=(call);if(FAILED(hr)){printf("FAILED %u %08lx\n",__LINE__,hr);return 1;}}while(0)
int main(void)
{
    static const GUID *sources[]={&GUID_WICPixelFormat32bppPBGRA,&GUID_WICPixelFormat32bppPRGBA};
    IWICImagingFactory *f;IWICBitmap *bitmap;IWICFormatConverter *c;
    BYTE *input=malloc(256*256*4);float *output=malloc(256*256*16),v,error,best;
    unsigned int src,x,y,k,index;HRESULT hr;
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    REQUIRE(CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void **)&f));
    for(y=0;y<256;++y)for(x=0;x<256;++x)
    {input[(y*256+x)*4]=input[(y*256+x)*4+1]=input[(y*256+x)*4+2]=x;input[(y*256+x)*4+3]=y;}
    for(src=0;src<2;++src)
    {
        REQUIRE(IWICImagingFactory_CreateBitmapFromMemory(f,256,256,sources[src],1024,256*256*4,input,&bitmap));
        REQUIRE(IWICImagingFactory_CreateFormatConverter(f,&c));
        REQUIRE(IWICFormatConverter_Initialize(c,(IWICBitmapSource *)bitmap,&GUID_WICPixelFormat128bppRGBAFloat,WICBitmapDitherTypeNone,NULL,0,WICBitmapPaletteTypeCustom));
        REQUIRE(IWICFormatConverter_CopyPixels(c,NULL,4096,256*256*16,(BYTE *)output));
        for(y=0;y<256;++y)
        {
            printf("row %u %u",src,y);
            for(x=0;x<256;++x)
            {
                best=1e9f;index=999;
                for(k=0;k<256;++k)
                {
                    v=k/255.0f;v=v<=.04045f?v/12.92f:powf((v+.055f)/1.055f,2.4f);
                    error=fabsf(v-output[(y*256+x)*4]);if(error<best){best=error;index=k;}
                }
                if(best>1e-6f){printf(" INVALID %g",best);return 1;}
                printf(" %02x",index);
            }
            puts("");
        }
        IWICFormatConverter_Release(c);IWICBitmap_Release(bitmap);
    }
    IWICImagingFactory_Release(f);CoUninitialize();free(input);free(output);puts("done");return 0;
}
