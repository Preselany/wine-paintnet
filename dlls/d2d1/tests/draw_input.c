/* Public-API measurements for custom Direct2D image-input pixel shaders.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <math.h>
#include "wine/test.h"
#include <stdlib.h>
#include "d2d1effectauthor.h"
#include "d2d1effects_2.h"
#include "d3d11.h"
#include "effect_test.h"
#include "d3dcompiler.h"
#include "initguid.h"

DEFINE_GUID(CLSID_ProbeDrawInput, 0xc5bd85e7, 0x576c, 0x4935, 0xab, 0x01, 0x77, 0x6c, 0x4d, 0x9e, 0xea, 0x03);
DEFINE_GUID(GUID_ProbeInputShader, 0xc5bd85e7, 0x576c, 0x4935, 0xab, 0x01, 0x77, 0x6c, 0x4d, 0x9e, 0xea, 0x04);

struct draw_effect
{
    ID2D1EffectImpl effect;
    ID2D1DrawTransform transform;
    LONG refs;
    ID2D1DrawInfo *info;
};
static struct draw_effect *latest;
static ID3DBlob *code;

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
    HRESULT hr = ID2D1EffectContext_LoadPixelShader(context,&GUID_ProbeInputShader,
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
static UINT32 STDMETHODCALLTYPE transform_count(ID2D1DrawTransform *iface) { (void)iface; return 1; }
static HRESULT STDMETHODCALLTYPE transform_output(ID2D1DrawTransform *iface, const D2D1_RECT_L *output,
        D2D1_RECT_L *inputs, UINT32 count)
{
    (void)iface;
    if (count != 1) return E_INVALIDARG;
    inputs[0] = *output;
    ++mapped_outputs;

    return count == 1 ? S_OK : E_INVALIDARG;
}
static HRESULT STDMETHODCALLTYPE transform_inputs(ID2D1DrawTransform *iface, const D2D1_RECT_L *inputs,
        const D2D1_RECT_L *opaque_inputs, UINT32 count, D2D1_RECT_L *output, D2D1_RECT_L *opaque)
{
    (void)iface; (void)opaque_inputs;
    if (count != 1) return E_INVALIDARG;
    ++mapped_inputs;
    *output = inputs[0];
    memset(opaque,0,sizeof(*opaque));
    return count == 1 ? S_OK : E_INVALIDARG;
}
static HRESULT STDMETHODCALLTYPE transform_invalid(ID2D1DrawTransform *iface, UINT32 index,
        D2D1_RECT_L input, D2D1_RECT_L *output)
{
    (void)iface;
    if (index != 0) return E_INVALIDARG;
    *output = input;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE transform_info(ID2D1DrawTransform *iface, ID2D1DrawInfo *info)
{
    struct draw_effect *e = CONTAINING_RECORD(iface,struct draw_effect,transform);
    if (e->info) ID2D1DrawInfo_Release(e->info);
    ID2D1DrawInfo_AddRef(e->info = info);
    return ID2D1DrawInfo_SetPixelShader(info,&GUID_ProbeInputShader,D2D1_PIXEL_OPTIONS_NONE);
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
    hr = ID2D1DrawInfo_SetPixelShader(info,&CLSID_ProbeDrawInput,0);
    ok(hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND),"Missing shader: %#lx.\n",hr);
    for (i = 0; i < 5; ++i)
    {
        hr = ID2D1DrawInfo_SetPixelShader(info,&GUID_ProbeInputShader,i);
        ok(hr == (i <= 1 ? S_OK : E_INVALIDARG),"Options %u: %#lx.\n",i,hr);
    }
    hr = ID2D1DrawInfo_SetInputDescription(info,0,input);
    ok(hr == S_OK,"Input zero: %#lx.\n",hr);
    for (i = 0; i < 24; ++i)
    {
        input.filter = i;
        hr = ID2D1DrawInfo_SetInputDescription(info,0,input);
        expected = i == 0 || i == 1 || i == 4 || i == 5 || i == 16 || i == 17 || i == 20 || i == 21
                ? S_OK : E_INVALIDARG;
        ok(hr == expected,"Filter %u: %#lx, expected %#lx.\n",i,hr,expected);
    }
    input.filter = D2D1_FILTER_ANISOTROPIC;
    for (i = 0; i < 4; ++i)
    {
        input.levelOfDetailCount = i;
        hr = ID2D1DrawInfo_SetInputDescription(info,0,input);
        ok(hr == S_OK,"LOD count %u: %#lx.\n",i,hr);
    }
    input.filter = D2D1_FILTER_MIN_MAG_MIP_LINEAR;
    input.levelOfDetailCount = 0;
    ID2D1DrawInfo_SetInputDescription(info,0,input);
    free(data);
}

/* Native WARP RGBA32F results; all six output precisions and three channel
 * depths produce these values when drawn directly to a float target. */
