/** @file PX_Texture.h
 *  @brief PainterEngine 纹理模块头文件
 *  纹理是对 px_surface 的封装，提供纹理创建、渲染和释放等功能。
 *  支持 HDR 颜色混合和对齐参考点渲染。
 */

#ifndef __PX_TEXTURE_H
#define __PX_TEXTURE_H

#include "PX_Surface.h"

/** 纹理类型，本质为表面(surface)的别名 */
typedef px_surface px_texture;

/**
 * @brief 纹理渲染混合参数结构体
 * 用于控制纹理渲染时的颜色混合效果
 */
typedef struct
{
    px_float hdr_R;   /**< HDR红色通道增益系数 */
    px_float hdr_G;   /**< HDR绿色通道增益系数 */
    px_float hdr_B;   /**< HDR蓝色通道增益系数 */
    px_float alpha;   /**< 透明度混合系数（0.0~1.0） */
} PX_TEXTURERENDER_BLEND;

/**
 * @brief 纹理镜像模式枚举
 */
typedef enum
{
    PX_TEXTURERENDER_MIRRROR_MODE_NONE,  /**< 无镜像 */
    PX_TEXTURERENDER_MIRRROR_MODE_H,     /**< 水平镜像 */
    PX_TEXTURERENDER_MIRRROR_MODE_V,     /**< 垂直镜像 */
    PX_TEXTURERENDER_MIRRROR_MODE_HV,    /**< 水平和垂直镜像 */
} PX_TEXTURERENDER_MIRRROR_MODE;

/**
 * 构建纹理渲染混合参数
 * @param hdr_R HDR红色增益系数
 * @param hdr_G HDR绿色增益系数
 * @param hdr_B HDR蓝色增益系数
 * @param alpha 透明度混合系数
 * @return 混合参数结构体
 */
PX_TEXTURERENDER_BLEND PX_TEXTURERENDER_BLEND_BUILD(px_float hdr_R, px_float hdr_G,
                                                    px_float hdr_B, px_float alpha);

/**
 * 创建纹理
 * @param mp 内存池指针
 * @param tex 纹理对象指针
 * @param width 纹理宽度
 * @param height 纹理高度
 * @return 创建成功返回PX_TRUE，失败返回PX_FALSE
 */
px_bool PX_TextureCreate(px_memorypool *mp, px_texture *tex, px_int width, px_int height);

/**
 * 渲染纹理到目标表面
 * @param psurface 目标表面指针
 * @param tex 源纹理指针
 * @param x 目标X坐标
 * @param y 目标Y坐标
 * @param refPoint 对齐参考点
 * @param blend 混合参数指针（可选）
 */
px_void PX_TextureRender(px_surface *psurface, px_texture *tex, px_int x, px_int y,
                         PX_ALIGN refPoint, PX_TEXTURERENDER_BLEND *blend);

/**
 * 释放纹理资源
 * @param tex 纹理对象指针
 */
px_void PX_TextureFree(px_texture *tex);

/** 清空纹理内容（宏，映射至表面清空操作） */
#define PX_TextureClear PX_SurfaceClear
/** 使用指定颜色填充整个纹理（宏） */
#define PX_TextureClearAll(ptexture, color) PX_SurfaceClearAll(ptexture, color)
/** 获取纹理指定像素的颜色（宏） */
#define PX_TextureGetPixel(ptexture, x, y) PX_SurfaceGetPixel(ptexture, x, y)

#endif
