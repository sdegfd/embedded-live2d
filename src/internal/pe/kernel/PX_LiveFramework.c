/**
 * @file PX_LiveFramework.c
 * @brief PainterEngine Live2D 框架内核实现
 *
 * 本文件实现了 Live2D 模型渲染框架的核心逻辑，包括：
 * - 顶点变换与 UV 坐标更新
 * - 三角形栅格化渲染（扫描线算法）
 * - 图层父子级联变换（旋转/拉伸/平移/弹性物理）
 * - 动画虚拟机（帧指令执行与时间插值）
 * - 导入导出序列化
 * - PX_Live 镜像 API
 * 该代码为第三方移植代码，属于 PainterEngine Live2D 渲染框架的一部分。
 */

#include "PX_LiveFramework.h"
#include "PX_LiveDeviceFormat.h"
#include "l2d_pe_port.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

/* ── 性能分析模块 ──────────────────────────────── */

static const char *PX_LIVEFRAMEWORK_PROFILE_TAG = "l2d.runtime";
static unsigned long long PX_LiveFrameworkProfileVMUs;       /**< 虚拟机耗时累计（微秒） */
static unsigned long long PX_LiveFrameworkProfilePhysicalUs;  /**< 物理更新耗时累计（微秒） */
static unsigned long long PX_LiveFrameworkProfileLayerUs;     /**< 图层渲染耗时累计（微秒） */
static unsigned long long PX_LiveFrameworkProfileSortUs;      /**< 图层排序耗时累计（微秒） */
static unsigned long long PX_LiveFrameworkProfileDrawUs;      /**< 图层绘制耗时累计（微秒） */
static unsigned long long PX_LiveFrameworkProfileLayerDetailUs[PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER];
static px_dword PX_LiveFrameworkProfileFrames;                /**< 性能采样帧数计数 */

/* Expensive workload counters are compiled out of formal timing builds. */
#if L2D_CFG_PROFILE_DETAIL
static PX_LiveFrameworkDetailFrame s_detail;
static px_int s_detailLayer=-1;
static px_uchar *s_overdraw;
static px_int s_overdrawWidth,s_overdrawHeight;
static px_void PX_LiveFrameworkDetailFragment(px_int x,px_int y,px_int alpha,px_bool fourZero)
{
    PX_LiveFrameworkLayerWork *w;
    if (s_detailLayer<0||s_detailLayer>=PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER) return;
    w=&s_detail.layers[s_detailLayer];
    w->fragments++;
    w->samplerCalls++;
    if (fourZero) w->fourTapAlphaZero++;
    if (alpha<=0) w->alphaZero++;
    else if (alpha>=255) w->alphaOpaque++;
    else w->alphaMixed++;
    if (s_overdraw&&x>=0&&y>=0&&x<s_overdrawWidth&&y<s_overdrawHeight)
    {
        px_uchar *v=s_overdraw+y*s_overdrawWidth+x;
        if (*v==0) s_detail.coveredPixels++;
        if (*v<255) ++*v;
        if (*v>s_detail.maxOverdraw) s_detail.maxOverdraw=*v;
        s_detail.fragmentOverdrawSum++;
    }
}
#define DETAIL_TRIANGLE() do { if (s_detailLayer>=0&&s_detailLayer<PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER) s_detail.layers[s_detailLayer].triangles++; } while(0)
#define DETAIL_FRAGMENT(x,y,a,z) PX_LiveFrameworkDetailFragment((x),(y),(a),(z))
static px_void PX_LiveFrameworkDetailSpan(px_int length, px_int s, px_int t,
                                          px_int ds, px_int dt, px_int width, px_int height)
{
    int64_t s_end=(int64_t)s+(int64_t)ds*(length-1);
    int64_t t_end=(int64_t)t+(int64_t)dt*(length-1);
    int64_t s_lo=s<s_end?s:s_end, s_hi=s>s_end?s:s_end;
    int64_t t_lo=t<t_end?t:t_end, t_hi=t>t_end?t:t_end;
    s_detail.spans++;
    s_detail.spanPixels+=(px_uint32)length;
    if ((px_uint32)length>s_detail.maxSpanLength) s_detail.maxSpanLength=(px_uint32)length;
    if (s_lo>=0 && t_lo>=0 && s_hi<((int64_t)width<<16) &&
        t_hi<((int64_t)height<<16) && s_hi<=INT32_MAX && t_hi<=INT32_MAX)
        s_detail.fullyInBoundsSpans++;
    else s_detail.partialOrOobSpans++;
}
#define DETAIL_SCANLINE() (s_detail.scanlines++)
#define DETAIL_SPAN(n,s,t,ds,dt,w,h) PX_LiveFrameworkDetailSpan((n),(s),(t),(ds),(dt),(w),(h))
#define DETAIL_SAMPLE_IN() (s_detail.sampleInBounds++)
#define DETAIL_SAMPLE_OUT() (s_detail.sampleOutOfBounds++)
#else
#define DETAIL_TRIANGLE() ((void)0)
#define DETAIL_FRAGMENT(x,y,a,z) ((void)0)
#define DETAIL_SCANLINE() ((void)0)
#define DETAIL_SPAN(n,s,t,ds,dt,w,h) ((void)0)
#define DETAIL_SAMPLE_IN() ((void)0)
#define DETAIL_SAMPLE_OUT() ((void)0)
#endif
px_void PX_LiveFrameworkGetGeometryBounds(const PX_LiveFramework *plive,
                                          PX_LiveGeometryBounds *out)
{
    if (out) *out=plive?plive->geometryBounds:(PX_LiveGeometryBounds){0};
}

#if L2D_CFG_PROFILE_ROI_DIAG || L2D_CFG_SRM_ROI
static px_void PX_LiveFrameworkBoundsVertex(PX_LiveGeometryBounds *b, px_float x, px_float y)
{
    if (!isfinite(x)||!isfinite(y)) { b->unsafe=PX_TRUE; return; }
    if (!b->valid) {
        b->min_x=b->max_x=x; b->min_y=b->max_y=y; b->valid=PX_TRUE;
    } else {
        if (x<b->min_x) b->min_x=x;
        if (x>b->max_x) b->max_x=x;
        if (y<b->min_y) b->min_y=y;
        if (y>b->max_y) b->max_y=y;
    }
}
#endif
px_void PX_LiveFrameworkDetailBeginFrame(px_uchar *overdraw,px_int width,px_int height)
{
#if L2D_CFG_PROFILE_DETAIL
    PX_memset(&s_detail,0,sizeof(s_detail));
    s_detailLayer=-1;
    s_overdraw=overdraw;
    s_overdrawWidth=width;
    s_overdrawHeight=height;
    if (overdraw&&width>0&&height>0) PX_memset(overdraw,0,(px_uint)(width*height));
#else
    (void)overdraw;(void)width;(void)height;
#endif
}
px_void PX_LiveFrameworkDetailGetFrame(PX_LiveFrameworkDetailFrame *out)
{
#if L2D_CFG_PROFILE_DETAIL
    if (out) *out=s_detail;
#else
    if (out) PX_memset(out,0,sizeof(*out));
#endif
}

/* ── UV 坐标工具函数 ────────────────────────────── */

/** 将 UV 坐标限制在 [0, 1) 范围内，防止纹理采样越界 */
static px_float PX_LiveFrameworkClampUV(px_float uv)
{
	const px_float max_uv = 1.0f - (1.0f / 65536.0f);
	if (uv < 0)
	{
		return 0;
	}
	if (uv > max_uv)
	{
		return max_uv;
	}
	return uv;
}

/* ── 旋转变换辅助函数 ───────────────────────────── */

px_void PX_LiveFrameworkGetTrigCacheFrame(const PX_LiveFramework *plive, px_dword *hit, px_dword *miss)
{
	if (!plive)
	{
		if (hit) *hit=0;
		if (miss) *miss=0;
		return;
	}
	if (hit) *hit=plive->trigCacheHit;
	if (miss) *miss=plive->trigCacheMiss;
}

/** 与 PX_MatrixRotateZ + PX_PointMulMatrix 相同的乘加顺序，sin/cos 由调用方提供。 */
static px_point PX_LiveFrameworkRotatePointFromSinCos(px_point point, px_float cos_angle, px_float sin_angle)
{
	px_matrix mat;
	mat._11=cos_angle; mat._12=sin_angle; mat._13=0.0f; mat._14=0.0f;
	mat._21=-sin_angle; mat._22=cos_angle; mat._23=0.0f; mat._24=0.0f;
	mat._31=0.0f; mat._32=0.0f; mat._33=1.0f; mat._34=0.0f;
	mat._41=0.0f; mat._42=0.0f; mat._43=0.0f; mat._44=1.0f;
	return PX_PointMulMatrix(point, mat);
}

static px_void PX_LiveFrameworkEnsureLocalRotationTrig(PX_LiveFramework *plive, PX_LiveLayer *pLayer)
{
	if (pLayer->rel_localRotationTrigValid &&
		pLayer->rel_cachedLocalRotationAngle==pLayer->rel_currentLocalRotationAngle)
	{
		plive->trigCacheHit++;
		return;
	}
	plive->trigCacheMiss++;
	pLayer->rel_cachedLocalRotationAngle=pLayer->rel_currentLocalRotationAngle;
#if L2D_CFG_PROFILE_VISUAL
	PX_VisualDiagPush(PX_VISUAL_SCOPE_SET);
#endif
	pLayer->rel_currentLocalRotationSin=PX_sin_angle(pLayer->rel_currentLocalRotationAngle);
	pLayer->rel_currentLocalRotationCos=PX_cos_angle(pLayer->rel_currentLocalRotationAngle);
#if L2D_CFG_PROFILE_VISUAL
	PX_VisualDiagPop();
#endif
	pLayer->rel_localRotationTrigValid=PX_TRUE;
}

/** 设置图层的当前旋转角度；角度完全相等时复用已有 sin/cos。 */
static px_void PX_LiveFrameworkSetLayerCurrentRotationAngle(PX_LiveFramework *plive, PX_LiveLayer *pLayer, px_float angle)
{
	if (pLayer->rel_rotationTrigValid && pLayer->rel_cachedRotationAngle==angle)
	{
		pLayer->rel_currentRotationAngle = angle;
		plive->trigCacheHit++;
		return;
	}
	plive->trigCacheMiss++;
	pLayer->rel_currentRotationAngle = angle;
	pLayer->rel_cachedRotationAngle = angle;
#if L2D_CFG_PROFILE_VISUAL
	PX_VisualDiagPush(PX_VISUAL_SCOPE_SET);
#endif
	pLayer->rel_currentRotationSin = PX_sin_angle(angle);
	pLayer->rel_currentRotationCos = PX_cos_angle(angle);
#if L2D_CFG_PROFILE_VISUAL
	PX_VisualDiagPop();
#endif
	pLayer->rel_rotationTrigValid=PX_TRUE;
}

/** 使用预计算的三角函数值进行二维旋转（优化：避免重复调用 sin/cos） */
static px_point PX_LiveFrameworkRotatePointCached(px_point point, px_float cos_angle, px_float sin_angle)
{
	px_point rotated;
	rotated.x = point.x * cos_angle - point.y * sin_angle;
	rotated.y = point.x * sin_angle + point.y * cos_angle;
	rotated.z = point.z;
	return rotated;
}

/* ── 内部颜色类型 ──────────────────────────────── */

/** RGBA 颜色联合体：支持按字节或 dword 访问 */
typedef struct
{
	union
	{
		struct
		{
			px_uchar r;
			px_uchar g;
			px_uchar b;
			px_uchar a;
		};
		px_dword ucolor;
	}_argb;
}px_liveframework_rgba_color;

/* ── 渲染用顶点结构体 ───────────────────────────── */

/** 栅格化渲染时使用的顶点结构，包含位置、法线和 UV */
typedef struct
{
	px_point position;   /**< 顶点位置 */
	px_point normal;     /**< 顶点法线 */
	px_float u,v;        /**< 纹理 UV 坐标 */
}PX_LiveRenderVertex;

/* ── 像素着色器（快速最近邻采样） ─────────────────── */

/** 快速像素着色器：使用最近邻采样，不进行混合 */
static px_void PX_LiveFramework_RenderListPixelShaderFaster(px_surface *psurface,px_int x,px_int y,px_float z,px_float u,px_float v,px_point normal,px_texture *pTexture,PX_TEXTURERENDER_BLEND *blend)
{
	/* 纹理映射：直接采样对应像素 */

	px_int resWidth;
	px_int resHeight;

	if (u<0||u>1||v<0||v>1)
	{
		return;
	}

	if (pTexture)
	{
		resWidth=pTexture->width;
		resHeight=pTexture->height;

		PX_SurfaceDrawPixel(psurface,x,y,PX_SURFACECOLOR(pTexture,(px_int)(u*resWidth),(px_int)(v*resHeight)));
	}
}

/* ── 像素着色器（双线性插值） ─────────────────────── */

/** 像素着色器：4 点双线性插值采样，支持 HDR 混合 */
static px_void PX_LiveFramework_RenderListPixelShader(px_surface *psurface,px_int x,px_int y,px_float z,px_float u,px_float v,px_point normal,px_texture *pTexture,PX_TEXTURERENDER_BLEND *blend)
{
	/* 双线性插值纹理映射 */
	px_double SampleX,SampleY,mapX,mapY;
	px_double mixa,mixr,mixg,mixb,Weight;
	px_color sampleColor;
	px_int resWidth;
	px_int resHeight;
#if L2D_CFG_PROFILE_DETAIL
	px_bool allFourAlphaZero=PX_TRUE;
#endif

	if (u<0||u>1||v<0||v>1)
	{
		return;
	}


	if (pTexture)
	{
		resWidth=pTexture->width;
		resHeight=pTexture->height;
		u=PX_ABS(u);
		v=PX_ABS(v);
		u-=(px_int)u;
		v-=(px_int)v;

		mapX=u*resWidth;
		mapY=v*resHeight;

		if (mapX<-0.5||mapX>resWidth+0.5)
		{
			return;
		}
		if (mapY<-0.5||mapY>resHeight+0.5)
		{
			return;
		}
		mixa=0;
		mixr=0;
		mixg=0;
		mixb=0;
		/* 采样 4 个邻近像素进行双线性插值 */
		/* 左上 */

		SampleX=(mapX-0.5f);
		SampleY=(mapY-0.5f);

		if (SampleX>0&&(SampleX)<resWidth&&SampleY>0&&(SampleY)<resHeight)
		{
			sampleColor=PX_SURFACECOLOR(pTexture,PX_TRUNC(SampleX),PX_TRUNC(SampleY));
#if L2D_CFG_PROFILE_DETAIL
			if (sampleColor._argb.a) allFourAlphaZero=PX_FALSE;
#endif
			Weight=(1-PX_FRAC(SampleX))*(1-PX_FRAC(SampleY));
			mixa+=sampleColor._argb.a*Weight;
			mixr+=sampleColor._argb.r*Weight;
			mixg+=sampleColor._argb.g*Weight;
			mixb+=sampleColor._argb.b*Weight;
		}

		/* 右上 */
		SampleX=(mapX+0.5f);
		SampleY=(mapY-0.5f);
		if (SampleX>0&&(SampleX)<resWidth&&SampleY>0&&(SampleY)<resHeight)
		{
			sampleColor=PX_SURFACECOLOR(pTexture,PX_TRUNC(SampleX),PX_TRUNC(SampleY));
#if L2D_CFG_PROFILE_DETAIL
			if (sampleColor._argb.a) allFourAlphaZero=PX_FALSE;
#endif
			Weight=PX_FRAC(mapX+0.5f)*(1-PX_FRAC(mapY-0.5f));
			mixa+=sampleColor._argb.a*Weight;
			mixr+=sampleColor._argb.r*Weight;
			mixg+=sampleColor._argb.g*Weight;
			mixb+=sampleColor._argb.b*Weight;
		}

		/* 左下 */
		SampleX=(mapX-0.5f);
		SampleY=(mapY+0.5f);
		if (SampleX>0&&(SampleX)<resWidth&&SampleY>0&&(SampleY)<resHeight)
		{
			sampleColor=PX_SURFACECOLOR(pTexture,PX_TRUNC(SampleX),PX_TRUNC(SampleY));
#if L2D_CFG_PROFILE_DETAIL
			if (sampleColor._argb.a) allFourAlphaZero=PX_FALSE;
#endif
			Weight=(1-PX_FRAC(SampleX))*(PX_FRAC(SampleY));
			mixa+=sampleColor._argb.a*Weight;
			mixr+=sampleColor._argb.r*Weight;
			mixg+=sampleColor._argb.g*Weight;
			mixb+=sampleColor._argb.b*Weight;
		}

		/* 右下 */
		SampleX=(mapX+0.5f);
		SampleY=(mapY+0.5f);
		if (SampleX>0&&(SampleX)<resWidth&&SampleY>0&&(SampleY)<resHeight)
		{
			sampleColor=PX_SURFACECOLOR(pTexture,PX_TRUNC(SampleX),PX_TRUNC(SampleY));
#if L2D_CFG_PROFILE_DETAIL
			if (sampleColor._argb.a) allFourAlphaZero=PX_FALSE;
#endif
			Weight=(PX_FRAC(SampleX))*(PX_FRAC(SampleY));
			mixa+=sampleColor._argb.a*Weight;
			mixr+=sampleColor._argb.r*Weight;
			mixg+=sampleColor._argb.g*Weight;
			mixb+=sampleColor._argb.b*Weight;
		}

		if (blend)
		{
			mixa*=blend->alpha;
			mixr*=blend->hdr_R;
			mixg*=blend->hdr_G;
			mixb*=blend->hdr_B;
		}

		mixa>255?mixa=255:0;
		mixr>255?mixr=255:0;
		mixg>255?mixg=255:0;
		mixb>255?mixb=255:0;

#if L2D_CFG_PROFILE_DETAIL
		DETAIL_FRAGMENT(x,y,(px_int)mixa,allFourAlphaZero);
#endif
		PX_SurfaceDrawPixel(psurface,x,y,PX_COLOR((px_uchar)mixa,(px_uchar)mixr,(px_uchar)mixg,(px_uchar)mixb));

	}
}

/* ── 扫描线渲染函数 ─────────────────────────────── */

/**
 * 扫描线像素着色跨度函数
 *
 * 在三角形的一条水平扫描线段上，线性插值 UV 坐标并逐个像素渲染。
 * 支持两种渲染路径：
 * 1. 快速近邻采样路径（定点数加速，带 Alpha 混合）
 * 2. 通用像素着色器回调路径
 */
static px_void PX_LiveFramework_RenderAffinePixelShaderSpan(px_surface *psurface,
	PX_LiveFramework *pLiveFramework,
	px_float y,           /**< 当前扫描线 Y 坐标 */
	px_float xleft,        /**< 扫描线左边界 */
	px_float xright,       /**< 扫描线右边界 */
	px_float y0,           /**< 顶点 0 的 Y 坐标 */
	px_float y1,           /**< 顶点 1 的 Y 坐标 */
	px_float y2,           /**< 顶点 2 的 Y 坐标 */
	px_float s0,           /**< 顶点 0 的 U 坐标 */
	px_float t0,           /**< 顶点 0 的 V 坐标 */
	px_float s1,           /**< 顶点 1 的 U 坐标 */
	px_float t1,           /**< 顶点 1 的 V 坐标 */
	px_float s2,           /**< 顶点 2 的 U 坐标 */
	px_float t2,           /**< 顶点 2 的 V 坐标 */
	px_float affine_x0, px_float affine_y0,
	px_float affine_s0, px_float affine_t0,
	px_float affine_s_dx, px_float affine_s_dy,
	px_float affine_t_dx, px_float affine_t_dy,
	px_point position,     /**< 渲染位置 */
	px_point normal,       /**< 法线 */
	px_texture *ptexture,  /**< 纹理 */
	PX_TEXTURERENDER_BLEND *blend) /**< 混合参数 */
{
	px_float sleft, sright, tleft, tright;
	px_float s_step, t_step;
	px_float s, t;
	px_float xstartf;
	px_color color;
	px_color *dst;
	px_int ix, iy;
	px_int xstart, xend;
	px_int tx, ty;
	px_int s_fp, t_fp;
	px_int s_fp_step, t_fp_step;
	px_int texture_width, texture_height;

	if (!pLiveFramework->pixelShader)
	{
		return;
	}
	DETAIL_SCANLINE();

	/* 退化情况检查：退化三角形或零宽度扫描线 */
	if (xright == xleft ||
		(!pLiveFramework->fastNearestSampling && (y1 == y0 || y2 == y0)))
	{
		return;
	}

	xstart = (px_int)(xleft + 0.5f);
	xend = (px_int)(xright + 0.5f);
	if (xend <= xstart)
	{
		return;
	}

	xstartf = (px_float)xstart;
	if (pLiveFramework->fastNearestSampling)
	{
		/* Affine UV gradients are constant over the whole triangle.  They are
		 * evaluated once by the triangle setup instead of repeating six float
		 * divisions for every scanline. */
		s_step=affine_s_dx;
		t_step=affine_t_dx;
		s=affine_s0+(xstartf-affine_x0)*affine_s_dx+(y-affine_y0)*affine_s_dy;
		t=affine_t0+(xstartf-affine_x0)*affine_t_dx+(y-affine_y0)*affine_t_dy;
	}
	else
	{
		/* Generic shader path retains the original edge interpolation. */
		sleft = (y - y0) * (s1 - s0) / (y1 - y0) + s0;
		sright = (y - y0) * (s2 - s0) / (y2 - y0) + s0;
		tleft = (y - y0) * (t1 - t0) / (y1 - y0) + t0;
		tright = (y - y0) * (t2 - t0) / (y2 - y0) + t0;
		s_step = (sright - sleft) / (xright - xleft);
		t_step = (tright - tleft) / (xright - xleft);
		s = sleft + (xstartf - xleft) * s_step;
		t = tleft + (xstartf - xleft) * t_step;
	}
	iy = (px_int)y;
	position.z = 1;

	/* 快速路径：定点数最近邻采样，直接操作表面缓冲区 */
	if (pLiveFramework->fastNearestSampling && ptexture && ptexture->surfaceBuffer)
	{
		/* 裁剪到表面边界 */
		if (iy < psurface->limit_top || iy > psurface->limit_bottom)
		{
			return;
		}

		if (xstart < psurface->limit_left)
		{
			xstart = psurface->limit_left;
		}
		if (xend > psurface->limit_right + 1)
		{
			xend = psurface->limit_right + 1;
		}
		if (xend <= xstart)
		{
			return;
		}

		xstartf = (px_float)xstart;
		s=affine_s0+(xstartf-affine_x0)*affine_s_dx+(y-affine_y0)*affine_s_dy;
		t=affine_t0+(xstartf-affine_x0)*affine_t_dx+(y-affine_y0)*affine_t_dy;
		dst = psurface->surfaceBuffer + xstart + psurface->width * iy;
		texture_width = ptexture->width;
		texture_height = ptexture->height;
		/* 转换为 16.16 定点数 */
		s_fp = (px_int)(s * texture_width * 65536.0f);
		t_fp = (px_int)(t * texture_height * 65536.0f);
		s_fp_step = (px_int)(s_step * texture_width * 65536.0f);
		t_fp_step = (px_int)(t_step * texture_height * 65536.0f);
		DETAIL_SPAN(xend-xstart,s_fp,t_fp,s_fp_step,t_fp_step,texture_width,texture_height);

		/* 无混合：直接覆盖或 Alpha 预乘 */
		if (!blend)
		{
			for (ix = xstart; ix < xend; ++ix, ++dst)
			{
				tx = s_fp >> 16;
				ty = t_fp >> 16;
				if ((px_uint)tx >= (px_uint)texture_width ||
					(px_uint)ty >= (px_uint)texture_height)
				{
					DETAIL_SAMPLE_OUT();
					s_fp += s_fp_step;
					t_fp += t_fp_step;
					continue;
				}
				DETAIL_SAMPLE_IN();
				color = PX_SURFACECOLOR(ptexture, tx, ty);
				DETAIL_FRAGMENT(ix,iy,color._argb.a,PX_FALSE);

				if (color._argb.a == 0xff)
				{
					*dst = color;
				}
				else if (color._argb.a)
				{
					dst->_argb.r = (px_uchar)(((256 - color._argb.a) * dst->_argb.r +
						color._argb.r * (color._argb.a + 1)) >> 8);
					dst->_argb.g = (px_uchar)(((256 - color._argb.a) * dst->_argb.g +
						color._argb.g * (color._argb.a + 1)) >> 8);
					dst->_argb.b = (px_uchar)(((256 - color._argb.a) * dst->_argb.b +
						color._argb.b * (color._argb.a + 1)) >> 8);
					dst->_argb.a = 255 - (((256 - dst->_argb.a) * (255 - color._argb.a)) >> 8);
				}

				s_fp += s_fp_step;
				t_fp += t_fp_step;
			}
			return;
		}

		/* 带 HDR 混合的路径 */
		for (ix = xstart; ix < xend; ++ix, ++dst)
		{
			tx = s_fp >> 16;
			ty = t_fp >> 16;
			if ((px_uint)tx >= (px_uint)texture_width ||
				(px_uint)ty >= (px_uint)texture_height)
			{
				DETAIL_SAMPLE_OUT();
				s_fp += s_fp_step;
				t_fp += t_fp_step;
				continue;
			}
			DETAIL_SAMPLE_IN();
			color = PX_SURFACECOLOR(ptexture, tx, ty);

			{
				px_int a = (px_int)(color._argb.a * blend->alpha);
				px_int r = (px_int)(color._argb.r * blend->hdr_R);
				px_int g = (px_int)(color._argb.g * blend->hdr_G);
				px_int b = (px_int)(color._argb.b * blend->hdr_B);

				color._argb.a = (px_uchar)((a > 255) ? 255 : ((a < 0) ? 0 : a));
				color._argb.r = (px_uchar)((r > 255) ? 255 : ((r < 0) ? 0 : r));
				color._argb.g = (px_uchar)((g > 255) ? 255 : ((g < 0) ? 0 : g));
				color._argb.b = (px_uchar)((b > 255) ? 255 : ((b < 0) ? 0 : b));
			}

			DETAIL_FRAGMENT(ix,iy,color._argb.a,PX_FALSE);
			if (color._argb.a == 0xff)
			{
				*dst = color;
			}
			else if (color._argb.a)
			{
				dst->_argb.r = (px_uchar)(((256 - color._argb.a) * dst->_argb.r +
					color._argb.r * (color._argb.a + 1)) >> 8);
				dst->_argb.g = (px_uchar)(((256 - color._argb.a) * dst->_argb.g +
					color._argb.g * (color._argb.a + 1)) >> 8);
				dst->_argb.b = (px_uchar)(((256 - color._argb.a) * dst->_argb.b +
					color._argb.b * (color._argb.a + 1)) >> 8);
				dst->_argb.a = 255 - (((256 - dst->_argb.a) * (255 - color._argb.a)) >> 8);
			}

			s_fp += s_fp_step;
			t_fp += t_fp_step;
		}
		return;
	}

	/* 通用路径：调用像素着色器回调 */
	for (ix = xstart; ix < xend; ++ix)
	{
		pLiveFramework->pixelShader(psurface, ix, iy, position, s, t, normal, ptexture, blend);
		s += s_step;
		t += t_step;
	}
}

