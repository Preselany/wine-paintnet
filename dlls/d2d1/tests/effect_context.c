/* Tests for the versioned effect context and lookup-table resources.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include "d2d1effectauthor_1.h"
#include "d3d11.h"
#include "wine/test.h"
#include "initguid.h"

DEFINE_GUID(CLSID_ContextEffect, 0x5ae2be8a, 0x12b7, 0x41a6, 0xb8, 0x30, 0x75, 0x4b, 0x19, 0x7f, 0x6a, 0x92);

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

static void check_table(ID2D1LookupTable3D *table, ID2D1Factory1 *factory)
{
    ID2D1Factory *actual_factory;
    IUnknown *unknown;
    ID2D1Resource *resource;
    void *unsupported = (void *)0xdeadbeef;
    HRESULT hr;

    ID2D1LookupTable3D_GetFactory(table, &actual_factory);
    ok(actual_factory == (ID2D1Factory *)factory, "Unexpected factory %p.\n", actual_factory);
    ID2D1Factory_Release(actual_factory);
    hr = ID2D1LookupTable3D_QueryInterface(table, &IID_IUnknown, (void **)&unknown);
    ok(hr == S_OK, "IUnknown query returned %#lx.\n", hr);
    if (SUCCEEDED(hr)) IUnknown_Release(unknown);
    hr = ID2D1LookupTable3D_QueryInterface(table, &IID_ID2D1Resource, (void **)&resource);
    ok(hr == S_OK, "Resource query returned %#lx.\n", hr);
    if (SUCCEEDED(hr)) ID2D1Resource_Release(resource);
    hr = ID2D1LookupTable3D_QueryInterface(table, &IID_ID2D1Bitmap, &unsupported);
    ok(hr == E_NOINTERFACE && !unsupported, "Unsupported interface returned %#lx, %p.\n", hr, unsupported);
}

static void test_lookup_tables(ID2D1DeviceContext2 *context, ID2D1EffectContext1 *effect_context,
        ID2D1Factory1 *factory)
{
    static const struct
    {
        D2D1_BUFFER_PRECISION precision;
        unsigned int bytes_per_pixel;
    } formats[] =
    {
        {D2D1_BUFFER_PRECISION_8BPC_UNORM, 4},
        {D2D1_BUFFER_PRECISION_8BPC_UNORM_SRGB, 4},
        {D2D1_BUFFER_PRECISION_16BPC_UNORM, 8},
        {D2D1_BUFFER_PRECISION_16BPC_FLOAT, 8},
        {D2D1_BUFFER_PRECISION_32BPC_FLOAT, 16},
    };
    UINT32 extents[] = {2, 3, 4}, strides[2];
    BYTE data[512] = {0};
    ID2D1LookupTable3D *table;
    unsigned int i, path;
    HRESULT hr;

    for (path = 0; path < 2; ++path)
    {
        if (path && !effect_context) continue;
        for (i = 0; i < ARRAY_SIZE(formats); ++i)
        {
            winetest_push_context("path %u, precision %u", path, formats[i].precision);
            strides[0] = extents[0] * formats[i].bytes_per_pixel;
            strides[1] = extents[1] * strides[0];
            table = NULL;
            if (path)
                hr = ID2D1EffectContext1_CreateLookupTable3D(effect_context, formats[i].precision, extents,
                        data, extents[2] * strides[1], strides, &table);
            else
                hr = ID2D1DeviceContext2_CreateLookupTable3D(context, formats[i].precision, extents,
                        data, extents[2] * strides[1], strides, &table);
            ok(hr == S_OK, "CreateLookupTable3D returned %#lx.\n", hr);
            if (SUCCEEDED(hr))
            {
                check_table(table, factory);
                ID2D1LookupTable3D_Release(table);
            }
            winetest_pop_context();
        }
    }

    strides[0] = 16;
    strides[1] = 64;
    hr = ID2D1DeviceContext2_CreateLookupTable3D(context, D2D1_BUFFER_PRECISION_8BPC_UNORM,
            extents, data, 232, strides, &table);
    ok(hr == S_OK, "Padded table returned %#lx.\n", hr);
    if (SUCCEEDED(hr)) ID2D1LookupTable3D_Release(table);
    hr = ID2D1DeviceContext2_CreateLookupTable3D(context, D2D1_BUFFER_PRECISION_8BPC_UNORM,
            extents, data, 231, strides, &table);
    ok(FAILED(hr), "Truncated final row returned %#lx.\n", hr);
    if (SUCCEEDED(hr)) ID2D1LookupTable3D_Release(table);

    strides[0] = 7;
    hr = ID2D1DeviceContext2_CreateLookupTable3D(context, D2D1_BUFFER_PRECISION_8BPC_UNORM,
            extents, data, sizeof(data), strides, &table);
    ok(FAILED(hr), "Overlapping rows returned %#lx.\n", hr);
    if (SUCCEEDED(hr)) ID2D1LookupTable3D_Release(table);
    strides[0] = 16;
    strides[1] = 39;
    hr = ID2D1DeviceContext2_CreateLookupTable3D(context, D2D1_BUFFER_PRECISION_8BPC_UNORM,
            extents, data, sizeof(data), strides, &table);
    ok(FAILED(hr), "Overlapping planes returned %#lx.\n", hr);
    if (SUCCEEDED(hr)) ID2D1LookupTable3D_Release(table);
    strides[1] = 64;
    extents[0] = 0;
    hr = ID2D1DeviceContext2_CreateLookupTable3D(context, D2D1_BUFFER_PRECISION_8BPC_UNORM,
            extents, data, sizeof(data), strides, &table);
    ok(FAILED(hr), "Zero extent returned %#lx.\n", hr);
    if (SUCCEEDED(hr)) ID2D1LookupTable3D_Release(table);
    extents[0] = extents[1] = extents[2] = ~0u;
    strides[0] = strides[1] = ~0u;
    hr = ID2D1DeviceContext2_CreateLookupTable3D(context, D2D1_BUFFER_PRECISION_32BPC_FLOAT,
            extents, data, sizeof(data), strides, &table);
    ok(FAILED(hr), "Overflowing extents returned %#lx.\n", hr);
    if (SUCCEEDED(hr)) ID2D1LookupTable3D_Release(table);
}

START_TEST(effect_context)
{
    static const WCHAR xml[] = L"<?xml version='1.0'?><Effect>"
        L"<Property name='DisplayName' type='string' value='Context test'/>"
        L"<Property name='Author' type='string' value='Wine'/>"
        L"<Property name='Category' type='string' value='Test'/>"
        L"<Property name='Description' type='string' value='Effect context regression'/>"
        L"<Inputs><Input name='Source'/></Inputs></Effect>";
    ID2D1Factory1 *factory;
    ID3D11Device *d3d_device;
    IDXGIDevice *dxgi_device;
    ID2D1Device *device;
    ID2D1DeviceContext *context;
    ID2D1DeviceContext2 *context2;
    ID2D1EffectContext1 *effect_context1 = NULL;
    ID2D1Effect *effect;
    ID2D1EffectContext *base;
    IUnknown *unknown_base = NULL, *unknown_version = NULL;
    D2D1_FEATURE_DATA_DOUBLES doubles;
    D3D11_FEATURE_DATA_DOUBLES d3d_doubles;
    HRESULT hr, expected_hr;
    float dpi_x = 0, dpi_y = 0;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            NULL, 0, D3D11_SDK_VERSION, &d3d_device, NULL, NULL);
    if (FAILED(hr)) { win_skip("No D3D11 device, hr %#lx.\n", hr); goto uninit; }
    hr = ID3D11Device_QueryInterface(d3d_device, &IID_IDXGIDevice, (void **)&dxgi_device);
    ok(hr == S_OK, "DXGI device returned %#lx.\n", hr);
    if (FAILED(hr)) goto release_d3d;
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL, (void **)&factory);
    ok(hr == S_OK, "Factory returned %#lx.\n", hr);
    if (FAILED(hr)) goto release_dxgi;
    hr = ID2D1Factory1_CreateDevice(factory, dxgi_device, &device);
    ok(hr == S_OK, "Device returned %#lx.\n", hr);
    if (FAILED(hr)) goto release_factory;
    hr = ID2D1Device_CreateDeviceContext(device, D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &context);
    ok(hr == S_OK, "Context returned %#lx.\n", hr);
    if (FAILED(hr)) goto release_device;
    hr = ID2D1DeviceContext_QueryInterface(context, &IID_ID2D1DeviceContext2, (void **)&context2);
    if (FAILED(hr)) { win_skip("No ID2D1DeviceContext2.\n"); goto release_context; }
    ID2D1DeviceContext_SetDpi(context, 123.0f, 145.0f);
    hr = ID2D1Factory1_RegisterEffectFromString(factory, &CLSID_ContextEffect, xml, NULL, 0, effect_factory);
    ok(hr == S_OK, "RegisterEffect returned %#lx.\n", hr);
    if (FAILED(hr)) goto release_context2;
    hr = ID2D1DeviceContext_CreateEffect(context, &CLSID_ContextEffect, &effect);
    ok(hr == S_OK, "CreateEffect returned %#lx.\n", hr);
    if (FAILED(hr)) goto unregister;
    ok(!!captured_context, "Initialize did not supply an effect context.\n");
    if (!captured_context) goto release_effect;
    hr = ID2D1EffectContext_QueryInterface(captured_context, &IID_ID2D1EffectContext1, (void **)&effect_context1);
    ok(hr == S_OK, "EffectContext1 query returned %#lx.\n", hr);
    if (SUCCEEDED(hr))
    {
        hr = ID2D1EffectContext1_QueryInterface(effect_context1, &IID_ID2D1EffectContext, (void **)&base);
        ok(hr == S_OK, "Base interface query returned %#lx.\n", hr);
        if (SUCCEEDED(hr)) ID2D1EffectContext_Release(base);
        hr = ID2D1EffectContext_QueryInterface(captured_context, &IID_IUnknown, (void **)&unknown_base);
        ok(hr == S_OK, "Base IUnknown query returned %#lx.\n", hr);
        hr = ID2D1EffectContext1_QueryInterface(effect_context1, &IID_IUnknown, (void **)&unknown_version);
        ok(hr == S_OK, "Versioned IUnknown query returned %#lx.\n", hr);
        if (unknown_base && unknown_version)
            ok(unknown_base == unknown_version, "Effect-context COM identity changed.\n");
        if (unknown_base) IUnknown_Release(unknown_base);
        if (unknown_version) IUnknown_Release(unknown_version);
        ID2D1EffectContext1_GetDpi(effect_context1, &dpi_x, &dpi_y);
        ok(dpi_x == 123.0f && dpi_y == 145.0f, "Unexpected DPI %f, %f.\n", dpi_x, dpi_y);
        expected_hr = ID3D11Device_CheckFeatureSupport(d3d_device, D3D11_FEATURE_DOUBLES,
                &d3d_doubles, sizeof(d3d_doubles));
        hr = ID2D1EffectContext1_CheckFeatureSupport(effect_context1, D2D1_FEATURE_DOUBLES,
                &doubles, sizeof(doubles));
        ok(hr == expected_hr, "Feature query returned %#lx, expected %#lx.\n", hr, expected_hr);
        if (SUCCEEDED(hr))
            ok(doubles.doublePrecisionFloatShaderOps == d3d_doubles.DoublePrecisionFloatShaderOps,
                    "Feature support did not match the D3D device.\n");
    }
    test_lookup_tables(context2, effect_context1, factory);
    ID2D1EffectContext_Release(captured_context);
    if (effect_context1) ID2D1EffectContext1_Release(effect_context1);
release_effect:
    ID2D1Effect_Release(effect);
unregister:
    ID2D1Factory1_UnregisterEffect(factory, &CLSID_ContextEffect);
release_context2:
    ID2D1DeviceContext2_Release(context2);
release_context:
    ID2D1DeviceContext_Release(context);
release_device:
    ID2D1Device_Release(device);
release_factory:
    ID2D1Factory1_Release(factory);
release_dxgi:
    IDXGIDevice_Release(dxgi_device);
release_d3d:
    ID3D11Device_Release(d3d_device);
uninit:
    CoUninitialize();
}
