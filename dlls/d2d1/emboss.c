/* Direct2D grayscale surface lighting.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "d2d1_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d2d);

struct emboss_effect
{
    ID2D1EffectImpl ID2D1EffectImpl_iface;
    LONG refcount;
    ID3D11ComputeShader *shader, *scale_shader;
    float height, direction;
};

static const char emboss_shader[] =
    "Texture2D<float4> image : register(t0);\n"
    "RWTexture2D<float4> output_image : register(u0);\n"
    "cbuffer params : register(b0) { uint2 extent; uint2 input_size; float2 scale; float height; float pad0;\n"
    " float3 light; float pad1; float4 gradient[81]; };\n"
    "float gray(int2 p) {\n"
    " p = clamp(p, int2(0,0), int2(input_size)-1);\n"
    " float4 color = image.Load(int3(p,0));\n"
    " return color.a != 0 ? dot(color.rgb/color.a, float3(0.299,0.587,0.114)) : 0;\n"
    "}\n"
    "float sample_gray(int2 p) {\n"
    " p = clamp(p, int2(0,0), int2(extent)-1);\n"
    " float2 source = (float2(p)+0.5)*scale-0.5;\n"
    " int2 base = int2(floor(source)); float2 f = frac(source);\n"
    " if (f.x == 0 && f.y == 0) return gray(base);\n"
    " return lerp(lerp(gray(base),gray(base+int2(1,0)),f.x),\n"
    "             lerp(gray(base+int2(0,1)),gray(base+int2(1,1)),f.x),f.y);\n"
    "}\n"
    "[numthreads(16,16,1)] void main(uint3 tid : SV_DispatchThreadID) {\n"
    " if (tid.x >= extent.x || tid.y >= extent.y) return;\n"
    " uint edge_x = tid.x == 0 ? 0 : tid.x == extent.x-1 ? 2 : 1;\n"
    " uint edge_y = tid.y == 0 ? 0 : tid.y == extent.y-1 ? 2 : 1;\n"
    " uint kernel = (edge_y*3+edge_x)*9; float2 slope = 0;\n"
    " for (int y = -1; y <= 1; ++y) for (int x = -1; x <= 1; ++x)\n"
    "  slope += sample_gray(int2(tid.xy)+int2(x,y))*gradient[kernel+(y+1)*3+x+1].xy;\n"
    " float3 normal = normalize(float3(-height*slope,1));\n"
    " float value = max(0,dot(normal,light));\n"
    " output_image[tid.xy] = float4(value,value,value,1);\n"
    "}\n";

static const char emboss_scale_shader[] =
    "Texture2D<float4> image : register(t0);\n"
    "RWTexture2D<float4> output_image : register(u0);\n"
    "cbuffer params : register(b0) { uint2 extent; uint2 input_size; float2 scale; float2 pad; };\n"
    "float4 fetch(int2 p) { return image.Load(int3(clamp(p,int2(0,0),int2(input_size)-1),0)); }\n"
    "[numthreads(16,16,1)] void main(uint3 tid : SV_DispatchThreadID) {\n"
    " if (tid.x >= extent.x || tid.y >= extent.y) return;\n"
    " float2 p = (float2(tid.xy)+0.5)/scale-0.5; int2 base = int2(floor(p)); float2 f = frac(p);\n"
    " output_image[tid.xy] = lerp(lerp(fetch(base),fetch(base+int2(1,0)),f.x),\n"
    "                           lerp(fetch(base+int2(0,1)),fetch(base+int2(1,1)),f.x),f.y);\n"
    "}\n";

static struct emboss_effect *impl_from_ID2D1EffectImpl(ID2D1EffectImpl *iface)
{
    return CONTAINING_RECORD(iface, struct emboss_effect, ID2D1EffectImpl_iface);
}

static HRESULT STDMETHODCALLTYPE emboss_QueryInterface(ID2D1EffectImpl *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_ID2D1EffectImpl) && !IsEqualGUID(iid, &IID_IUnknown)) return E_NOINTERFACE;
    *out = iface;
    ID2D1EffectImpl_AddRef(iface);
    return S_OK;
}

static ULONG STDMETHODCALLTYPE emboss_AddRef(ID2D1EffectImpl *iface)
{
    return InterlockedIncrement(&impl_from_ID2D1EffectImpl(iface)->refcount);
}

static ULONG STDMETHODCALLTYPE emboss_Release(ID2D1EffectImpl *iface)
{
    struct emboss_effect *effect = impl_from_ID2D1EffectImpl(iface);
    ULONG refcount = InterlockedDecrement(&effect->refcount);
    if (!refcount)
    {
        if (effect->shader) ID3D11ComputeShader_Release(effect->shader);
        if (effect->scale_shader) ID3D11ComputeShader_Release(effect->scale_shader);
        free(effect);
    }
    return refcount;
}

static HRESULT STDMETHODCALLTYPE emboss_Initialize(ID2D1EffectImpl *iface, ID2D1EffectContext *context,
        ID2D1TransformGraph *graph)
{
    struct emboss_effect *effect = impl_from_ID2D1EffectImpl(iface);
    struct d2d_effect_context *effect_context = CONTAINING_RECORD(context,
            struct d2d_effect_context, ID2D1EffectContext1_iface);
    ID3D11Device1 *device = effect_context->device_context->d3d_device;

    HRESULT hr;
    if (FAILED(hr = d2d_effect_compile_compute_shader(device, emboss_shader, &effect->shader))) return hr;
    return d2d_effect_compile_compute_shader(device, emboss_scale_shader, &effect->scale_shader);
}

static HRESULT STDMETHODCALLTYPE emboss_PrepareForRender(ID2D1EffectImpl *iface, D2D1_CHANGE_TYPE change)
{
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE emboss_SetGraph(ID2D1EffectImpl *iface, ID2D1TransformGraph *graph)
{
    return E_NOTIMPL;
}

static const ID2D1EffectImplVtbl emboss_vtbl =
{
    emboss_QueryInterface, emboss_AddRef, emboss_Release,
    emboss_Initialize, emboss_PrepareForRender, emboss_SetGraph,
};

static HRESULT CALLBACK emboss_factory(IUnknown **out)
{
    struct emboss_effect *effect = calloc(1, sizeof(*effect));
    if (!effect) return E_OUTOFMEMORY;
    effect->ID2D1EffectImpl_iface.lpVtbl = &emboss_vtbl;
    effect->refcount = 1;
    effect->height = 1;
    *out = (IUnknown *)&effect->ID2D1EffectImpl_iface;
    return S_OK;
}


static HRESULT property_get(const void *value, UINT size, BYTE *data, UINT data_size, UINT *actual)
{
    if (actual) *actual = size;
    if (!data) return S_OK;
    if (data_size < size) return E_NOT_SUFFICIENT_BUFFER;
    memcpy(data, value, size);
    return S_OK;
}

#define FLOAT_PROPERTY(name, minimum, maximum) \
static HRESULT CALLBACK name##_get(const IUnknown *iface, BYTE *data, UINT size, UINT *actual) \
{ \
    const struct emboss_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface); \
    return property_get(&effect->name, sizeof(effect->name), data, size, actual); \
} \
static HRESULT CALLBACK name##_set(IUnknown *iface, const BYTE *data, UINT size) \
{ \
    struct emboss_effect *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface); \
    float value; \
    if (!data || size != sizeof(value)) return E_INVALIDARG; \
    memcpy(&value, data, size); \
    if (isnan(value)) return E_INVALIDARG; \
    effect->name = min(max(value, minimum), maximum); \
    return S_OK; \
}
FLOAT_PROPERTY(height, 0, 10)
FLOAT_PROPERTY(direction, 0, 360)
#undef FLOAT_PROPERTY

void d2d_emboss_init_builtin(struct d2d_factory *factory)
{
    static const WCHAR description[] = L"<?xml version='1.0'?><Effect>"
        L"<Property name='DisplayName' type='string' value='Emboss'/>"
        L"<Property name='Author' type='string' value='The Wine Project'/>"
        L"<Property name='Category' type='string' value='Stylize'/>"
        L"<Property name='Description' type='string' value='Lights a surface derived from image luminance'/>"
        L"<Inputs minimum='1' maximum='1'><Input name='Source'/></Inputs>"
        L"<Property name='Height' type='float' value='1'/>"
        L"<Property name='Direction' type='float' value='0'/></Effect>";
    static const D2D1_PROPERTY_BINDING bindings[] =
    {
        {L"Height", height_set, height_get},
        {L"Direction", direction_set, direction_get},
    };
    HRESULT hr;
    if (FAILED(hr = d2d_factory_register_builtin_effect(factory, &CLSID_D2D1Emboss,
            description, bindings, ARRAY_SIZE(bindings), emboss_factory)))
        WARN("Failed to register Emboss, hr %#lx.\n", hr);
}

/* Coefficients measured through the public Windows/WARP API on independent
 * 5x5 and 8x8 impulse bases. Units are 1/1024. Each pair is the X/Y derivative;
 * groups select top/interior/bottom and left/interior/right boundary behavior.
 * See paintnet/REFERENCE.md. No Microsoft implementation code is used. */
