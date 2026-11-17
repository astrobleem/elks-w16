#ifndef W16_POLYGON_H
#define W16_POLYGON_H
/* Bounded ALTERNATE polygons, solid hairline pen and solid brush.
 * Negative point count means open Polyline: no fill and no closing edge. */
struct poly_point { int x,y; };
typedef void (*poly_span)(void *,int,int,int,int);
int polygon_raster(struct poly_point *,int,int,int,int,int,poly_span,void *);
int oval_raster(struct poly_point *,int,int,int,int,poly_span,void *);
#endif
