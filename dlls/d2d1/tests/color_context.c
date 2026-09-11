/* Direct2D color profile resource tests.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include "d2d1_3.h"
#include "d3d11.h"
#include "wincodec.h"
#include "wine/test.h"
#include "effect_test.h"

static void test_profile(ID2D1DeviceContext *dc, ID2D1Factory1 *factory, D2D1_COLOR_SPACE space)
{
    ID2D1ColorContext *context = NULL, *copy, *returned;
    ID2D1ColorContext1 *context1;
    ID2D1Factory *returned_factory;
    ID2D1Bitmap1 *bitmap;
    D2D1_BITMAP_PROPERTIES1 desc = {{DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED},96,96,0,NULL};
    D2D1_SIZE_U bitmap_size = {2,2};
    D2D1_SIMPLE_COLOR_PROFILE simple, before;
    const IID *iids[] = {&IID_IUnknown,&IID_ID2D1Resource,&IID_ID2D1ColorContext,&IID_ID2D1ColorContext1};
    IUnknown *unknown;
    BYTE *profile, *buffer;
    UINT size, i;
    ULONG ref;
    HRESULT hr;

    hr = ID2D1DeviceContext_CreateColorContext(dc,space,NULL,0,&context);
    ok(hr == S_OK,"space %u: CreateColorContext returned %#lx.\n",space,hr);
    if (FAILED(hr)) return;
    ok(ID2D1ColorContext_GetColorSpace(context) == space,"Wrong color space.\n");
    size = ID2D1ColorContext_GetProfileSize(context);
    ok(size >= 128 && size < 65536,"Unexpected ICC size %u.\n",size);
    if (size < 128 || size >= 65536) goto done;
    for (i = 0; i < ARRAY_SIZE(iids); ++i)
    {
        hr = ID2D1ColorContext_QueryInterface(context,iids[i],(void **)&unknown);
        ok(hr == S_OK,"Interface %u returned %#lx.\n",i,hr);
        ok(unknown == (IUnknown *)context,"Interface %u changes identity.\n",i);
        if (SUCCEEDED(hr)) IUnknown_Release(unknown);
    }
    unknown = (void *)0xdeadbeef;
    hr = ID2D1ColorContext_QueryInterface(context,&IID_ID2D1Bitmap,(void **)&unknown);
    ok(hr == E_NOINTERFACE && unknown == (void *)0xdeadbeef,"Invalid QI returned %#lx, %p.\n",hr,unknown);
    ID2D1ColorContext_GetFactory(context,&returned_factory);
    ok(returned_factory == (ID2D1Factory *)factory,"Wrong factory.\n");
    ID2D1Factory_Release(returned_factory);
    hr = ID2D1ColorContext_QueryInterface(context,&IID_ID2D1ColorContext1,(void **)&context1);
    if (SUCCEEDED(hr))
    {
        ok(ID2D1ColorContext1_GetColorContextType(context1) == D2D1_COLOR_CONTEXT_TYPE_ICC,"Wrong context type.\n");
        ok(ID2D1ColorContext1_GetDXGIColorSpace(context1) == space-1,"Wrong DXGI space.\n");
        memset(&simple,0xcc,sizeof(simple)); before = simple;
        hr = ID2D1ColorContext1_GetSimpleColorProfile(context1,&simple);
        ok(hr == E_INVALIDARG,"GetSimpleColorProfile returned %#lx.\n",hr);
        ok(!memcmp(&simple,&before,sizeof(simple)),"Failed getter changed output.\n");
        ID2D1ColorContext1_Release(context1);
    }
    profile = malloc(size); buffer = malloc(size+16);
    hr = ID2D1ColorContext_GetProfile(context,profile,size);
    ok(hr == S_OK,"GetProfile returned %#lx.\n",hr);
    ok(!memcmp(profile+36,"acsp",4),"Missing ICC signature.\n");
    ok(!memcmp(profile+16,"RGB ",4),"Expected RGB ICC profile.\n");
    memset(buffer,0xcc,size+16);
    hr = ID2D1ColorContext_GetProfile(context,buffer,size-1);
    ok(hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER),"Short buffer returned %#lx.\n",hr);
    for (i = 0; i < size+16; ++i)
        ok(buffer[i] == (i < size-1 ? 0 : 0xcc),"Short buffer byte %u is %#x.\n",i,buffer[i]);
    memset(buffer,0xcc,size+16);
    hr = ID2D1ColorContext_GetProfile(context,buffer,size+8);
    ok(hr == S_OK,"Oversized buffer returned %#lx.\n",hr);
    ok(!memcmp(buffer,profile,size),"Profile copy mismatch.\n");
    for (i = size; i < size+16; ++i)
        ok(buffer[i] == (i < size+8 ? 0 : 0xcc),"Tail byte %u is %#x.\n",i,buffer[i]);
    hr = ID2D1DeviceContext_CreateColorContext(dc,D2D1_COLOR_SPACE_CUSTOM,profile,size,&copy);
    ok(hr == S_OK,"Custom profile returned %#lx.\n",hr);
    if (SUCCEEDED(hr))
    {
        ok(ID2D1ColorContext_GetColorSpace(copy) == D2D1_COLOR_SPACE_CUSTOM,"Custom API reclassified ICC.\n");
        memset(profile,0xcc,size);
        hr = ID2D1ColorContext_GetProfile(copy,profile,size);
        ok(hr == S_OK && !memcmp(profile,buffer,size),"Custom profile did not retain its own bytes.\n");
        ID2D1ColorContext_Release(copy);
    }
    desc.colorContext = context;
    hr = ID2D1DeviceContext_CreateBitmap(dc,bitmap_size,NULL,0,&desc,&bitmap);
    ok(hr == S_OK,"CreateBitmap returned %#lx.\n",hr);
    if (SUCCEEDED(hr))
    {
        ID2D1Bitmap1_GetColorContext(bitmap,&returned);
        ok(returned == context,"Bitmap did not retain context.\n");
        ref = ID2D1ColorContext_Release(context);
        context = NULL;
        ok(ref >= 2,"Expected bitmap and getter references, got %lu.\n",ref);
        ID2D1Bitmap1_Release(bitmap);
        if (returned)
        {
            hr = ID2D1ColorContext_GetProfile(returned,profile,size);
            ok(hr == S_OK && !memcmp(profile,buffer,size),"Getter reference did not survive bitmap destruction.\n");
            ID2D1ColorContext_Release(returned);
        }
    }
    desc.colorContext = NULL;
    hr = ID2D1DeviceContext_CreateBitmap(dc,bitmap_size,NULL,0,&desc,&bitmap);
    ok(hr == S_OK,"CreateBitmap returned %#lx.\n",hr);
    if (SUCCEEDED(hr))
    {
        returned = (void *)0xdeadbeef;
        ID2D1Bitmap1_GetColorContext(bitmap,&returned);
        ok(!returned,"Expected no color context, got %p.\n",returned);
        ID2D1Bitmap1_Release(bitmap);
    }
    free(profile); free(buffer);
done:
    if (context) ID2D1ColorContext_Release(context);
}

static void test_extended(ID2D1DeviceContext *dc)
{
    ID2D1DeviceContext5 *dc5;
    ID2D1ColorContext1 *context;
    D2D1_SIMPLE_COLOR_PROFILE simple = {{0.64f,0.33f},{0.3f,0.6f},{0.15f,0.06f},{0.95047f,1.08883f},0};
    D2D1_SIMPLE_COLOR_PROFILE returned, saved;
    BYTE buffer[16], before[16];
    unsigned int i;
    BOOL valid;
    HRESULT hr;

    hr = ID2D1DeviceContext_QueryInterface(dc,&IID_ID2D1DeviceContext5,(void **)&dc5);
    if (FAILED(hr)) { win_skip("DeviceContext5 unavailable.\n"); return; }
    for (i = 0; i < 27; ++i)
    {
        DXGI_COLOR_SPACE_TYPE space = i == 26 ? DXGI_COLOR_SPACE_CUSTOM : i;
        valid = i <= 3 || i == 12 || i == 14 || i == 17;
        context = (void *)0xdeadbeef;
        hr = ID2D1DeviceContext5_CreateColorContextFromDxgiColorSpace(dc5,space,&context);
        ok(hr == (valid ? S_OK : E_INVALIDARG),"DXGI space %u returned %#lx.\n",space,hr);
        if (!valid) { ok(!context,"Failure did not clear output.\n"); continue; }
        if (FAILED(hr)) continue;
        ok(ID2D1ColorContext1_GetColorContextType(context) == D2D1_COLOR_CONTEXT_TYPE_DXGI,"Wrong type.\n");
        ok(ID2D1ColorContext1_GetDXGIColorSpace(context) == space,"Wrong DXGI space.\n");
        ok(ID2D1ColorContext1_GetColorSpace(context) == (i < 2 ? i+1 : 0),"Wrong D2D space.\n");
        ok(!ID2D1ColorContext1_GetProfileSize(context),"DXGI profile has ICC bytes.\n");
        memset(buffer,0xcc,sizeof(buffer)); memcpy(before,buffer,sizeof(buffer));
        hr = ID2D1ColorContext1_GetProfile(context,buffer,sizeof(buffer));
        ok(hr == E_INVALIDARG && !memcmp(buffer,before,sizeof(buffer)),"ICC getter returned %#lx or changed output.\n",hr);
        ID2D1ColorContext1_Release(context);
    }
    for (i = 0; i < 5; ++i)
    {
        simple.gamma = i == 4 ? 0xffffffff : i;
        saved = simple;
        hr = ID2D1DeviceContext5_CreateColorContextFromSimpleColorProfile(dc5,&simple,&context);
        ok(hr == S_OK,"Simple gamma %u returned %#lx.\n",simple.gamma,hr);
        if (FAILED(hr)) continue;
        simple.whitePointXZ.x = 0;
        ok(ID2D1ColorContext1_GetColorContextType(context) == D2D1_COLOR_CONTEXT_TYPE_SIMPLE,"Wrong type.\n");
        ok(ID2D1ColorContext1_GetDXGIColorSpace(context) == DXGI_COLOR_SPACE_CUSTOM,"Wrong DXGI space.\n");
        ok(ID2D1ColorContext1_GetColorSpace(context) == D2D1_COLOR_SPACE_CUSTOM,"Wrong D2D space.\n");
        ok(!ID2D1ColorContext1_GetProfileSize(context),"Simple profile has ICC bytes.\n");
        hr = ID2D1ColorContext1_GetSimpleColorProfile(context,&returned);
        ok(hr == S_OK && !memcmp(&returned,&saved,sizeof(saved)),"Simple profile data not copied.\n");
        simple = saved;
        ID2D1ColorContext1_Release(context);
    }
    ID2D1DeviceContext5_Release(dc5);
}

static void test_wic(ID2D1DeviceContext *dc)
{
    static const UINT exif[] = {0,1,2,0xffff,0xffffffff};
    IWICImagingFactory *factory;
    IWICColorContext *wic;
    ID2D1ColorContext *context;
    BYTE invalid[160] = {0};
    unsigned int i;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void **)&factory);
    ok(hr == S_OK,"WIC factory returned %#lx.\n",hr);
    if (FAILED(hr)) return;
    for (i = 0; i < ARRAY_SIZE(exif)+1; ++i)
    {
        hr = IWICImagingFactory_CreateColorContext(factory,&wic);
        ok(hr == S_OK,"WIC CreateColorContext returned %#lx.\n",hr);
        if (FAILED(hr)) continue;
        if (i < ARRAY_SIZE(exif))
        {
            hr = IWICColorContext_InitializeFromExifColorSpace(wic,exif[i]);
            ok(hr == S_OK,"InitializeFromExifColorSpace returned %#lx.\n",hr);
        }
        context = (void *)0xdeadbeef;
        hr = ID2D1DeviceContext_CreateColorContextFromWicColorContext(dc,wic,&context);
        ok(hr == (i < 4 ? S_OK : E_INVALIDARG),"WIC case %u returned %#lx.\n",i,hr);
        IWICColorContext_Release(wic);
        if (i < 4 && SUCCEEDED(hr))
        {
            ok(ID2D1ColorContext_GetColorSpace(context) == (i == 2 ? 0 : 1),"Wrong EXIF space.\n");
            ok(ID2D1ColorContext_GetProfileSize(context) >= 128,"Expected ICC profile.\n");
            ID2D1ColorContext_Release(context);
        }
        else ok(!context,"Failed WIC call did not clear output.\n");
    }
    for (i = 0; i < 2; ++i)
    {
        if (i) { invalid[3] = sizeof(invalid); memcpy(invalid+36,"acsp",4); memcpy(invalid+52,"sRGB",4); }
        context = (void *)0xdeadbeef;
        hr = ID2D1DeviceContext_CreateColorContext(dc,D2D1_COLOR_SPACE_CUSTOM,invalid,sizeof(invalid),&context);
        ok(hr == HRESULT_FROM_WIN32(ERROR_INVALID_PROFILE),"Invalid ICC returned %#lx.\n",hr);
        ok(!context,"Invalid ICC did not clear output.\n");
    }
    IWICImagingFactory_Release(factory);
}

static void test_icc_classification(ID2D1DeviceContext *dc)
{
    static const BYTE scrgb_id[16] = {0xb0,0xd4,0xc7,0x85,0xae,0x7e,0x20,0xd0,
            0xc6,0x43,0x9c,0x12,0xa1,0x08,0x74,0x10};
    IWICImagingFactory *factory;
    IWICColorContext *wic;
    ID2D1ColorContext *source, *context;
    WCHAR temporary[MAX_PATH], filename[MAX_PATH];
    HANDLE file;
    BYTE *profile, *buffer;
    DWORD size, written;
    unsigned int i;
    HRESULT hr;

    hr = ID2D1DeviceContext_CreateColorContext(dc,D2D1_COLOR_SPACE_SRGB,NULL,0,&source);
    ok(hr == S_OK,"CreateColorContext returned %#lx.\n",hr);
    if (FAILED(hr)) return;
    hr = CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void **)&factory);
    ok(hr == S_OK,"WIC factory returned %#lx.\n",hr);
    if (FAILED(hr)) { ID2D1ColorContext_Release(source); return; }
    size = ID2D1ColorContext_GetProfileSize(source);
    profile = malloc(size+16); buffer = malloc(size+16);
    GetTempPathW(ARRAY_SIZE(temporary),temporary);
    GetTempFileNameW(temporary,L"d2c",0,filename);
    for (i = 0; i < 4; ++i)
    {
        D2D1_COLOR_SPACE expected = i == 0 || i == 3 ? D2D1_COLOR_SPACE_SRGB
                : i == 1 ? D2D1_COLOR_SPACE_CUSTOM : D2D1_COLOR_SPACE_SCRGB;
        hr = ID2D1ColorContext_GetProfile(source,profile,size);
        ok(hr == S_OK,"GetProfile returned %#lx.\n",hr);
        memset(profile+84,0,16);
        if (i == 1 || i == 2) memset(profile+52,0,4);
        if (i >= 2) memcpy(profile+84,scrgb_id,sizeof(scrgb_id));
        hr = IWICImagingFactory_CreateColorContext(factory,&wic);
        ok(hr == S_OK,"CreateColorContext returned %#lx.\n",hr);
        hr = IWICColorContext_InitializeFromMemory(wic,profile,size);
        ok(hr == S_OK,"InitializeFromMemory returned %#lx.\n",hr);
        hr = ID2D1DeviceContext_CreateColorContextFromWicColorContext(dc,wic,&context);
        ok(hr == S_OK,"WIC case %u returned %#lx.\n",i,hr);
        IWICColorContext_Release(wic);
        if (SUCCEEDED(hr))
        {
            ok(ID2D1ColorContext_GetColorSpace(context) == expected,"WIC case %u space %u, expected %u.\n",
                    i,ID2D1ColorContext_GetColorSpace(context),expected);
            hr = ID2D1ColorContext_GetProfile(context,buffer,size);
            ok(hr == S_OK && !memcmp(buffer,profile,size),"WIC creation changed profile bytes.\n");
            ID2D1ColorContext_Release(context);
        }
        memset(profile+size,0xcc,16);
        file = CreateFileW(filename,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);
        ok(file != INVALID_HANDLE_VALUE,"CreateFile failed, %lu.\n",GetLastError());
        if (file == INVALID_HANDLE_VALUE) continue;
        ok(WriteFile(file,profile,size+16,&written,NULL) && written == size+16,"WriteFile failed.\n");
        CloseHandle(file);
        hr = ID2D1DeviceContext_CreateColorContextFromFilename(dc,filename,&context);
        ok(hr == S_OK,"Filename case %u returned %#lx.\n",i,hr);
        if (SUCCEEDED(hr))
        {
            ok(ID2D1ColorContext_GetColorSpace(context) == expected,"Filename case %u space %u, expected %u.\n",
                    i,ID2D1ColorContext_GetColorSpace(context),expected);
            ok(ID2D1ColorContext_GetProfileSize(context) == size+16,"File size was not retained.\n");
            hr = ID2D1ColorContext_GetProfile(context,buffer,size+16);
            ok(hr == S_OK && !memcmp(buffer,profile,size+16),"File profile bytes differ.\n");
            ID2D1ColorContext_Release(context);
        }
    }
    DeleteFileW(filename);
    context = (void *)0xdeadbeef;
    hr = ID2D1DeviceContext_CreateColorContextFromFilename(dc,filename,&context);
    ok(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) && !context,"Missing file returned %#lx, %p.\n",hr,context);
    free(profile); free(buffer);
    IWICImagingFactory_Release(factory); ID2D1ColorContext_Release(source);
}

START_TEST(color_context)
{
    ID3D11Device *d3d;
    IDXGIDevice *dxgi;
    ID2D1Factory1 *factory;
    ID2D1Device *device;
    ID2D1DeviceContext *dc;
    HRESULT hr;

    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    hr = D3D11CreateDevice(NULL,effect_test_driver(),NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            NULL,0,D3D11_SDK_VERSION,&d3d,NULL,NULL);
    if (FAILED(hr)) { skip("D3D11 unavailable, %#lx.\n",hr); CoUninitialize(); return; }
    hr = ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi);
    ok(hr == S_OK,"DXGI QI returned %#lx.\n",hr);
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory);
    ok(hr == S_OK,"CreateFactory returned %#lx.\n",hr);
    hr = ID2D1Factory1_CreateDevice(factory,dxgi,&device);
    ok(hr == S_OK,"CreateDevice returned %#lx.\n",hr);
    hr = ID2D1Device_CreateDeviceContext(device,0,&dc);
    ok(hr == S_OK,"CreateDeviceContext returned %#lx.\n",hr);
    if (SUCCEEDED(hr))
    {
        test_profile(dc,factory,D2D1_COLOR_SPACE_SRGB);
        test_profile(dc,factory,D2D1_COLOR_SPACE_SCRGB);
        test_extended(dc);
        test_wic(dc);
        test_icc_classification(dc);
        ID2D1DeviceContext_Release(dc);
    }
    ID2D1Device_Release(device); ID2D1Factory1_Release(factory);
    IDXGIDevice_Release(dxgi); ID3D11Device_Release(d3d); CoUninitialize();
}
