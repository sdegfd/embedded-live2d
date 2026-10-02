/* Differential pixel test against the frozen scalar raster, including
 * subpixel coverage boundaries, clipped triangles and translucent texels. */
#include "l2d_raster_nearest.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

void l2d_raster_reference_nearest(px_surface *, PX_LiveRenderVertex,
    PX_LiveRenderVertex, PX_LiveRenderVertex, px_texture *);
static uint32_t seed = 0x31415926u;
static uint32_t random_word(void)
{
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    return seed;
}
int main(void)
{
    px_color texture_pixels[31*23], actual[64*64], expected[64*64];
    px_texture texture = {0}; px_surface a = {0}, b = {0};
    texture.surfaceBuffer = texture_pixels; texture.width = 31; texture.height = 23;
    a.surfaceBuffer = actual; a.width = a.height = 64;
    a.limit_right = a.limit_bottom = 63; b = a; b.surfaceBuffer = expected;
    for (int y=0;y<23;++y) for (int x=0;x<31;++x) {
        unsigned tile=(y/8)*4+x/8;
        uint32_t pixel=random_word();
        if (tile%3==0) pixel&=0x00ffffffu;
        if (tile%3==1) pixel|=0xff000000u;
        texture_pixels[y*31+x]._argb.ucolor=pixel;
    }
    for (unsigned trial=0;trial<20000;++trial) {
        PX_LiveRenderVertex v[3] = {0};
        for (int i=0;i<3;++i) {
            v[i].position.x=(float)((int)(random_word()%11200)-2400)/100.f;
            v[i].position.y=(float)((int)(random_word()%11200)-2400)/100.f;
            if (trial%4==0) {
                v[i].position.x=nextafterf(floorf(v[i].position.x)+0.5f,
                    i%2 ? INFINITY : -INFINITY);
            }
            v[i].u=(float)((int)(random_word()%1300)-150)/1000.f;
            v[i].v=(float)((int)(random_word()%1300)-150)/1000.f;
        }
        for (int i=0;i<64*64;++i) actual[i]._argb.ucolor=random_word();
        memcpy(expected,actual,sizeof(actual));
        a.limit_left=b.limit_left=(int)(trial%7);
        a.limit_top=b.limit_top=(int)(trial%5);
        l2d_raster_reference_nearest(&b,v[0],v[1],v[2],&texture);
        px_texture *source=&texture;
        if (trial%4>=2) {
            l2d_raster_job job;
            l2d_raster_prepare(&job,v[0],v[1],v[2],source);
            if (trial%8>=4) {
                px_color scratch[64*8];
                l2d_raster_jobs_scratch(&job,1,&a,scratch,sizeof(scratch),2,0);
                l2d_raster_jobs_scratch(&job,1,&a,scratch,sizeof(scratch),2,1);
            } else {
                l2d_raster_jobs(&job,1,&a,2,0);
                l2d_raster_jobs(&job,1,&a,2,1);
            }
        } else l2d_raster_fast_nearest(&a,v[0],v[1],v[2],source);
        if (memcmp(actual,expected,sizeof(actual))) {
            fprintf(stderr,"Raster pixel mismatch at triangle %u\n",trial); return 1;
        }
    }
    puts("PASS: 20000 randomized triangles exactly match scalar pixel oracle");
    return 0;
}
