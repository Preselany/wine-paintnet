/* Tests for Direct2D alpha conversion, including feature level 10.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <math.h>
#include <string.h>
#include "wine/test.h"
#include "d2d1_1.h"
#include "d2d1effects.h"
#include "d3d11.h"
#include "effect_test.h"
#define REQUIRE(call) do { hr=(call); ok(hr == S_OK,"%s returned %#lx.\n",#call,hr); if(FAILED(hr))return; } while(0)

static void test_conversion(BOOL fl10)
{
    ID3D11Device *d3d;
    IDXGIDevice *dxgi;
    ID2D1Factory1 *factory;
    ID2D1Device *device;
    ID2D1DeviceContext *dc;
    ID2D1Bitmap1 *source, *target, *readback;
    ID2D1Effect *effect, *second;
    ID2D1Image *image, *first;
    D2D1_SIZE_U size = {16,1};
    D2D1_BITMAP_PROPERTIES1 desc = {{DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},
            96,96,D2D1_BITMAP_OPTIONS_NONE,NULL};
    D2D1_COLOR_F clear = {0,0,0,0};
    D2D1_MAPPED_RECT mapped;
    D2D1_RECT_F bounds;
    float pixels[16][4];
    static const float alphas[] = {0,1,.25,.5,-.5,2,.000001f,-.000001f};
    const GUID *ids[] = {&CLSID_D2D1Premultiply,&CLSID_D2D1UnPremultiply};
    UINT mode, operation, chain, i;
    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_10_0;
    HRESULT hr;
    REQUIRE(D3D11CreateDevice(NULL,effect_test_driver(),
            NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,fl10 ? &level : NULL,fl10 ? 1 : 0,D3D11_SDK_VERSION,&d3d,NULL,NULL));
    ok(!fl10 || ID3D11Device_GetFeatureLevel(d3d) == D3D_FEATURE_LEVEL_10_0,"Unexpected feature level.\n");
    REQUIRE(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    REQUIRE(ID2D1Factory1_CreateDevice(factory,dxgi,&device));
    REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&dc));
    for(i=0;i<16;++i)
    {
        pixels[i][0] = i < 8 ? .25f : -.5f;
        pixels[i][1] = i < 8 ? .5f : 1.25f;
        pixels[i][2] = i < 8 ? .75f : -.00001f;
        pixels[i][3] = alphas[i%8];
    }
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,NULL,0,&desc,&target));
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,NULL,0,&desc,&readback));
    ID2D1DeviceContext_SetTarget(dc,(ID2D1Image *)target);
    for (mode=1;mode<=3;++mode)
    {
        desc.bitmapOptions = D2D1_BITMAP_OPTIONS_NONE;
        desc.pixelFormat.alphaMode = mode;
        hr=ID2D1DeviceContext_CreateBitmap(dc,size,pixels,sizeof(pixels),&desc,&source);
        ok(hr == (mode == D2D1_ALPHA_MODE_STRAIGHT ? D2DERR_UNSUPPORTED_PIXEL_FORMAT : S_OK),"Source mode %u returned %#lx.\n",mode,hr);
        if(FAILED(hr))continue;
        for(operation=0;operation<2;++operation)
        for(chain=0;chain<2;++chain)
        {
            winetest_push_context("mode %u, operation %u, chain %u",mode,operation,chain);
            REQUIRE(ID2D1DeviceContext_CreateEffect(dc,ids[operation],&effect));
            ok(ID2D1Effect_GetPropertyCount(effect) == 0,"Unexpected properties.\n");
            ok(ID2D1Effect_GetInputCount(effect) == 1,"Unexpected input count.\n");
            ID2D1Effect_SetInput(effect,0,(ID2D1Image *)source,TRUE);
            ID2D1Effect_GetOutput(effect,&image);
            second=NULL;
            if(chain)
            {
                first=image;
                REQUIRE(ID2D1DeviceContext_CreateEffect(dc,ids[1-operation],&second));
                ID2D1Effect_SetInput(second,0,first,TRUE);
                ID2D1Effect_GetOutput(second,&image);
                ID2D1Image_Release(first);
            }
            hr=ID2D1DeviceContext_GetImageLocalBounds(dc,image,&bounds);
            ok(hr == S_OK,"Bounds returned %#lx.\n",hr);
            ok(bounds.left == 0 && bounds.top == 0 && bounds.right == 16 && bounds.bottom == 1,
                    "Unexpected bounds %.9g %.9g %.9g %.9g.\n",bounds.left,bounds.top,bounds.right,bounds.bottom);
            ID2D1DeviceContext_BeginDraw(dc);
            ID2D1DeviceContext_Clear(dc,&clear);
            ID2D1DeviceContext_DrawImage(dc,image,NULL,NULL,D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,D2D1_COMPOSITE_MODE_SOURCE_OVER);
            hr=ID2D1DeviceContext_EndDraw(dc,NULL,NULL);
            ok(hr == S_OK,"Draw returned %#lx.\n",hr);
            if(SUCCEEDED(hr))
            {
                REQUIRE(ID2D1Bitmap1_CopyFromBitmap(readback,NULL,(ID2D1Bitmap *)target,NULL));
                REQUIRE(ID2D1Bitmap1_Map(readback,D2D1_MAP_OPTIONS_READ,&mapped));
                for(i=0;i<64;++i)
                {
                    float value, alpha=pixels[i/4][3], expected=pixels[i/4][i%4];
                    memcpy(&value,mapped.bits+i*4,4);
                    if(i%4 != 3)
                    {
                        if(!alpha) expected=0;
                        else if(!chain) expected=operation ? expected/alpha : expected*alpha;
                    }
                    ok(isfinite(value) && fabsf(value-expected) <= max(1e-10f,fabsf(expected)*1e-6f),
                            "Channel %u: %.9g, expected %.9g.\n",i,value,expected);
                }
                REQUIRE(ID2D1Bitmap1_Unmap(readback));
            }
            ID2D1Image_Release(image);
            if(second)ID2D1Effect_Release(second);
            ID2D1Effect_Release(effect);
            winetest_pop_context();
        }
        ID2D1Bitmap1_Release(source);
    }
    ID2D1DeviceContext_SetTarget(dc,NULL);
    ID2D1Bitmap1_Release(readback); ID2D1Bitmap1_Release(target);
    ID2D1DeviceContext_Release(dc); ID2D1Device_Release(device); ID2D1Factory1_Release(factory);
    IDXGIDevice_Release(dxgi); ID3D11Device_Release(d3d);

}

START_TEST(alpha_conversion)
{
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    winetest_push_context("feature level default");
    test_conversion(FALSE);
    winetest_pop_context();
    winetest_push_context("feature level 10");
    test_conversion(TRUE);
    winetest_pop_context();
    CoUninitialize();
}