/* ── 三角形栅格化 ──────────────────────────────── */

/**
 * 三角形栅格化主函数
 *
 * 使用扫描线算法将三角形渲染到表面。将三角形分为上半部分和下半部分，
 * 分别沿 Y 轴扫描，对每条扫描线计算左右边界后进行纹理映射绘制。
 * 支持透视校正插值（通过 1/z 和 s/z, t/z 的线性插值）。
 */
static px_void PX_LiveFramework_RenderListRasterization(px_surface *psurface,PX_LiveFramework *pLiveFramework,PX_LiveRenderVertex p0,PX_LiveRenderVertex p1,PX_LiveRenderVertex p2,px_texture *ptexture,PX_TEXTURERENDER_BLEND *blend)
{
	DETAIL_TRIANGLE();
	px_float affine_x0=p0.position.x,affine_y0=p0.position.y;
	px_float affine_s0=p0.u,affine_t0=p0.v;
	px_float affine_s_dx=0,affine_s_dy=0,affine_t_dx=0,affine_t_dy=0;
	if (pLiveFramework->fastNearestSampling)
	{
		px_float dx1=p1.position.x-affine_x0;
		px_float dy1=p1.position.y-affine_y0;
		px_float dx2=p2.position.x-affine_x0;
		px_float dy2=p2.position.y-affine_y0;
		px_float denominator=dx1*dy2-dx2*dy1;
		px_float invDenominator;
		if (PX_ABS(denominator)<0.000001f)
		{
			return;
		}
		invDenominator=1.0f/denominator;
		affine_s_dx=((p1.u-affine_s0)*dy2-(p2.u-affine_s0)*dy1)*invDenominator;
		affine_s_dy=(dx1*(p2.u-affine_s0)-dx2*(p1.u-affine_s0))*invDenominator;
		affine_t_dx=((p1.v-affine_t0)*dy2-(p2.v-affine_t0)*dy1)*invDenominator;
		affine_t_dy=(dx1*(p2.v-affine_t0)-dx2*(p1.v-affine_t0))*invDenominator;
	}
	/* 防御：RT30 局部变换在异常量化下可能把顶点推到极远位置，导致扫描线 y 循环
	 * （不裁剪到表面）空转卡死。跳过任意顶点坐标超出大阈值的退化三角形。
	 * 屏幕内三角形（坐标 0~1024）不受影响。 */
	if (p0.position.x>100000.f||p0.position.x<-100000.f||p0.position.y>100000.f||p0.position.y<-100000.f||
		p1.position.x>100000.f||p1.position.x<-100000.f||p1.position.y>100000.f||p1.position.y<-100000.f||
		p2.position.x>100000.f||p2.position.x<-100000.f||p2.position.y>100000.f||p2.position.y<-100000.f)
	{
		return;
	}
	px_int   ix,iy;
	px_bool  k01infinite=PX_FALSE;
	px_bool  k02infinite=PX_FALSE;
	px_float k01,b01,k02,b02;
	px_float x0;
	px_float y0;
	px_float z0;
	px_float s0;
	px_float t0;

	px_float x1;
	px_float y1;
	px_float z1;
	px_float s1;
	px_float t1;

	px_float x2;
	px_float y2;
	px_float z2;
	px_float s2;
	px_float t2;


	px_float y,xleft, xright; 
	px_float oneoverz_left, oneoverz_right; 
	px_float oneoverz_top, oneoverz_bottom; 
	px_float oneoverz, oneoverz_step;   
	px_float soverz_top, soverz_bottom; 
	px_float toverz_top, toverz_bottom; 
	px_float soverz_left, soverz_right; 
	px_float toverz_left, toverz_right;
	px_float soverz, soverz_step;
	px_float toverz, toverz_step; 
	px_float s, t;
	px_float btmy,midy;
	px_float originalZ;

	px_point position;
	if (!pLiveFramework->fastNearestSampling)
	{
		px_float a,b,c;
		a=(px_float)PX_sqrtd((p1.position.x-p2.position.x)*(p1.position.x-p2.position.x));
		b=(px_float)PX_sqrtd((p0.position.x-p2.position.x)*(p0.position.x-p2.position.x));
		c=(px_float)PX_sqrtd((p1.position.x-p0.position.x)*(p1.position.x-p0.position.x));

		position.x=(a*p0.position.x+b*p1.position.x+c*p2.position.x)/(a+b+c);
		position.y=(a*p0.position.y+b*p1.position.y+c*p2.position.y)/(a+b+c);
	}
	else
	{
		position.x = 0;
		position.y = 0;
	}
	/* 按 Y 坐标排序三个顶点，p0 为最上方（最小 Y） */
	/*    p0
	 * p1   p2
	 */

	if (p1.position.y<p0.position.y)
	{
		PX_LiveRenderVertex t;
		t=p1;
		p1=p0;
		p0=t;
	}

	if (p2.position.y<p0.position.y)
	{
		PX_LiveRenderVertex t;
		t=p2;
		p2=p0;
		p0=t;
	}

	btmy=p1.position.y;
	midy=p2.position.y;
	if (p2.position.y>btmy)
	{
		midy=p1.position.y;
		btmy=p2.position.y;
	}



	if (!pLiveFramework->fastNearestSampling)
	{
		px_float a,b,c;
		a=(px_float)PX_sqrtd((p1.position.x-p2.position.x)*(p1.position.x-p2.position.x));
		b=(px_float)PX_sqrtd((p0.position.x-p2.position.x)*(p0.position.x-p2.position.x));
		c=(px_float)PX_sqrtd((p1.position.x-p0.position.x)*(p1.position.x-p0.position.x));

		position.x=(a*p0.position.x+b*p1.position.x+c*p2.position.x)/(a+b+c);
		position.y=(a*p0.position.y+b*p1.position.y+c*p2.position.y)/(a+b+c);
	}
	else
	{
		position.x = 0;
		position.y = 0;
	}
	{
		px_float x01m;

		x0=p0.position.x;
		y0=p0.position.y;
		x1=p1.position.x;
		y1=p1.position.y;
		x2=p2.position.x;
		y2=p2.position.y;


		if (x0==x1)
		{
			x01m=x0;
		}
		else
		{
			k01=(y0-y1)/(x0-x1);
			b01=y0-k01*x0;
			x01m=(y2-b01)/k01;
		}

		if (x01m>x2)
		{
			PX_LiveRenderVertex t;
			t=p2;
			p2=p1;
			p1=t;
		}
	} while (0);



	x0=p0.position.x;
	y0=p0.position.y;
	z0=p0.position.z;
	s0=p0.u;
	t0=p0.v;

	x1=p1.position.x;
	y1=p1.position.y;
	z1=p1.position.z;
	s1=p1.u;
	t1=p1.v;

	x2=p2.position.x;
	y2=p2.position.y;
	z2=p2.position.z;
	s2=p2.u;
	t2=p2.v;

	k01infinite=PX_FALSE;
	k02infinite=PX_FALSE;
	if (x0==x1)
	{
		k01infinite=PX_TRUE;
		k01 = 1;
		b01=x0;
	}
	else
	{
		k01=(y0-y1)/(x0-x1);
		b01=y0-k01*x0;
	}

	if (x0==x2)
	{
		k02infinite=PX_TRUE;
		k02 = 1;
		b02=x0;
	}
	else
	{
		k02=(y0-y2)/(x0-x2);
		b02=y0-k02*x0;
	}


	/* 扫描线上半部分：从顶部顶点 y0 向下到中间顶点 midy */
	for(y = (px_int)(y0+0.5f)+0.5f; y <=midy; y++)
	{
		if (k01infinite)
		{
			xleft=b01;
		}
		else
		{
			xleft = (y-b01)/k01;
		}

		if (k02infinite)
		{
			xright=b02;
		}
		else
		{
			xright = (y-b02)/k02;
		}

		if (pLiveFramework->pixelShader)
		{
			PX_LiveFramework_RenderAffinePixelShaderSpan(psurface, pLiveFramework, y, xleft, xright,
				y0, y1, y2, s0, t0, s1, t1, s2, t2,
				affine_x0,affine_y0,affine_s0,affine_t0,
				affine_s_dx,affine_s_dy,affine_t_dx,affine_t_dy,
				position, p0.normal, ptexture, blend);
			continue;
		}

		/* 透视校正插值：对 1/z 和 (s/z, t/z) 做线性插值 */
		oneoverz_top = 1.0f / z0;
		oneoverz_bottom = 1.0f/z1;
		oneoverz_left = (y-y0) * (oneoverz_bottom-oneoverz_top) / (y1-y0) + oneoverz_top;
		oneoverz_bottom = 1.0f / z2;
		oneoverz_right = (y-y0) * (oneoverz_bottom-oneoverz_top) / (y2-y0) + oneoverz_top;
		oneoverz_step = (oneoverz_right-oneoverz_left) / (xright-xleft);
		soverz_top = s0 / z0;
		soverz_bottom = s1 / z1;
		soverz_left = (y-y0) * (soverz_bottom-soverz_top) / (y1-y0) + soverz_top;
		soverz_bottom = s2 / z2;
		soverz_right = (y-y0) * (soverz_bottom-soverz_top) / (y2-y0) + soverz_top;
		soverz_step = (soverz_right-soverz_left) / (xright-xleft);
		toverz_top = t0 / z0;
		toverz_bottom = t1 / z1;
		toverz_left = (y-y0) * (toverz_bottom-toverz_top) / (y1-y0) + toverz_top;
		toverz_bottom = t2 / z2;
		toverz_right = (y-y0) * (toverz_bottom-toverz_top) / (y2-y0) + toverz_top;
		toverz_step = (toverz_right-toverz_left) / (xright-xleft);
		oneoverz = oneoverz_left,soverz = soverz_left, toverz = toverz_left;

		/* 透视校正后逐像素渲染 */
	for(ix = (px_int)(xleft+0.5);ix < (px_int)(xright+0.5f); ++ix)
		{
			s = soverz / oneoverz;
			t = toverz / oneoverz;
			originalZ=1.0f/oneoverz;

			iy=(px_int)y;
			if (pLiveFramework->pixelShader)
			{
				position.z=originalZ;
				pLiveFramework->pixelShader(psurface,ix,iy,position,s,t,p0.normal,ptexture,blend);
			}
			else
			{
				PX_LiveFramework_RenderListPixelShader(psurface,ix,iy,originalZ,s,t,p0.normal,ptexture,blend);
			}


			oneoverz += oneoverz_step;
			soverz += soverz_step;
			toverz += toverz_step;
		}
	}

	/* 扫描线下半部分：从中间顶点向下到底部顶点 */
	// p1   p2
	//    p0
	if (p1.position.y>p0.position.y)
	{
		PX_LiveRenderVertex t;
		t=p1;
		p1=p0;
		p0=t;
	}

	if (p2.position.y>p0.position.y)
	{
		PX_LiveRenderVertex t;
		t=p2;
		p2=p0;
		p0=t;
	}

	btmy=p1.position.y;
	midy=p2.position.y;
	if (p2.position.y<btmy)
	{
		midy=p1.position.y;
		btmy=p2.position.y;
	}



	do 
	{
		px_float x01m;

		x0=p0.position.x;
		y0=p0.position.y;
		x1=p1.position.x;
		y1=p1.position.y;
		x2=p2.position.x;
		y2=p2.position.y;


		if (x0==x1)
		{
			x01m=x0;
		}
		else
		{
			k01=(y0-y1)/(x0-x1);
			b01=y0-k01*x0;
			x01m=(y2-b01)/k01;
		}

		if (x01m>x2)
		{
			PX_LiveRenderVertex t;
			t=p2;
			p2=p1;
			p1=t;
		}
	} while (0);



	x0=p0.position.x;
	y0=p0.position.y;
	z0=p0.position.z;
	s0=p0.u;
	t0=p0.v;

	x1=p1.position.x;
	y1=p1.position.y;
	z1=p1.position.z;
	s1=p1.u;
	t1=p1.v;

	x2=p2.position.x;
	y2=p2.position.y;
	z2=p2.position.z;
	s2=p2.u;
	t2=p2.v;

	k01infinite=PX_FALSE;
	k02infinite=PX_FALSE;
	if (x0==x1)
	{
		k01infinite=PX_TRUE;
		b01=x0;
	}
	else
	{
		k01=(y0-y1)/(x0-x1);
		b01=y0-k01*x0;
	}

	if (x0==x2)
	{
		k02infinite=PX_TRUE;
		b02=x0;
	}
	else
	{
		k02=(y0-y2)/(x0-x2);
		b02=y0-k02*x0;
	}


	/* 扫描线下半部分：从 midy 继续向下到底部顶点 y0 */
	for(y = (px_int)(midy+0.5f)+0.5f; y < y0; y++)
	{
		if (k01infinite)
		{
			xleft=b01;
		}
		else
		{
			xleft = (y-b01)/k01;
		}

		if (k02infinite)
		{
			xright=b02;
		}
		else
		{
			xright = (y-b02)/k02;
		}

		if (pLiveFramework->pixelShader)
		{
			PX_LiveFramework_RenderAffinePixelShaderSpan(psurface, pLiveFramework, y, xleft, xright,
				y0, y1, y2, s0, t0, s1, t1, s2, t2,
				affine_x0,affine_y0,affine_s0,affine_t0,
				affine_s_dx,affine_s_dy,affine_t_dx,affine_t_dy,
				position, p0.normal, ptexture, blend);
			continue;
		}


		oneoverz_top = 1.0f / z0;
		oneoverz_bottom = 1.0f/z1;
		oneoverz_left = (y-y0) * (oneoverz_bottom-oneoverz_top) / (y1-y0) + oneoverz_top;
		oneoverz_bottom = 1.0f / z2;
		oneoverz_right = (y-y0) * (oneoverz_bottom-oneoverz_top) / (y2-y0) + oneoverz_top;
		oneoverz_step = (oneoverz_right-oneoverz_left) / (xright-xleft);
		soverz_top = s0 / z0;
		soverz_bottom = s1 / z1;
		soverz_left = (y-y0) * (soverz_bottom-soverz_top) / (y1-y0) + soverz_top;
		soverz_bottom = s2 / z2;
		soverz_right = (y-y0) * (soverz_bottom-soverz_top) / (y2-y0) + soverz_top;
		soverz_step = (soverz_right-soverz_left) / (xright-xleft);
		toverz_top = t0 / z0;
		toverz_bottom = t1 / z1;
		toverz_left = (y-y0) * (toverz_bottom-toverz_top) / (y1-y0) + toverz_top;
		toverz_bottom = t2 / z2;
		toverz_right = (y-y0) * (toverz_bottom-toverz_top) / (y2-y0) + toverz_top;
		toverz_step = (toverz_right-toverz_left) / (xright-xleft);
		oneoverz = oneoverz_left,soverz = soverz_left, toverz = toverz_left;

		for(ix = (px_int)(xleft+0.5);ix < (px_int)(xright+0.5f); ++ix)
		{
			s = soverz / oneoverz;
			t = toverz / oneoverz;
			originalZ=1.0f/oneoverz;
			iy=(px_int)y;
			if (pLiveFramework->pixelShader)
			{
				position.z=originalZ;
				pLiveFramework->pixelShader(psurface,ix,iy,position,s,t,p0.normal,ptexture,blend);
			}
			else
			{
				PX_LiveFramework_RenderListPixelShader(psurface,ix,iy,originalZ,s,t,p0.normal,ptexture,blend);
			}
			oneoverz += oneoverz_step;
			soverz += soverz_step;
			toverz += toverz_step;
		}
	}

}

/* ── 框架初始化 ──────────────────────────────── */

/** 初始化 Live2D 框架：分配内存并设置默认状态 */
px_bool PX_LiveFrameworkInitialize(px_memorypool *mp,PX_LiveFramework *plive,px_int width,px_int height)
{
	PX_memset(plive,0,sizeof(PX_LiveFramework));
	plive->mp=mp;
	plive->animationMode=PX_LIVE_MODE_NEUTRAL;
	PX_LiveRealtimeInitialize(&plive->realtime,mp);
	if(!PX_VectorInitialize(mp,&plive->layers,sizeof(PX_LiveLayer),1))return PX_FALSE;
	if(!PX_VectorInitialize(mp,&plive->livetextures,sizeof(PX_LiveTexture),1))return PX_FALSE;
	if(!PX_VectorInitialize(mp,&plive->liveAnimations,sizeof(PX_LiveAnimation),1))return PX_FALSE;
	plive->width=width;
	plive->height=height;
	plive->showFocusLayer=PX_TRUE;
	plive->currentEditAnimationIndex=-1;
	plive->currentEditFrameIndex=-1;
	plive->currentEditVertexIndex=-1;
	plive->currentEditLayerIndex=-1;
	return PX_TRUE;
}

/** 开始播放当前动画 */
px_void PX_LiveFrameworkPlay(PX_LiveFramework *plive)
{
	if (plive->animationMode==PX_LIVE_MODE_REALTIME30)
	{
		PX_LiveRealtimeLeave(plive);
	}
	plive->animationMode=PX_LIVE_MODE_TIMELINE;
	plive->status=PX_LIVEFRAMEWORK_STATUS_PLAYING;
}

/** 暂停播放当前动画 */
px_void PX_LiveFrameworkPause(PX_LiveFramework *plive)
{
	plive->status=PX_LIVEFRAMEWORK_STATUS_STOP;
}

/** 重置框架状态：清零所有寄存器并恢复各图层到初始位置 */
px_void PX_LiveFrameworkReset(PX_LiveFramework *plive)
{
	px_int i;
#if L2D_CFG_PROFILE_VISUAL
	PX_LiveFrameworkVisualDiagInvalidate();
#endif
	plive->reg_duration=0;
	plive->reg_ip=0;
	plive->reg_elapsed=0;
	plive->reg_bp=-1;
	plive->status=PX_LIVEFRAMEWORK_STATUS_STOP;
	plive->animationMode=PX_LIVE_MODE_NEUTRAL;
	PX_LiveRealtimeResetAll(plive);

	for (i=0;i<plive->layers.size;i++)
	{
		px_int j;
		PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
		pLayer->rel_beginRotationAngle=0;
		pLayer->rel_rotationTrigValid=PX_FALSE;
		pLayer->rel_localRotationTrigValid=PX_FALSE;
		PX_LiveFrameworkSetLayerCurrentRotationAngle(plive, pLayer, 0);
		pLayer->rel_endRotationAngle=0;

		pLayer->rel_beginTranslation=PX_POINT(0,0,0);
		pLayer->rel_currentTranslation=PX_POINT(0,0,0);
		pLayer->rel_endTranslation=PX_POINT(0,0,0);

		pLayer->rel_beginLocalTranslation=PX_POINT(0,0,0);
		pLayer->rel_currentLocalTranslation=PX_POINT(0,0,0);
		pLayer->rel_endLocalTranslation=PX_POINT(0,0,0);
		pLayer->rel_beginLocalRotationAngle=0;
		pLayer->rel_currentLocalRotationAngle=0;
		pLayer->rel_endLocalRotationAngle=0;
		pLayer->rel_beginLocalScale=1;
		pLayer->rel_currentLocalScale=1;
		pLayer->rel_endLocalScale=1;
		pLayer->rel_beginStretch=1;
		pLayer->rel_currentStretch=1;
		pLayer->rel_endStretch=1;

		pLayer->RenderTextureIndex=pLayer->LinkTextureIndex;

		for (j=0;j<pLayer->vertices.size;j++)
		{
			PX_LiveVertex *pVertex=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,j);
			pVertex->beginTranslation=PX_POINT(0,0,0);
			pVertex->currentTranslation=PX_POINT(0,0,0);
			pVertex->endTranslation=PX_POINT(0,0,0);

			pVertex->currentPosition=pVertex->sourcePosition;
			pVertex->velocity=PX_POINT(0,0,0);
		}
	}
}

