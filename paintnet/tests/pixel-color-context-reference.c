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
    const struct {const char *name;const GUID *format;} formats[]={F(1bppIndexed),F(8bppIndexed),F(8bppGray),
        F(16bppGray),F(16bppGrayHalf),F(32bppGrayFloat),F(24bppBGR),F(32bppBGRA),F(32bppPBGRA),F(32bppRGBA),
        F(64bppRGBA),F(64bppRGBAHalf),F(128bppRGBAFloat),F(128bppPRGBAFloat),F(32bppCMYK),F(8bppAlpha)};
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
        context=(void *)0xdeadbeef;hr=IWICPixelFormatInfo2_GetColorContext(info,&context);printf("context %08lx present %u\n",hr,context!=NULL);
        if(SUCCEEDED(hr)&&context)
        {
            type=99;hr=IWICColorContext_GetType(context,&type);printf("type %08lx %u\n",hr,type);
            exif=99;hr=IWICColorContext_GetExifColorSpace(context,&exif);printf("exif %08lx %u\n",hr,exif);
            size=0;hr=IWICColorContext_GetProfileBytes(context,0,NULL,&size);printf("profile %08lx %u\n",hr,size);
            if(SUCCEEDED(hr)&&size && size<65536)
            {
                UINT j;bytes=malloc(size);hr=IWICColorContext_GetProfileBytes(context,size,bytes,&size);
                printf("profile bytes %08lx",hr);for(j=0;j<size;++j)printf(" %02x",bytes[j]);puts("");free(bytes);
            }
            IWICColorContext_Release(context);
        }
        IWICPixelFormatInfo2_Release(info);
    }
    IWICImagingFactory_Release(factory);CoUninitialize();puts("done");return 0;
}
