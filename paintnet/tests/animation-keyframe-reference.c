/* UIAnimation keyframe/loop reference measurements.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <stdio.h>
#include <math.h>
#include "windows.h"
#include "initguid.h"
#include "uianimation.h"

#define CALL(name, expression) do { hr = (expression); printf("CALL %s %08lx\n", name, hr); if (FAILED(hr)) goto done; } while (0)

static void run(IUIAnimationTransitionLibrary *library, unsigned int kind, int count)
{
    IUIAnimationManager *manager = NULL;
    IUIAnimationVariable *variable = NULL;
    IUIAnimationStoryboard *storyboard = NULL;
    IUIAnimationTransition *transition = NULL, *reset = NULL, *tail = NULL;
    UI_ANIMATION_KEYFRAME start = UI_ANIMATION_KEYFRAME_STORYBOARD_START, end = NULL, offset = NULL;
    UI_ANIMATION_STORYBOARD_STATUS status;
    UI_ANIMATION_UPDATE_RESULT changed;
    double value, elapsed, final, duration;
    HRESULT hr, eh, dh;
    unsigned int i;
    printf("CASE kind=%u count=%d\n", kind, count);
    CALL("manager", CoCreateInstance(&CLSID_UIAnimationManager, NULL, CLSCTX_INPROC_SERVER, &IID_IUIAnimationManager, (void **)&manager));
    CALL("enable", IUIAnimationManager_SetAnimationMode(manager, UI_ANIMATION_MODE_ENABLED));
    CALL("variable", IUIAnimationManager_CreateAnimationVariable(manager, 0, &variable));
    CALL("storyboard", IUIAnimationManager_CreateStoryboard(manager, &storyboard));
    CALL("transition", IUIAnimationTransitionLibrary_CreateLinearTransition(library, 1, 10, &transition));
    if (kind == 1)
    {
        CALL("reset", IUIAnimationTransitionLibrary_CreateInstantaneousTransition(library, 0, &reset));
        CALL("add_reset", IUIAnimationStoryboard_AddTransition(storyboard, variable, reset));
        CALL("start_after_reset", IUIAnimationStoryboard_AddKeyframeAfterTransition(storyboard, reset, &start));
    }
    if (kind == 2)
    {
        CALL("start_offset", IUIAnimationStoryboard_AddKeyframeAtOffset(storyboard, start, .5, &start));
        CALL("end_offset", IUIAnimationStoryboard_AddKeyframeAtOffset(storyboard, start, .25, &end));
    }
    if (kind == 4 || kind == 5)
    {
        CALL("start_offset", IUIAnimationStoryboard_AddKeyframeAtOffset(storyboard, start, 1, &start));
        if (kind == 5)
        {
            CALL("end_offset", IUIAnimationStoryboard_AddKeyframeAtOffset(storyboard, start, 1.5, &end));
            CALL("between", IUIAnimationStoryboard_AddTransitionBetweenKeyframes(storyboard, variable, transition, start, end));
        }
        else CALL("at", IUIAnimationStoryboard_AddTransitionAtKeyframe(storyboard, variable, transition, start));
    }
    else CALL("add", IUIAnimationStoryboard_AddTransition(storyboard, variable, transition));
    if (!end) CALL("end_after", IUIAnimationStoryboard_AddKeyframeAfterTransition(storyboard, transition, &end));
    if (kind == 3)
    {
        CALL("tail", IUIAnimationTransitionLibrary_CreateLinearTransition(library, 1, 20, &tail));
        CALL("add_tail", IUIAnimationStoryboard_AddTransition(storyboard, variable, tail));
    }
    CALL("repeat", IUIAnimationStoryboard_RepeatBetweenKeyframes(storyboard, start, end, count));
    CALL("update_before", IUIAnimationManager_Update(manager, 10, NULL));
    CALL("schedule", IUIAnimationStoryboard_Schedule(storyboard, 10, NULL));
    for (i = 0; i <= 24; ++i)
    {
        double time = i * .25;
        if (count == -1 && i == 9) printf("CONCLUDE %08lx\n", IUIAnimationStoryboard_Conclude(storyboard));
        changed = 99;
        hr = IUIAnimationManager_Update(manager, 10 + time, &changed);
        IUIAnimationVariable_GetValue(variable, &value);
        IUIAnimationVariable_GetFinalValue(variable, &final);
        IUIAnimationStoryboard_GetStatus(storyboard, &status);
        eh = IUIAnimationStoryboard_GetElapsedTime(storyboard, &elapsed);
        dh = IUIAnimationTransition_GetDuration(transition, &duration);
        printf("FRAME t=%.9g hr=%08lx value=%.9g final=%.9g elapsed=%.9g eh=%08lx duration=%.9g dh=%08lx status=%u changed=%u\n",
                time, hr, value, final, elapsed, eh, duration, dh, status, changed);
    }
    printf("SEALED_OFFSET %08lx\n", IUIAnimationStoryboard_AddKeyframeAtOffset(storyboard, start, 1, &offset));
 done:
    if (manager) IUIAnimationManager_Shutdown(manager);
    if (tail) IUIAnimationTransition_Release(tail);
    if (reset) IUIAnimationTransition_Release(reset);
    if (transition) IUIAnimationTransition_Release(transition);
    if (storyboard) IUIAnimationStoryboard_Release(storyboard);
    if (variable) IUIAnimationVariable_Release(variable);
    if (manager) IUIAnimationManager_Release(manager);
    fflush(stdout);
}

static void errors(IUIAnimationTransitionLibrary *library)
{
    IUIAnimationManager *m;
    IUIAnimationVariable *v;
    IUIAnimationStoryboard *a, *b;
    IUIAnimationTransition *t;
    UI_ANIMATION_KEYFRAME key = NULL, other = NULL, out;
    HRESULT hr;
    CoCreateInstance(&CLSID_UIAnimationManager, NULL, CLSCTX_INPROC_SERVER, &IID_IUIAnimationManager, (void **)&m);
    IUIAnimationManager_CreateAnimationVariable(m, 0, &v);
    IUIAnimationManager_CreateStoryboard(m, &a);
    IUIAnimationManager_CreateStoryboard(m, &b);
    IUIAnimationTransitionLibrary_CreateLinearTransition(library, 1, 10, &t);
#define ERROR_CALL(name, expr) do { out = (void *)0xdeadbeef; hr = (expr); printf("ERROR %s %08lx null=%d unchanged=%d\n", name, hr, out == NULL, out == (void *)0xdeadbeef); fflush(stdout); } while (0)
    ERROR_CALL("after_missing", IUIAnimationStoryboard_AddKeyframeAfterTransition(a, t, &out));
    ERROR_CALL("after_null", IUIAnimationStoryboard_AddKeyframeAfterTransition(a, NULL, &out));
    ERROR_CALL("offset_null", IUIAnimationStoryboard_AddKeyframeAtOffset(a, NULL, 1, &out));
    ERROR_CALL("offset_negative", IUIAnimationStoryboard_AddKeyframeAtOffset(a, UI_ANIMATION_KEYFRAME_STORYBOARD_START, -1, &out));
    ERROR_CALL("offset_nan", IUIAnimationStoryboard_AddKeyframeAtOffset(a, UI_ANIMATION_KEYFRAME_STORYBOARD_START, NAN, &out));
    IUIAnimationStoryboard_AddKeyframeAtOffset(b, UI_ANIMATION_KEYFRAME_STORYBOARD_START, 1, &other);
    ERROR_CALL("offset_foreign", IUIAnimationStoryboard_AddKeyframeAtOffset(a, other, 1, &out));
    IUIAnimationStoryboard_AddTransition(a, v, t);
    ERROR_CALL("after_added", IUIAnimationStoryboard_AddKeyframeAfterTransition(a, t, &out));
    key = out;
    ERROR_CALL("after_again", IUIAnimationStoryboard_AddKeyframeAfterTransition(a, t, &out));
    ERROR_CALL("offset_null_out", IUIAnimationStoryboard_AddKeyframeAtOffset(a, key, 1, NULL));
    ERROR_CALL("after_null_out", IUIAnimationStoryboard_AddKeyframeAfterTransition(a, t, NULL));
    ERROR_CALL("repeat_bad_count", IUIAnimationStoryboard_RepeatBetweenKeyframes(a, UI_ANIMATION_KEYFRAME_STORYBOARD_START, key, -2));
    ERROR_CALL("repeat_foreign", IUIAnimationStoryboard_RepeatBetweenKeyframes(a, other, key, 2));
    ERROR_CALL("repeat_null", IUIAnimationStoryboard_RepeatBetweenKeyframes(a, NULL, key, 2));
    ERROR_CALL("repeat_reverse", IUIAnimationStoryboard_RepeatBetweenKeyframes(a, key, UI_ANIMATION_KEYFRAME_STORYBOARD_START, 2));
    ERROR_CALL("repeat_zero_time", IUIAnimationStoryboard_RepeatBetweenKeyframes(a, key, key, 2));
    ERROR_CALL("conclude_unstarted", IUIAnimationStoryboard_Conclude(a));
#undef ERROR_CALL
    IUIAnimationManager_Shutdown(m);
    IUIAnimationTransition_Release(t);
    IUIAnimationStoryboard_Release(a);
    IUIAnimationStoryboard_Release(b);
    IUIAnimationVariable_Release(v);
    IUIAnimationManager_Release(m);
}

int main(void)
{
    IUIAnimationTransitionLibrary *library = NULL;
    unsigned int kind;
    HRESULT hr;
    setvbuf(stdout, NULL, _IOFBF, 65536);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = CoCreateInstance(&CLSID_UIAnimationTransitionLibrary, NULL, CLSCTX_INPROC_SERVER,
            &IID_IUIAnimationTransitionLibrary, (void **)&library);
    if (FAILED(hr)) return 1;
    errors(library);
    run(library, 0, 0);
    run(library, 0, 1);
    run(library, 0, 2);
    run(library, 0, -1);
    for (kind = 1; kind <= 5; ++kind) run(library, kind, 2);
    IUIAnimationTransitionLibrary_Release(library);
    CoUninitialize();
    return 0;
}
