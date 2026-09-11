/* Tests for custom effect factories with distinct COM interfaces.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include "d2d1_1.h"
#include "d2d1effectauthor.h"
#include "d3d11.h"
#include "initguid.h"
#include "d2d1effects.h"
#include "wine/test.h"

DEFINE_GUID(CLSID_IdentityEffect, 0x3b8f3590, 0x52db, 0x49b3, 0x9a, 0x65, 0x18, 0x2b, 0xb2, 0x7f, 0x03, 0x61);

/* Extra slots make an incorrect cast to ID2D1EffectImpl fail deterministically
 * instead of reading beyond an IUnknown vtable and crashing the test process. */
struct identity_vtbl
{
    IUnknownVtbl unknown;
    HRESULT (STDMETHODCALLTYPE *unexpected_initialize)(IUnknown *, ID2D1EffectContext *, ID2D1TransformGraph *);
    HRESULT (STDMETHODCALLTYPE *unexpected_prepare)(IUnknown *, D2D1_CHANGE_TYPE);
    HRESULT (STDMETHODCALLTYPE *unexpected_graph)(IUnknown *, ID2D1TransformGraph *);
};

struct identity_effect
{
    IUnknown IUnknown_iface;
    ID2D1EffectImpl ID2D1EffectImpl_iface;
    LONG refcount;
    UINT32 value;
};

static unsigned int initialized, destroyed, wrong_interface, property_calls;
static BOOL reject_interface;
static HRESULT initialize_result;

static HRESULT STDMETHODCALLTYPE identity_QueryInterface(IUnknown *iface, REFIID iid, void **out)
{
    struct identity_effect *effect = CONTAINING_RECORD(iface, struct identity_effect, IUnknown_iface);
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown)) *out = &effect->IUnknown_iface;
    else if (IsEqualGUID(iid, &IID_ID2D1EffectImpl) && !reject_interface) *out = &effect->ID2D1EffectImpl_iface;
    else return E_NOINTERFACE;
    IUnknown_AddRef(iface);
    return S_OK;
}

static ULONG STDMETHODCALLTYPE identity_AddRef(IUnknown *iface)
{
    struct identity_effect *effect = CONTAINING_RECORD(iface, struct identity_effect, IUnknown_iface);
    return InterlockedIncrement(&effect->refcount);
}

static ULONG STDMETHODCALLTYPE identity_Release(IUnknown *iface)
{
    struct identity_effect *effect = CONTAINING_RECORD(iface, struct identity_effect, IUnknown_iface);
    ULONG ref = InterlockedDecrement(&effect->refcount);
    if (!ref) { destroyed++; free(effect); }
    return ref;
}

