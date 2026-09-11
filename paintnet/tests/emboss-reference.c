/* Black-box Emboss measurements through the public Direct2D API.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Run with Microsoft's WARP on Windows; never link this to a replacement effect.
 */
#define COBJMACROS
#include <stdio.h>
#include <math.h>
#include "d2d1_1.h"
#include "d3d11.h"

#ifndef PROBE_SIDE
#define PROBE_SIDE 5
#endif

static const CLSID emboss_clsid = {0xb1c5eb2b,0x0348,0x43f0,{0x81,0x07,0x49,0x57,0xca,0xcb,0xa2,0xae}};

static void pattern(float data[PROBE_SIDE*PROBE_SIDE][4], unsigned int kind)
{
    unsigned int x,y,c;
    for (y = 0; y < PROBE_SIDE; ++y)
        for (x = 0; x < PROBE_SIDE; ++x)
        {
            float value = x == PROBE_SIDE/2 && y == PROBE_SIDE/2 ? 1 : 0;
            data[y*PROBE_SIDE+x][3] = kind == 6 ? (y+1)/(float)PROBE_SIDE : 1;
            if (kind == 9) data[y*PROBE_SIDE+x][3] = value;
            if (kind >= 11) value = y*PROBE_SIDE+x == kind-11 ? 1 : 0;
            if (kind == 7) value = !x && !y ? 1 : 0;
            for (c = 0; c < 3; ++c)
            {
                if (kind == 1 || kind == 6) data[y*PROBE_SIDE+x][c] = x/(PROBE_SIDE-1.0f);
                else if (kind == 2) data[y*PROBE_SIDE+x][c] = y/(PROBE_SIDE-1.0f);
                else if (kind >= 3 && kind <= 5) data[y*PROBE_SIDE+x][c] = c == kind-3 ? value : 0;
                else if (kind == 8) data[y*PROBE_SIDE+x][c] = x*.75f-1;
                else if (kind == 10) data[y*PROBE_SIDE+x][c] = ((int)((x*13+y*7+c*11)%19)-4)/10.0f;
                else data[y*PROBE_SIDE+x][c] = value;
                data[y*PROBE_SIDE+x][c] *= data[y*PROBE_SIDE+x][3];
            }
        }
}

#define CHECK(call) do { hr = (call); if (FAILED(hr)) { fprintf(stderr, "%s: %#lx\n", #call, hr); goto done; } } while (0)

