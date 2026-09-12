/* Native Flood properties, infinite bounds, and pixel output.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define main color_context_base_main
#include "color-context-reference.c"
#undef main
#include "d2d1effects.h"
#include <math.h>
int main(void)
{
    ID3D11Device *d3d; IDXGIDevice *dxgi; ID2D1Factory1 *factory; ID2D1Device *device; ID2D1DeviceContext *dc;
    ID2D1Bitmap1 *target,*readback; ID2D1Effect *flood,*crop; ID2D1Image *image,*flood_image;
    D2D1_BITMAP_PROPERTIES1 desc={{DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},96,96,D2D1_BITMAP_OPTIONS_TARGET,NULL};
    D2D1_RECT_F rect={1,1,4,3},bounds; D2D1_MAPPED_RECT map;
    D2D1_POINT_2F offset={2,2}; D2D1_COLOR_F clear={.1f,.2f,.3f,.5f}; D2D1_VECTOR_4F value;
    static const D2D1_VECTOR_4F colors[]={{.2f,.4f,.8f,.5f},{.2f,.4f,.8f,0},{0,0,0,0},{0,0,0,1},{2,-.25f,.1f,2},{-.25f,.5f,2,-.5f},{NAN,.5f,.25f,1},{.25f,NAN,.5f,1},{.25f,.5f,1,NAN},{INFINITY,-INFINITY,.25f,1}};
    UINT color,dpi,unit,path,mode,x,y; HRESULT hr;
    setvbuf(stdout,NULL,_IOFBF,65536);CoInitializeEx(NULL,COINIT_MULTITHREADED);
    REQUIRE(D3D11CreateDevice(NULL,getenv("D2D1_TEST_WARP")?D3D_DRIVER_TYPE_WARP:D3D_DRIVER_TYPE_HARDWARE,NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,NULL,0,D3D11_SDK_VERSION,&d3d,NULL,NULL));
    REQUIRE(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    REQUIRE(ID2D1Factory1_CreateDevice(factory,dxgi,&device));REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&dc));
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,(D2D1_SIZE_U){8,6},NULL,0,&desc,&target));
    desc.bitmapOptions=D2D1_BITMAP_OPTIONS_CPU_READ|D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,(D2D1_SIZE_U){8,6},NULL,0,&desc,&readback));
    REQUIRE(ID2D1DeviceContext_CreateEffect(dc,&CLSID_D2D1Flood,&flood));REQUIRE(ID2D1DeviceContext_CreateEffect(dc,&CLSID_D2D1Crop,&crop));
    ID2D1Effect_GetOutput(flood,&flood_image);ID2D1Effect_SetInput(crop,0,flood_image,TRUE);
    REQUIRE(ID2D1Effect_SetValue(crop,D2D1_CROP_PROP_RECT,D2D1_PROPERTY_TYPE_VECTOR4,(BYTE *)&rect,sizeof(rect)));
    REQUIRE(ID2D1Effect_GetValue(flood,0,D2D1_PROPERTY_TYPE_VECTOR4,(BYTE *)&value,sizeof(value)));
    printf("default %.9g %.9g %.9g %.9g inputs %u properties %u\n",value.x,value.y,value.z,value.w,ID2D1Effect_GetInputCount(flood),ID2D1Effect_GetPropertyCount(flood));
    ID2D1DeviceContext_SetTarget(dc,(ID2D1Image *)target);
    for(color=0;color<ARRAY_SIZE(colors);++color)for(dpi=96;dpi<=192;dpi+=48)for(unit=0;unit<2;++unit)for(path=0;path<2;++path)for(mode=0;mode<2;++mode)
    {
        printf("case %u %u %u %u %u\n",color,dpi,unit,path,mode);
        hr=ID2D1Effect_SetValue(flood,0,D2D1_PROPERTY_TYPE_VECTOR4,(BYTE *)&colors[color],sizeof(value));printf("set %08lx\n",hr);
        REQUIRE(ID2D1Effect_GetValue(flood,0,D2D1_PROPERTY_TYPE_VECTOR4,(BYTE *)&value,sizeof(value)));printf("get %.9g %.9g %.9g %.9g\n",value.x,value.y,value.z,value.w);
        ID2D1DeviceContext_SetDpi(dc,dpi,dpi);ID2D1DeviceContext_SetUnitMode(dc,unit);
        ID2D1Effect_GetOutput(path?crop:flood,&image);bounds=(D2D1_RECT_F){-99,-99,-99,-99};
        hr=ID2D1DeviceContext_GetImageLocalBounds(dc,image,&bounds);printf("bounds %08lx %.9g %.9g %.9g %.9g\n",hr,bounds.left,bounds.top,bounds.right,bounds.bottom);
        ID2D1DeviceContext_BeginDraw(dc);ID2D1DeviceContext_Clear(dc,&clear);
        ID2D1DeviceContext_DrawImage(dc,image,&offset,NULL,D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,mode?D2D1_COMPOSITE_MODE_SOURCE_COPY:D2D1_COMPOSITE_MODE_SOURCE_OVER);
        hr=ID2D1DeviceContext_EndDraw(dc,NULL,NULL);printf("draw %08lx\n",hr);
        if(SUCCEEDED(hr))
        {
            REQUIRE(ID2D1Bitmap1_CopyFromBitmap(readback,NULL,(ID2D1Bitmap *)target,NULL));REQUIRE(ID2D1Bitmap1_Map(readback,D2D1_MAP_OPTIONS_READ,&map));
            printf("pixels");for(y=0;y<6;++y)for(x=0;x<32;++x){float v;memcpy(&v,map.bits+y*map.pitch+x*4,4);printf(" %.9g",v);}puts("");REQUIRE(ID2D1Bitmap1_Unmap(readback));
        }
        ID2D1Image_Release(image);fflush(stdout);
    }
    ID2D1DeviceContext_SetTarget(dc,NULL);ID2D1Image_Release(flood_image);ID2D1Effect_Release(crop);ID2D1Effect_Release(flood);
    ID2D1Bitmap1_Release(readback);ID2D1Bitmap1_Release(target);ID2D1DeviceContext_Release(dc);ID2D1Device_Release(device);ID2D1Factory1_Release(factory);IDXGIDevice_Release(dxgi);ID3D11Device_Release(d3d);CoUninitialize();puts("done");return 0;
}
