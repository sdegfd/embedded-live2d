/**
 * Embedded fast-nearest raster. Coverage and 16.16 UV match the generic
 * span in PX_LiveFramework.c. The kernel is a separate translation unit so
 * the legacy framework file does not keep growing around it.
 */
#ifndef L2D_RASTER_NEAREST_H
#define L2D_RASTER_NEAREST_H

#include "PX_LiveFramework.h"

/* BGRA8888 word blend. Channel math matches the byte formula exactly. */
static inline px_dword l2d_blend_bgra(px_dword dst, px_dword src)
{
	px_dword sa = src >> 24;
	px_dword sb = src & 255u;
	px_dword sg = (src >> 8) & 255u;
	px_dword sr = (src >> 16) & 255u;
	px_dword da = dst >> 24;
	px_dword db = dst & 255u;
	px_dword dg = (dst >> 8) & 255u;
	px_dword dr = (dst >> 16) & 255u;
	px_dword inv = 256u - sa;
	px_dword sp1 = sa + 1u;
	px_dword ob = (inv * db + sb * sp1) >> 8;
	px_dword og = (inv * dg + sg * sp1) >> 8;
	px_dword orr = (inv * dr + sr * sp1) >> 8;
	px_dword oa = 255u - (((256u - da) * (255u - sa)) >> 8);
	return ob | (og << 8) | (orr << 16) | (oa << 24);
}

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