/** 停止播放（等价于重置） */
px_void PX_LiveFrameworkStop(PX_LiveFramework *plive)
{
	PX_LiveFrameworkReset(plive);
}

/* ── 图层插值更新 ──────────────────────────────── */

/** 根据进度调度值更新图层的插值参数（旋转、拉伸、平移、扇形变形） */
static px_void PX_LiveFrameworkUpdateLayerInterpolation(PX_LiveFramework *plive,PX_LiveLayer *pLayer)
{
	px_float schedule;
	px_int i;

	if (plive->status==PX_LIVEFRAMEWORK_STATUS_STOP)
	{
		schedule = 1;
	}
	else if (plive->reg_duration==0)
	{
		schedule=1;
	}
	else
	{
		schedule=plive->reg_elapsed*1.0f/plive->reg_duration;
	}

	if (schedule>1)
	{
		schedule=1;
	}

	//update parameters
	pLayer->panc_currentx= pLayer->panc_beginx + (pLayer->panc_endx - pLayer->panc_beginx) * schedule;
	pLayer->panc_currenty = pLayer->panc_beginy + (pLayer->panc_endy - pLayer->panc_beginy) * schedule;

	//point translation
	for (i=0;i<pLayer->vertices.size;i++)
	{
		px_point *pbegin,*pcurrent,*pend;
		pbegin=&PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,i)->beginTranslation;
		pcurrent=&PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,i)->currentTranslation;
		pend=&PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,i)->endTranslation;

		pcurrent->x=pbegin->x+(pend->x-pbegin->x)*schedule;
		pcurrent->y=pbegin->y+(pend->y-pbegin->y)*schedule;
	}

	if (pLayer->parent_index!=-1)
	{
		//stretch
		pLayer->rel_currentStretch=pLayer->rel_beginStretch+(pLayer->rel_endStretch-pLayer->rel_beginStretch)*schedule;
	}
	else
	{
		//keypoint translation
		pLayer->rel_currentTranslation.x=pLayer->rel_beginTranslation.x+(pLayer->rel_endTranslation.x-pLayer->rel_beginTranslation.x)*schedule;
		pLayer->rel_currentTranslation.y=pLayer->rel_beginTranslation.y+(pLayer->rel_endTranslation.y-pLayer->rel_beginTranslation.y)*schedule;
		pLayer->rel_currentTranslation.z=0;
	}
	
	//rotation
	PX_LiveFrameworkSetLayerCurrentRotationAngle(plive, pLayer,
		pLayer->rel_beginRotationAngle+(pLayer->rel_endRotationAngle-pLayer->rel_beginRotationAngle)*schedule);

		//selected-layer-only visual transform
		pLayer->rel_currentLocalTranslation.x=pLayer->rel_beginLocalTranslation.x+(pLayer->rel_endLocalTranslation.x-pLayer->rel_beginLocalTranslation.x)*schedule;
		pLayer->rel_currentLocalTranslation.y=pLayer->rel_beginLocalTranslation.y+(pLayer->rel_endLocalTranslation.y-pLayer->rel_beginLocalTranslation.y)*schedule;
		pLayer->rel_currentLocalTranslation.z=0;
		pLayer->rel_currentLocalRotationAngle=pLayer->rel_beginLocalRotationAngle+(pLayer->rel_endLocalRotationAngle-pLayer->rel_beginLocalRotationAngle)*schedule;
		pLayer->rel_currentLocalScale=pLayer->rel_beginLocalScale+(pLayer->rel_endLocalScale-pLayer->rel_beginLocalScale)*schedule;

	

}

/* ── 图层关键点更新 ─────────────────────────────── */

/** 递归更新图层及其所有子图层的关键点位置（级联变换） */
static px_void PX_LiveFramework_UpdateLayerKeyPoint(PX_LiveFramework *pLive,PX_LiveLayer *pLayer)
{
	px_int i;
	if (pLayer->parent_index!=-1)
	{
		PX_LiveLayer *pParent = PX_LiveFrameworkGetLayerParent(pLive,pLayer);
		//stretch
		px_point v=PX_PointSub(pLayer->keyPoint,pParent->keyPoint);
		v=PX_PointMul(v,pLayer->rel_currentStretch);
			if (pLive->animationMode==PX_LIVE_MODE_REALTIME30)
			{
				v=PX_PointAdd(v,pLayer->rel_currentTranslation);
			}
		v=PX_LiveFrameworkRotatePointCached(v, pParent->rel_currentRotationCos, pParent->rel_currentRotationSin);
		pLayer->currentKeyPoint=PX_PointAdd(v,pParent->currentKeyPoint);
	}
	else
	{
		pLayer->currentKeyPoint=PX_PointAdd(pLayer->keyPoint,pLayer->rel_currentTranslation);
	}
	pLayer->currentKeyPoint.z=pLayer->keyPoint.z;

	for (i=0;i<PX_COUNTOF(pLayer->child_index);i++)
	{
		if (pLayer->child_index[i]!=-1)
		{
			PX_LiveFramework_UpdateLayerKeyPoint(pLive,PX_LiveFrameworkGetLayerChild(pLive,pLayer,pLayer->child_index[i]));
		}
		else
		{
			break;
		}
	}
}


/*
 * Compose the selected-layer visual transforms from this layer up through its
 * ancestors.  The result maps an already evaluated hierarchy/world point to
 * its visual subtree position.  It is calculated once per layer update, not
 * once per vertex per ancestor.
 */
#if L2D_CFG_PROFILE_VISUAL
static PX_LiveVisualDiagFrame s_visual_diag;
static px_byte s_visual_ancestor_seen[PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER];
static int s_visual_history_valid;
static int s_visual_history_layers;
typedef struct {
	px_float rotation;
	px_float local_rotation;
	px_float scale;
	px_float local_tx, local_ty, local_tz;
	px_float hierarchy_tx, hierarchy_ty;
	px_float key_x, key_y, key_z;
} PX_LiveVisualDiagSnap;
static PX_LiveVisualDiagSnap s_visual_snap[PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER];

static px_dword PX_LiveVisualDiagSum(const px_dword *values)
{
	px_dword sum=0;
	int i;
	for (i=0;i<PX_VISUAL_SCOPE_COUNT;i++) sum+=values[i];
	return sum;
}

static int PX_LiveVisualDiagUniqueFloats(const px_float *values, int count)
{
	int i,j,unique=0;
	for (i=0;i<count;i++)
	{
		for (j=0;j<i;j++)
		{
			if (values[j]==values[i]) break;
		}
		if (j==i) unique++;
	}
	return unique;
}

static px_dword PX_LiveVisualDiagFloatBits(px_float value)
{
	union { px_float f; px_dword u; } bits;
	bits.f=value;
	return bits.u;
}

static int PX_LiveVisualDiagAncestorChanged(PX_LiveFramework *plive, int index)
{
	int guard=0;
	int parent;
	if (index<0||index>=plive->layers.size) return 0;
	parent=PX_VECTORAT(PX_LiveLayer,&plive->layers,index)->parent_index;
	while (parent>=0&&parent<plive->layers.size&&guard++<PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER)
	{
		const PX_LiveLayer *ancestor=PX_VECTORAT(PX_LiveLayer,&plive->layers,parent);
		const PX_LiveVisualDiagSnap *snap=&s_visual_snap[parent];
		if (ancestor->rel_currentRotationAngle!=snap->rotation||
			ancestor->rel_currentLocalRotationAngle!=snap->local_rotation||
			ancestor->rel_currentLocalScale!=snap->scale||
			ancestor->rel_currentLocalTranslation.x!=snap->local_tx||
			ancestor->rel_currentLocalTranslation.y!=snap->local_ty||
			ancestor->rel_currentLocalTranslation.z!=snap->local_tz||
			ancestor->currentKeyPoint.x!=snap->key_x||
			ancestor->currentKeyPoint.y!=snap->key_y||
			ancestor->currentKeyPoint.z!=snap->key_z)
		{
			return 1;
		}
		parent=ancestor->parent_index;
	}
	return 0;
}

static void PX_LiveVisualDiagBeginFrame(void)
{
	memset(&s_visual_diag,0,sizeof(s_visual_diag));
	memset(s_visual_ancestor_seen,0,sizeof(s_visual_ancestor_seen));
	PX_VisualDiagBegin();
}

static void PX_LiveVisualDiagFinishFrame(PX_LiveFramework *plive, int physical_ran)
{
	PX_VisualDiagTrig trig;
	static px_float rotations[PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER];
	static px_float local_rotations[PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER];
	int i,n,unique_ancestors=0;
	PX_VisualDiagEnd();
	PX_VisualDiagRead(&trig);
	n=plive->layers.size;
	if (n>PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER) n=PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER;
	s_visual_diag.physical_ran=physical_ran?1u:0u;
	s_visual_diag.layers=(px_dword)n;
	s_visual_diag.sin_angle_calls=PX_LiveVisualDiagSum(trig.sin_angle);
	s_visual_diag.cos_angle_calls=PX_LiveVisualDiagSum(trig.cos_angle);
	s_visual_diag.sind_calls=PX_LiveVisualDiagSum(trig.sind);
	s_visual_diag.sind_transform=trig.sind[PX_VISUAL_SCOPE_TRANSFORM];
	s_visual_diag.sind_final=trig.sind[PX_VISUAL_SCOPE_FINAL];
	s_visual_diag.sind_set_rotation=trig.sind[PX_VISUAL_SCOPE_SET];
	s_visual_diag.sind_other=trig.sind[PX_VISUAL_SCOPE_OTHER];
	s_visual_diag.point_rotate_calls=PX_LiveVisualDiagSum(trig.point_rotate);
	s_visual_diag.unique_point_rotate_angles=trig.unique_point_rotate_angles;
	s_visual_diag.point_rotate_angle_samples=trig.point_rotate_angle_samples;
	s_visual_diag.point_rotate_angle_overflow=trig.point_rotate_angle_overflow;
	for (i=0;i<PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER;i++)
	{
		if (s_visual_ancestor_seen[i]) unique_ancestors++;
	}
	s_visual_diag.unique_ancestors=(px_dword)unique_ancestors;
	for (i=0;i<n;i++)
	{
		PX_LiveLayer *layer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
		rotations[i]=layer->rel_currentRotationAngle;
		local_rotations[i]=layer->rel_currentLocalRotationAngle;
	}
	s_visual_diag.unique_rotation_angles=(px_dword)PX_LiveVisualDiagUniqueFloats(rotations,n);
	s_visual_diag.unique_local_rotation_angles=(px_dword)PX_LiveVisualDiagUniqueFloats(local_rotations,n);
	s_visual_diag.history_valid=(s_visual_history_valid&&s_visual_history_layers==n)?1u:0u;
	if (s_visual_diag.history_valid)
	{
		for (i=0;i<n;i++)
		{
			PX_LiveLayer *layer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
			const PX_LiveVisualDiagSnap *snap=&s_visual_snap[i];
			if (layer->rel_currentRotationAngle!=snap->rotation) s_visual_diag.rotation_changed++;
			if (layer->rel_currentLocalRotationAngle!=snap->local_rotation) s_visual_diag.local_rotation_changed++;
			if (layer->rel_currentLocalScale!=snap->scale) s_visual_diag.scale_changed++;
			if (layer->rel_currentLocalTranslation.x!=snap->local_tx||
				layer->rel_currentLocalTranslation.y!=snap->local_ty||
				layer->rel_currentLocalTranslation.z!=snap->local_tz)
			{
				s_visual_diag.local_translation_changed++;
			}
			if (layer->rel_currentTranslation.x!=snap->hierarchy_tx||
				layer->rel_currentTranslation.y!=snap->hierarchy_ty)
			{
				s_visual_diag.hierarchy_translation_changed++;
			}
			if (layer->currentKeyPoint.x!=snap->key_x||
				layer->currentKeyPoint.y!=snap->key_y||
				layer->currentKeyPoint.z!=snap->key_z)
			{
				s_visual_diag.keypoint_changed++;
			}
			if (PX_LiveVisualDiagAncestorChanged(plive,i)) s_visual_diag.parent_visual_changed++;
		}
	}
	for (i=0;i<n;i++)
	{
		PX_LiveLayer *layer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
		PX_LiveVisualDiagSnap *snap=&s_visual_snap[i];
		snap->rotation=layer->rel_currentRotationAngle;
		snap->local_rotation=layer->rel_currentLocalRotationAngle;
		snap->scale=layer->rel_currentLocalScale;
		snap->local_tx=layer->rel_currentLocalTranslation.x;
		snap->local_ty=layer->rel_currentLocalTranslation.y;
		snap->local_tz=layer->rel_currentLocalTranslation.z;
		snap->hierarchy_tx=layer->rel_currentTranslation.x;
		snap->hierarchy_ty=layer->rel_currentTranslation.y;
		snap->key_x=layer->currentKeyPoint.x;
		snap->key_y=layer->currentKeyPoint.y;
		snap->key_z=layer->currentKeyPoint.z;
	}
	s_visual_history_valid=1;
	s_visual_history_layers=n;
	s_visual_diag.diag_pose_us=plive->frameProfile.poseUs;
	s_visual_diag.diag_physical_us=plive->frameProfile.physicalUs;
}

void PX_LiveFrameworkVisualDiagInvalidate(void)
{
	s_visual_history_valid=0;
}

void PX_LiveFrameworkVisualDiagCopy(PX_LiveFramework *plive, PX_LiveVisualDiagFrame *out)
{
	(void)plive;
	if (!out) return;
	*out=s_visual_diag;
}

int PX_LiveFrameworkVisualDiagTopology(PX_LiveFramework *plive, PX_LiveVisualDiagLayer *out, int capacity)
{
	int i,n;
	if (!plive||!out||capacity<=0) return 0;
	n=plive->layers.size;
	if (n>capacity) n=capacity;
	for (i=0;i<n;i++)
	{
		PX_LiveLayer *layer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
		int depth=1,guard=0,parent=layer->parent_index,children=0,c;
		PX_LiveVisualDiagLayer *row=&out[i];
		memset(row,0,sizeof(*row));
		row->index=i;
		row->parent=layer->parent_index;
		while (parent>=0&&parent<plive->layers.size&&guard++<PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER)
		{
			depth++;
			parent=PX_VECTORAT(PX_LiveLayer,&plive->layers,parent)->parent_index;
		}
		row->depth=depth;
		for (c=0;c<PX_COUNTOF(layer->child_index);c++)
		{
			if (layer->child_index[c]==-1) break;
			children++;
		}
		row->children=children;
		row->rotation_bits=PX_LiveVisualDiagFloatBits(layer->rel_currentRotationAngle);
		row->local_rotation_bits=PX_LiveVisualDiagFloatBits(layer->rel_currentLocalRotationAngle);
		row->scale_bits=PX_LiveVisualDiagFloatBits(layer->rel_currentLocalScale);
		row->local_tx_bits=PX_LiveVisualDiagFloatBits(layer->rel_currentLocalTranslation.x);
		row->local_ty_bits=PX_LiveVisualDiagFloatBits(layer->rel_currentLocalTranslation.y);
		row->key_x_bits=PX_LiveVisualDiagFloatBits(layer->currentKeyPoint.x);
		row->key_y_bits=PX_LiveVisualDiagFloatBits(layer->currentKeyPoint.y);
		for (c=0;c<PX_LIVE_ID_MAX_LEN-1&&layer->id[c];c++) row->id[c]=layer->id[c];
		row->id[c]=0;
	}
	return n;
}
#endif

static px_void PX_LiveFramework_GetLayerVisualTransform(PX_LiveFramework *pLive,PX_LiveLayer *pLayer,px_float *pScale,px_float *pRotation,px_point *pTranslation)
{
	PX_LiveLayer *pCurrent=pLayer;
	*pScale=1;
	*pRotation=0;
	*pTranslation=PX_POINT(0,0,0);
#if L2D_CFG_PROFILE_VISUAL
	{
	int depth=0;
	PX_VisualDiagPush(PX_VISUAL_SCOPE_TRANSFORM);
	s_visual_diag.get_visual_calls++;
#endif
	while(pCurrent)
	{
#if L2D_CFG_PROFILE_VISUAL
		int layer_index;
		long long t_enter,t_after_pivot,t_after_local,t_before_rel,t_after_rel;
		depth++;
		s_visual_diag.ancestor_visits++;
		layer_index=(int)(pCurrent-(PX_LiveLayer *)pLive->layers.data);
		if (layer_index>=0&&layer_index<PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER)
		{
			s_visual_ancestor_seen[layer_index]=1;
		}
		t_enter=l2d_pe_time_us();
#endif
		px_point pivot=pCurrent->currentKeyPoint;
		if (!pCurrent->rel_rotationTrigValid ||
			pCurrent->rel_cachedRotationAngle!=pCurrent->rel_currentRotationAngle)
		{
			PX_LiveFrameworkSetLayerCurrentRotationAngle(pLive, pCurrent, pCurrent->rel_currentRotationAngle);
		}
		PX_LiveFrameworkEnsureLocalRotationTrig(pLive, pCurrent);
#if L2D_CFG_PROFILE_VISUAL
		t_after_pivot=l2d_pe_time_us();
#endif
		px_point localTranslation=PX_LiveFrameworkRotatePointFromSinCos(
			pCurrent->rel_currentLocalTranslation,
			pCurrent->rel_currentRotationCos,
			pCurrent->rel_currentRotationSin);
#if L2D_CFG_PROFILE_VISUAL
		t_after_local=l2d_pe_time_us();
#endif
		px_point relative=PX_PointSub(*pTranslation,pivot);
#if L2D_CFG_PROFILE_VISUAL
		t_before_rel=l2d_pe_time_us();
#endif
		relative=PX_LiveFrameworkRotatePointFromSinCos(
			relative,
			pCurrent->rel_currentLocalRotationCos,
			pCurrent->rel_currentLocalRotationSin);
#if L2D_CFG_PROFILE_VISUAL
		t_after_rel=l2d_pe_time_us();
#endif
		relative=PX_PointMul(relative,pCurrent->rel_currentLocalScale);
		*pTranslation=PX_PointAdd(PX_PointAdd(relative,pivot),localTranslation);
		pTranslation->z=0;
		*pScale*=pCurrent->rel_currentLocalScale;
		*pRotation+=pCurrent->rel_currentLocalRotationAngle;
		pCurrent=PX_LiveFrameworkGetLayerParent(pLive,pCurrent);
#if L2D_CFG_PROFILE_VISUAL
		s_visual_diag.traverse_us+=(px_dword)((t_after_pivot-t_enter)+(t_before_rel-t_after_local)+(l2d_pe_time_us()-t_after_rel));
		s_visual_diag.local_translation_rotate_us+=(px_dword)(t_after_local-t_after_pivot);
		s_visual_diag.relative_rotate_us+=(px_dword)(t_after_rel-t_before_rel);
#endif
	}
#if L2D_CFG_PROFILE_VISUAL
	if (depth>(int)s_visual_diag.max_depth) s_visual_diag.max_depth=(px_dword)depth;
	s_visual_diag.depth_sum+=(px_dword)depth;
	PX_VisualDiagPop();
	}
#endif
}

static px_point PX_LiveFramework_GetLayerVisualKeyPoint(PX_LiveFramework *pLive,PX_LiveLayer *pLayer)
{
	px_float scale,rotation;
	px_point translation,point;
	PX_LiveFramework_GetLayerVisualTransform(pLive,pLayer,&scale,&rotation,&translation);
	point=PX_PointRotate(pLayer->currentKeyPoint,rotation);
	point=PX_PointAdd(PX_PointMul(point,scale),translation);
	point.z=pLayer->currentKeyPoint.z;
	return point;
}

/* ── 顶点更新与弹性物理 ─────────────────────────── */

