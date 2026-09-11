/* Direct2D color profile resources.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "d2d1_private.h"
#include "wincodec.h"
#include "icm.h"
#include "shlwapi.h"
#include "lcms2.h"

WINE_DEFAULT_DEBUG_CHANNEL(d2d);

struct d2d_color_context
{
    ID2D1ColorContext1 ID2D1ColorContext1_iface;
    LONG refcount;
    ID2D1Factory *factory;
    D2D1_COLOR_CONTEXT_TYPE type;
    D2D1_COLOR_SPACE space;
    DXGI_COLOR_SPACE_TYPE dxgi_space;
    D2D1_SIMPLE_COLOR_PROFILE simple;
    BYTE *profile;
    UINT32 profile_size;
};

static inline struct d2d_color_context *impl_from_ID2D1ColorContext1(ID2D1ColorContext1 *iface)
{
    return CONTAINING_RECORD(iface, struct d2d_color_context, ID2D1ColorContext1_iface);
}

static HRESULT STDMETHODCALLTYPE d2d_color_context_QueryInterface(ID2D1ColorContext1 *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_ID2D1Resource)
            || IsEqualGUID(iid, &IID_ID2D1ColorContext) || IsEqualGUID(iid, &IID_ID2D1ColorContext1))
    {
        ID2D1ColorContext1_AddRef(iface);
        *out = iface;
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE d2d_color_context_AddRef(ID2D1ColorContext1 *iface)
{
    return InterlockedIncrement(&impl_from_ID2D1ColorContext1(iface)->refcount);
}

static ULONG STDMETHODCALLTYPE d2d_color_context_Release(ID2D1ColorContext1 *iface)
{
    struct d2d_color_context *context = impl_from_ID2D1ColorContext1(iface);
    ULONG refcount = InterlockedDecrement(&context->refcount);

    if (!refcount)
    {
        ID2D1Factory_Release(context->factory);
        free(context->profile);
        free(context);
    }
    return refcount;
}

static void STDMETHODCALLTYPE d2d_color_context_GetFactory(ID2D1ColorContext1 *iface, ID2D1Factory **factory)
{
    struct d2d_color_context *context = impl_from_ID2D1ColorContext1(iface);
    ID2D1Factory_AddRef(*factory = context->factory);
}

static D2D1_COLOR_SPACE STDMETHODCALLTYPE d2d_color_context_GetColorSpace(ID2D1ColorContext1 *iface)
{
    return impl_from_ID2D1ColorContext1(iface)->space;
}

static UINT32 STDMETHODCALLTYPE d2d_color_context_GetProfileSize(ID2D1ColorContext1 *iface)
{
    return impl_from_ID2D1ColorContext1(iface)->profile_size;
}

static HRESULT STDMETHODCALLTYPE d2d_color_context_GetProfile(ID2D1ColorContext1 *iface, BYTE *profile, UINT32 size)
{
    struct d2d_color_context *context = impl_from_ID2D1ColorContext1(iface);

    if (context->type != D2D1_COLOR_CONTEXT_TYPE_ICC || (!profile && size))
        return E_INVALIDARG;
    if (size) memset(profile, 0, size);
    if (size < context->profile_size)
        return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    if (context->profile_size) memcpy(profile, context->profile, context->profile_size);
    return S_OK;
}

static D2D1_COLOR_CONTEXT_TYPE STDMETHODCALLTYPE d2d_color_context_GetColorContextType(ID2D1ColorContext1 *iface)
{
    return impl_from_ID2D1ColorContext1(iface)->type;
}

static DXGI_COLOR_SPACE_TYPE STDMETHODCALLTYPE d2d_color_context_GetDXGIColorSpace(ID2D1ColorContext1 *iface)
{
    return impl_from_ID2D1ColorContext1(iface)->dxgi_space;
}

static HRESULT STDMETHODCALLTYPE d2d_color_context_GetSimpleColorProfile(ID2D1ColorContext1 *iface,
        D2D1_SIMPLE_COLOR_PROFILE *profile)
{
    struct d2d_color_context *context = impl_from_ID2D1ColorContext1(iface);

    if (context->type != D2D1_COLOR_CONTEXT_TYPE_SIMPLE || !profile)
        return E_INVALIDARG;
    *profile = context->simple;
    return S_OK;
}

static const ID2D1ColorContext1Vtbl d2d_color_context_vtbl =
{
    d2d_color_context_QueryInterface,
    d2d_color_context_AddRef,
    d2d_color_context_Release,
    d2d_color_context_GetFactory,
    d2d_color_context_GetColorSpace,
    d2d_color_context_GetProfileSize,
    d2d_color_context_GetProfile,
    d2d_color_context_GetColorContextType,
    d2d_color_context_GetDXGIColorSpace,
    d2d_color_context_GetSimpleColorProfile,
};

static struct d2d_color_context *d2d_color_context_alloc(ID2D1Factory *factory, D2D1_COLOR_CONTEXT_TYPE type)
{
    struct d2d_color_context *context;

    if (!(context = calloc(1, sizeof(*context)))) return NULL;
    context->ID2D1ColorContext1_iface.lpVtbl = &d2d_color_context_vtbl;
    context->refcount = 1;
    ID2D1Factory_AddRef(context->factory = factory);
    context->type = type;
    context->dxgi_space = DXGI_COLOR_SPACE_CUSTOM;
    return context;
}

static void d2d_color_context_set_space(struct d2d_color_context *context, D2D1_COLOR_SPACE space)
{
    context->space = space;
    context->dxgi_space = space == D2D1_COLOR_SPACE_SRGB ? DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709
            : space == D2D1_COLOR_SPACE_SCRGB ? DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709 : DXGI_COLOR_SPACE_CUSTOM;
}

static void d2d_color_context_classify_profile(ID2D1ColorContext *iface)
{
    static const BYTE scrgb_id[16] = {0xb0,0xd4,0xc7,0x85,0xae,0x7e,0x20,0xd0,
            0xc6,0x43,0x9c,0x12,0xa1,0x08,0x74,0x10};
    struct d2d_color_context *context = impl_from_ID2D1ColorContext1((ID2D1ColorContext1 *)iface);

    /* WIC and filename creation recognize the sRGB model and the Windows
     * scRGB profile ID. Memory creation deliberately retains CUSTOM. */
    if (!memcmp(context->profile + 52, "sRGB", 4))
        d2d_color_context_set_space(context, D2D1_COLOR_SPACE_SRGB);
    else if (!memcmp(context->profile + 84, scrgb_id, sizeof(scrgb_id)))
        d2d_color_context_set_space(context, D2D1_COLOR_SPACE_SCRGB);
}

