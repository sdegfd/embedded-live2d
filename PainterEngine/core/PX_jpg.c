/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Embedded Live2D contributors
 * JPEG decoding is implemented by stb_image (see third_party/stb).
 */
#include "PX_jpg.h"
#include <limits.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 8192
#include "../third_party/stb/stb_image.h"

px_bool PX_JpgVerify(px_byte *bytes, px_int size)
{
    return bytes && size >= 3 && bytes[0] == 0xff &&
        bytes[1] == 0xd8 && bytes[2] == 0xff;
}

px_bool PX_JpgDecoderInitialize(px_memorypool *mp, PX_JpgDecoder *decoder,
                                px_byte *bytes, px_int size)
{
    int width, height, channels;
    stbi_uc *decoded;
    px_uint count;
    if (!decoder) return PX_FALSE;
    PX_memset(decoder, 0, sizeof(*decoder));
    if (!mp || !PX_JpgVerify(bytes, size)) return PX_FALSE;
    decoded = stbi_load_from_memory(bytes, size, &width, &height, &channels, 4);
    if (!decoded) return PX_FALSE;
    if (width <= 0 || height <= 0 || (unsigned)width > UINT_MAX/4u/(unsigned)height) {
        stbi_image_free(decoded); return PX_FALSE;
    }
    count = (px_uint)width * (px_uint)height * 4u;
    decoder->pixels = MP_Malloc(mp, count);
    if (!decoder->pixels) { stbi_image_free(decoded); return PX_FALSE; }
    PX_memcpy(decoder->pixels, decoded, count);
    stbi_image_free(decoded);
    decoder->mp = mp; decoder->width = width; decoder->height = height;
    return PX_TRUE;
}

px_int PX_JpgDecoderGetWidth(PX_JpgDecoder *decoder)
{
    return decoder ? decoder->width : 0;
}
px_int PX_JpgDecoderGetHeight(PX_JpgDecoder *decoder)
{
    return decoder ? decoder->height : 0;
}
px_void PX_JpgDecoderRenderToSurface(PX_JpgDecoder *decoder, px_surface *surface)
{
    px_int x, y, width, height;
    if (!decoder || !decoder->pixels || !surface || !surface->surfaceBuffer) return;
    width = decoder->width < surface->width ? decoder->width : surface->width;
    height = decoder->height < surface->height ? decoder->height : surface->height;
    for (y = 0; y < height; ++y) for (x = 0; x < width; ++x) {
        const px_byte *p = decoder->pixels + ((px_uint)y * decoder->width + x) * 4u;
        PX_SurfaceSetPixel(surface, x, y, PX_COLOR(p[3], p[0], p[1], p[2]));
    }
}
px_void PX_JpgDecoderFree(PX_JpgDecoder *decoder)
{
    if (!decoder) return;
    if (decoder->pixels) MP_Free(decoder->mp, decoder->pixels);
    PX_memset(decoder, 0, sizeof(*decoder));
}
