#ifndef __PX_SURFACE_H
#define __PX_SURFACE_H
/** @file @brief PainterEngine 像素表面（画布）模块头文件 */
#include "PX_MemoryPool.h"

/** 像素表面结构，表示一个可绘制的图像画布 */
typedef struct _PX_Surface
{
	px_memorypool *MP;			/**< 所属内存池 */
	px_color *surfaceBuffer;	/**< 像素缓冲区（ARGB格式） */
	px_ushort *rgb565_sink; /* Optional terminal output; blends remain BGRA in scratch. */
	px_color *bgra_capture; /* Optional correctness copy, never used as a blend target. */
	px_int buffer_y0; /* Raster scratch buffer origin; logical y stays unchanged. */
	px_uint row_step, row_phase; /* 0: all rows; otherwise disjoint worker rows. */
	px_int height;				/**< 表面高度 */
	px_int width;				/**< 表面宽度 */
	px_int limit_left;			/**< 绘制区域左边界限制 */
	px_int limit_top;			/**< 绘制区域上边界限制 */
	px_int limit_right;		/**< 绘制区域右边界限制 */
	px_int limit_bottom;		/**< 绘制区域下边界限制 */
}px_surface;
static inline px_uint PX_SurfacePixelIndex(const px_surface *s, px_uint x, px_uint y)
{
    return y * (px_uint)s->width + x;
}
#define PX_SURFACECOLOR(main_pSurface,X,Y) ((main_pSurface)->surfaceBuffer[PX_SurfacePixelIndex(main_pSurface,(X),(Y))]) /**< 获取表面指定坐标的像素颜色 */
#define PX_SurfaceGetPixel(main_pSurface,X,Y)  PX_SURFACECOLOR(main_pSurface,X,Y) /**< 获取像素的便捷宏 */

/** 绘制区域限制信息结构 */
typedef struct
{
	px_int limit_left;		/**< 左边界 */
	px_int limit_top;		/**< 上边界 */
	px_int limit_right;	/**< 右边界 */
	px_int limit_bottom;	/**< 下边界 */
}PX_SurfaceLimitInfo;

/** 获取当前绘制区域限制 */
PX_SurfaceLimitInfo PX_SurfaceGetLimit(px_surface *ps);
/** 通过结构体设置绘制区域限制 */
px_void PX_SurfaceSetLimitInfo(px_surface *ps,PX_SurfaceLimitInfo info);
/** 设置绘制区域限制（自动裁剪越界值） */
px_void PX_SurfaceSetLimit(px_surface *ps,px_int limit_left,px_int limit_top,px_int limit_right,px_int limit_bottom);
/** 设置像素颜色（带区域限制检查） */
px_void PX_SurfaceSetPixel(px_surface *ps,px_int x,px_int y,px_color color);
/** 绘制像素（带区域限制，支持Alpha混合） */
px_void PX_SurfaceDrawPixel(px_surface *ps,px_int x,px_int y,px_color color);
/** 绘制像素（不带区域限制，支持Alpha混合） */
px_void PX_SurfaceDrawPixelWithoutLimit(px_surface* psurface, px_int X, px_int Y, px_color COLOR);
/** 创建像素表面 */
px_bool PX_SurfaceCreate(px_memorypool *mp,px_int width,px_int height,_OUT px_surface *surface);
/** 解除绘制区域限制 */
px_void PX_SurfaceUnlimit(px_surface* psurface);
/** 清除指定矩形区域为指定颜色 */
px_void PX_SurfaceClear(px_surface* ps, px_int left, px_int top, px_int right, px_int bottom, px_color color);
/** 清除整个表面为指定颜色 */
px_void PX_SurfaceClearAll(px_surface* psurface, px_color color);
/** 释放像素表面 */
px_void PX_SurfaceFree(px_surface* psurface);
/** 计算表面所需内存大小 */
px_int PX_SurfaceMemorySize(px_uint width,px_uint height);
#endif
