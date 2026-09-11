/* Tests for command-list recording, lifecycle, and public-sink replay.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include "d2d1_1.h"
#include "d3d11.h"
#include "wine/test.h"
#include "effect_test.h"

/* Native Windows reference: public-API command-list lifecycle and replay.
 * Each small image is encoded using exact binary-fraction RGBA palette values.
 */
static const char *expected_records[] = {
    "case 0 CSC S",
    "close 00000000",
    "target 0",
    "stream_begin",
    "cmd BeginDraw",
    "cmd EndDraw",
    "stream_end 00000000",
    "close 88990001",
    "target 0",
    "stream_begin",
    "stream_end 88990001",
    "case 1 LCS",
    "close 00000000",
    "target 0",
    "stream_begin",
    "cmd BeginDraw",
    "cmd EndDraw",
    "stream_end 00000000",
    "case 2 LBFECS",
    "end 00000000 0 0",
    "close 00000000",
    "target 0",
    "stream_begin",
    "cmd BeginDraw",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 10 20",
    "cmd SetTransform 1 0 0 1 0 0",
    "cmd FillRectangle 1 1 3 3 0.25 0.5 0.75 0.5",
    "cmd EndDraw",
    "stream_end 00000000",
    "case 3 bBLFbCSE",
    "close 00000000",
    "target 2",
    "stream_begin",
    "cmd BeginDraw",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 10 20",
    "cmd SetTransform 1 0 0 1 0 0",
    "cmd FillRectangle 1 1 3 3 0.25 0.5 0.75 0.5",
    "cmd EndDraw",
    "stream_end 00000000",
    "end 00000000 0 0",
    "case 4 NBLFNCSE",
    "close 00000000",
    "target 0",
    "stream_begin",
    "cmd BeginDraw",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 10 20",
    "cmd SetTransform 1 0 0 1 0 0",
    "cmd FillRectangle 1 1 3 3 0.25 0.5 0.75 0.5",
    "cmd EndDraw",
    "stream_end 00000000",
    "end 00000000 0 0",
    "case 5 LFCS",
    "close 88990001",
    "target 0",
    "stream_begin",
    "stream_end 88990001",
    "case 6 LBFEBFECS",
    "end 00000000 0 0",
    "end 00000000 0 0",
    "close 00000000",
    "target 0",
    "stream_begin",
    "cmd BeginDraw",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 10 20",
    "cmd SetTransform 1 0 0 1 0 0",
    "cmd FillRectangle 1 1 3 3 0.25 0.5 0.75 0.5",
    "cmd FillRectangle 1 1 3 3 0.25 0.5 0.75 0.5",
    "cmd EndDraw",
    "stream_end 00000000",
    "case 7 bBLFbXLFbCSE",
    "close 00000000",
    "target 2",
    "stream_begin",
    "cmd BeginDraw",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 10 20",
    "cmd SetTransform 1 0 0 1 0 0",
    "cmd FillRectangle 1 1 3 3 0.25 0.5 0.75 0.5",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 10 20",
    "cmd SetTransform 1 0 0 1 5 7",
    "cmd FillRectangle 1 1 3 3 0.25 0.5 0.75 0.5",
    "cmd EndDraw",
    "stream_end 00000000",
    "end 00000000 0 0",
    "case 8 LBF CSES",
    "close 00000000",
    "target 0",
    "stream_begin",
    "cmd BeginDraw",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 10 20",
    "cmd SetTransform 1 0 0 1 0 0",
    "cmd FillRectangle 1 1 3 3 0.25 0.5 0.75 0.5",
    "cmd EndDraw",
    "stream_end 00000000",
    "end 00000000 0 0",
    "stream_begin",
    "cmd BeginDraw",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 10 20",
    "cmd SetTransform 1 0 0 1 0 0",
    "cmd FillRectangle 1 1 3 3 0.25 0.5 0.75 0.5",
    "cmd EndDraw",
    "stream_end 00000000",
    "case 9 LBF CFES",
    "close 00000000",
    "target 0",
    "end 88990001 10 20",
    "stream_begin",
    "cmd BeginDraw",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 10 20",
    "cmd SetTransform 1 0 0 1 0 0",
    "cmd FillRectangle 1 1 3 3 0.25 0.5 0.75 0.5",
    "cmd EndDraw",
    "stream_end 00000000",
    "case 10 LBFE CBFES",
    "end 00000000 0 0",
    "close 00000000",
    "target 0",
    "end 88990001 10 20",
    "stream_begin",
    "cmd BeginDraw",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 10 20",
    "cmd SetTransform 1 0 0 1 0 0",
    "cmd FillRectangle 1 1 3 3 0.25 0.5 0.75 0.5",
    "cmd EndDraw",
    "stream_end 00000000",
    "case 11 bBLFNLFNCSE",
    "close 00000000",
    "target 0",
    "stream_begin",
    "cmd BeginDraw",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 10 20",
    "cmd SetTransform 1 0 0 1 0 0",
    "cmd FillRectangle 1 1 3 3 0.25 0.5 0.75 0.5",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 10 20",
    "cmd SetTransform 1 0 0 1 0 0",
    "cmd FillRectangle 1 1 3 3 0.25 0.5 0.75 0.5",
    "cmd EndDraw",
    "stream_end 00000000",
    "end 00000000 0 0",
    "case 12 LBXFECS",
    "end 00000000 0 0",
    "close 00000000",
    "target 0",
    "stream_begin",
    "cmd BeginDraw",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 10 20",
    "cmd SetTransform 1 0 0 1 0 0",
    "cmd SetTransform 1 0 0 1 5 7",
    "cmd FillRectangle 1 1 3 3 0.25 0.5 0.75 0.5",
    "cmd EndDraw",
    "stream_end 00000000",
    "case 13 LSBFSECS",
    "stream_begin",
    "stream_end 88990001",
    "stream_begin",
    "stream_end 88990001",
    "end 00000000 0 0",
    "close 00000000",
    "target 0",
    "stream_begin",
    "cmd BeginDraw",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 10 20",
    "cmd SetTransform 1 0 0 1 0 0",
    "cmd FillRectangle 1 1 3 3 0.25 0.5 0.75 0.5",
    "cmd EndDraw",
    "stream_end 00000000",
    "case 14 BLFECS",
    "end 00000000 0 0",
    "close 00000000",
    "target 0",
    "stream_begin",
    "cmd BeginDraw",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 10 20",
    "cmd SetTransform 1 0 0 1 0 0",
    "cmd FillRectangle 1 1 3 3 0.25 0.5 0.75 0.5",
    "cmd EndDraw",
    "stream_end 00000000",
    "case 15 LBFE X BFECS",
    "end 00000000 0 0",
    "end 00000000 0 0",
    "close 00000000",
    "target 0",
    "stream_begin",
    "cmd BeginDraw",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 10 20",
    "cmd SetTransform 1 0 0 1 0 0",
    "cmd FillRectangle 1 1 3 3 0.25 0.5 0.75 0.5",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 10 20",
    "cmd SetTransform 1 0 0 1 5 7",
    "cmd FillRectangle 1 1 3 3 0.25 0.5 0.75 0.5",
    "cmd EndDraw",
    "stream_end 00000000",
    "case 16 LX BFECS",
    "end 00000000 0 0",
    "close 00000000",
    "target 0",
    "stream_begin",
    "cmd BeginDraw",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 10 20",
    "cmd SetTransform 1 0 0 1 5 7",
    "cmd FillRectangle 1 1 3 3 0.25 0.5 0.75 0.5",
    "cmd EndDraw",
    "stream_end 00000000",
    "case 17 LBFE FCS",
    "end 00000000 0 0",
    "close 88990001",
    "target 0",
    "stream_begin",
    "stream_end 88990001",
    "case 18 LBF C LBFES",
    "close 00000000",
    "target 0",
    "end 88990001 10 20",
    "stream_begin",
    "cmd BeginDraw",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 10 20",
    "cmd SetTransform 1 0 0 1 0 0",
    "cmd FillRectangle 1 1 3 3 0.25 0.5 0.75 0.5",
    "cmd EndDraw",
    "stream_end 00000000",
    "case 19 LBFE CLS",
    "end 00000000 0 0",
    "close 00000000",
    "target 0",
    "stream_begin",
    "cmd BeginDraw",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 10 20",
    "cmd SetTransform 1 0 0 1 0 0",
    "cmd FillRectangle 1 1 3 3 0.25 0.5 0.75 0.5",
    "cmd EndDraw",
    "stream_end 00000000",
    "case 20 bBLFbNF CSE",
    "close 88990001",
    "target 0",
    "stream_begin",
    "stream_end 88990001",
    "end 88990001 10 20",
    "case 21 bBLFYZFbCSE",
    "close 00000000",
    "target 2",
    "stream_begin",
    "cmd BeginDraw",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 10 20",
    "cmd SetTransform 1 0 0 1 0 0",
    "cmd FillRectangle 1 1 3 3 0.25 0.5 0.75 0.5",
    "cmd SetTransform 1 0 0 1 2 1",
    "cmd FillRectangle 1 1 3 3 1 0 0 1",
    "cmd EndDraw",
    "stream_end 00000000",
    "end 00000000 0 0",
    "case 22 LBFYFECS",
    "end 00000000 0 0",
    "close 00000000",
    "target 0",
    "stream_begin",
    "cmd BeginDraw",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 10 20",
    "cmd SetTransform 1 0 0 1 0 0",
    "cmd FillRectangle 1 1 3 3 0.25 0.5 0.75 0.5",
    "cmd FillRectangle 1 1 3 3 1 0 0 1",
    "cmd EndDraw",
    "stream_end 00000000",
    "case 0",
    "target 2 1",
    "end_second 00000000",
    "close 00000000",
    "target 1 0",
    "target 2 0",
    "cmd BeginDraw",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 0 0",
    "cmd SetTransform 1 0 0 1 0 0",
    "cmd FillRectangle 0 0 2 2 1 0 0 1",
    "cmd EndDraw",
    "stream 00000000",
    "case 1",
    "target 2 0",
    "end_second 88990015",
    "close 00000000",
    "target 1 0",
    "target 2 0",
    "cmd BeginDraw",
    "cmd EndDraw",
    "stream 00000000",
    "case 2",
    "target 2 1",
    "end_second 88990001",
    "end_first 00000000",
    "close 88990001",
    "target 1 0",
    "target 2 0",
    "stream 88990001",
    "case 3",
    "end_first 00000000",
    "target 2 1",
    "end_second 00000000",
    "close 00000000",
    "target 1 0",
    "target 2 0",
    "cmd BeginDraw",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 0 0",
    "cmd SetTransform 1 0 0 1 0 0",
    "cmd FillRectangle 0 0 2 2 1 0 0 1",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 0 0",
    "cmd SetTransform 1 0 0 1 0 0",
    "cmd FillRectangle 0 0 2 2 1 0 0 1",
    "cmd EndDraw",
    "stream 00000000",
    "case 4",
    "target 2 1",
    "close 00000000",
    "target 1 0",
    "target 2 0",
    "cmd BeginDraw",
    "cmd EndDraw",
    "stream 00000000",
    "case 5",
    "target 2 1",
    "end_second 00000000",
    "close 00000000",
    "target 2 0",
    "cmd BeginDraw",
    "cmd SetAntialiasMode",
    "cmd SetPrimitiveBlend",
    "cmd SetUnitMode",
    "cmd SetTextAntialiasMode",
    "cmd SetTags 0 0",
    "cmd SetTransform 1 0 0 1 0 0",
    "cmd FillRectangle 0 0 2 2 1 0 0 1",
    "cmd EndDraw",
    "stream 00000000",
    "case 6",
    "target 2 1",
    "end_second 88990001",
    "end_first 00000000",
    "close 88990001",
    "target 1 0",
    "target 2 0",
    "stream 88990001",
};
static const float palette[][4] = {
    {0.0f, 0.0f, 0.0f, 0.0f},
    {0.125f, 0.25f, 0.375f, 0.5f},
    {0.1875f, 0.375f, 0.5625f, 0.75f},
    {1.0f, 0.0f, 0.0f, 1.0f},
};
static const BYTE expected_images[][64] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,2,2,0,0,0,0,0,0,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,2,2,0,0,0,0,0,0,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,3,3,0,0,0,0,0,0,3,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,3,3,0,0,0,0,0,0,3,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};
static unsigned int record_index, image_index;

