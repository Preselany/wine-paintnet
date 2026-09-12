/* Public Direct2D white-level adjustment measurements.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define main color_context_base_main
#include "color-context-reference.c"
#undef main
#include "d2d1effects.h"
#include <math.h>
static const GUID white_level_id = {0x44a1cadb,0x6cdd,0x4818,{0x8f,0xf4,0x26,0xc1,0xcf,0xe9,0x5b,0xdb}};

int main(void)
{
    ID3D11Device *d3d;
    IDXGIDevice *dxgi;
    ID2D1Factory1 *factory;
    ID2D1Device *device;
    ID2D1DeviceContext *dc;
    ID2D1Bitmap1 *source, *target, *readback;
    ID2D1Effect *effect;
    ID2D1Properties *sub;
    ID2D1Image *image;
    D2D1_SIZE_U size = {16,1};
    D2D1_BITMAP_PROPERTIES1 desc = {{DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},
            96,96,D2D1_BITMAP_OPTIONS_NONE,NULL};
    D2D1_COLOR_F clear = {0,0,0,0};
    D2D1_MAPPED_RECT mapped;
    D2D1_RECT_F bounds;
    float pixels[16][4];
    static const float alphas[] = {0,1,.25,.5,-.5,2,.000001f,-.000001f};
    static const float levels[][2] = {{80,80},{80,200},{200,80},{1,1},{0,80},{80,0},{-1,80},{80,-1},{10000,1},{1,10000}};
    static const float values[] = {0,-1,1,80,10000,INFINITY,-INFINITY,NAN};
    float value;
    UINT prop,j;
    UINT mode, operation, i;
    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_10_0;
    BOOL fl10 = getenv("D2D1_TEST_FL10") != NULL;
    HRESULT hr;
    setvbuf(stdout,NULL,_IOFBF,65536);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    REQUIRE(D3D11CreateDevice(NULL,getenv("D2D1_TEST_WARP") ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE,
            NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,fl10 ? &level : NULL,fl10 ? 1 : 0,D3D11_SDK_VERSION,&d3d,NULL,NULL));
    printf("feature_level %x\n",ID3D11Device_GetFeatureLevel(d3d));
    REQUIRE(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    REQUIRE(ID2D1Factory1_CreateDevice(factory,dxgi,&device));
    REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&dc));
    REQUIRE(ID2D1DeviceContext_CreateEffect(dc,&white_level_id,&effect));
    printf("properties %u\n",ID2D1Effect_GetPropertyCount(effect));
    for(prop=0;prop<2;++prop)
    {
        REQUIRE(ID2D1Effect_GetValue(effect,prop,D2D1_PROPERTY_TYPE_FLOAT,(BYTE *)&value,sizeof(value)));
        printf("default %u %.9g\n",prop,value);
        REQUIRE(ID2D1Effect_GetSubProperties(effect,prop,&sub));
        for(j=D2D1_SUBPROPERTY_MIN;j<=D2D1_SUBPROPERTY_DEFAULT;++j)
        {
            value=123;
            hr=ID2D1Properties_GetValue(sub,j,D2D1_PROPERTY_TYPE_FLOAT,(BYTE *)&value,sizeof(value));
            printf("sub %u %x %08lx %.9g\n",prop,j,hr,value);
        }
        ID2D1Properties_Release(sub);
        for(j=0;j<sizeof(values)/sizeof(values[0]);++j)
        {
            hr=ID2D1Effect_SetValue(effect,prop,D2D1_PROPERTY_TYPE_FLOAT,(const BYTE *)&values[j],sizeof(float));
            REQUIRE(ID2D1Effect_GetValue(effect,prop,D2D1_PROPERTY_TYPE_FLOAT,(BYTE *)&value,sizeof(value)));
            printf("set %u %u %08lx %.9g\n",prop,j,hr,value);
        }
    }
    ID2D1Effect_Release(effect);
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
        printf("source %u %08lx\n",mode,hr);
        if(FAILED(hr))continue;
        for(operation=0;operation<sizeof(levels)/sizeof(levels[0]);++operation)
        {
            printf("case %u %u %.9g %.9g\n",mode,operation,levels[operation][0],levels[operation][1]);
            REQUIRE(ID2D1DeviceContext_CreateEffect(dc,&white_level_id,&effect));
            for(prop=0;prop<2;++prop)
            {
                hr=ID2D1Effect_SetValue(effect,prop,D2D1_PROPERTY_TYPE_FLOAT,(const BYTE *)&levels[operation][prop],sizeof(float));
                printf("level %u %08lx\n",prop,hr);
            }
            printf("properties %u\n",ID2D1Effect_GetPropertyCount(effect));
            ID2D1Effect_SetInput(effect,0,(ID2D1Image *)source,TRUE);
            ID2D1Effect_GetOutput(effect,&image);
            hr=ID2D1DeviceContext_GetImageLocalBounds(dc,image,&bounds);
            printf("bounds %08lx %.9g %.9g %.9g %.9g\n",hr,bounds.left,bounds.top,bounds.right,bounds.bottom);
            ID2D1DeviceContext_BeginDraw(dc);
            ID2D1DeviceContext_Clear(dc,&clear);
            ID2D1DeviceContext_DrawImage(dc,image,NULL,NULL,D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,D2D1_COMPOSITE_MODE_SOURCE_OVER);
            hr=ID2D1DeviceContext_EndDraw(dc,NULL,NULL);
            printf("draw %08lx\n",hr);
            if(SUCCEEDED(hr))
            {
                REQUIRE(ID2D1Bitmap1_CopyFromBitmap(readback,NULL,(ID2D1Bitmap *)target,NULL));
                REQUIRE(ID2D1Bitmap1_Map(readback,D2D1_MAP_OPTIONS_READ,&mapped));
                printf("pixels");
                for(i=0;i<64;++i){float value;memcpy(&value,mapped.bits+i*4,4);printf(" %.9g",value);}
                puts("");
                REQUIRE(ID2D1Bitmap1_Unmap(readback));
            }
            ID2D1Image_Release(image);
            ID2D1Effect_Release(effect);
        }
        ID2D1Bitmap1_Release(source);
    }
    ID2D1DeviceContext_SetTarget(dc,NULL);
    ID2D1Bitmap1_Release(readback); ID2D1Bitmap1_Release(target);
    ID2D1DeviceContext_Release(dc); ID2D1Device_Release(device); ID2D1Factory1_Release(factory);
    IDXGIDevice_Release(dxgi); ID3D11Device_Release(d3d); CoUninitialize();
    puts("done");
    return 0;
}
