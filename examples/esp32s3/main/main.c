/**
 * ESP32-S3 software-port smoke test.
 * No display, pins, or PSRAM size are assumed. A passing boot log means the
 * shared runtime linked. It does not mean a panel was verified.
 */
#include "l2d/l2d.h"

#include <stdio.h>

void app_main(void)
{
    l2d_model_t *model = NULL;
    l2d_instance_t *instance = NULL;
    l2d_status_t status = l2d_model_load_memory(NULL, 0, &model);
    uint32_t red = 0xFFFF0000u;
    uint8_t src[4];
    uint8_t dst[2];
    if (status == L2D_OK || model != NULL) {
        printf("L2D_S3_SMOKE_FAIL null model was accepted\n");
        l2d_model_destroy(model);
        return;
    }
    status = l2d_instance_create(NULL, &instance);
    if (status == L2D_OK || instance != NULL) {
        printf("L2D_S3_SMOKE_FAIL null instance was accepted\n");
        l2d_instance_destroy(instance);
        return;
    }
    src[0] = 0x00;
    src[1] = 0x00;
    src[2] = 0xFF;
    src[3] = 0xFF;
    if (l2d_bgra8888_to_rgb565(red) != 0xF800 ||
        l2d_convert_bgra_to_rgb565(src, 4, sizeof(src), dst, 2, sizeof(dst), 1, 1) != L2D_OK) {
        printf("L2D_S3_SMOKE_FAIL rgb565\n");
        return;
    }
    printf("L2D_S3_SMOKE_OK\n");
}