/** 更新图层的所有顶点位置（含拉伸、旋转、平移、扇形变形及弹性物理模拟） */
static px_void PX_LiveFramework_UpdateLayerVertices(PX_LiveFramework *pLive,PX_LiveLayer *pLayer,px_dword elapsed)
{
	px_int i;
	px_point2D keyDirection;
	px_int k;
	px_float visualScale,visualRotation;
	px_float visualRotationCos,visualRotationSin;
	px_point visualTranslation;
#if L2D_CFG_PROFILE_FINE
	long long fine_start_us, fine_vertex_begin_us, fine_stretch_begin_us;
	long long fine_stretch_total_us=0;
	fine_start_us=l2d_pe_time_us();
#endif


	PX_LiveFrameworkUpdateLayerRenderVerticesUV(pLive,pLayer);
#if L2D_CFG_PROFILE_FINE
	pLive->frameProfile.uvUpdateUs+=(px_dword)(l2d_pe_time_us()-fine_start_us);
#endif

	/* REALTIME30 forces k=0 below, so keyDirection is never consumed. */
	keyDirection=PX_POINT2D(0,1);
	if (pLive->animationMode!=PX_LIVE_MODE_REALTIME30)
	{
		if (pLayer->child_index[0]==-1)
		{
			keyDirection=PX_POINT2D(0,1);
		}
		else
		{
			keyDirection=PX_POINT2D(0,0);
			for (i=0;i<PX_COUNTOF(pLayer->child_index);i++)
			{
				px_point v;
				if (pLayer->child_index[i]==-1)
				{
					break;
				}
			
				v=PX_PointNormalization(PX_PointSub(pLayer->currentKeyPoint,PX_LiveFrameworkGetLayerChild(pLive,pLayer,pLayer->child_index[i])->currentKeyPoint));
				keyDirection=PX_Point2DAdd(keyDirection,PX_POINT2D(v.x,v.y));
			}
			keyDirection=PX_Point2DNormalization(keyDirection);
		}
	}
#if L2D_CFG_PROFILE_FINE
	fine_start_us=l2d_pe_time_us();
#endif
	PX_LiveFramework_GetLayerVisualTransform(pLive,pLayer,&visualScale,&visualRotation,&visualTranslation);
	/* The visual rotation is identical for every vertex in this layer.  Building
	 * a matrix (and evaluating sin/cos) for every vertex consumed a large share
	 * of the ESP32-P4 frame budget, especially while head/face axes are active. */
#if L2D_CFG_PROFILE_VISUAL
	{
	long long final_begin_us=l2d_pe_time_us();
	PX_VisualDiagPush(PX_VISUAL_SCOPE_FINAL);
#endif
	visualRotationCos=PX_cos_angle(visualRotation);
	visualRotationSin=PX_sin_angle(visualRotation);
#if L2D_CFG_PROFILE_VISUAL
	PX_VisualDiagPop();
	s_visual_diag.final_sincos_us+=(px_dword)(l2d_pe_time_us()-final_begin_us);
	}
#endif
#if L2D_CFG_PROFILE_FINE
	pLive->frameProfile.visualTransformUs+=(px_dword)(l2d_pe_time_us()-fine_start_us);
	fine_vertex_begin_us=l2d_pe_time_us();
#endif

	//for each vertex
	for (i=0;i<pLayer->vertices.size;i++)
	{
		PX_LiveVertex *plv=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,i);
		px_point resultPosition;
		
		//get relative position
		resultPosition.x=plv->sourcePosition.x-pLayer->keyPoint.x;
		resultPosition.y=plv->sourcePosition.y-pLayer->keyPoint.y;
		resultPosition.z=plv->sourcePosition.z-pLayer->keyPoint.z;
		//panc translation
		if (pLayer->panc_currentx!=pLayer->panc_sx|| pLayer->panc_currenty != pLayer->panc_sy)
		{
			if (plv->sourcePosition.x>pLayer->panc_x&& plv->sourcePosition.x < pLayer->panc_x+pLayer->panc_width)
			{
				if (plv->sourcePosition.x < pLayer->panc_sx)
				{
					px_float disx = plv->sourcePosition.x - pLayer->panc_x;
					resultPosition.x += disx * (pLayer->panc_currentx-pLayer->panc_x) /(pLayer->panc_sx- pLayer->panc_x) - disx;
				}
				else
				{
					px_float disx =  (pLayer->panc_x+pLayer->panc_width)- plv->sourcePosition.x;
					resultPosition.x -= disx * (pLayer->panc_x + pLayer->panc_width - pLayer->panc_currentx) / (pLayer->panc_x + pLayer->panc_width - pLayer->panc_sx) - disx;
				}
			}

			if (plv->sourcePosition.y > pLayer->panc_y && plv->sourcePosition.y < pLayer->panc_y + pLayer->panc_height)
			{
				if (plv->sourcePosition.y < pLayer->panc_sy)
				{
					px_float disy = plv->sourcePosition.y - pLayer->panc_y;
					resultPosition.y += disy * (pLayer->panc_currenty - pLayer->panc_y) / (pLayer->panc_sy - pLayer->panc_y) - disy;
				}
				else
				{
					px_float disy = (pLayer->panc_y + pLayer->panc_height) - plv->sourcePosition.y;
					resultPosition.y -= disy * (pLayer->panc_y + pLayer->panc_height - pLayer->panc_currenty) / (pLayer->panc_y + pLayer->panc_height - pLayer->panc_sy) - disy;
				}
			}

			
		}
		
		
		//relative translation
		resultPosition.x+=plv->currentTranslation.x;
		resultPosition.y+=plv->currentTranslation.y;
		resultPosition.z+=plv->currentTranslation.z;

#if L2D_CFG_PROFILE_FINE
		fine_stretch_begin_us=l2d_pe_time_us();
#endif
		//stretch
		do 
		{
			px_int j;
			for (j=0;j<PX_COUNTOF(pLayer->child_index);j++)
			{
				PX_LiveLayer *pChild=PX_LiveFrameworkGetLayerChild(pLive,pLayer,pLayer->child_index[j]);
				if(!pChild)
					break;

				
				if (pChild->rel_currentStretch!=1)
				{
					px_float cos_v12;
					px_point v1,v2,u1;
					v1=PX_PointSub(pChild->keyPoint,pLayer->keyPoint);
					v2=resultPosition;
					cos_v12=PX_PointDot(v1,v2)/PX_PointMod(v1)/PX_PointMod(v2);
					if (cos_v12>0)
					{
						px_float distance;
						u1=PX_PointNormalization(v1);
						distance=cos_v12*PX_PointMod(v2);
						resultPosition=PX_PointAdd(resultPosition,PX_PointMul(u1,distance*(pChild->rel_currentStretch-1)));
					}
				}
			}
		} while (0);


#if L2D_CFG_PROFILE_FINE
		fine_stretch_total_us+=l2d_pe_time_us()-fine_stretch_begin_us;
#endif
		//Rotation
		resultPosition=PX_LiveFrameworkRotatePointCached(resultPosition,
			pLayer->rel_currentRotationCos, pLayer->rel_currentRotationSin);

		//absolute translation
		resultPosition.x+=pLayer->currentKeyPoint.x;
		resultPosition.y+=pLayer->currentKeyPoint.y;

			/* Apply this layer visual transform and every inherited ancestor transform. */
			{
				px_float sourceZ=resultPosition.z;
				resultPosition=PX_LiveFrameworkRotatePointCached(
					resultPosition,visualRotationCos,visualRotationSin);
				resultPosition=PX_PointAdd(PX_PointMul(resultPosition,visualScale),visualTranslation);
				resultPosition.z=sourceZ;
			}


		//elastic
		/* RT30 模式禁用弹性物理：大帧间隔下二次弹簧(distance²·dt)不稳定，
		 * 视觉变换移动目标顶点会引发指数发散漂移；RT30 直接取目标位置。
		 * Timeline 模式保持原弹性（成熟工程已验证）。 */
		k=(pLive->animationMode==PX_LIVE_MODE_REALTIME30)?0:plv->k;

		if (k==0)
		{
			plv->currentPosition=resultPosition;
		}
		else
		{
			px_point2D direction,direction_normal;
			px_float distance;
			px_dword updateelapsed=elapsed+elapsed/2;
			px_dword atomelapsed;

			while (updateelapsed)
			{
				if (updateelapsed>50)
				{
					atomelapsed=50;
					updateelapsed-=50;
				}
				else
				{
					atomelapsed=updateelapsed;
					updateelapsed=0;
				}
				direction.x=resultPosition.x-plv->currentPosition.x;
				direction.y=resultPosition.y-plv->currentPosition.y;

				direction_normal=PX_Point2DNormalization(direction);
				distance=PX_Point2DMod(direction);


				if (distance>k)
				{
					plv->currentPosition.x=resultPosition.x-direction_normal.x*(k);
					plv->currentPosition.y=resultPosition.y-direction_normal.y*(k);
					distance=k*1.0f;
				}


				do
				{
					px_point2D incv=PX_Point2DMul(direction_normal,distance*distance);
					px_point2D  velocity;
					px_point velocity_vx,velocity_vy;
					px_float _cos,length;
					incv.x+=pLayer->rel_impulse.x;
					incv.y+=pLayer->rel_impulse.y;
					incv=PX_Point2DMul(incv,atomelapsed/1000.f);
					velocity=PX_Point2DAdd(PX_POINT2D(plv->velocity.x,plv->velocity.y),incv);
					if (velocity.x||velocity.y)
					{
						//resistance
						_cos=PX_Point2DDot(velocity,keyDirection)/PX_Point2DMod(velocity)/PX_Point2DMod(keyDirection);
						length=_cos*PX_Point2DMod(velocity);
						velocity_vx.x=length*keyDirection.x;
						velocity_vx.y=length*keyDirection.y;
						velocity_vx.z=0;

						velocity_vy.x=velocity.x-velocity_vx.x;
						velocity_vy.y=velocity.y-velocity_vx.y;
						velocity_vy.z=0;


						velocity_vx=PX_PointMul(velocity_vx,1.0f-atomelapsed/(k*10.f+50));
						velocity_vy=PX_PointMul(velocity_vy,1.0f-atomelapsed/(k*30.f+50));

						if (velocity_vx.x>10000||velocity_vx.y>10000||velocity_vy.x>10000||velocity_vy.y>10000)
						{
							PX_ASSERT();
						}

						plv->velocity=PX_PointAdd(velocity_vx,velocity_vy);
					}
					plv->currentPosition=PX_PointAdd(plv->currentPosition,PX_PointMul(plv->velocity,atomelapsed/1000.f));
										
				}while(0);
			}
			
		}
		
	}
#if L2D_CFG_PROFILE_FINE
	{
		long long fine_vertex_total_us=l2d_pe_time_us()-fine_vertex_begin_us;
		pLive->frameProfile.stretchUs+=(px_dword)fine_stretch_total_us;
		pLive->frameProfile.vertexTransformUs+=(px_dword)(fine_vertex_total_us-fine_stretch_total_us);
	}
#endif
}

/* ── 物理更新编排 ──────────────────────────────── */

/** 执行完整的物理更新流程：插值计算 -> 关键点更新 -> 顶点更新 */
static px_void PX_LiveFrameworkUpdatePhysical(PX_LiveFramework *plive,px_dword elapsed,px_bool interpolateTimeline)
{
	px_int i;
	if (interpolateTimeline)
	{
		for (i=0;i<plive->layers.size;i++)
		{
			PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
			PX_LiveFrameworkUpdateLayerInterpolation(plive,pLayer);
		}
	}
	else if (plive->animationMode==PX_LIVE_MODE_REALTIME30)
	{
		/* RT30 已直接写好 current/begin/end。只刷新设备端 sin/cos 缓存，
		 * 不再无意义地遍历每个图层的全部顶点做 begin/end 插值。 */
		for (i=0;i<plive->layers.size;i++)
		{
			PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
			PX_LiveFrameworkSetLayerCurrentRotationAngle(plive, 
				pLayer,pLayer->rel_currentRotationAngle);
		}
	}

#if L2D_CFG_PROFILE_FINE
	long long fine_keypoint_begin_us=l2d_pe_time_us();
#endif
	//LayerUpdate
	for (i=0;i<plive->layers.size;i++)
	{
		PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
		if (pLayer->parent_index==-1)
		{
			PX_LiveFramework_UpdateLayerKeyPoint(plive,pLayer);
		}
	}

#if L2D_CFG_PROFILE_FINE
	plive->frameProfile.keypointUs+=(px_dword)(l2d_pe_time_us()-fine_keypoint_begin_us);
#endif
	for (i=0;i<plive->layers.size;i++)
	{
		PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
		PX_LiveFramework_UpdateLayerVertices(plive,pLayer,elapsed);
	}
}

/* ── 虚拟机指令执行 ─────────────────────────────── */

/** 执行指定动画帧的指令集：更新每个图层的纹理映射、旋转、拉伸、平移和顶点位移 */
static px_bool PX_LiveFrameworkExecuteInstr(PX_LiveFramework *plive,px_int animation_index,px_int frameindex)
{
	//execute instr
	px_int frame_offset=0,frame_size=0;
	px_int layerindex=0;
	PX_LiveAnimation *pAnimation;
	px_byte *pFrameInstrData;
	PX_LiveAnimationFrameHeader *pFrameHeader;
	if (animation_index<0||animation_index>=plive->liveAnimations.size)
	{
		goto _ERROR;
	}
	pAnimation=PX_VECTORAT(PX_LiveAnimation,&plive->liveAnimations,animation_index);
	if (frameindex<0)
	{
		goto _ERROR;
	}
	if (frameindex >= pAnimation->framesMemPtr.size)
	{
		goto _ERROR;
	}
	pFrameInstrData=*PX_VECTORAT(px_byte *,&pAnimation->framesMemPtr,frameindex);
	pFrameHeader=(PX_LiveAnimationFrameHeader *)pFrameInstrData;
	frame_size=pFrameHeader->size+sizeof(PX_LiveAnimationFrameHeader);
	//////////////////////////////////////////////////////////////////////////
	/* ── 更新时间戳 ──────────────────────── */
	plive->reg_duration=pFrameHeader->duration_ms;
	frame_offset+=sizeof(PX_LiveAnimationFrameHeader);
	/* ── 解析荷载数据（纹理/旋转变换/顶点变换等） ──── */
	while (frame_offset<frame_size)
	{
		PX_LiveLayer *pLayer;
		PX_LiveAnimationFramePayload *pPayload=(PX_LiveAnimationFramePayload *)(pFrameInstrData+frame_offset);

		pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,layerindex);
		
		/////////////////////////////////////////* ── 纹理映射 ────────────────────── */
		pLayer->RenderTextureIndex=pPayload->mapTexture;
		
		/////////////////////////////////////////////* ── 旋转寄存器 ────────────────────── */
		pLayer->rel_beginRotationAngle=pLayer->rel_endRotationAngle;
		PX_LiveFrameworkSetLayerCurrentRotationAngle(plive, pLayer, pLayer->rel_beginRotationAngle);
		pLayer->rel_endRotationAngle=pPayload->rotation;

		/////////////////////////////////////////////* ── 拉伸寄存器 ────────────────────── */
		pLayer->rel_beginStretch=pLayer->rel_endStretch;
		pLayer->rel_currentStretch=pLayer->rel_beginStretch;
		pLayer->rel_endStretch=pPayload->stretch;

		/////////////////////////////////////////////* ── 平移寄存器 ────────────────────── */
		pLayer->rel_beginTranslation=pLayer->rel_endTranslation;
		pLayer->rel_currentTranslation=pLayer->rel_beginTranslation;
		pLayer->rel_endTranslation=pPayload->translation;

		/////////////////////////////////////////////* ── 弹性冲量 ──────────────────────── */
		pLayer->rel_impulse=pPayload->impulse;


		/////////////////////////////////////////////* ── 扇形变形（PANC） ──────────────── */
		pLayer->panc_x = pPayload->panc_x;
		pLayer->panc_y = pPayload->panc_y;

		pLayer->panc_width = pPayload->panc_width;
		pLayer->panc_height = pPayload->panc_height;

		pLayer->panc_sx = pPayload->panc_sx;
		pLayer->panc_sy = pPayload->panc_sy;

		pLayer->panc_beginx = pLayer->panc_endx;
		pLayer->panc_beginy = pLayer->panc_endy;

		pLayer->panc_currentx = pLayer->panc_beginx;
		pLayer->panc_currenty = pLayer->panc_beginy;

		pLayer->panc_endx = pPayload->panc_endx;
		pLayer->panc_endy = pPayload->panc_endy;

		/////////////////////////////////////////////* ── 顶点位移寄存器 ──────────────────── */
		if (pPayload->translationVerticesCount!=pLayer->vertices.size)
		{
			goto _ERROR;
		}
		else
		{
			px_int j;
			px_point *pVertexTranslation=(px_point *)(pFrameInstrData+frame_offset+sizeof(PX_LiveAnimationFramePayload));
		
			for (j=0;j<(px_int)pPayload->translationVerticesCount;j++)
			{
				PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,j)->beginTranslation=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,j)->endTranslation;
				PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,j)->currentTranslation=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,j)->beginTranslation;
				PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,j)->endTranslation=pVertexTranslation[j];
			}
		}
		frame_offset+=sizeof(PX_LiveAnimationFramePayload);
		frame_offset+=sizeof(px_point)*pPayload->translationVerticesCount;
		layerindex++;
	}
	if (layerindex!=plive->layers.size)
	{
		goto _ERROR;
	}
	return PX_TRUE;
_ERROR:
	return PX_FALSE;
}

/* ── 虚拟机更新循环 ─────────────────────────────── */

/** 驱动动画虚拟机的执行循环：按时间推进播放帧 */
static px_void PX_LiveFrameworkUpdateVM(PX_LiveFramework *plive,px_dword elapsed)
{
	if (plive->status==PX_LIVEFRAMEWORK_STATUS_STOP)
	{
		return;
	}

	plive->reg_elapsed+=elapsed;


	while (PX_TRUE)
	{

		if (plive->reg_elapsed>=plive->reg_duration)
		{
			px_dword duration = plive->reg_duration;
			if (!PX_LiveFrameworkExecuteInstr(plive, plive->reg_animation, plive->reg_ip))
			{
				plive->status = PX_LIVEFRAMEWORK_STATUS_STOP;
				goto _ERROR;
			}

			//update time
			plive->reg_elapsed -= duration;

			//update ip
			plive->reg_ip++;
		}
		else
			break;
	}
	return;
_ERROR:
	return;
}

/* ── 图层渲染 ──────────────────────────────────── */

/** 渲染单个图层：对三角形网格进行栅格化，支持调试网格显示 */
static px_void PX_LiveFrameworkRenderLayer(px_surface *psurface,PX_LiveFramework *plive,PX_LiveLayer *pLayer,px_int x,px_int y,px_dword elapsed)
{
	PX_TEXTURERENDER_BLEND blend;
	PX_LiveTexture *pLiveTexture;
	if (!pLayer->visible)
	{
		return;
	}
	pLiveTexture=PX_LiveFrameworkGetLiveTexture(plive,pLayer->RenderTextureIndex);
	if (pLiveTexture)
	{
		px_texture *pTexture;		
		px_float renderScale = plive->renderScale;
		px_float renderOffsetX;
		px_float renderOffsetY;
		pTexture=&pLiveTexture->Texture;

		if (renderScale <= 0 || renderScale > 1)
		{
			renderScale = 1;
		}
		renderOffsetX = plive->width * (1 - renderScale) * 0.5f;
		renderOffsetY = plive->height * (1 - renderScale) * 0.5f;

		if (pLayer->triangles.size&&pLayer->vertices.size)
		{
			px_int t;
			PX_Delaunay_Triangle *pTriangleIndex;
			for (t=0;t<pLayer->triangles.size;t++)
			{
				PX_LiveRenderVertex v0,v1,v2;
				PX_LiveVertex *pv0,*pv1,*pv2;
				pTriangleIndex=PX_VECTORAT(PX_Delaunay_Triangle,&pLayer->triangles,t);
				pv0=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,pTriangleIndex->index1);
				pv1=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,pTriangleIndex->index2);
				pv2=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,pTriangleIndex->index3);
				v0.position.x=pv0->currentPosition.x*renderScale+x+renderOffsetX;
				v0.position.y=pv0->currentPosition.y*renderScale+y+renderOffsetY;
				v0.position.z=1;
				v0.normal=pv0->normal;
				v0.u=pv0->u;
				v0.v=pv0->v;
				v1.position.x=pv1->currentPosition.x*renderScale+x+renderOffsetX;
				v1.position.y=pv1->currentPosition.y*renderScale+y+renderOffsetY;
				v1.position.z=1;
				v1.normal=pv1->normal;
				v1.u=pv1->u;
				v1.v=pv1->v;
				v2.position.x=pv2->currentPosition.x*renderScale+x+renderOffsetX;
				v2.position.y=pv2->currentPosition.y*renderScale+y+renderOffsetY;
				v2.position.z=1;
				v2.normal=pv2->normal;
				v2.u=pv2->u;
				v2.v=pv2->v;
#if L2D_CFG_PROFILE_ROI_DIAG || L2D_CFG_SRM_ROI
				PX_LiveFrameworkBoundsVertex(&plive->geometryBounds,v0.position.x,v0.position.y);
				PX_LiveFrameworkBoundsVertex(&plive->geometryBounds,v1.position.x,v1.position.y);
				PX_LiveFrameworkBoundsVertex(&plive->geometryBounds,v2.position.x,v2.position.y);
#endif

				if (plive->currentEditLayerIndex>=0&&plive->currentEditLayerIndex<plive->layers.size)
				{
					if (pLayer==PX_VECTORAT(PX_LiveLayer,&plive->layers,plive->currentEditLayerIndex)||!plive->showFocusLayer)
					{
						PX_LiveFramework_RenderListRasterization(psurface,plive,v0,v1,v2,pTexture,PX_NULL);
					}
					else
					{
						blend.alpha=0.2f;
						blend.hdr_B=1;
						blend.hdr_G=1;
						blend.hdr_R=1;
						PX_LiveFramework_RenderListRasterization(psurface,plive,v0,v1,v2,pTexture,&blend);
					}
				}
				else
				{
					PX_LiveFramework_RenderListRasterization(psurface,plive,v0,v1,v2,pTexture,PX_NULL);
				}
				
			}

			if (pLayer->showMesh)
			{
				for (t=0;t<pLayer->triangles.size;t++)
				{
					PX_LiveRenderVertex v0,v1,v2;
					PX_LiveVertex *pv0,*pv1,*pv2;
					pTriangleIndex=PX_VECTORAT(PX_Delaunay_Triangle,&pLayer->triangles,t);
					pv0=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,pTriangleIndex->index1);
					pv1=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,pTriangleIndex->index2);
					pv2=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,pTriangleIndex->index3);
					v0.position.x=pv0->currentPosition.x*renderScale+x+renderOffsetX;
					v0.position.y=pv0->currentPosition.y*renderScale+y+renderOffsetY;
					v0.normal=pv0->normal;
					v0.u=pv0->u;
					v0.v=pv0->v;

					v1.position.x=pv1->currentPosition.x*renderScale+x+renderOffsetX;
					v1.position.y=pv1->currentPosition.y*renderScale+y+renderOffsetY;
					v1.normal=pv1->normal;
					v1.u=pv1->u;
					v1.v=pv1->v;

					v2.position.x=pv2->currentPosition.x*renderScale+x+renderOffsetX;
					v2.position.y=pv2->currentPosition.y*renderScale+y+renderOffsetY;
					v2.normal=pv2->normal;
					v2.u=pv2->u;
					v2.v=pv2->v;


					PX_GeoDrawLine(psurface,(px_int)v0.position.x,(px_int)v0.position.y,(px_int)v1.position.x,(px_int)v1.position.y,1,PX_COLOR(128,255,0,0));
					PX_GeoDrawLine(psurface,(px_int)v0.position.x,(px_int)v0.position.y,(px_int)v2.position.x,(px_int)v2.position.y,1,PX_COLOR(128,255,0,0));
					PX_GeoDrawLine(psurface,(px_int)v1.position.x,(px_int)v1.position.y,(px_int)v2.position.x,(px_int)v2.position.y,1,PX_COLOR(128,255,0,0));

					PX_GeoDrawSolidCircle(psurface,(px_int)v0.position.x,(px_int)v0.position.y,3,PX_COLOR(128,255,0,0));
					PX_GeoDrawSolidCircle(psurface,(px_int)v1.position.x,(px_int)v1.position.y,3,PX_COLOR(128,255,0,0));
					PX_GeoDrawSolidCircle(psurface,(px_int)v2.position.x,(px_int)v2.position.y,3,PX_COLOR(128,255,0,0));
				}

				if (plive->currentEditLayerIndex>=0&&plive->currentEditLayerIndex<plive->layers.size)
				{
					if (pLayer==PX_VECTORAT(PX_LiveLayer,&plive->layers,plive->currentEditLayerIndex))
					{
						if (plive->currentEditVertexIndex>=0&&plive->currentEditVertexIndex<pLayer->vertices.size)
						{
							PX_LiveVertex *pLiveVertex=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,plive->currentEditVertexIndex);
							PX_GeoDrawCircle(psurface,(px_int)(pLiveVertex->currentPosition.x+x),(px_int)(pLiveVertex->currentPosition.y+y),5,1,PX_COLOR(255,255,128,0));
						}
					}
				}

			}
		}
		else
		{
			if (plive->currentEditLayerIndex>=0&&plive->currentEditLayerIndex<plive->layers.size)
			{
				if (plive->showFocusLayer)
				{
					if (pLayer==PX_VECTORAT(PX_LiveLayer,&plive->layers,plive->currentEditLayerIndex))
					{
						PX_TextureRender(psurface,pTexture,(px_int)(x+pLiveTexture->textureOffsetX),
							(px_int)(y+pLiveTexture->textureOffsetY),PX_ALIGN_LEFTTOP,PX_NULL);
					}
					else
					{
						blend.alpha=0.2f;
						blend.hdr_B=1;
						blend.hdr_G=1;
						blend.hdr_R=1;
						PX_TextureRender(psurface,pTexture,(px_int)(x+pLiveTexture->textureOffsetX),
							(px_int)(y+pLiveTexture->textureOffsetY),PX_ALIGN_LEFTTOP,&blend);
					}
				}
				else
				{
					PX_TextureRender(psurface,pTexture,(px_int)(x+pLiveTexture->textureOffsetX),
						(px_int)(y+pLiveTexture->textureOffsetY),PX_ALIGN_LEFTTOP,PX_NULL);
				}
				
			}
			else
			{
				PX_TextureRender(psurface,pTexture,(px_int)(x+pLiveTexture->textureOffsetX),
					(px_int)(y+pLiveTexture->textureOffsetY),PX_ALIGN_LEFTTOP,PX_NULL);

			}
		}
	
	}
}
/* ── 主更新函数 ────────────────────────────────── */

