/* DC background preservation through the legacy and device-context interfaces.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <stdio.h>
#include "windows.h"
#include "initguid.h"
#include "d2d1_1.h"
#define REQUIRE(call) do {hr=(call);if(FAILED(hr)){printf("FAILED %u %08lx\n",__LINE__,hr);return 1;}}while(0)

int main(void)
{
    ID2D1Factory1 *factory;
    ID2D1DCRenderTarget *target;
    ID2D1DeviceContext *context;
    ID2D1SolidColorBrush *brush;
    D2D1_RENDER_TARGET_PROPERTIES desc = {D2D1_RENDER_TARGET_TYPE_DEFAULT,
            {DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_IGNORE},96,96,0,0};
    D2D1_COLOR_F red = {1,0,0,1};
    D2D1_RECT_F fill = {1,1,3,3};
    RECT rect = {2,3,10,9};
    BITMAPINFO bmi = {{sizeof(BITMAPINFOHEADER),16,-12,1,32,BI_RGB}};
    HDC hdc;
    HBITMAP bitmap;
    HGDIOBJ previous;
    DWORD *pixels;
    UINT path, alpha, stage, i;
    HRESULT hr;

    setvbuf(stdout,NULL,_IOLBF,0);
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    hdc=CreateCompatibleDC(NULL);
    bitmap=CreateDIBSection(hdc,&bmi,DIB_RGB_COLORS,(void **)&pixels,NULL,0);
    previous=SelectObject(hdc,bitmap);
    for(alpha=0;alpha<2;++alpha)for(path=0;path<2;++path)
    {
        desc.pixelFormat.alphaMode=alpha?D2D1_ALPHA_MODE_PREMULTIPLIED:D2D1_ALPHA_MODE_IGNORE;
        REQUIRE(ID2D1Factory1_CreateDCRenderTarget(factory,&desc,&target));
        REQUIRE(ID2D1DCRenderTarget_QueryInterface(target,&IID_ID2D1DeviceContext,(void **)&context));
        REQUIRE(ID2D1DCRenderTarget_CreateSolidColorBrush(target,&red,NULL,&brush));
        REQUIRE(ID2D1DCRenderTarget_BindDC(target,hdc,&rect));
        for(stage=0;stage<3;++stage)
        {
            GdiFlush();
            for(i=0;i<16*12;++i) pixels[i]=0xff204060+stage*0x102030+i;
            if(path) ID2D1DeviceContext_BeginDraw(context);
            else ID2D1DCRenderTarget_BeginDraw(target);
            if(stage)
            {
                if(path) ID2D1DeviceContext_FillRectangle(context,&fill,(ID2D1Brush *)brush);
                else ID2D1DCRenderTarget_FillRectangle(target,&fill,(ID2D1Brush *)brush);
            }
            if(path) hr=ID2D1DeviceContext_EndDraw(context,NULL,NULL);
            else hr=ID2D1DCRenderTarget_EndDraw(target,NULL,NULL);
            GdiFlush();
            printf("case %u %u %u end %08lx\n",alpha,path,stage,hr);
            for(i=0;i<16*12;++i)printf("pixel %u %08lx\n",i,pixels[i]);
        }
        ID2D1SolidColorBrush_Release(brush);
        ID2D1DeviceContext_Release(context);
        ID2D1DCRenderTarget_Release(target);
    }
    SelectObject(hdc,previous);
    DeleteObject(bitmap);
    DeleteDC(hdc);
    ID2D1Factory1_Release(factory);
    return 0;
}
