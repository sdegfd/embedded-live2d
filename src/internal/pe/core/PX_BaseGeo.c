/** @file @brief 基础几何图形绘制函数（桩实现/占位实现） */

#include "PX_BaseGeo.h"

/**
 * @brief 绘制线段（桩实现）
 * @param psurface 目标表面
 * @param x0 起点 x 坐标
 * @param y0 起点 y 坐标
 * @param x1 终点 x 坐标
 * @param y1 终点 y 坐标
 * @param lineWidth 线宽
 * @param color 颜色
 */
px_void PX_GeoDrawLine(px_surface *psurface, px_int x0, px_int y0, px_int x1, px_int y1,
                       px_int lineWidth, px_color color)
{
    (void)psurface;
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
    (void)lineWidth;
    (void)color;
}

/**
 * @brief 绘制矩形边框（桩实现）
 * @param psurface 目标表面
 * @param left 左边界坐标
 * @param top 上边界坐标
 * @param right 右边界坐标
 * @param bottom 下边界坐标
 * @param lineWidth 线宽
 * @param color 颜色
 */
px_void PX_GeoDrawBorder(px_surface *psurface, px_int left, px_int top, px_int right,
                         px_int bottom, px_int lineWidth, px_color color)
{
    (void)psurface;
    (void)left;
    (void)top;
    (void)right;
    (void)bottom;
    (void)lineWidth;
    (void)color;
}

/**
 * @brief 绘制实心圆（桩实现）
 * @param psurface 目标表面
 * @param x 圆心 x 坐标
 * @param y 圆心 y 坐标
 * @param Radius 半径
 * @param color 颜色
 */
px_void PX_GeoDrawSolidCircle(px_surface *psurface, px_int x, px_int y, px_int Radius,
                              px_color color)
{
    (void)psurface;
    (void)x;
    (void)y;
    (void)Radius;
    (void)color;
}

/**
 * @brief 绘制空心圆（桩实现）
 * @param psurface 目标表面
 * @param x 圆心 x 坐标
 * @param y 圆心 y 坐标
 * @param Radius 半径
 * @param lineWidth 线宽
 * @param color 颜色
 */
px_void PX_GeoDrawCircle(px_surface *psurface, px_int x, px_int y, px_int Radius,
                         px_int lineWidth, px_color color)
{
    (void)psurface;
    (void)x;
    (void)y;
    (void)Radius;
    (void)lineWidth;
    (void)color;
}

/**
 * @brief 绘制箭头（桩实现）
 * @param psurface 目标表面
 * @param p0 起点坐标
 * @param p1 终点坐标
 * @param size 箭头大小
 * @param color 颜色
 */
px_void PX_GeoDrawArrow(px_surface *psurface, px_point2D p0, px_point2D p1,
                        px_float size, px_color color)
{
    (void)psurface;
    (void)p0;
    (void)p1;
    (void)size;
    (void)color;
}
