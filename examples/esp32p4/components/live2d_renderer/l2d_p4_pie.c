#include "l2d_p4_pie.h"
#include "l2d/l2d.h"
#include <string.h>

void l2d_p4_pie_rgb565_blocks(const uint32_t *, uint16_t *, size_t);
void l2d_p4_pie_zero_blocks(void *, size_t);

void l2d_p4_pie_rgb565(const uint32_t *src, uint16_t *dst, size_t count)
{
    while (count && (((uintptr_t)src | (uintptr_t)dst) & 15u)) {
        *dst++=l2d_bgra8888_to_rgb565(*src++); --count;
    }
    size_t blocks=count/8u;
    if (blocks) {
        l2d_p4_pie_rgb565_blocks(src,dst,blocks);
        src+=blocks*8u; dst+=blocks*8u; count-=blocks*8u;
    }
    while (count--) *dst++=l2d_bgra8888_to_rgb565(*src++);
}

void l2d_p4_pie_zero(void *target, size_t bytes)
{
    unsigned char *dst=target;
    while (bytes && ((uintptr_t)dst & 15u)) { *dst++=0; --bytes; }
    size_t blocks=bytes/16u;
    if (blocks) { l2d_p4_pie_zero_blocks(dst,blocks); dst+=blocks*16u; bytes-=blocks*16u; }
    memset(dst,0,bytes);
}

bool l2d_p4_pie_selftest(void)
{
    uint32_t src[48] __attribute__((aligned(16)));
    uint16_t dst[64] __attribute__((aligned(16)));
    unsigned char zeros[160] __attribute__((aligned(16)));
    for (unsigned offset=0;offset<8;++offset) for (unsigned n=1;n<=33;++n) {
        for (unsigned i=0;i<48;++i) src[i]=0x9e3779b9u*(i+offset*37u+n*71u);
        memset(dst,0xa5,sizeof(dst));
        l2d_p4_pie_rgb565(src+offset,dst+offset,n);
        for (unsigned i=0;i<64;++i) {
            uint16_t expected=i>=offset && i<offset+n ?
                l2d_bgra8888_to_rgb565(src[i]) : 0xa5a5;
            if (dst[i]!=expected) return false;
        }
    }
    for (unsigned offset=0;offset<16;++offset) for (unsigned n=0;n<=127;++n) {
        memset(zeros,0xa5,sizeof(zeros));
        l2d_p4_pie_zero(zeros+offset,n);
        for (unsigned i=0;i<sizeof(zeros);++i)
            if (zeros[i]!=(i>=offset && i<offset+n ? 0 : 0xa5)) return false;
    }
    return true;
}