static px_bool PX_LiveFrameworkHasElasticVertex(PX_LiveFramework *plive)
{
    px_int layerIndex,vertexIndex;
    for (layerIndex=0;layerIndex<plive->layers.size;layerIndex++)
    {
        PX_LiveLayer *layer=PX_VECTORAT(PX_LiveLayer,&plive->layers,layerIndex);
        for (vertexIndex=0;vertexIndex<layer->vertices.size;vertexIndex++)
        {
            if (PX_VECTORAT(PX_LiveVertex,&layer->vertices,vertexIndex)->k) return PX_TRUE;
        }
    }
    return PX_FALSE;
}

/** 执行状态更新：根据动画模式调度 VM 或实时更新，然后执行物理更新 */
px_void PX_LiveFrameworkUpdate(PX_LiveFramework *plive,px_dword elapsed)
{
	long long vm_begin_us, physical_begin_us;
	long long vm_elapsed_us, physical_elapsed_us;
#if L2D_CFG_PROFILE_VISUAL
	int visual_physical_ran=0;
#endif

	if (!plive)
	{
		return;
	}
	plive->trigCacheHit=0;
	plive->trigCacheMiss=0;
#if L2D_CFG_PROFILE_VISUAL
	PX_LiveVisualDiagBeginFrame();
#endif

#if L2D_CFG_PROFILE_FINE
	plive->frameProfile.keypointUs=0;
	plive->frameProfile.visualTransformUs=0;
	plive->frameProfile.stretchUs=0;
	plive->frameProfile.vertexTransformUs=0;
	plive->frameProfile.uvUpdateUs=0;
#endif
	vm_begin_us=l2d_pe_time_us();
	if (plive->animationMode==PX_LIVE_MODE_REALTIME30)
	{
		px_bool poseValid=PX_LiveRealtimeUpdate(plive);
		vm_elapsed_us=l2d_pe_time_us()-vm_begin_us;
		physical_begin_us=l2d_pe_time_us();
		if (poseValid&&(plive->meshPoseRevision!=plive->realtime.evaluatedRevision||
			PX_LiveFrameworkHasElasticVertex(plive)))
		{
			PX_LiveFrameworkUpdatePhysical(plive,elapsed,PX_FALSE);
			plive->meshPoseRevision=plive->realtime.evaluatedRevision;
#if L2D_CFG_PROFILE_VISUAL
			visual_physical_ran=1;
#endif
		}
	}
	else
	{
		PX_LiveFrameworkUpdateVM(plive,elapsed);
		vm_elapsed_us=l2d_pe_time_us()-vm_begin_us;
		physical_begin_us=l2d_pe_time_us();
		PX_LiveFrameworkUpdatePhysical(plive,elapsed,PX_TRUE);
#if L2D_CFG_PROFILE_VISUAL
		visual_physical_ran=1;
#endif
	}
	physical_elapsed_us=l2d_pe_time_us()-physical_begin_us;

	plive->frameProfile.poseUs=(px_dword)vm_elapsed_us;
	plive->frameProfile.physicalUs=(px_dword)physical_elapsed_us;
#if L2D_CFG_PROFILE_VISUAL
	PX_LiveVisualDiagFinishFrame(plive, visual_physical_ran);
#endif
	PX_LiveFrameworkProfileVMUs+=(unsigned long long)vm_elapsed_us;
	PX_LiveFrameworkProfilePhysicalUs+=(unsigned long long)physical_elapsed_us;
}

/* ── 当前帧渲染函数 ──────────────────────────────── */

/** 渲染当前状态：对齐、排序、绘制所有图层，并输出调试信息 */
px_void PX_LiveFrameworkRenderCurrent(px_surface *psurface,PX_LiveFramework *plive,px_int x,px_int y,PX_ALIGN refPoint)
{
	PX_memset(&plive->geometryBounds,0,sizeof(plive->geometryBounds));
	PX_QuickSortAtom sAtom[PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER];
	px_int i,count;
	long long layer_begin_us, sort_end_us, draw_end_us;
	long long layer_elapsed_us, sort_elapsed_us, draw_elapsed_us;

	switch (refPoint)
	{
	case PX_ALIGN_LEFTTOP:
		break;
	case PX_ALIGN_MIDTOP:
		x-=plive->width/2;
		break;
	case PX_ALIGN_RIGHTTOP:
		x-=plive->width;
		break;
	case PX_ALIGN_LEFTMID:
		y-=plive->height/2;
		break;
	case PX_ALIGN_CENTER:
		y-=plive->height/2;
		x-=plive->width/2;
		break;
	case PX_ALIGN_RIGHTMID:
		y-=plive->height/2;
		x-=plive->width;
		break;
	case PX_ALIGN_LEFTBOTTOM:
		y-=plive->height;
		break;
	case PX_ALIGN_MIDBOTTOM:
		y-=plive->height;
		x-=plive->width/2;
		break;
	case PX_ALIGN_RIGHTBOTTOM:
		y-=plive->height;
		x-=plive->width;
		break;
	}


	if (plive->layers.size)
	{
		layer_begin_us=l2d_pe_time_us();
		for (i=0;i<plive->layers.size;i++)
		{
			PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
			sAtom[i].weight=pLayer->currentKeyPoint.z;
			sAtom[i].pData=pLayer;
			if (i>=PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER-1)
			{
				break;
			}
		}
		count=i;
		PX_Quicksort_ArrayMaxToMin(sAtom,0,count-1);
		sort_end_us=l2d_pe_time_us();

		for (i=0;i<count;i++)
		{
			PX_LiveLayer *pLayer=(PX_LiveLayer *)sAtom[i].pData;
			px_int layerIndex=(px_int)(pLayer-(PX_LiveLayer *)plive->layers.data);
			long long single_begin_us=l2d_pe_time_us();
#if L2D_CFG_PROFILE_DETAIL
			s_detailLayer=layerIndex;
			if (layerIndex+1>s_detail.layerCount) s_detail.layerCount=layerIndex+1;
#endif
			PX_LiveFrameworkRenderLayer(psurface,plive,pLayer,x,y,0);
#if L2D_CFG_PROFILE_DETAIL
			s_detailLayer=-1;
#endif
			if (layerIndex>=0&&layerIndex<PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER)
			{
				PX_LiveFrameworkProfileLayerDetailUs[layerIndex]+=(unsigned long long)(l2d_pe_time_us()-single_begin_us);
			}
		}
		draw_end_us=l2d_pe_time_us();
		sort_elapsed_us=sort_end_us-layer_begin_us;
		draw_elapsed_us=draw_end_us-sort_end_us;
		layer_elapsed_us=draw_end_us-layer_begin_us;
	}
	else
	{
		layer_elapsed_us=0;
		sort_elapsed_us=0;
		draw_elapsed_us=0;
	}

	plive->frameProfile.sortUs=(px_dword)sort_elapsed_us;
	plive->frameProfile.drawUs=(px_dword)draw_elapsed_us;
	PX_LiveFrameworkProfileLayerUs+=(unsigned long long)layer_elapsed_us;
	PX_LiveFrameworkProfileSortUs+=(unsigned long long)sort_elapsed_us;
	PX_LiveFrameworkProfileDrawUs+=(unsigned long long)draw_elapsed_us;
	PX_LiveFrameworkProfileFrames++;
	if (PX_LiveFrameworkProfileFrames>=60)
	{
		px_int topIndex[5]={-1,-1,-1,-1,-1};
		unsigned long long topUs[5]={0,0,0,0,0};
		for (i=0;i<plive->layers.size&&i<PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER;i++)
		{
			unsigned long long avg=PX_LiveFrameworkProfileLayerDetailUs[i]/PX_LiveFrameworkProfileFrames;
			px_int slot;
			for (slot=0;slot<5;slot++)
			{
				if (avg>topUs[slot])
				{
					px_int move;
					for (move=4;move>slot;move--)
					{
						topUs[move]=topUs[move-1];
						topIndex[move]=topIndex[move-1];
					}
					topUs[slot]=avg;
					topIndex[slot]=i;
					break;
				}
			}
		}
		l2d_pe_log_info(PX_LIVEFRAMEWORK_PROFILE_TAG,
			"Profile avg: vm=%llu us, physical=%llu us, layer=%llu us(sort=%llu draw=%llu)",
			PX_LiveFrameworkProfileVMUs/PX_LiveFrameworkProfileFrames,
			PX_LiveFrameworkProfilePhysicalUs/PX_LiveFrameworkProfileFrames,
			PX_LiveFrameworkProfileLayerUs/PX_LiveFrameworkProfileFrames,
			PX_LiveFrameworkProfileSortUs/PX_LiveFrameworkProfileFrames,
			PX_LiveFrameworkProfileDrawUs/PX_LiveFrameworkProfileFrames);
		for (i=0;i<5;i++)
		{
			if (topIndex[i]>=0&&topIndex[i]<plive->layers.size)
			{
				PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,topIndex[i]);
				l2d_pe_log_info(PX_LIVEFRAMEWORK_PROFILE_TAG,
					"Layer top%d: index=%d id='%.*s' avg=%llu us visible=%d triangles=%d vertices=%d texture=%d",
					i+1,
					topIndex[i],
					PX_LIVE_ID_MAX_LEN,
					pLayer->id,
					topUs[i],
					pLayer->visible,
					pLayer->triangles.size,
					pLayer->vertices.size,
					pLayer->RenderTextureIndex);
			}
		}
		PX_LiveFrameworkProfileVMUs=0;
		PX_LiveFrameworkProfilePhysicalUs=0;
		PX_LiveFrameworkProfileLayerUs=0;
		PX_LiveFrameworkProfileSortUs=0;
		PX_LiveFrameworkProfileDrawUs=0;
		PX_memset(PX_LiveFrameworkProfileLayerDetailUs,0,sizeof(PX_LiveFrameworkProfileLayerDetailUs));
		PX_LiveFrameworkProfileFrames=0;
	}

	if (plive->showKeypoint&&!plive->showlinker)
	{
		for (i=0;i<plive->layers.size;i++)
		{
			PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
			if (pLayer->parent_index!=-1)
			{
				PX_GeoDrawSolidCircle(psurface,(px_int)(pLayer->currentKeyPoint.x+plive->refer_x),(px_int)(pLayer->currentKeyPoint.y+plive->refer_y),3,PX_COLOR(255,0,64,192));
				PX_GeoDrawCircle(psurface,(px_int)(pLayer->currentKeyPoint.x+plive->refer_x),(px_int)(pLayer->currentKeyPoint.y+plive->refer_y),5,1,PX_COLOR(255,0,64,192));
			}
			else
			{
				PX_GeoDrawSolidCircle(psurface,(px_int)(pLayer->currentKeyPoint.x+plive->refer_x),(px_int)(pLayer->currentKeyPoint.y+plive->refer_y),3,PX_COLOR(255,255,0,0));
				PX_GeoDrawCircle(psurface,(px_int)(pLayer->currentKeyPoint.x+plive->refer_x),(px_int)(pLayer->currentKeyPoint.y+plive->refer_y),5,1,PX_COLOR(255,255,0,0));
			}

		}
	}

	if (plive->showlinker)
	{
		for (i=0;i<plive->layers.size;i++)
		{
			px_int j;
			PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);

			if (pLayer->parent_index!=-1)
			{
				PX_GeoDrawSolidCircle(psurface,(px_int)(pLayer->currentKeyPoint.x+plive->refer_x),(px_int)(pLayer->currentKeyPoint.y+plive->refer_y),3,PX_COLOR(255,0,64,192));
				PX_GeoDrawCircle(psurface,(px_int)(pLayer->currentKeyPoint.x+plive->refer_x),(px_int)(pLayer->currentKeyPoint.y+plive->refer_y),5,1,PX_COLOR(255,0,64,192));
			}
			else
			{
				PX_GeoDrawSolidCircle(psurface,(px_int)(pLayer->currentKeyPoint.x+plive->refer_x),(px_int)(pLayer->currentKeyPoint.y+plive->refer_y),3,PX_COLOR(255,255,0,0));
				PX_GeoDrawCircle(psurface,(px_int)(pLayer->currentKeyPoint.x+plive->refer_x),(px_int)(pLayer->currentKeyPoint.y+plive->refer_y),5,1,PX_COLOR(255,255,0,0));
			}

			for (j=0;j<PX_LIVE_LAYER_MAX_LINK_NODE;j++)
			{

				PX_LiveLayer *pLinkLayer=PX_LiveFrameworkGetLayerChild(plive,pLayer,pLayer->child_index[j]);

				if (!pLinkLayer)
				{
					break;
				}

				PX_GeoDrawArrow(psurface,
					PX_POINT2D(pLayer->currentKeyPoint.x+plive->refer_x,pLayer->currentKeyPoint.y+plive->refer_y),
					PX_POINT2D(pLinkLayer->currentKeyPoint.x+plive->refer_x,pLinkLayer->currentKeyPoint.y+plive->refer_y),
					1,
					PX_COLOR(255,255,0,0)
					);

			}
		}
	}


	if (plive->showRange)
	{
		PX_GeoDrawBorder(psurface,x,y,x+plive->width,y+plive->height,1,PX_COLOR(255,0,0,255));
	}

	if (plive->showRootHelperLine)
	{
		PX_LiveLayer *pLayer=PX_LiveFrameworkGetCurrentEditLiveLayer(plive);
		if (pLayer&&pLayer->parent_index==-1)
		{
			PX_GeoDrawLine(psurface,(px_int)(x+pLayer->keyPoint.x),(px_int)(y+pLayer->keyPoint.y),(px_int)(x+pLayer->currentKeyPoint.x),(px_int)(y+pLayer->currentKeyPoint.y),1,PX_COLOR(255,255,0,255));
		}
	}

}

/* ── 主渲染函数 ────────────────────────────────── */

/** 渲染主函数：先执行状态更新，再进行当前帧渲染输出 */
px_void PX_LiveFrameworkRender(px_surface *psurface,PX_LiveFramework *plive,px_int x,px_int y,PX_ALIGN refPoint,px_dword elapsed)
{
	PX_LiveFrameworkUpdate(plive,elapsed);
	PX_LiveFrameworkRenderCurrent(psurface,plive,x,y,refPoint);
}
/** 使用框架参考坐标偏移进行渲染 */
px_void PX_LiveFrameworkRenderRefer(px_surface *psurface,PX_LiveFramework *plive,PX_ALIGN refPoint,px_dword elapsed)
{
	PX_LiveFrameworkRender(psurface,plive,(px_int)plive->refer_x,(px_int)plive->refer_y,refPoint,elapsed);
}



/* ── 动画播放控制 ──────────────────────────────── */

/** 播放指定索引的动画：重置帧指针并进入播放状态 */
px_bool PX_LiveFrameworkPlayAnimation(PX_LiveFramework *plive,px_int index)
{
	if (index>=0&&index<plive->liveAnimations.size)
	{
		if (plive->animationMode==PX_LIVE_MODE_REALTIME30)
		{
			PX_LiveRealtimeLeave(plive);
		}
		plive->animationMode=PX_LIVE_MODE_TIMELINE;
		plive->currentEditFrameIndex=-1;
		plive->currentEditLayerIndex=-1;
		plive->currentEditVertexIndex=-1;
		plive->reg_animation=index;
		plive->reg_ip = 0;
		plive->status=PX_LIVEFRAMEWORK_STATUS_PLAYING;
		return PX_TRUE;
	}
	return PX_FALSE;
}

/** 按名称查找并播放动画 */
px_bool PX_LiveFrameworkPlayAnimationByName(PX_LiveFramework *plive,const px_char name[])
{
	px_int i;
	for (i=0;i<plive->liveAnimations.size;i++)
	{
		PX_LiveAnimation *pAnimation=PX_VECTORAT(PX_LiveAnimation,&plive->liveAnimations,i);
		if (PX_strequ2(name,pAnimation->id))
		{
			PX_LiveFrameworkPlayAnimation(plive,i);
			return PX_TRUE;
		}
	}
	return PX_FALSE;
}

/* ── 图层查询 ──────────────────────────────────── */

/** 通过 ID 查找图层 */
PX_LiveLayer * PX_LiveFrameworkGetLayerById(PX_LiveFramework *plive,const px_char id[])
{
	px_int i;
	for (i=0;i<plive->layers.size;i++)
	{
		PX_LiveLayer *pLayer=PX_LiveFrameworkGetLayer(plive,i);
		if (pLayer&&PX_memequ(pLayer->id,id,PX_LIVE_ID_MAX_LEN))
		{
			return pLayer;
		}
	}
	return PX_NULL;
}

/* ── 图层创建 ──────────────────────────────────── */

/** 创建新图层：分配顶点和三角形数组，设置默认参数 */
PX_LiveLayer * PX_LiveFrameworkCreateLayer(PX_LiveFramework *plive,const px_char id[])
{
	PX_LiveLayer layer;
	px_int i;

	if (plive->layers.size>=PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER)
	{
		return PX_NULL;
	}

	if (PX_LiveFrameworkGetLayerById(plive,id))
	{
		return PX_NULL;
	}


	PX_memset(&layer,0,sizeof(layer));

	if(!PX_VectorInitialize(plive->mp,&layer.triangles,sizeof(PX_Delaunay_Triangle),0))return PX_FALSE;
	if(!PX_VectorInitialize(plive->mp,&layer.vertices,sizeof(PX_LiveVertex),0))return PX_FALSE;


	layer.rotationAngle=0;
	layer.rel_currentRotationCos=1;
	layer.visible=PX_TRUE;
	layer.keyPoint.z=1;

	layer.rel_beginStretch=1;
	layer.rel_currentStretch=1;
	layer.rel_endStretch=1;

	layer.RenderTextureIndex = -1;
	layer.LinkTextureIndex = -1;

	layer.parent_index=-1;
	for (i=0;i<PX_COUNTOF(layer.child_index);i++)
	{
		layer.child_index[i]=-1;
	}

	PX_strcpy(layer.id,id,sizeof(layer.id));

	if(!PX_VectorPushback(&plive->layers,&layer))
		return PX_FALSE;

	return PX_VECTORLAST(PX_LiveLayer,&plive->layers);

}

/** 获取指定图层的父图层 */
PX_LiveLayer * PX_LiveFrameworkGetLayerParent(PX_LiveFramework *plive,PX_LiveLayer *pLayer)
{
	if (pLayer->parent_index>=0&&pLayer->parent_index<plive->layers.size)
	{
		return PX_VECTORAT(PX_LiveLayer,&plive->layers,pLayer->parent_index);
	}
	
#ifdef PX_DEBUG_MODE
	if (pLayer->parent_index!=-1)
	{
		//Data Crash
		PX_ASSERT();
	}
#endif
	
	return PX_NULL;
}

/** 获取指定图层的子图层 */
PX_LiveLayer * PX_LiveFrameworkGetLayerChild(PX_LiveFramework *plive,PX_LiveLayer *pLayer,px_int childIndex)
{
	if (childIndex>=0&&childIndex<plive->layers.size)
	{
		return PX_VECTORAT(PX_LiveLayer,&plive->layers,childIndex);
	}

	if (childIndex!=-1)
	{
		PX_ASSERT();
	}
	
	return PX_NULL;
}

/* ── 纹理-图层绑定 ─────────────────────────────── */

/** 将纹理绑定到图层：同时设置图层的关键点位置为纹理中心 */
px_bool PX_LiveFrameworkLinkLayerTexture(PX_LiveFramework *plive,const px_char layer_id[],const px_char texture_id[])
{
	px_int index=PX_LiveFrameworkGetLiveTextureIndexById(plive,texture_id);
	PX_LiveTexture *pLiveTexture=PX_LiveFrameworkGetLiveTextureById(plive,texture_id);
	PX_LiveLayer *pLayer=PX_LiveFrameworkGetLayerById(plive,layer_id);
	if (pLiveTexture&&pLayer)
	{
		pLayer->LinkTextureIndex=index;
		pLayer->RenderTextureIndex=index;
		pLayer->keyPoint.x=pLiveTexture->textureOffsetX+pLiveTexture->Texture.width/2.0f;
		pLayer->keyPoint.y=pLiveTexture->textureOffsetY+pLiveTexture->Texture.height/2.0f;
		pLayer->rel_currentTranslation=PX_POINT(0,0,0);
		return PX_TRUE;
	}
	return PX_FALSE;
}

/** 通过索引获取图层 */
PX_LiveLayer * PX_LiveFrameworkGetLayer(PX_LiveFramework *plive,px_int i)
{
	if (i>=0&&i<plive->layers.size)
	{
		return PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
	}
	return PX_NULL;
}

/** 获取图层的索引位置 */
px_int PX_LiveFrameworkGetLayerIndex(PX_LiveFramework *plive,PX_LiveLayer *pLayer)
{
	px_int i;
	for (i=0;i<plive->layers.size;i++)
	{
		if (pLayer==PX_VECTORAT(PX_LiveLayer,&plive->layers,i))
		{
			return i;
		}
	}
	return -1;
}

/** 获取最后创建的图层 */
PX_LiveLayer *PX_LiveFrameworkGetLastCreateLayer(PX_LiveFramework *plive)
{
	if (plive->layers.size)
	{
		return PX_VECTORLAST(PX_LiveLayer,&plive->layers);
	}
	return PX_NULL;
}

/* ── UV 坐标更新 ──────────────────────────────── */

