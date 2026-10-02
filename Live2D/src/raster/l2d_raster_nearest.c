/**
 * Fast-nearest embedded raster.
 * Edge coverage, clipping, 16.16 UV and BGRA word blend match the formulas
 * previously inlined beside the generic PainterEngine span.
 */
#include "l2d_raster_nearest.h"
#include "l2d_config.h"
#include <string.h>

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
	dst = psurface->surfaceBuffer + xstart + psurface->width * (iy-psurface->buffer_y0);
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
			#if L2D_CFG_PROFILE_DETAIL
            px_dword src = PX_SURFACECOLOR(ptexture,tx,ty)._argb.ucolor;
#else
            px_dword src = ptexture->surfaceBuffer[(px_uint)ty*(px_uint)texture_width+(px_uint)tx]._argb.ucolor;
#endif
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
    px_float clip_begin=psurface->limit_top>0 ? psurface->limit_top+0.5f : -0.5f;
    px_float clip_end=psurface->limit_bottom+0.5f;

    if ((p0.position.y<clip_begin && p1.position.y<clip_begin && p2.position.y<clip_begin) ||
        (p0.position.y>clip_end && p1.position.y>clip_end && p2.position.y>clip_end)) return;
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

    y=(px_int)(y0+0.5f)+0.5f;
    if (y<clip_begin) y=clip_begin;
	for(; y<=midy && y<=clip_end; y++)
	{
        if (psurface->row_step && (psurface->row_step==2 ? ((px_uint)(px_int)y&1u) :
            (px_uint)(px_int)y%psurface->row_step)!=psurface->row_phase) continue;
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

    y=(px_int)(midy+0.5f)+0.5f;
    if (y<clip_begin) y=clip_begin;
	for(; y<y0 && y<=clip_end; y++)
	{
        if (psurface->row_step && (psurface->row_step==2 ? ((px_uint)(px_int)y&1u) :
            (px_uint)(px_int)y%psurface->row_step)!=psurface->row_phase) continue;
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

/* Prepare exact edge and affine coefficients once per triangle/frame.
 * Preserve the original operation order; workers only evaluate scanlines. */
void l2d_raster_prepare(l2d_raster_job *job,
    PX_LiveRenderVertex p0, PX_LiveRenderVertex p1, PX_LiveRenderVertex p2,
    px_texture *ptexture)
{
    job->valid=0; job->texture=ptexture;

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

    job->half[0]=(l2d_raster_half){k01,b01,k02,b02,
        (px_int)(y0+0.5f)+0.5f,midy,k01infinite,k02infinite};

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

    job->half[1]=(l2d_raster_half){k01,b01,k02,b02,
        (px_int)(midy+0.5f)+0.5f,y0,k01infinite,k02infinite};
    job->affine_x0=affine_x0; job->affine_y0=affine_y0;
    job->affine_s0=affine_s0; job->affine_t0=affine_t0;
    job->affine_s_dx=affine_s_dx; job->affine_s_dy=affine_s_dy;
    job->affine_t_dx=affine_t_dx; job->affine_t_dy=affine_t_dy;
    job->s_fp_step=s_fp_step; job->t_fp_step=t_fp_step;
    job->valid=1;
}

static void l2d_raster_prepared(px_surface *surface,const l2d_raster_job *job)
{
    if (!job->valid) return;
    px_float clip_begin=surface->limit_top>0 ? surface->limit_top+0.5f : -0.5f;
    px_float clip_end=surface->limit_bottom+0.5f;
    for (int part=0;part<2;++part) {
        const l2d_raster_half *h=&job->half[part];
        px_float y=h->begin;
        if (y<clip_begin) y=clip_begin;
        for (; (part ? y<h->end : y<=h->end) && y<=clip_end; y++) {
            if (surface->row_step && (surface->row_step==2 ? ((px_uint)(px_int)y&1u) :
                (px_uint)(px_int)y%surface->row_step)!=surface->row_phase) continue;
            px_float left=h->left_vertical ? h->left_b : (y-h->left_b)/h->left_k;
            px_float right=h->right_vertical ? h->right_b : (y-h->right_b)/h->right_k;
            l2d_span_fast_nearest(surface,job->texture,y,left,right,
                job->affine_x0,job->affine_y0,job->affine_s0,job->affine_t0,
                job->affine_s_dx,job->affine_s_dy,job->affine_t_dx,job->affine_t_dy,
                job->s_fp_step,job->t_fp_step);
        }
    }
}

void l2d_raster_jobs(const l2d_raster_job *jobs, int count,
                     const px_surface *surface, unsigned step, unsigned phase)
{
    px_surface rows = *surface;
    rows.row_step = step;
    rows.row_phase = phase;
    for (int i=0; i<count; ++i) {
        l2d_raster_prepared(&rows,&jobs[i]);
    }
}

void l2d_raster_jobs_scratch(const l2d_raster_job *jobs, int count,
    const px_surface *surface, void *scratch, size_t bytes, unsigned workers, unsigned phase)
{
    size_t stride=(size_t)surface->width*sizeof(px_color);
    size_t capacity=bytes/stride;
    if (!workers || phase>=workers) return;
    if (!capacity) { l2d_raster_jobs(jobs,count,surface,workers,phase); return; }
    if (capacity>(size_t)surface->height) capacity=(size_t)surface->height;
    int band_height=(int)capacity;
    for (int top=(int)phase*band_height; top<surface->height; top+=(int)workers*band_height) {
        int bottom=top+band_height-1;
        if (bottom>=surface->height) bottom=surface->height-1;
        px_surface band=*surface;
        band.limit_top=top>surface->limit_top ? top : surface->limit_top;
        band.limit_bottom=bottom<surface->limit_bottom ? bottom : surface->limit_bottom;
        if (band.limit_top>band.limit_bottom) continue;
        size_t span_bytes=(size_t)(bottom-top+1)*stride;
        px_color *pixels=surface->surfaceBuffer+(size_t)top*surface->width;
        memcpy(scratch,pixels,span_bytes);
        band.surfaceBuffer=scratch;
        band.buffer_y0=top;
        l2d_raster_jobs(jobs,count,&band,0,0);
        memcpy(pixels,scratch,span_bytes);
    }
}

void l2d_raster_jobs_rgb565(const l2d_raster_job *jobs, int count,
    const px_surface *surface, void *scratch, size_t bytes, unsigned workers, unsigned phase)
{
    if (!surface || !scratch || !surface->rgb565_sink || surface->width<=0 ||
        surface->height<=0 || !workers || phase>=workers) return;
    size_t stride=(size_t)surface->width*sizeof(px_color);
    int rows=(int)(bytes/stride);
    if (!rows) return;
    if (rows>surface->height) rows=surface->height;
    for (int top=(int)phase*rows;top<surface->height;top+=(int)workers*rows) {
        int n=surface->height-top;
        if (n>rows) n=rows;
        px_surface band=*surface;
        band.surfaceBuffer=scratch; band.buffer_y0=top;
        band.limit_top=top; band.limit_bottom=top+n-1;
        size_t pixels=(size_t)n*surface->width;
        memset(scratch,0,pixels*sizeof(px_color));
        l2d_raster_jobs(jobs,count,&band,0,0);
        const px_color *bgra=scratch;
        px_ushort *rgb=surface->rgb565_sink+(size_t)top*surface->width;
        for (size_t i=0;i<pixels;++i) {
            px_dword c=bgra[i]._argb.ucolor;
            rgb[i]=(px_ushort)(((c>>8)&0xf800u)|((c>>5)&0x7e0u)|((c>>3)&0x1fu));
        }
        if (surface->bgra_capture)
            memcpy(surface->bgra_capture+(size_t)top*surface->width,scratch,pixels*sizeof(px_color));
    }
}
