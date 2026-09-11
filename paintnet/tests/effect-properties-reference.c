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
