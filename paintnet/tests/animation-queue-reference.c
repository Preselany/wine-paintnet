/* Overlapping UIAnimation storyboards, default arbitration and queued starts.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <stdio.h>
#include "windows.h"
#include "initguid.h"
#include "uianimation.h"
#define REQUIRE(call) do {hr=(call);if(FAILED(hr)){printf("FAILED %u %08lx\n",__LINE__,hr);return 1;}}while(0)
static IUIAnimationStoryboard *third;
static void snapshot(IUIAnimationVariable *v,IUIAnimationVariable *other,IUIAnimationStoryboard *a,IUIAnimationStoryboard *b,unsigned int step)
{
    double value=-1,previous=-1,final=-1,other_value=-1,ea=-1,eb=-1;
    UI_ANIMATION_STORYBOARD_STATUS sa=99,sb=99,sc=99;double ec=-1;HRESULT hc;IUIAnimationStoryboard *current=NULL;
    HRESULT hr1=IUIAnimationVariable_GetCurrentStoryboard(v,&current),hr2,hr3;
    IUIAnimationVariable_GetValue(v,&value);IUIAnimationVariable_GetPreviousValue(v,&previous);IUIAnimationVariable_GetFinalValue(v,&final);
    IUIAnimationVariable_GetValue(other,&other_value);IUIAnimationStoryboard_GetStatus(a,&sa);IUIAnimationStoryboard_GetStatus(b,&sb);
    IUIAnimationStoryboard_GetStatus(third,&sc);hc=IUIAnimationStoryboard_GetElapsedTime(third,&ec);
    hr2=IUIAnimationStoryboard_GetElapsedTime(a,&ea);hr3=IUIAnimationStoryboard_GetElapsedTime(b,&eb);
    printf("state %u %.17g %.17g %.17g %.17g %u %u %08lx %u %08lx %.17g %08lx %.17g\n",step,value,previous,final,other_value,sa,sb,hr1,current==a?1:current==b?2:current==third?3:0,hr2,ea,hr3,eb);
    printf("third %u %u %08lx %.17g\n",step,sc,hc,ec);
    if(current)IUIAnimationStoryboard_Release(current);
}
int main(void)
{
    const double delays[]={0,.25,2,UI_ANIMATION_SECONDS_EVENTUALLY};
    const double times[]={10.25,10.375,10.5,10.625,10.75,11,11.5,12,12.5,13};
    IUIAnimationTransitionLibrary *lib;IUIAnimationManager *m;IUIAnimationVariable *v,*other;
    IUIAnimationStoryboard *a,*b;IUIAnimationTransition *ta,*tb,*to,*tc;
    UI_ANIMATION_SCHEDULING_RESULT result;UI_ANIMATION_UPDATE_RESULT changed;UI_ANIMATION_KEYFRAME key;
    unsigned int oldkind,newkind,delay,extra,future,i;HRESULT hr;
    CoInitializeEx(NULL,COINIT_MULTITHREADED);setvbuf(stdout,NULL,_IOFBF,65536);
    REQUIRE(CoCreateInstance(&CLSID_UIAnimationTransitionLibrary,NULL,CLSCTX_INPROC_SERVER,&IID_IUIAnimationTransitionLibrary,(void **)&lib));
    for(oldkind=0;oldkind<3;++oldkind)for(newkind=0;newkind<3;++newkind)for(delay=0;delay<4;++delay)for(extra=0;extra<2;++extra)for(future=0;future<2;++future)
    {
        printf("case %u %u %u %u %u\n",oldkind,newkind,delay,extra,future);
        REQUIRE(CoCreateInstance(&CLSID_UIAnimationManager,NULL,CLSCTX_INPROC_SERVER,&IID_IUIAnimationManager,(void **)&m));
        REQUIRE(IUIAnimationManager_SetAnimationMode(m,UI_ANIMATION_MODE_ENABLED));
        REQUIRE(IUIAnimationManager_CreateAnimationVariable(m,0,&v));REQUIRE(IUIAnimationManager_CreateAnimationVariable(m,0,&other));
        if(future)REQUIRE(IUIAnimationManager_CreateStoryboard(m,&third));
        REQUIRE(IUIAnimationManager_CreateStoryboard(m,&a));REQUIRE(IUIAnimationManager_CreateStoryboard(m,&b));
        if(!future)REQUIRE(IUIAnimationManager_CreateStoryboard(m,&third));
        if(oldkind==1)REQUIRE(IUIAnimationTransitionLibrary_CreateSmoothStopTransition(lib,1,10,&ta));
        else REQUIRE(IUIAnimationTransitionLibrary_CreateLinearTransition(lib,1,10,&ta));
        REQUIRE(IUIAnimationStoryboard_AddTransition(a,v,ta));
        if(oldkind==2)
        {
            REQUIRE(IUIAnimationStoryboard_AddKeyframeAfterTransition(a,ta,&key));
            REQUIRE(IUIAnimationStoryboard_RepeatBetweenKeyframes(a,UI_ANIMATION_KEYFRAME_STORYBOARD_START,key,-1));
        }
        if(extra)
        {
            REQUIRE(IUIAnimationTransitionLibrary_CreateLinearTransition(lib,2,100,&to));
            REQUIRE(IUIAnimationStoryboard_AddTransition(a,other,to));IUIAnimationTransition_Release(to);
        }
        if(newkind==0)REQUIRE(IUIAnimationTransitionLibrary_CreateLinearTransition(lib,.5,20,&tb));
        else if(newkind==1)REQUIRE(IUIAnimationTransitionLibrary_CreateSmoothStopTransition(lib,.5,20,&tb));
        else REQUIRE(IUIAnimationTransitionLibrary_CreateInstantaneousTransition(lib,20,&tb));
        REQUIRE(IUIAnimationStoryboard_AddTransition(b,v,tb));REQUIRE(IUIAnimationStoryboard_SetLongestAcceptableDelay(b,delays[delay]));
        REQUIRE(IUIAnimationManager_Update(m,10,NULL));REQUIRE(IUIAnimationStoryboard_Schedule(a,10,&result));REQUIRE(IUIAnimationManager_Update(m,10.25,NULL));
        REQUIRE(IUIAnimationTransitionLibrary_CreateLinearTransition(lib,.375,30,&tc));
        REQUIRE(IUIAnimationStoryboard_AddTransition(third,v,tc));
        REQUIRE(IUIAnimationStoryboard_SetLongestAcceptableDelay(third,delays[delay]));
        snapshot(v,other,a,b,0);result=99;hr=IUIAnimationStoryboard_Schedule(b,future?10.5:10.25,&result);
        printf("schedule %08lx %u\n",hr,result);
        result=99;hr=IUIAnimationStoryboard_Schedule(third,future?10.75:10.25,&result);
        printf("third_schedule %08lx %u\n",hr,result);snapshot(v,other,a,b,1);
        for(i=0;i<ARRAY_SIZE(times);++i)
        {
            changed=99;hr=IUIAnimationManager_Update(m,times[i],&changed);printf("update %u %08lx %u\n",i,hr,changed);snapshot(v,other,a,b,i+2);
        }
        IUIAnimationManager_Shutdown(m);IUIAnimationTransition_Release(tc);IUIAnimationStoryboard_Release(third);IUIAnimationTransition_Release(tb);IUIAnimationTransition_Release(ta);
        IUIAnimationStoryboard_Release(b);IUIAnimationStoryboard_Release(a);IUIAnimationVariable_Release(other);IUIAnimationVariable_Release(v);IUIAnimationManager_Release(m);fflush(stdout);
    }
    IUIAnimationTransitionLibrary_Release(lib);CoUninitialize();puts("done");return 0;
}