/** 计算图层顶点相对于绑定纹理的 UV 坐标 */
px_void PX_LiveFrameworkUpdateLayerSourceVerticesUV(PX_LiveFramework *plive,PX_LiveLayer *pLayer)
{
	px_int i;
	for (i=0;i<pLayer->vertices.size;i++)
	{
		PX_LiveVertex *pVertex=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,i);
		
		if (pLayer->LinkTextureIndex>=0&&pLayer->LinkTextureIndex<plive->livetextures.size)
		{
			PX_LiveTexture *pLiveTexture=PX_VECTORAT(PX_LiveTexture,&plive->livetextures,pLayer->LinkTextureIndex);
			pVertex->u=(pVertex->sourcePosition.x-pLiveTexture->textureOffsetX)*1.0f/pLiveTexture->Texture.width;
			pVertex->u=PX_LiveFrameworkClampUV((pVertex->sourcePosition.x-pLiveTexture->textureOffsetX)*1.0f/pLiveTexture->Texture.width);
			pVertex->v=PX_LiveFrameworkClampUV((pVertex->sourcePosition.y-pLiveTexture->textureOffsetY)*1.0f/pLiveTexture->Texture.height);
		}
	}
}

/** 更新所有图层的源顶点 UV 坐标 */
px_void PX_LiveFrameworkUpdateSourceVerticesUV(PX_LiveFramework *plive)
{
	px_int i;
	for (i=0;i<plive->layers.size;i++)
	{
		PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
		PX_LiveFrameworkUpdateLayerSourceVerticesUV(plive,pLayer);
	}
}

/** 更新指定图层的渲染 UV 坐标（基于当前渲染纹理计算） */
px_void PX_LiveFrameworkUpdateLayerRenderVerticesUV(PX_LiveFramework *plive,PX_LiveLayer *pLayer)
{
	px_int i;
	PX_LiveTexture *pLiveTexture;
	px_float invWidth,invHeight;
	if (pLayer->RenderTextureIndex<0||pLayer->RenderTextureIndex>=plive->livetextures.size)
	{
		return;
	}
	pLiveTexture=PX_VECTORAT(PX_LiveTexture,&plive->livetextures,pLayer->RenderTextureIndex);
	invWidth=1.0f/pLiveTexture->Texture.width;
	invHeight=1.0f/pLiveTexture->Texture.height;
	for (i=0;i<pLayer->vertices.size;i++)
	{
		PX_LiveVertex *pVertex=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,i);
		pVertex->u=PX_LiveFrameworkClampUV(
			(pVertex->sourcePosition.x-pLiveTexture->textureOffsetX)*invWidth);
		pVertex->v=PX_LiveFrameworkClampUV(
			(pVertex->sourcePosition.y-pLiveTexture->textureOffsetY)*invHeight);
	}
}

/* ── 动画数据管理 ──────────────────────────────── */

/** 通过 ID 获取动画对象 */
PX_LiveAnimation * PX_LiveFrameworkGetAnimationById(PX_LiveFramework *plive,const px_char id[])
{
	px_int i;
	for (i=0;i<plive->liveAnimations.size;i++)
	{
		PX_LiveAnimation *pAnimation=PX_LiveFrameworkGetAnimation(plive,i);
		if (pAnimation&&PX_strequ(pAnimation->id,id))
		{
			return pAnimation;
		}
	}
	return PX_NULL;

}

/** 创建新动画对象 */
PX_LiveAnimation * PX_LiveFrameworkCreateAnimation(PX_LiveFramework *plive,const px_char id[])
{
	PX_LiveAnimation animation;
	PX_memset(&animation,0,sizeof(animation));
	PX_VectorInitialize(plive->mp,&animation.framesMemPtr,sizeof(px_void *),1);
	PX_strcpy(animation.id,id,sizeof(animation.id));
	if(!PX_VectorPushback(&plive->liveAnimations,&animation))return PX_NULL;
	return PX_VECTORLAST(PX_LiveAnimation,&plive->liveAnimations);
}

/** 通过索引获取动画对象 */
PX_LiveAnimation * PX_LiveFrameworkGetAnimation(PX_LiveFramework *plive,px_int index)
{
	if (index>=0&&index<plive->liveAnimations.size)
	{
		return PX_VECTORAT(PX_LiveAnimation,&plive->liveAnimations,index);
	}
	return PX_NULL;
}

/** 获取最后创建的动画对象 */
PX_LiveAnimation * PX_LiveFrameworkGetLastCreateAnimation(PX_LiveFramework *plive)
{
	if (plive->liveAnimations.size)
	{
		return PX_VECTORLAST(PX_LiveAnimation,&plive->liveAnimations);
	}
	return PX_NULL;
}

/* ── 纹理管理 ──────────────────────────────────── */

/** 添加 Live2D 纹理到框架 */
px_bool PX_LiveFrameworkAddLiveTexture(PX_LiveFramework *plive,PX_LiveTexture livetexture)
{
	return PX_VectorPushback(&plive->livetextures,&livetexture);
}

/** 通过 ID 查找纹理 */
PX_LiveTexture * PX_LiveFrameworkGetLiveTextureById(PX_LiveFramework *plive,const px_char id[])
{
	px_int i;
	for (i=0;i<plive->livetextures.size;i++)
	{
		PX_LiveTexture *pTexture=PX_VECTORAT(PX_LiveTexture,&plive->livetextures,i);
		if (PX_strequ(id,pTexture->id))
		{
			return pTexture;
		}
	}
	return PX_NULL;
}

/** 通过 ID 获取纹理索引 */
px_int PX_LiveFrameworkGetLiveTextureIndexById(PX_LiveFramework *plive,const px_char id[])
{
	px_int i;
	for (i=0;i<plive->livetextures.size;i++)
	{
		PX_LiveTexture *pTexture=PX_VECTORAT(PX_LiveTexture,&plive->livetextures,i);
		if (PX_strequ(id,pTexture->id))
		{
			return i;
		}
	}
	return -1;
}

/** 通过指针比较获取纹理索引 */
px_int PX_LiveFrameworkGetLiveTextureIndex(PX_LiveFramework *plive,PX_LiveTexture *pCompareTexture)
{
	px_int i;
	for (i=0;i<plive->livetextures.size;i++)
	{
		PX_LiveTexture *pTexture=PX_VECTORAT(PX_LiveTexture,&plive->livetextures,i);
		if (pTexture==pCompareTexture)
		{
			return i;
		}
	}
	return -1;
}

/** 通过索引获取纹理 */
PX_LiveTexture * PX_LiveFrameworkGetLiveTexture(PX_LiveFramework *plive,px_int index)
{
	if (index>=0&&index<plive->livetextures.size)
	{
		return PX_VECTORAT(PX_LiveTexture,&plive->livetextures,index);
	}
	return PX_NULL;
}

/* ── 删除操作 ──────────────────────────────────── */

/** 通过 ID 删除纹理 */
px_void PX_LiveFrameworkDeleteLiveTextureById(PX_LiveFramework *plive,const px_char id[])
{
	px_int i;
	for (i=0;i<plive->livetextures.size;i++)
	{
		PX_LiveTexture *pTexture=PX_VECTORAT(PX_LiveTexture,&plive->livetextures,i);
		if (PX_strequ(id,pTexture->id))
		{
			PX_LiveFrameworkDeleteLiveTexture(plive,i);
			return;
		}
	}
}

/** 通过 ID 删除动画 */
px_void PX_LiveFrameworkDeleteLiveAnimationById(PX_LiveFramework *plive,const px_char id[])
{
	px_int i;
	for (i=0;i<plive->liveAnimations.size;i++)
	{
		PX_LiveAnimation *pAnimation=PX_VECTORAT(PX_LiveAnimation,&plive->liveAnimations,i);
		if (PX_strequ(id,pAnimation->id))
		{
			PX_LiveFrameworkDeleteLiveAnimation(plive,i);
			return;
		}
	}
}

/* ── 图层链接管理 ──────────────────────────────── */

/** 递归搜索子图层中是否已存在目标链接（防止循环链接） */
px_bool PX_LiveFrameworkLinkLayerSearchSubLayer(PX_LiveFramework *plive,PX_LiveLayer *pLayer,PX_LiveLayer *pSearchLayer)
{
	px_int i;
	for (i=0;i<PX_LIVE_LAYER_MAX_LINK_NODE;i++)
	{
		PX_LiveLayer *pSubLinkLayer=PX_LiveFrameworkGetLayerChild(plive,pLayer,pLayer->child_index[i]);

		if (pSubLinkLayer==PX_NULL)
		{
			return PX_FALSE;
		}

		if (pSubLinkLayer==pSearchLayer)
		{
			return PX_TRUE;
		}
		else
		{
			if(pSubLinkLayer!=PX_NULL)
			return PX_LiveFrameworkLinkLayerSearchSubLayer(plive,pSubLinkLayer,pSearchLayer);
		}
	}
	return PX_FALSE;
}

/** 建立图层父子链接关系 */
px_void PX_LiveFrameworkLinkLayer(PX_LiveFramework *plive,PX_LiveLayer *pLayer,PX_LiveLayer *linkLayer)
{
	px_int i;
	if (pLayer==linkLayer)
	{
		return;
	}

	if (PX_LiveFrameworkLinkLayerSearchSubLayer(plive,pLayer,pLayer))
	{
		return;
	}

	for (i=0;i<PX_LIVE_LAYER_MAX_LINK_NODE;i++)
	{
		PX_LiveLayer *pSubLinkLayer=PX_LiveFrameworkGetLayerChild(plive,pLayer,pLayer->child_index[i]);

		if (!pSubLinkLayer)
		{
			break;
		}

		if (pSubLinkLayer==linkLayer)
		{
			return;
		}

	}
	if(i<PX_LIVE_LAYER_MAX_LINK_NODE)
	{
		if (linkLayer->parent_index==-1)
		{
			pLayer->child_index[i]=PX_LiveFrameworkGetLayerIndex(plive,linkLayer);
			linkLayer->parent_index=PX_LiveFrameworkGetLayerIndex(plive,pLayer);
		}
	}
}

/** 清除所有图层的父子链接关系 */
px_void PX_LiveFrameworkClearLinker(PX_LiveFramework *plive)
{
	px_int i;
	for (i=0;i<plive->layers.size;i++)
	{
		px_int j;
		PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
		for (j=0;j<PX_COUNTOF(pLayer->child_index);j++)
		{
			pLayer->child_index[j]=-1;
		}
		pLayer->parent_index=-1;
	}
}

/* ── 图层删除（按索引） ─────────────────────────── */

/** 删除指定索引的图层：同时更新所有图层中的父子关系索引 */
px_void PX_LiveFrameworkDeleteLayer(PX_LiveFramework *plive,px_int index)
{
	if (index>=0&&index<plive->layers.size)
	{
		px_int i,j;
		PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,index);
		for (i=0;i<plive->layers.size;i++)
		{
			PX_LiveLayer *pSearchLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);

			if (pSearchLayer->parent_index == index)
			{
				pSearchLayer->parent_index = -1;
			}
			else if (pSearchLayer->parent_index > index)
			{
				pSearchLayer->parent_index--;
			}

			for (j=0;j<PX_LIVE_LAYER_MAX_LINK_NODE;j++)
			{
				if (pSearchLayer->child_index[j]==index)
				{
					px_int k;
					for (k=j;k<PX_COUNTOF(pLayer->child_index);k++)
					{
						pSearchLayer->child_index[k]=pSearchLayer->child_index[k+1];
						if (pSearchLayer->child_index[k]==-1)
						{
							break;
						}
					}
				}
				
				if (pSearchLayer->child_index[j] > index)
				{
					pSearchLayer->child_index[j]--;
				}
			}
		}

		PX_VectorFree(&pLayer->vertices);
		PX_VectorFree(&pLayer->triangles);
		PX_VectorErase(&plive->layers,index);
	}
}

/** 删除指定索引的纹理：更新所有图层的纹理索引绑定 */
px_void PX_LiveFrameworkDeleteLiveTexture(PX_LiveFramework *plive,px_int index)
{
	if (index>=0&&index<plive->livetextures.size)
	{
		px_int i;
		PX_LiveTexture *pTexture=PX_VECTORAT(PX_LiveTexture,&plive->livetextures,index);
		for (i=0;i<plive->layers.size;i++)
		{
			PX_LiveLayer *player=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
			if (player->LinkTextureIndex>=index)
			{
				if (player->LinkTextureIndex>index)
				{
					player->LinkTextureIndex--;
					player->RenderTextureIndex=player->LinkTextureIndex;
				}
				else
				{
					player->LinkTextureIndex=-1;
					player->RenderTextureIndex=player->LinkTextureIndex;
				}
				
			}
		}
		PX_TextureFree(&pTexture->Texture);
		PX_VectorErase(&plive->livetextures,index);
	}
}

/** 删除指定动画的某一帧（释放帧内存） */
px_void PX_LiveFrameworkDeleteLiveAnimationFrame(PX_LiveFramework *plive,PX_LiveAnimation *pliveAnimation,px_int index)
{
	if (index>=0&&index<pliveAnimation->framesMemPtr.size)
	{
		px_void *pFrameMemories=*PX_VECTORAT(px_void*,&pliveAnimation->framesMemPtr,index);
		if(pFrameMemories && !plive->sharesImmutablePayloads)
			MP_Free(plive->mp,pFrameMemories);
		PX_VectorErase(&pliveAnimation->framesMemPtr,index);
	}
}

/** 删除指定索引的动画：释放所有帧内存 */
px_void PX_LiveFrameworkDeleteLiveAnimation(PX_LiveFramework *plive,px_int index)
{
	if (index>=0&&index<plive->liveAnimations.size)
	{
		PX_LiveAnimation *pAnimation=PX_VECTORAT(PX_LiveAnimation,&plive->liveAnimations,index);
		while (pAnimation->framesMemPtr.size)
		{
			PX_LiveFrameworkDeleteLiveAnimationFrame(plive,pAnimation,0);
		}
		PX_VectorFree(&pAnimation->framesMemPtr);
		PX_VectorErase(&plive->liveAnimations,index);

		plive->currentEditAnimationIndex=-1;
		plive->currentEditFrameIndex=-1;
		plive->currentEditLayerIndex=-1;
		plive->currentEditVertexIndex=1;
	}
}

/** 通过动画索引和帧索引删除指定帧 */
px_void PX_LiveFrameworkDeleteLiveAnimationFrameByIndex(PX_LiveFramework *plive,px_int AnimationIndex,px_int frameIndex)
{
	if (AnimationIndex>=0&&AnimationIndex<plive->liveAnimations.size)
	{
		PX_LiveAnimation *pAnimation=PX_VECTORAT(PX_LiveAnimation,&plive->liveAnimations,AnimationIndex);
		if (frameIndex>=0&&frameIndex<pAnimation->framesMemPtr.size)
		{
			PX_LiveFrameworkDeleteLiveAnimationFrame(plive,pAnimation,frameIndex);
		}
		PX_VectorErase(&pAnimation->framesMemPtr,frameIndex);
	}
}

/* ── 框架资源释放 ──────────────────────────────── */

/** 释放框架所有资源（图层、纹理、动画及其帧数据） */
px_void PX_LiveFrameworkFree(PX_LiveFramework *plive)
{
	PX_LiveRealtimeFree(&plive->realtime);
	while (plive->liveAnimations.size)
	{
		PX_LiveFrameworkDeleteLiveAnimation(plive,0);
	}
	while (plive->layers.size)
	{
		PX_LiveFrameworkDeleteLayer(plive,0);
	}
	while(plive->livetextures.size)
	{
		PX_LiveFrameworkDeleteLiveTexture(plive,0);
	}
	PX_VectorFree(&plive->liveAnimations);
	PX_VectorFree(&plive->layers);
	PX_VectorFree(&plive->livetextures);
}

/* ── 编辑器函数 ────────────────────────────────── */

/** 获取当前编辑中的动画帧数据指针 */
px_void * PX_LiveFrameworkGetCurrentEditFrame(PX_LiveFramework *plive)
{
	if (plive->currentEditAnimationIndex>=0&&plive->currentEditAnimationIndex<plive->liveAnimations.size)
	{
		PX_LiveAnimation *pAnimation=PX_VECTORAT(PX_LiveAnimation,&plive->liveAnimations,plive->currentEditAnimationIndex);
		if (plive->currentEditFrameIndex>=0&&plive->currentEditFrameIndex<pAnimation->framesMemPtr.size)
		{
			return *PX_VECTORAT(px_void *,&pAnimation->framesMemPtr,plive->currentEditFrameIndex);
		}
	}
	return PX_NULL;
}

/** 获取当前编辑的图层对象 */
PX_LiveLayer * PX_LiveFrameworkGetCurrentEditLiveLayer(PX_LiveFramework *plive)
{
	if (plive->currentEditLayerIndex>=0&&plive->currentEditLayerIndex<plive->layers.size)
	{
		return PX_VECTORAT(PX_LiveLayer,&plive->layers,plive->currentEditLayerIndex);
	}
	return PX_NULL;
}

/** 获取当前编辑帧中指定荷载索引的荷载数据 */
PX_LiveAnimationFramePayload *PX_LiveFrameworkGetCurrentEditAnimationFramePayloadIndex(PX_LiveFramework *plive,px_int payloadIndex)
{
	px_int boffset=0;
	px_int FramePayloadSize;
	px_byte *pOffset;
	PX_LiveAnimationFrameHeader *pHeader;
	pOffset=(px_byte *)PX_LiveFrameworkGetCurrentEditFrame(plive);
	if (!pOffset)
	{
		return PX_NULL;
	}

	pHeader=(PX_LiveAnimationFrameHeader *)pOffset;
	FramePayloadSize=pHeader->size+sizeof(PX_LiveAnimationFrameHeader);
	boffset+=sizeof(PX_LiveAnimationFrameHeader);
	while (PX_TRUE)
	{
		PX_LiveAnimationFramePayload *pPayloadHeder;
		if (boffset>FramePayloadSize)
		{
			break;
		}
		pPayloadHeder=(PX_LiveAnimationFramePayload *)(pOffset+boffset);
		if (payloadIndex<=0)
		{
			return pPayloadHeder;
		}
		boffset+=sizeof(PX_LiveAnimationFramePayload);
		boffset+=pPayloadHeder->translationVerticesCount*sizeof(px_point);
		payloadIndex--;
	}
	return PX_NULL;
}

/** 获取当前编辑帧的荷载（基于当前编辑的图层索引） */
PX_LiveAnimationFramePayload * PX_LiveFrameworkGetCurrentEditAnimationFramePayload(PX_LiveFramework *plive)
{
	return PX_LiveFrameworkGetCurrentEditAnimationFramePayloadIndex(plive,plive->currentEditLayerIndex);
}

/** 将当前编辑帧在动画序列中下移一位 */
px_void PX_LiveFrameworkCurrentEditMoveFrameDown(PX_LiveFramework *plive)
{
	PX_LiveAnimation *pAnimation=PX_LiveFrameworkGetCurrentEditAnimation(plive);
	if (!pAnimation)
	{
		return;
	}
	if (plive->currentEditFrameIndex>=0&&plive->currentEditFrameIndex<pAnimation->framesMemPtr.size)
	{
		if (plive->currentEditFrameIndex==pAnimation->framesMemPtr.size-1)
		{
			return;
		}
		else
		{
			px_void **ptr1,**ptr2,*temp;
			ptr1=PX_VECTORAT(px_void *,&pAnimation->framesMemPtr,plive->currentEditFrameIndex);
			ptr2=PX_VECTORAT(px_void *,&pAnimation->framesMemPtr,plive->currentEditFrameIndex+1);
			temp=*ptr1;
			*ptr1=*ptr2;
			*ptr2=temp;
		}
	}
}

/** 将当前编辑帧在动画序列中上移一位 */
px_void PX_LiveFrameworkCurrentEditMoveFrameUp(PX_LiveFramework *plive)
{
	PX_LiveAnimation *pAnimation=PX_LiveFrameworkGetCurrentEditAnimation(plive);
	if (!pAnimation)
	{
		return;
	}
	if (plive->currentEditFrameIndex>=0&&plive->currentEditFrameIndex<pAnimation->framesMemPtr.size)
	{
		if (plive->currentEditFrameIndex==0)
		{
			return;
		}
		else
		{
			px_void **ptr1,**ptr2,*temp;
			ptr1=PX_VECTORAT(px_void *,&pAnimation->framesMemPtr,plive->currentEditFrameIndex);
			ptr2=PX_VECTORAT(px_void *,&pAnimation->framesMemPtr,plive->currentEditFrameIndex-1);
			temp=*ptr1;
			*ptr1=*ptr2;
			*ptr2=temp;
		}
	}
}

/** 获取当前编辑的动画对象 */
PX_LiveAnimation * PX_LiveFrameworkGetCurrentEditAnimation(PX_LiveFramework *plive)
{
	if (plive->currentEditAnimationIndex>=0&&plive->currentEditAnimationIndex<plive->liveAnimations.size)
	{
		PX_LiveAnimation *pAnimation=PX_VECTORAT(PX_LiveAnimation,&plive->liveAnimations,plive->currentEditAnimationIndex);
		return pAnimation;
	}
	return PX_NULL;
}

/** 获取当前编辑帧的帧头部信息 */
PX_LiveAnimationFrameHeader * PX_LiveFrameworkGetCurrentEditAnimationFrame(PX_LiveFramework *plive)
{
	PX_LiveAnimation *pAnimation=PX_LiveFrameworkGetCurrentEditAnimation(plive);
	if (pAnimation)
	{
		if (plive->currentEditFrameIndex>=0&&plive->currentEditFrameIndex<pAnimation->framesMemPtr.size)
		{
			return (PX_LiveAnimationFrameHeader *)(*PX_VECTORAT(px_void*,&pAnimation->framesMemPtr,plive->currentEditFrameIndex));
		}
	}
	return PX_NULL;
}

/** 获取当前编辑中的顶点对象 */
PX_LiveVertex * PX_LiveFrameworkGetCurrentEditLiveVertex(PX_LiveFramework *plive)
{
	PX_LiveLayer *currentEditLayer=PX_LiveFrameworkGetCurrentEditLiveLayer(plive);
	if (currentEditLayer)
	{
		if (plive->currentEditVertexIndex>=0&&plive->currentEditVertexIndex<currentEditLayer->vertices.size)
		{
			return PX_VECTORAT(PX_LiveVertex,&currentEditLayer->vertices,plive->currentEditVertexIndex);
		}
	}
	return PX_NULL;
}

/** 获取当前编辑帧荷载中指定顶点的位移数据 */
px_point * PX_LiveFrameworkGetCurrentEditAnimationFramePayloadVertex(PX_LiveFramework *plive)
{
	px_byte *pPayloadByte=(px_byte *)PX_LiveFrameworkGetCurrentEditAnimationFramePayload(plive);
	PX_LiveAnimationFramePayload *ppayload=(PX_LiveAnimationFramePayload *)pPayloadByte;
	if (pPayloadByte)
	{
		if (plive->currentEditVertexIndex>=0&&plive->currentEditVertexIndex<(px_int)ppayload->translationVerticesCount)
		{
			return (px_point *)(pPayloadByte+sizeof(PX_LiveAnimationFramePayload)+plive->currentEditVertexIndex*sizeof(px_point));
		}
	}
	return PX_NULL;
}

