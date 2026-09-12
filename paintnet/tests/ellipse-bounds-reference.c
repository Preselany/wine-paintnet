/* Public ellipse geometry bounds measurements.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define main color_context_base_main
#include "color-context-reference.c"
#undef main
int main(void)
{
    ID2D1Factory1 *factory;
    ID2D1EllipseGeometry *geometry;
    static const float radii[][2] = {{2,3},{-2,3},{2,-3},{-2,-3},{0,3},{2,0},{0,0}};
    D2D1_MATRIX_3X2_F matrices[] = {{.m11=1,.m22=1},{.m11=1.5f,.m12=.25f,.m21=-.5f,.m22=1,.dx=-3,.dy=1},
        {.m11=0,.m22=0},{.m11=.6f,.m12=.8f,.m21=-.8f,.m22=.6f,.dx=4,.dy=-2}};
    D2D1_ELLIPSE ellipse={{4,5},0,0};
    D2D1_RECT_F bounds;
    HRESULT hr;
    unsigned int r,m;
    setvbuf(stdout,NULL,_IOFBF,65536);
    REQUIRE(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&IID_ID2D1Factory1,NULL,(void **)&factory));
    for(r=0;r<ARRAY_SIZE(radii);++r)
    {
        ellipse.radiusX=radii[r][0];ellipse.radiusY=radii[r][1];
        REQUIRE(ID2D1Factory1_CreateEllipseGeometry(factory,&ellipse,&geometry));
        for(m=0;m<=ARRAY_SIZE(matrices);++m)
        {
            memset(&bounds,0xcc,sizeof(bounds));
            hr=ID2D1EllipseGeometry_GetBounds(geometry,m?&matrices[m-1]:NULL,&bounds);
            printf("bounds %u %u %08lx %.9g %.9g %.9g %.9g\n",r,m,hr,bounds.left,bounds.top,bounds.right,bounds.bottom);
        }
        ID2D1EllipseGeometry_Release(geometry);
    }
    ID2D1Factory1_Release(factory);puts("done");return 0;
}
