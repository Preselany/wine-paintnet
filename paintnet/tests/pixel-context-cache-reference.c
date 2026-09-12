/* Default pixel-format color context identity and initialization semantics.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <stdio.h>
#include "windows.h"
#include "initguid.h"
#include "wincodec.h"
#include "icm.h"

int main(void)
{
    const GUID *formats[] = {&GUID_WICPixelFormat32bppBGRA, &GUID_WICPixelFormat24bppBGR,
            &GUID_WICPixelFormat8bppGray, &GUID_WICPixelFormat32bppCMYK};
    IWICImagingFactory *factory;
    IWICComponentInfo *component;
    IWICPixelFormatInfo2 *info;
    IWICColorContext *contexts[4] = {0}, *second;
    BYTE bytes[] = {1, 2, 3, 4};
    WCHAR filename[MAX_PATH];
    DWORD size = sizeof(filename);
    UINT i, actual;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    setvbuf(stdout, NULL, _IOLBF, 0);
    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
            &IID_IWICImagingFactory, (void **)&factory);
    if (FAILED(hr)) return 1;
    for (i = 0; i < ARRAY_SIZE(formats); ++i)
    {
        hr = IWICImagingFactory_CreateComponentInfo(factory, formats[i], &component);
        if (FAILED(hr)) return 2;
        hr = IWICComponentInfo_QueryInterface(component, &IID_IWICPixelFormatInfo2, (void **)&info);
        IWICComponentInfo_Release(component);
        if (FAILED(hr)) return 3;
        hr = IWICPixelFormatInfo2_GetColorContext(info, &contexts[i]);
        printf("format %u context %08lx\n", i, hr);
        if (SUCCEEDED(hr))
        {
            hr = IWICPixelFormatInfo2_GetColorContext(info, &second);
            printf("repeat %08lx same %u same-first %u\n", hr, second == contexts[i], contexts[i] == contexts[0]);
            if (SUCCEEDED(hr)) IWICColorContext_Release(second);
        }
        IWICPixelFormatInfo2_Release(info);
    }
    if (contexts[0])
    {
        hr = IWICColorContext_InitializeFromMemory(contexts[0], bytes, sizeof(bytes));
        printf("init-memory %08lx\n", hr);
        hr = IWICColorContext_GetProfileBytes(contexts[0], 0, NULL, &actual);
        printf("profile-after-memory %08lx %u\n", hr, actual);
        hr = IWICColorContext_InitializeFromExifColorSpace(contexts[0], 1);
        printf("init-exif %08lx\n", hr);
        if (GetStandardColorSpaceProfileW(NULL, LCS_sRGB, filename, &size))
        {
            hr = IWICColorContext_InitializeFromFilename(contexts[0], filename);
            printf("init-file %08lx\n", hr);
        }
        for (i = 0; i < ARRAY_SIZE(contexts); ++i)
        {
            if (!contexts[i]) continue;
            hr = IWICColorContext_GetProfileBytes(contexts[i], 0, NULL, &actual);
            printf("final-profile %u %08lx %u\n", i, hr, actual);
            IWICColorContext_Release(contexts[i]);
        }
    }
    IWICImagingFactory_Release(factory);
    CoUninitialize();
    return 0;
}
