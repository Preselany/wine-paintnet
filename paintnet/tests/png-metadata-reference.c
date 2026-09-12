/* Native PNG metadata block writer and serialization reference.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <stdio.h>
#include <stdlib.h>
#include "windows.h"
#include "initguid.h"
#include "wincodec.h"
#include "wincodecsdk.h"
#define REQUIRE(call) do {hr=(call);if(FAILED(hr)){printf("FAILED %u %08lx\n",__LINE__,hr);return 1;}}while(0)
static DWORD crc32(const BYTE *p, unsigned int size)
{
    DWORD crc=~0u;unsigned int i,b;
    for(i=0;i<size;++i){crc^=p[i];for(b=0;b<8;++b)crc=(crc>>1)^(0xedb88320u & -(crc&1));}
    return ~crc;
}
static void dump_stream(IStream *stream, const char *label)
{
    STATSTG stat;LARGE_INTEGER zero={0};BYTE data[4096];ULONG count,i;
    HRESULT hr=IStream_Stat(stream,&stat,STATFLAG_NONAME);
    printf("%s size %08lx %llu\n",label,hr,stat.cbSize.QuadPart);
    if(FAILED(hr)||stat.cbSize.QuadPart>sizeof(data))return;
    IStream_Seek(stream,zero,STREAM_SEEK_SET,NULL);hr=IStream_Read(stream,data,sizeof(data),&count);
    printf("%s bytes %08lx",label,hr);for(i=0;i<count;++i)printf(" %02x",data[i]);puts("");
}
int main(void)
{
    IWICImagingFactory *factory;IWICComponentFactory *component;IWICBitmapEncoder *encoder;
    IWICBitmapFrameEncode *frame;IWICMetadataBlockWriter *block;IWICMetadataWriter *writer,*found;
    IWICPersistStream *persist;IStream *stream,*saved;IPropertyBag2 *bag;
    BYTE chunk[]={0,0,0,14,'e','X','I','f','I','I',42,0,8,0,0,0,0,0,0,0,0,0,0,0,0,0};
    BYTE pixels[]={1,2,3,255,4,5,6,255};PROPVARIANT empty={0},value={0};
    GUID format;DWORD crc;UINT count;unsigned int when;HRESULT hr;ULARGE_INTEGER size;
    setvbuf(stdout,NULL,_IOLBF,0);CoInitializeEx(NULL,COINIT_MULTITHREADED);
    REQUIRE(CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void **)&factory));
    REQUIRE(IWICImagingFactory_QueryInterface(factory,&IID_IWICComponentFactory,(void **)&component));
    crc=crc32(chunk+4,18);chunk[22]=crc>>24;chunk[23]=crc>>16;chunk[24]=crc>>8;chunk[25]=crc;
    for(when=0;when<4;++when)
    {
        printf("case %u\n",when);
        REQUIRE(CreateStreamOnHGlobal(NULL,TRUE,&stream));
        REQUIRE(IWICImagingFactory_CreateEncoder(factory,&GUID_ContainerFormatPng,NULL,&encoder));
        REQUIRE(IWICBitmapEncoder_Initialize(encoder,stream,WICBitmapEncoderNoCache));
        REQUIRE(IWICBitmapEncoder_CreateNewFrame(encoder,&frame,&bag));
        REQUIRE(IWICBitmapFrameEncode_QueryInterface(frame,&IID_IWICMetadataBlockWriter,(void **)&block));
        count=999;hr=IWICMetadataBlockWriter_GetCount(block,&count);printf("initial count %08lx %u\n",hr,count);
        REQUIRE(IWICComponentFactory_CreateMetadataWriter(component,&GUID_MetadataFormatUnknown,NULL,0,&writer));
        value.vt=VT_BLOB;value.blob.cbSize=sizeof(chunk);value.blob.pBlobData=chunk;
        REQUIRE(IWICMetadataWriter_SetValue(writer,&empty,&empty,&value));
        REQUIRE(IWICMetadataWriter_QueryInterface(writer,&IID_IWICPersistStream,(void **)&persist));
        size.QuadPart=999;hr=IWICPersistStream_GetSizeMax(persist,&size);printf("persist size %08lx %llu\n",hr,size.QuadPart);
        REQUIRE(CreateStreamOnHGlobal(NULL,TRUE,&saved));
        hr=IWICPersistStream_SaveEx(persist,saved,0,FALSE);printf("persist save %08lx\n",hr);dump_stream(saved,"persist");
        IStream_Release(saved);IWICPersistStream_Release(persist);
        if(when==0){hr=IWICMetadataBlockWriter_AddWriter(block,writer);printf("add %08lx\n",hr);}
        REQUIRE(IWICBitmapFrameEncode_Initialize(frame,bag));
        REQUIRE(IWICBitmapFrameEncode_SetSize(frame,2,1));format=GUID_WICPixelFormat32bppBGRA;
        REQUIRE(IWICBitmapFrameEncode_SetPixelFormat(frame,&format));
        if(when==1){hr=IWICMetadataBlockWriter_AddWriter(block,writer);printf("add %08lx\n",hr);}
        REQUIRE(IWICBitmapFrameEncode_WritePixels(frame,1,8,sizeof(pixels),pixels));
        if(when==2){hr=IWICMetadataBlockWriter_AddWriter(block,writer);printf("add %08lx\n",hr);}
        hr=IWICBitmapFrameEncode_Commit(frame);printf("frame commit %08lx\n",hr);
        if(when==3){hr=IWICMetadataBlockWriter_AddWriter(block,writer);printf("add %08lx\n",hr);}
        count=999;hr=IWICMetadataBlockWriter_GetCount(block,&count);printf("final count %08lx %u\n",hr,count);
        found=NULL;hr=IWICMetadataBlockWriter_GetWriterByIndex(block,0,&found);printf("get writer %08lx same %u\n",hr,found==writer);
        if(found)IWICMetadataWriter_Release(found);
        hr=IWICBitmapEncoder_Commit(encoder);printf("encoder commit %08lx\n",hr);dump_stream(stream,"png");
        IWICMetadataWriter_Release(writer);IWICMetadataBlockWriter_Release(block);IPropertyBag2_Release(bag);
        IWICBitmapFrameEncode_Release(frame);IWICBitmapEncoder_Release(encoder);IStream_Release(stream);
    }
    IWICComponentFactory_Release(component);IWICImagingFactory_Release(factory);CoUninitialize();puts("done");return 0;
}
