/* Tests for the effect-context feature levels.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include "d2d1effectauthor_1.h"
#include "d3d11_1.h"
#include "effect_test.h"
#include "wine/test.h"
#include "initguid.h"

DEFINE_GUID(CLSID_FeatureLevelEffect, 0x5ae2be8b, 0x12b7, 0x41a6, 0xb8, 0x30, 0x75, 0x4b, 0x19, 0x7f, 0x6a, 0x92);

struct context_effect
{
    ID2D1EffectImpl ID2D1EffectImpl_iface;
    LONG refcount;
};

static ID2D1EffectContext *captured_context;

static HRESULT STDMETHODCALLTYPE effect_QueryInterface(ID2D1EffectImpl *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_ID2D1EffectImpl))
        return E_NOINTERFACE;
    *out = iface;
    ID2D1EffectImpl_AddRef(iface);
    return S_OK;
}

static ULONG STDMETHODCALLTYPE effect_AddRef(ID2D1EffectImpl *iface)
{
    struct context_effect *effect = CONTAINING_RECORD(iface, struct context_effect, ID2D1EffectImpl_iface);
    return InterlockedIncrement(&effect->refcount);
}

static ULONG STDMETHODCALLTYPE effect_Release(ID2D1EffectImpl *iface)
{
    struct context_effect *effect = CONTAINING_RECORD(iface, struct context_effect, ID2D1EffectImpl_iface);
    ULONG refcount = InterlockedDecrement(&effect->refcount);
    if (!refcount) free(effect);
    return refcount;
}

static HRESULT STDMETHODCALLTYPE effect_Initialize(ID2D1EffectImpl *iface, ID2D1EffectContext *context,
        ID2D1TransformGraph *graph)
{
    captured_context = context;
    ID2D1EffectContext_AddRef(context);
    return ID2D1TransformGraph_SetPassthroughGraph(graph, 0);
}

static HRESULT STDMETHODCALLTYPE effect_PrepareForRender(ID2D1EffectImpl *iface, D2D1_CHANGE_TYPE type)
{
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE effect_SetGraph(ID2D1EffectImpl *iface, ID2D1TransformGraph *graph)
{
    return ID2D1TransformGraph_SetPassthroughGraph(graph, 0);
}

static const ID2D1EffectImplVtbl effect_vtbl =
{
    effect_QueryInterface, effect_AddRef, effect_Release,
    effect_Initialize, effect_PrepareForRender, effect_SetGraph,
};

static HRESULT CALLBACK effect_factory(IUnknown **out)
{
    struct context_effect *effect = calloc(1, sizeof(*effect));
    if (!effect) return E_OUTOFMEMORY;
    effect->ID2D1EffectImpl_iface.lpVtbl = &effect_vtbl;
    effect->refcount = 1;
    *out = (IUnknown *)&effect->ID2D1EffectImpl_iface;
    return S_OK;
}


static void test_feature_levels(D3D_FEATURE_LEVEL requested, UINT flags)
{
    static const struct
    {
        D3D_FEATURE_LEVEL levels[3];
        UINT count;
        HRESULT hr;
        D3D_FEATURE_LEVEL expected;
    } cases[] =
    {
        {{0x9100}, 1, S_OK, 0x9100},
        {{0x9200}, 1, S_OK, 0x9200},
        {{0x9300}, 1, S_OK, 0x9300},
        {{0xa000}, 1, S_OK, 0xa000},
        {{0xa100}, 1, S_OK, 0xa100},
        {{0xb000}, 1, S_OK, 0xb000},
        {{0xb100}, 1, S_OK, 0xb100},
        {{0xc000}, 1, D2DERR_INSUFFICIENT_DEVICE_CAPABILITIES, 0xdeadbeef},
        {{0xc100}, 1, D2DERR_INSUFFICIENT_DEVICE_CAPABILITIES, 0xdeadbeef},
        {{0xc200}, 1, D2DERR_INSUFFICIENT_DEVICE_CAPABILITIES, 0xdeadbeef},
        {{0x9100, 0xa000, 0xb000}, 3, S_OK, 0xb000},
        {{0xb100, 0xb000, 0xa000}, 3, S_OK, 0xb100},
        {{0xa000, 0xb000, 0x9300}, 3, S_OK, 0x9300},
        {{0x0}, 1, S_OK, 0x0},
        {{0x1}, 1, S_OK, 0x1},
        {{0xffffffff}, 1, S_OK, 0xffffffff},
        {{0xafff}, 1, S_OK, 0xafff},
        {{0xa001}, 1, S_OK, 0xa001},
        {{0x0, 0xb000}, 2, S_OK, 0xb000},
        {{0xb000, 0x0}, 2, S_OK, 0x0},
        {{0xffff, 0xa000}, 2, S_OK, 0xa000},
        {{0}, 0, D2DERR_INSUFFICIENT_DEVICE_CAPABILITIES, 0xdeadbeef},
        {{0x9300, 0x9300}, 2, S_OK, 0x9300},
        {{0x9300, 0xa000}, 2, S_OK, 0xa000},
        {{0x9300, 0xb000}, 2, S_OK, 0xb000},
        {{0x9300, 0xb100}, 2, S_OK, 0xb100},
        {{0x9300, 0xc000}, 2, S_OK, 0x9300},
        {{0xa000, 0x9300}, 2, S_OK, 0x9300},
        {{0xa000, 0xa000}, 2, S_OK, 0xa000},
        {{0xa000, 0xb000}, 2, S_OK, 0xb000},
        {{0xa000, 0xb100}, 2, S_OK, 0xb100},
        {{0xa000, 0xc000}, 2, S_OK, 0xa000},
        {{0xb000, 0x9300}, 2, S_OK, 0x9300},
        {{0xb000, 0xa000}, 2, S_OK, 0xa000},
        {{0xb000, 0xb000}, 2, S_OK, 0xb000},
        {{0xb000, 0xb100}, 2, S_OK, 0xb100},
        {{0xb000, 0xc000}, 2, S_OK, 0xb000},
        {{0xb100, 0x9300}, 2, S_OK, 0xb100},
        {{0xb100, 0xa000}, 2, S_OK, 0xb100},
        {{0xb100, 0xb000}, 2, S_OK, 0xb100},
        {{0xb100, 0xb100}, 2, S_OK, 0xb100},
        {{0xb100, 0xc000}, 2, S_OK, 0xb100},
        {{0xc000, 0x9300}, 2, S_OK, 0x9300},
        {{0xc000, 0xa000}, 2, S_OK, 0xa000},
        {{0xc000, 0xb000}, 2, S_OK, 0xb000},
        {{0xc000, 0xb100}, 2, S_OK, 0xb100},
        {{0xc000, 0xc000}, 2, D2DERR_INSUFFICIENT_DEVICE_CAPABILITIES, 0xdeadbeef},
    };
    static const WCHAR xml[] = L"<?xml version='1.0'?><Effect>"
        L"<Property name='DisplayName' type='string' value='Feature level test'/>"
        L"<Property name='Author' type='string' value='Wine'/>"
        L"<Property name='Category' type='string' value='Test'/>"
        L"<Property name='Description' type='string' value='Feature levels'/>"
        L"<Inputs><Input name='Source'/></Inputs></Effect>";
    const D3D_FEATURE_LEVEL required = D3D_FEATURE_LEVEL_11_1;
    ID3D11Device *d3d;
    ID3D11Device1 *d3d1;
    IDXGIDevice *dxgi;
    ID2D1Factory1 *factory;
    ID2D1Device *device;
    ID2D1DeviceContext *dc;
    ID2D1Effect *effect;
    D3D_FEATURE_LEVEL result, before;
    UINT i, repeat, state_flags;
    HRESULT hr;

    winetest_push_context("device %#x, flags %#x", requested, flags);
    hr = D3D11CreateDevice(NULL, effect_test_driver(), NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT | flags,
            &requested, 1, D3D11_SDK_VERSION, &d3d, NULL, NULL);
    if (FAILED(hr)) { win_skip("No requested D3D11 device, hr %#lx.\n", hr); goto done; }
    hr = ID3D11Device_QueryInterface(d3d, &IID_ID3D11Device1, (void **)&d3d1);
    if (FAILED(hr)) { win_skip("No ID3D11Device1.\n"); goto release_d3d; }
    state_flags = flags & D3D11_CREATE_DEVICE_SINGLETHREADED
            ? D3D11_1_CREATE_DEVICE_CONTEXT_STATE_SINGLETHREADED : 0;
    hr = ID3D11Device1_CreateDeviceContextState(d3d1, state_flags, &required, 1,
            D3D11_SDK_VERSION, &IID_ID3D11Device1, &result, NULL);
    ID3D11Device1_Release(d3d1);
    if (FAILED(hr)) { win_skip("Reference fixtures require adapter support for 11.1.\n"); goto release_d3d; }
    /* DXVK 3.1 promotes its reported level even for a query-only state call. */
    todo_wine ok(ID3D11Device_GetFeatureLevel(d3d) == requested, "Support query changed device level.\n");
    before = ID3D11Device_GetFeatureLevel(d3d);
    hr = ID3D11Device_QueryInterface(d3d, &IID_IDXGIDevice, (void **)&dxgi);
    ok(hr == S_OK, "DXGI query returned %#lx.\n", hr);
    if (FAILED(hr)) goto release_d3d;
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL, (void **)&factory);
    ok(hr == S_OK, "Factory returned %#lx.\n", hr);
    if (FAILED(hr)) goto release_dxgi;
    hr = ID2D1Factory1_CreateDevice(factory, dxgi, &device);
    ok(hr == S_OK, "Device returned %#lx.\n", hr);
    if (FAILED(hr)) goto release_factory;
    hr = ID2D1Device_CreateDeviceContext(device, 0, &dc);
    ok(hr == S_OK, "Context returned %#lx.\n", hr);
    if (FAILED(hr)) goto release_device;
    hr = ID2D1Factory1_RegisterEffectFromString(factory, &CLSID_FeatureLevelEffect, xml, NULL, 0, effect_factory);
    ok(hr == S_OK, "Registration returned %#lx.\n", hr);
    if (FAILED(hr)) goto release_dc;
    hr = ID2D1DeviceContext_CreateEffect(dc, &CLSID_FeatureLevelEffect, &effect);
    ok(hr == S_OK, "Effect returned %#lx.\n", hr);
    if (FAILED(hr)) goto unregister;
    /* These expected results are fixed native measurements, including the
     * unusual list-order behavior and preservation of output on failure. */
    for (repeat = 0; repeat < 2; ++repeat)
        for (i = 0; i < ARRAY_SIZE(cases); ++i)
        {
            winetest_push_context("repeat %u, case %u", repeat, i);
            result = 0xdeadbeef;
            hr = ID2D1EffectContext_GetMaximumSupportedFeatureLevel(captured_context,
                    cases[i].levels, cases[i].count, &result);
            ok(hr == cases[i].hr, "Got hr %#lx, expected %#lx.\n", hr, cases[i].hr);
            ok(result == cases[i].expected, "Got level %#x, expected %#x.\n", result, cases[i].expected);
            winetest_pop_context();
        }
    ok(ID3D11Device_GetFeatureLevel(d3d) == before, "Effect query changed device feature level.\n");
    ID2D1EffectContext_Release(captured_context);
    captured_context = NULL;
    ID2D1Effect_Release(effect);
unregister:
    ID2D1Factory1_UnregisterEffect(factory, &CLSID_FeatureLevelEffect);
release_dc:
    ID2D1DeviceContext_Release(dc);
release_device:
    ID2D1Device_Release(device);
release_factory:
    ID2D1Factory1_Release(factory);
release_dxgi:
    IDXGIDevice_Release(dxgi);
release_d3d:
    ID3D11Device_Release(d3d);
done:
    winetest_pop_context();
}

START_TEST(feature_level)
{
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    test_feature_levels(D3D_FEATURE_LEVEL_11_0, 0);
    test_feature_levels(D3D_FEATURE_LEVEL_10_0, 0);
    test_feature_levels(D3D_FEATURE_LEVEL_11_0, D3D11_CREATE_DEVICE_SINGLETHREADED);
    test_feature_levels(D3D_FEATURE_LEVEL_10_0, D3D11_CREATE_DEVICE_SINGLETHREADED);
    CoUninitialize();
}