/** 删除当前编辑中的动画 */
px_void PX_LiveFrameworkDeleteCurrentEditAnimation(PX_LiveFramework *plive)
{
	PX_LiveFrameworkDeleteLiveAnimation(plive,plive->currentEditAnimationIndex);
}

/** 删除当前编辑中的动画帧 */
px_void PX_LiveFrameworkDeleteCurrentEditAnimationFrame(PX_LiveFramework *plive)
{
	PX_LiveAnimation *pAnimation=PX_LiveFrameworkGetCurrentEditAnimation(plive);
	if (pAnimation)
	{
		PX_LiveFrameworkDeleteLiveAnimationFrame(plive,pAnimation,plive->currentEditFrameIndex);
	}
	
}

/** 创建新编辑帧：可选择拷贝当前帧或创建空白帧 */
px_bool PX_LiveFrameworkNewEditFrame(PX_LiveFramework *plive,px_char id[],px_bool bCopyFrame)
{
	PX_LiveAnimation *pAnimation;
	px_void *pFramebuffer;
	pAnimation=PX_LiveFrameworkGetCurrentEditAnimation(plive);
	if (!pAnimation)
	{
		return PX_FALSE;
	}
	pFramebuffer=PX_LiveFrameworkGetCurrentEditAnimationFrame(plive);

	if (pFramebuffer)
	{
		px_uint size;
		PX_LiveAnimationFrameHeader *pHeader;
		px_void *newFramePtr;
		px_void *buffer=pFramebuffer;
		pHeader=(PX_LiveAnimationFrameHeader *)buffer;
		size=pHeader->size+sizeof(PX_LiveAnimationFrameHeader);
		newFramePtr=MP_Malloc(plive->mp,size);
		if (newFramePtr)
		{
			PX_memcpy(newFramePtr,buffer,size);
			PX_memcpy(pHeader->frameid,id,sizeof(pHeader->frameid));
			if (bCopyFrame)
			{
				PX_VectorPushTo(&pAnimation->framesMemPtr,&newFramePtr,plive->currentEditFrameIndex+1);
			}
			else
			{
				PX_VectorPushback(&pAnimation->framesMemPtr,&newFramePtr);
			}
			
			return PX_TRUE;
		}
		else
		{
			return PX_FALSE;
		}
	}
	else
	{
		px_void *newFramePtr;
		//create new frame
		px_int i;
		px_uint size=0;
		//header
		size+=sizeof(PX_LiveAnimationFrameHeader);
		for (i=0;i<plive->layers.size;i++)
		{
			PX_LiveLayer *player=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
			//payload
			size+=sizeof(PX_LiveAnimationFramePayload);
			//translation
			size+=sizeof(px_point)*player->vertices.size;
		}
		newFramePtr=MP_Malloc(plive->mp,size);

		if (newFramePtr)
		{
			PX_LiveAnimationFrameHeader *pHeader=(PX_LiveAnimationFrameHeader *)newFramePtr;
			PX_LiveAnimationFramePayload *pPayload;
			px_byte *pdata;
			px_int offset=0;
			PX_memset(newFramePtr,0,size);
			pdata=(px_byte *)newFramePtr;
			pHeader->size=size-sizeof(PX_LiveAnimationFrameHeader);
			offset+=sizeof(PX_LiveAnimationFrameHeader);
			for (i=0;i<plive->layers.size;i++)
			{
				PX_LiveLayer *player=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
				pPayload=(PX_LiveAnimationFramePayload *)(pdata+offset);
				pPayload->translationVerticesCount=player->vertices.size;
				pPayload->mapTexture=player->LinkTextureIndex;
				pPayload->stretch=1;
				offset+=sizeof(PX_LiveAnimationFramePayload)+sizeof(px_point)*player->vertices.size;
			}
			PX_memcpy(pHeader->frameid,id,sizeof(pHeader->frameid));
			return PX_VectorPushback(&pAnimation->framesMemPtr,&newFramePtr);
		}
		else
		{
			return PX_FALSE;
		}
	}


	
	
	return PX_FALSE;
}

/** 立即执行当前编辑帧的变换指令（用于编辑器预览） */
px_void PX_LiveFrameworkRunCurrentEditFrame(PX_LiveFramework *plive)
{
	PX_LiveFrameworkExecuteInstr(plive,plive->currentEditAnimationIndex,plive->currentEditFrameIndex);
	PX_LiveFrameworkExecuteInstr(plive,plive->currentEditAnimationIndex,plive->currentEditFrameIndex);
}
/* ── RT30 实时轨迹导入 ──────────────────────────────── */

typedef struct
{
	px_char id[PX_LIVE_REALTIME_AXIS_ID_MAX_LEN];
	px_uint32 idHash;
	px_byte middleKeyIndex;
	px_byte defaultSampleIndex;
	px_byte coordFractionBits;
	px_byte rotationFractionBits;
	px_byte stretchFractionBits;
	px_uint16 bindingCount;
	px_uint32 vertexIndexCount;
	px_uint32 sampleStride;
	px_uint32 sampleBytes;
	px_uint32 bindingsOffset;
	px_uint32 vertexIndicesOffset;
	px_uint32 samplesOffset;
}PX_LiveFrameworkRT30TrailerAxis;

static px_bool PX_LiveFrameworkRT30ReadAxis(const px_byte *data,px_uint32 size,
	px_uint32 axesOffset,px_uint32 index,PX_LiveFrameworkRT30TrailerAxis *axis)
{
	PX_LiveDeviceReader reader;
	px_uint32 relative,offset;
	px_byte reserved;
	if (!PX_LiveDeviceSafeMulU32(index,PX_LIVE_RT30_TRAILER_AXIS_ENTRY_SIZE,&relative)||
		!PX_LiveDeviceSafeAddU32(axesOffset,relative,&offset)||
		!PX_LiveDeviceRangeIsValid(size,offset,PX_LIVE_RT30_TRAILER_AXIS_ENTRY_SIZE)||
		!PX_LiveDeviceReaderInitialize(&reader,data,size)||
		!PX_LiveDeviceReaderSkip(&reader,offset)||
		!PX_LiveDeviceReaderReadBytes(&reader,axis->id,sizeof(axis->id))||
		!PX_LiveDeviceReaderReadU32LE(&reader,&axis->idHash)||
		!PX_LiveDeviceReaderReadU8(&reader,&axis->middleKeyIndex)||
		!PX_LiveDeviceReaderReadU8(&reader,&axis->defaultSampleIndex)||
		!PX_LiveDeviceReaderReadU8(&reader,&axis->coordFractionBits)||
		!PX_LiveDeviceReaderReadU8(&reader,&axis->rotationFractionBits)||
		!PX_LiveDeviceReaderReadU8(&reader,&axis->stretchFractionBits)||
		!PX_LiveDeviceReaderReadU8(&reader,&reserved)||
		!PX_LiveDeviceReaderReadU16LE(&reader,&axis->bindingCount)||
		!PX_LiveDeviceReaderReadU32LE(&reader,&axis->vertexIndexCount)||
		!PX_LiveDeviceReaderReadU32LE(&reader,&axis->sampleStride)||
		!PX_LiveDeviceReaderReadU32LE(&reader,&axis->sampleBytes)||
		!PX_LiveDeviceReaderReadU32LE(&reader,&axis->bindingsOffset)||
		!PX_LiveDeviceReaderReadU32LE(&reader,&axis->vertexIndicesOffset)||
		!PX_LiveDeviceReaderReadU32LE(&reader,&axis->samplesOffset))
	{
		return PX_FALSE;
	}
	return PX_TRUE;
}

static px_bool PX_LiveFrameworkValidateRealtimeTrailer(PX_LiveFramework *plive,
	const px_byte *data,px_uint32 size,px_uint16 axisCount,px_uint32 axesOffset)
{
	px_uint32 tableBytes,dataOffset;
	px_int i;
	px_byte commonCoord=0,commonRotation=0,commonStretch=0;
	if (!PX_LiveDeviceSafeMulU32(axisCount,PX_LIVE_RT30_TRAILER_AXIS_ENTRY_SIZE,&tableBytes)||
		!PX_LiveDeviceSafeAddU32(axesOffset,tableBytes,&dataOffset)||
		!PX_LiveDeviceSafeAlignU32(dataOffset,4,&dataOffset))
	{
		return PX_FALSE;
	}
	for (i=0;i<axisCount;i++)
	{
		PX_LiveFrameworkRT30TrailerAxis axis,previous;
		PX_LiveDeviceReader reader;
		px_uint32 bindingBytes,indexBytes,sampleBytes,nextOffset,staticBytes;
		px_uint32 expectedSampleOffset=0,expectedVertexOffset=0;
		px_int j,idLength=0;
		if (!PX_LiveFrameworkRT30ReadAxis(data,size,axesOffset,i,&axis)) return PX_FALSE;
		while (idLength<PX_LIVE_REALTIME_AXIS_ID_MAX_LEN&&axis.id[idLength]) idLength++;
		if (idLength==0||idLength>=PX_LIVE_REALTIME_AXIS_ID_MAX_LEN||
			PX_LiveRealtimeHashId(axis.id)!=axis.idHash||
			axis.middleKeyIndex<1||axis.middleKeyIndex>28||
			(axis.defaultSampleIndex!=0&&axis.defaultSampleIndex!=axis.middleKeyIndex&&axis.defaultSampleIndex!=29)||
			axis.coordFractionBits>PX_LIVE_DEVICE_MAX_FRACTION_BITS||
			axis.rotationFractionBits>PX_LIVE_DEVICE_MAX_FRACTION_BITS||
			axis.stretchFractionBits>PX_LIVE_DEVICE_MAX_FRACTION_BITS||
			!axis.bindingCount||axis.bindingCount>PX_LIVE_DEVICE_MAX_BINDINGS_PER_AXIS||
			!axis.sampleStride||
			!PX_LiveDeviceSafeMulU32(axis.sampleStride,PX_LIVE_REALTIME_SAMPLE_COUNT,&sampleBytes)||
			sampleBytes!=axis.sampleBytes||
			!PX_LiveDeviceSafeMulU32(axis.bindingCount,PX_LIVE_RT30_TRAILER_BINDING_ENTRY_SIZE,&bindingBytes)||
			!PX_LiveDeviceSafeMulU32(axis.vertexIndexCount,2,&indexBytes)||
			!PX_LiveDeviceSafeAddU32((px_uint32)axis.bindingCount*(px_uint32)sizeof(PX_LiveRealtimeBinding),
				indexBytes,&staticBytes)||
			!PX_LiveDeviceSafeAddU32(staticBytes,axis.sampleBytes,&staticBytes)||
			staticBytes>PX_LIVE_REALTIME_STATIC_BUDGET_BYTES||
			axis.bindingsOffset!=dataOffset||
			!PX_LiveDeviceSafeAddU32(axis.bindingsOffset,bindingBytes,&dataOffset)||
			!PX_LiveDeviceSafeAlignU32(dataOffset,4,&dataOffset)||
			axis.vertexIndicesOffset!=dataOffset||
			!PX_LiveDeviceSafeAddU32(axis.vertexIndicesOffset,indexBytes,&dataOffset)||
			!PX_LiveDeviceSafeAlignU32(dataOffset,4,&dataOffset)||
			axis.samplesOffset!=dataOffset||
			!PX_LiveDeviceSafeAddU32(axis.samplesOffset,axis.sampleBytes,&dataOffset)||
			!PX_LiveDeviceSafeAlignU32(dataOffset,4,&nextOffset)||dataOffset>size)
		{
			return PX_FALSE;
		}
		dataOffset=nextOffset;
		if (i==0)
		{
			commonCoord=axis.coordFractionBits;
			commonRotation=axis.rotationFractionBits;
			commonStretch=axis.stretchFractionBits;
		}
		else if (axis.coordFractionBits!=commonCoord||axis.rotationFractionBits!=commonRotation||
			axis.stretchFractionBits!=commonStretch)
		{
			return PX_FALSE;
		}
		for (j=0;j<i;j++)
		{
			if (!PX_LiveFrameworkRT30ReadAxis(data,size,axesOffset,j,&previous)||
				previous.idHash==axis.idHash) return PX_FALSE;
		}
		PX_LiveDeviceReaderInitialize(&reader,data,size);
		if (!PX_LiveDeviceReaderSkip(&reader,axis.bindingsOffset)) return PX_FALSE;
		for (j=0;j<axis.bindingCount;j++)
		{
			px_uint16 layerIndex,propertyMask,vertexCount,reserved16,vertexIndex;
			px_uint32 vertexOffset,sampleOffset,propertyBytes=0,k,indexByteOffset;
			PX_LiveLayer *layer;
			if (!PX_LiveDeviceReaderReadU16LE(&reader,&layerIndex)||
				!PX_LiveDeviceReaderReadU16LE(&reader,&propertyMask)||
				!PX_LiveDeviceReaderReadU16LE(&reader,&vertexCount)||
				!PX_LiveDeviceReaderReadU16LE(&reader,&reserved16)||
				!PX_LiveDeviceReaderReadU32LE(&reader,&vertexOffset)||
				!PX_LiveDeviceReaderReadU32LE(&reader,&sampleOffset)||
				layerIndex>=(px_uint16)plive->layers.size||!propertyMask||
				(propertyMask&~PX_LIVE_REALTIME_PROPERTY_ALL)||
				vertexOffset!=expectedVertexOffset||sampleOffset!=expectedSampleOffset)
			{
				return PX_FALSE;
			}
			layer=PX_VECTORAT(PX_LiveLayer,&plive->layers,layerIndex);
			if (propertyMask&PX_LIVE_REALTIME_PROPERTY_TRANSLATION) propertyBytes+=4;
			if (propertyMask&PX_LIVE_REALTIME_PROPERTY_ROTATION) propertyBytes+=2;
			if (propertyMask&PX_LIVE_REALTIME_PROPERTY_STRETCH) propertyBytes+=2;
			if (propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_TRANSLATION) propertyBytes+=4;
			if (propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_ROTATION) propertyBytes+=2;
			if (propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_SCALE) propertyBytes+=2;
			if (propertyMask&PX_LIVE_REALTIME_PROPERTY_TEXTURE) propertyBytes+=2;
			if (propertyMask&PX_LIVE_REALTIME_PROPERTY_IMPULSE) propertyBytes+=4;
			if (propertyMask&PX_LIVE_REALTIME_PROPERTY_VERTICES)
			{
				if (expectedVertexOffset>axis.vertexIndexCount||
					vertexCount>axis.vertexIndexCount-expectedVertexOffset) return PX_FALSE;
				for (k=0;k<vertexCount;k++)
				{
					PX_LiveDeviceReader indexReader;
					if (!PX_LiveDeviceSafeMulU32(expectedVertexOffset+k,2,&indexByteOffset)||
						!PX_LiveDeviceSafeAddU32(axis.vertexIndicesOffset,indexByteOffset,&indexByteOffset)||
						!PX_LiveDeviceReaderInitialize(&indexReader,data,size)||
						!PX_LiveDeviceReaderSkip(&indexReader,indexByteOffset)||
						!PX_LiveDeviceReaderReadU16LE(&indexReader,&vertexIndex)||
						vertexIndex>=(px_uint16)layer->vertices.size) return PX_FALSE;
				}
				propertyBytes+=(px_uint32)vertexCount*4u;
				expectedVertexOffset+=vertexCount;
			}
			else if (vertexCount) return PX_FALSE;
			if (!PX_LiveDeviceSafeAddU32(expectedSampleOffset,propertyBytes,&expectedSampleOffset)||
				expectedSampleOffset>axis.sampleStride) return PX_FALSE;
		}
		if (expectedVertexOffset!=axis.vertexIndexCount||
			!PX_LiveDeviceSafeAlignU32(expectedSampleOffset,4,&expectedSampleOffset)||
			expectedSampleOffset!=axis.sampleStride) return PX_FALSE;
	}
	return dataOffset==size;
}

static px_void PX_LiveFrameworkFreeTemporaryRealtimeAxis(px_memorypool *mp,
	PX_LiveRealtimeAxis *axis)
{
	if (axis->bindings) MP_Free(mp,axis->bindings);
	if (axis->vertexIndices) MP_Free(mp,axis->vertexIndices);
	if (axis->samples) MP_Free(mp,axis->samples);
	PX_memset(axis,0,sizeof(*axis));
}

static px_bool PX_LiveFrameworkImportRealtimeTrailer(px_memorypool *mp,
	PX_LiveFramework *plive,const px_byte *data,px_uint32 size)
{
	PX_LiveDeviceReader reader;
	px_uint32 magic,chunkSize,axesOffset,reserved32;
	px_uint16 version,headerSize,axisCount,axisEntrySize;
	px_int i;
	if (size<4) return PX_TRUE;
	PX_LiveDeviceReaderInitialize(&reader,data,size);
	if (!PX_LiveDeviceReaderReadU32LE(&reader,&magic)) return PX_FALSE;
	if (magic!=PX_LIVE_RT30_TRAILER_MAGIC) return PX_TRUE;
	if (!PX_LiveDeviceReaderReadU16LE(&reader,&version)||
		!PX_LiveDeviceReaderReadU16LE(&reader,&headerSize)||
		!PX_LiveDeviceReaderReadU32LE(&reader,&chunkSize)||
		!PX_LiveDeviceReaderReadU16LE(&reader,&axisCount)||
		!PX_LiveDeviceReaderReadU16LE(&reader,&axisEntrySize)||
		!PX_LiveDeviceReaderReadU32LE(&reader,&axesOffset)||
		!PX_LiveDeviceReaderReadU32LE(&reader,&reserved32)||
		version!=PX_LIVE_RT30_TRAILER_VERSION||
		headerSize!=PX_LIVE_RT30_TRAILER_HEADER_SIZE||chunkSize!=size||
		!axisCount||axisCount>PX_LIVE_REALTIME_MAX_AXES||
		axisEntrySize!=PX_LIVE_RT30_TRAILER_AXIS_ENTRY_SIZE||
		axesOffset!=PX_LIVE_RT30_TRAILER_HEADER_SIZE||
		!PX_LiveFrameworkValidateRealtimeTrailer(plive,data,size,axisCount,axesOffset))
	{
		return PX_FALSE;
	}
	for (i=0;i<axisCount;i++)
	{
		PX_LiveFrameworkRT30TrailerAxis wire;
		PX_LiveRealtimeAxis source;
		px_int j;
		PX_memset(&source,0,sizeof(source));
		if (!PX_LiveFrameworkRT30ReadAxis(data,size,axesOffset,i,&wire)) return PX_FALSE;
		PX_memcpy(source.id,wire.id,sizeof(source.id));
		source.id[PX_LIVE_REALTIME_AXIS_ID_MAX_LEN-1]=0;
		source.idHash=wire.idHash;
		source.middleKeyIndex=wire.middleKeyIndex;
		source.defaultSampleIndex=wire.defaultSampleIndex;
		source.sampleIndex=wire.defaultSampleIndex;
		source.coordFractionBits=wire.coordFractionBits;
		source.rotationFractionBits=wire.rotationFractionBits;
		source.stretchFractionBits=wire.stretchFractionBits;
		source.valid=PX_TRUE;
		source.weightQ15=0;
		source.bindingCount=wire.bindingCount;
		source.vertexIndexCount=wire.vertexIndexCount;
		source.sampleStride=wire.sampleStride;
		source.sampleBytes=wire.sampleBytes;
		source.bindings=(PX_LiveRealtimeBinding *)MP_Malloc(mp,
			(px_uint)source.bindingCount*sizeof(PX_LiveRealtimeBinding));
		if (source.vertexIndexCount)
		{
			source.vertexIndices=(px_uint16 *)MP_Malloc(mp,
				source.vertexIndexCount*(px_uint)sizeof(px_uint16));
		}
		source.samples=(px_int16 *)MP_Malloc(mp,source.sampleBytes);
		if (!source.bindings||(source.vertexIndexCount&&!source.vertexIndices)||!source.samples)
		{
			PX_LiveFrameworkFreeTemporaryRealtimeAxis(mp,&source);
			return PX_FALSE;
		}
		PX_LiveDeviceReaderInitialize(&reader,data,size);
		PX_LiveDeviceReaderSkip(&reader,wire.bindingsOffset);
		for (j=0;j<source.bindingCount;j++)
		{
			px_uint16 reserved16;
			if (!PX_LiveDeviceReaderReadU16LE(&reader,&source.bindings[j].layerIndex)||
				!PX_LiveDeviceReaderReadU16LE(&reader,&source.bindings[j].propertyMask)||
				!PX_LiveDeviceReaderReadU16LE(&reader,&source.bindings[j].vertexCount)||
				!PX_LiveDeviceReaderReadU16LE(&reader,&reserved16)||
				!PX_LiveDeviceReaderReadU32LE(&reader,&source.bindings[j].vertexIndexOffset)||
				!PX_LiveDeviceReaderReadU32LE(&reader,&source.bindings[j].sampleOffset))
			{
				PX_LiveFrameworkFreeTemporaryRealtimeAxis(mp,&source);
				return PX_FALSE;
			}
		}
		PX_LiveDeviceReaderInitialize(&reader,data,size);
		PX_LiveDeviceReaderSkip(&reader,wire.vertexIndicesOffset);
		for (j=0;j<(px_int)source.vertexIndexCount;j++)
		{
			if (!PX_LiveDeviceReaderReadU16LE(&reader,&source.vertexIndices[j]))
			{
				PX_LiveFrameworkFreeTemporaryRealtimeAxis(mp,&source);
				return PX_FALSE;
			}
		}
		PX_LiveDeviceReaderInitialize(&reader,data,size);
		PX_LiveDeviceReaderSkip(&reader,wire.samplesOffset);
		for (j=0;j<(px_int)(source.sampleBytes/2);j++)
		{
			if (!PX_LiveDeviceReaderReadI16LE(&reader,&source.samples[j]))
			{
				PX_LiveFrameworkFreeTemporaryRealtimeAxis(mp,&source);
				return PX_FALSE;
			}
		}
		if (!PX_LiveRealtimeInstallBakedAxis(plive,PX_LIVE_REALTIME_INVALID_HANDLE,
			&source,PX_NULL))
		{
			PX_LiveFrameworkFreeTemporaryRealtimeAxis(mp,&source);
			return PX_FALSE;
		}
		PX_LiveFrameworkFreeTemporaryRealtimeAxis(mp,&source);
	}
	return PX_TRUE;
}
/* ── 导出功能 ──────────────────────────────────── */