static const float expected_pixels[4][256] = {
    {
        0.03125f,0.0f,0.125f,0.9375f,0.15625f,0.0f,0.375f,0.9375f,0.28125f,0.0f,0.125f,0.9375f,0.40625003f,0.0f,0.375f,0.9375f,
        0.53125f,0.0f,0.125f,0.9375f,0.65625f,0.0f,0.375f,0.9375f,0.78125f,0.0f,0.125f,0.9375f,0.875f,0.0f,0.5f,0.9375f,
        0.03125f,0.09375f,0.3125f,0.9375f,0.15625f,0.09375f,0.1875f,0.9375f,0.28125f,0.09375f,0.3125f,0.9375f,0.40625003f,0.09375f,0.1875f,0.9375f,
        0.53125f,0.09375f,0.3125f,0.9375f,0.65625f,0.09375f,0.1875f,0.9375f,0.78125f,0.09375f,0.3125f,0.9375f,0.875f,0.09375f,0.124999993f,0.9375f,
        0.03125f,0.218750015f,0.1875f,0.9375f,0.15625f,0.218750015f,0.3125f,0.9375f,0.28125f,0.218750015f,0.1875f,0.9375f,0.40625003f,0.218750015f,0.3125f,0.9375f,
        0.53125f,0.218750015f,0.1875f,0.9375f,0.65625f,0.218750015f,0.3125f,0.9375f,0.78125f,0.218750015f,0.1875f,0.9375f,0.875f,0.218750015f,0.375f,0.9375f,
        0.03125f,0.34375f,0.3125f,0.9375f,0.15625f,0.34375f,0.1875f,0.9375f,0.28125f,0.34375f,0.3125f,0.9375f,0.40625003f,0.34375f,0.1875f,0.9375f,
        0.53125f,0.34375f,0.3125f,0.9375f,0.65625f,0.34375f,0.1875f,0.9375f,0.78125f,0.34375f,0.3125f,0.9375f,0.875f,0.34375f,0.124999993f,0.9375f,
        0.03125f,0.46875f,0.1875f,0.9375f,0.15625f,0.46875f,0.3125f,0.9375f,0.28125f,0.46875f,0.1875f,0.9375f,0.40625003f,0.46875f,0.3125f,0.9375f,
        0.53125f,0.46875f,0.1875f,0.9375f,0.65625f,0.46875f,0.3125f,0.9375f,0.78125f,0.46875f,0.1875f,0.9375f,0.875f,0.46875f,0.375f,0.9375f,
        0.03125f,0.59375f,0.3125f,0.9375f,0.15625f,0.59375f,0.1875f,0.9375f,0.28125f,0.59375f,0.3125f,0.9375f,0.40625003f,0.59375f,0.1875f,0.9375f,
        0.53125f,0.59375f,0.3125f,0.9375f,0.65625f,0.59375f,0.1875f,0.9375f,0.78125f,0.59375f,0.3125f,0.9375f,0.875f,0.59375f,0.124999993f,0.9375f,
        0.03125f,0.71875006f,0.1875f,0.9375f,0.15625f,0.71875006f,0.3125f,0.9375f,0.28125f,0.71875006f,0.1875f,0.9375f,0.40625003f,0.71875006f,0.3125f,0.9375f,
        0.53125f,0.71875006f,0.1875f,0.9375f,0.65625f,0.71875006f,0.3125f,0.9375f,0.78125f,0.71875006f,0.1875f,0.9375f,0.875f,0.71875006f,0.375f,0.9375f,
        0.03125f,0.84375f,0.3125f,0.9375f,0.15625f,0.84375f,0.1875f,0.9375f,0.28125f,0.84375f,0.3125f,0.9375f,0.40625003f,0.84375f,0.1875f,0.9375f,
        0.53125f,0.84375f,0.3125f,0.9375f,0.65625f,0.84375f,0.1875f,0.9375f,0.78125f,0.84375f,0.3125f,0.9375f,0.875f,0.84375f,0.124999993f,0.9375f
    },
    {
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.03125f,0.0f,0.125f,0.9375f,0.15625f,0.0f,0.375f,0.9375f,
        0.28125f,0.0f,0.125f,0.9375f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.03125f,0.09375f,0.3125f,0.9375f,0.15625f,0.09375f,0.1875f,0.9375f,
        0.28125f,0.09375f,0.3125f,0.9375f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.03125f,0.218750015f,0.1875f,0.9375f,0.15625f,0.218750015f,0.3125f,0.9375f,
        0.28125f,0.218750015f,0.1875f,0.9375f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f
    },
    {
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.03125f,0.0f,0.125f,0.9375f,0.15625f,0.0f,0.375f,0.9375f,0.28125f,0.0f,0.125f,0.9375f,0.40625003f,0.0f,0.375f,0.9375f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.03125f,0.09375f,0.3125f,0.9375f,0.15625f,0.09375f,0.1875f,0.9375f,0.28125f,0.09375f,0.3125f,0.9375f,0.40625003f,0.09375f,0.1875f,0.9375f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.03125f,0.218750015f,0.1875f,0.9375f,0.15625f,0.218750015f,0.3125f,0.9375f,0.28125f,0.218750015f,0.1875f,0.9375f,0.40625003f,0.218750015f,0.3125f,0.9375f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.03125f,0.34375f,0.3125f,0.9375f,0.15625f,0.34375f,0.1875f,0.9375f,0.28125f,0.34375f,0.3125f,0.9375f,0.40625003f,0.34375f,0.1875f,0.9375f
    },
    {
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.9375f,0.125f,0.0f,0.5f,0.9375f,
        0.25f,0.0f,0.0f,0.9375f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.125f,0.5f,0.9375f,0.125f,0.125f,0.0f,0.9375f,
        0.25f,0.125f,0.5f,0.9375f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.25f,0.0f,0.9375f,0.125f,0.25f,0.5f,0.9375f,
        0.25f,0.25f,0.0f,0.9375f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
        0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f
    }
};

