#include "l2d_p4_bitscrambler.h"
#include "driver/bitscrambler_loopback.h"
#include "soc/bitscrambler_peri_select.h"
#include "esp_heap_caps.h"
#include "esp_cache.h"
#include <stdint.h>
#include <string.h>

BITSCRAMBLER_PROGRAM(bgra_program,"l2d_bgra_rgb565");

esp_err_t l2d_p4_bs_create(void **handle, size_t max_bytes)
{
    bitscrambler_handle_t bs=NULL;
    *handle=NULL;
    /* This example has no GPSPI2 DMA user. Do not share that DMA peripheral. */
    esp_err_t ret=bitscrambler_loopback_create(&bs,SOC_BITSCRAMBLER_ATTACH_GPSPI2,max_bytes);
    if (ret!=ESP_OK) return ret;
    ret=bitscrambler_load_program(bs,bgra_program);
    if (ret!=ESP_OK) { bitscrambler_free(bs); return ret; }
    *handle=bs;
    return ESP_OK;
}
void l2d_p4_bs_free(void *handle)
{
    if (handle) bitscrambler_free(handle);
}
esp_err_t l2d_p4_bs_convert(void *handle, void *bgra, size_t bgra_bytes,
                            void *rgb565, size_t rgb_bytes)
{
    if (!handle || !bgra || !rgb565 || !rgb_bytes || rgb_bytes>SIZE_MAX/2 || bgra_bytes!=rgb_bytes*2) return ESP_ERR_INVALID_ARG;
    size_t written=0;
    esp_err_t ret=bitscrambler_loopback_run(handle,bgra,bgra_bytes,rgb565,rgb_bytes,&written);
    return ret!=ESP_OK ? ret : written==rgb_bytes ? ESP_OK : ESP_ERR_INVALID_SIZE;
}
bool l2d_p4_bs_selftest(void *handle)
{
    uint32_t *src=heap_caps_aligned_alloc(128,512*4,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    uint16_t *dst=heap_caps_aligned_alloc(128,640*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if (!src || !dst) { heap_caps_free(src); heap_caps_free(dst); return false; }
    bool ok=true;
    uint32_t rng=0x9e3779b9u;
    for (unsigned pass=0;pass<3 && ok;++pass) {
        for (unsigned i=0;i<512;++i) {
            rng^=rng<<13; rng^=rng>>17; rng^=rng<<5;
            src[i]=pass==0 ? i*0x01010101u : pass==1 ? 1u<<(i%32) : rng;
        }
        for (unsigned n=64;n<=512 && ok;n*=2) {
            memset(dst,0xa5,640*2);
            esp_cache_msync(dst,640*2,ESP_CACHE_MSYNC_FLAG_DIR_C2M);
            ok=l2d_p4_bs_convert(handle,src,n*4,dst,n*2)==ESP_OK;
            esp_cache_msync(dst,640*2,ESP_CACHE_MSYNC_FLAG_DIR_M2C);
            for (unsigned i=0;i<640 && ok;++i) {
                uint32_t c=src[i<512 ? i : 0];
                uint16_t expected=i<n ? ((c>>8)&0xf800u)|((c>>5)&0x7e0u)|((c>>3)&0x1fu) : 0xa5a5;
                if (dst[i]!=expected) ok=false;
            }
        }
    }
    heap_caps_free(src); heap_caps_free(dst);
    return ok;
}
