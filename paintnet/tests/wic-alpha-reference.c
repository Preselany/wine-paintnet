/* Public WIC alpha-only pixel format metadata and bitmap measurements.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#include <stdio.h>
#include <initguid.h>
#define REQUIRE(x) do {hr=(x);if(FAILED(hr)){printf("failed %s %08lx\n",#x,hr);return 1;}}while(0)
int main(void)
{
    static const GUID *formats[]={&GUID_WICPixelFormat8bppAlpha};
    IWICImagingFactory *factory;IWICComponentInfo *component;IWICPixelFormatInfo2 *info;IWICBitmap *bitmap;
    UINT f,i,size,actual,bits,count;BYTE mask[16],pixels[16];BOOL transparent;WICPixelFormatNumericRepresentation numeric;
    static const UINT sizes[]={0,1,8,16};HRESULT hr;WCHAR name[128];
    setvbuf(stdout,NULL,_IOFBF,65536);CoInitializeEx(NULL,COINIT_MULTITHREADED);
    REQUIRE(CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void **)&factory));
    for(f=0;f<ARRAYSIZE(formats);++f)
    {
        printf("format %u\n",f);component=NULL;
        hr=IWICImagingFactory_CreateComponentInfo(factory,formats[f],&component);printf("component %08lx\n",hr);
        if(FAILED(hr))continue;
        REQUIRE(IWICComponentInfo_QueryInterface(component,&IID_IWICPixelFormatInfo2,(void **)&info));
        REQUIRE(IWICPixelFormatInfo2_GetBitsPerPixel(info,&bits));REQUIRE(IWICPixelFormatInfo2_GetChannelCount(info,&count));
        REQUIRE(IWICPixelFormatInfo2_SupportsTransparency(info,&transparent));REQUIRE(IWICPixelFormatInfo2_GetNumericRepresentation(info,&numeric));
        REQUIRE(IWICComponentInfo_GetFriendlyName(component,ARRAYSIZE(name),name,&actual));
        printf("metadata %u %u %u %u %ls\n",bits,count,transparent,numeric,name);
        for(i=0;i<count+1;++i)for(size=0;size<ARRAYSIZE(sizes);++size)
        {
            memset(mask,0xcc,sizeof(mask));actual=0xdeadbeef;
            hr=IWICPixelFormatInfo2_GetChannelMask(info,i,sizes[size],sizes[size]?mask:NULL,&actual);
            printf("mask %u %u %08lx %u",i,sizes[size],hr,actual);
            for(UINT b=0;b<sizeof(mask);++b)printf(" %02x",mask[b]);
            puts("");
        }
        memset(pixels,0x5a,sizeof(pixels));bitmap=NULL;
        hr=IWICImagingFactory_CreateBitmapFromMemory(factory,1,1,formats[f],bits/8,bits/8,pixels,&bitmap);
        printf("bitmap %08lx\n",hr);
        if(SUCCEEDED(hr))
        {
            memset(mask,0xcc,sizeof(mask));hr=IWICBitmap_CopyPixels(bitmap,NULL,bits/8,bits/8,mask);
            printf("copy %08lx",hr);for(i=0;i<bits/8;++i)printf(" %02x",mask[i]);puts("");IWICBitmap_Release(bitmap);
        }
        IWICPixelFormatInfo2_Release(info);IWICComponentInfo_Release(component);fflush(stdout);
    }
    IWICImagingFactory_Release(factory);CoUninitialize();puts("done");return 0;
}