/** 将 Live2D 框架数据导出为二进制格式：文件头 -> 基础属性 -> 纹理 -> 图层 -> 动画 */
px_bool PX_LiveFrameworkExport(PX_LiveFramework *plive,px_memory *exportbuffer)
{
	/* ── 写入文件头 ────────────────────── */
	if(!PX_MemoryCat(exportbuffer,"PainterEngineLiveDBinary",24))return PX_FALSE;

	/* ── 导出基本属性 ──────────────────── */
	do 
	{

		typedef struct  
		{
			px_char id[PX_LIVE_ID_MAX_LEN];
			px_dword version;
			px_int32 width;
			px_int32 height;
			px_int32 layerCount;
			px_int32 animationCount;
			px_int32 textureCount;
		}PX_LiveFrameworkBaseAttributes;

		PX_LiveFrameworkBaseAttributes desc;

		PX_memset(&desc,0,sizeof(PX_LiveFrameworkBaseAttributes));
		
		PX_memcpy(desc.id,plive->id,PX_LIVE_ID_MAX_LEN);
		desc.version = PX_LIVE_VERSION;

		desc.width=plive->width;
		
		desc.height=plive->height;

		desc.layerCount=plive->layers.size;

		desc.animationCount=plive->liveAnimations.size;

		desc.textureCount=plive->livetextures.size;
		
		if(!PX_MemoryCat(exportbuffer,&desc,sizeof(desc)))return PX_FALSE;
	} while (0);

	/* ── 导出纹理数据 ──────────────────── */
	do 
	{
		typedef struct  
		{
			px_char id[PX_LIVE_ID_MAX_LEN];
			px_int32 width;
			px_int32 height;
			px_int32 textureOffsetX;
			px_int32 textureOffsetY;
		}PX_LiveTextureExportInfo;

		px_int i;
		for (i=0;i<plive->livetextures.size;i++)
		{
			PX_LiveTexture *pTexture=PX_VECTORAT(PX_LiveTexture,&plive->livetextures,i);
			
			//export live texture structure
			do 
			{
				PX_LiveTextureExportInfo desc;
				desc.height=pTexture->Texture.height;
				desc.width=pTexture->Texture.width;
				PX_memcpy(desc.id,pTexture->id,sizeof(desc.id));
				desc.textureOffsetX=pTexture->textureOffsetX;
				desc.textureOffsetY=pTexture->textureOffsetY;
				if(!PX_MemoryCat(exportbuffer,&desc,sizeof(desc)))return PX_FALSE;
			} while (0);
			
			//export pixels data
			do 
			{
				px_int k;
				px_liveframework_rgba_color renderColor;
				px_color *pColor =(px_color*)(pTexture->Texture.surfaceBuffer);
				for (k = 0; k < pTexture->Texture.width * pTexture->Texture.height; k++)
				{
					renderColor._argb.r = pColor[k]._argb.r;
					renderColor._argb.g = pColor[k]._argb.g;
					renderColor._argb.b = pColor[k]._argb.b;
					renderColor._argb.a = pColor[k]._argb.a;
					if (!PX_MemoryCat(exportbuffer, &renderColor, sizeof(px_liveframework_rgba_color)))return PX_FALSE;
				}
				//if(!PX_MemoryCat(exportbuffer,pTexture->Texture.surfaceBuffer,sizeof(px_color)*pTexture->Texture.height*pTexture->Texture.width))return PX_FALSE;
			} while (0);
		}
	} while (0);

	/* ── 导出图层数据 ──────────────────── */
	do 
	{
		typedef struct  
		{
			px_char id[PX_LIVE_ID_MAX_LEN];
			px_int32 parent_index;
			px_int32 child_index[PX_LIVE_LAYER_MAX_LINK_NODE];
			px_int32 triangleCount;
			px_int32 verticesCount;
			px_point32 KeyPoint;
			px_int32  LinkTextureIndex;
		}PX_LiveFramework_LayerExportInfo;
		px_int i;
		for (i=0;i<plive->layers.size;i++)
		{
			PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
			PX_LiveFramework_LayerExportInfo desc;
			PX_memcpy(desc.id,pLayer->id,sizeof(desc.id));

			desc.triangleCount=pLayer->triangles.size;
			desc.verticesCount=pLayer->vertices.size;

			desc.parent_index=pLayer->parent_index;

			do 
			{
				px_int j;
				for (j=0;j<PX_COUNTOF(pLayer->child_index);j++)
				{
					desc.child_index[j]=pLayer->child_index[j];
				}
			} while (0);

			desc.KeyPoint=pLayer->keyPoint;
			desc.LinkTextureIndex=pLayer->LinkTextureIndex;

			if(!PX_MemoryCat(exportbuffer,&desc,sizeof(desc)))return PX_FALSE;
			
			
			//export layer triangles
			do 
			{
				if(!PX_MemoryCat(exportbuffer,pLayer->triangles.data,pLayer->triangles.nodesize*pLayer->triangles.size))return PX_FALSE;
			} while (0);

			//export layer vertex
			do 
			{
				if(!PX_MemoryCat(exportbuffer,pLayer->vertices.data,pLayer->vertices.nodesize*pLayer->vertices.size))return PX_FALSE;
			} while (0);
		}
	} while (0);

	/* ── 导出动画数据 ──────────────────── */
	do 
	{
		px_int i;
		for (i=0;i<plive->liveAnimations.size;i++)
		{
			PX_LiveAnimation *pAnimation=PX_VECTORAT(PX_LiveAnimation,&plive->liveAnimations,i);
			//export Live Animation structure
			do 
			{
				typedef struct  
				{
					px_char id[PX_LIVE_ID_MAX_LEN];
					px_int32  size;
				}PX_LiveAnimationExportInfo;

				PX_LiveAnimationExportInfo desc;
				PX_memcpy(desc.id,pAnimation->id,sizeof(desc.id));
				desc.size=pAnimation->framesMemPtr.size;
				if(!PX_MemoryCat(exportbuffer,&desc,sizeof(desc)))return PX_FALSE;
			} while (0);
			
			//export live animation frame
			do 
			{
				px_int j;
				for (j=0;j<pAnimation->framesMemPtr.size;j++)
				{
					//export size
					px_void *pdata;
					px_int32 payloadsize;
					PX_LiveAnimationFrameHeader *pheader;
					pdata=*PX_VECTORAT(px_void*,&pAnimation->framesMemPtr,j);
					pheader=(PX_LiveAnimationFrameHeader *)pdata;
					payloadsize=sizeof(PX_LiveAnimationFrameHeader)+pheader->size;

					if(!PX_MemoryCat(exportbuffer,pdata,payloadsize))return PX_FALSE;
				}
			} while (0);


		}
	} while (0);
	return PX_TRUE;
}

/* ── 导入功能 ──────────────────────────────────── */

/** 从二进制缓冲区导入 Live2D 框架数据 */
static px_bool l2d_wire_fits(px_int offset, px_int size, px_uint32 nbytes)
{
	if (offset < 0 || size < 0) return PX_FALSE;
	if ((px_uint32)offset > (px_uint32)size) return PX_FALSE;
	if (nbytes > (px_uint32)size - (px_uint32)offset) return PX_FALSE;
	return PX_TRUE;
}

static px_bool l2d_wire_mul(px_int count, px_uint32 elem, px_uint32 *out_bytes)
{
	if (count < 0 || !out_bytes) return PX_FALSE;
	if (elem != 0 && (px_uint32)count > 0xffffffffu / elem) return PX_FALSE;
	*out_bytes = (px_uint32)count * elem;
	return PX_TRUE;
}

px_bool PX_LiveFrameworkImport(px_memorypool *mp,PX_LiveFramework *plive,px_void *buffer,px_int size)
{
	px_byte*bBuffer = (px_byte *)buffer;
	px_int rOffset=0;
	/* ── 验证文件头 ────────────────────── */
	
	do 
	{
		if (!buffer || !l2d_wire_fits(0, size, 24) ||
			!PX_memequ(buffer,"PainterEngineLiveDBinary",24))
		{
			return PX_FALSE;
		}
		rOffset=24;
		
	} while (0);
	
	/* ── 导入框架基本属性 ────────────────── */
	do 
	{
		typedef struct  
		{
			px_char id[PX_LIVE_ID_MAX_LEN];
			px_dword version;
			px_int32 width;
			px_int32 height;
			px_int32 layerCount;
			px_int32 animationCount;
			px_int32 textureCount;
		}PX_LiveFrameworkBaseAttributes;

		PX_LiveFrameworkBaseAttributes readAttr;
		if (!l2d_wire_fits(rOffset, size, sizeof(readAttr))) return PX_FALSE;
		PX_memcpy(&readAttr, bBuffer + rOffset, sizeof(readAttr));
		rOffset += (px_int)sizeof(readAttr);

		//////////////////////////////////////////////////////////////////////////
		if (readAttr.version != PX_LIVE_VERSION) return PX_FALSE;
		if (readAttr.width < 0 || readAttr.height < 0 ||
			readAttr.layerCount < 0 || readAttr.animationCount < 0 ||
			readAttr.textureCount < 0) return PX_FALSE;

		PX_memset(plive,0,sizeof(PX_LiveFramework));

		PX_memcpy(plive->id,readAttr.id,sizeof(plive->id));
		plive->width=readAttr.width;
		plive->height=readAttr.height;

		//////////////////////////////////////////////////////////////////////////
		plive->mp=mp;
		plive->animationMode=PX_LIVE_MODE_NEUTRAL;
		/* realtime 子结构被上面的 memset 清零（realtime.mp=NULL），必须在此重新初始化，
		 * 否则 RT30 尾部导入时 PX_LiveRealtimePrepareRuntime 用 NULL 池触发 Load access fault。 */
		PX_LiveRealtimeInitialize(&plive->realtime,mp);
		if(!PX_VectorInitialize(mp,&plive->layers,sizeof(PX_LiveLayer),readAttr.layerCount))return PX_FALSE;
		plive->layers.size=readAttr.layerCount;

		if(!PX_VectorInitialize(mp,&plive->livetextures,sizeof(PX_LiveTexture),readAttr.textureCount))return PX_FALSE;
		plive->livetextures.size=readAttr.textureCount;

		if(!PX_VectorInitialize(mp,&plive->liveAnimations,sizeof(PX_LiveAnimation),readAttr.animationCount))return PX_FALSE;
		plive->liveAnimations.size=readAttr.animationCount;

		plive->reg_animation=0;
		plive->reg_bp=0;
		plive->reg_duration=0;
		plive->reg_elapsed=0;
		plive->reg_ip=0;

		plive->currentEditAnimationIndex=-1;
		plive->currentEditFrameIndex=-1;
		plive->currentEditLayerIndex=-1;
		plive->currentEditVertexIndex=-1;
	} while (0);

	
	/////////////////////////////////////////////* ── 导入纹理数据 ──────────────────── */
	do 
	{
		typedef struct  
		{
			px_char id[PX_LIVE_ID_MAX_LEN];
			px_int32 width;
			px_int32 height;
			px_int32 textureOffsetX;
			px_int32 textureOffsetY;
		}PX_LiveTextureImportInfo;

		px_int i;
		for (i=0;i<plive->livetextures.size;i++)
		{
			PX_LiveTexture *pTexture=PX_VECTORAT(PX_LiveTexture,&plive->livetextures,i);
			do 
			{
				px_color *prenderColor;
				px_int k;
				px_uint32 pixel_bytes;
				PX_LiveTextureImportInfo texInfo;
				if (!l2d_wire_fits(rOffset, size, sizeof(texInfo))) goto _ERROR;
				PX_memcpy(&texInfo, bBuffer + rOffset, sizeof(texInfo));
				rOffset += (px_int)sizeof(texInfo);
				if (texInfo.width <= 0 || texInfo.height <= 0 ||
					!l2d_wire_mul(texInfo.width, 4u, &pixel_bytes) ||
					!l2d_wire_mul(texInfo.height, pixel_bytes, &pixel_bytes) ||
					!l2d_wire_fits(rOffset, size, pixel_bytes))
					goto _ERROR;
				PX_memset(pTexture,0,sizeof(PX_LiveTexture));

				if(!PX_TextureCreate(mp,&pTexture->Texture,texInfo.width,texInfo.height))
					goto _ERROR;
				PX_memcpy(pTexture->id,texInfo.id,sizeof(pTexture->id));
				pTexture->textureOffsetX=texInfo.textureOffsetX;
				pTexture->textureOffsetY=texInfo.textureOffsetY;

				prenderColor = pTexture->Texture.surfaceBuffer;
				for (k = 0; k < texInfo.width * texInfo.height; k++)
				{
					const px_byte *src = bBuffer + rOffset + (px_uint32)k * 4u;
					prenderColor[k]._argb.r=src[0];
					prenderColor[k]._argb.g=src[1];
					prenderColor[k]._argb.b=src[2];
					prenderColor[k]._argb.a=src[3];
				}
				rOffset += (px_int)pixel_bytes;
			} while (0);
		}
	} while (0);

	/* ── 导入图层数据 ──────────────────── */
	do 
	{
		typedef struct  
		{
			px_char id[PX_LIVE_ID_MAX_LEN];
			px_int parent_index;
			px_int child_index[PX_LIVE_LAYER_MAX_LINK_NODE];
			px_int triangleCount;
			px_int verticesCount;
			px_point KeyPoint;
			px_int  LinkTextureIndex;
		}PX_LiveFramework_LayerExportInfo;

		px_int i;
		for (i=0;i<plive->layers.size;i++)
		{
			PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
			PX_LiveFramework_LayerExportInfo readLayer;
			px_uint32 tri_bytes, vert_bytes;
			if (!l2d_wire_fits(rOffset, size, sizeof(readLayer))) goto _ERROR;
			PX_memcpy(&readLayer, bBuffer + rOffset, sizeof(readLayer));
			rOffset += (px_int)sizeof(readLayer);
			if (readLayer.parent_index < -1 || readLayer.parent_index >= plive->layers.size ||
				readLayer.triangleCount < 0 || readLayer.verticesCount < 0 ||
				!l2d_wire_mul(readLayer.triangleCount, sizeof(PX_Delaunay_Triangle), &tri_bytes) ||
				!l2d_wire_mul(readLayer.verticesCount, sizeof(PX_LiveVertex), &vert_bytes) ||
				!l2d_wire_fits(rOffset, size, tri_bytes) ||
				(px_uint32)rOffset > 0xffffffffu - tri_bytes ||
				!l2d_wire_fits((px_int)((px_uint32)rOffset + tri_bytes), size, vert_bytes))
				goto _ERROR;

			PX_memset(pLayer,0,sizeof(PX_LiveLayer));
			PX_memcpy(pLayer->id,readLayer.id,sizeof(pLayer->id));
			pLayer->keyPoint=readLayer.KeyPoint;
			pLayer->LinkTextureIndex=readLayer.LinkTextureIndex;
			pLayer->RenderTextureIndex=pLayer->LinkTextureIndex;

			pLayer->rel_beginStretch=1;
			pLayer->rel_currentStretch=1;
			pLayer->rel_endStretch=1;
			pLayer->visible=PX_TRUE;

			if(!PX_VectorInitialize(mp,&pLayer->triangles,sizeof(PX_Delaunay_Triangle),readLayer.triangleCount)) 
				goto _ERROR;
			pLayer->triangles.size=readLayer.triangleCount;

			if(!PX_VectorInitialize(mp,&pLayer->vertices,sizeof(PX_LiveVertex),readLayer.verticesCount)) 
				goto _ERROR;
			pLayer->vertices.size=readLayer.verticesCount;


			pLayer->parent_index=readLayer.parent_index;

			do 
			{
				px_int j;
				for (j=0;j<PX_COUNTOF(readLayer.child_index);j++)
				{
					pLayer->child_index[j]=readLayer.child_index[j];
				}
			} while (0);

			
			PX_memcpy(pLayer->triangles.data,bBuffer+rOffset,tri_bytes);
			rOffset += (px_int)tri_bytes;
			PX_memcpy(pLayer->vertices.data,bBuffer+rOffset,vert_bytes);
			rOffset += (px_int)vert_bytes;
		}
	} while (0);

	/* ── 导入动画数据 ──────────────────── */
	do 
	{
		typedef struct  
		{
			px_char id[PX_LIVE_ID_MAX_LEN];
			px_int32  size;
		}PX_LiveAnimationImportInfo;

		px_int i;
		for (i=0;i<plive->liveAnimations.size;i++)
		{
			PX_LiveAnimation *pAnimation=PX_VECTORAT(PX_LiveAnimation,&plive->liveAnimations,i);
			
			//import Live Animation structure
			do 
			{
				PX_LiveAnimationImportInfo animInfo;
				if (!l2d_wire_fits(rOffset, size, sizeof(animInfo))) goto _ERROR;
				PX_memcpy(&animInfo, bBuffer + rOffset, sizeof(animInfo));
				rOffset += (px_int)sizeof(animInfo);
				if (animInfo.size < 0) goto _ERROR;

				PX_memcpy(pAnimation->id,animInfo.id,sizeof(pAnimation->id));

				if(!PX_VectorInitialize(mp,&pAnimation->framesMemPtr,sizeof(px_void *),animInfo.size))goto _ERROR;
				pAnimation->framesMemPtr.size=animInfo.size;
			} while (0);

			//import live animation frame
			do 
			{
				px_int j;
				for (j=0;j<pAnimation->framesMemPtr.size;j++)
				{
					//import size
					px_void **ppdata;
					px_uint32 payloadsize;
					PX_LiveAnimationFrameHeader frameHeader;
					ppdata=PX_VECTORAT(px_void*,&pAnimation->framesMemPtr,j);
					if (!l2d_wire_fits(rOffset, size, sizeof(frameHeader))) goto _ERROR;
					PX_memcpy(&frameHeader, bBuffer + rOffset, sizeof(frameHeader));
					if (frameHeader.size < 0) goto _ERROR;
					payloadsize=(px_uint32)sizeof(PX_LiveAnimationFrameHeader)+(px_uint32)frameHeader.size;
					if (payloadsize < (px_uint32)sizeof(PX_LiveAnimationFrameHeader) ||
						!l2d_wire_fits(rOffset, size, payloadsize)) goto _ERROR;

					*ppdata=MP_Malloc(mp,(px_int)payloadsize);
					if(*ppdata==PX_NULL) goto _ERROR;
					PX_memcpy(*ppdata,bBuffer+rOffset,payloadsize);
					rOffset+=(px_int)payloadsize;
				}
			} while (0);
		}
	} while (0);
		if (rOffset<size && !PX_LiveFrameworkImportRealtimeTrailer(mp,plive,(const px_byte *)bBuffer+rOffset,(px_uint32)(size-rOffset))) goto _ERROR;
	PX_LiveFrameworkReset(plive);
	return PX_TRUE;
_ERROR:
	PX_LiveFrameworkFree(plive);
	return PX_FALSE;
}

/* ── PX_Live 镜像 API ───────────────────────────── */

/** 从现有框架创建 Live 实例：深拷贝图层和顶点数据 */
px_bool PX_LiveCreate(px_memorypool *mp,PX_LiveFramework *pLiveFramework,PX_Live *pLive)
{
	px_int i,initializedVertices=0;
	*pLive=*pLiveFramework;
	pLive->mp=mp;
	PX_LiveRealtimeInitialize(&pLive->realtime,mp);
	if(!PX_VectorInitialize(mp,&pLive->layers,sizeof(PX_LiveLayer),0))goto _ERROR;
	if(!PX_VectorCopy(&pLive->layers,&pLiveFramework->layers)) goto _ERROR;
	for (i=0;i<pLive->layers.size;i++)
	{
		PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&pLive->layers,i);
		PX_LiveLayer *pFrameworkLayer=PX_VECTORAT(PX_LiveLayer,&pLiveFramework->layers,i);
		PX_memset(&pLayer->vertices,0,sizeof(pLayer->vertices));
		if(!PX_VectorInitialize(mp,&pLayer->vertices,sizeof(PX_LiveVertex),0))
		{
			goto _ERROR;
		}
		initializedVertices++;
		if(!PX_VectorCopy(&pLayer->vertices,&pFrameworkLayer->vertices))
		{
			goto _ERROR;
		}
	}
	if (!PX_LiveRealtimeClone(&pLive->realtime,mp,&pLiveFramework->realtime))
	{
		goto _ERROR;
	}
	if (!PX_LiveRealtimePrepareRuntime(pLive))
	{
		goto _ERROR;
	}
	return PX_TRUE;
_ERROR:
	PX_LiveRealtimeFree(&pLive->realtime);
	for (i=0;i<initializedVertices;i++)
	{
		PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&pLive->layers,i);
		PX_VectorFree(&pLayer->vertices);
	}
	PX_VectorFree(&pLive->layers);
	PX_memset(pLive,0,sizeof(*pLive));
	pLive->mp=mp;
	return PX_FALSE;
	PX_LiveRealtimeFree(&pLive->realtime);
}

/** 释放 Live 实例的图层和顶点资源 */
px_void PX_LiveFree(PX_Live *pLive)
{
	px_int i;
	for (i=0;i<pLive->layers.size;i++)
	{
		PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&pLive->layers,i);
		PX_VectorFree(&pLayer->vertices);
	}
	PX_VectorFree(&pLive->layers);
}

/** 开始播放 */
px_void PX_LivePlay(PX_Live*plive)
{
	PX_LiveFrameworkPlay(plive);
}

/** 获取动画数量 */
px_int PX_LiveGetAnimationCount(PX_Live* plive)
{
	return plive->liveAnimations.size;
}

/** 通过索引播放动画 */
px_bool PX_LivePlayAnimation(PX_Live *plive,px_int index)
{
	return PX_LiveFrameworkPlayAnimation(plive,index);
}

/** 通过名称播放动画 */
px_bool PX_LivePlayAnimationByName(PX_Live *plive,const px_char name[])
{
	return PX_LiveFrameworkPlayAnimationByName(plive,name);
}

/** 暂停播放 */
px_void PX_LivePause(PX_Live *plive)
{
	PX_LiveFrameworkPause(plive);
}

/** 重置状态 */
px_void PX_LiveReset(PX_Live *plive)
{
	PX_LiveFrameworkReset(plive);
}

/** 停止播放 */
px_void PX_LiveStop(PX_Live *plive)
{
	PX_LiveFrameworkStop(plive);
}

/** 进入 RT30 实时模式 */
px_bool PX_LiveEnterRealtime30(PX_Live *plive)
{
	return PX_LiveRealtimeEnter(plive);
}

/** 离开 RT30 实时模式 */
px_void PX_LiveLeaveRealtime30(PX_Live *plive)
{
	PX_LiveRealtimeLeave(plive);
}

/** 设置 RT30 轴采样 */
px_bool PX_LiveSetRealtimeAxisSample(PX_Live *plive,px_int axisHandle,px_uchar sampleIndex,px_uint16 weightQ15)
{
	return PX_LiveRealtimeSetAxisSample(plive,axisHandle,sampleIndex,weightQ15);
}

/** 设置 RT30 轴归一化值 (Q15) */
px_bool PX_LiveSetRealtimeAxisNormalizedQ15(PX_Live *plive,px_int axisHandle,px_uint16 normalizedQ15,px_uint16 weightQ15)
{
	return PX_LiveRealtimeSetAxisNormalizedQ15(plive,axisHandle,normalizedQ15,weightQ15);
}

/** 通过 ID 查找 RT30 轴 */
px_int PX_LiveFindRealtimeAxisById(const PX_Live *plive,const px_char id[])
{
	return PX_LiveRealtimeFindAxisById(plive,id);
}


/** 渲染一帧 */
px_void PX_LiveRender(px_surface *psurface,PX_Live *plive,px_int x,px_int y,PX_ALIGN refPoint,px_dword elapsed)
{
	PX_LiveFrameworkRender(psurface,plive,x,y,refPoint,elapsed);
}
