/* Native Crop rectangle, border, bounds and pixel measurements.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define main color_context_base_main
#include "color-context-reference.c"
#undef main
#include "d2d1effects.h"
#include <float.h>
#include <math.h>

int main(void)
{
    ID3D11Device *d3d;
    IDXGIDevice *dxgi;
    ID2D1Factory1 *factory;
    ID2D1Device *device;
    ID2D1DeviceContext *dc;
    ID2D1Bitmap1 *source,*target,*readback;
    ID2D1Effect *effect;
    ID2D1Image *image;
    D2D1_BITMAP_PROPERTIES1 desc={{DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},96,96,0,NULL};
    D2D1_COLOR_F clear={0};
    D2D1_POINT_2F offset={3,3};
    D2D1_RECT_F bounds;
    D2D1_MAPPED_RECT mapped;
    D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_10_0;
    BOOL fl10=getenv("D2D1_TEST_FL10")!=NULL;
    float pixels[3][5][4];
    static const D2D1_RECT_F rectangles[]={
        {-FLT_MAX,-FLT_MAX,FLT_MAX,FLT_MAX},{0,0,5,3},{1,1,4,3},
        {.25f,.25f,4.75f,2.75f},{1.2f,.3f,3.8f,2.7f},{-1,-.5f,2.2f,1.6f},
        {5,0,8,2},{1,1,1,2},{3,2,1,0},{.5f,.5f,4.5f,2.5f},
        {.001f,.001f,4.999f,2.999f},{1.99f,.99f,2.01f,1.01f},
        {-INFINITY,-INFINITY,INFINITY,INFINITY},{NAN,0,5,3},{0,NAN,5,3},
        {0,0,NAN,3},{0,0,5,NAN},{INFINITY,0,INFINITY,3},{-INFINITY,0,-INFINITY,3}};
    unsigned int rect,dpi,unit,border,x,y;
    HRESULT hr;
    setvbuf(stdout,NULL,_IOFBF,65536);
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    REQUIRE(D3D11CreateDevice(NULL,getenv("D2D1_TEST_WARP")?D3D_DRIVER_TYPE_WARP:D3D_DRIVER_TYPE_HARDWARE,
            NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,fl10?&level:NULL,fl10?1:0,D3D11_SDK_VERSION,&d3d,NULL,NULL));
    REQUIRE(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    REQUIRE(ID2D1Factory1_CreateDevice(factory,dxgi,&device));
    REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&dc));
    for(y=0;y<3;++y)for(x=0;x<5;++x)
    {
        pixels[y][x][0]=x*.125f;
        pixels[y][x][1]=y*.25f;
        pixels[y][x][2]=(x==2 && y==1)?1:0;
        pixels[y][x][3]=.5f+((x+y)%2)*.25f;
    }
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,(D2D1_SIZE_U){5,3},pixels,sizeof(pixels[0]),&desc,&source));
    desc.bitmapOptions=D2D1_BITMAP_OPTIONS_TARGET;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,(D2D1_SIZE_U){12,10},NULL,0,&desc,&target));
    desc.bitmapOptions=D2D1_BITMAP_OPTIONS_CPU_READ|D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,(D2D1_SIZE_U){12,10},NULL,0,&desc,&readback));
    REQUIRE(ID2D1DeviceContext_CreateEffect(dc,&CLSID_D2D1Crop,&effect));
    ID2D1Effect_SetInput(effect,0,(ID2D1Image *)source,TRUE);
    ID2D1Effect_GetOutput(effect,&image);
    ID2D1DeviceContext_SetTarget(dc,(ID2D1Image *)target);
    {
        D2D1_VECTOR_4F value;
        UINT v;
        static const UINT enums[]={0,1,2,3,0xffffffff};
        printf("properties %u inputs %u\n",ID2D1Effect_GetPropertyCount(effect),ID2D1Effect_GetInputCount(effect));
        REQUIRE(ID2D1Effect_GetValue(effect,0,D2D1_PROPERTY_TYPE_VECTOR4,(BYTE *)&value,sizeof(value)));
        printf("default_rect %.9g %.9g %.9g %.9g\n",value.x,value.y,value.z,value.w);
        REQUIRE(ID2D1Effect_GetValue(effect,1,D2D1_PROPERTY_TYPE_ENUM,(BYTE *)&v,sizeof(v)));
        printf("default_border %u\n",v);
        for(x=0;x<ARRAY_SIZE(enums);++x)
        {
            v=1;ID2D1Effect_SetValue(effect,1,D2D1_PROPERTY_TYPE_ENUM,(BYTE *)&v,4);
            hr=ID2D1Effect_SetValue(effect,1,D2D1_PROPERTY_TYPE_ENUM,(BYTE *)&enums[x],4);
            ID2D1Effect_GetValue(effect,1,D2D1_PROPERTY_TYPE_ENUM,(BYTE *)&v,4);
            printf("enum %u %08lx %u\n",enums[x],hr,v);
        }
    }
    for(dpi=96;dpi<=96;dpi+=48)for(unit=0;unit<2;++unit)for(rect=0;rect<ARRAY_SIZE(rectangles);++rect)for(border=0;border<2;++border)
    {
        printf("case %u %u %u %u\n",dpi,unit,rect,border);
        ID2D1DeviceContext_SetDpi(dc,dpi,dpi);ID2D1DeviceContext_SetUnitMode(dc,unit);
        hr=ID2D1Effect_SetValue(effect,D2D1_CROP_PROP_RECT,D2D1_PROPERTY_TYPE_VECTOR4,(BYTE *)&rectangles[rect],sizeof(D2D1_RECT_F));
        printf("set %08lx\n",hr);
        ID2D1Effect_GetValue(effect,0,D2D1_PROPERTY_TYPE_VECTOR4,(BYTE *)&bounds,sizeof(bounds));
        printf("get %.9g %.9g %.9g %.9g\n",bounds.left,bounds.top,bounds.right,bounds.bottom);
        REQUIRE(ID2D1Effect_SetValue(effect,D2D1_CROP_PROP_BORDER_MODE,D2D1_PROPERTY_TYPE_ENUM,(BYTE *)&border,sizeof(border)));
        bounds=(D2D1_RECT_F){-99,-99,-99,-99};
        hr=ID2D1DeviceContext_GetImageLocalBounds(dc,image,&bounds);
        printf("bounds %08lx %.9g %.9g %.9g %.9g\n",hr,bounds.left,bounds.top,bounds.right,bounds.bottom);
        ID2D1DeviceContext_BeginDraw(dc);ID2D1DeviceContext_Clear(dc,&clear);
        ID2D1DeviceContext_DrawImage(dc,image,&offset,NULL,D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,D2D1_COMPOSITE_MODE_SOURCE_OVER);
        hr=ID2D1DeviceContext_EndDraw(dc,NULL,NULL);printf("draw %08lx\n",hr);
        if(SUCCEEDED(hr))
        {
            REQUIRE(ID2D1Bitmap1_CopyFromBitmap(readback,NULL,(ID2D1Bitmap *)target,NULL));
            REQUIRE(ID2D1Bitmap1_Map(readback,D2D1_MAP_OPTIONS_READ,&mapped));printf("pixels");
            for(y=0;y<10;++y)for(x=0;x<48;++x){float v;memcpy(&v,mapped.bits+y*mapped.pitch+x*4,4);printf(" %.9g",v);}
            puts("");REQUIRE(ID2D1Bitmap1_Unmap(readback));
        }
        fflush(stdout);
    }
    ID2D1DeviceContext_SetTarget(dc,NULL);ID2D1Image_Release(image);ID2D1Effect_Release(effect);
    ID2D1Bitmap1_Release(readback);ID2D1Bitmap1_Release(target);ID2D1Bitmap1_Release(source);
    ID2D1DeviceContext_Release(dc);ID2D1Device_Release(device);ID2D1Factory1_Release(factory);
    IDXGIDevice_Release(dxgi);ID3D11Device_Release(d3d);CoUninitialize();puts("done");return 0;
}