START_TEST(draw_input)
{
    static const char shader[] = "Texture2D<float4> image:register(t0); SamplerState sam:register(s0);"
        "cbuffer C : register(b0) {float4 c;}"
        "float4 main(float4 pos:SV_POSITION,float4 scene:SCENE_POSITION,float4 tc:TEXCOORD0):SV_TARGET"
        "{return image.Sample(sam,tc.xy+c.xy*tc.zw)*c.z;}";
    static const WCHAR xml[] = L"<?xml version='1.0'?><Effect><Property name='DisplayName' type='string' value='Draw probe'/>"
        L"<Property name='Author' type='string' value='Wine'/><Property name='Category' type='string' value='Test'/>"
        L"<Property name='Description' type='string' value='Custom shader probe'/><Inputs><Input name='Input'/></Inputs></Effect>";
    ID3D11Device *d3d = NULL;
    IDXGIDevice *dxgi = NULL;
    ID2D1Factory1 *f = NULL;
    ID2D1Device *device = NULL;
    ID2D1DeviceContext *dc = NULL;
    ID2D1Effect *effect = NULL, *outer = NULL;
    ID2D1Image *image = NULL;
    ID2D1Bitmap1 *target = NULL,*readback = NULL,*source = NULL;
    ID3DBlob *errors = NULL;
    D2D1_BITMAP_PROPERTIES1 props = {{DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},96,96,D2D1_BITMAP_OPTIONS_TARGET,NULL};
    D2D1_SIZE_U size = {8,8};
    D2D1_COLOR_F clear = {0};
    D2D1_POINT_2F offset = {1,1};
    D2D1_RECT_F crop = {-1,-1,3,3},rect;
    D2D1_MAPPED_RECT map;
    float constants[4] = {.25f,-.25f,1.25f,0};
    float pixels[8][8][4];
    HRESULT hr;
    unsigned int p,d,variant,x,y;
    D3D_DRIVER_TYPE driver = effect_test_driver();
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
#define REQUIRE(call) do { hr = (call); ok(hr == S_OK,"%s returned %#lx.\n",#call,hr); if (FAILED(hr)) goto done; } while (0)
    REQUIRE(D3DCompile(shader,strlen(shader),NULL,NULL,NULL,"main","ps_4_0",0,0,&code,&errors));
    REQUIRE(D3D11CreateDevice(NULL,driver,NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,NULL,0,D3D11_SDK_VERSION,&d3d,NULL,NULL));
    REQUIRE(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&f));
    REQUIRE(ID2D1Factory1_CreateDevice(f,dxgi,&device));
    REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&dc));
    REQUIRE(ID2D1Factory1_RegisterEffectFromString(f,&CLSID_ProbeDrawInput,xml,NULL,0,factory));
    REQUIRE(ID2D1DeviceContext_CreateEffect(dc,&CLSID_ProbeDrawInput,&effect));
    validate(latest->info);
    ID2D1DrawInfo_SetPixelShader(latest->info,&GUID_ProbeInputShader,0);
    REQUIRE(ID2D1DrawInfo_SetPixelShaderConstantBuffer(latest->info,(BYTE *)constants,sizeof(constants)));
    for(y=0;y<8;++y)for(x=0;x<8;++x)
    {
        pixels[y][x][0] = x * .1f;
        pixels[y][x][1] = y * .1f;
        pixels[y][x][2] = ((x+y)%2) * .4f;
        pixels[y][x][3] = .75f;
    }
    props.bitmapOptions = D2D1_BITMAP_OPTIONS_NONE;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,pixels,sizeof(pixels[0]),&props,&source));
    ID2D1Effect_SetInput(effect,0,(ID2D1Image *)source,TRUE);
    props.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,NULL,0,&props,&target));
    props.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,NULL,0,&props,&readback));
    ID2D1DeviceContext_SetTarget(dc,(ID2D1Image *)target);
    ID2D1Effect_GetOutput(effect,&image);
    for (p = 0; p <= 5; ++p) for (d = 0; d <= 4; d += d ? 3 : 1) for (variant = 0; variant < 4; ++variant)
    {
        winetest_push_context("precision %u depth %u variant %u",p,d,variant);
        ID2D1DrawInfo_SetOutputBuffer(latest->info,p,d);
        ID2D1DeviceContext_SetDpi(dc,variant == 2 ? 192 : 96,variant == 2 ? 192 : 96);
        {
            D2D1_INPUT_DESCRIPTION desc = {variant == 3 ? D2D1_FILTER_MIN_MAG_MIP_POINT : D2D1_FILTER_MIN_MAG_MIP_LINEAR,0};
            REQUIRE(ID2D1DrawInfo_SetInputDescription(latest->info,0,desc));
        }
        prepared = mapped_inputs = mapped_outputs = 0;
        /* Input invalidation is also valid for source effects: invalidate the output via property dirtiness. */
        ID2D1Effect_SetValue(effect,D2D1_PROPERTY_CACHED,D2D1_PROPERTY_TYPE_BOOL,(const BYTE *)&(BOOL){FALSE},sizeof(BOOL));
        hr = ID2D1DeviceContext_GetImageLocalBounds(dc,image,&rect);
        ok(hr == S_OK,"Bounds returned %#lx.\n",hr);
        ok(rect.left == 0 && rect.top == 0 && rect.right == (variant == 2 ? 4 : 8)
                && rect.bottom == (variant == 2 ? 4 : 8),"Unexpected bounds %.9g %.9g %.9g %.9g.\n",
                rect.left,rect.top,rect.right,rect.bottom);
        ID2D1DeviceContext_BeginDraw(dc);
        ID2D1DeviceContext_Clear(dc,&clear);
        ID2D1DeviceContext_DrawImage(dc,image,variant ? &offset : NULL,variant ? &crop : NULL,
                D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,D2D1_COMPOSITE_MODE_SOURCE_OVER);
        hr = ID2D1DeviceContext_EndDraw(dc,NULL,NULL);
        ok(hr == S_OK,"Draw returned %#lx.\n",hr);
        if (FAILED(hr)) {winetest_pop_context(); continue;}
        REQUIRE(ID2D1Bitmap1_CopyFromBitmap(readback,NULL,(ID2D1Bitmap *)target,NULL));
        REQUIRE(ID2D1Bitmap1_Map(readback,D2D1_MAP_OPTIONS_READ,&map));
        for (y = 0; y < size.height; ++y) for (x = 0; x < size.width; ++x)
        {
            const float *v = (float *)(map.bits+y*map.pitch)+4*x;
            unsigned int channel;
            for (channel = 0; channel < 4; ++channel)
            {
                float expected = expected_pixels[variant][(y*8+x)*4+channel];
                ok(fabsf(v[channel]-expected) < 1e-6f,"Pixel %u,%u channel %u: %.9g, expected %.9g.\n",
                        x,y,channel,v[channel],expected);
            }
        }
        ID2D1Bitmap1_Unmap(readback);
        winetest_pop_context();
    }
done:
    if (dc) ID2D1DeviceContext_SetTarget(dc,NULL);
    if (image) ID2D1Image_Release(image);
    if (readback) ID2D1Bitmap1_Release(readback);
    if (target) ID2D1Bitmap1_Release(target);
    if (source) ID2D1Bitmap1_Release(source);
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
