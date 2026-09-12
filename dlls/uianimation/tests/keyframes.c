/* Native-measured UIAnimation keyframe and repeating timeline regressions.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <stdio.h>
#include <math.h>
#include "windows.h"
#include "uianimation.h"
#include "wine/test.h"
struct expected_frame
{
    double value, final, elapsed, duration;
    HRESULT elapsed_hr, duration_hr;
    unsigned int status, changed;
};
static const struct expected_frame expected_frames[9][25] =
{
    {
        {0, 10, 0, 0, 0x802a0102, 0x802a0101, 1, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 1},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
    },
    {
        {0, 10, 0, 0, 0x802a0102, 0x802a0101, 1, 0},
        {2.5, 10, 0.25, 0, 0x00000000, 0x802a0101, 3, 1},
        {5, 10, 0.5, 0, 0x00000000, 0x802a0101, 3, 1},
        {7.5, 10, 0.75, 0, 0x00000000, 0x802a0101, 3, 1},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 1},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
    },
    {
        {0, 10, 0, 0, 0x802a0102, 0x802a0101, 1, 0},
        {2.5, 10, 0.25, 0, 0x00000000, 0x802a0101, 3, 1},
        {5, 10, 0.5, 0, 0x00000000, 0x802a0101, 3, 1},
        {7.5, 10, 0.75, 0, 0x00000000, 0x802a0101, 3, 1},
        {0, 10, 1, 0, 0x00000000, 0x802a0101, 3, 1},
        {2.5, 10, 1.25, 0, 0x00000000, 0x802a0101, 3, 1},
        {5, 10, 1.5, 0, 0x00000000, 0x802a0101, 3, 1},
        {7.5, 10, 1.75, 0, 0x00000000, 0x802a0101, 3, 1},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 1},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
    },
    {
        {0, 10, 0, 0, 0x802a0102, 0x802a0101, 1, 0},
        {2.5, 10, 0.25, 0, 0x00000000, 0x802a0101, 3, 1},
        {5, 10, 0.5, 0, 0x00000000, 0x802a0101, 3, 1},
        {7.5, 10, 0.75, 0, 0x00000000, 0x802a0101, 3, 1},
        {0, 10, 1, 0, 0x00000000, 0x802a0101, 3, 1},
        {2.5, 10, 1.25, 0, 0x00000000, 0x802a0101, 3, 1},
        {5, 10, 1.5, 0, 0x00000000, 0x802a0101, 3, 1},
        {7.5, 10, 1.75, 0, 0x00000000, 0x802a0101, 3, 1},
        {0, 10, 2, 0, 0x00000000, 0x802a0101, 3, 1},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 1},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
    },
    {
        {0, 10, 0, 0, 0x802a0102, 0x802a0101, 1, 0},
        {2.5, 10, 0.25, 0, 0x00000000, 0x802a0101, 3, 1},
        {5, 10, 0.5, 0, 0x00000000, 0x802a0101, 3, 1},
        {7.5, 10, 0.75, 0, 0x00000000, 0x802a0101, 3, 1},
        {0, 10, 1, 0, 0x00000000, 0x802a0101, 3, 1},
        {2.5, 10, 1.25, 0, 0x00000000, 0x802a0101, 3, 1},
        {5, 10, 1.5, 0, 0x00000000, 0x802a0101, 3, 1},
        {7.5, 10, 1.75, 0, 0x00000000, 0x802a0101, 3, 1},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 1},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
    },
    {
        {0, 10, 0, 0, 0x802a0102, 0x802a0101, 1, 0},
        {2.5, 10, 0.25, 0, 0x00000000, 0x802a0101, 3, 1},
        {5, 10, 0.5, 0, 0x00000000, 0x802a0101, 3, 1},
        {5, 10, 0.75, 0, 0x00000000, 0x802a0101, 3, 0},
        {7.5, 10, 1, 0, 0x00000000, 0x802a0101, 3, 1},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 1},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
    },
    {
        {0, 20, 0, 0, 0x802a0102, 0x802a0101, 1, 0},
        {2.5, 20, 0.25, 0, 0x00000000, 0x802a0101, 3, 1},
        {5, 20, 0.5, 0, 0x00000000, 0x802a0101, 3, 1},
        {7.5, 20, 0.75, 0, 0x00000000, 0x802a0101, 3, 1},
        {0, 20, 1, 0, 0x00000000, 0x802a0101, 3, 1},
        {2.5, 20, 1.25, 0, 0x00000000, 0x802a0101, 3, 1},
        {5, 20, 1.5, 0, 0x00000000, 0x802a0101, 3, 1},
        {7.5, 20, 1.75, 0, 0x00000000, 0x802a0101, 3, 1},
        {10, 20, 2, 0, 0x00000000, 0x802a0101, 3, 1},
        {12.5, 20, 2.25, 0, 0x00000000, 0x802a0101, 3, 1},
        {15, 20, 2.5, 0, 0x00000000, 0x802a0101, 3, 1},
        {17.5, 20, 2.75, 0, 0x00000000, 0x802a0101, 3, 1},
        {20, 20, 0, 1, 0x802a0102, 0x00000000, 6, 1},
        {20, 20, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {20, 20, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {20, 20, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {20, 20, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {20, 20, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {20, 20, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {20, 20, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {20, 20, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {20, 20, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {20, 20, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {20, 20, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {20, 20, 0, 1, 0x802a0102, 0x00000000, 6, 0},
    },
    {
        {0, 10, 0, 0, 0x802a0102, 0x802a0101, 1, 0},
        {0, 10, 0, 0, 0x802a0102, 0x802a0101, 1, 0},
        {0, 10, 0, 0, 0x802a0102, 0x802a0101, 1, 0},
        {0, 10, 0, 0, 0x802a0102, 0x802a0101, 1, 0},
        {0, 10, 1, 0, 0x00000000, 0x802a0101, 3, 0},
        {2.5, 10, 1.25, 0, 0x00000000, 0x802a0101, 3, 1},
        {5, 10, 1.5, 0, 0x00000000, 0x802a0101, 3, 1},
        {7.5, 10, 1.75, 0, 0x00000000, 0x802a0101, 3, 1},
        {0, 10, 2, 0, 0x00000000, 0x802a0101, 3, 1},
        {2.5, 10, 2.25, 0, 0x00000000, 0x802a0101, 3, 1},
        {5, 10, 2.5, 0, 0x00000000, 0x802a0101, 3, 1},
        {7.5, 10, 2.75, 0, 0x00000000, 0x802a0101, 3, 1},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 1},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1, 0x802a0102, 0x00000000, 6, 0},
    },
    {
        {0, 10, 0, 0, 0x802a0102, 0x802a0101, 1, 0},
        {0, 10, 0, 0, 0x802a0102, 0x802a0101, 1, 0},
        {0, 10, 0, 0, 0x802a0102, 0x802a0101, 1, 0},
        {0, 10, 0, 0, 0x802a0102, 0x802a0101, 1, 0},
        {0, 10, 1, 0, 0x00000000, 0x802a0101, 3, 0},
        {1.66666667, 10, 1.25, 0, 0x00000000, 0x802a0101, 3, 1},
        {3.33333333, 10, 1.5, 0, 0x00000000, 0x802a0101, 3, 1},
        {5, 10, 1.75, 0, 0x00000000, 0x802a0101, 3, 1},
        {6.66666667, 10, 2, 0, 0x00000000, 0x802a0101, 3, 1},
        {8.33333333, 10, 2.25, 0, 0x00000000, 0x802a0101, 3, 1},
        {0, 10, 2.5, 0, 0x00000000, 0x802a0101, 3, 1},
        {1.66666667, 10, 2.75, 0, 0x00000000, 0x802a0101, 3, 1},
        {3.33333333, 10, 3, 0, 0x00000000, 0x802a0101, 3, 1},
        {5, 10, 3.25, 0, 0x00000000, 0x802a0101, 3, 1},
        {6.66666667, 10, 3.5, 0, 0x00000000, 0x802a0101, 3, 1},
        {8.33333333, 10, 3.75, 0, 0x00000000, 0x802a0101, 3, 1},
        {10, 10, 0, 1.5, 0x802a0102, 0x00000000, 6, 1},
        {10, 10, 0, 1.5, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1.5, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1.5, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1.5, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1.5, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1.5, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1.5, 0x802a0102, 0x00000000, 6, 0},
        {10, 10, 0, 1.5, 0x802a0102, 0x00000000, 6, 0},
    },
};
static const struct { HRESULT hr; BOOL null_out, unchanged; } expected_errors[] =
{
    {0x802a0107, 0, 0},
    {0x80004003, 0, 0},
    {0x80070057, 0, 0},
    {0x00000000, 1, 0},
    {0x00000000, 0, 0},
    {0x00000000, 0, 0},
    {0x00000000, 0, 0},
    {0x00000000, 0, 0},
    {0x80004003, 0, 1},
    {0x80004003, 0, 1},
    {0x80070057, 0, 1},
    {0x00000000, 0, 1},
    {0x802a0105, 0, 1},
    {0x802a0103, 0, 1},
    {0x00000000, 0, 1},
    {0x00000000, 0, 1},
};


#define CALL(name, expression) do { hr = (expression); ok(hr == S_OK, "%s returned %#lx.\n", name, hr); if (FAILED(hr)) goto done; } while (0)

static void run(IUIAnimationTransitionLibrary *library, unsigned int fixture, unsigned int kind, int count)
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
    winetest_push_context("kind %u, count %d", kind, count);
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
        const struct expected_frame *expected = &expected_frames[fixture][i];
        double time = i * .25;
        winetest_push_context("time %g", time);
        if (count == -1 && i == 9) { hr = IUIAnimationStoryboard_Conclude(storyboard); ok(hr == S_OK, "Conclude returned %#lx.\n", hr); }
        changed = 99;
        hr = IUIAnimationManager_Update(manager, 10 + time, &changed);
        IUIAnimationVariable_GetValue(variable, &value);
        IUIAnimationVariable_GetFinalValue(variable, &final);
        IUIAnimationStoryboard_GetStatus(storyboard, &status);
        eh = IUIAnimationStoryboard_GetElapsedTime(storyboard, &elapsed);
        dh = IUIAnimationTransition_GetDuration(transition, &duration);
        ok(hr == S_OK, "Update returned %#lx.\n", hr);
        ok(fabs(value - expected->value) < 1e-7, "Value %.17g, expected %.17g.\n", value, expected->value);
        ok(fabs(final - expected->final) < 1e-7, "Final %.17g, expected %.17g.\n", final, expected->final);
        ok(fabs(elapsed - expected->elapsed) < 1e-7, "Elapsed %.17g, expected %.17g.\n", elapsed, expected->elapsed);
        ok(fabs(duration - expected->duration) < 1e-7, "Duration %.17g, expected %.17g.\n", duration, expected->duration);
        ok(eh == expected->elapsed_hr, "Elapsed HRESULT %#lx, expected %#lx.\n", eh, expected->elapsed_hr);
        ok(dh == expected->duration_hr, "Duration HRESULT %#lx, expected %#lx.\n", dh, expected->duration_hr);
        ok(status == expected->status, "Status %u, expected %u.\n", status, expected->status);
        ok(changed == expected->changed, "Changed %u, expected %u.\n", changed, expected->changed);
        winetest_pop_context();
    }
    hr = IUIAnimationStoryboard_AddKeyframeAtOffset(storyboard, start, 1, &offset);
    ok(hr == UI_E_OBJECT_SEALED, "Sealed keyframe addition returned %#lx.\n", hr);
 done:
    if (manager) IUIAnimationManager_Shutdown(manager);
    if (tail) IUIAnimationTransition_Release(tail);
    if (reset) IUIAnimationTransition_Release(reset);
    if (transition) IUIAnimationTransition_Release(transition);
    if (storyboard) IUIAnimationStoryboard_Release(storyboard);
    if (variable) IUIAnimationVariable_Release(variable);
    if (manager) IUIAnimationManager_Release(manager);
    winetest_pop_context();
}

static void errors(IUIAnimationTransitionLibrary *library)
{
    unsigned int index = 0;
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
#define ERROR_CALL(name, expr) do { \
    out = (void *)0xdeadbeef; hr = (expr); \
    ok(hr == expected_errors[index].hr, "%s returned %#lx, expected %#lx.\n", name, hr, expected_errors[index].hr); \
    ok((out == NULL) == expected_errors[index].null_out, "%s unexpected null output %p.\n", name, out); \
    ok((out == (void *)0xdeadbeef) == expected_errors[index].unchanged, "%s unexpected output %p.\n", name, out); \
    ++index; \
} while (0)
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

START_TEST(keyframes)
{
    IUIAnimationTransitionLibrary *library = NULL;
    unsigned int kind, fixture = 0;
    HRESULT hr;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = CoCreateInstance(&CLSID_UIAnimationTransitionLibrary, NULL, CLSCTX_INPROC_SERVER,
            &IID_IUIAnimationTransitionLibrary, (void **)&library);
    ok(hr == S_OK, "Create library returned %#lx.\n", hr);
    if (FAILED(hr)) { CoUninitialize(); return; }
    errors(library);
    run(library, fixture++, 0, 0);
    run(library, fixture++, 0, 1);
    run(library, fixture++, 0, 2);
    run(library, fixture++, 0, -1);
    for (kind = 1; kind <= 5; ++kind) run(library, fixture++, kind, 2);
    IUIAnimationTransitionLibrary_Release(library);
    CoUninitialize();
}
