/** @file PX_Texture.c
 *  @brief PainterEngine 纹理模块实现
 *  提供纹理的创建、渲染和释放功能。纹理基于 px_surface 实现，支持定点对齐和HDR颜色混合。
 */

#include "PX_Texture.h"

/**
 * 构建纹理渲染混合参数
 * @param hdr_R HDR红色通道增益系数
 * @param hdr_G HDR绿色通道增益系数
 * @param hdr_B HDR蓝色通道增益系数
 * @param alpha 透明度混合系数
 * @return 构建完成的混合结构体
 */
PX_TEXTURERENDER_BLEND PX_TEXTURERENDER_BLEND_BUILD(px_float hdr_R, px_float hdr_G,
                                                    px_float hdr_B, px_float alpha)
{
    PX_TEXTURERENDER_BLEND blend;
    blend.hdr_R = hdr_R;   /**< HDR红通道系数 */
    blend.hdr_G = hdr_G;   /**< HDR绿通道系数 */
    blend.hdr_B = hdr_B;   /**< HDR蓝通道系数 */
    blend.alpha = alpha;   /**< 透明度系数 */
    return blend;
}

/**
 * 创建纹理
 * @param mp 内存池指针
 * @param tex 纹理对象指针
 * @param width 纹理宽度（像素）
 * @param height 纹理高度（像素）
 * @return 创建成功返回PX_TRUE，失败返回PX_FALSE
 */
px_bool PX_TextureCreate(px_memorypool *mp, px_texture *tex, px_int width, px_int height)
{
    return PX_SurfaceCreate(mp, width, height, tex);
}

/**
 * 对像素颜色应用HDR混合效果
 * @param color 原始像素颜色
 * @param blend 混合参数指针，若为NULL则直接返回原始颜色
 * @return 混合后的像素颜色
 */
static px_color PX_TextureApplyBlend(px_color color, PX_TEXTURERENDER_BLEND *blend)
{
    if (!blend) {
        return color;
    }

    /* 分别对ARGB四个通道应用混合系数 */
    px_int a = (px_int)(color._argb.a * blend->alpha);
    px_int r = (px_int)(color._argb.r * blend->hdr_R);
    px_int g = (px_int)(color._argb.g * blend->hdr_G);
    px_int b = (px_int)(color._argb.b * blend->hdr_B);

    /* 将结果钳制到[0, 255]范围内 */
    if (a > 255) a = 255;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    if (a < 0) a = 0;
    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;

    return PX_COLOR((px_uchar)a, (px_uchar)r, (px_uchar)g, (px_uchar)b);
}

/**
 * 渲染纹理到目标表面
 * @param psurface 目标表面指针
 * @param tex 纹理指针
 * @param x 目标位置的X坐标
 * @param y 目标位置的Y坐标
 * @param refPoint 对齐参考点（左上角、中心等）
 * @param blend 混合参数指针，可选
 */
px_void PX_TextureRender(px_surface *psurface, px_texture *tex, px_int x, px_int y,
                         PX_ALIGN refPoint, PX_TEXTURERENDER_BLEND *blend)
{
    if (!psurface || !tex || !tex->surfaceBuffer) {
        return;
    }

    /* ── 根据对齐参考点调整渲染位置 ──────────────────── */
    switch (refPoint) {
    case PX_ALIGN_MIDTOP:
        x -= tex->width / 2;
        break;
    case PX_ALIGN_RIGHTTOP:
        x -= tex->width;
        break;
    case PX_ALIGN_LEFTMID:
        y -= tex->height / 2;
        break;
    case PX_ALIGN_CENTER:
        x -= tex->width / 2;
        y -= tex->height / 2;
        break;
    case PX_ALIGN_RIGHTMID:
        x -= tex->width;
        y -= tex->height / 2;
        break;
    case PX_ALIGN_LEFTBOTTOM:
        y -= tex->height;
        break;
    case PX_ALIGN_MIDBOTTOM:
        x -= tex->width / 2;
        y -= tex->height;
        break;
    case PX_ALIGN_RIGHTBOTTOM:
        x -= tex->width;
        y -= tex->height;
        break;
    case PX_ALIGN_LEFTTOP:
    default:
        break;
    }

    /* ── 计算渲染矩形范围，并进行裁剪 ──────────────── */
    px_int left = x;
    px_int top = y;
    px_int right = x + tex->width - 1;
    px_int bottom = y + tex->height - 1;

    /* 裁剪至目标表面的有效区域 */
    if (left < psurface->limit_left) left = psurface->limit_left;
    if (top < psurface->limit_top) top = psurface->limit_top;
    if (right > psurface->limit_right) right = psurface->limit_right;
    if (bottom > psurface->limit_bottom) bottom = psurface->limit_bottom;
    if (left > right || top > bottom) {
        return;
    }

    /* ── 逐像素渲染纹理 ────────────────────────────── */
    for (px_int py = top; py <= bottom; ++py) {
        px_int sy = py - y;      /* 纹理中的Y偏移 */
        for (px_int px = left; px <= right; ++px) {
            px_int sx = px - x;  /* 纹理中的X偏移 */
            px_color color = tex->surfaceBuffer[sy * tex->width + sx];
            color = PX_TextureApplyBlend(color, blend);
            PX_SurfaceDrawPixel(psurface, px, py, color);
        }
    }
}

/**
 * 释放纹理资源
 * @param tex 纹理对象指针
 */
px_void PX_TextureFree(px_texture *tex)
{
    if (!tex || !tex->surfaceBuffer || !tex->MP) {
        return;
    }
    PX_SurfaceFree(tex);
}