int main(int argc, char **argv)
{
    static const char *names[] = {"impulse","ramp-x","ramp-y","red-impulse","green-impulse","blue-impulse","alpha",
            "corner-impulse","hdr-ramp","transparent-impulse","mixed"};
    static const float heights[] = {0,.125f,1,2,10};
    ID2D1Factory1 *factory = NULL;
    ID3D11Device *d3d = NULL;
    IDXGIDevice *dxgi = NULL;
    ID2D1Device *device = NULL;
    ID2D1DeviceContext *context = NULL;
    ID2D1Effect *effect = NULL;
    ID2D1Image *image = NULL;
    ID2D1Bitmap1 *source = NULL, *target = NULL, *readback = NULL;
    D2D1_BITMAP_PROPERTIES1 desc = {{DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},
            96,96,D2D1_BITMAP_OPTIONS_NONE,NULL};
    const D2D1_SIZE_U source_size = {PROBE_SIDE,PROBE_SIDE}, target_size = {PROBE_SIDE+2,PROBE_SIDE+2};
    const D2D1_COLOR_F clear = {0};
    D2D1_RECT_F crop;
    D2D1_POINT_2F offset;
    D2D1_RECT_F bounds = {0};
    D2D1_MAPPED_RECT mapped;
    unsigned int config,p,h,angle,x,y,c,precision,unit_mode;
    BOOL basis = argc > 1 && !strcmp(argv[1],"--basis");
    char pattern_name[40];
    float data[PROBE_SIDE*PROBE_SIDE][4], direction, pixel,dpi,unit_scale;
    HRESULT hr = S_OK, bounds_hr, draw_hr;

    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    CHECK(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    CHECK(D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_WARP,NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            NULL,0,D3D11_SDK_VERSION,&d3d,NULL,NULL));
    CHECK(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));
    CHECK(ID2D1Factory1_CreateDevice(factory,dxgi,&device));
    CHECK(ID2D1Device_CreateDeviceContext(device,0,&context));
    CHECK(ID2D1DeviceContext_CreateEffect(context,&emboss_clsid,&effect));
    CHECK(ID2D1DeviceContext_CreateBitmap(context,source_size,NULL,0,&desc,&source));
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
    CHECK(ID2D1DeviceContext_CreateBitmap(context,target_size,NULL,0,&desc,&target));
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_CANNOT_DRAW | D2D1_BITMAP_OPTIONS_CPU_READ;
    CHECK(ID2D1DeviceContext_CreateBitmap(context,target_size,NULL,0,&desc,&readback));
    ID2D1DeviceContext_SetTarget(context,(ID2D1Image *)target);
    ID2D1Effect_SetInput(effect,0,(ID2D1Image *)source,TRUE);
    ID2D1Effect_GetOutput(effect,&image);
    for (config = 0; config < (basis ? 1 : 6); ++config)
    {
    dpi = config < 2 ? 96 : 192;
    unit_mode = config >= 4 ? D2D1_UNIT_MODE_PIXELS : D2D1_UNIT_MODE_DIPS;
    precision = config % 2 ? D2D1_BUFFER_PRECISION_32BPC_FLOAT : D2D1_BUFFER_PRECISION_UNKNOWN;
    ID2D1DeviceContext_SetDpi(context,dpi,dpi);
    ID2D1DeviceContext_SetUnitMode(context,unit_mode);
    CHECK(ID2D1Effect_SetValue(effect,D2D1_PROPERTY_PRECISION,D2D1_PROPERTY_TYPE_ENUM,(const BYTE *)&precision,sizeof(precision)));
    unit_scale = unit_mode == D2D1_UNIT_MODE_PIXELS ? 1 : dpi/96;
    crop = (D2D1_RECT_F){0,0,PROBE_SIDE/unit_scale,PROBE_SIDE/unit_scale};
    offset = (D2D1_POINT_2F){1/unit_scale,1/unit_scale};
    for (p = basis ? 11 : 0; p < (basis ? 11+PROBE_SIDE*PROBE_SIDE : sizeof(names)/sizeof(names[0])); ++p)
    {
        pattern(data,p);
        if (basis) snprintf(pattern_name,sizeof(pattern_name),"basis-%u",p-11);
        else snprintf(pattern_name,sizeof(pattern_name),"%s",names[p]);
        CHECK(ID2D1Bitmap1_CopyFromMemory(source,NULL,data,PROBE_SIDE*4*sizeof(float)));
        for (h = basis ? 1 : 0; h < (basis ? 2 : sizeof(heights)/sizeof(heights[0])); ++h)
            for (angle = 0; angle <= (basis ? 270 : 360); angle += basis ? 90 : 45)
            {
                direction = angle;
                CHECK(ID2D1Effect_SetValue(effect,0,D2D1_PROPERTY_TYPE_FLOAT,(const BYTE *)&heights[h],sizeof(float)));
                CHECK(ID2D1Effect_SetValue(effect,1,D2D1_PROPERTY_TYPE_FLOAT,(const BYTE *)&direction,sizeof(float)));
                bounds_hr = ID2D1DeviceContext_GetImageLocalBounds(context,image,&bounds);
                ID2D1DeviceContext_BeginDraw(context);
                ID2D1DeviceContext_Clear(context,&clear);
                ID2D1DeviceContext_DrawImage(context,image,&offset,&crop,D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                        D2D1_COMPOSITE_MODE_SOURCE_OVER);
                draw_hr = ID2D1DeviceContext_EndDraw(context,NULL,NULL);
                CHECK(ID2D1Bitmap1_CopyFromBitmap(readback,NULL,(ID2D1Bitmap *)target,NULL));
                CHECK(ID2D1Bitmap1_Map(readback,D2D1_MAP_OPTIONS_READ,&mapped));
                printf("{\"side\":%u,\"dpi\":%g,\"unit_mode\":%u,\"precision\":%u,\"pattern\":\"%s\",\"height\":%.9g,\"direction\":%u,\"bounds_hr\":%ld,"
                        "\"bounds\":[%.9g,%.9g,%.9g,%.9g],\"draw_hr\":%ld,\"pixels\":[",
                        PROBE_SIDE,dpi,unit_mode,precision,pattern_name,heights[h],angle,bounds_hr,bounds.left,bounds.top,bounds.right,bounds.bottom,draw_hr);
                for (y = 0; y < PROBE_SIDE+2; ++y)
                    for (x = 0; x < PROBE_SIDE+2; ++x)
                    {
                        if (x || y) putchar(',');
                        putchar('[');
                        for (c = 0; c < 4; ++c)
                        {
                            memcpy(&pixel,mapped.bits+y*mapped.pitch+(x*4+c)*sizeof(float),sizeof(float));
                            printf(c ? ",%.9g" : "%.9g",pixel);
                        }
                        putchar(']');
                    }
                puts("]}");
                ID2D1Bitmap1_Unmap(readback);
            }
    }
    }
done:
    if (context) ID2D1DeviceContext_SetTarget(context,NULL);
    if (image) ID2D1Image_Release(image);
    if (effect) ID2D1Effect_Release(effect);
    if (source) ID2D1Bitmap1_Release(source);
    if (target) ID2D1Bitmap1_Release(target);
    if (readback) ID2D1Bitmap1_Release(readback);
    if (context) ID2D1DeviceContext_Release(context);
    if (device) ID2D1Device_Release(device);
    if (dxgi) IDXGIDevice_Release(dxgi);
    if (d3d) ID3D11Device_Release(d3d);
    if (factory) ID2D1Factory1_Release(factory);
    CoUninitialize();
    return FAILED(hr);
}
