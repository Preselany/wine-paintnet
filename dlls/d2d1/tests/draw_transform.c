/* Tests for custom Direct2D pixel shader source transforms.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "wine/test.h"
#include "d2d1effectauthor.h"
#include "d2d1effects_2.h"
#include "d3d11.h"
#include "effect_test.h"
#include "d3dcompiler.h"
#include "initguid.h"

DEFINE_GUID(CLSID_ProbeDraw, 0xc5bd85e7, 0x576c, 0x4935, 0xab, 0x01, 0x77, 0x6c, 0x4d, 0x9e, 0xea, 0x03);
DEFINE_GUID(GUID_ProbeShader, 0xc5bd85e7, 0x576c, 0x4935, 0xab, 0x01, 0x77, 0x6c, 0x4d, 0x9e, 0xea, 0x04);

struct draw_effect
{
    ID2D1EffectImpl effect;
    ID2D1DrawTransform transform;
    LONG refs;
    ID2D1DrawInfo *info;
};
static struct draw_effect *latest;
static ID3DBlob *code;
static D2D1_RECT_L bounds = {-2,-1,6,5};
static unsigned int prepared, mapped_inputs, mapped_outputs;

static HRESULT STDMETHODCALLTYPE effect_qi(ID2D1EffectImpl *iface, REFIID iid, void **out)
{
    struct draw_effect *e = CONTAINING_RECORD(iface, struct draw_effect, effect);
    *out = NULL;
    if (IsEqualGUID(iid,&IID_IUnknown) || IsEqualGUID(iid,&IID_ID2D1EffectImpl)) *out = iface;
    else if (IsEqualGUID(iid,&IID_ID2D1TransformNode) || IsEqualGUID(iid,&IID_ID2D1Transform)
            || IsEqualGUID(iid,&IID_ID2D1DrawTransform)) *out = &e->transform;
    else return E_NOINTERFACE;
    InterlockedIncrement(&e->refs);
    return S_OK;
}
static ULONG STDMETHODCALLTYPE effect_addref(ID2D1EffectImpl *iface)
{
    return InterlockedIncrement(&CONTAINING_RECORD(iface,struct draw_effect,effect)->refs);
}
static ULONG STDMETHODCALLTYPE effect_release(ID2D1EffectImpl *iface)
{
    struct draw_effect *e = CONTAINING_RECORD(iface,struct draw_effect,effect);
    ULONG refs = InterlockedDecrement(&e->refs);
    if (!refs) { if (e->info) ID2D1DrawInfo_Release(e->info); free(e); }
    return refs;
}
static HRESULT STDMETHODCALLTYPE effect_initialize(ID2D1EffectImpl *iface, ID2D1EffectContext *context,
        ID2D1TransformGraph *graph)
{
    struct draw_effect *e = CONTAINING_RECORD(iface,struct draw_effect,effect);
    HRESULT hr = ID2D1EffectContext_LoadPixelShader(context,&GUID_ProbeShader,
            ID3D10Blob_GetBufferPointer(code),ID3D10Blob_GetBufferSize(code));
    if (FAILED(hr)) return hr;
    return ID2D1TransformGraph_SetSingleTransformNode(graph,(ID2D1TransformNode *)&e->transform);
}
static HRESULT STDMETHODCALLTYPE effect_prepare(ID2D1EffectImpl *iface, D2D1_CHANGE_TYPE change)
{
    (void)iface;
    ++prepared;
    (void)change;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE effect_graph(ID2D1EffectImpl *iface, ID2D1TransformGraph *graph)
{
    (void)iface; (void)graph;
    return E_NOTIMPL;
}
static const ID2D1EffectImplVtbl effect_vtbl = {
    effect_qi,effect_addref,effect_release,effect_initialize,effect_prepare,effect_graph
};
static HRESULT STDMETHODCALLTYPE transform_qi(ID2D1DrawTransform *iface, REFIID iid, void **out)
{
    return effect_qi(&CONTAINING_RECORD(iface,struct draw_effect,transform)->effect,iid,out);
}
static ULONG STDMETHODCALLTYPE transform_addref(ID2D1DrawTransform *iface)
{
    return effect_addref(&CONTAINING_RECORD(iface,struct draw_effect,transform)->effect);
}
static ULONG STDMETHODCALLTYPE transform_release(ID2D1DrawTransform *iface)
{
    return effect_release(&CONTAINING_RECORD(iface,struct draw_effect,transform)->effect);
}
static UINT32 STDMETHODCALLTYPE transform_count(ID2D1DrawTransform *iface) { (void)iface; return 0; }
static HRESULT STDMETHODCALLTYPE transform_output(ID2D1DrawTransform *iface, const D2D1_RECT_L *output,
        D2D1_RECT_L *inputs, UINT32 count)
{
    (void)iface; (void)inputs;
    ++mapped_outputs;
    (void)output;
    return count ? E_INVALIDARG : S_OK;
}
static HRESULT STDMETHODCALLTYPE transform_inputs(ID2D1DrawTransform *iface, const D2D1_RECT_L *inputs,
        const D2D1_RECT_L *opaque_inputs, UINT32 count, D2D1_RECT_L *output, D2D1_RECT_L *opaque)
{
    (void)iface; (void)inputs; (void)opaque_inputs;
    ++mapped_inputs;
    *output = bounds;
    memset(opaque,0,sizeof(*opaque));
    return count ? E_INVALIDARG : S_OK;
}
static HRESULT STDMETHODCALLTYPE transform_invalid(ID2D1DrawTransform *iface, UINT32 index,
        D2D1_RECT_L input, D2D1_RECT_L *output)
{
    (void)iface; (void)index; (void)input; (void)output;
    return E_INVALIDARG;
}
static HRESULT STDMETHODCALLTYPE transform_info(ID2D1DrawTransform *iface, ID2D1DrawInfo *info)
{
    struct draw_effect *e = CONTAINING_RECORD(iface,struct draw_effect,transform);
    if (e->info) ID2D1DrawInfo_Release(e->info);
    ID2D1DrawInfo_AddRef(e->info = info);
    return ID2D1DrawInfo_SetPixelShader(info,&GUID_ProbeShader,D2D1_PIXEL_OPTIONS_NONE);
}
static const ID2D1DrawTransformVtbl transform_vtbl = {
    transform_qi,transform_addref,transform_release,transform_count,
    transform_output,transform_inputs,transform_invalid,transform_info
};
static HRESULT CALLBACK factory(IUnknown **out)
{
    struct draw_effect *e = calloc(1,sizeof(*e));
    if (!e) return E_OUTOFMEMORY;
    e->effect.lpVtbl = &effect_vtbl;
    e->transform.lpVtbl = &transform_vtbl;
    e->refs = 1;
    latest = e;
    *out = (IUnknown *)&e->effect;
    return S_OK;
}
static void validate(ID2D1DrawInfo *info)
{
    static const unsigned int sizes[] = {0,1,4,15,16,17,31,32,65536,65537};
    static const unsigned int depths[] = {0,1,2,3,4,5,0xffffffff};
    BYTE *data = calloc(1,65552);
    unsigned int p,d,i;
    D2D1_INPUT_DESCRIPTION input = {D2D1_FILTER_MIN_MAG_MIP_POINT,0};
    HRESULT hr, expected;
    for (p = 0; p <= 6; ++p) for (d = 0; d < ARRAY_SIZE(depths); ++d)
    {
        hr = ID2D1DrawInfo_SetOutputBuffer(info,p,depths[d]);
        expected = p <= 5 && (depths[d] == 0 || depths[d] == 1 || depths[d] == 4) ? S_OK : E_INVALIDARG;
        ok(hr == expected,"Precision %u depth %u: %#lx expected %#lx.\n",p,depths[d],hr,expected);
    }
    for (i = 0; i < ARRAY_SIZE(sizes); ++i)
    {
        hr = ID2D1DrawInfo_SetPixelShaderConstantBuffer(info,data,sizes[i]);
        ok(hr == S_OK,"Constant buffer size %u: %#lx.\n",sizes[i],hr);
    }
    hr = ID2D1DrawInfo_SetPixelShaderConstantBuffer(info,NULL,0);
    ok(hr == S_OK,"Empty constant buffer: %#lx.\n",hr);
    hr = ID2D1DrawInfo_SetPixelShader(info,&CLSID_ProbeDraw,0);
    ok(hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND),"Missing shader: %#lx.\n",hr);
    for (i = 0; i < 5; ++i)
    {
        hr = ID2D1DrawInfo_SetPixelShader(info,&GUID_ProbeShader,i);
        ok(hr == (i <= 1 ? S_OK : E_INVALIDARG),"Options %u: %#lx.\n",i,hr);
    }
    hr = ID2D1DrawInfo_SetInputDescription(info,0,input);
    ok(hr == E_INVALIDARG,"Input zero in source transform: %#lx.\n",hr);
    free(data);
}

START_TEST(draw_transform)
{
    static const char shader[] = "cbuffer C : register(b0) {float4 c;}"
        "float4 main(float4 pos:SV_POSITION,float4 scene:SCENE_POSITION):SV_TARGET"
        "{return float4(scene.xy*0.1,c.z,c.w);}";
    static const WCHAR xml[] = L"<?xml version='1.0'?><Effect><Property name='DisplayName' type='string' value='Draw probe'/>"
        L"<Property name='Author' type='string' value='Wine'/><Property name='Category' type='string' value='Test'/>"
        L"<Property name='Description' type='string' value='Custom shader probe'/><Inputs></Inputs></Effect>";
    ID3D11Device *d3d = NULL;
    IDXGIDevice *dxgi = NULL;
    ID2D1Factory1 *f = NULL;
    ID2D1Device *device = NULL;
    ID2D1DeviceContext *dc = NULL;
    ID2D1Effect *effect = NULL, *outer = NULL;
    ID2D1Image *image = NULL;
    ID2D1Bitmap1 *target = NULL,*readback = NULL;
    ID3DBlob *errors = NULL;
    D2D1_BITMAP_PROPERTIES1 props = {{DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},96,96,D2D1_BITMAP_OPTIONS_TARGET,NULL};
    D2D1_SIZE_U size = {8,8};
    D2D1_COLOR_F clear = {0};
    D2D1_POINT_2F offset = {1,1};
    D2D1_RECT_F crop = {-1,-1,3,3},rect;
    D2D1_MAPPED_RECT map;
    float constants[4] = {0,0,1.25f,0.75f};
    HRESULT hr;
    unsigned int p,d,variant,x,y,cached,channel,nested;
    D3D_DRIVER_TYPE driver = effect_test_driver();
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
#define REQUIRE(call) do { hr = (call); ok(hr == S_OK,"%s: %#lx.\n",#call,hr); if (FAILED(hr)) goto done; } while (0)
    REQUIRE(D3DCompile(shader,strlen(shader),NULL,NULL,NULL,"main","ps_4_0",0,0,&code,&errors));
    REQUIRE(D3D11CreateDevice(NULL,driver,NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,NULL,0,D3D11_SDK_VERSION,&d3d,NULL,NULL));
    REQUIRE(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&f));
    REQUIRE(ID2D1Factory1_CreateDevice(f,dxgi,&device));
    REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&dc));
    REQUIRE(ID2D1Factory1_RegisterEffectFromString(f,&CLSID_ProbeDraw,xml,NULL,0,factory));
    REQUIRE(ID2D1DeviceContext_CreateEffect(dc,&CLSID_ProbeDraw,&effect));
    validate(latest->info);
    ID2D1DrawInfo_SetPixelShader(latest->info,&GUID_ProbeShader,0);
    REQUIRE(ID2D1DrawInfo_SetPixelShaderConstantBuffer(latest->info,(BYTE *)constants,sizeof(constants)));
    constants[2] = 7; /* SetPixelShaderConstantBuffer must own a copy. */
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,NULL,0,&props,&target));
    props.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,NULL,0,&props,&readback));
    ID2D1DeviceContext_SetTarget(dc,(ID2D1Image *)target);
    ID2D1Effect_GetOutput(effect,&image);
    for (nested = 0; nested < 2; ++nested)
    {
        if (nested)
        {
            const float amount = 0.5f;
            REQUIRE(ID2D1DeviceContext_CreateEffect(dc,&CLSID_D2D1Opacity,&outer));
            ID2D1Effect_SetInput(outer,0,image,TRUE);
            ID2D1Image_Release(image);
            ID2D1Effect_GetOutput(outer,&image);
            REQUIRE(ID2D1Effect_SetValue(outer,0,D2D1_PROPERTY_TYPE_FLOAT,(const BYTE *)&amount,sizeof(amount)));
        }
    for (cached = 0; cached < 2; ++cached) for (p = 0; p <= 5; ++p) for (d = 0; d <= 4; d += d ? 3 : 1) for (variant = 0; variant < 4; ++variant)
    {
        float sx,ox,oy,px,py,expected[4];
        BOOL inside;
        ID2D1DrawInfo_SetCached(latest->info,cached);
        REQUIRE(ID2D1DrawInfo_SetOutputBuffer(latest->info,p,d));
        ID2D1DeviceContext_SetDpi(dc,variant == 2 ? 192 : 96,variant == 2 ? 192 : 96);
        if (variant == 3) bounds = (D2D1_RECT_L){INT_MIN,INT_MIN,INT_MAX,INT_MAX};
        else bounds = (D2D1_RECT_L){-2,-1,6,5};
        prepared = mapped_inputs = mapped_outputs = 0;
        /* Input invalidation is also valid for source effects: invalidate the output via property dirtiness. */
        ID2D1Effect_SetValue(effect,D2D1_PROPERTY_CACHED,D2D1_PROPERTY_TYPE_BOOL,(const BYTE *)&(BOOL){FALSE},sizeof(BOOL));
        hr = ID2D1DeviceContext_GetImageLocalBounds(dc,image,&rect);
        sx = variant == 2 ? 2 : 1;
        ok(hr == S_OK,"GetImageLocalBounds: %#lx.\n",hr);
        ok(rect.left == (float)bounds.left/sx && rect.top == (float)bounds.top/sx
                && rect.right == (float)bounds.right/sx && rect.bottom == (float)bounds.bottom/sx,
                "Bounds {%g,%g,%g,%g}, variant %u.\n",rect.left,rect.top,rect.right,rect.bottom,variant);
        ID2D1DeviceContext_BeginDraw(dc);
        ID2D1DeviceContext_Clear(dc,&clear);
        ID2D1DeviceContext_DrawImage(dc,image,variant ? &offset : NULL,variant ? &crop : NULL,
                D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,D2D1_COMPOSITE_MODE_SOURCE_OVER);
        hr = ID2D1DeviceContext_EndDraw(dc,NULL,NULL);
        ok(hr == S_OK,"Draw precision %u depth %u variant %u cached %u: %#lx.\n",p,d,variant,cached,hr);
        ok(mapped_inputs >= 2 && !mapped_outputs,"Mapping calls %u %u.\n",mapped_inputs,mapped_outputs);
        if (FAILED(hr)) continue;
        REQUIRE(ID2D1Bitmap1_CopyFromBitmap(readback,NULL,(ID2D1Bitmap *)target,NULL));
        REQUIRE(ID2D1Bitmap1_Map(readback,D2D1_MAP_OPTIONS_READ,&map));

        for (y = 0; y < size.height; ++y) for (x = 0; x < size.width; ++x)
        {
            const float *v = (float *)(map.bits+y*map.pitch)+4*x;
            ox = variant ? offset.x-crop.left : 0;
            oy = variant ? offset.y-crop.top : 0;
            px = x+0.5f-ox*sx;
            py = y+0.5f-oy*sx;
            inside = px >= bounds.left && py >= bounds.top && px < bounds.right && py < bounds.bottom;
            if (variant) inside = inside && px >= crop.left*sx && py >= crop.top*sx
                    && px < crop.right*sx && py < crop.bottom*sx;
            memset(expected,0,sizeof(expected));
            if (inside)
            {
                expected[0] = px*0.1f;
                expected[1] = cached && d == 1 ? 0 : py*0.1f;
                expected[2] = cached && d == 1 ? 0 : 1.25f;
                expected[3] = cached && d == 1 ? 1 : 0.75f;
            }
            if (nested) for (channel = 0; channel < 4; ++channel) expected[channel] *= 0.5f;
            for (channel = 0; channel < 4; ++channel)
                ok(fabsf(v[channel]-expected[channel]) < 2e-5f,
                        "Precision %u depth %u variant %u cached %u pixel (%u,%u) channel %u: %.9g expected %.9g.\n",
                        p,d,variant,cached,x,y,channel,v[channel],expected[channel]);
        }
        ID2D1Bitmap1_Unmap(readback);
    }
    }
done:
    if (dc) ID2D1DeviceContext_SetTarget(dc,NULL);
    if (image) ID2D1Image_Release(image);
    if (readback) ID2D1Bitmap1_Release(readback);
    if (target) ID2D1Bitmap1_Release(target);
    if (outer) ID2D1Effect_Release(outer);
    if (effect) ID2D1Effect_Release(effect);
    if (dc) ID2D1DeviceContext_Release(dc);
    if (device) ID2D1Device_Release(device);
    if (f) ID2D1Factory1_Release(f);
    if (dxgi) IDXGIDevice_Release(dxgi);
    if (d3d) ID3D11Device_Release(d3d);
    if (errors) ID3D10Blob_Release(errors);
    if (code) ID3D10Blob_Release(code);
    CoUninitialize();

}
