/* Native Windows/WARP Emboss pixel fixtures, property behavior, and graph recovery.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <math.h>
#include "d2d1_1.h"
#include "d2d1effects_2.h"
#include "d3d11.h"
#include "effect_test.h"
#include "wine/test.h"
#define PROBE_SIDE 5

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

static const struct reference_case
{
    float dpi;
    UINT unit_mode, pattern;
    float height, direction;
    UINT width;
    float gray[25];
} cases[] =
{
    {96.0f,0,0,0.0f,0.0f,5, /* impulse */
        {0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f}},
    {96.0f,0,0,0.125f,0.0f,5, /* impulse */
        {0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.745565951f, 0.764529228f, 0.785364926f, 0.766044378f, 0.766044378f, 0.724121213f, 0.766044378f, 0.804720759f, 0.766044378f, 0.766044378f, 0.7452299f, 0.764546752f, 0.785292566f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f}},
    {96.0f,0,0,1.0f,0.0f,5, /* impulse */
        {0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.574084103f, 0.684098661f, 0.873740137f, 0.766044378f, 0.766044378f, 0.394842833f, 0.766044378f, 0.972815514f, 0.766044378f, 0.766044378f, 0.57072705f, 0.684903204f, 0.873526752f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f}},
    {96.0f,0,0,2.0f,45.0f,5, /* impulse */
        {0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.261512548f, 0.216921449f, 0.625472605f, 0.766044378f, 0.766044378f, 0.216921449f, 0.766044378f, 0.862853765f, 0.766044378f, 0.766044378f, 0.625472605f, 0.862853765f, 0.996432364f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f}},
    {96.0f,0,0,10.0f,225.0f,5, /* impulse */
        {0.766044557f, 0.766044557f, 0.766044557f, 0.766044557f, 0.766044557f, 0.766044557f, 0.829340696f, 0.594937563f, 0.208489254f, 0.766044557f, 0.766044557f, 0.594940007f, 0.766044557f, 0.0f, 0.766044557f, 0.766044557f, 0.208492592f, 0.0f, 0.0f, 0.766044557f, 0.766044557f, 0.766044557f, 0.766044557f, 0.766044557f, 0.766044557f}},
    {96.0f,0,3,1.0f,0.0f,5, /* red-impulse */
        {0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.714886606f, 0.757494688f, 0.809581697f, 0.766044378f, 0.766044378f, 0.661728084f, 0.766044378f, 0.852814317f, 0.766044378f, 0.766044378f, 0.714017451f, 0.757592261f, 0.809429944f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f}},
    {96.0f,0,4,1.0f,90.0f,5, /* green-impulse */
        {0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.659566164f, 0.552259207f, 0.657700658f, 0.766044378f, 0.766044378f, 0.734582484f, 0.766044378f, 0.734925389f, 0.766044378f, 0.766044378f, 0.842422724f, 0.916273236f, 0.842197418f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f}},
    {96.0f,0,5,10.0f,45.0f,5, /* blue-impulse */
        {0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.474996626f, 0.437842786f, 0.710504413f, 0.766044378f, 0.766044378f, 0.437842786f, 0.766044378f, 0.890614033f, 0.766044378f, 0.766044378f, 0.710504413f, 0.890614033f, 0.950376153f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f}},
    {96.0f,0,6,1.0f,90.0f,5, /* alpha */
        {0.685170889f, 0.685170889f, 0.685170889f, 0.685170889f, 0.685170889f, 0.685170889f, 0.685872376f, 0.685872376f, 0.685872376f, 0.685170889f, 0.685170889f, 0.685872376f, 0.685872376f, 0.685872376f, 0.685170889f, 0.685170889f, 0.685872376f, 0.685872376f, 0.685872376f, 0.685170889f, 0.685170889f, 0.685170889f, 0.685170889f, 0.685170889f, 0.685170889f}},
    {96.0f,0,7,1.0f,135.0f,5, /* corner-impulse */
        {0.356731057f, 0.527015328f, 0.766044378f, 0.766044378f, 0.766044378f, 0.647175491f, 0.722546637f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f}},
    {96.0f,0,8,2.0f,0.0f,5, /* hdr-ramp */
        {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {96.0f,0,10,10.0f,225.0f,5, /* mixed */
        {0.457304507f, 0.719017863f, 0.0f, 0.0f, 0.23404637f, 0.0f, 0.0f, 0.0f, 0.0f, 0.267748028f, 0.0f, 0.0f, 0.67171818f, 0.472254783f, 0.0584512949f, 0.363418132f, 0.472254783f, 0.974088728f, 0.548000157f, 0.563546002f, 0.162763745f, 0.638240755f, 0.111386672f, 0.483262002f, 0.490645707f}},
    {96.0f,0,9,10.0f,45.0f,5, /* transparent-impulse */
        {0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.0f, 0.0f, 0.208490878f, 0.766044378f, 0.766044378f, 0.0f, 0.766044378f, 0.595678508f, 0.766044378f, 0.766044378f, 0.208490878f, 0.595678508f, 0.827590346f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f}},
    {96.0f,0,1,1.0f,360.0f,5, /* ramp-x */
        {0.397707492f, 0.397707492f, 0.397707492f, 0.397707492f, 0.397707492f, 0.397707492f, 0.397707283f, 0.397707283f, 0.397707283f, 0.397707492f, 0.397707492f, 0.397707283f, 0.397707283f, 0.397707283f, 0.397707492f, 0.397707492f, 0.397707283f, 0.397707283f, 0.397707283f, 0.397707492f, 0.397707492f, 0.397707492f, 0.397707492f, 0.397707492f, 0.397707492f}},
    {96.0f,0,2,1.0f,90.0f,5, /* ramp-y */
        {0.397707492f, 0.397707492f, 0.397707492f, 0.397707492f, 0.397707492f, 0.397707492f, 0.397707283f, 0.397707283f, 0.397707283f, 0.397707492f, 0.397707492f, 0.397707283f, 0.397707283f, 0.397707283f, 0.397707492f, 0.397707492f, 0.397707283f, 0.397707283f, 0.397707283f, 0.397707492f, 0.397707492f, 0.397707492f, 0.397707492f, 0.397707492f, 0.397707492f}},
    {192.0f,0,0,1.0f,0.0f,4, /* impulse */
        {0.643493533f, 0.63739872f, 0.625208974f, 0.619114161f, 0.0f, 0.611807108f, 0.606098592f, 0.594681561f, 0.588973045f, 0.0f, 0.548434138f, 0.543498278f, 0.533626676f, 0.528690815f, 0.0f, 0.516747713f, 0.51219821f, 0.503099203f, 0.4985497f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {192.0f,0,1,1.0f,0.0f,4, /* ramp-x */
        {0.0871556103f, 0.0871556103f, 0.0871556103f, 0.0871556103f, 0.0f, 0.0871556103f, 0.0871556103f, 0.0871556103f, 0.0871556103f, 0.0f, 0.0871556103f, 0.0871556103f, 0.0871556103f, 0.0871556103f, 0.0f, 0.0871556103f, 0.0871556103f, 0.0871556103f, 0.0871556103f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {192.0f,0,7,2.0f,45.0f,4, /* corner-impulse */
        {0.99809581f, 0.993105412f, 0.983124554f, 0.978134155f, 0.0f, 0.993105412f, 0.988642573f, 0.979716837f, 0.975253999f, 0.0f, 0.983124554f, 0.979716837f, 0.972901404f, 0.969493687f, 0.0f, 0.978134155f, 0.975253999f, 0.969493687f, 0.966613531f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {192.0f,0,10,10.0f,315.0f,4, /* mixed */
        {0.140511036f, 0.191511065f, 0.293511093f, 0.344511122f, 0.0f, 0.108207747f, 0.157685354f, 0.256640553f, 0.30611816f, 0.0f, 0.0436011702f, 0.0900339484f, 0.18289949f, 0.229332268f, 0.0f, 0.0112978816f, 0.056208238f, 0.146028951f, 0.190939307f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {192.0f,0,6,1.0f,0.0f,4, /* alpha */
        {0.0871556103f, 0.0871556103f, 0.0871556103f, 0.0871556103f, 0.0f, 0.0871556103f, 0.0871556103f, 0.0871556103f, 0.0871556103f, 0.0f, 0.0871556103f, 0.0871556103f, 0.0871556103f, 0.0871556103f, 0.0f, 0.0871556103f, 0.0871556103f, 0.0871556103f, 0.0871556103f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {192.0f,0,9,2.0f,0.0f,4, /* transparent-impulse */
        {0.503555059f, 0.489000887f, 0.459892571f, 0.445338398f, 0.0f, 0.445189953f, 0.432690114f, 0.407690465f, 0.395190626f, 0.0f, 0.32845974f, 0.320068568f, 0.303286225f, 0.294895053f, 0.0f, 0.270094633f, 0.263757795f, 0.251084119f, 0.244747281f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {192.0f,1,0,2.0f,45.0f,5, /* impulse */
        {0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.261512548f, 0.216921449f, 0.625472605f, 0.766044378f, 0.766044378f, 0.216921449f, 0.766044378f, 0.862853765f, 0.766044378f, 0.766044378f, 0.625472605f, 0.862853765f, 0.996432364f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f, 0.766044378f}},
    {192.0f,1,10,10.0f,225.0f,5, /* mixed */
        {0.457304507f, 0.719017863f, 0.0f, 0.0f, 0.23404637f, 0.0f, 0.0f, 0.0f, 0.0f, 0.267748028f, 0.0f, 0.0f, 0.67171818f, 0.472254783f, 0.0584512949f, 0.363418132f, 0.472254783f, 0.974088728f, 0.548000157f, 0.563546002f, 0.162763745f, 0.638240755f, 0.111386672f, 0.483262002f, 0.490645707f}},
};

static void compare(ID2D1Bitmap1 *target, ID2D1Bitmap1 *readback, const struct reference_case *test)
{
    D2D1_MAPPED_RECT mapped;
    UINT x,y,c;
    float value, expected;
    BOOL inside;
    HRESULT hr = ID2D1Bitmap1_CopyFromBitmap(readback,NULL,(ID2D1Bitmap *)target,NULL);
    ok(hr == S_OK,"Copy returned %#lx.\n",hr);
    hr = ID2D1Bitmap1_Map(readback,D2D1_MAP_OPTIONS_READ,&mapped);
    ok(hr == S_OK,"Map returned %#lx.\n",hr);
    if (FAILED(hr)) return;
    for (y = 0; y < 7; ++y) for (x = 0; x < 7; ++x) for (c = 0; c < 4; ++c)
    {
        inside = x >= 1 && y >= 1 && x <= test->width && y <= test->width;
        expected = !inside ? 0 : c == 3 ? 1 : test->gray[(y-1)*5+x-1];
        memcpy(&value,mapped.bits+y*mapped.pitch+(x*4+c)*sizeof(float),sizeof(float));
        ok(fabsf(value-expected) < .00002f,"Pixel %u,%u channel %u: %.9g, expected %.9g.\n",x,y,c,value,expected);
    }
    ID2D1Bitmap1_Unmap(readback);
}

START_TEST(emboss)
{
    ID2D1Factory1 *factory = NULL;
    ID3D11Device *d3d = NULL;
    IDXGIDevice *dxgi = NULL;
    ID2D1Device *device = NULL;
    ID2D1DeviceContext *context = NULL;
    ID2D1Effect *effect = NULL;
    ID2D1Image *image = NULL;
    ID2D1Bitmap1 *source = NULL, *target = NULL, *readback = NULL;
    D2D1_BITMAP_PROPERTIES1 desc = {{DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},96,96,0,NULL};
    D2D1_SIZE_U source_size = {5,5}, target_size = {7,7};
    D2D1_RECT_F crop, bounds;
    D2D1_POINT_2F offset;
    D2D1_COLOR_F clear = {0};
    float data[25][4], value, unit_scale;
    UINT i;
    HRESULT hr;
#define CHECK(call) do { hr = (call); ok(hr == S_OK,"%s returned %#lx.\n",#call,hr); if (FAILED(hr)) goto done; } while (0)
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    CHECK(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    hr = D3D11CreateDevice(NULL,effect_test_driver(),NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,NULL,0,D3D11_SDK_VERSION,&d3d,NULL,NULL);
    if (FAILED(hr)) { win_skip("No D3D11 device.\n"); goto done; }
    CHECK(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));
    CHECK(ID2D1Factory1_CreateDevice(factory,dxgi,&device));
    CHECK(ID2D1Device_CreateDeviceContext(device,0,&context));
    CHECK(ID2D1DeviceContext_CreateEffect(context,&CLSID_D2D1Emboss,&effect));
    ok(ID2D1Effect_GetPropertyCount(effect) == 2,"Wrong property count.\n");
    ok(ID2D1Effect_GetInputCount(effect) == 1,"Wrong input count.\n");
    CHECK(ID2D1Effect_GetValue(effect,0,D2D1_PROPERTY_TYPE_FLOAT,(BYTE *)&value,sizeof(value)));
    ok(value == 1,"Default height %g.\n",value);
    CHECK(ID2D1Effect_GetValue(effect,1,D2D1_PROPERTY_TYPE_FLOAT,(BYTE *)&value,sizeof(value)));
    ok(value == 0,"Default direction %g.\n",value);
    for (i = 0; i < 4; ++i)
    {
        static const float inputs[] = {-1,11,-1,361}, clamped[] = {0,10,0,360};
        CHECK(ID2D1Effect_SetValue(effect,i/2,D2D1_PROPERTY_TYPE_FLOAT,(const BYTE *)&inputs[i],sizeof(float)));
        CHECK(ID2D1Effect_GetValue(effect,i/2,D2D1_PROPERTY_TYPE_FLOAT,(BYTE *)&value,sizeof(value)));
        ok(value == clamped[i],"Clamp %u gave %g.\n",i,value);
    }
    CHECK(ID2D1DeviceContext_CreateBitmap(context,source_size,NULL,0,&desc,&source));
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
    CHECK(ID2D1DeviceContext_CreateBitmap(context,target_size,NULL,0,&desc,&target));
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_CANNOT_DRAW | D2D1_BITMAP_OPTIONS_CPU_READ;
    CHECK(ID2D1DeviceContext_CreateBitmap(context,target_size,NULL,0,&desc,&readback));
    ID2D1DeviceContext_SetTarget(context,(ID2D1Image *)target);
    ID2D1Effect_SetInput(effect,0,(ID2D1Image *)source,TRUE);
    ID2D1Effect_GetOutput(effect,&image);
    for (i = 0; i < ARRAY_SIZE(cases); ++i)
    {
        const struct reference_case *test = &cases[i];
        winetest_push_context("native fixture %u",i);
        pattern(data,test->pattern);
        CHECK(ID2D1Bitmap1_CopyFromMemory(source,NULL,data,5*16));
        ID2D1DeviceContext_SetDpi(context,test->dpi,test->dpi);
        ID2D1DeviceContext_SetUnitMode(context,test->unit_mode);
        CHECK(ID2D1Effect_SetValue(effect,0,D2D1_PROPERTY_TYPE_FLOAT,(const BYTE *)&test->height,sizeof(float)));
        CHECK(ID2D1Effect_SetValue(effect,1,D2D1_PROPERTY_TYPE_FLOAT,(const BYTE *)&test->direction,sizeof(float)));
        unit_scale = test->unit_mode == D2D1_UNIT_MODE_PIXELS ? 1 : test->dpi/96;
        crop = (D2D1_RECT_F){0,0,5/unit_scale,5/unit_scale};
        offset = (D2D1_POINT_2F){1/unit_scale,1/unit_scale};
        CHECK(ID2D1DeviceContext_GetImageLocalBounds(context,image,&bounds));
        ok(!bounds.left && !bounds.top && bounds.right == test->width/unit_scale && bounds.bottom == test->width/unit_scale,
                "Bounds {%g,%g,%g,%g}.\n",bounds.left,bounds.top,bounds.right,bounds.bottom);
        ID2D1DeviceContext_BeginDraw(context);
        ID2D1DeviceContext_Clear(context,&clear);
        ID2D1DeviceContext_DrawImage(context,image,&offset,&crop,D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,D2D1_COMPOSITE_MODE_SOURCE_OVER);
        CHECK(ID2D1DeviceContext_EndDraw(context,NULL,NULL));
        compare(target,readback,test);
        winetest_pop_context();
    }
    ID2D1Effect_SetInput(effect,0,image,TRUE);
    hr = ID2D1DeviceContext_GetImageLocalBounds(context,image,&bounds);
    ok(hr == D2DERR_CYCLIC_GRAPH,"Cycle returned %#lx.\n",hr);
    ID2D1Effect_SetInput(effect,0,(ID2D1Image *)source,TRUE);
    CHECK(ID2D1DeviceContext_GetImageLocalBounds(context,image,&bounds));
    ID2D1DeviceContext_BeginDraw(context);
    ID2D1DeviceContext_Clear(context,&clear);
    ID2D1DeviceContext_DrawImage(context,image,&offset,&crop,D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,D2D1_COMPOSITE_MODE_SOURCE_OVER);
    CHECK(ID2D1DeviceContext_EndDraw(context,NULL,NULL));
    compare(target,readback,&cases[ARRAY_SIZE(cases)-1]);
done:
    if (context) ID2D1DeviceContext_SetTarget(context,NULL);
    if (image) ID2D1Image_Release(image);
    if (effect) { ID2D1Effect_SetInput(effect,0,NULL,TRUE); ID2D1Effect_Release(effect); }
    if (source) ID2D1Bitmap1_Release(source);
    if (target) ID2D1Bitmap1_Release(target);
    if (readback) ID2D1Bitmap1_Release(readback);
    if (context) ID2D1DeviceContext_Release(context);
    if (device) ID2D1Device_Release(device);
    if (dxgi) IDXGIDevice_Release(dxgi);
    if (d3d) ID3D11Device_Release(d3d);
    if (factory) ID2D1Factory1_Release(factory);
    CoUninitialize();
#undef CHECK
}
