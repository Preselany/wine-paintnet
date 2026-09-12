/* Default pixel-format color contexts, compared with native Windows.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include "wine/test.h"
#include "wincodec.h"
#include "icm.h"
static const struct {const GUID *format; const char *name; UINT profile; BOOL missing;} formats[] = {
    {&GUID_WICPixelFormat1bppIndexed, "1bppIndexed", 1, FALSE},
    {&GUID_WICPixelFormat2bppIndexed, "2bppIndexed", 1, FALSE},
    {&GUID_WICPixelFormat4bppIndexed, "4bppIndexed", 1, FALSE},
    {&GUID_WICPixelFormat8bppIndexed, "8bppIndexed", 1, FALSE},
    {&GUID_WICPixelFormatBlackWhite, "BlackWhite", 1, FALSE},
    {&GUID_WICPixelFormat2bppGray, "2bppGray", 1, FALSE},
    {&GUID_WICPixelFormat4bppGray, "4bppGray", 1, FALSE},
    {&GUID_WICPixelFormat8bppGray, "8bppGray", 1, FALSE},
    {&GUID_WICPixelFormat16bppGray, "16bppGray", 1, FALSE},
    {&GUID_WICPixelFormat8bppAlpha, "8bppAlpha", 0, FALSE},
    {&GUID_WICPixelFormat16bppBGR555, "16bppBGR555", 1, FALSE},
    {&GUID_WICPixelFormat16bppBGR565, "16bppBGR565", 1, FALSE},
    {&GUID_WICPixelFormat16bppBGRA5551, "16bppBGRA5551", 1, FALSE},
    {&GUID_WICPixelFormat24bppBGR, "24bppBGR", 1, FALSE},
    {&GUID_WICPixelFormat24bppRGB, "24bppRGB", 1, FALSE},
    {&GUID_WICPixelFormat32bppBGR, "32bppBGR", 1, FALSE},
    {&GUID_WICPixelFormat32bppBGRA, "32bppBGRA", 1, FALSE},
    {&GUID_WICPixelFormat32bppPBGRA, "32bppPBGRA", 1, FALSE},
    {&GUID_WICPixelFormat32bppRGB, "32bppRGB", 1, FALSE},
    {&GUID_WICPixelFormat32bppRGBA, "32bppRGBA", 1, FALSE},
    {&GUID_WICPixelFormat32bppPRGBA, "32bppPRGBA", 1, FALSE},
    {&GUID_WICPixelFormat32bppGrayFloat, "32bppGrayFloat", 0, FALSE},
    {&GUID_WICPixelFormat48bppRGB, "48bppRGB", 1, FALSE},
    {&GUID_WICPixelFormat48bppBGR, "48bppBGR", 1, TRUE},
    {&GUID_WICPixelFormat64bppRGB, "64bppRGB", 1, TRUE},
    {&GUID_WICPixelFormat64bppRGBA, "64bppRGBA", 1, FALSE},
    {&GUID_WICPixelFormat64bppBGRA, "64bppBGRA", 1, TRUE},
    {&GUID_WICPixelFormat64bppPRGBA, "64bppPRGBA", 1, FALSE},
    {&GUID_WICPixelFormat64bppPBGRA, "64bppPBGRA", 1, TRUE},
    {&GUID_WICPixelFormat16bppGrayFixedPoint, "16bppGrayFixedPoint", 0, TRUE},
    {&GUID_WICPixelFormat32bppBGR101010, "32bppBGR101010", 1, TRUE},
    {&GUID_WICPixelFormat48bppRGBFixedPoint, "48bppRGBFixedPoint", 0, TRUE},
    {&GUID_WICPixelFormat48bppBGRFixedPoint, "48bppBGRFixedPoint", 0, TRUE},
    {&GUID_WICPixelFormat96bppRGBFixedPoint, "96bppRGBFixedPoint", 0, TRUE},
    {&GUID_WICPixelFormat96bppRGBFloat, "96bppRGBFloat", 0, FALSE},
    {&GUID_WICPixelFormat128bppRGBAFloat, "128bppRGBAFloat", 0, FALSE},
    {&GUID_WICPixelFormat128bppPRGBAFloat, "128bppPRGBAFloat", 0, FALSE},
    {&GUID_WICPixelFormat128bppRGBFloat, "128bppRGBFloat", 0, FALSE},
    {&GUID_WICPixelFormat32bppCMYK, "32bppCMYK", 2, FALSE},
    {&GUID_WICPixelFormat64bppRGBAFixedPoint, "64bppRGBAFixedPoint", 0, TRUE},
    {&GUID_WICPixelFormat64bppBGRAFixedPoint, "64bppBGRAFixedPoint", 0, TRUE},
    {&GUID_WICPixelFormat64bppRGBFixedPoint, "64bppRGBFixedPoint", 0, TRUE},
    {&GUID_WICPixelFormat128bppRGBAFixedPoint, "128bppRGBAFixedPoint", 0, TRUE},
    {&GUID_WICPixelFormat128bppRGBFixedPoint, "128bppRGBFixedPoint", 0, TRUE},
    {&GUID_WICPixelFormat64bppRGBAHalf, "64bppRGBAHalf", 0, FALSE},
    {&GUID_WICPixelFormat64bppPRGBAHalf, "64bppPRGBAHalf", 0, FALSE},
    {&GUID_WICPixelFormat64bppRGBHalf, "64bppRGBHalf", 0, TRUE},
    {&GUID_WICPixelFormat48bppRGBHalf, "48bppRGBHalf", 0, FALSE},
    {&GUID_WICPixelFormat32bppRGBE, "32bppRGBE", 0, TRUE},
    {&GUID_WICPixelFormat16bppGrayHalf, "16bppGrayHalf", 0, FALSE},
    {&GUID_WICPixelFormat32bppGrayFixedPoint, "32bppGrayFixedPoint", 0, TRUE},
    {&GUID_WICPixelFormat32bppRGBA1010102, "32bppRGBA1010102", 1, TRUE},
    {&GUID_WICPixelFormat32bppRGBA1010102XR, "32bppRGBA1010102XR", 0, TRUE},
    {&GUID_WICPixelFormat32bppR10G10B10A2HDR10, "32bppR10G10B10A2HDR10", 1, TRUE},
    {&GUID_WICPixelFormat64bppCMYK, "64bppCMYK", 2, FALSE},
    {&GUID_WICPixelFormat24bpp3Channels, "24bpp3Channels", 1, TRUE},
    {&GUID_WICPixelFormat32bpp4Channels, "32bpp4Channels", 2, TRUE},
    {&GUID_WICPixelFormat40bpp5Channels, "40bpp5Channels", 0, TRUE},
    {&GUID_WICPixelFormat48bpp6Channels, "48bpp6Channels", 0, TRUE},
    {&GUID_WICPixelFormat56bpp7Channels, "56bpp7Channels", 0, TRUE},
    {&GUID_WICPixelFormat64bpp8Channels, "64bpp8Channels", 0, TRUE},
    {&GUID_WICPixelFormat48bpp3Channels, "48bpp3Channels", 1, TRUE},
    {&GUID_WICPixelFormat64bpp4Channels, "64bpp4Channels", 2, TRUE},
    {&GUID_WICPixelFormat80bpp5Channels, "80bpp5Channels", 0, TRUE},
    {&GUID_WICPixelFormat96bpp6Channels, "96bpp6Channels", 0, TRUE},
    {&GUID_WICPixelFormat112bpp7Channels, "112bpp7Channels", 0, TRUE},
    {&GUID_WICPixelFormat128bpp8Channels, "128bpp8Channels", 0, TRUE},
    {&GUID_WICPixelFormat40bppCMYKAlpha, "40bppCMYKAlpha", 2, TRUE},
    {&GUID_WICPixelFormat80bppCMYKAlpha, "80bppCMYKAlpha", 2, TRUE},
    {&GUID_WICPixelFormat32bpp3ChannelsAlpha, "32bpp3ChannelsAlpha", 1, TRUE},
    {&GUID_WICPixelFormat40bpp4ChannelsAlpha, "40bpp4ChannelsAlpha", 2, TRUE},
    {&GUID_WICPixelFormat48bpp5ChannelsAlpha, "48bpp5ChannelsAlpha", 0, TRUE},
    {&GUID_WICPixelFormat56bpp6ChannelsAlpha, "56bpp6ChannelsAlpha", 0, TRUE},
    {&GUID_WICPixelFormat64bpp7ChannelsAlpha, "64bpp7ChannelsAlpha", 0, TRUE},
    {&GUID_WICPixelFormat72bpp8ChannelsAlpha, "72bpp8ChannelsAlpha", 0, TRUE},
    {&GUID_WICPixelFormat64bpp3ChannelsAlpha, "64bpp3ChannelsAlpha", 1, TRUE},
    {&GUID_WICPixelFormat80bpp4ChannelsAlpha, "80bpp4ChannelsAlpha", 2, TRUE},
    {&GUID_WICPixelFormat96bpp5ChannelsAlpha, "96bpp5ChannelsAlpha", 0, TRUE},
    {&GUID_WICPixelFormat112bpp6ChannelsAlpha, "112bpp6ChannelsAlpha", 0, TRUE},
    {&GUID_WICPixelFormat128bpp7ChannelsAlpha, "128bpp7ChannelsAlpha", 0, TRUE},
    {&GUID_WICPixelFormat144bpp8ChannelsAlpha, "144bpp8ChannelsAlpha", 0, TRUE},
    {&GUID_WICPixelFormat8bppY, "8bppY", 1, TRUE},
    {&GUID_WICPixelFormat8bppCb, "8bppCb", 1, TRUE},
    {&GUID_WICPixelFormat8bppCr, "8bppCr", 1, TRUE},
    {&GUID_WICPixelFormat16bppCbCr, "16bppCbCr", 1, TRUE},
};

static BYTE *load_profile(const WCHAR *path, DWORD *size)
{
    HANDLE file;
    BYTE *bytes;
    DWORD read;

    file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    if(file==INVALID_HANDLE_VALUE)return NULL;
    *size=GetFileSize(file,NULL);
    bytes=malloc(*size);
    if(!bytes || !ReadFile(file,bytes,*size,&read,NULL) || read!=*size)
    {
        free(bytes);
        bytes=NULL;
    }
    CloseHandle(file);
    return bytes;
}

START_TEST(pixel_color_context)
{
    IWICImagingFactory *factory;
    IWICComponentInfo *component;
    IWICPixelFormatInfo2 *info;
    IWICColorContext *context, *second;
    WICColorContextType type;
    WCHAR path[MAX_PATH];
    BYTE *profiles[2]={0}, *bytes;
    DWORD sizes[2]={0}, size;
    UINT i, actual, exif;
    HRESULT hr;
    BOOL ret;

    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    hr=CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void **)&factory);
    ok(hr==S_OK,"Factory returned %#lx.\n",hr);
    if(FAILED(hr))
    {
        CoUninitialize();
        return;
    }
    size=sizeof(path);
    ret=GetStandardColorSpaceProfileW(NULL,LCS_sRGB,path,&size);
    ok(ret,"No standard sRGB profile path, error %lu.\n",GetLastError());
    if(ret)profiles[0]=load_profile(path,&sizes[0]);
    ok(profiles[0]!=NULL,"Cannot load sRGB profile.\n");
    size=sizeof(path);
    if(GetColorDirectoryW(NULL,path,&size))
    {
        if (lstrlenW(path) + ARRAY_SIZE(L"\\RSWOP.icm") <= ARRAY_SIZE(path))
        {
            lstrcatW(path,L"\\RSWOP.icm");
            profiles[1]=load_profile(path,&sizes[1]);
        }
    }
    for(i=0;i<ARRAY_SIZE(formats);++i)
    {
        winetest_push_context("%s",formats[i].name);
        hr=IWICImagingFactory_CreateComponentInfo(factory,formats[i].format,&component);
        todo_wine_if(formats[i].missing)ok(hr==S_OK,"Component returned %#lx.\n",hr);
        if(FAILED(hr))goto next;
        hr=IWICComponentInfo_QueryInterface(component,&IID_IWICPixelFormatInfo2,(void **)&info);
        ok(hr==S_OK,"Pixel info returned %#lx.\n",hr);
        IWICComponentInfo_Release(component);
        if(FAILED(hr))goto next;
        hr=IWICPixelFormatInfo2_GetColorContext(info,NULL);
        ok(hr==E_INVALIDARG,"Null output returned %#lx.\n",hr);
        context=(void *)0xdeadbeef;
        hr=IWICPixelFormatInfo2_GetColorContext(info,&context);
        if(!formats[i].profile)
        {
            ok(hr==WINCODEC_ERR_UNSUPPORTEDOPERATION,"Unexpected result %#lx.\n",hr);
            ok(context==(void *)0xdeadbeef,"Output changed on failure.\n");
        }
        else
        {
            /* Wine does not distribute the native RSWOP profile. */
            todo_wine_if(formats[i].profile==2 && !profiles[1])
                ok(hr==S_OK,"Color context returned %#lx.\n",hr);
            if(SUCCEEDED(hr))
            {
                hr=IWICColorContext_GetType(context,&type);
                ok(hr==S_OK && type==WICColorContextProfile,"Type %#lx, %u.\n",hr,type);
                hr=IWICColorContext_GetExifColorSpace(context,&exif);
                ok(hr==S_OK && exif==~0u,"Exif %#lx, %u.\n",hr,exif);
                hr=IWICColorContext_GetProfileBytes(context,0,NULL,&actual);
                ok(hr==S_OK && actual>=128,"Profile size %#lx, %u.\n",hr,actual);
                if(SUCCEEDED(hr) && (bytes=malloc(actual)))
                {
                    hr=IWICColorContext_GetProfileBytes(context,actual,bytes,&actual);
                    ok(hr==S_OK,"Profile bytes returned %#lx.\n",hr);
                    ok(actual>=128 && !memcmp(bytes+36,"acsp",4),"Invalid ICC header.\n");
                    ok(actual>=128 && !memcmp(bytes+16,formats[i].profile==1?"RGB ":"CMYK",4),"Wrong ICC color space.\n");
                    if(profiles[formats[i].profile-1])
                    {
                        ok(actual==sizes[formats[i].profile-1],"Profile length %u, expected %lu.\n",actual,sizes[formats[i].profile-1]);
                        ok(actual==sizes[formats[i].profile-1] && !memcmp(bytes,profiles[formats[i].profile-1],actual),
                                "Default profile bytes differ.\n");
                    }
                    free(bytes);
                }
                hr=IWICPixelFormatInfo2_GetColorContext(info,&second);
                ok(hr==S_OK,"Second context returned %#lx.\n",hr);
                if(SUCCEEDED(hr))
                {
                    ok(second==context,"Default color context was not cached.\n");
                    IWICColorContext_Release(second);
                }
                IWICColorContext_Release(context);
            }
            else ok(context==(void *)0xdeadbeef,"Output changed on failure.\n");
        }
        IWICPixelFormatInfo2_Release(info);
next:
        winetest_pop_context();
    }
    free(profiles[0]);free(profiles[1]);
    IWICImagingFactory_Release(factory);
    CoUninitialize();
}
