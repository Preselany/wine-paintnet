/* Public compiler/reflection measurements, preserving bytecode for cross-host checks.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "windows.h"
#include "initguid.h"
#include "d3dcompiler.h"
#include "d3d11shader.h"

static const struct { const char *name, *profile, *source; } cases[] =
{
    {"pixel40","ps_4_0","float4 main(float4 c:COLOR):SV_Target{return c;}"},
    {"pixel41","ps_4_1","float4 main(float4 c:COLOR):SV_Target{return c;}"},
    {"pixel50","ps_5_0","float4 main(float4 c:COLOR):SV_Target{return c;}"},
    {"vertex40","vs_4_0","float4 main(float4 p:POSITION):SV_Position{return p;}"},
    {"vertex41","vs_4_1","float4 main(float4 p:POSITION):SV_Position{return p;}"},
    {"vertex50","vs_5_0","float4 main(float4 p:POSITION):SV_Position{return p;}"},
    {"pixel91","ps_4_0_level_9_1","float4 main(float4 c:COLOR):SV_Target{return c;}"},
    {"pixel93","ps_4_0_level_9_3","float4 main(float4 c:COLOR):SV_Target{return c;}"},
    {"vertex91","vs_4_0_level_9_1","float4 main(float4 p:POSITION):SV_Position{return p;}"},
    {"vertex93","vs_4_0_level_9_3","float4 main(float4 p:POSITION):SV_Position{return p;}"},
    {"compute40","cs_4_0","RWStructuredBuffer<float> result; [numthreads(1,1,1)]void main(uint3 p:SV_DispatchThreadID){result[p.x]=1;}"},
    {"compute41","cs_4_1","RWStructuredBuffer<float> result; [numthreads(1,1,1)]void main(uint3 p:SV_DispatchThreadID){result[p.x]=1;}"},
    {"compute50","cs_5_0","RWStructuredBuffer<float> result; [numthreads(1,1,1)]void main(uint3 p:SV_DispatchThreadID){result[p.x]=1;}"},
    {"early_depth","ps_5_0","[earlydepthstencil]float4 main(float4 c:COLOR):SV_Target{return c;}"},
    {"double_add","ps_5_0","cbuffer c{double a,b;}float4 main():SV_Target{return (float)(a+b);}"},
    {"double_div","ps_5_0","cbuffer c{double a,b;}float4 main():SV_Target{return (float)(a/b);}"},
    {"min_precision","ps_5_0","float4 main(min16float4 c:COLOR):SV_Target{return c*c;}"},
    {"vertex_uav","vs_5_0","RWStructuredBuffer<float> result; float4 main(float4 p:POSITION):SV_Position{result[0]=p.x;return p;}"},
};

int main(int argc, char **argv)
{
    ID3DBlob *code=NULL,*errors=NULL;
    ID3D11ShaderReflection *reflection=NULL;
    D3D11_SHADER_DESC desc;
    D3D_FEATURE_LEVEL level;
    HRESULT hr,lh;
    unsigned int i,j,k,count,offset,size;
    unsigned char *data;
    size_t length;
    char path[1024];
    FILE *file;
    int failed=0;
    for(i=0;i<sizeof(cases)/sizeof(*cases);++i)
    {
        if(argc==2)
        {
            snprintf(path,sizeof(path),"%s/%s.cso.log",argv[1],cases[i].name);
            if(!(file=fopen(path,"rb"))) {printf("{\"name\":\"%s\",\"missing\":true}\n",cases[i].name);failed=1;continue;}
            fseek(file,0,SEEK_END);length=ftell(file);rewind(file);
            data=malloc(length); if(!data){fclose(file);return 1;}
            if(fread(data,1,length,file)!=length){free(data);fclose(file);return 1;}fclose(file);
        }
        else
        {
            hr=D3DCompile(cases[i].source,strlen(cases[i].source),NULL,NULL,NULL,"main",cases[i].profile,
                    D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&errors);
            if(FAILED(hr))
            {
                printf("{\"name\":\"%s\",\"compile_hr\":%lu}\n",cases[i].name,(unsigned long)hr);
                if(errors){fprintf(stderr,"%s: %s\n",cases[i].name,(char *)ID3D10Blob_GetBufferPointer(errors));ID3D10Blob_Release(errors);errors=NULL;}
                failed=1;continue;
            }
            length=ID3D10Blob_GetBufferSize(code);data=ID3D10Blob_GetBufferPointer(code);
            snprintf(path,sizeof(path),"%s.cso.log",cases[i].name);
            if(!(file=fopen(path,"wb")))return 1;
            if(fwrite(data,1,length,file)!=length){fclose(file);return 1;}fclose(file);
        }
        hr=D3DReflect(data,length,&IID_ID3D11ShaderReflection,(void **)&reflection);
        if(FAILED(hr)){printf("{\"name\":\"%s\",\"reflect_hr\":%lu}\n",cases[i].name,(unsigned long)hr);failed=1;goto next;}
        memset(&desc,0,sizeof(desc));hr=reflection->lpVtbl->GetDesc(reflection,&desc);
        level=0;lh=reflection->lpVtbl->GetMinFeatureLevel(reflection,&level);
        printf("{\"name\":\"%s\",\"profile\":\"%s\",\"desc_hr\":%lu,\"version\":%u,\"level_hr\":%lu,\"level\":%u,\"requires\":%llu,\"size\":%zu}\n",
                cases[i].name,cases[i].profile,(unsigned long)hr,desc.Version,(unsigned long)lh,level,
                reflection->lpVtbl->GetRequiresFlags(reflection),length);
        if(FAILED(hr)||FAILED(lh))failed=1;
        if(argc==1 && length>=32)
        {
            memcpy(&count,data+28,4);
            for(j=0;j<count && 32+4*j+4<=length;++j)
            {
                memcpy(&offset,data+32+4*j,4);if(offset+8>length)continue;
                memcpy(&size,data+offset+4,4);if(offset+8+size>length)continue;
                printf("{\"name\":\"%s\",\"section\":\"%.4s\",\"size\":%u,\"prefix\":\"",cases[i].name,data+offset,size);
                for(k=0;k<size && k<32;++k)printf("%02x",data[offset+8+k]);
                printf("\"}\n");
            }
        }
        reflection->lpVtbl->Release(reflection);reflection=NULL;
next:
        if(argc==2)free(data);
        if(code){ID3D10Blob_Release(code);code=NULL;}
        if(errors){ID3D10Blob_Release(errors);errors=NULL;}
    }
    return failed;
}
