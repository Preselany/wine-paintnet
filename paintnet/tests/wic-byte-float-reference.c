/* WIC conversion of straight/premultiplied byte RGBA/BGRA to float images.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "windows.h"
#include "initguid.h"
#include "wincodec.h"
#define REQUIRE(call) do {hr=(call);if(FAILED(hr)){printf("FAILED %u %08lx\n",__LINE__,hr);return 1;}}while(0)
int main(void)
{
    static const GUID *sources[]={&GUID_WICPixelFormat32bppBGRA,&GUID_WICPixelFormat32bppPBGRA,&GUID_WICPixelFormat32bppRGBA,&GUID_WICPixelFormat32bppPRGBA};
    static const struct {const GUID *id;UINT bpp;} targets[]={
        {&GUID_WICPixelFormat32bppBGRA,4},{&GUID_WICPixelFormat32bppPBGRA,4},
        {&GUID_WICPixelFormat32bppRGBA,4},{&GUID_WICPixelFormat32bppPRGBA,4},
        {&GUID_WICPixelFormat128bppRGBAFloat,16},{&GUID_WICPixelFormat128bppPRGBAFloat,16}};
    static const BYTE values[]={0,1,10,128,192,255,77,231};
    static const BYTE alphas[]={0,1,2,25,26,51,128,191,254,255,0,1};
    IWICImagingFactory *f;IWICBitmap *bitmap;IWICFormatConverter *c;
    BYTE pixels[12][8][4];BYTE output[12*(8*16+8)];
    WICRect crops[]={{0,0,8,12},{1,2,5,7},{7,11,1,1}};
    UINT src,dst,x,y,phase,stride,size,i;BOOL can;GUID actual;HRESULT hr;
    CoInitializeEx(NULL,COINIT_MULTITHREADED);setvbuf(stdout,NULL,_IOFBF,65536);
    REQUIRE(CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void **)&f));
    for(y=0;y<12;++y)for(x=0;x<8;++x)
    {pixels[y][x][0]=values[x];pixels[y][x][1]=values[(x+2)%8];pixels[y][x][2]=values[(x+4)%8];pixels[y][x][3]=alphas[y];}
    for(src=0;src<ARRAY_SIZE(sources);++src)for(dst=0;dst<ARRAY_SIZE(targets);++dst)
    {
        printf("case %u %u\n",src,dst);
        REQUIRE(IWICImagingFactory_CreateBitmapFromMemory(f,8,12,sources[src],8*4,sizeof(pixels),(BYTE *)pixels,&bitmap));
        REQUIRE(IWICImagingFactory_CreateFormatConverter(f,&c));
        can=99;hr=IWICFormatConverter_CanConvert(c,sources[src],targets[dst].id,&can);printf("can %08lx %d\n",hr,can);
        hr=IWICFormatConverter_Initialize(c,(IWICBitmapSource *)bitmap,targets[dst].id,WICBitmapDitherTypeNone,NULL,0,WICBitmapPaletteTypeCustom);
        printf("initialize %08lx\n",hr);
        if(SUCCEEDED(hr))
        {
            hr=IWICFormatConverter_GetPixelFormat(c,&actual);printf("format %08lx %d\n",hr,IsEqualGUID(&actual,targets[dst].id));
            for(phase=0;phase<3;++phase)
            {
                stride=crops[phase].Width*targets[dst].bpp+8;size=(crops[phase].Height-1)*stride+crops[phase].Width*targets[dst].bpp;
                memset(output,0xcc,sizeof(output));hr=IWICFormatConverter_CopyPixels(c,&crops[phase],stride,size,output);
                printf("copy%u %08lx\n",phase,hr);printf("pixels%u",phase);for(i=0;i<size;++i)printf(" %02x",output[i]);puts("");
            }
        }
        IWICFormatConverter_Release(c);IWICBitmap_Release(bitmap);fflush(stdout);
    }
    IWICImagingFactory_Release(f);CoUninitialize();puts("done");return 0;
}
