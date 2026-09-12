/* Per-window pointer feedback configuration.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "user_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(win);

/* Window properties are held by the server and disappear with the HWND.
 * Encode FALSE as 1 and TRUE as 2 so absence remains distinguishable. */
static const WCHAR * const feedback_properties[] =
{
    L"__wine_feedback_01", L"__wine_feedback_02", L"__wine_feedback_03",
    L"__wine_feedback_04", L"__wine_feedback_05", L"__wine_feedback_06",
    L"__wine_feedback_07", L"__wine_feedback_08", L"__wine_feedback_09",
    L"__wine_feedback_10", L"__wine_feedback_11", L"__wine_feedback_12",
    L"__wine_feedback_13",
};

BOOL WINAPI SetWindowFeedbackSetting(HWND hwnd, FEEDBACK_TYPE feedback, DWORD flags,
        UINT32 size, const void *configuration)
{
    DWORD error = GetLastError();
    BOOL result;
    TRACE("hwnd %p, feedback %u, flags %#lx, size %u, configuration %p.\n",
            hwnd, feedback, flags, size, configuration);
    if (!IsWindow(hwnd)) {SetLastError(ERROR_INVALID_WINDOW_HANDLE); return FALSE;}
    if ((feedback != FEEDBACK_MAX && (unsigned int)feedback - 1 >= ARRAY_SIZE(feedback_properties)) || flags
            || (size && (size != sizeof(BOOL) || !configuration)))
    {SetLastError(ERROR_INVALID_PARAMETER); return FALSE;}
    /* Native accepts the MAX sentinel without selecting any feedback type. */
    if (feedback == FEEDBACK_MAX) {SetLastError(error); return TRUE;}
    if (configuration)
        result = SetPropW(hwnd, feedback_properties[feedback - 1], (HANDLE)(ULONG_PTR)(*(const BOOL *)configuration ? 2 : 1));
    else
    {
        RemovePropW(hwnd, feedback_properties[feedback - 1]);
        result = TRUE;
    }
    if (result) SetLastError(error);
    return result;
}

BOOL WINAPI GetWindowFeedbackSetting(HWND hwnd, FEEDBACK_TYPE feedback, DWORD flags,
        UINT32 *size, void *configuration)
{
    DWORD error = GetLastError();
    HANDLE value = NULL;
    UINT32 available;
    TRACE("hwnd %p, feedback %u, flags %#lx, size %p, configuration %p.\n",
            hwnd, feedback, flags, size, configuration);
    if (!IsWindow(hwnd)) {SetLastError(ERROR_INVALID_WINDOW_HANDLE); return FALSE;}
    if ((feedback != FEEDBACK_MAX && (unsigned int)feedback - 1 >= ARRAY_SIZE(feedback_properties)) || !size || (flags & ~GWFS_INCLUDE_ANCESTORS))
    {SetLastError(ERROR_INVALID_PARAMETER); return FALSE;}
    available = *size; *size = sizeof(BOOL);
    if (configuration && available < sizeof(BOOL)) {SetLastError(ERROR_INSUFFICIENT_BUFFER); return FALSE;}
    while (feedback != FEEDBACK_MAX && hwnd)
    {
        value = GetPropW(hwnd, feedback_properties[feedback - 1]);
        if (value || !(flags & GWFS_INCLUDE_ANCESTORS)) break;
        hwnd = NtUserGetAncestor(hwnd, GA_PARENT);
    }
    if (configuration) *(BOOL *)configuration = value == (HANDLE)2;
    SetLastError(error);
    return value != NULL;
}
