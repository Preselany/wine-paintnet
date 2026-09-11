/* Public-API effect metadata measurements, with no replacement implementation.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <stdio.h>
#include "d2d1_1.h"
#include "d3d11.h"

static void quoted(const WCHAR *text)
{
    putchar('"');
    for (; *text; ++text)
    {
        if (*text == '"' || *text == '\\') printf("\\%c", *text);
        else if (*text >= 32 && *text < 127) putchar(*text);
        else printf("\\u%04x", *text);
    }
    putchar('"');
}

static void properties(ID2D1Properties *object, const WCHAR *guid, const char *parent, unsigned int depth)
{
    static const UINT32 system_indices[] = {D2D1_PROPERTY_CLSID, D2D1_PROPERTY_DISPLAYNAME,
            D2D1_PROPERTY_AUTHOR, D2D1_PROPERTY_CATEGORY, D2D1_PROPERTY_DESCRIPTION,
            D2D1_PROPERTY_INPUTS, D2D1_PROPERTY_CACHED, D2D1_PROPERTY_PRECISION,
            D2D1_PROPERTY_MIN_INPUTS, D2D1_PROPERTY_MAX_INPUTS};
    UINT32 count = ID2D1Properties_GetPropertyCount(object), index, size, i, j, total;
    ID2D1Properties *sub;
    D2D1_PROPERTY_TYPE type;
    WCHAR name[256];
    BYTE data[4096];
    char path[128];
    HRESULT hr;

    if (count > 256 || depth > 3) return;
    total = count + (depth ? 7 : sizeof(system_indices)/sizeof(system_indices[0]));
    for (i = 0; i < total; ++i)
    {
        index = i < count ? i : depth ? D2D1_SUBPROPERTY_DISPLAYNAME+i-count : system_indices[i-count];
        name[0] = 0;
        ID2D1Properties_GetPropertyName(object,index,name,sizeof(name)/sizeof(name[0]));
        type = ID2D1Properties_GetType(object,index);
        if (type == D2D1_PROPERTY_TYPE_UNKNOWN) continue;
        size = ID2D1Properties_GetValueSize(object,index);
        if (size > sizeof(data)) hr = E_NOT_SUFFICIENT_BUFFER;
        else hr = ID2D1Properties_GetValue(object,index,type,data,size);
        printf("{\"effect\":"); quoted(guid);
        printf(",\"parent\":\"%s\",\"index\":%u,\"name\":",parent,index); quoted(name);
        printf(",\"type\":%u,\"size\":%u,\"hr\":%ld,\"hex\":\"",type,size,hr);
        if (SUCCEEDED(hr)) for (j = 0; j < size; ++j) printf("%02x",data[j]);
        puts("\"}");
        if (SUCCEEDED(ID2D1Properties_GetSubProperties(object,index,&sub)))
        {
            snprintf(path,sizeof(path),"%s/%u",parent,index);
            properties(sub,guid,path,depth+1);
            ID2D1Properties_Release(sub);
        }
    }
}

static void numeric_validation(ID2D1Effect *effect, const WCHAR *guid)
{
    UINT32 index, count = ID2D1Effect_GetPropertyCount(effect), size, i, j, channel;
    D2D1_PROPERTY_TYPE type;
    union { float f[2]; UINT32 u[2]; BYTE bytes[8]; } original, input, output;
    HRESULT set_hr, get_hr;

    for (index = 0; index < count; ++index)
    {
        type = ID2D1Effect_GetType(effect,index);
        if (type != D2D1_PROPERTY_TYPE_FLOAT && type != D2D1_PROPERTY_TYPE_VECTOR2
                && type != D2D1_PROPERTY_TYPE_UINT32) continue;
        size = ID2D1Effect_GetValueSize(effect,index);
        if (size > sizeof(original) || FAILED(ID2D1Effect_GetValue(effect,index,type,original.bytes,size))) continue;
        for (i = 0; i < 4; ++i)
        {
            static const float samples[] = {-1,0,1,2};
            input = original;
            for (channel = 0; channel < size/4; ++channel)
                if (type == D2D1_PROPERTY_TYPE_UINT32) input.u[channel] = i;
                else input.f[channel] = samples[i];
            set_hr = ID2D1Effect_SetValue(effect,index,type,input.bytes,size);
            memset(&output,0,sizeof(output));
            get_hr = ID2D1Effect_GetValue(effect,index,type,output.bytes,size);
            printf("{\"effect\":"); quoted(guid);
            printf(",\"parent\":\"validation\",\"index\":%u,\"type\":%u,\"set_hr\":%ld,\"get_hr\":%ld,\"input\":\"",
                    index,type,set_hr,get_hr);
            for (j = 0; j < size; ++j) printf("%02x",input.bytes[j]);
            printf("\",\"output\":\"");
            for (j = 0; j < size; ++j) printf("%02x",output.bytes[j]);
            puts("\"}");
            ID2D1Effect_SetValue(effect,index,type,original.bytes,size);
        }
    }
}

static void convolution_geometry(ID2D1DeviceContext *context, ID2D1Effect *effect, const WCHAR *guid)
{
    D2D1_BITMAP_PROPERTIES1 desc = {{DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},96,96,0,NULL};
    D2D1_SIZE_U size = {3,2}, target_size = {5,4};
    D2D1_RECT_F bounds = {0};
    D2D1_RECT_F crop = {-1,-1,4,3};
    D2D1_POINT_2F offset = {0};
    D2D1_COLOR_F clear = {0};
    D2D1_MAPPED_RECT mapped;
    ID2D1Bitmap1 *bitmap, *target = NULL, *readback = NULL;
    ID2D1Image *image;
    float pixels[6][4] = {{0}}, matrix[16];
    UINT32 i, j, x, y, width = 3, bytes;
    HRESULT hr, set_hr, draw_hr;
    for (i = 0; i < 6; ++i)
    {
        pixels[i][0] = pixels[i][1] = pixels[i][2] = (i+1)/10.0f;
        pixels[i][3] = 1;
    }
    if (FAILED(ID2D1DeviceContext_CreateBitmap(context,size,pixels,3*16,&desc,&bitmap))) return;
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
    if (FAILED(ID2D1DeviceContext_CreateBitmap(context,target_size,NULL,0,&desc,&target))) goto done;
    desc.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    if (FAILED(ID2D1DeviceContext_CreateBitmap(context,target_size,NULL,0,&desc,&readback))) goto done;
    ID2D1DeviceContext_SetTarget(context,(ID2D1Image *)target);
    ID2D1Effect_SetInput(effect,0,(ID2D1Image *)bitmap,TRUE);
    ID2D1Effect_SetValue(effect,2,D2D1_PROPERTY_TYPE_UINT32,(const BYTE *)&width,sizeof(width));
    ID2D1Effect_SetValue(effect,3,D2D1_PROPERTY_TYPE_UINT32,(const BYTE *)&width,sizeof(width));
    ID2D1Effect_GetOutput(effect,&image);
    for (i = 0; i < 11; ++i)
    {
        for (j = 0; j < 9; ++j) matrix[j] = i == 9 || i == j ? 1 : 0;
        set_hr = ID2D1Effect_SetValue(effect,4,D2D1_PROPERTY_TYPE_BLOB,(const BYTE *)matrix,9*sizeof(float));
        hr = ID2D1DeviceContext_GetImageLocalBounds(context,image,&bounds);
        ID2D1DeviceContext_BeginDraw(context);
        ID2D1DeviceContext_Clear(context,&clear);
        ID2D1DeviceContext_DrawImage(context,image,&offset,&crop,D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,D2D1_COMPOSITE_MODE_SOURCE_OVER);
        draw_hr = ID2D1DeviceContext_EndDraw(context,NULL,NULL);
        printf("{\"effect\":"); quoted(guid);
        printf(",\"parent\":\"geometry\",\"kernel\":%u,\"set_hr\":%ld,\"hr\":%ld,\"bounds\":[%g,%g,%g,%g],\"draw_hr\":%ld,\"pixels\":[",
                i,set_hr,hr,bounds.left,bounds.top,bounds.right,bounds.bottom,draw_hr);
        if (SUCCEEDED(ID2D1Bitmap1_CopyFromBitmap(readback,NULL,(ID2D1Bitmap *)target,NULL))
                && SUCCEEDED(ID2D1Bitmap1_Map(readback,D2D1_MAP_OPTIONS_READ,&mapped)))
        {
            for (y = 0; y < 4; ++y) for (x = 0; x < 5; ++x)
            {
                const float *pixel = (const float *)(mapped.bits+y*mapped.pitch+x*16);
                printf("%s[%g,%g]",x || y ? "," : "",pixel[0],pixel[3]);
            }
            ID2D1Bitmap1_Unmap(readback);
        }
        puts("]}");
    }
    for (i = 0; i <= 10; ++i)
    {
        for (j = 0; j < 16; ++j) matrix[j] = j+1;
        set_hr = ID2D1Effect_SetValue(effect,4,D2D1_PROPERTY_TYPE_BLOB,(const BYTE *)matrix,i*sizeof(float));
        bytes = ID2D1Effect_GetValueSize(effect,4);
        printf("{\"effect\":"); quoted(guid);
        printf(",\"parent\":\"kernel-size\",\"count\":%u,\"set_hr\":%ld,\"bytes\":%u}\n",i,set_hr,bytes);
    }
    ID2D1Image_Release(image);
    ID2D1Effect_SetInput(effect,0,NULL,TRUE);
done:
    ID2D1DeviceContext_SetTarget(context,NULL);
    if (readback) ID2D1Bitmap1_Release(readback);
    if (target) ID2D1Bitmap1_Release(target);
    ID2D1Bitmap1_Release(bitmap);
}

int main(void)
{
    ID2D1Factory1 *factory;
    ID2D1Properties *object;
    ID3D11Device *d3d = NULL;
    IDXGIDevice *dxgi = NULL;
    ID2D1Device *device = NULL;
    ID2D1DeviceContext *context = NULL;
    ID2D1Effect *effect;
    CLSID effects[256];
    UINT32 returned = 0, registered = 0, i;
    WCHAR guid[40];
    HRESULT hr;

    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory);
    if (FAILED(hr)) { fprintf(stderr,"Factory: %#lx\n",hr); CoUninitialize(); return 1; }
    hr = D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_WARP,NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            NULL,0,D3D11_SDK_VERSION,&d3d,NULL,NULL);
    if (SUCCEEDED(hr)) hr = ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi);
    if (SUCCEEDED(hr)) hr = ID2D1Factory1_CreateDevice(factory,dxgi,&device);
    if (SUCCEEDED(hr)) hr = ID2D1Device_CreateDeviceContext(device,0,&context);
    if (FAILED(hr)) fprintf(stderr,"Live-property context: %#lx\n",hr);
    hr = ID2D1Factory1_GetRegisteredEffects(factory,effects,256,&returned,&registered);
    fprintf(stderr,"Registered effects: %u, returned: %u, hr: %#lx\n",registered,returned,hr);
    if (SUCCEEDED(hr)) for (i = 0; i < returned; ++i)
    {
        StringFromGUID2(&effects[i],guid,40);
        hr = ID2D1Factory1_GetEffectProperties(factory,&effects[i],&object);
        if (FAILED(hr)) { fprintf(stderr,"Metadata: %#lx\n",hr); continue; }
        properties(object,guid,"",0);
        ID2D1Properties_Release(object);
        if (context && SUCCEEDED(ID2D1DeviceContext_CreateEffect(context,&effects[i],&effect)))
        {
            properties((ID2D1Properties *)effect,guid,"live",0);
            switch (effects[i].Data1)
            {
                case 0xb1c5eb2b: case 0xb648a78a: case 0x407f8c08:
                case 0x5fb6c24d: case 0x881db7d0:
                    numeric_validation(effect,guid);
            }
            if (effects[i].Data1 == 0x407f8c08) convolution_geometry(context,effect,guid);
            ID2D1Effect_Release(effect);
        }
    }
    if (context) ID2D1DeviceContext_Release(context);
    if (device) ID2D1Device_Release(device);
    if (dxgi) IDXGIDevice_Release(dxgi);
    if (d3d) ID3D11Device_Release(d3d);
    ID2D1Factory1_Release(factory);
    CoUninitialize();
    return FAILED(hr);
}