static const short emboss_gradient[81][2] =
{
    {0,0}, {0,0}, {0,0}, {0,0}, {-1376,-1376}, {1376,-672}, {0,0}, {-672,1376}, {672,672},
    {0,0}, {0,0}, {0,0}, {-688,-510}, {0,-1026}, {688,-512}, {-336,510}, {0,1026}, {336,512},
    {0,0}, {0,0}, {0,0}, {-1376,-680}, {1376,-1368}, {0,0}, {-672,680}, {672,1368}, {0,0},
    {0,0}, {-512,-688}, {512,-336}, {0,0}, {-1032,0}, {1032,0}, {0,0}, {-504,688}, {504,336},
    {-255,-255}, {0,-513}, {256,-256}, {-513,0}, {0,0}, {516,0}, {-256,256}, {0,516}, {252,252},
    {-510,-340}, {510,-684}, {0,0}, {-1026,0}, {1026,0}, {0,0}, {-512,340}, {512,684}, {0,0},
    {0,0}, {-680,-1376}, {680,-672}, {0,0}, {-1368,1376}, {1368,672}, {0,0}, {0,0}, {0,0},
    {-340,-510}, {0,-1026}, {340,-512}, {-684,510}, {0,1026}, {684,512}, {0,0}, {0,0}, {0,0},
    {-680,-680}, {680,-1368}, {0,0}, {-1368,680}, {1368,1368}, {0,0}, {0,0}, {0,0}, {0,0},
};

