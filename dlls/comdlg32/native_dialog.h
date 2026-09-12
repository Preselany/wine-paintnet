/* Paint.NET Ubuntu native dialog bridge. LGPL-2.1-or-later. */
#ifndef __COMDLG32_NATIVE_DIALOG_H
#define __COMDLG32_NATIVE_DIALOG_H
#include "wine/unixlib.h"

#define NATIVE_DIALOG_LIMIT (1024 * 1024)
struct native_dialog_params
{
    const char *request;
    unsigned int request_size;
    char *response;
    unsigned int response_size;
    LONG cancelled;
};
#endif