/* Built-in profiles are generated from the published primaries and transfer
 * functions. The ICC serialization differs from the Windows system profiles.
 * Keep the immutable serialization for reuse by subsequent context objects. */
static struct builtin_profile
{
    INIT_ONCE once;
    BYTE *data;
    cmsUInt32Number size;
} builtin_profiles[3];

static BOOL CALLBACK d2d_color_context_init_profile(INIT_ONCE *once, void *parameter, void **out)
{
    unsigned int index = (ULONG_PTR)parameter;
    struct builtin_profile *profile = &builtin_profiles[index];
    cmsCIExyY white = {0.3127, 0.3290, 1.0};
    cmsCIExyYTRIPLE primaries = {{0.64, 0.33, 1.0}, {0.30, 0.60, 1.0}, {0.15, 0.06, 1.0}};
    cmsToneCurve *curve, *curves[3];
    cmsHPROFILE handle;
    BOOL ret = FALSE;

    if (!index)
    {
        cmsUInt16Number values[1024];
        unsigned int i;

        if (!(handle = cmsCreate_sRGBProfile())) return FALSE;
        /* The Windows sRGB ICC profile uses sampled, bounded TRCs. Keep
         * that behavior for values outside the nominal 0..1 range too. */
        for (i = 0; i < ARRAY_SIZE(values); ++i)
        {
            double x = (double)i / (ARRAY_SIZE(values) - 1);
            double linear = x <= 0.04045 ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4);
            values[i] = floor(linear * 65535.0 + 0.5);
        }
        curve = cmsBuildTabulatedToneCurve16(NULL, ARRAY_SIZE(values), values);
        if (!curve || !cmsWriteTag(handle, cmsSigRedTRCTag, curve)
                || !cmsLinkTag(handle, cmsSigGreenTRCTag, cmsSigRedTRCTag)
                || !cmsLinkTag(handle, cmsSigBlueTRCTag, cmsSigRedTRCTag))
        {
            if (curve) cmsFreeToneCurve(curve);
            cmsCloseProfile(handle);
            return FALSE;
        }
        cmsFreeToneCurve(curve);
        cmsSetHeaderModel(handle, 0x73524742); /* sRGB */
    }
    else
    {
        if (index == 2)
        {
            primaries.Green.x = 0.21;
            primaries.Green.y = 0.71;
        }
        if (!(curve = cmsBuildGamma(NULL, index == 1 ? 1.0 : 563.0 / 256.0))) return FALSE;
        curves[0] = curves[1] = curves[2] = curve;
        handle = cmsCreateRGBProfile(&white, &primaries, curves);
        cmsFreeToneCurve(curve);
        if (!handle) return FALSE;
        if (index == 1) cmsSetHeaderModel(handle, 0x73635247); /* scRG */
    }
    if (cmsSaveProfileToMem(handle, NULL, &profile->size) && (profile->data = malloc(profile->size)))
    {
        ret = cmsSaveProfileToMem(handle, profile->data, &profile->size);
        if (!ret) { free(profile->data); profile->data = NULL; }
    }
    cmsCloseProfile(handle);
    return ret;
}

