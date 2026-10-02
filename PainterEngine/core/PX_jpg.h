/* SPDX-License-Identifier: MIT
 * PC JPEG facade for the licensed stb_image decoder.
 */
#ifndef PX_JPG_H
#define PX_JPG_H
#include "PX_Surface.h"
typedef struct {
    px_memorypool *mp;
    px_byte *pixels;
    px_int width, height;
} PX_JpgDecoder;
px_bool PX_JpgDecoderInitialize(px_memorypool *, PX_JpgDecoder *, px_byte *, px_int);
px_bool PX_JpgVerify(px_byte *, px_int);
px_int PX_JpgDecoderGetWidth(PX_JpgDecoder *);
px_int PX_JpgDecoderGetHeight(PX_JpgDecoder *);
px_void PX_JpgDecoderRenderToSurface(PX_JpgDecoder *, px_surface *);
px_void PX_JpgDecoderFree(PX_JpgDecoder *);
#endif
