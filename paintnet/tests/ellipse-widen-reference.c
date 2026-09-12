/* Native transformed ellipse stroke bounds, area, contour structure, and pixels.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define main color_context_base_main
#include "color-context-reference.c"
#undef main

static HRESULT STDMETHODCALLTYPE dump_qi(ID2D1SimplifiedGeometrySink *iface,REFIID iid,void **out)
{(void)iface;(void)iid;*out=NULL;return E_NOINTERFACE;}
static ULONG STDMETHODCALLTYPE dump_ref(ID2D1SimplifiedGeometrySink *iface){(void)iface;return 1;}
static void STDMETHODCALLTYPE dump_fill(ID2D1SimplifiedGeometrySink *iface,D2D1_FILL_MODE mode)
{(void)iface;printf("contour fill %u\n",mode);}
static void STDMETHODCALLTYPE dump_flags(ID2D1SimplifiedGeometrySink *iface,D2D1_PATH_SEGMENT flags)
{(void)iface;printf("contour flags %u\n",flags);}
static void STDMETHODCALLTYPE dump_begin(ID2D1SimplifiedGeometrySink *iface,D2D1_POINT_2F p,D2D1_FIGURE_BEGIN begin)
{(void)iface;printf("contour begin %.9g %.9g %u\n",p.x,p.y,begin);}
static void STDMETHODCALLTYPE dump_lines(ID2D1SimplifiedGeometrySink *iface,const D2D1_POINT_2F *p,UINT n)
{UINT i;(void)iface;for(i=0;i<n;++i)printf("contour line %.9g %.9g\n",p[i].x,p[i].y);}
static void STDMETHODCALLTYPE dump_beziers(ID2D1SimplifiedGeometrySink *iface,const D2D1_BEZIER_SEGMENT *p,UINT n)
{UINT i;(void)iface;for(i=0;i<n;++i)printf("contour bezier %.9g %.9g %.9g %.9g %.9g %.9g\n",
 p[i].point1.x,p[i].point1.y,p[i].point2.x,p[i].point2.y,p[i].point3.x,p[i].point3.y);}
static void STDMETHODCALLTYPE dump_end(ID2D1SimplifiedGeometrySink *iface,D2D1_FIGURE_END end)
{(void)iface;printf("contour end %u\n",end);}
static HRESULT STDMETHODCALLTYPE dump_close(ID2D1SimplifiedGeometrySink *iface){(void)iface;return S_OK;}
static const ID2D1SimplifiedGeometrySinkVtbl dump_vtbl={dump_qi,dump_ref,dump_ref,dump_fill,dump_flags,
 dump_begin,dump_lines,dump_beziers,dump_end,dump_close};
static ID2D1SimplifiedGeometrySink dump_sink={&dump_vtbl};

int main(void)
{
    ID3D11Device *d3d;
    IDXGIDevice *dxgi;
    ID2D1Factory1 *factory;
    ID2D1Device *device;
    ID2D1DeviceContext *dc;
    ID2D1EllipseGeometry *ellipse;
    ID2D1PathGeometry *result;
    ID2D1GeometrySink *sink;
    ID2D1Geometry *input;
    ID2D1TransformedGeometry *transformed_input;
    ID2D1StrokeStyle1 *style;
    D2D1_STROKE_STYLE_PROPERTIES1 style_desc = {0};
    ID2D1SolidColorBrush *brush;
    ID2D1Bitmap1 *target,*readback;
    D2D1_SIZE_U size = {32,32};
    D2D1_BITMAP_PROPERTIES1 desc = {{DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},
            96,96,D2D1_BITMAP_OPTIONS_TARGET,NULL};
    const float widths[] = {0,1,2.5,16};
    const D2D1_ELLIPSE ellipse_desc = {{0,0},.5,.5};
    const D2D1_MATRIX_3X2_F stored[] = {{.m11=12,.m22=12,.dx=16,.dy=16},
            {.m11=18,.m22=8,.dx=16,.dy=16}, {.m11=8,.m12=4,.m21=-2,.m22=6,.dx=16,.dy=16},
            {.m11=1,.m22=1,.dx=16,.dy=16}};
    const D2D1_COLOR_F color = {.25,.5,.75,1}, clear = {0};
    const D2D1_MATRIX_3X2_F transform = {.m11=.5,.m22=.5,.dx=2,.dy=1};
    D2D1_MAPPED_RECT map;
    D2D1_RECT_F bounds;
    D2D1_MATRIX_3X2_F stored_transform;
    float area;
    HRESULT hr, close_hr;
    unsigned int shape,mode,transformed,x,y;
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
    REQUIRE(ID2D1Factory1_CreateEllipseGeometry(factory,&ellipse_desc,&ellipse));
    ID2D1DeviceContext_SetTarget(dc,(ID2D1Image *)target);
    ID2D1DeviceContext_SetAntialiasMode(dc,D2D1_ANTIALIAS_MODE_ALIASED);
    style_desc.lineJoin = D2D1_LINE_JOIN_ROUND;
    style_desc.startCap = style_desc.endCap = D2D1_CAP_STYLE_ROUND;
    style_desc.miterLimit = 10;
    REQUIRE(ID2D1Factory1_CreateStrokeStyle(factory,&style_desc,NULL,0,&style));
    for(shape=0;shape<ARRAY_SIZE(stored);++shape)for(transformed=0;transformed<2;++transformed)for(mode=0;mode<4;++mode)
    {
        stored_transform = stored[shape];
        if (getenv("ELLIPSE_REFLECT")) {stored_transform.m11 = -stored_transform.m11; stored_transform.m12 = -stored_transform.m12;}
        REQUIRE(ID2D1Factory1_CreateTransformedGeometry(factory,(ID2D1Geometry *)ellipse,&stored_transform,&transformed_input));
        input = (ID2D1Geometry *)transformed_input;
        REQUIRE(ID2D1Factory_CreatePathGeometry((ID2D1Factory *)factory,&result));
        REQUIRE(ID2D1PathGeometry_Open(result,&sink));
        hr=ID2D1Geometry_Widen(input,widths[mode],getenv("ELLIPSE_STYLE") ? (ID2D1StrokeStyle *)style : NULL,transformed ? &transform : NULL,
                .25f,(ID2D1SimplifiedGeometrySink *)sink);
        printf("case %u %u %u\nwiden %08lx\n",shape,transformed,mode,hr);
        close_hr=ID2D1GeometrySink_Close(sink); printf("close %08lx\n",close_hr);
        ID2D1GeometrySink_Release(sink);
        if (SUCCEEDED(hr) && SUCCEEDED(close_hr))
        {
            hr=ID2D1PathGeometry_GetBounds(result,NULL,&bounds);
            printf("bounds %08lx %.9g %.9g %.9g %.9g\n",hr,bounds.left,bounds.top,bounds.right,bounds.bottom);
            hr=ID2D1PathGeometry_ComputeArea(result,NULL,.25f,&area);printf("area %08lx %.9g\n",hr,area);
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
        if (getenv("ELLIPSE_CONTOUR"))
            ID2D1PathGeometry_Simplify(result,D2D1_GEOMETRY_SIMPLIFICATION_OPTION_CUBICS_AND_LINES,
                    NULL,.25f,&dump_sink);
        ID2D1PathGeometry_Release(result);
        ID2D1TransformedGeometry_Release(transformed_input);
    }
    ID2D1StrokeStyle1_Release(style);
    ID2D1DeviceContext_SetTarget(dc,NULL);
    ID2D1EllipseGeometry_Release(ellipse);
    ID2D1SolidColorBrush_Release(brush);ID2D1Bitmap1_Release(readback);ID2D1Bitmap1_Release(target);
    ID2D1DeviceContext_Release(dc);ID2D1Device_Release(device);ID2D1Factory1_Release(factory);
    IDXGIDevice_Release(dxgi);ID3D11Device_Release(d3d);CoUninitialize();
    puts("done");return 0;
}