static void record(const char *format, ...)
{
    char line[512];
    va_list args;
    size_t len;
    va_start(args,format);
    vsnprintf(line,sizeof(line),format,args);
    va_end(args);
    len = strlen(line);
    if (len && line[len-1] == '\n') line[len-1] = 0;
    ok(record_index < ARRAY_SIZE(expected_records),"Unexpected extra record %s.\n",line);
    if (record_index < ARRAY_SIZE(expected_records))
        ok(!strcmp(line,expected_records[record_index]),"Record %u: %s, expected %s.\n",
                record_index,line,expected_records[record_index]);
    ++record_index;
}

static void check_pixels(const D2D1_MAPPED_RECT *mapped)
{
    unsigned int x,y,c;
    const float *v,*expected;
    ok(image_index < ARRAY_SIZE(expected_images),"Unexpected extra image %u.\n",image_index);
    if (image_index >= ARRAY_SIZE(expected_images)) return;
    for (y = 0; y < 8; ++y) for (x = 0; x < 8; ++x)
    {
        v = (const float *)(mapped->bits+y*mapped->pitch)+4*x;
        expected = palette[expected_images[image_index][y*8+x]];
        for (c = 0; c < 4; ++c)
            ok(fabsf(v[c]-expected[c]) < 1e-7f,
                    "Image %u pixel %u,%u channel %u: %.9g, expected %.9g.\n",
                    image_index,x,y,c,v[c],expected[c]);
    }
    ++image_index;
}

