/* Native bounds, area, and pixels for intersecting geometry combinations.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define main color_context_base_main
#include "color-context-reference.c"
#undef main

int main(void)
{
    ID3D11Device *d3d;
    IDXGIDevice *dxgi;
    ID2D1Factory1 *factory;
    ID2D1Device *device;
    ID2D1DeviceContext *dc;
    ID2D1RectangleGeometry *rectangle, *inner_rectangle;
    ID2D1EllipseGeometry *ellipse;
    ID2D1PathGeometry *path, *result, *primary;
    ID2D1GeometrySink *sink;
    ID2D1Geometry *input, *source;
    ID2D1RoundedRectangleGeometry *rounded;
    ID2D1TransformedGeometry *wrapped;
    const D2D1_ROUNDED_RECT rounded_desc = {{0,0,11,11},3,2};
    const D2D1_MATRIX_3X2_F source_transform = {.m11=.8,.m12=.2,.m21=-.3,.m22=.9,.dx=2,.dy=1};
    ID2D1SolidColorBrush *brush;
    ID2D1Bitmap1 *target,*readback;
    D2D1_SIZE_U size = {12,12};
    D2D1_BITMAP_PROPERTIES1 desc = {{DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},
            96,96,D2D1_BITMAP_OPTIONS_TARGET,NULL};
    const D2D1_RECT_F outer = {0,0,11,11}, inner = {2,2,9,9};
    const D2D1_POINT_2F points[] = {{2,2},{9,2},{9,5},{6,5},{6,9},{2,9}};
    const D2D1_ELLIPSE ellipse_desc = {{5,5},3,2};
    const D2D1_COLOR_F color = {.25,.5,.75,1}, clear = {0};
    const D2D1_MATRIX_3X2_F transform = {.m11=1,.m12=.25,.m21=-.5,.m22=1,.dx=7,.dy=3};
    D2D1_MAPPED_RECT map;
    D2D1_RECT_F bounds;
    float area;
    HRESULT hr, close_hr;
    unsigned int source_index,shape,mode,transformed,x,y;
    setvbuf(stdout,NULL,_IOFBF,65536);
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    REQUIRE(D3D11CreateDevice(NULL,getenv("D2D1_TEST_WARP") ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE,
            NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,NULL,0,D3D11_SDK_VERSION,&d3d,NULL,NULL));
    REQUIRE(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    REQUIRE(ID2D1Factory1_CreateDevice(factory,dxgi,&device));
    REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&dc));
    REQUIRE(ID2D1DeviceContext_CreateSolidColorBrush(dc,&color,NULL,&brush));
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,NULL,0,&desc,&target));
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,NULL,0,&desc,&readback));
    REQUIRE(ID2D1Factory1_CreateRectangleGeometry(factory,&outer,&rectangle));
    REQUIRE(ID2D1Factory1_CreateRectangleGeometry(factory,&inner,&inner_rectangle));
    REQUIRE(ID2D1Factory1_CreateEllipseGeometry(factory,&ellipse_desc,&ellipse));
    REQUIRE(ID2D1Factory1_CreateRoundedRectangleGeometry(factory,&rounded_desc,&rounded));
    REQUIRE(ID2D1Factory1_CreateTransformedGeometry(factory,(ID2D1Geometry *)rounded,&source_transform,&wrapped));
    REQUIRE(ID2D1Factory_CreatePathGeometry((ID2D1Factory *)factory,&path));
    REQUIRE(ID2D1PathGeometry_Open(path,&sink));
    ID2D1GeometrySink_SetFillMode(sink,D2D1_FILL_MODE_ALTERNATE);
    ID2D1GeometrySink_BeginFigure(sink,points[0],D2D1_FIGURE_BEGIN_FILLED);
    ID2D1GeometrySink_AddLines(sink,&points[1],5);
    ID2D1GeometrySink_EndFigure(sink,D2D1_FIGURE_END_CLOSED);
    REQUIRE(ID2D1GeometrySink_Close(sink));
    ID2D1GeometrySink_Release(sink);
    REQUIRE(ID2D1Factory_CreatePathGeometry((ID2D1Factory *)factory,&primary));
    REQUIRE(ID2D1PathGeometry_Open(primary,&sink));
    REQUIRE(ID2D1RectangleGeometry_Simplify(rectangle,D2D1_GEOMETRY_SIMPLIFICATION_OPTION_LINES,NULL,.25f,
            (ID2D1SimplifiedGeometrySink *)sink));
    if(getenv("COMBINE_HOLE"))
    {
        const D2D1_POINT_2F hole[] = {{3,3},{8,3},{8,8},{3,8}};
        ID2D1GeometrySink_BeginFigure(sink,hole[0],D2D1_FIGURE_BEGIN_FILLED);
        ID2D1GeometrySink_AddLines(sink,&hole[1],3);
        ID2D1GeometrySink_EndFigure(sink,D2D1_FIGURE_END_CLOSED);
    }
    REQUIRE(ID2D1GeometrySink_Close(sink));
    ID2D1GeometrySink_Release(sink);
    ID2D1DeviceContext_SetTarget(dc,(ID2D1Image *)target);
    ID2D1DeviceContext_SetAntialiasMode(dc,D2D1_ANTIALIAS_MODE_ALIASED);
    for(source_index=0;source_index<4;++source_index)for(shape=0;shape<3;++shape)for(transformed=0;transformed<2;++transformed)for(mode=0;mode<4;++mode)
    {
        input = shape == 0 ? (ID2D1Geometry *)inner_rectangle : shape == 1 ? (ID2D1Geometry *)path : (ID2D1Geometry *)ellipse;
        REQUIRE(ID2D1Factory_CreatePathGeometry((ID2D1Factory *)factory,&result));
        REQUIRE(ID2D1PathGeometry_Open(result,&sink));
        source = source_index == 0 ? (ID2D1Geometry *)rectangle : source_index == 1 ? (ID2D1Geometry *)ellipse : source_index == 2 ? (ID2D1Geometry *)rounded : (ID2D1Geometry *)wrapped;
        hr=ID2D1Geometry_CombineWithGeometry(source,input,mode,transformed ? &transform : NULL,
                .01f,(ID2D1SimplifiedGeometrySink *)sink);
        printf("case %u %u %u %u\ncombine %08lx\n",source_index,shape,transformed,mode,hr);
        close_hr=ID2D1GeometrySink_Close(sink); printf("close %08lx\n",close_hr);
        ID2D1GeometrySink_Release(sink);
        if (SUCCEEDED(hr) && SUCCEEDED(close_hr))
        {
            hr=ID2D1PathGeometry_GetBounds(result,NULL,&bounds);
            printf("bounds %08lx %.9g %.9g %.9g %.9g\n",hr,bounds.left,bounds.top,bounds.right,bounds.bottom);
            hr=ID2D1PathGeometry_ComputeArea(result,NULL,.01f,&area);printf("area %08lx %.9g\n",hr,area);
            ID2D1DeviceContext_BeginDraw(dc);
            ID2D1DeviceContext_Clear(dc,&clear);
            ID2D1DeviceContext_FillGeometry(dc,(ID2D1Geometry *)result,(ID2D1Brush *)brush,NULL);
            hr=ID2D1DeviceContext_EndDraw(dc,NULL,NULL);printf("draw %08lx\n",hr);
            if(SUCCEEDED(hr))
            {
                REQUIRE(ID2D1Bitmap1_CopyFromBitmap(readback,NULL,(ID2D1Bitmap *)target,NULL));
                REQUIRE(ID2D1Bitmap1_Map(readback,D2D1_MAP_OPTIONS_READ,&map));
                printf("pixels");
                for(y=0;y<size.height;++y)for(x=0;x<size.width*4;++x)
                {float f;memcpy(&f,map.bits+y*map.pitch+x*4,4);printf(" %.9g",f);}
                puts("");
                REQUIRE(ID2D1Bitmap1_Unmap(readback));
            }
        }
        ID2D1PathGeometry_Release(result);
    }
    ID2D1DeviceContext_SetTarget(dc,NULL);
    ID2D1RectangleGeometry_Release(rectangle);ID2D1RectangleGeometry_Release(inner_rectangle);
    ID2D1PathGeometry_Release(primary);
    ID2D1RoundedRectangleGeometry_Release(rounded);ID2D1TransformedGeometry_Release(wrapped);
    ID2D1EllipseGeometry_Release(ellipse);ID2D1PathGeometry_Release(path);
    ID2D1SolidColorBrush_Release(brush);ID2D1Bitmap1_Release(readback);ID2D1Bitmap1_Release(target);
    ID2D1DeviceContext_Release(dc);ID2D1Device_Release(device);ID2D1Factory1_Release(factory);
    IDXGIDevice_Release(dxgi);ID3D11Device_Release(d3d);CoUninitialize();
    puts("done");return 0;
}
