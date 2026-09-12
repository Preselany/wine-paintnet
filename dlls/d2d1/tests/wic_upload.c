/* High precision WIC bitmap uploads.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <math.h>
#include "d2d1_1.h"
#include "d3d11.h"
#include "wincodec.h"
#include "wine/test.h"

#define REQUIRE(call) do {hr=(call);ok(hr==S_OK,"%s returned %#lx.\n",#call,hr);if(FAILED(hr))return;}while(0)

static void test_upload(D3D_FEATURE_LEVEL level)
{
    static const struct
    {
        const GUID *wic;
        UINT bytes;
        DXGI_FORMAT dxgi;
        D2D1_ALPHA_MODE alpha;
    }
    formats[] =
    {
        {&GUID_WICPixelFormat64bppPRGBAHalf,8,DXGI_FORMAT_R16G16B16A16_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},
        {&GUID_WICPixelFormat64bppRGBAHalf,8,DXGI_FORMAT_R16G16B16A16_FLOAT,D2D1_ALPHA_MODE_STRAIGHT},
        {&GUID_WICPixelFormat48bppRGBHalf,6,DXGI_FORMAT_UNKNOWN,D2D1_ALPHA_MODE_UNKNOWN},
        {&GUID_WICPixelFormat16bppGrayHalf,2,DXGI_FORMAT_UNKNOWN,D2D1_ALPHA_MODE_UNKNOWN},
        {&GUID_WICPixelFormat128bppPRGBAFloat,16,DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},
        {&GUID_WICPixelFormat128bppRGBAFloat,16,DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_STRAIGHT},
        {&GUID_WICPixelFormat64bppPRGBA,8,DXGI_FORMAT_R16G16B16A16_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED},
        {&GUID_WICPixelFormat64bppRGBA,8,DXGI_FORMAT_R16G16B16A16_UNORM,D2D1_ALPHA_MODE_STRAIGHT},
        {&GUID_WICPixelFormat32bppPBGRA,4,DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED},
        {&GUID_WICPixelFormat32bppPRGBA,4,DXGI_FORMAT_R8G8B8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED},
        {&GUID_WICPixelFormat32bppBGR,4,DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_IGNORE},
        {&GUID_WICPixelFormat32bppRGB,4,DXGI_FORMAT_R8G8B8A8_UNORM,D2D1_ALPHA_MODE_IGNORE},
    };
    static const WORD half[] = {0x3000,0x3400,0x3800,0x3a00,0xbc00,0x4000,0x0400,0x3c00,
            0x0001,0x8000,0x3c00,0x3800};
    static const float half_values[] = {.125f,.25f,.5f,.75f,-1,2,0.00006103515625f,1,
            0.000000059604644775390625f,0,1,.5f};
    static const WORD unorm[] = {0x2000,0x4000,0x8000,0xc000,0,0xffff,1,0xffff,0x4000,0,0x8000,0x8000};
    static const BYTE unorm8[] = {32,64,128,192,0,255,1,255,64,0,128,128};
    ID3D11Device *d3d;
    IDXGIDevice *dxgi;
    ID2D1Factory1 *factory;
    ID2D1Device *device;
    ID2D1DeviceContext *dc;
    IWICImagingFactory *wic;
    IWICBitmap *source;
    IWICBitmapLock *lock;
    ID2D1Bitmap1 *bitmap, *readback, *target, *float_readback;
    D2D1_SIZE_U size = {3,2};
    D2D1_PIXEL_FORMAT actual;
    D2D1_BITMAP_PROPERTIES1 desc = {{DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},
            96,96,D2D1_BITMAP_OPTIONS_TARGET,NULL}, read_desc;
    D2D1_MAPPED_RECT mapped;
    D2D1_COLOR_F clear = {0,0,0,0};
    D2D1_RECT_F dest = {0,0,3,2};
    BYTE data[128], *locked;
    float expected[24], value, xdpi, ydpi;
    UINT f, mode, api, i, y, stride, count;
    D2D1_ALPHA_MODE alpha;
    DXGI_FORMAT format;
    HRESULT hr, expected_hr;

    hr = D3D11CreateDevice(NULL, getenv("D2D1_TEST_WARP") ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE,
            NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,&level,1,D3D11_SDK_VERSION,&d3d,NULL,NULL);
    if (FAILED(hr)) {win_skip("Feature level %#x unavailable, hr %#lx.\n",level,hr);return;}
    REQUIRE(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    REQUIRE(ID2D1Factory1_CreateDevice(factory,dxgi,&device));
    REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&dc));
    REQUIRE(CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void **)&wic));
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,NULL,0,&desc,&target));
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,NULL,0,&desc,&float_readback));
    ID2D1DeviceContext_SetTarget(dc,(ID2D1Image *)target);

    for (f=0; f<ARRAY_SIZE(formats); ++f)
    {
        stride = 3*formats[f].bytes+4;
        memset(data,0xcd,sizeof(data));
        for (y=0; y<2; ++y)
        {
            if (f<4) memcpy(data+y*stride,half,3*formats[f].bytes);
            else if (f<6) memcpy(data+y*stride,half_values,sizeof(half_values));
            else if (f<8) memcpy(data+y*stride,unorm,sizeof(unorm));
            else memcpy(data+y*stride,unorm8,sizeof(unorm8));
            for (i=0; i<12; ++i)
            {
                if (f<6) expected[y*12+i] = half_values[i];
                else if (f<8) expected[y*12+i] = unorm[i]/65535.0f;
                else expected[y*12+i] = unorm8[(f==8 || f==10) && i%4!=1 && i%4!=3 ? i^2 : i]/255.0f;
            }
        }
        for (api=0; api<2; ++api)
        for (mode=0; mode<13; ++mode)
        {
            winetest_push_context("level %#x format %u API %u properties %u",level,f,api,mode);
            REQUIRE(IWICImagingFactory_CreateBitmapFromMemory(wic,3,2,formats[f].wic,stride,2*stride,data,&source));
            REQUIRE(IWICBitmap_SetResolution(source,144,192));
            memset(&desc,0,sizeof(desc));
            if (mode>=2 && mode<=5) {desc.pixelFormat.format=formats[f].dxgi;desc.pixelFormat.alphaMode=mode-2;}
            if (mode==6) {desc.pixelFormat.format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.pixelFormat.alphaMode=D2D1_ALPHA_MODE_PREMULTIPLIED;}
            if (mode==7) {desc.pixelFormat.format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.pixelFormat.alphaMode=D2D1_ALPHA_MODE_PREMULTIPLIED;}
            if (mode==8) {desc.pixelFormat.format=DXGI_FORMAT_R16G16B16A16_FLOAT;desc.pixelFormat.alphaMode=D2D1_ALPHA_MODE_PREMULTIPLIED;}
            if (mode==9) {desc.pixelFormat.alphaMode=D2D1_ALPHA_MODE_IGNORE;desc.dpiX=120;desc.dpiY=160;}
            if (mode==10) {desc.pixelFormat.format=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;desc.pixelFormat.alphaMode=D2D1_ALPHA_MODE_PREMULTIPLIED;}
            if (mode==11) {desc.pixelFormat.format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;desc.pixelFormat.alphaMode=D2D1_ALPHA_MODE_PREMULTIPLIED;}
            if (mode==12) {desc.pixelFormat.format=DXGI_FORMAT_B8G8R8X8_UNORM;desc.pixelFormat.alphaMode=D2D1_ALPHA_MODE_IGNORE;}
            alpha = desc.pixelFormat.alphaMode ? desc.pixelFormat.alphaMode : formats[f].alpha;
            format = desc.pixelFormat.format ? desc.pixelFormat.format : formats[f].dxgi;
            expected_hr = S_OK;
            if (f==2 || f==3) expected_hr = D2DERR_UNSUPPORTED_PIXEL_FORMAT;
            else if (formats[f].alpha==D2D1_ALPHA_MODE_IGNORE && alpha!=D2D1_ALPHA_MODE_IGNORE)
                expected_hr = D2DERR_UNSUPPORTED_PIXEL_FORMAT;
            else if (format!=formats[f].dxgi && !(f==8 && mode==10) && !(f==9 && mode==11))
                expected_hr = E_INVALIDARG;
            else if (alpha==D2D1_ALPHA_MODE_STRAIGHT) expected_hr = D2DERR_UNSUPPORTED_PIXEL_FORMAT;

            bitmap = NULL;
            if (api) hr = ID2D1RenderTarget_CreateBitmapFromWicBitmap((ID2D1RenderTarget *)dc,(IWICBitmapSource *)source,
                    mode ? (D2D1_BITMAP_PROPERTIES *)&desc : NULL,(ID2D1Bitmap **)&bitmap);
            else hr = ID2D1DeviceContext_CreateBitmapFromWicBitmap(dc,(IWICBitmapSource *)source,mode ? &desc : NULL,&bitmap);
            ok(hr==expected_hr,"Create returned %#lx, expected %#lx.\n",hr,expected_hr);
            if (FAILED(hr)) {IWICBitmap_Release(source);winetest_pop_context();continue;}

            /* Upload owns its pixels even if the original WIC bitmap changes. */
            REQUIRE(IWICBitmap_Lock(source,NULL,WICBitmapLockWrite,&lock));
            REQUIRE(IWICBitmapLock_GetDataPointer(lock,&count,&locked));
            memset(locked,0,count);
            IWICBitmapLock_Release(lock);
            IWICBitmap_Release(source);
            actual = ID2D1Bitmap1_GetPixelFormat(bitmap);
            ok(actual.format==format && actual.alphaMode==alpha,"Got format %u alpha %u.\n",actual.format,actual.alphaMode);
            ID2D1Bitmap1_GetDpi(bitmap,&xdpi,&ydpi);
            ok(xdpi==(mode==9?120:96) && ydpi==(mode==9?160:96),"Got DPI %g, %g.\n",xdpi,ydpi);
            read_desc=desc;read_desc.pixelFormat=actual;read_desc.dpiX=read_desc.dpiY=96;
            read_desc.bitmapOptions=D2D1_BITMAP_OPTIONS_CPU_READ|D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
            REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,NULL,0,&read_desc,&readback));
            REQUIRE(ID2D1Bitmap1_CopyFromBitmap(readback,NULL,(ID2D1Bitmap *)bitmap,NULL));
            REQUIRE(ID2D1Bitmap1_Map(readback,D2D1_MAP_OPTIONS_READ,&mapped));
            for (y=0; y<2; ++y)
                ok(!memcmp(data+y*stride,mapped.bits+y*mapped.pitch,3*formats[f].bytes),"Raw row %u changed.\n",y);
            REQUIRE(ID2D1Bitmap1_Unmap(readback));
            ID2D1Bitmap1_Release(readback);
            ID2D1DeviceContext_BeginDraw(dc);
            ID2D1DeviceContext_Clear(dc,&clear);
            ID2D1DeviceContext_DrawBitmap(dc,(ID2D1Bitmap *)bitmap,&dest,1,D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,NULL,NULL);
            REQUIRE(ID2D1DeviceContext_EndDraw(dc,NULL,NULL));
            REQUIRE(ID2D1Bitmap1_CopyFromBitmap(float_readback,NULL,(ID2D1Bitmap *)target,NULL));
            REQUIRE(ID2D1Bitmap1_Map(float_readback,D2D1_MAP_OPTIONS_READ,&mapped));
            for (y=0; y<2; ++y)
            for (i=0; i<12; ++i)
            {
                float want = i%4==3 && alpha==D2D1_ALPHA_MODE_IGNORE ? 1 : expected[y*12+i];
                if (i%4!=3 && (mode==10 || mode==11))
                    want = want<=0.04045f ? want/12.92f : powf((want+0.055f)/1.055f,2.4f);
                memcpy(&value,mapped.bits+y*mapped.pitch+i*4,4);
                /* sRGB texture decoding can use an approximate GPU conversion.
                 * The uploaded bytes above must still match exactly. */
                ok(fabsf(value-want)<=(mode==10 || mode==11 ? 0.001f : 0.000001f),"Pixel %u component %u: %.9g expected %.9g.\n",y*3+i/4,i%4,value,want);
            }
            REQUIRE(ID2D1Bitmap1_Unmap(float_readback));
            ID2D1Bitmap1_Release(bitmap);
            winetest_pop_context();
        }
    }
    ID2D1DeviceContext_SetTarget(dc,NULL);
    ID2D1Bitmap1_Release(target);ID2D1Bitmap1_Release(float_readback);
    IWICImagingFactory_Release(wic);ID2D1DeviceContext_Release(dc);ID2D1Device_Release(device);
    ID2D1Factory1_Release(factory);IDXGIDevice_Release(dxgi);ID3D11Device_Release(d3d);
}

START_TEST(wic_upload)
{
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    test_upload(D3D_FEATURE_LEVEL_11_0);
    test_upload(D3D_FEATURE_LEVEL_10_0);
    CoUninitialize();
}
