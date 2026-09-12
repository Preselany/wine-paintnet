/* Concurrent creation of the system font collection must preserve caller refs.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include "dwrite.h"
#include "wine/test.h"
#define THREADS 12
struct request
{
    IDWriteFactory *factory;
    HANDLE start;
    IDWriteFontCollection *collection;
    HRESULT hr;
};
static DWORD WINAPI fetch_collection(void *data)
{
    struct request *request=data;
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    WaitForSingleObject(request->start,INFINITE);
    request->hr=IDWriteFactory_GetSystemFontCollection(request->factory,&request->collection,FALSE);
    CoUninitialize();
    return 0;
}
START_TEST(font_cache)
{
    struct request requests[THREADS];HANDLE threads[THREADS],start;
    IDWriteFactory *factory;UINT round,i,j,held;ULONG refs;HRESULT hr;
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    for(round=0;round<8;++round)
    {
        winetest_push_context("round %u",round);
        hr=DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED,&IID_IDWriteFactory,(IUnknown **)&factory);
        ok(hr==S_OK,"Factory creation failed %#lx.\n",hr);if(FAILED(hr))break;
        start=CreateEventW(NULL,TRUE,FALSE,NULL);
        for(i=0;i<THREADS;++i)
        {
            requests[i]=(struct request){factory,start,NULL,E_FAIL};
            threads[i]=CreateThread(NULL,0,fetch_collection,&requests[i],0,NULL);
            ok(threads[i]!=NULL,"Thread creation failed.\n");
        }
        SetEvent(start);WaitForMultipleObjects(THREADS,threads,TRUE,INFINITE);
        for(i=0;i<THREADS;++i)
        {
            CloseHandle(threads[i]);ok(requests[i].hr==S_OK,"Collection %u failed %#lx.\n",i,requests[i].hr);
            if(!requests[i].collection)continue;
            for(j=0;j<i;++j)if(requests[i].collection==requests[j].collection)break;
            if(j!=i)continue;
            held=0;for(j=0;j<THREADS;++j)if(requests[j].collection==requests[i].collection)++held;
            IDWriteFontCollection_AddRef(requests[i].collection);
            refs=IDWriteFontCollection_Release(requests[i].collection);
            ok(refs>=held,"Collection retained %lu references for %u callers.\n",refs,held);
            /* Keep the failing implementation alive for the remaining public
             * API checks instead of dereferencing a freed object. */
            while(refs<held){IDWriteFontCollection_AddRef(requests[i].collection);++refs;}
        }
        for(i=0;i<THREADS;++i)if(requests[i].collection)
        {
            UINT families=IDWriteFontCollection_GetFontFamilyCount(requests[i].collection);
            ok(families>0,"Collection %u lost its font families.\n",i);
            IDWriteFontCollection_Release(requests[i].collection);
        }
        CloseHandle(start);IDWriteFactory_Release(factory);winetest_pop_context();
    }
    CoUninitialize();
}
