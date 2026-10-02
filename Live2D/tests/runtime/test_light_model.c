#include "l2d/l2d.h"
#include "l2d_light_scenarios.h"
#include "l2d_test_allocator.h"
#include <openssl/sha.h>
#include <zlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); result = 1; goto done; } } while (0)

int main(int argc, char **argv)
{
    int result = 0;
    void *wire = NULL, *bgra = NULL, *rgb = NULL, *band_bgra=NULL, *band_rgb=NULL;
    FILE *file = NULL, *csv = NULL;
    l2d_model_t *model = NULL;
    l2d_instance_t *instance = NULL, *other = NULL;
    l2d_test_allocator_t allocator;
    l2d_model_config_t config;
    l2d_test_allocator_init(&allocator);
    config.allocator = &allocator.allocator;
    int bands=0;
    if (argc>1 && !strcmp(argv[1],"--bands")) { bands=1; --argc; ++argv; }
    file = fopen(L2D_MODEL_LIGHT_LIVE, "rb");
    CHECK(file);
    CHECK(fseek(file, 0, SEEK_END) == 0);
    long size = ftell(file);
    CHECK(size == L2D_LIGHT_FILE_BYTES);
    rewind(file);
    wire = malloc((size_t)size);
    CHECK(wire && fread(wire, 1, (size_t)size, file) == (size_t)size);
    fclose(file); file = NULL;
    unsigned char sha[32]; char hex[65];
    SHA256(wire, (size_t)size, sha);
    for (int i = 0; i < 32; ++i) sprintf(hex + i * 2, "%02x", sha[i]);
    CHECK(strcmp(hex, L2D_LIGHT_SHA256) == 0);
    /* Experimental wire version 2 is unsupported; failed loads must release memory. */
    {
        unsigned char *bytes=wire;
        unsigned char saved[4]; memcpy(saved,bytes+56,4);
        bytes[56]=2; bytes[57]=bytes[58]=bytes[59]=0;
        l2d_model_t *bad=NULL;
        l2d_status_t status=l2d_model_load_memory(&config,wire,(size_t)size,&bad);
        memcpy(bytes+56,saved,4);
        CHECK(status==L2D_ERR_VERSION && !bad && allocator.alloc_calls==allocator.free_calls);
    }
    CHECK(l2d_model_load_memory(&config, wire, (size_t)size, &model) == L2D_OK);
    free(wire); wire = NULL;
    l2d_model_info_t info;
    l2d_model_info(model, &info);
    CHECK(info.width == 480 && info.height == 640 && info.layer_count == 14 &&
          info.texture_count == 14 && info.animation_count == 0 && info.axis_count == 13);
    CHECK(l2d_instance_create(model, NULL, &instance) == L2D_OK);
    CHECK(l2d_instance_create(model, NULL, &other) == L2D_OK);
    CHECK(!l2d_instance_play_animation(instance, 0));
    CHECK(l2d_instance_enter_rt30(instance) && l2d_instance_enter_rt30(other));
    int handles[L2D_LIGHT_AXES];
    CHECK(l2d_light_find_axes(instance, handles));
    l2d_instance_set_render_scale(instance, L2D_LIGHT_SCALE);
    l2d_instance_set_render_scale(other, L2D_LIGHT_SCALE);
    const size_t bgra_bytes = L2D_LIGHT_CANVAS_W * L2D_LIGHT_CANVAS_H * 4u;
    const size_t rgb_bytes = L2D_LIGHT_CANVAS_W * L2D_LIGHT_CANVAS_H * 2u;
    bgra = malloc(bgra_bytes); rgb = malloc(rgb_bytes);
    CHECK(bgra && rgb);
    l2d_surface_t surface = {bgra, L2D_LIGHT_CANVAS_W, L2D_LIGHT_CANVAS_H,
        L2D_LIGHT_CANVAS_W * 4, bgra_bytes, L2D_PIXEL_BGRA8888_LE};
    if (argc == 3 && strcmp(argv[1], "--crc-out") == 0) {
        csv = fopen(argv[2], "w"); CHECK(csv);
        fprintf(csv, "pose,argb_crc,rgb565_crc,render_ok,submit_ok,conversion_ok\n");
    }
    if (bands) {
        CHECK(l2d_instance_can_render_rgb565(instance));
        band_bgra=malloc(bgra_bytes); band_rgb=malloc(rgb_bytes);
        CHECK(band_bgra && band_rgb);
    }
    /* Reject invalid/unsupported RGB sinks without touching either output. */
    {
        l2d_surface_t target={rgb,L2D_LIGHT_CANVAS_W,L2D_LIGHT_CANVAS_H,
            L2D_LIGHT_CANVAS_W*2,rgb_bytes,L2D_PIXEL_RGB565_LE};
        memset(rgb,0xa5,rgb_bytes); memset(bgra,0xa5,bgra_bytes);
        uint32_t rgb_guard=(uint32_t)crc32(0,rgb,(uInt)rgb_bytes);
        uint32_t bgra_guard=(uint32_t)crc32(0,bgra,(uInt)bgra_bytes);
        if (!l2d_instance_can_render_rgb565(instance))
            CHECK(l2d_instance_render_rgb565(instance,&target,0,-20,&surface)==L2D_ERR_UNSUPPORTED);
        l2d_surface_t bad=target; ++bad.stride_bytes;
        CHECK(l2d_instance_render_rgb565(instance,&bad,0,-20,NULL)==L2D_ERR_RANGE);
        bad=target; bad.width=8193;
        CHECK(l2d_instance_render_rgb565(instance,&bad,0,-20,NULL)==L2D_ERR_RANGE);
        bad=target; bad.buffer_size_bytes=rgb_bytes-1;
        CHECK(l2d_instance_render_rgb565(instance,&bad,0,-20,NULL)==L2D_ERR_RANGE);
        l2d_surface_t overlap=surface; overlap.data=(char *)rgb+2;
        CHECK(l2d_instance_render_rgb565(instance,&target,0,-20,&overlap)==L2D_ERR_RANGE);
        CHECK((uint32_t)crc32(0,rgb,(uInt)rgb_bytes)==rgb_guard &&
              (uint32_t)crc32(0,bgra,(uInt)bgra_bytes)==bgra_guard);
    }
    uint32_t rest_crc = 0; int changed = 0;
    uint32_t alloc_before = allocator.alloc_calls;
    allocator.trap = 1;
    for (int pose = 0; pose < L2D_LIGHT_CRC_POSES; ++pose) {
        char name[48];
        CHECK(l2d_light_correctness_pose(instance, handles, pose, name, sizeof(name)));
        memset(bgra, 0, bgra_bytes);
        CHECK(l2d_pipeline_frame(instance, &surface, 0, -20, 33) == L2D_OK);
        CHECK(l2d_convert_bgra_to_rgb565(bgra, L2D_LIGHT_CANVAS_W * 4, bgra_bytes,
              rgb, L2D_LIGHT_CANVAS_W * 2, rgb_bytes, L2D_LIGHT_CANVAS_W,
              L2D_LIGHT_CANVAS_H) == L2D_OK);
        if (bands) {
            l2d_surface_t target={band_rgb,L2D_LIGHT_CANVAS_W,L2D_LIGHT_CANVAS_H,
                L2D_LIGHT_CANVAS_W*2,rgb_bytes,L2D_PIXEL_RGB565_LE};
            l2d_surface_t capture={band_bgra,L2D_LIGHT_CANVAS_W,L2D_LIGHT_CANVAS_H,
                L2D_LIGHT_CANVAS_W*4,bgra_bytes,L2D_PIXEL_BGRA8888_LE};
            memset(band_bgra,0xa5,bgra_bytes); memset(band_rgb,0xa5,rgb_bytes);
            CHECK(l2d_instance_render_rgb565(instance,&target,0,-20,&capture)==L2D_OK);
            CHECK(!memcmp(bgra,band_bgra,bgra_bytes) && !memcmp(rgb,band_rgb,rgb_bytes));
            memset(band_rgb,0xa5,rgb_bytes);
            CHECK(l2d_instance_render_rgb565(instance,&target,0,-20,NULL)==L2D_OK);
            CHECK(!memcmp(rgb,band_rgb,rgb_bytes));
        }
        uint32_t argb_crc = (uint32_t)crc32(0, bgra, (uInt)bgra_bytes);
        uint32_t rgb_crc = (uint32_t)crc32(0, rgb, (uInt)rgb_bytes);
        if (pose == 0) rest_crc = argb_crc;
        else if (argb_crc != rest_crc) ++changed;
        printf("LIGHT_CRC %s %08x %08x\n", name, argb_crc, rgb_crc);
        if (csv) fprintf(csv, "%s,%08x,%08x,1,1,1\n", name, argb_crc, rgb_crc);
    }
    CHECK(changed >= 13);
    memset(bgra, 0, bgra_bytes);
    CHECK(l2d_pipeline_frame(other, &surface, 0, -20, 33) == L2D_OK);
    CHECK((uint32_t)crc32(0, bgra, (uInt)bgra_bytes) == rest_crc);
    l2d_instance_reset_rt30(instance);
    memset(bgra, 0, bgra_bytes);
    CHECK(l2d_pipeline_frame(instance, &surface, 0, -20, 33) == L2D_OK);
    CHECK((uint32_t)crc32(0, bgra, (uInt)bgra_bytes) == rest_crc);
    CHECK(allocator.alloc_calls == alloc_before);
    l2d_instance_info_t instance_info;
    l2d_instance_info(instance, &instance_info);
    printf("LIGHT_MEM model_reserved=%zu model_used=%zu instance_reserved=%zu instance_used=%zu\n",
        info.pool_bytes, info.pool_bytes-info.pool_free, instance_info.pool_bytes,
        instance_info.pool_bytes-instance_info.pool_free);
done:
    allocator.trap = 0;
    if (csv) fclose(csv);
    if (file) fclose(file);
    free(wire); free(bgra); free(rgb); free(band_bgra); free(band_rgb);
    l2d_instance_destroy(other); l2d_instance_destroy(instance); l2d_model_destroy(model);
    if (!result) puts("L2D_LIGHT_TESTS_OK");
    return result;
}
