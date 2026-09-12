/* Native window feedback configuration reference.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>
static BOOL (WINAPI *set_feedback)(HWND,FEEDBACK_TYPE,DWORD,UINT32,const void *);
static BOOL (WINAPI *get_feedback)(HWND,FEEDBACK_TYPE,DWORD,UINT32 *,void *);
static void set(const char *label,HWND hwnd,UINT type,DWORD flags,UINT size,const void *value)
{
    BOOL result;SetLastError(0xdeadbeef);result=set_feedback(hwnd,type,flags,size,value);
    printf("set %s %u %lu %u %d %lu\n",label,type,flags,size,result,GetLastError());
}
static void get(const char *label,HWND hwnd,UINT type,DWORD flags,UINT size,BOOL null_size,BOOL null_value)
{
    BOOL value=0x11223344,result;SetLastError(0xdeadbeef);
    result=get_feedback(hwnd,type,flags,null_size ? NULL : &size,null_value ? NULL : &value);
    printf("get %s %u %lu %d %lu %u %d\n",label,type,flags,result,GetLastError(),size,value);
}
int main(void)
{
    HWND parent,child;HMODULE module=GetModuleHandleW(L"user32.dll");UINT type;BOOL value=0;
    setvbuf(stdout,NULL,_IONBF,0);
    set_feedback=(void *)GetProcAddress(module,"SetWindowFeedbackSetting");
    get_feedback=(void *)GetProcAddress(module,"GetWindowFeedbackSetting");
    if(!set_feedback||!get_feedback){puts("missing export");return 1;}
    parent=CreateWindowExW(0,L"STATIC",L"feedback-test",WS_OVERLAPPED,0,0,10,10,NULL,NULL,NULL,NULL);
    child=CreateWindowExW(0,L"STATIC",L"child",WS_CHILD,0,0,5,5,parent,NULL,NULL,NULL);
    for(type=1;type<=11;++type)
    {
        get("unset",parent,type,0,4,FALSE,FALSE);
        set("disable",parent,type,0,4,&value);get("disabled",parent,type,0,4,FALSE,FALSE);
        get("local",child,type,0,4,FALSE,FALSE);get("inherited",child,type,1,4,FALSE,FALSE);
        value=7;set("enable",child,type,0,4,&value);get("enabled",child,type,1,4,FALSE,FALSE);
        set("reset",child,type,0,0,NULL);get("reset-local",child,type,0,4,FALSE,FALSE);get("reset-inherit",child,type,1,4,FALSE,FALSE);
        value=0;
    }
    for(type=0;type<=40;++type)
    {
        set("type",parent,type,0,4,&value);get("type",parent,type,0,4,FALSE,FALSE);
    }
    set("flags",parent,1,1,4,&value);get("flags",parent,1,2,4,FALSE,FALSE);
    set("short",parent,1,0,1,&value);set("zero-value",parent,1,0,0,&value);
    set("null-config",parent,1,0,4,NULL);
    get("size-zero",parent,1,0,0,FALSE,FALSE);get("size-large",parent,1,0,8,FALSE,FALSE);
    get("null-config",parent,1,0,4,FALSE,TRUE);get("null-size",parent,1,0,4,TRUE,FALSE);
    set("null-window",NULL,1,0,4,&value);get("null-window",NULL,1,0,4,FALSE,FALSE);
    DestroyWindow(child);set("destroyed",child,1,0,4,&value);get("destroyed",child,1,0,4,FALSE,FALSE);
    {
        static const UINT sizes[]={0,1,3,4,8};
        UINT i,j;char label[48];
        for(i=0;i<5;++i)for(j=0;j<2;++j)
        {
            value=1;set_feedback(parent,1,0,4,&value);
            sprintf(label,"size-%u-null-%u",sizes[i],j);
            get(label,parent,1,0,sizes[i],FALSE,j);
            set(label,parent,1,0,sizes[i],j ? NULL : &value);
            get(label,parent,1,0,4,FALSE,FALSE);
        }
        set("type-max",parent,0xffffffff,0,4,&value);get("type-max",parent,0xffffffff,0,4,FALSE,FALSE);
    }
    for(type=1;type<=13;++type)get("after-max-on",parent,type,0,4,FALSE,FALSE);
    value=0;set("max-off",parent,0xffffffff,0,4,&value);
    for(type=1;type<=13;++type)get("after-max-off",parent,type,0,4,FALSE,FALSE);
    set("max-reset",parent,0xffffffff,0,0,NULL);
    for(type=1;type<=13;++type)get("after-max-reset",parent,type,0,4,FALSE,FALSE);
    set_feedback(parent,1,0,0,NULL);
    get("unset-query",parent,1,0,0,FALSE,TRUE);
    get("unset-query-small",parent,1,0,1,FALSE,TRUE);
    DestroyWindow(parent);return 0;
}
