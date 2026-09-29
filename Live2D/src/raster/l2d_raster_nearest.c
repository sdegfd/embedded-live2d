/**
 * Fast-nearest embedded raster.
 * Edge coverage, clipping, 16.16 UV and BGRA word blend match the formulas
 * previously inlined beside the generic PainterEngine span.
 */
#include "l2d_raster_nearest.h"
#include "l2d_config.h"

#if L2D_CFG_PROFILE_DETAIL
#define DETAIL_FRAGMENT(x,y,a) l2d_raster_detail_fragment((x),(y),(a))
#define DETAIL_SCANLINE() l2d_raster_detail_scanline()
#define DETAIL_SPAN(n,s,t,ds,dt,w,h) l2d_raster_detail_span((n),(s),(t),(ds),(dt),(w),(h))
#define DETAIL_SAMPLE_IN() l2d_raster_detail_sample_in()
#define DETAIL_SAMPLE_OUT() l2d_raster_detail_sample_out()
#else
#define DETAIL_FRAGMENT(x,y,a) ((void)0)
#define DETAIL_SCANLINE() ((void)0)
#define DETAIL_SPAN(n,s,t,ds,dt,w,h) ((void)0)
#define DETAIL_SAMPLE_IN() ((void)0)
#define DETAIL_SAMPLE_OUT() ((void)0)
#endif

/* Fast-nearest span. Coverage, clipping and 16.16 UV match the generic span.
 * s_fp_step / t_fp_step are constant for the triangle. */
static L2D_ALWAYS_INLINE px_void l2d_span_fast_nearest(px_surface *psurface,
	px_texture *ptexture,
	px_float y,
	px_float xleft,
	px_float xright,
	px_float affine_x0, px_float affine_y0,
	px_float affine_s0, px_float affine_t0,
	px_float affine_s_dx, px_float affine_s_dy,
	px_float affine_t_dx, px_float affine_t_dy,
	px_int s_fp_step, px_int t_fp_step)
{
	px_float s, t;
	px_float xstartf;
	px_color *dst;
	px_int ix, iy;
	px_int xstart, xend;
	px_int tx, ty;
	px_int s_fp, t_fp;
	px_int texture_width, texture_height;

	DETAIL_SCANLINE();
	if (xright == xleft)
	{
		return;
	}

	xstart = (px_int)(xleft + 0.5f);
	xend = (px_int)(xright + 0.5f);
	if (xend <= xstart)
	{
		return;
	}

	iy = (px_int)y;
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
	s_fp = (px_int)(s * texture_width * 65536.0f);
	t_fp = (px_int)(t * texture_height * 65536.0f);
	DETAIL_SPAN(xend-xstart,s_fp,t_fp,s_fp_step,t_fp_step,texture_width,texture_height);

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
		{
			px_dword src = ptexture->surfaceBuffer[(px_uint)ty * (px_uint)texture_width + (px_uint)tx]._argb.ucolor;
			px_dword a = src >> 24;
			DETAIL_FRAGMENT(ix, iy, (px_int)a);
			if (a == 255u)
			{
				dst->_argb.ucolor = src;
			}
			else if (a)
			{
				dst->_argb.ucolor = l2d_blend_bgra(dst->_argb.ucolor, src);
			}
		}
		s_fp += s_fp_step;
		t_fp += t_fp_step;
	}
}

/* noinline: inlining this back into PX_LiveFrameworkRenderCurrent spills the
 * edge and affine state out of registers on every scanline. */
L2D_NOINLINE px_void l2d_raster_fast_nearest(px_surface *psurface,
	PX_LiveRenderVertex p0, PX_LiveRenderVertex p1, PX_LiveRenderVertex p2,
	px_texture *ptexture)
{
	px_float affine_x0=p0.position.x,affine_y0=p0.position.y;
	px_float affine_s0=p0.u,affine_t0=p0.v;
	px_float affine_s_dx,affine_s_dy,affine_t_dx,affine_t_dy;
	px_float dx1=p1.position.x-affine_x0;
	px_float dy1=p1.position.y-affine_y0;
	px_float dx2=p2.position.x-affine_x0;
	px_float dy2=p2.position.y-affine_y0;
	px_float denominator=dx1*dy2-dx2*dy1;
	px_float invDenominator;
	px_int s_fp_step,t_fp_step;
	px_int texture_width,texture_height;
	px_bool k01infinite,k02infinite;
	px_float k01,b01,k02,b02;
	px_float x0,y0,x1,y1,x2,y2;
	px_float y,xleft,xright;
	px_float btmy,midy;

	if (PX_ABS(denominator)<0.000001f)
	{
		return;
	}
	invDenominator=1.0f/denominator;
	affine_s_dx=((p1.u-affine_s0)*dy2-(p2.u-affine_s0)*dy1)*invDenominator;
	affine_s_dy=(dx1*(p2.u-affine_s0)-dx2*(p1.u-affine_s0))*invDenominator;
	affine_t_dx=((p1.v-affine_t0)*dy2-(p2.v-affine_t0)*dy1)*invDenominator;
	affine_t_dy=(dx1*(p2.v-affine_t0)-dx2*(p1.v-affine_t0))*invDenominator;

	if (p0.position.x>100000.f||p0.position.x<-100000.f||p0.position.y>100000.f||p0.position.y<-100000.f||
		p1.position.x>100000.f||p1.position.x<-100000.f||p1.position.y>100000.f||p1.position.y<-100000.f||
		p2.position.x>100000.f||p2.position.x<-100000.f||p2.position.y>100000.f||p2.position.y<-100000.f)
	{
		return;
	}

	texture_width=ptexture->width;
	texture_height=ptexture->height;
	s_fp_step=(px_int)(affine_s_dx * texture_width * 65536.0f);
	t_fp_step=(px_int)(affine_t_dx * texture_height * 65536.0f);

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
	}

	x0=p0.position.x;
	y0=p0.position.y;
	x1=p1.position.x;
	y1=p1.position.y;
	x2=p2.position.x;
	y2=p2.position.y;

	k01infinite=PX_FALSE;
	k02infinite=PX_FALSE;
	if (x0==x1)
	{
		k01infinite=PX_TRUE;
		k01=1;
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
		k02=1;
		b02=x0;
	}
	else
	{
		k02=(y0-y2)/(x0-x2);
		b02=y0-k02*x0;
	}

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
		l2d_span_fast_nearest(psurface, ptexture, y, xleft, xright,
			affine_x0,affine_y0,affine_s0,affine_t0,
			affine_s_dx,affine_s_dy,affine_t_dx,affine_t_dy,
			s_fp_step,t_fp_step);
	}

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
	}

	x0=p0.position.x;
	y0=p0.position.y;
	x1=p1.position.x;
	y1=p1.position.y;
	x2=p2.position.x;
	y2=p2.position.y;

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
		l2d_span_fast_nearest(psurface, ptexture, y, xleft, xright,
			affine_x0,affine_y0,affine_s0,affine_t0,
			affine_s_dx,affine_s_dy,affine_t_dx,affine_t_dy,
			s_fp_step,t_fp_step);
	}
}
