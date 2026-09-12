/* Native default color contexts for WIC pixel formats.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <stdio.h>
#include <stdlib.h>
#include "windows.h"
#include "initguid.h"
#include "wincodec.h"
#define F(name) {#name,&GUID_WICPixelFormat##name}
int main(void)
{
    const struct {const char *name;const GUID *format;} formats[]={F(DontCare),F(1bppIndexed),F(2bppIndexed),F(4bppIndexed),F(8bppIndexed),F(BlackWhite),F(2bppGray),F(4bppGray),F(8bppGray),F(16bppGray),F(8bppAlpha),F(16bppBGR555),F(16bppBGR565),F(16bppBGRA5551),F(24bppBGR),F(24bppRGB),F(32bppBGR),F(32bppBGRA),F(32bppPBGRA),F(32bppRGB),F(32bppRGBA),F(32bppPRGBA),F(32bppGrayFloat),F(48bppRGB),F(48bppBGR),F(64bppRGB),F(64bppRGBA),F(64bppBGRA),F(64bppPRGBA),F(64bppPBGRA),F(16bppGrayFixedPoint),F(32bppBGR101010),F(48bppRGBFixedPoint),F(48bppBGRFixedPoint),F(96bppRGBFixedPoint),F(96bppRGBFloat),F(128bppRGBAFloat),F(128bppPRGBAFloat),F(128bppRGBFloat),F(32bppCMYK),F(64bppRGBAFixedPoint),F(64bppBGRAFixedPoint),F(64bppRGBFixedPoint),F(128bppRGBAFixedPoint),F(128bppRGBFixedPoint),F(64bppRGBAHalf),F(64bppPRGBAHalf),F(64bppRGBHalf),F(48bppRGBHalf),F(32bppRGBE),F(16bppGrayHalf),F(32bppGrayFixedPoint),F(32bppRGBA1010102),F(32bppRGBA1010102XR),F(32bppR10G10B10A2),F(32bppR10G10B10A2HDR10),F(64bppCMYK),F(24bpp3Channels),F(32bpp4Channels),F(40bpp5Channels),F(48bpp6Channels),F(56bpp7Channels),F(64bpp8Channels),F(48bpp3Channels),F(64bpp4Channels),F(80bpp5Channels),F(96bpp6Channels),F(112bpp7Channels),F(128bpp8Channels),F(40bppCMYKAlpha),F(80bppCMYKAlpha),F(32bpp3ChannelsAlpha),F(40bpp4ChannelsAlpha),F(48bpp5ChannelsAlpha),F(56bpp6ChannelsAlpha),F(64bpp7ChannelsAlpha),F(72bpp8ChannelsAlpha),F(64bpp3ChannelsAlpha),F(80bpp4ChannelsAlpha),F(96bpp5ChannelsAlpha),F(112bpp6ChannelsAlpha),F(128bpp7ChannelsAlpha),F(144bpp8ChannelsAlpha),F(8bppY),F(8bppCb),F(8bppCr),F(16bppCbCr),F(16bppYQuantizedDctCoefficients),F(16bppCbQuantizedDctCoefficients),F(16bppCrQuantizedDctCoefficients)};
    IWICImagingFactory *factory;IWICComponentInfo *component;IWICPixelFormatInfo2 *info;IWICColorContext *context;
    WICColorContextType type;UINT size,exif,i;HRESULT hr;BYTE *bytes;
    CoInitializeEx(NULL,COINIT_MULTITHREADED);setvbuf(stdout,NULL,_IOLBF,0);
    hr=CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void **)&factory);
    if(FAILED(hr))return 1;
    for(i=0;i<ARRAY_SIZE(formats);++i)
    {
        printf("case %s\n",formats[i].name);
        hr=IWICImagingFactory_CreateComponentInfo(factory,formats[i].format,&component);printf("component %08lx\n",hr);if(FAILED(hr))continue;
        hr=IWICComponentInfo_QueryInterface(component,&IID_IWICPixelFormatInfo2,(void **)&info);IWICComponentInfo_Release(component);if(FAILED(hr))continue;
        hr=IWICPixelFormatInfo2_GetColorContext(info,NULL);printf("null context %08lx\n",hr);
        context=(void *)0xdeadbeef;hr=IWICPixelFormatInfo2_GetColorContext(info,&context);printf("context %08lx present %u\n",hr,context!=NULL);
        if(SUCCEEDED(hr)&&context)
        {
            type=99;hr=IWICColorContext_GetType(context,&type);printf("type %08lx %u\n",hr,type);
            exif=99;hr=IWICColorContext_GetExifColorSpace(context,&exif);printf("exif %08lx %u\n",hr,exif);
            size=0;hr=IWICColorContext_GetProfileBytes(context,0,NULL,&size);printf("profile %08lx %u\n",hr,size);
            if(SUCCEEDED(hr)&&size && size<65536)
            {
                UINT j;bytes=malloc(size);hr=IWICColorContext_GetProfileBytes(context,size,bytes,&size);
                printf("profile bytes %08lx",hr);for(j=0;j<size && j<128;++j)printf(" %02x",bytes[j]);puts("");free(bytes);
            }
            IWICColorContext_Release(context);
        }
        IWICPixelFormatInfo2_Release(info);
    }
    IWICImagingFactory_Release(factory);CoUninitialize();puts("done");return 0;
}