static HRESULT STDMETHODCALLTYPE unexpected_initialize(IUnknown *iface, ID2D1EffectContext *context,
        ID2D1TransformGraph *graph)
{
    wrong_interface++;
    return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE unexpected_prepare(IUnknown *iface, D2D1_CHANGE_TYPE type)
{
    wrong_interface++;
    return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE unexpected_graph(IUnknown *iface, ID2D1TransformGraph *graph)
{
    wrong_interface++;
    return E_UNEXPECTED;
}

static const struct identity_vtbl identity_vtbl =
{
    {identity_QueryInterface, identity_AddRef, identity_Release},
    unexpected_initialize, unexpected_prepare, unexpected_graph,
};

static HRESULT STDMETHODCALLTYPE implementation_QueryInterface(ID2D1EffectImpl *iface, REFIID iid, void **out)
{
    struct identity_effect *effect = CONTAINING_RECORD(iface, struct identity_effect, ID2D1EffectImpl_iface);
    return IUnknown_QueryInterface(&effect->IUnknown_iface, iid, out);
}

static ULONG STDMETHODCALLTYPE implementation_AddRef(ID2D1EffectImpl *iface)
{
    struct identity_effect *effect = CONTAINING_RECORD(iface, struct identity_effect, ID2D1EffectImpl_iface);
    return IUnknown_AddRef(&effect->IUnknown_iface);
}

static ULONG STDMETHODCALLTYPE implementation_Release(ID2D1EffectImpl *iface)
{
    struct identity_effect *effect = CONTAINING_RECORD(iface, struct identity_effect, ID2D1EffectImpl_iface);
    return IUnknown_Release(&effect->IUnknown_iface);
}

static HRESULT STDMETHODCALLTYPE implementation_Initialize(ID2D1EffectImpl *iface, ID2D1EffectContext *context,
        ID2D1TransformGraph *graph)
{
    initialized++;
    if (FAILED(initialize_result)) return initialize_result;
    return ID2D1TransformGraph_SetPassthroughGraph(graph, 0);
}

static HRESULT STDMETHODCALLTYPE implementation_PrepareForRender(ID2D1EffectImpl *iface, D2D1_CHANGE_TYPE type)
{
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE implementation_SetGraph(ID2D1EffectImpl *iface, ID2D1TransformGraph *graph)
{
    return ID2D1TransformGraph_SetPassthroughGraph(graph, 0);
}

static const ID2D1EffectImplVtbl implementation_vtbl =
{
    implementation_QueryInterface, implementation_AddRef, implementation_Release,
    implementation_Initialize, implementation_PrepareForRender, implementation_SetGraph,
};

static HRESULT CALLBACK identity_factory(IUnknown **out)
{
    struct identity_effect *effect = calloc(1, sizeof(*effect));
    if (!effect) return E_OUTOFMEMORY;
    effect->IUnknown_iface.lpVtbl = &identity_vtbl.unknown;
    effect->ID2D1EffectImpl_iface.lpVtbl = &implementation_vtbl;
    effect->refcount = 1;
    effect->value = 42;
    *out = &effect->IUnknown_iface;
    return S_OK;
}

static HRESULT CALLBACK get_value(const IUnknown *iface, BYTE *data, UINT32 size, UINT32 *actual_size)
{
    const struct identity_effect *effect;
    property_calls++;
    ok(iface->lpVtbl == &identity_vtbl.unknown, "Property getter received a different interface.\n");
    if (iface->lpVtbl != &identity_vtbl.unknown) return E_NOINTERFACE;
    effect = CONTAINING_RECORD(iface, const struct identity_effect, IUnknown_iface);
    if (actual_size) *actual_size = sizeof(effect->value);
    if (!data) return S_OK;
    if (size < sizeof(effect->value)) return E_NOT_SUFFICIENT_BUFFER;
    memcpy(data, &effect->value, sizeof(effect->value));
    return S_OK;
}

static HRESULT CALLBACK set_value(IUnknown *iface, const BYTE *data, UINT32 size)
{
    struct identity_effect *effect;
    property_calls++;
    ok(iface->lpVtbl == &identity_vtbl.unknown, "Property setter received a different interface.\n");
    if (iface->lpVtbl != &identity_vtbl.unknown) return E_NOINTERFACE;
    effect = CONTAINING_RECORD(iface, struct identity_effect, IUnknown_iface);
    if (!data || size != sizeof(effect->value)) return E_INVALIDARG;
    memcpy(&effect->value, data, sizeof(effect->value));
    return S_OK;
}

START_TEST(effect_identity)
{
    static const WCHAR xml[] = L"<?xml version='1.0'?><Effect>"
        L"<Property name='DisplayName' type='string' value='Identity test'/>"
        L"<Property name='Author' type='string' value='Wine'/>"
        L"<Property name='Category' type='string' value='Test'/>"
        L"<Property name='Description' type='string' value='COM identity regression'/>"
        L"<Inputs><Input name='Source'/></Inputs>"
        L"<Property name='Value' type='uint32'/></Effect>";
    static const D2D1_PROPERTY_BINDING binding = {L"Value", set_value, get_value};
    ID2D1Factory1 *factory;
    ID3D11Device *d3d_device;
    IDXGIDevice *dxgi_device;
    ID2D1Device *device;
    ID2D1DeviceContext *context;
    ID2D1Effect *effect = NULL;
    HRESULT hr;
    UINT32 value;
    D2D1_VECTOR_4F color = {0.2f, 0.4f, 0.6f, 0.8f}, actual;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            NULL, 0, D3D11_SDK_VERSION, &d3d_device, NULL, NULL);
    if (FAILED(hr)) { win_skip("No D3D11 device, hr %#lx.\n", hr); goto uninit; }
    hr = ID3D11Device_QueryInterface(d3d_device, &IID_IDXGIDevice, (void **)&dxgi_device);
    ok(hr == S_OK, "DXGI device hr %#lx.\n", hr);
    if (FAILED(hr)) goto release_d3d;
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL, (void **)&factory);
    ok(hr == S_OK, "Factory hr %#lx.\n", hr);
    if (FAILED(hr)) goto release_dxgi;
    hr = ID2D1Factory1_CreateDevice(factory, dxgi_device, &device);
    ok(hr == S_OK, "Device hr %#lx.\n", hr);
    if (FAILED(hr)) goto release_factory;
    hr = ID2D1Device_CreateDeviceContext(device, D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &context);
    ok(hr == S_OK, "Context hr %#lx.\n", hr);
    if (FAILED(hr)) goto release_device;

    /* A builtin factory returns an implementation interface directly. Keep
     * that existing case working alongside the distinct-interface case. */
    hr = ID2D1DeviceContext_CreateEffect(context, &CLSID_D2D1Flood, &effect);
    ok(hr == S_OK, "Builtin Flood creation returned %#lx.\n", hr);
    if (SUCCEEDED(hr))
    {
        hr = ID2D1Effect_SetValue(effect, D2D1_FLOOD_PROP_COLOR, D2D1_PROPERTY_TYPE_VECTOR4,
                (const BYTE *)&color, sizeof(color));
        ok(hr == S_OK, "Builtin property setter returned %#lx.\n", hr);
        hr = ID2D1Effect_GetValue(effect, D2D1_FLOOD_PROP_COLOR, D2D1_PROPERTY_TYPE_VECTOR4,
                (BYTE *)&actual, sizeof(actual));
        ok(hr == S_OK && !memcmp(&actual, &color, sizeof(color)), "Builtin property round trip failed, hr %#lx.\n", hr);
        ID2D1Effect_Release(effect);
    }
    effect = NULL;
    hr = ID2D1Factory1_RegisterEffectFromString(factory, &CLSID_IdentityEffect, xml, &binding, 1, identity_factory);
    ok(hr == S_OK, "Register effect hr %#lx.\n", hr);
    if (FAILED(hr)) goto release_context;

    hr = ID2D1DeviceContext_CreateEffect(context, &CLSID_IdentityEffect, &effect);
    ok(hr == S_OK, "Create effect with distinct IUnknown/ID2D1EffectImpl returned %#lx.\n", hr);
    ok(initialized == 1, "Initialize called %u times.\n", initialized);
    ok(!wrong_interface, "Called an ID2D1EffectImpl method through IUnknown %u times.\n", wrong_interface);
    if (SUCCEEDED(hr))
    {
        value = 0;
        hr = ID2D1Effect_GetValue(effect, 0, D2D1_PROPERTY_TYPE_UINT32, (BYTE *)&value, sizeof(value));
        ok(hr == S_OK && value == 42, "Initial property hr %#lx, value %u.\n", hr, value);
        value = 73;
        hr = ID2D1Effect_SetValue(effect, 0, D2D1_PROPERTY_TYPE_UINT32, (BYTE *)&value, sizeof(value));
        ok(hr == S_OK, "Set property hr %#lx.\n", hr);
        value = 0;
        hr = ID2D1Effect_GetValue(effect, 0, D2D1_PROPERTY_TYPE_UINT32, (BYTE *)&value, sizeof(value));
        ok(hr == S_OK && value == 73, "Changed property hr %#lx, value %u.\n", hr, value);
        ok(ID2D1Effect_GetValueSize(effect, 0) == sizeof(value), "Incorrect property size.\n");
        ok(property_calls >= 4, "Property callbacks were not invoked.\n");
        ID2D1Effect_Release(effect);
    }
    ok(destroyed == 1, "Expected one destroyed implementation, got %u.\n", destroyed);

    reject_interface = TRUE;
    effect = NULL;
    hr = ID2D1DeviceContext_CreateEffect(context, &CLSID_IdentityEffect, &effect);
    ok(hr == E_NOINTERFACE, "Missing ID2D1EffectImpl returned %#lx.\n", hr);
    if (SUCCEEDED(hr)) ID2D1Effect_Release(effect);
    ok(destroyed == 2, "Rejected implementation leaked; destroyed %u.\n", destroyed);

    reject_interface = FALSE;
    initialize_result = E_ABORT;
    effect = NULL;
    hr = ID2D1DeviceContext_CreateEffect(context, &CLSID_IdentityEffect, &effect);
    ok(hr == E_ABORT, "Initialize failure returned %#lx.\n", hr);
    if (SUCCEEDED(hr)) ID2D1Effect_Release(effect);
    ok(destroyed == 3, "Failed implementation leaked; destroyed %u.\n", destroyed);
    ok(initialized == 2, "Expected two Initialize calls, got %u.\n", initialized);

    ID2D1Factory1_UnregisterEffect(factory, &CLSID_IdentityEffect);
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