static HRESULT d2d_color_context_create_builtin(ID2D1Factory *factory, unsigned int index,
        ID2D1ColorContext **out)
{
    struct builtin_profile *profile = &builtin_profiles[index];
    struct d2d_color_context *context;

    if (!InitOnceExecuteOnce(&profile->once, d2d_color_context_init_profile, (void *)(ULONG_PTR)index, NULL))
        return E_OUTOFMEMORY;
    if (!(context = d2d_color_context_alloc(factory, D2D1_COLOR_CONTEXT_TYPE_ICC)))
        return E_OUTOFMEMORY;
    if (!(context->profile = malloc(profile->size)))
    {
        ID2D1ColorContext1_Release(&context->ID2D1ColorContext1_iface);
        return E_OUTOFMEMORY;
    }
    memcpy(context->profile, profile->data, profile->size);
    context->profile_size = profile->size;
    d2d_color_context_set_space(context, index == 0 ? D2D1_COLOR_SPACE_SRGB
            : index == 1 ? D2D1_COLOR_SPACE_SCRGB : D2D1_COLOR_SPACE_CUSTOM);
    *out = (ID2D1ColorContext *)&context->ID2D1ColorContext1_iface;
    return S_OK;
}

HRESULT d2d_color_context_create(ID2D1Factory *factory, D2D1_COLOR_SPACE space,
        const BYTE *profile, UINT32 size, ID2D1ColorContext **out)
{
    struct d2d_color_context *context;
    cmsHPROFILE handle;

    if (!out) return E_INVALIDARG;
    *out = NULL;
    if (space == D2D1_COLOR_SPACE_SRGB || space == D2D1_COLOR_SPACE_SCRGB)
        return d2d_color_context_create_builtin(factory, space - 1, out);
    if (space != D2D1_COLOR_SPACE_CUSTOM) return E_INVALIDARG;
    if (!profile || size < 128 || !(handle = cmsOpenProfileFromMem(profile, size)))
        return HRESULT_FROM_WIN32(ERROR_INVALID_PROFILE);
    if (!cmsGetDeviceClass(handle) || !cmsChannelsOfColorSpace(cmsGetColorSpace(handle))
            || cmsGetProfileVersion(handle) < 2.0 || cmsGetProfileVersion(handle) >= 5.0)
    {
        cmsCloseProfile(handle);
        return HRESULT_FROM_WIN32(ERROR_INVALID_PROFILE);
    }
    cmsCloseProfile(handle);
    if (!(context = d2d_color_context_alloc(factory, D2D1_COLOR_CONTEXT_TYPE_ICC)))
        return E_OUTOFMEMORY;
    if (!(context->profile = malloc(size)))
    {
        ID2D1ColorContext1_Release(&context->ID2D1ColorContext1_iface);
        return E_OUTOFMEMORY;
    }
    memcpy(context->profile, profile, size);
    context->profile_size = size;
    d2d_color_context_set_space(context, D2D1_COLOR_SPACE_CUSTOM);
    *out = (ID2D1ColorContext *)&context->ID2D1ColorContext1_iface;
    return S_OK;
}

HRESULT d2d_color_context_create_dxgi(ID2D1Factory *factory, DXGI_COLOR_SPACE_TYPE space, ID2D1ColorContext1 **out)
{
    struct d2d_color_context *context;

    if (!out) return E_INVALIDARG;
    *out = NULL;
    switch (space)
    {
        case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709:
        case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
        case DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709:
        case DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P2020:
        case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
        case DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020:
        case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020:
            break;
        default:
            return E_INVALIDARG;
    }
    if (!(context = d2d_color_context_alloc(factory, D2D1_COLOR_CONTEXT_TYPE_DXGI)))
        return E_OUTOFMEMORY;
    context->dxgi_space = space;
    context->space = space == DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 ? D2D1_COLOR_SPACE_SRGB
            : space == DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709 ? D2D1_COLOR_SPACE_SCRGB : D2D1_COLOR_SPACE_CUSTOM;
    *out = &context->ID2D1ColorContext1_iface;
    return S_OK;
}

