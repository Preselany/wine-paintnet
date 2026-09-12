/* Alpha-only pixel format registration and bitmap storage regressions.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#include "wine/test.h"
#define REQUIRE(call) do { hr=(call); ok(hr==S_OK,"%s returned %#lx.\n",#call,hr); if(FAILED(hr))return; } while(0)

static void test_alpha_format(void)
{
    static const struct {const GUID *format;UINT bits,channels;BOOL alpha;} formats[]={
        {&GUID_WICPixelFormat8bppAlpha,8,1,TRUE},
    };
    IWICImagingFactory *factory;
    IWICComponentInfo *component;
    IWICPixelFormatInfo2 *info;
    IWICBitmap *bitmap;
    WICPixelFormatNumericRepresentation numeric;
    WICRect rect={1,0,1,2};
    BYTE mask[16],expected[16],source[128],output[128];
    UINT f,c,i,bits,channels,actual,stride,bytes,width,height;
    BOOL alpha;
    HRESULT hr;
    REQUIRE(CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,
            &IID_IWICImagingFactory,(void **)&factory));
    for(f=0;f<ARRAY_SIZE(formats);++f)
    {
        winetest_push_context("format %u",f);
        REQUIRE(IWICImagingFactory_CreateComponentInfo(factory,formats[f].format,&component));
        REQUIRE(IWICComponentInfo_QueryInterface(component,&IID_IWICPixelFormatInfo2,(void **)&info));
        REQUIRE(IWICPixelFormatInfo2_GetBitsPerPixel(info,&bits));
        REQUIRE(IWICPixelFormatInfo2_GetChannelCount(info,&channels));
        REQUIRE(IWICPixelFormatInfo2_SupportsTransparency(info,&alpha));
        REQUIRE(IWICPixelFormatInfo2_GetNumericRepresentation(info,&numeric));
        ok(bits==formats[f].bits,"Got %u bits.\n",bits);
        ok(channels==formats[f].channels,"Got %u channels.\n",channels);
        ok(alpha==formats[f].alpha,"Got alpha flag %d.\n",alpha);
        ok(numeric==WICPixelFormatNumericRepresentationUnsignedInteger,"Got representation %u.\n",numeric);
        bytes=formats[f].bits/8;
        for(c=0;c<channels;++c)
        {
            actual=0xdeadbeef;
            REQUIRE(IWICPixelFormatInfo2_GetChannelMask(info,c,0,NULL,&actual));
            ok(actual==bytes,"Mask size %u, expected %u.\n",actual,bytes);
            memset(mask,0xcc,sizeof(mask));actual=0xdeadbeef;
            memset(expected,0xcc,sizeof(expected));memset(expected,0,bytes);
            memset(expected+c,0xff,1);
            REQUIRE(IWICPixelFormatInfo2_GetChannelMask(info,c,sizeof(mask),mask,&actual));
            ok(actual==bytes && !memcmp(mask,expected,sizeof(mask)),"Unexpected channel %u mask.\n",c);
        }
        actual=0xdeadbeef;
        hr=IWICPixelFormatInfo2_GetChannelMask(info,channels,sizeof(mask),mask,&actual);
        ok(hr==E_INVALIDARG && actual==0xdeadbeef,"Invalid channel: %#lx, %u.\n",hr,actual);
        for(i=0;i<sizeof(source);++i)source[i]=i*13+7;
        stride=3*bytes+4;
        REQUIRE(IWICImagingFactory_CreateBitmapFromMemory(factory,3,2,formats[f].format,
                stride,2*stride,source,&bitmap));
        REQUIRE(IWICBitmap_GetSize(bitmap,&width,&height));
        ok(width==3 && height==2,"Unexpected dimensions %u x %u.\n",width,height);
        memset(output,0xcc,sizeof(output));
        REQUIRE(IWICBitmap_CopyPixels(bitmap,NULL,stride,sizeof(output),output));
        for(i=0;i<2;++i)
            ok(!memcmp(source+i*stride,output+i*stride,3*bytes),"Pixel row %u differs.\n",i);
        memset(output,0xcc,sizeof(output));
        REQUIRE(IWICBitmap_CopyPixels(bitmap,&rect,bytes,sizeof(output),output));
        for(i=0;i<2;++i)
            ok(!memcmp(source+i*stride+bytes,output+i*bytes,bytes),"Cropped row %u differs.\n",i);
        IWICBitmap_Release(bitmap);IWICPixelFormatInfo2_Release(info);IWICComponentInfo_Release(component);
        winetest_pop_context();
    }
    IWICImagingFactory_Release(factory);
}

START_TEST(alpha_format)
{
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    test_alpha_format();
    CoUninitialize();
}
