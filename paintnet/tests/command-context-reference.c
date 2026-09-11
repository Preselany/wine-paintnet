/* Command-list resource-domain and ownership measurements.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define main command_sequence_main
#include "command-list-reference.c"
#undef main

static void target_state(ID2D1DeviceContext *dc, ID2D1CommandList *list, unsigned int id)
{
    ID2D1Image *image;
    ID2D1DeviceContext_GetTarget(dc,&image);
    printf("target %u %u\n",id,image == (ID2D1Image *)list ? 1 : image ? 2 : 0);
    if (image) ID2D1Image_Release(image);
}
int main(void)
{
    ID3D11Device *d3d = NULL;
    IDXGIDevice *dxgi = NULL;
    ID2D1Factory1 *factory = NULL;
    ID2D1Device *device = NULL, *other = NULL;
    ID2D1DeviceContext *first = NULL, *second = NULL;
    ID2D1CommandList *list = NULL;
    ID2D1SolidColorBrush *brush = NULL;
    ID2D1CommandSink sink = {&sink_vtbl};
    D2D1_COLOR_F color = {1,0,0,1};
    D2D1_RECT_F rect = {0,0,2,2};
    D3D_DRIVER_TYPE driver = getenv("D2D1_TEST_WARP") ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE;
    HRESULT hr;
    unsigned int i;
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    REQUIRE(D3D11CreateDevice(NULL,driver,NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,NULL,0,D3D11_SDK_VERSION,&d3d,NULL,NULL));
    REQUIRE(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    REQUIRE(ID2D1Factory1_CreateDevice(factory,dxgi,&device));
    REQUIRE(ID2D1Factory1_CreateDevice(factory,dxgi,&other));
    for (i = 0; i < 7; ++i)
    {
        REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&first));
        REQUIRE(ID2D1Device_CreateDeviceContext(i == 1 ? other : device,0,&second));
        REQUIRE(ID2D1DeviceContext_CreateCommandList(first,&list));
        REQUIRE(ID2D1DeviceContext_CreateSolidColorBrush(second,&color,NULL,&brush));
        printf("case %u\n",i);
        if ((i >= 2 && i <= 4) || i == 6)
        {
            ID2D1DeviceContext_SetTarget(first,(ID2D1Image *)list);
            if (i != 4)
            {
                ID2D1DeviceContext_BeginDraw(first);
                ID2D1DeviceContext_FillRectangle(first,&rect,(ID2D1Brush *)brush);
                if (i != 6) ID2D1DeviceContext_SetTarget(first,NULL);
            }
            if (i == 3) printf("end_first %08lx\n",ID2D1DeviceContext_EndDraw(first,NULL,NULL));
        }
        if (i == 5) { ID2D1DeviceContext_Release(first); first = NULL; }
        ID2D1DeviceContext_SetTarget(second,(ID2D1Image *)list);
        target_state(second,list,2);
        if (i != 4)
        {
            ID2D1DeviceContext_BeginDraw(second);
            ID2D1DeviceContext_FillRectangle(second,&rect,(ID2D1Brush *)brush);
            printf("end_second %08lx\n",ID2D1DeviceContext_EndDraw(second,NULL,NULL));
        }
        if (i == 2 || i == 6) printf("end_first %08lx\n",ID2D1DeviceContext_EndDraw(first,NULL,NULL));
        printf("close %08lx\n",ID2D1CommandList_Close(list));
        if (first) target_state(first,list,1);
        target_state(second,list,2);
        hr = ID2D1CommandList_Stream(list,&sink);
        printf("stream %08lx\n",hr);
        if (first) { ID2D1DeviceContext_SetTarget(first,NULL); ID2D1DeviceContext_Release(first); first = NULL; }
        ID2D1DeviceContext_SetTarget(second,NULL); ID2D1DeviceContext_Release(second); second = NULL;
        ID2D1SolidColorBrush_Release(brush); brush = NULL;
        ID2D1CommandList_Release(list); list = NULL;
    }
done:
    if (first) ID2D1DeviceContext_Release(first);
    if (second) ID2D1DeviceContext_Release(second);
    if (brush) ID2D1SolidColorBrush_Release(brush);
    if (list) ID2D1CommandList_Release(list);
    if (device) ID2D1Device_Release(device);
    if (other) ID2D1Device_Release(other);
    if (factory) ID2D1Factory1_Release(factory);
    if (dxgi) IDXGIDevice_Release(dxgi);
    if (d3d) ID3D11Device_Release(d3d);
    CoUninitialize();
    return 0;
}