HRESULT d2d_color_context_create_simple(ID2D1Factory *factory, const D2D1_SIMPLE_COLOR_PROFILE *profile,
        ID2D1ColorContext1 **out)
{
    struct d2d_color_context *context;

    if (!out) return E_INVALIDARG;
    *out = NULL;
    if (!profile) return E_INVALIDARG;
    if (!(context = d2d_color_context_alloc(factory, D2D1_COLOR_CONTEXT_TYPE_SIMPLE)))
        return E_OUTOFMEMORY;
    context->simple = *profile;
    *out = &context->ID2D1ColorContext1_iface;
    return S_OK;
}

HRESULT d2d_color_context_create_wic(ID2D1Factory *factory, IWICColorContext *wic, ID2D1ColorContext **out)
{
    WICColorContextType type;
    BYTE *profile;
    UINT size, actual, exif;
    HRESULT hr;

    if (!out) return E_INVALIDARG;
    *out = NULL;
    if (!wic) return E_INVALIDARG;
    if (FAILED(hr = IWICColorContext_GetType(wic, &type))) return hr;
    if (type == WICColorContextExifColorSpace)
    {
        if (FAILED(hr = IWICColorContext_GetExifColorSpace(wic, &exif))) return hr;
        if (exif != 0 && exif != 1 && exif != 2 && exif != 0xffff) return E_INVALIDARG;
        return d2d_color_context_create_builtin(factory, exif == 2 ? 2 : 0, out);
    }
    if (type != WICColorContextProfile) return E_INVALIDARG;
    if (FAILED(hr = IWICColorContext_GetProfileBytes(wic, 0, NULL, &size))) return hr;
    if (!(profile = malloc(size))) return E_OUTOFMEMORY;
    if (SUCCEEDED(hr = IWICColorContext_GetProfileBytes(wic, size, profile, &actual)))
        hr = actual > size ? E_INVALIDARG : d2d_color_context_create(factory, D2D1_COLOR_SPACE_CUSTOM, profile, actual, out);
    free(profile);
    if (SUCCEEDED(hr)) d2d_color_context_classify_profile(*out);
    return hr;
}

HRESULT d2d_color_context_create_filename(ID2D1Factory *factory, const WCHAR *filename, ID2D1ColorContext **out)
{
    WCHAR *path = NULL;
    HANDLE handle;
    LARGE_INTEGER file_size;
    BYTE *data;
    DWORD size, count;
    HRESULT hr;

    if (!out) return E_INVALIDARG;
    *out = NULL;
    if (!filename) return E_INVALIDARG;
    if (PathIsRelativeW(filename))
    {
        size = 0;
        GetColorDirectoryW(NULL, NULL, &size);
        if (!size) return HRESULT_FROM_WIN32(GetLastError());
        if (!(path = malloc(size + (wcslen(filename) + 2) * sizeof(WCHAR)))) return E_OUTOFMEMORY;
        if (!GetColorDirectoryW(NULL, path, &size))
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            free(path);
            return hr;
        }
        wcscat(path, L"\\");
        wcscat(path, filename);
        filename = path;
    }
    handle = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    free(path);
    if (handle == INVALID_HANDLE_VALUE)
        return HRESULT_FROM_WIN32(GetLastError());
    if (!GetFileSizeEx(handle, &file_size)) hr = HRESULT_FROM_WIN32(GetLastError());
    else if (file_size.QuadPart < 128) hr = HRESULT_FROM_WIN32(ERROR_INVALID_PROFILE);
    else if (file_size.QuadPart > UINT_MAX) hr = E_OUTOFMEMORY;
    else
    {
        size = file_size.QuadPart;
        if (!(data = malloc(size))) hr = E_OUTOFMEMORY;
        else
        {
            if (!ReadFile(handle, data, size, &count, NULL)) hr = HRESULT_FROM_WIN32(GetLastError());
            else if (count != size) hr = HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
            else hr = d2d_color_context_create(factory, D2D1_COLOR_SPACE_CUSTOM, data, size, out);
            free(data);
        }
    }
    CloseHandle(handle);
    if (SUCCEEDED(hr)) d2d_color_context_classify_profile(*out);
    return hr;
}

void d2d_color_context_cleanup(void)
{
    unsigned int i;
    for (i = 0; i < ARRAY_SIZE(builtin_profiles); ++i)
        free(builtin_profiles[i].data);
}
