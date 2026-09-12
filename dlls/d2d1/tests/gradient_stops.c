/* Legacy gradient metadata and rendering, measured against Windows WARP.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <math.h>
#include "d2d1_1.h"
#include "d3d11.h"
#include "wine/test.h"
#include "effect_test.h"
#define REQUIRE(call) do { hr = (call); ok(hr == S_OK, "%s returned %#lx.\n", #call, hr); if (FAILED(hr)) return; } while (0)

static const float expected_stops[4][25] =
{
    {
        0.0f, 0.100000001f, 0.200000003f, 0.899999976f, 0.200000003f, 0.5f, 0.800000012f, 0.400000006f,
        0.200000003f, 0.800000012f, 1.0f, 0.300000012f, 0.699999988f, 0.5f, 1.0f, 99.0f,
        99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f,
        99.0f,
    },
    {
        0.0f, -1.0f, 2.0f, 0.400000006f, 0.0f, 0.5f, 0.25f, -0.5f,
        1.5f, 0.5f, 1.0f, 2.0f, 0.5f, -2.0f, 2.0f, 99.0f,
        99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f,
        99.0f,
    },
    {
        0.0f, 0.0100228256f, 0.0331047624f, 0.787412345f, 0.200000003f, 0.5f, 0.603827417f, 0.132868335f,
        0.0331047624f, 0.800000012f, 1.0f, 0.0732389688f, 0.447988421f, 0.214041144f, 1.0f, 99.0f,
        99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f,
        99.0f,
    },
    {
        0.0f, 0.0f, 1.0f, 0.132868335f, 0.0f, 0.5f, 0.0508760884f, 0.0f,
        1.0f, 0.5f, 1.0f, 1.0f, 0.214041144f, 0.0f, 1.0f, 99.0f,
        99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f,
        99.0f,
    },
};

static const float expected_pixels[12][64] =
{
    {
        0.0196078438f, 0.0392156877f, 0.180392161f, 0.200000003f, 0.0196078438f, 0.0392156877f, 0.180392161f, 0.200000003f,
        0.0196078438f, 0.0392156877f, 0.180392161f, 0.200000003f, 0.0196078438f, 0.0392156877f, 0.180392161f, 0.200000003f,
        0.052941177f, 0.0612745099f, 0.222549021f, 0.275490195f, 0.154901966f, 0.118137255f, 0.271078438f, 0.426470578f,
        0.309313715f, 0.186764702f, 0.264215678f, 0.574019611f, 0.518627465f, 0.271078438f, 0.205882356f, 0.724509776f,
        0.608823538f, 0.362745106f, 0.196568623f, 0.825980365f, 0.535294116f, 0.449509799f, 0.272549033f, 0.874509811f,
        0.449509799f, 0.542156875f, 0.358333319f, 0.925980389f, 0.35147059f, 0.647058845f, 0.449999988f, 0.974019587f,
        0.301960796f, 0.701960802f, 0.501960814f, 1.0f, 0.301960796f, 0.701960802f, 0.501960814f, 1.0f,
        0.301960796f, 0.701960802f, 0.501960814f, 1.0f, 0.301960796f, 0.701960802f, 0.501960814f, 1.0f,
    },
    {
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.00147058826f, 0.0504901968f, 0.0303921569f, 0.0617647059f, 0.0176470596f, 0.115196079f, 0.118137255f, 0.18921569f,
        0.0485294126f, 0.115196079f, 0.24166666f, 0.312745094f, 0.095588237f, 0.0504901968f, 0.404901952f, 0.436274499f,
        0.194607839f, 0.0372549035f, 0.490686268f, 0.563725471f, 0.367647052f, 0.12990196f, 0.427941173f, 0.687254906f,
        0.583823502f, 0.25343138f, 0.304411769f, 0.81078434f, 0.851960778f, 0.410294116f, 0.116176471f, 0.938235283f,
        1.0f, 0.501960814f, 0.0f, 1.0f, 1.0f, 0.501960814f, 0.0f, 1.0f,
        1.0f, 0.501960814f, 0.0f, 1.0f, 1.0f, 0.501960814f, 0.0f, 1.0f,
    },
    {
        0.607843161f, 0.360784322f, 0.196078435f, 0.823529422f, 0.53725493f, 0.447058827f, 0.274509817f, 0.874509811f,
        0.450980395f, 0.545098066f, 0.356862754f, 0.925490201f, 0.352941185f, 0.647058845f, 0.450980395f, 0.97647059f,
        0.0509803928f, 0.0627451017f, 0.223529413f, 0.274509817f, 0.152941182f, 0.117647059f, 0.270588249f, 0.423529416f,
        0.309803933f, 0.188235298f, 0.266666681f, 0.576470613f, 0.517647088f, 0.270588249f, 0.20784314f, 0.725490212f,
        0.607843161f, 0.360784322f, 0.196078435f, 0.823529422f, 0.53725493f, 0.447058827f, 0.274509817f, 0.874509811f,
        0.450980395f, 0.545098066f, 0.356862754f, 0.925490201f, 0.352941185f, 0.647058845f, 0.450980395f, 0.97647059f,
        0.0509803928f, 0.0627451017f, 0.223529413f, 0.274509817f, 0.152941182f, 0.117647059f, 0.270588249f, 0.423529416f,
        0.309803933f, 0.188235298f, 0.266666681f, 0.576470613f, 0.517647088f, 0.270588249f, 0.20784314f, 0.725490212f,
    },
    {
        0.192156866f, 0.0352941193f, 0.494117647f, 0.56078434f, 0.36470589f, 0.129411772f, 0.431372553f, 0.686274529f,
        0.58431375f, 0.254901975f, 0.305882365f, 0.811764717f, 0.850980401f, 0.411764711f, 0.117647059f, 0.937254906f,
        0.0f, 0.0549019612f, 0.0313725509f, 0.0627451017f, 0.0156862754f, 0.117647059f, 0.117647059f, 0.188235298f,
        0.0470588244f, 0.117647059f, 0.243137255f, 0.313725501f, 0.0941176489f, 0.0549019612f, 0.403921574f, 0.43921569f,
        0.192156866f, 0.0352941193f, 0.494117647f, 0.56078434f, 0.36470589f, 0.129411772f, 0.431372553f, 0.686274529f,
        0.58431375f, 0.254901975f, 0.305882365f, 0.811764717f, 0.850980401f, 0.411764711f, 0.117647059f, 0.937254906f,
        0.0f, 0.0549019612f, 0.0313725509f, 0.0627451017f, 0.0156862754f, 0.117647059f, 0.117647059f, 0.188235298f,
        0.0470588244f, 0.117647059f, 0.243137255f, 0.313725501f, 0.0941176489f, 0.0549019612f, 0.403921574f, 0.43921569f,
    },
    {
        0.517647088f, 0.270588249f, 0.20784314f, 0.725490212f, 0.309803933f, 0.188235298f, 0.266666681f, 0.576470613f,
        0.152941182f, 0.117647059f, 0.270588249f, 0.423529416f, 0.0509803928f, 0.0627451017f, 0.223529413f, 0.274509817f,
        0.0509803928f, 0.0627451017f, 0.223529413f, 0.274509817f, 0.152941182f, 0.117647059f, 0.270588249f, 0.423529416f,
        0.309803933f, 0.188235298f, 0.266666681f, 0.576470613f, 0.517647088f, 0.270588249f, 0.20784314f, 0.725490212f,
        0.607843161f, 0.360784322f, 0.196078435f, 0.823529422f, 0.53725493f, 0.447058827f, 0.274509817f, 0.874509811f,
        0.450980395f, 0.545098066f, 0.356862754f, 0.925490201f, 0.352941185f, 0.647058845f, 0.450980395f, 0.97647059f,
        0.352941185f, 0.647058845f, 0.450980395f, 0.97647059f, 0.450980395f, 0.545098066f, 0.356862754f, 0.925490201f,
        0.53725493f, 0.447058827f, 0.274509817f, 0.874509811f, 0.607843161f, 0.360784322f, 0.196078435f, 0.823529422f,
    },
    {
        0.0941176489f, 0.0549019612f, 0.403921574f, 0.43921569f, 0.0470588244f, 0.117647059f, 0.243137255f, 0.313725501f,
        0.0156862754f, 0.117647059f, 0.117647059f, 0.188235298f, 0.0f, 0.0549019612f, 0.0313725509f, 0.0627451017f,
        0.0f, 0.0549019612f, 0.0313725509f, 0.0627451017f, 0.0156862754f, 0.117647059f, 0.117647059f, 0.188235298f,
        0.0470588244f, 0.117647059f, 0.243137255f, 0.313725501f, 0.0941176489f, 0.0549019612f, 0.403921574f, 0.43921569f,
        0.192156866f, 0.0352941193f, 0.494117647f, 0.56078434f, 0.36470589f, 0.129411772f, 0.431372553f, 0.686274529f,
        0.58431375f, 0.254901975f, 0.305882365f, 0.811764717f, 0.850980401f, 0.411764711f, 0.117647059f, 0.937254906f,
        0.850980401f, 0.411764711f, 0.117647059f, 0.937254906f, 0.58431375f, 0.254901975f, 0.305882365f, 0.811764717f,
        0.36470589f, 0.129411772f, 0.431372553f, 0.686274529f, 0.192156866f, 0.0352941193f, 0.494117647f, 0.56078434f,
    },
    {
        0.0196078438f, 0.0392156877f, 0.180392161f, 0.200000003f, 0.0196078438f, 0.0392156877f, 0.180392161f, 0.200000003f,
        0.0196078438f, 0.0392156877f, 0.180392161f, 0.200000003f, 0.0196078438f, 0.0392156877f, 0.180392161f, 0.200000003f,
        0.0892156884f, 0.0666666701f, 0.232352942f, 0.275490195f, 0.222058818f, 0.126470596f, 0.313235283f, 0.426470578f,
        0.373039216f, 0.195098042f, 0.340686262f, 0.574019611f, 0.546078444f, 0.276470602f, 0.275000006f, 0.724509776f,
        0.627941191f, 0.375f, 0.214215681f, 0.825980365f, 0.58431375f, 0.470098048f, 0.30735293f, 0.874509811f,
        0.515686274f, 0.566176474f, 0.386274517f, 0.925980389f, 0.397058815f, 0.652941167f, 0.462254912f, 0.974019587f,
        0.301960796f, 0.701960802f, 0.501960814f, 1.0f, 0.301960796f, 0.701960802f, 0.501960814f, 1.0f,
        0.301960796f, 0.701960802f, 0.501960814f, 1.0f, 0.301960796f, 0.701960802f, 0.501960814f, 1.0f,
    },
    {
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.00441176491f, 0.0573529415f, 0.0333333351f, 0.0617647059f, 0.0294117648f, 0.153431371f, 0.134313732f, 0.18921569f,
        0.0602941178f, 0.201470584f, 0.261274517f, 0.312745094f, 0.100980394f, 0.159313723f, 0.41568628f, 0.436274499f,
        0.250980407f, 0.0970588252f, 0.53039217f, 0.563725471f, 0.461764693f, 0.215686277f, 0.556372523f, 0.687254906f,
        0.667647064f, 0.325490206f, 0.522058845f, 0.81078434f, 0.887745082f, 0.441666663f, 0.346568614f, 0.938235283f,
        1.0f, 0.501960814f, 0.0f, 1.0f, 1.0f, 0.501960814f, 0.0f, 1.0f,
        1.0f, 0.501960814f, 0.0f, 1.0f, 1.0f, 0.501960814f, 0.0f, 1.0f,
    },
    {
        0.627451003f, 0.372549027f, 0.215686277f, 0.823529422f, 0.588235319f, 0.470588237f, 0.309803933f, 0.874509811f,
        0.513725519f, 0.56078434f, 0.388235301f, 0.925490201f, 0.400000006f, 0.654901981f, 0.4627451f, 0.97647059f,
        0.0901960805f, 0.0627451017f, 0.235294119f, 0.274509817f, 0.219607845f, 0.125490203f, 0.313725501f, 0.423529416f,
        0.376470596f, 0.196078435f, 0.345098048f, 0.576470613f, 0.545098066f, 0.274509817f, 0.286274523f, 0.725490212f,
        0.627451003f, 0.372549027f, 0.215686277f, 0.823529422f, 0.588235319f, 0.470588237f, 0.309803933f, 0.874509811f,
        0.513725519f, 0.56078434f, 0.388235301f, 0.925490201f, 0.400000006f, 0.654901981f, 0.4627451f, 0.97647059f,
        0.0901960805f, 0.0627451017f, 0.235294119f, 0.274509817f, 0.219607845f, 0.125490203f, 0.313725501f, 0.423529416f,
        0.376470596f, 0.196078435f, 0.345098048f, 0.576470613f, 0.545098066f, 0.274509817f, 0.286274523f, 0.725490212f,
    },
    {
        0.250980407f, 0.101960786f, 0.529411793f, 0.56078434f, 0.458823532f, 0.215686277f, 0.556862772f, 0.686274529f,
        0.666666687f, 0.325490206f, 0.525490224f, 0.811764717f, 0.886274517f, 0.43921569f, 0.36470589f, 0.937254906f,
        0.00392156886f, 0.0588235296f, 0.0313725509f, 0.0627451017f, 0.0274509806f, 0.152941182f, 0.13333334f, 0.188235298f,
        0.0627451017f, 0.203921571f, 0.262745112f, 0.313725501f, 0.101960786f, 0.168627456f, 0.41568628f, 0.43921569f,
        0.250980407f, 0.101960786f, 0.529411793f, 0.56078434f, 0.458823532f, 0.215686277f, 0.556862772f, 0.686274529f,
        0.666666687f, 0.325490206f, 0.525490224f, 0.811764717f, 0.886274517f, 0.43921569f, 0.36470589f, 0.937254906f,
        0.00392156886f, 0.0588235296f, 0.0313725509f, 0.0627451017f, 0.0274509806f, 0.152941182f, 0.13333334f, 0.188235298f,
        0.0627451017f, 0.203921571f, 0.262745112f, 0.313725501f, 0.101960786f, 0.168627456f, 0.41568628f, 0.43921569f,
    },
    {
        0.545098066f, 0.274509817f, 0.286274523f, 0.725490212f, 0.376470596f, 0.196078435f, 0.345098048f, 0.576470613f,
        0.219607845f, 0.125490203f, 0.313725501f, 0.423529416f, 0.0901960805f, 0.0627451017f, 0.235294119f, 0.274509817f,
        0.0901960805f, 0.0627451017f, 0.235294119f, 0.274509817f, 0.219607845f, 0.125490203f, 0.313725501f, 0.423529416f,
        0.376470596f, 0.196078435f, 0.345098048f, 0.576470613f, 0.545098066f, 0.274509817f, 0.286274523f, 0.725490212f,
        0.627451003f, 0.372549027f, 0.215686277f, 0.823529422f, 0.588235319f, 0.470588237f, 0.309803933f, 0.874509811f,
        0.513725519f, 0.56078434f, 0.388235301f, 0.925490201f, 0.400000006f, 0.654901981f, 0.4627451f, 0.97647059f,
        0.400000006f, 0.654901981f, 0.4627451f, 0.97647059f, 0.513725519f, 0.56078434f, 0.388235301f, 0.925490201f,
        0.588235319f, 0.470588237f, 0.309803933f, 0.874509811f, 0.627451003f, 0.372549027f, 0.215686277f, 0.823529422f,
    },
    {
        0.101960786f, 0.168627456f, 0.41568628f, 0.43921569f, 0.0627451017f, 0.203921571f, 0.262745112f, 0.313725501f,
        0.0274509806f, 0.152941182f, 0.13333334f, 0.188235298f, 0.00392156886f, 0.0588235296f, 0.0313725509f, 0.0627451017f,
        0.00392156886f, 0.0588235296f, 0.0313725509f, 0.0627451017f, 0.0274509806f, 0.152941182f, 0.13333334f, 0.188235298f,
        0.0627451017f, 0.203921571f, 0.262745112f, 0.313725501f, 0.101960786f, 0.168627456f, 0.41568628f, 0.43921569f,
        0.250980407f, 0.101960786f, 0.529411793f, 0.56078434f, 0.458823532f, 0.215686277f, 0.556862772f, 0.686274529f,
        0.666666687f, 0.325490206f, 0.525490224f, 0.811764717f, 0.886274517f, 0.43921569f, 0.36470589f, 0.937254906f,
        0.886274517f, 0.43921569f, 0.36470589f, 0.937254906f, 0.666666687f, 0.325490206f, 0.525490224f, 0.811764717f,
        0.458823532f, 0.215686277f, 0.556862772f, 0.686274529f, 0.250980407f, 0.101960786f, 0.529411793f, 0.56078434f,
    },
};

static void test_gradient_stops(void)
{
    ID3D11Device *d3d;
    IDXGIDevice *dxgi;
    ID2D1Factory1 *factory;
    ID2D1Device *device;
    ID2D1DeviceContext *dc;
    ID2D1GradientStopCollection *gradient;
    ID2D1GradientStopCollection1 *gradient1;
    ID2D1LinearGradientBrush *brush;
    ID2D1Bitmap1 *target, *readback;
    IUnknown *unknown;
    D2D1_SIZE_U size = {16,1};
    D2D1_BITMAP_PROPERTIES1 desc = {{DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},
            96,96,D2D1_BITMAP_OPTIONS_TARGET,NULL};
    D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES brush_desc = {{4,0},{12,0}};
    D2D1_GRADIENT_STOP stops[3], copy[5];
    D2D1_COLOR_F clear = {0};
    D2D1_RECT_F rect = {0,0,16,1};
    D2D1_MAPPED_RECT map;
    HRESULT hr;
    unsigned int gamma, extend, colors, i;
    REQUIRE(D3D11CreateDevice(NULL, effect_test_driver(),
            NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0, D3D11_SDK_VERSION, &d3d, NULL, NULL));
    REQUIRE(ID3D11Device_QueryInterface(d3d, &IID_IDXGIDevice, (void **)&dxgi));
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL, (void **)&factory));
    REQUIRE(ID2D1Factory1_CreateDevice(factory, dxgi, &device));
    REQUIRE(ID2D1Device_CreateDeviceContext(device, 0, &dc));
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc, size, NULL, 0, &desc, &target));
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc, size, NULL, 0, &desc, &readback));
    ID2D1DeviceContext_SetTarget(dc, (ID2D1Image *)target);
    for (gamma = 0; gamma < 2; ++gamma)
    for (extend = 0; extend < 3; ++extend)
    for (colors = 0; colors < 2; ++colors)
    {
        stops[0] = (D2D1_GRADIENT_STOP){0, {.1f,.2f,.9f,.2f}};
        stops[1] = (D2D1_GRADIENT_STOP){.5f, {.8f,.4f,.2f,.8f}};
        stops[2] = (D2D1_GRADIENT_STOP){1, {.3f,.7f,.5f,1}};
        if (colors)
        {
            stops[0].color = (D2D1_COLOR_F){-1, 2, .4f, 0};
            stops[1].color = (D2D1_COLOR_F){.25f, -.5f, 1.5f, .5f};
            stops[2].color = (D2D1_COLOR_F){2, .5f, -2, 2};
        }
        winetest_push_context("gamma %u, extend %u, colors %u", gamma, extend, colors);
        hr = ID2D1RenderTarget_CreateGradientStopCollection((ID2D1RenderTarget *)dc, stops, 3, gamma, extend, &gradient);
        ok(hr == S_OK, "Create returned %#lx.\n", hr);
        if (FAILED(hr)) { winetest_pop_context(); continue; }
        ok(ID2D1GradientStopCollection_GetGradientStopCount(gradient) == 3, "Wrong stop count.\n");
        ok(ID2D1GradientStopCollection_GetColorInterpolationGamma(gradient) == gamma, "Wrong gamma.\n");
        ok(ID2D1GradientStopCollection_GetExtendMode(gradient) == extend, "Wrong extend mode.\n");
        for (i = 0; i < 5; ++i) copy[i] = (D2D1_GRADIENT_STOP){99,{99,99,99,99}};
        ID2D1GradientStopCollection_GetGradientStops(gradient, copy, 5);
        ok(!memcmp(copy, stops, sizeof(stops)), "Original stops changed.\n");
        for (i = 3; i < 5; ++i)
            ok(copy[i].position == 99 && copy[i].color.r == 99 && copy[i].color.g == 99
                    && copy[i].color.b == 99 && copy[i].color.a == 99, "Tail overwritten.\n");
        hr = ID2D1GradientStopCollection_QueryInterface(gradient, &IID_ID2D1GradientStopCollection1, (void **)&gradient1);
        ok(hr == S_OK, "Versioned QI returned %#lx.\n", hr);
        if (SUCCEEDED(hr))
        {
            ok(ID2D1GradientStopCollection1_GetPreInterpolationSpace(gradient1) == (gamma ? 2 : 1), "Wrong pre space.\n");
            ok(ID2D1GradientStopCollection1_GetPostInterpolationSpace(gradient1) == 1, "Wrong post space.\n");
            ok(ID2D1GradientStopCollection1_GetBufferPrecision(gradient1) == 1, "Wrong precision.\n");
            ok(ID2D1GradientStopCollection1_GetColorInterpolationMode(gradient1) == 0, "Wrong interpolation.\n");
            for (i = 0; i < 5; ++i) copy[i] = (D2D1_GRADIENT_STOP){99,{99,99,99,99}};
            ID2D1GradientStopCollection1_GetGradientStops1(gradient1, copy, 5);
            for (i = 0; i < 25; ++i)
            {
                float expected = expected_stops[gamma * 2 + colors][i];
                ok(fabsf(((float *)copy)[i] - expected) < 1e-7f, "Stop component %u: %.9g, expected %.9g.\n",
                        i, ((float *)copy)[i], expected);
            }
            REQUIRE(ID2D1GradientStopCollection1_QueryInterface(gradient1, &IID_IUnknown, (void **)&unknown));
            ok(unknown == (IUnknown *)gradient, "Versioned interface changes identity.\n");
            IUnknown_Release(unknown);
            ID2D1GradientStopCollection1_Release(gradient1);
        }
        REQUIRE(ID2D1DeviceContext_CreateLinearGradientBrush(dc, &brush_desc, NULL, gradient, &brush));
        ID2D1DeviceContext_BeginDraw(dc);
        ID2D1DeviceContext_Clear(dc, &clear);
        ID2D1DeviceContext_FillRectangle(dc, &rect, (ID2D1Brush *)brush);
        hr = ID2D1DeviceContext_EndDraw(dc, NULL, NULL);
        ok(hr == S_OK, "Draw returned %#lx.\n", hr);
        if (SUCCEEDED(hr))
        {
            REQUIRE(ID2D1Bitmap1_CopyFromBitmap(readback, NULL, (ID2D1Bitmap *)target, NULL));
            REQUIRE(ID2D1Bitmap1_Map(readback, D2D1_MAP_OPTIONS_READ, &map));
            for (i = 0; i < 64; ++i)
            {
                float value = ((float *)map.bits)[i];
                float expected = expected_pixels[gamma * 6 + extend * 2 + colors][i];
                /* Gamma 1.0 still differs in the last 8-bit quantization step;
                 * retain the Windows reference values rather than baking in Wine's results. */
                float tolerance = gamma ? 1.0f / 255.0f + 1e-6f : 1e-6f;
                ok(isfinite(value) && fabsf(value - expected) <= tolerance,
                        "Channel %u: %.9g, expected %.9g.\n", i, value, expected);
            }
            REQUIRE(ID2D1Bitmap1_Unmap(readback));
        }
        ID2D1LinearGradientBrush_Release(brush);
        ID2D1GradientStopCollection_Release(gradient);
        winetest_pop_context();
    }
    ID2D1DeviceContext_SetTarget(dc, NULL);
    ID2D1Bitmap1_Release(readback);
    ID2D1Bitmap1_Release(target);
    ID2D1DeviceContext_Release(dc);
    ID2D1Device_Release(device);
    ID2D1Factory1_Release(factory);
    IDXGIDevice_Release(dxgi);
    ID3D11Device_Release(d3d);

}

START_TEST(gradient_stops)
{
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    test_gradient_stops();
    CoUninitialize();
}