static void emboss_scale(const struct d2d_device_context *context, float *x, float *y)
{
    *x = context->drawing_state.unitMode == D2D1_UNIT_MODE_PIXELS ? 1 : context->desc.dpiX/96.0f;
    *y = context->drawing_state.unitMode == D2D1_UNIT_MODE_PIXELS ? 1 : context->desc.dpiY/96.0f;
}

HRESULT d2d_emboss_bounds(struct d2d_device_context *context, const D2D1_RECT_L *input, D2D1_RECT_L *output)
{
    float scale_x, scale_y, width, height;
    double work_width, work_height;
    emboss_scale(context,&scale_x,&scale_y);
    work_width = floor(((double)input->right-input->left)/scale_x);
    work_height = floor(((double)input->bottom-input->top)/scale_y);
    if (!isfinite(work_width) || !isfinite(work_height) || work_width < 0 || work_height < 0
            || work_width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION || work_height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
        return D2DERR_EXCEEDS_MAX_BITMAP_SIZE;
    width = work_width*scale_x;
    height = work_height*scale_y;
    /* The image evaluator currently represents bounds in integral pixels. */
    if (fabsf(width-roundf(width)) > .0001f || fabsf(height-roundf(height)) > .0001f
            || fabsf(input->left/scale_x-roundf(input->left/scale_x)) > .0001f
            || fabsf(input->top/scale_y-roundf(input->top/scale_y)) > .0001f)
        return E_NOTIMPL;
    *output = *input;
    output->right = output->left + (int)roundf(width);
    output->bottom = output->top + (int)roundf(height);
    return S_OK;
}

HRESULT d2d_emboss_render(struct d2d_effect *base, struct d2d_device_context *context,
        const struct d2d_effect_image *input, struct d2d_effect_image *output)
{
    struct emboss_effect *effect = impl_from_ID2D1EffectImpl(base->impl);
    struct d2d_bitmap *bitmap = unsafe_impl_from_ID2D1Bitmap(input->bitmap);
    struct d2d_effect_image intermediate = {0};
    struct
    {
        UINT extent[2], input_size[2];
        float scale[2], height, pad0, light[3], pad1, gradient[81][4];
    } params = {0};
    struct
    {
        UINT extent[2], input_size[2];
        float scale[2], pad[2];
    } resample = {0};
    float angle;
    UINT i;
    HRESULT hr;

    if (base->impl->lpVtbl != &emboss_vtbl) return E_UNEXPECTED;
    emboss_scale(context,&params.scale[0],&params.scale[1]);
    params.extent[0] = floorf(bitmap->pixel_size.width/params.scale[0]);
    params.extent[1] = floorf(bitmap->pixel_size.height/params.scale[1]);
    params.input_size[0] = bitmap->pixel_size.width;
    params.input_size[1] = bitmap->pixel_size.height;
    params.height = effect->height;
    angle = effect->direction * M_PI / 180.0;
    params.light[0] = cosf(angle) * 0.6427876097f;
    params.light[1] = sinf(angle) * 0.6427876097f;
    params.light[2] = 0.7660444431f;
    for (i = 0; i < ARRAY_SIZE(emboss_gradient); ++i)
    {
        params.gradient[i][0] = emboss_gradient[i][0]/1024.0f;
        params.gradient[i][1] = emboss_gradient[i][1]/1024.0f;
    }
    if ((params.scale[0] == 1 && params.scale[1] == 1) || !params.extent[0] || !params.extent[1])
        return d2d_effect_dispatch(context,effect->shader,input,1,&params,sizeof(params),NULL,output);
    intermediate.rect = (D2D1_RECT_L){0,0,params.extent[0],params.extent[1]};
    if (FAILED(hr = d2d_effect_dispatch(context,effect->shader,input,1,&params,sizeof(params),NULL,&intermediate))) return hr;
    resample.extent[0] = output->rect.right-output->rect.left;
    resample.extent[1] = output->rect.bottom-output->rect.top;
    resample.input_size[0] = params.extent[0];
    resample.input_size[1] = params.extent[1];
    resample.scale[0] = params.scale[0];
    resample.scale[1] = params.scale[1];
    hr = d2d_effect_dispatch(context,effect->scale_shader,&intermediate,1,&resample,sizeof(resample),NULL,output);
    ID2D1Bitmap_Release(intermediate.bitmap);
    return hr;
}
