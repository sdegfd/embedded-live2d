/** @file @brief 基础几何图形绘制接口声明 */

#ifndef PIXELSES_BASEGEO
#define PIXELSES_BASEGEO

#include "PX_Surface.h"

/** 绘制线段 */
px_void PX_GeoDrawLine(px_surface *psurface, px_int x0, px_int y0, px_int x1, px_int y1,
                       px_int lineWidth, px_color color);
/** 绘制矩形边框 */
px_void PX_GeoDrawBorder(px_surface *psurface, px_int left, px_int top, px_int right,
                         px_int bottom, px_int lineWidth, px_color color);
/** 绘制实心圆 */
px_void PX_GeoDrawSolidCircle(px_surface *psurface, px_int x, px_int y, px_int Radius,
                              px_color color);
/** 绘制空心圆 */
px_void PX_GeoDrawCircle(px_surface *psurface, px_int x, px_int y, px_int Radius,
                         px_int lineWidth, px_color color);
/** 绘制箭头 */
px_void PX_GeoDrawArrow(px_surface *psurface, px_point2D p0, px_point2D p1,
                        px_float size, px_color color);

#endif