static ID2D1DeviceContext *replay;

static HRESULT STDMETHODCALLTYPE sink_QueryInterface(ID2D1CommandSink *This, REFIID riid, void **ppvObject)
{
    *ppvObject = NULL;
    if (!IsEqualGUID(riid,&IID_IUnknown) && !IsEqualGUID(riid,&IID_ID2D1CommandSink)) return E_NOINTERFACE;
    *ppvObject = This;
    return S_OK;
}
static ULONG STDMETHODCALLTYPE sink_AddRef(ID2D1CommandSink *This)
{
    (void)This; return 2;
}
static ULONG STDMETHODCALLTYPE sink_Release(ID2D1CommandSink *This)
{
    (void)This; return 2;
}
static HRESULT STDMETHODCALLTYPE sink_BeginDraw(ID2D1CommandSink *This)
{
    (void)This;
    record("%s\n","cmd BeginDraw");
    if (replay) { ID2D1DeviceContext_BeginDraw(replay); }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_EndDraw(ID2D1CommandSink *This)
{
    (void)This;
    record("%s\n","cmd EndDraw");
    if (replay) { return ID2D1DeviceContext_EndDraw(replay,NULL,NULL); }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_SetAntialiasMode(ID2D1CommandSink *This, D2D1_ANTIALIAS_MODE antialias_mode)
{
    (void)This;
    (void)antialias_mode;
    record("%s\n","cmd SetAntialiasMode");
    if (replay) { ID2D1DeviceContext_SetAntialiasMode(replay,antialias_mode); }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_SetTags(ID2D1CommandSink *This, D2D1_TAG tag1, D2D1_TAG tag2)
{
    (void)This;
    (void)tag1;
    (void)tag2;
    record("cmd SetTags %llu %llu\n",tag1,tag2);
    if (replay) { ID2D1DeviceContext_SetTags(replay,tag1,tag2); }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_SetTextAntialiasMode(ID2D1CommandSink *This, D2D1_TEXT_ANTIALIAS_MODE antialias_mode)
{
    (void)This;
    (void)antialias_mode;
    record("%s\n","cmd SetTextAntialiasMode");
    if (replay) { ID2D1DeviceContext_SetTextAntialiasMode(replay,antialias_mode); }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_SetTextRenderingParams(ID2D1CommandSink *This, IDWriteRenderingParams *text_rendering_params)
{
    (void)This;
    (void)text_rendering_params;
    record("%s\n","cmd SetTextRenderingParams");
    if (replay) { ID2D1DeviceContext_SetTextRenderingParams(replay,text_rendering_params); }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_SetTransform(ID2D1CommandSink *This, const D2D1_MATRIX_3X2_F *transform)
{
    (void)This;
    (void)transform;
    record("cmd SetTransform %g %g %g %g %g %g\n",transform->m11,transform->m12,transform->m21,transform->m22,transform->dx,transform->dy);
    if (replay) { ID2D1DeviceContext_SetTransform(replay,transform); }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_SetPrimitiveBlend(ID2D1CommandSink *This, D2D1_PRIMITIVE_BLEND primitive_blend)
{
    (void)This;
    (void)primitive_blend;
    record("%s\n","cmd SetPrimitiveBlend");
    if (replay) { ID2D1DeviceContext_SetPrimitiveBlend(replay,primitive_blend); }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_SetUnitMode(ID2D1CommandSink *This, D2D1_UNIT_MODE unit_mode)
{
    (void)This;
    (void)unit_mode;
    record("%s\n","cmd SetUnitMode");
    if (replay) { ID2D1DeviceContext_SetUnitMode(replay,unit_mode); }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_Clear(ID2D1CommandSink *This, const D2D1_COLOR_F *color)
{
    (void)This;
    (void)color;
    record("%s\n","cmd Clear");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_DrawGlyphRun(ID2D1CommandSink *This, D2D1_POINT_2F baseline_origin, const DWRITE_GLYPH_RUN *glyph_run, const DWRITE_GLYPH_RUN_DESCRIPTION *glyph_run_desc, ID2D1Brush *brush, DWRITE_MEASURING_MODE measuring_mode)
{
    (void)This;
    (void)baseline_origin;
    (void)glyph_run;
    (void)glyph_run_desc;
    (void)brush;
    (void)measuring_mode;
    record("%s\n","cmd DrawGlyphRun");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_DrawLine(ID2D1CommandSink *This, D2D1_POINT_2F p0, D2D1_POINT_2F p1, ID2D1Brush *brush, float stroke_width, ID2D1StrokeStyle *stroke_style)
{
    (void)This;
    (void)p0;
    (void)p1;
    (void)brush;
    (void)stroke_width;
    (void)stroke_style;
    record("%s\n","cmd DrawLine");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_DrawGeometry(ID2D1CommandSink *This, ID2D1Geometry *geometry, ID2D1Brush *brush, float stroke_width, ID2D1StrokeStyle *stroke_style)
{
    (void)This;
    (void)geometry;
    (void)brush;
    (void)stroke_width;
    (void)stroke_style;
    record("%s\n","cmd DrawGeometry");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_DrawRectangle(ID2D1CommandSink *This, const D2D1_RECT_F *rect, ID2D1Brush *brush, float stroke_width, ID2D1StrokeStyle *stroke_style)
{
    (void)This;
    (void)rect;
    (void)brush;
    (void)stroke_width;
    (void)stroke_style;
    record("%s\n","cmd DrawRectangle");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_DrawBitmap(ID2D1CommandSink *This, ID2D1Bitmap *bitmap, const D2D1_RECT_F *dst_rect, float opacity, D2D1_INTERPOLATION_MODE interpolation_mode, const D2D1_RECT_F *src_rect, const D2D1_MATRIX_4X4_F *perspective_transform)
{
    (void)This;
    (void)bitmap;
    (void)dst_rect;
    (void)opacity;
    (void)interpolation_mode;
    (void)src_rect;
    (void)perspective_transform;
    record("%s\n","cmd DrawBitmap");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_DrawImage(ID2D1CommandSink *This, ID2D1Image *image, const D2D1_POINT_2F *target_offset, const D2D1_RECT_F *image_rect, D2D1_INTERPOLATION_MODE interpolation_mode, D2D1_COMPOSITE_MODE composite_mode)
{
    (void)This;
    (void)image;
    (void)target_offset;
    (void)image_rect;
    (void)interpolation_mode;
    (void)composite_mode;
    record("%s\n","cmd DrawImage");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_DrawGdiMetafile(ID2D1CommandSink *This, ID2D1GdiMetafile *metafile, const D2D1_POINT_2F *target_offset)
{
    (void)This;
    (void)metafile;
    (void)target_offset;
    record("%s\n","cmd DrawGdiMetafile");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_FillMesh(ID2D1CommandSink *This, ID2D1Mesh *mesh, ID2D1Brush *brush)
{
    (void)This;
    (void)mesh;
    (void)brush;
    record("%s\n","cmd FillMesh");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_FillOpacityMask(ID2D1CommandSink *This, ID2D1Bitmap *bitmap, ID2D1Brush *brush, const D2D1_RECT_F *dst_rect, const D2D1_RECT_F *src_rect)
{
    (void)This;
    (void)bitmap;
    (void)brush;
    (void)dst_rect;
    (void)src_rect;
    record("%s\n","cmd FillOpacityMask");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_FillGeometry(ID2D1CommandSink *This, ID2D1Geometry *geometry, ID2D1Brush *brush, ID2D1Brush *opacity_brush)
{
    (void)This;
    (void)geometry;
    (void)brush;
    (void)opacity_brush;
    record("%s\n","cmd FillGeometry");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_FillRectangle(ID2D1CommandSink *This, const D2D1_RECT_F *rect, ID2D1Brush *brush)
{
    ID2D1SolidColorBrush *solid;
    D2D1_COLOR_F c = {0};
    (void)This;
    if (SUCCEEDED(ID2D1Brush_QueryInterface(brush,&IID_ID2D1SolidColorBrush,(void **)&solid)))
    {
        c = ID2D1SolidColorBrush_GetColor(solid);
        ID2D1SolidColorBrush_Release(solid);
    }
    record("cmd FillRectangle %g %g %g %g %g %g %g %g\n",rect->left,rect->top,rect->right,rect->bottom,c.r,c.g,c.b,c.a);
    if (replay) { ID2D1DeviceContext_FillRectangle(replay,rect,brush); }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_PushAxisAlignedClip(ID2D1CommandSink *This, const D2D1_RECT_F *clip_rect, D2D1_ANTIALIAS_MODE antialias_mode)
{
    (void)This;
    (void)clip_rect;
    (void)antialias_mode;
    record("%s\n","cmd PushAxisAlignedClip");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_PushLayer(ID2D1CommandSink *This, const D2D1_LAYER_PARAMETERS1 *layer_parameters, ID2D1Layer *layer)
{
    (void)This;
    (void)layer_parameters;
    (void)layer;
    record("%s\n","cmd PushLayer");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_PopAxisAlignedClip(ID2D1CommandSink *This)
{
    (void)This;
    record("%s\n","cmd PopAxisAlignedClip");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE sink_PopLayer(ID2D1CommandSink *This)
{
    (void)This;
    record("%s\n","cmd PopLayer");
    return S_OK;
}
static const ID2D1CommandSinkVtbl sink_vtbl = {
    sink_QueryInterface,
    sink_AddRef,
    sink_Release,
    sink_BeginDraw,
    sink_EndDraw,
    sink_SetAntialiasMode,
    sink_SetTags,
    sink_SetTextAntialiasMode,
    sink_SetTextRenderingParams,
    sink_SetTransform,
    sink_SetPrimitiveBlend,
    sink_SetUnitMode,
    sink_Clear,
    sink_DrawGlyphRun,
    sink_DrawLine,
    sink_DrawGeometry,
    sink_DrawRectangle,
    sink_DrawBitmap,
    sink_DrawImage,
    sink_DrawGdiMetafile,
    sink_FillMesh,
    sink_FillOpacityMask,
    sink_FillGeometry,
    sink_FillRectangle,
    sink_PushAxisAlignedClip,
    sink_PushLayer,
    sink_PopAxisAlignedClip,
    sink_PopLayer
};

#define REQUIRE_CONTEXT(call) do { hr = (call); ok(hr == S_OK,"%s: %#lx.\n",#call,hr); if (FAILED(hr)) goto done; } while(0)
static void target_state(ID2D1DeviceContext *dc, ID2D1CommandList *list, unsigned int id)
{
    ID2D1Image *image;
    ID2D1DeviceContext_GetTarget(dc,&image);
    record("target %u %u\n",id,image == (ID2D1Image *)list ? 1 : image ? 2 : 0);
    if (image) ID2D1Image_Release(image);
}
static void test_contexts(void)
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
    D3D_DRIVER_TYPE driver = effect_test_driver();
    HRESULT hr;
    ID2D1DeviceContext *saved_replay = replay;
    unsigned int i;
    replay = NULL;
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    REQUIRE_CONTEXT(D3D11CreateDevice(NULL,driver,NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,NULL,0,D3D11_SDK_VERSION,&d3d,NULL,NULL));
    REQUIRE_CONTEXT(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));
    REQUIRE_CONTEXT(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    REQUIRE_CONTEXT(ID2D1Factory1_CreateDevice(factory,dxgi,&device));
    REQUIRE_CONTEXT(ID2D1Factory1_CreateDevice(factory,dxgi,&other));
    for (i = 0; i < 7; ++i)
    {
        REQUIRE_CONTEXT(ID2D1Device_CreateDeviceContext(device,0,&first));
        REQUIRE_CONTEXT(ID2D1Device_CreateDeviceContext(i == 1 ? other : device,0,&second));
        REQUIRE_CONTEXT(ID2D1DeviceContext_CreateCommandList(first,&list));
        REQUIRE_CONTEXT(ID2D1DeviceContext_CreateSolidColorBrush(second,&color,NULL,&brush));
        record("case %u\n",i);
        if ((i >= 2 && i <= 4) || i == 6)
        {
            ID2D1DeviceContext_SetTarget(first,(ID2D1Image *)list);
            if (i != 4)
            {
                ID2D1DeviceContext_BeginDraw(first);
                ID2D1DeviceContext_FillRectangle(first,&rect,(ID2D1Brush *)brush);
                if (i != 6) ID2D1DeviceContext_SetTarget(first,NULL);
            }
            if (i == 3) record("end_first %08lx\n",ID2D1DeviceContext_EndDraw(first,NULL,NULL));
        }
        if (i == 5) { ID2D1DeviceContext_Release(first); first = NULL; }
        ID2D1DeviceContext_SetTarget(second,(ID2D1Image *)list);
        target_state(second,list,2);
        if (i != 4)
        {
            ID2D1DeviceContext_BeginDraw(second);
            ID2D1DeviceContext_FillRectangle(second,&rect,(ID2D1Brush *)brush);
            record("end_second %08lx\n",ID2D1DeviceContext_EndDraw(second,NULL,NULL));
        }
        if (i == 2 || i == 6) record("end_first %08lx\n",ID2D1DeviceContext_EndDraw(first,NULL,NULL));
        record("close %08lx\n",ID2D1CommandList_Close(list));
        if (first) target_state(first,list,1);
        target_state(second,list,2);
        hr = ID2D1CommandList_Stream(list,&sink);
        record("stream %08lx\n",hr);
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
    replay = saved_replay;
}

#undef REQUIRE_CONTEXT

START_TEST(command_list_state)
{
    static const char *sequences[] = {
        "CSC S", "LCS", "LBFECS", "bBLFbCSE", "NBLFNCSE", "LFCS",
        "LBFEBFECS", "bBLFbXLFbCSE", "LBF CSES", "LBF CFES",
        "LBFE CBFES", "bBLFNLFNCSE", "LBXFECS", "LSBFSECS", "BLFECS",
        "LBFE X BFECS", "LX BFECS", "LBFE FCS", "LBF C LBFES", "LBFE CLS",
        "bBLFbNF CSE", "bBLFYZFbCSE", "LBFYFECS",
    };
    ID3D11Device *d3d = NULL;
    IDXGIDevice *dxgi = NULL;
    ID2D1Factory1 *factory = NULL;
    ID2D1Device *device = NULL;
    ID2D1DeviceContext *dc = NULL;
    ID2D1CommandList *list = NULL;
    ID2D1Image *bound;
    ID2D1Bitmap1 *target = NULL, *replay_target = NULL, *readback = NULL;
    ID2D1SolidColorBrush *brush = NULL;
    ID2D1CommandSink sink = {&sink_vtbl};
    D2D1_BITMAP_PROPERTIES1 props = {{DXGI_FORMAT_R32G32B32A32_FLOAT,D2D1_ALPHA_MODE_PREMULTIPLIED},96,96,D2D1_BITMAP_OPTIONS_TARGET,NULL};
    D2D1_SIZE_U size = {8,8};
    D2D1_COLOR_F color = {0.25,0.5,0.75,0.5}, red = {1,0,0,1}, clear = {0};
    D2D1_MAPPED_RECT mapped;
    D2D1_MATRIX_3X2_F identity = {.m11=1,.m22=1}, translated = {.m11=1,.m22=1,.dx=2,.dy=1};
    D2D1_RECT_F rect = {1,1,3,3};
    D2D1_MATRIX_3X2_F transform = {.m11=1,.m22=1,.dx=5,.dy=7};
    D2D1_TAG tag1,tag2;
    HRESULT hr = S_OK;
    unsigned int i,j;
    D3D_DRIVER_TYPE driver = effect_test_driver();
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
#define REQUIRE(call) do { hr = (call); ok(hr == S_OK,"%s: %#lx.\n",#call,hr); if (FAILED(hr)) goto done; } while(0)
    REQUIRE(D3D11CreateDevice(NULL,driver,NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,NULL,0,D3D11_SDK_VERSION,&d3d,NULL,NULL));
    REQUIRE(ID3D11Device_QueryInterface(d3d,&IID_IDXGIDevice,(void **)&dxgi));
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    REQUIRE(ID2D1Factory1_CreateDevice(factory,dxgi,&device));
    REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&replay));
    REQUIRE(ID2D1DeviceContext_CreateBitmap(replay,size,NULL,0,&props,&replay_target));
    props.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    REQUIRE(ID2D1DeviceContext_CreateBitmap(replay,size,NULL,0,&props,&readback));
    props.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
    ID2D1DeviceContext_SetTarget(replay,(ID2D1Image *)replay_target);
    for (i = 0; i < sizeof(sequences)/sizeof(*sequences); ++i)
    {
        REQUIRE(ID2D1Device_CreateDeviceContext(device,0,&dc));
        REQUIRE(ID2D1DeviceContext_CreateBitmap(dc,size,NULL,0,&props,&target));
        REQUIRE(ID2D1DeviceContext_CreateCommandList(dc,&list));
        REQUIRE(ID2D1DeviceContext_CreateSolidColorBrush(dc,&color,NULL,&brush));
        ID2D1DeviceContext_SetAntialiasMode(dc,D2D1_ANTIALIAS_MODE_ALIASED);
        ID2D1DeviceContext_SetTags(dc,10,20);
        record("case %u %s\n",i,sequences[i]);
        for (j = 0; sequences[i][j]; ++j)
        {
            switch (sequences[i][j])
            {
                case 'B': ID2D1DeviceContext_BeginDraw(dc); break;
                case 'b': ID2D1DeviceContext_SetTarget(dc,(ID2D1Image *)target); break;
                case 'L': ID2D1DeviceContext_SetTarget(dc,(ID2D1Image *)list); break;
                case 'N': ID2D1DeviceContext_SetTarget(dc,NULL); break;
                case 'X': ID2D1DeviceContext_SetTransform(dc,&transform); break;
                case 'Y': ID2D1SolidColorBrush_SetColor(brush,&red); break;
                case 'Z': ID2D1DeviceContext_SetTransform(dc,&translated); break;
                case 'F': ID2D1DeviceContext_FillRectangle(dc,&rect,(ID2D1Brush *)brush); break;
                case 'E':
                    tag1 = tag2 = 0xdeadbeef;
                    hr = ID2D1DeviceContext_EndDraw(dc,&tag1,&tag2);
                    record("end %08lx %llu %llu\n",hr,tag1,tag2); break;
                case 'C':
                    record("close %08lx\n",ID2D1CommandList_Close(list));
                    ID2D1DeviceContext_GetTarget(dc,&bound);
                    record("target %u\n",bound == (ID2D1Image *)list ? 1 : bound == (ID2D1Image *)target ? 2 : bound ? 3 : 0);
                    if (bound) ID2D1Image_Release(bound);
                    break;
                case 'S':
                    ID2D1DeviceContext_BeginDraw(replay);
                    ID2D1DeviceContext_SetTransform(replay,&identity);
                    ID2D1DeviceContext_Clear(replay,&clear);
                    REQUIRE(ID2D1DeviceContext_EndDraw(replay,NULL,NULL));
                    record("%s\n","stream_begin");
                    hr = ID2D1CommandList_Stream(list,&sink);
                    record("stream_end %08lx\n",hr);
                    REQUIRE(ID2D1Bitmap1_CopyFromBitmap(readback,NULL,(ID2D1Bitmap *)replay_target,NULL));
                    REQUIRE(ID2D1Bitmap1_Map(readback,D2D1_MAP_OPTIONS_READ,&mapped));
                    check_pixels(&mapped);
                    ID2D1Bitmap1_Unmap(readback);
                    break;
            }
        }
        ID2D1DeviceContext_SetTarget(dc,NULL);
        ID2D1SolidColorBrush_Release(brush); brush = NULL;
        ID2D1CommandList_Release(list); list = NULL;
        ID2D1Bitmap1_Release(target); target = NULL;
        ID2D1DeviceContext_Release(dc); dc = NULL;
    }
    test_contexts();
    ok(record_index == ARRAY_SIZE(expected_records),"Read %u records, expected %u.\n",record_index,(unsigned int)ARRAY_SIZE(expected_records));
    ok(image_index == ARRAY_SIZE(expected_images),"Read %u images, expected %u.\n",image_index,(unsigned int)ARRAY_SIZE(expected_images));
done:
    if (replay) ID2D1DeviceContext_SetTarget(replay,NULL);
    if (readback) ID2D1Bitmap1_Release(readback);
    if (replay_target) ID2D1Bitmap1_Release(replay_target);
    if (replay) ID2D1DeviceContext_Release(replay);
    if (dc) ID2D1DeviceContext_SetTarget(dc,NULL);
    if (brush) ID2D1SolidColorBrush_Release(brush);
    if (list) ID2D1CommandList_Release(list);
    if (target) ID2D1Bitmap1_Release(target);
    if (dc) ID2D1DeviceContext_Release(dc);
    if (device) ID2D1Device_Release(device);
    if (factory) ID2D1Factory1_Release(factory);
    if (dxgi) IDXGIDevice_Release(dxgi);
    if (d3d) ID3D11Device_Release(d3d);
    CoUninitialize();
}
