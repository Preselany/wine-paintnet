/* Select Microsoft's software renderer for Windows reference measurements.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef __WINE_D2D_EFFECT_TEST_H
#define __WINE_D2D_EFFECT_TEST_H

static D3D_DRIVER_TYPE effect_test_driver(void)
{
    char value[2];
    return GetEnvironmentVariableA("D2D1_TEST_WARP", value, sizeof(value)) == 1 && value[0] == '1'
            ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE;
}

#endif
