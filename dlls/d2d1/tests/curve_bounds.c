/* Native ellipse and rounded-rectangle bounds regressions.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <math.h>
#include <string.h>
#include "wine/test.h"
#include "d2d1_1.h"
#define REQUIRE(call) do {hr=(call);ok(hr==S_OK,"%s returned %#lx.\n",#call,hr);if(FAILED(hr))return;}while(0)

static void check_bounds(const D2D1_RECT_F *actual, const D2D1_RECT_F *expected)
{
    unsigned int i;
    for(i=0;i<4;++i)
        ok(fabsf(((const float *)actual)[i]-((const float *)expected)[i])<0.000002f,
                "Bound %u is %.9g, expected %.9g.\n",i,((const float *)actual)[i],((const float *)expected)[i]);
}
static const D2D1_RECT_F ellipse_expected[] = {
    {2, 2, 6, 8},
    {2, 2, 6, 8},
    {-2.85483646, 3.95814419, 3.85483694, 10.0418558},
    {0, 0, 0, 0},
    {-0.283869356, 1.79165518, 5.08386993, 6.60834455},
    {2, 2, 6, 8},
    {2, 2, 6, 8},
    {-2.85483646, 3.95814419, 3.85483694, 10.0418558},
    {0, 0, 0, 0},
    {-0.283869356, 1.79165518, 5.08386993, 6.60834455},
    {2, 2, 6, 8},
    {2, 2, 6, 8},
    {-2.85483646, 3.95814419, 3.85483694, 10.0418558},
    {0, 0, 0, 0},
    {-0.283869356, 1.79165518, 5.08386993, 6.60834455},
    {2, 2, 6, 8},
    {2, 2, 6, 8},
    {-2.85483646, 3.95814419, 3.85483694, 10.0418558},
    {0, 0, 0, 0},
    {-0.283869356, 1.79165518, 5.08386993, 6.60834455},
    {4, 2, 4, 8},
    {4, 2, 4, 8},
    {-1, 4, 2, 10},
    {0, 0, 0, 0},
    {0, 2.4000001, 4.80000019, 6},
    {2, 5, 6, 5},
    {2, 5, 6, 5},
    {-2.5, 6.5, 3.5, 7.5},
    {0, 0, 0, 0},
    {1.20000005, 2.5999999, 3.60000014, 5.80000019},
    {4, 5, 4, 5},
    {4, 5, 4, 5},
    {0.5, 7, 0.5, 7},
    {0, 0, 0, 0},
    {2.4000001, 4.19999981, 2.4000001, 4.19999981},
};

static void test_ellipse(void)
{
    ID2D1Factory1 *factory;
    ID2D1EllipseGeometry *geometry;
    static const float radii[][2] = {{2,3},{-2,3},{2,-3},{-2,-3},{0,3},{2,0},{0,0}};
    D2D1_MATRIX_3X2_F matrices[] = {{.m11=1,.m22=1},{.m11=1.5f,.m12=.25f,.m21=-.5f,.m22=1,.dx=-3,.dy=1},
        {.m11=0,.m22=0},{.m11=.6f,.m12=.8f,.m21=-.8f,.m22=.6f,.dx=4,.dy=-2}};
    D2D1_ELLIPSE ellipse={{4,5},0,0};
    D2D1_RECT_F bounds;
    ID2D1TransformedGeometry *wrapped;
    D2D1_MATRIX_3X2_F identity={.m11=1,.m22=1};
    unsigned int index=0;
    HRESULT hr;
    unsigned int r,m;
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    for(r=0;r<ARRAY_SIZE(radii);++r)
    {
        ellipse.radiusX=radii[r][0];ellipse.radiusY=radii[r][1];
        REQUIRE(ID2D1Factory1_CreateEllipseGeometry(factory,&ellipse,&geometry));
        for(m=0;m<=ARRAY_SIZE(matrices);++m)
        {
            memset(&bounds,0xcc,sizeof(bounds));
            hr=ID2D1EllipseGeometry_GetBounds(geometry,m?&matrices[m-1]:NULL,&bounds);
            winetest_push_context("ellipse radius %u matrix %u",r,m);
            ok(hr==S_OK,"GetBounds returned %#lx.\n",hr);
            check_bounds(&bounds,&ellipse_expected[index]);
            REQUIRE(ID2D1Factory1_CreateTransformedGeometry(factory,(ID2D1Geometry *)geometry,m?&matrices[m-1]:&identity,&wrapped));
            REQUIRE(ID2D1TransformedGeometry_GetBounds(wrapped,NULL,&bounds));
            check_bounds(&bounds,&ellipse_expected[index]);
            ID2D1TransformedGeometry_Release(wrapped);
            ++index;winetest_pop_context();
        }
        ID2D1EllipseGeometry_Release(geometry);
    }
    ID2D1Factory1_Release(factory);
}

static const D2D1_RECT_F rounded_expected[] = {
    {1, 2, 9, 8},
    {1, 2, 9, 8},
    {-4.35483646, 3.70814395, 8.35483742, 10.7918549},
    {0, 0, 0, 0},
    {-0.883869231, 0.99165529, 6.88386917, 9.00834465},
    {1, 2, 9, 8},
    {1, 2, 9, 8},
    {-4.35483646, 3.70814395, 8.35483742, 10.7918549},
    {0, 0, 0, 0},
    {-0.883869231, 0.99165529, 6.88386917, 9.00834465},
    {1, 2, 9, 8},
    {1, 2, 9, 8},
    {-4.35483646, 3.70814395, 8.35483742, 10.7918549},
    {0, 0, 0, 0},
    {-0.883869231, 0.99165529, 6.88386917, 9.00834465},
    {1, 2, 9, 8},
    {1, 2, 9, 8},
    {-4.35483646, 3.70814395, 8.35483742, 10.7918549},
    {0, 0, 0, 0},
    {-0.883869231, 0.99165529, 6.88386917, 9.00834465},
    {1, 2, 9, 8},
    {1, 2, 9, 8},
    {-5.5, 3.25, 9.5, 11.25},
    {0, 0, 0, 0},
    {-1.80000019, 0, 7.80000019, 10},
    {1, 2, 9, 8},
    {1, 2, 9, 8},
    {-5.5, 3.25, 9.5, 11.25},
    {0, 0, 0, 0},
    {-1.80000019, 0, 7.80000019, 10},
    {1, 2, 9, 8},
    {1, 2, 9, 8},
    {-5.5, 3.25, 9.5, 11.25},
    {0, 0, 0, 0},
    {-1.80000019, 0, 7.80000019, 10},
    {1, 2, 9, 8},
    {1, 2, 9, 8},
    {-4.18612146, 4.08686447, 8.18612099, 10.4131355},
    {0, 0, 0, 0},
    {-0.394112587, 1.32783675, 6.39411259, 8.67216301},
    {1, 2, 9, 8},
    {1, 2, 9, 8},
    {-5.08156776, 3.4689796, 9.08156776, 11.0310211},
    {0, 0, 0, 0},
    {-1.40005875, 0.399941325, 7.40005922, 9.60005856},
    {1, 2, 9, 8},
    {1, 2, 9, 8},
    {-4.35483646, 3.70814395, 8.35483742, 10.7918549},
    {0, 0, 0, 0},
    {-0.883869231, 0.99165529, 6.88386917, 9.00834465},
    {1, 2, 9, 8},
    {1, 2, 9, 8},
    {-4.35483646, 3.70814395, 8.35483742, 10.7918549},
    {0, 0, 0, 0},
    {-0.883869231, 0.99165529, 6.88386917, 9.00834465},
    {1, 2, 9, 8},
    {1, 2, 9, 8},
    {-4.35483646, 3.70814395, 8.35483742, 10.7918549},
    {0, 0, 0, 0},
    {-0.883869231, 0.99165529, 6.88386917, 9.00834465},
    {1, 2, 9, 8},
    {1, 2, 9, 8},
    {-4.35483646, 3.70814395, 8.35483742, 10.7918549},
    {0, 0, 0, 0},
    {-0.883869231, 0.99165529, 6.88386917, 9.00834465},
    {1, 2, 9, 8},
    {1, 2, 9, 8},
    {-5.5, 3.25, 9.5, 11.25},
    {0, 0, 0, 0},
    {-1.80000019, 0, 7.80000019, 10},
    {1, 2, 9, 8},
    {1, 2, 9, 8},
    {-5.5, 3.25, 9.5, 11.25},
    {0, 0, 0, 0},
    {-1.80000019, 0, 7.80000019, 10},
    {1, 2, 9, 8},
    {1, 2, 9, 8},
    {-5.5, 3.25, 9.5, 11.25},
    {0, 0, 0, 0},
    {-1.80000019, 0, 7.80000019, 10},
    {1, 2, 9, 8},
    {1, 2, 9, 8},
    {-4.18612146, 4.08686447, 8.18612099, 10.4131355},
    {0, 0, 0, 0},
    {-0.394112587, 1.32783675, 6.39411259, 8.67216301},
    {1, 2, 9, 8},
    {1, 2, 9, 8},
    {-5.08156776, 3.4689796, 9.08156776, 11.0310211},
    {0, 0, 0, 0},
    {-1.40005875, 0.399941325, 7.40005922, 9.60005856},
    {1, 2, 1, 8},
    {1, 2, 1, 8},
    {-5.5, 3.25, -2.5, 9.25},
    {0, 0, 0, 0},
    {-1.80000019, 0, 3, 3.60000038},
    {1, 2, 1, 8},
    {1, 2, 1, 8},
    {-5.5, 3.25, -2.5, 9.25},
    {0, 0, 0, 0},
    {-1.80000019, 0, 3, 3.60000038},
    {1, 2, 1, 8},
    {1, 2, 1, 8},
    {-5.5, 3.25, -2.5, 9.25},
    {0, 0, 0, 0},
    {-1.80000019, 0, 3, 3.60000038},
    {1, 2, 1, 8},
    {1, 2, 1, 8},
    {-5.5, 3.25, -2.5, 9.25},
    {0, 0, 0, 0},
    {-1.80000019, 0, 3, 3.60000038},
    {1, 2, 1, 8},
    {1, 2, 1, 8},
    {-5.5, 3.25, -2.5, 9.25},
    {0, 0, 0, 0},
    {-1.80000019, 0, 3, 3.60000038},
    {1, 2, 1, 8},
    {1, 2, 1, 8},
    {-5.5, 3.25, -2.5, 9.25},
    {0, 0, 0, 0},
    {-1.80000019, 0, 3, 3.60000038},
    {1, 2, 1, 8},
    {1, 2, 1, 8},
    {-5.5, 3.25, -2.5, 9.25},
    {0, 0, 0, 0},
    {-1.80000019, 0, 3, 3.60000038},
    {1, 2, 1, 8},
    {1, 2, 1, 8},
    {-5.5, 3.25, -2.5, 9.25},
    {0, 0, 0, 0},
    {-1.80000019, 0, 3, 3.60000038},
    {1, 2, 1, 8},
    {1, 2, 1, 8},
    {-5.5, 3.25, -2.5, 9.25},
    {0, 0, 0, 0},
    {-1.80000019, 0, 3, 3.60000038},
    {1, 2, 9, 2},
    {1, 2, 9, 2},
    {-2.5, 3.25, 9.5, 5.25},
    {0, 0, 0, 0},
    {3, 0, 7.80000019, 6.40000057},
    {1, 2, 9, 2},
    {1, 2, 9, 2},
    {-2.5, 3.25, 9.5, 5.25},
    {0, 0, 0, 0},
    {3, 0, 7.80000019, 6.40000057},
    {1, 2, 9, 2},
    {1, 2, 9, 2},
    {-2.5, 3.25, 9.5, 5.25},
    {0, 0, 0, 0},
    {3, 0, 7.80000019, 6.40000057},
    {1, 2, 9, 2},
    {1, 2, 9, 2},
    {-2.5, 3.25, 9.5, 5.25},
    {0, 0, 0, 0},
    {3, 0, 7.80000019, 6.40000057},
    {1, 2, 9, 2},
    {1, 2, 9, 2},
    {-2.5, 3.25, 9.5, 5.25},
    {0, 0, 0, 0},
    {3, 0, 7.80000019, 6.40000057},
    {1, 2, 9, 2},
    {1, 2, 9, 2},
    {-2.5, 3.25, 9.5, 5.25},
    {0, 0, 0, 0},
    {3, 0, 7.80000019, 6.40000057},
    {1, 2, 9, 2},
    {1, 2, 9, 2},
    {-2.5, 3.25, 9.5, 5.25},
    {0, 0, 0, 0},
    {3, 0, 7.80000019, 6.40000057},
    {1, 2, 9, 2},
    {1, 2, 9, 2},
    {-2.5, 3.25, 9.5, 5.25},
    {0, 0, 0, 0},
    {3, 0, 7.80000019, 6.40000057},
    {1, 2, 9, 2},
    {1, 2, 9, 2},
    {-2.5, 3.25, 9.5, 5.25},
    {0, 0, 0, 0},
    {3, 0, 7.80000019, 6.40000057},
    {1, 2, 1, 2},
    {1, 2, 1, 2},
    {-2.5, 3.25, -2.5, 3.25},
    {0, 0, 0, 0},
    {3, 0, 3, 0},
    {1, 2, 1, 2},
    {1, 2, 1, 2},
    {-2.5, 3.25, -2.5, 3.25},
    {0, 0, 0, 0},
    {3, 0, 3, 0},
    {1, 2, 1, 2},
    {1, 2, 1, 2},
    {-2.5, 3.25, -2.5, 3.25},
    {0, 0, 0, 0},
    {3, 0, 3, 0},
    {1, 2, 1, 2},
    {1, 2, 1, 2},
    {-2.5, 3.25, -2.5, 3.25},
    {0, 0, 0, 0},
    {3, 0, 3, 0},
    {1, 2, 1, 2},
    {1, 2, 1, 2},
    {-2.5, 3.25, -2.5, 3.25},
    {0, 0, 0, 0},
    {3, 0, 3, 0},
    {1, 2, 1, 2},
    {1, 2, 1, 2},
    {-2.5, 3.25, -2.5, 3.25},
    {0, 0, 0, 0},
    {3, 0, 3, 0},
    {1, 2, 1, 2},
    {1, 2, 1, 2},
    {-2.5, 3.25, -2.5, 3.25},
    {0, 0, 0, 0},
    {3, 0, 3, 0},
    {1, 2, 1, 2},
    {1, 2, 1, 2},
    {-2.5, 3.25, -2.5, 3.25},
    {0, 0, 0, 0},
    {3, 0, 3, 0},
    {1, 2, 1, 2},
    {1, 2, 1, 2},
    {-2.5, 3.25, -2.5, 3.25},
    {0, 0, 0, 0},
    {3, 0, 3, 0},
};

static void test_rounded(void)
{
    ID2D1Factory1 *factory;
    ID2D1RoundedRectangleGeometry *geometry;
    static const float radii[][2] = {{2,3},{-2,3},{2,-3},{-2,-3},{0,3},{2,0},{0,0},{10,20},{1,1}};
    D2D1_MATRIX_3X2_F matrices[] = {{.m11=1,.m22=1},{.m11=1.5f,.m12=.25f,.m21=-.5f,.m22=1,.dx=-3,.dy=1},
        {.m11=0,.m22=0},{.m11=.6f,.m12=.8f,.m21=-.8f,.m22=.6f,.dx=4,.dy=-2}};
    D2D1_ROUNDED_RECT rounded={.rect={1,2,9,8}};
    static const D2D1_RECT_F rects[]={{1,2,9,8},{9,8,1,2},{1,2,1,8},{1,2,9,2},{1,2,1,2}};
    D2D1_RECT_F bounds;
    ID2D1TransformedGeometry *wrapped;
    D2D1_MATRIX_3X2_F identity={.m11=1,.m22=1};
    unsigned int index=0;
    HRESULT hr;
    unsigned int r,m,rect;
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    for(rect=0;rect<ARRAY_SIZE(rects);++rect)for(r=0;r<ARRAY_SIZE(radii);++r)
    {
        rounded.rect=rects[rect];rounded.radiusX=radii[r][0];rounded.radiusY=radii[r][1];
        REQUIRE(ID2D1Factory1_CreateRoundedRectangleGeometry(factory,&rounded,&geometry));
        for(m=0;m<=ARRAY_SIZE(matrices);++m)
        {
            memset(&bounds,0xcc,sizeof(bounds));
            hr=ID2D1RoundedRectangleGeometry_GetBounds(geometry,m?&matrices[m-1]:NULL,&bounds);
            winetest_push_context("rounded rect %u radius %u matrix %u",rect,r,m);
            ok(hr==S_OK,"GetBounds returned %#lx.\n",hr);
            check_bounds(&bounds,&rounded_expected[index]);
            REQUIRE(ID2D1Factory1_CreateTransformedGeometry(factory,(ID2D1Geometry *)geometry,m?&matrices[m-1]:&identity,&wrapped));
            REQUIRE(ID2D1TransformedGeometry_GetBounds(wrapped,NULL,&bounds));
            check_bounds(&bounds,&rounded_expected[index]);
            ID2D1TransformedGeometry_Release(wrapped);
            ++index;winetest_pop_context();
        }
        ID2D1RoundedRectangleGeometry_Release(geometry);
    }
    ID2D1Factory1_Release(factory);
}


START_TEST(curve_bounds)
{
    test_ellipse();
    test_rounded();
}
