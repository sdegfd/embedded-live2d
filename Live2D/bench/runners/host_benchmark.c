#include "l2d/l2d.h"
#include "l2d_scenarios.h"

#include <openssl/sha.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef L2D_MODEL_ESP_LIVE
#error L2D_MODEL_ESP_LIVE must point to the formal model
#endif

static const unsigned char model_sha[SHA256_DIGEST_LENGTH] = {
    0x1d,0x7f,0x21,0x47,0x1d,0xce,0xde,0xe2,0xd2,0x05,0x90,0x47,0x91,0xdf,0x71,0x47,
    0xc6,0x37,0x60,0x26,0x94,0x62,0xad,0x5d,0x71,0x69,0xa9,0x7a,0xfe,0x38,0x61,0x00
};

static uint64_t now_us(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000u + (uint64_t)t.tv_nsec / 1000u;
}

static int read_model(unsigned char **out, size_t *size)
{
    FILE *f = fopen(L2D_MODEL_ESP_LIVE, "rb");
    long bytes;
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) || (bytes = ftell(f)) != 800796 || fseek(f, 0, SEEK_SET)) {
        fclose(f);
        return 0;
    }
    *out = malloc((size_t)bytes);
    if (!*out || fread(*out, 1, (size_t)bytes, f) != (size_t)bytes) {
        free(*out);
        fclose(f);
        return 0;
    }
    fclose(f);
    *size = (size_t)bytes;
    unsigned char sha[SHA256_DIGEST_LENGTH];
    SHA256(*out, *size, sha);
    return memcmp(sha, model_sha, sizeof(sha)) == 0;
}

int main(void)
{
    static const struct { const char *name; int id; } scenes[] = {
        {"STATIC", L2D_BENCH_STATIC}, {"EYE_L_SWEEP", L2D_BENCH_EYE_L},
        {"MULTI_AXIS", L2D_BENCH_MULTI}
    };
    static const char *axis_ids[5] = {"left_eye", "right_eye", "neck", "face", "mouth"};
    unsigned char *file = NULL;
    size_t file_size = 0;
    l2d_model_t *model = NULL;
    l2d_instance_t *instance = NULL;
    uint32_t *pixels = NULL;
    int handles[5];
    int ok = 0;
    if (!read_model(&file, &file_size)) {
        fprintf(stderr, "formal five-axis models/esp.live identity mismatch\n");
        goto done;
    }
    if (l2d_model_load_memory(NULL, file, file_size, &model) != L2D_OK ||
        l2d_instance_create(model, NULL, &instance) != L2D_OK ||
        !l2d_instance_enter_rt30(instance)) goto done;
    l2d_instance_info_t info;
    l2d_instance_info(instance, &info);
    if (info.width != 320 || info.height != 320 || info.axis_count != 5) goto done;
    for (int i = 0; i < 5; ++i) {
        handles[i] = l2d_instance_find_axis(instance, axis_ids[i]);
        if (handles[i] < 0) goto done;
    }
    pixels = calloc(320u * 320u, 4u);
    if (!pixels) goto done;
    l2d_surface_t surface = {pixels, 320, 320, 320 * 4, 320u * 320u * 4u,
                             L2D_PIXEL_BGRA8888_LE};
    l2d_instance_set_render_scale(instance, L2D_PRESET_SHORT_SCALE);
    printf("preset,scenario,frames,total_avg_us,pose_avg_us,draw_avg_us,model_sha256\n");
    for (size_t s = 0; s < sizeof(scenes)/sizeof(scenes[0]); ++s) {
        uint64_t total = 0, pose = 0, draw = 0;
        l2d_instance_reset_rt30(instance);
        for (int frame = 0; frame < L2D_PRESET_SHORT_WARMUP + L2D_PRESET_SHORT_MEASURE;
             ++frame) {
            memset(pixels, 0, surface.buffer_size_bytes);
            if (!l2d_bench_drive(instance, handles, scenes[s].id, frame)) goto done;
            uint64_t start = now_us();
            if (l2d_pipeline_frame(instance, &surface, 0, 0, 33) != L2D_OK) goto done;
            uint64_t elapsed = now_us() - start;
            if (frame >= L2D_PRESET_SHORT_WARMUP) {
                l2d_frame_profile_t profile;
                l2d_instance_frame_profile(instance, &profile);
                total += elapsed;
                pose += profile.pose_us;
                draw += profile.draw_us;
            }
        }
        printf("%s,%s,%d,%.2f,%.2f,%.2f,1d7f21471dcedee2d205904791df7147c63760269462ad5d7169a97afe386100\n",
               L2D_PRESET_SHORT_NAME, scenes[s].name, L2D_PRESET_SHORT_MEASURE,
               (double)total/L2D_PRESET_SHORT_MEASURE,
               (double)pose/L2D_PRESET_SHORT_MEASURE,
               (double)draw/L2D_PRESET_SHORT_MEASURE);
    }
    ok = 1;
done:
    if (!ok) fprintf(stderr, "host benchmark failed\n");
    free(pixels);
    l2d_instance_destroy(instance);
    l2d_model_destroy(model);
    free(file);
    return ok ? 0 : 1;
}
