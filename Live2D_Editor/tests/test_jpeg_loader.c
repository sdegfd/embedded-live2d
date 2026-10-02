/* PC-only JPEG facade: baseline/progressive, clipped output, failure cleanup. */
#include "PX_jpg.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"JPEG check failed at %d\n",__LINE__); return 1; } } while (0)
int main(void)
{
    const char *paths[]={JPEG_BASELINE_FILE,JPEG_PROGRESSIVE_FILE};
    unsigned char *arena=malloc(65536);
    CHECK(arena);
    px_memorypool pool=MP_Create(arena,65536);
    px_uint free_before=pool.FreeSize;
    for (int fixture=0;fixture<2;++fixture) {
        unsigned char bytes[4096];
        FILE *file=fopen(paths[fixture],"rb"); CHECK(file);
        size_t size=fread(bytes,1,sizeof(bytes),file); fclose(file);
        CHECK(size && size<sizeof(bytes));
        PX_JpgDecoder decoder;
        CHECK(!PX_JpgVerify(bytes,0) && !PX_JpgVerify(NULL,10));
        CHECK(!PX_JpgDecoderInitialize(&pool,&decoder,bytes,3));
        PX_JpgDecoderFree(&decoder);
        CHECK(pool.FreeSize==free_before);
        {
            unsigned char tiny_arena[128];
            px_memorypool tiny=MP_Create(tiny_arena,sizeof(tiny_arena));
            MP_NoCatchError(&tiny);
            CHECK(!PX_JpgDecoderInitialize(&tiny,&decoder,bytes,(px_int)size));
            CHECK(!decoder.pixels);
            PX_JpgDecoderFree(&decoder);
        }
        CHECK(PX_JpgDecoderInitialize(&pool,&decoder,bytes,(px_int)size));
        CHECK(PX_JpgDecoderGetWidth(&decoder)==9 && PX_JpgDecoderGetHeight(&decoder)==7);
        px_color pixels[14];
        for (int i=0;i<14;++i) pixels[i]._argb.ucolor=0x12345678;
        px_surface surface={0};
        surface.surfaceBuffer=pixels+1; surface.width=4; surface.height=3;
        surface.limit_right=3; surface.limit_bottom=2;
        PX_JpgDecoderRenderToSurface(&decoder,&surface);
        CHECK(pixels[0]._argb.ucolor==0x12345678 && pixels[13]._argb.ucolor==0x12345678);
        for (int i=1;i<=12;++i) {
            CHECK(abs((int)pixels[i]._argb.r-23)<=2 &&
                  abs((int)pixels[i]._argb.g-89)<=2 &&
                  abs((int)pixels[i]._argb.b-157)<=2 && pixels[i]._argb.a==255);
        }
        PX_JpgDecoderFree(&decoder); PX_JpgDecoderFree(&decoder);
        CHECK(pool.FreeSize==free_before);
    }
    free(arena);
    puts("PASS: baseline/progressive JPEG, clipped RGBA output and failure cleanup");
    return 0;
}
