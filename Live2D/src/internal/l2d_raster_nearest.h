/**
 * Embedded fast-nearest raster. Coverage and 16.16 UV match the generic
 * span in PX_LiveFramework.c. The kernel is a separate translation unit so
 * the legacy framework file does not keep growing around it.
 */
#ifndef L2D_RASTER_NEAREST_H
#define L2D_RASTER_NEAREST_H

#include "PX_LiveFramework.h"
#include <stddef.h>

/* BGRA8888 word blend. Channel math matches the byte formula exactly. */
static inline px_dword l2d_blend_bgra(px_dword dst, px_dword src)
{
	px_dword sa = src >> 24;
	px_dword inv = 256u - sa, sp1 = sa + 1u;
	/* Each 16-bit lane sums to at most 255*257=65535: no carry
	 * crosses between blue and red, so this is the exact byte formula. */
	px_dword rb = (((dst & 0x00ff00ffu) * inv +
	                 (src & 0x00ff00ffu) * sp1) >> 8) & 0x00ff00ffu;
	px_dword g = (((dst & 0x0000ff00u) * inv +
	                (src & 0x0000ff00u) * sp1) >> 8) & 0x0000ff00u;
	px_dword oa = 255u - (((256u - (dst >> 24)) * (255u - sa)) >> 8);
	return rb | g | (oa << 24);
}

typedef struct l2d_raster_half {
    px_float left_k,left_b,right_k,right_b,begin,end;
    px_bool left_vertical,right_vertical;
} l2d_raster_half;
typedef struct l2d_raster_job {
    l2d_raster_half half[2];
    px_float affine_x0,affine_y0,affine_s0,affine_t0;
    px_float affine_s_dx,affine_s_dy,affine_t_dx,affine_t_dy;
    px_int s_fp_step,t_fp_step;
    px_bool valid;
    px_texture *texture; /* Immutable for the synchronous batch lifetime. */
} l2d_raster_job;
void l2d_raster_prepare(l2d_raster_job *job,
    PX_LiveRenderVertex p0,PX_LiveRenderVertex p1,PX_LiveRenderVertex p2,
    px_texture *texture);

/* Runs jobs in original painter order, touching only this worker's rows. */
void l2d_raster_jobs(const l2d_raster_job *jobs, int count,
                     const px_surface *surface, unsigned step, unsigned phase);
/* Uses bounded caller-owned SRAM, preserving every preexisting destination pixel. */
void l2d_raster_jobs_scratch(const l2d_raster_job *jobs, int count,
    const px_surface *surface, void *scratch, size_t bytes, unsigned workers, unsigned phase);
void l2d_raster_jobs_rgb565(const l2d_raster_job *jobs, int count,
    const px_surface *surface, void *scratch, size_t bytes, unsigned workers, unsigned phase);
int l2d_port_rgb565_supported(void);
void l2d_port_raster_dispatch(const l2d_raster_job *jobs, int count, const px_surface *surface);

void l2d_raster_fast_nearest(px_surface *psurface,
                             PX_LiveRenderVertex p0,
                             PX_LiveRenderVertex p1,
                             PX_LiveRenderVertex p2,
                             px_texture *ptexture);

#if L2D_CFG_PROFILE_DETAIL
void l2d_raster_detail_fragment(px_int x, px_int y, px_int alpha);
void l2d_raster_detail_scanline(void);
void l2d_raster_detail_span(px_int length, px_int s, px_int t,
                            px_int ds, px_int dt, px_int width, px_int height);
void l2d_raster_detail_sample_in(void);
void l2d_raster_detail_sample_out(void);
#endif

#endif
