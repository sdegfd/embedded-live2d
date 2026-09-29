#include "l2d/l2d.h"
#include "l2d_pe_port.h"
#include "live2d_engine_internal.h"

#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define FOUR_AXIS_SHA "786b18e342f3a7b3e67042822f138824aaad610d6c46130f58f5c59bbb379b0b"
#define FOUR_AXIS_SIZE 798660u
#define POOL_BYTES (16u * 1024u * 1024u)

static int g_fails;

static void expect_true(int cond, const char *msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", msg);
        g_fails++;
    }
}

static int read_file(const char *path, void **data, size_t *size)
{
    FILE *f = fopen(path, "rb");
    long n;
    void *buf;
    if (!f) {
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    n = ftell(f);
    if (n < 0) {
        fclose(f);
        return -1;
    }
    rewind(f);
    buf = malloc((size_t)n);
    if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        fclose(f);
        return -1;
    }
    fclose(f);
    *data = buf;
    *size = (size_t)n;
    return 0;
}

static void sha256_hex(const void *data, size_t size, char out[65])
{
    unsigned char raw[SHA256_DIGEST_LENGTH];
    int i;
    SHA256((const unsigned char *)data, size, raw);
    for (i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        sprintf(out + i * 2, "%02x", raw[i]);
    }
    out[64] = 0;
}

static uint32_t crc_pixels(const void *pixels, int w, int h)
{
    return (uint32_t)crc32(0L, (const Bytef *)pixels, (uInt)((size_t)w * (size_t)h * 4u));
}

static l2d_surface_t tight_surface(void *pixels, int w, int h)
{
    l2d_surface_t s;
    memset(&s, 0, sizeof(s));
    s.data = pixels;
    s.width = w;
    s.height = h;
    s.stride_bytes = w * 4;
    s.buffer_size_bytes = (size_t)w * (size_t)h * 4u;
    s.format = L2D_PIXEL_BGRA8888_LE;
    return s;
}

static void set_all(l2d_instance_t *instance, const int *handles, int n, float pos)
{
    int i;
    for (i = 0; i < n; ++i) {
        if (handles[i] >= 0) {
            l2d_instance_set_axis_position(instance, handles[i], pos, 32767);
        }
    }
}

static uint32_t render_pose(l2d_instance_t *instance, void *pixels, int w, int h, const char *name,
                            uint32_t *hit, uint32_t *miss)
{
    l2d_surface_t surface = tight_surface(pixels, w, h);
    uint32_t crc;
    memset(pixels, 0, (size_t)w * (size_t)h * 4u);
    expect_true(l2d_pipeline_frame(instance, &surface, 0, 0, 33) == L2D_OK, name);
    crc = crc_pixels(pixels, w, h);
    l2d_instance_trig_cache(instance, hit, miss);
    printf("POSE %s crc=%08x hit=%u miss=%u\n", name, crc, hit ? *hit : 0, miss ? *miss : 0);
    return crc;
}

static void test_pixels(void)
{
    uint32_t red = 0xFFFF0000u;
    uint32_t red_clear = 0x00FF0000u;
    uint32_t red_half = 0x80FF0000u;
    uint32_t blue = 0xFF0000FFu;
    uint8_t src[16];
    uint8_t dst[8];
    uint32_t bgra;
    uint16_t px;
    l2d_status_t status;
    expect_true(l2d_bgra8888_to_rgb565(red) == 0xF800, "opaque red");
    expect_true(l2d_bgra8888_to_rgb565(red) == l2d_bgra8888_to_rgb565(red_clear), "alpha 0 ignored");
    expect_true(l2d_bgra8888_to_rgb565(red) == l2d_bgra8888_to_rgb565(red_half), "alpha 128 ignored");
    expect_true(l2d_bgra8888_to_rgb565(blue) == 0x001F, "opaque blue");
    bgra = red;
    memcpy(src, &bgra, 4);
    bgra = blue;
    memcpy(src + 8, &bgra, 4);
    status = l2d_convert_bgra_to_rgb565(src, 8, sizeof(src), dst, 4, sizeof(dst), 1, 2);
    expect_true(status == L2D_OK, "padded convert");
    memcpy(&px, dst, 2);
    expect_true(px == 0xF800, "row0 red");
    memcpy(&px, dst + 4, 2);
    expect_true(px == 0x001F, "row1 blue");
    expect_true(l2d_convert_bgra_to_rgb565(src, 4, 4, dst, 2, 2, 1, 2) == L2D_ERR_RANGE,
                "short buffer");
}

static void test_roi(void)
{
    l2d_output_t *a = NULL;
    l2d_output_t *b = NULL;
    l2d_geometry_bounds_t bounds;
    l2d_output_view_t view;
    l2d_roi_rect_t current, conversion;
    int force = 0;
    l2d_roi_rect_t rect;
    expect_true(l2d_output_create(&a) == L2D_OK, "output a");
    expect_true(l2d_output_create(&b) == L2D_OK, "output b");
    memset(&bounds, 0, sizeof(bounds));
    bounds.valid = 1;
    bounds.min_x = 10.2f;
    bounds.min_y = 20.2f;
    bounds.max_x = 30.1f;
    bounds.max_y = 40.1f;
    rect = l2d_roi_from_geometry(&bounds, 320, 320);
    expect_true(rect.valid && rect.x == 8 && rect.y == 18, "floor-2 origin");
    expect_true(rect.w == (34 - 8) && rect.h == (44 - 18), "ceil+3 extent");
    memset(&view, 0, sizeof(view));
    view.canvas_w = 320;
    view.canvas_h = 240;
    view.target = a;
    view.conversion_enabled = 1;
    expect_true(l2d_output_plan(a, &view, &bounds, &current, &conversion, &force) == L2D_OK,
                "first plan");
    expect_true(force && conversion.w == 320 && conversion.h == 240, "first frame full");
    l2d_output_commit(a, &view, current);
    bounds.min_x = 12.f;
    bounds.max_x = 40.f;
    expect_true(l2d_output_plan(a, &view, &bounds, &current, &conversion, &force) == L2D_OK,
                "second plan");
    expect_true(!force && conversion.valid && conversion.w < 320, "union is not full");
    expect_true(l2d_output_plan(b, &view, &bounds, &current, &conversion, &force) == L2D_OK,
                "other target");
    expect_true(force, "history is not global");
    l2d_output_invalidate(a);
    expect_true(l2d_output_plan(a, &view, &bounds, &current, &conversion, &force) == L2D_OK,
                "after invalidate");
    expect_true(force, "invalidate restores full");
    bounds.unsafe = 1;
    l2d_output_commit(a, &view, current);
    expect_true(l2d_output_plan(a, &view, &bounds, &current, &conversion, &force) == L2D_OK,
                "unsafe plan");
    expect_true(force, "unsafe is full");
    l2d_output_destroy(a);
    l2d_output_destroy(b);
}

static void test_model(const char *path)
{
    void *model = NULL;
    size_t size = 0;
    char hex[65];
    l2d_model_t *model_obj = NULL;
    l2d_model_t *bad_model = NULL;
    l2d_instance_t *a = NULL;
    l2d_instance_t *b = NULL;
    l2d_instance_info_t info;
    l2d_share_view_t model_share;
    l2d_share_view_t share_a;
    l2d_share_view_t share_b;
    uint32_t crc_b_before;
    int handles[5];
    const char *ids[] = {"left_eye", "right_eye", "neck", "face", "mouth"};
    void *pixels;
    int w, h, i;
    uint32_t hit = 0, miss = 0, hit_b = 0, miss_b = 0;
    uint32_t crc_static, crc_145, crc_29, crc_neck, crc_again;
    uint32_t calls;
    l2d_surface_t bad;
    unsigned char truncated[64];
    expect_true(read_file(path, &model, &size) == 0, "read fixture");
    if (!model) {
        return;
    }
    sha256_hex(model, size, hex);
    expect_true(size == FOUR_AXIS_SIZE, "4-axis size");
    expect_true(strcmp(hex, FOUR_AXIS_SHA) == 0, "4-axis sha256");
    printf("MODEL sha=%s size=%zu\n", hex, size);

    memcpy(truncated, model, sizeof(truncated));
    expect_true(l2d_model_load_memory(truncated, sizeof(truncated), &model_obj) != L2D_OK,
                "truncated");
    expect_true(model_obj == NULL, "truncated not loaded");
    expect_true(l2d_model_load_memory(model, size, &model_obj) == L2D_OK, "load model");
    expect_true(l2d_instance_create(model_obj, &a) == L2D_OK, "create a");
    expect_true(l2d_instance_enter_rt30(a), "enter a");
    l2d_instance_info(a, &info);
    printf("INFO %dx%d layers=%d textures=%d anim=%d axes=%d\n", info.width, info.height,
           info.layer_count, info.texture_count, info.animation_count, info.axis_count);
    expect_true(info.width == 320 && info.height == 320, "canvas");
    expect_true(info.layer_count == 11 && info.axis_count == 4, "4-axis topology");
    w = info.width;
    h = info.height;
    for (i = 0; i < 5; ++i) {
        handles[i] = l2d_instance_find_axis(a, ids[i]);
    }
    expect_true(handles[0] == 0 && handles[1] == 1 && handles[2] == 2 && handles[3] == 3, "handles");
    expect_true(handles[4] < 0, "mouth absent");

    pixels = calloc((size_t)w * (size_t)h, 4u);
    expect_true(pixels != NULL, "pixels");
    if (!pixels) {
        l2d_instance_destroy(a);
        l2d_model_destroy(model_obj);
        free(model);
        return;
    }

    l2d_instance_reset_rt30(a);
    crc_static = render_pose(a, pixels, w, h, "STATIC", &hit, &miss);
    expect_true(crc_static == 0x6793bec7u && hit == 29 && miss == 22, "STATIC");

    l2d_instance_reset_rt30(a);
    set_all(a, handles, 5, 14.5f);
    crc_145 = render_pose(a, pixels, w, h, "ALL_14_5", &hit, &miss);
    expect_true(crc_145 == 0xd522cbf1u && hit == 29 && miss == 22, "ALL_14_5");

    l2d_instance_reset_rt30(a);
    set_all(a, handles, 5, 29.f);
    crc_29 = render_pose(a, pixels, w, h, "ALL_29", &hit, &miss);
    expect_true(crc_29 == 0x1033faa1u && hit == 29 && miss == 22, "ALL_29");

    l2d_instance_reset_rt30(a);
    l2d_instance_set_axis_position(a, handles[2], 15.f, 32767);
    crc_neck = render_pose(a, pixels, w, h, "NECK_15", &hit, &miss);
    expect_true(crc_neck == 0x6793bec7u && hit == 29 && miss == 22, "NECK_15");

    l2d_instance_update(a, 33);
    l2d_instance_trig_cache(a, &hit, &miss);
    {
        l2d_surface_t surface = tight_surface(pixels, w, h);
        uint32_t hit2 = 0, miss2 = 0;
        memset(pixels, 0, (size_t)w * (size_t)h * 4u);
        expect_true(l2d_instance_render_current(a, &surface, 0, 0) == L2D_OK, "render current");
        expect_true(l2d_instance_render_current(a, &surface, 0, 0) == L2D_OK, "render current again");
        l2d_instance_trig_cache(a, &hit2, &miss2);
        expect_true(hit2 == hit && miss2 == miss, "render does not retouch trig cache");
    }

    bad = tight_surface(pixels, w, h);
    bad.stride_bytes = w * 4 + 4;
    expect_true(l2d_pipeline_frame(a, &bad, 0, 0, 33) == L2D_ERR_UNSUPPORTED, "loose stride");
    bad = tight_surface(pixels, 160, 80);
    bad.buffer_size_bytes = 160u * 80u * 4u;
    memset(pixels, 0, bad.buffer_size_bytes);
    expect_true(l2d_pipeline_frame(a, &bad, 0, 0, 33) == L2D_OK, "nonsquare");

    calls = l2d_port_alloc_calls();
    {
        l2d_surface_t surface = tight_surface(pixels, w, h);
        int frame;
        for (frame = 0; frame < 3; ++frame) {
            expect_true(l2d_pipeline_frame(a, &surface, 0, 0, 33) == L2D_OK, "steady frame");
        }
    }
    expect_true(l2d_port_alloc_calls() == calls, "steady state zero alloc");

    expect_true(l2d_instance_create(model_obj, &b) == L2D_OK, "create b");
    expect_true(l2d_instance_enter_rt30(b), "enter b");
    l2d_model_share_view(model_obj, &model_share);
    l2d_instance_share_view(a, &share_a);
    l2d_instance_share_view(b, &share_b);
    printf("MEM model_bytes=%zu instance_a_bytes=%zu instance_b_bytes=%zu\n",
           model_share.pool_used, share_a.pool_used, share_b.pool_used);
    expect_true(model_share.texture_pixels &&
                    model_share.texture_pixels == share_a.texture_pixels &&
                    share_a.texture_pixels == share_b.texture_pixels,
                "shared texture");
    expect_true(model_share.triangle_indices &&
                    model_share.triangle_indices == share_a.triangle_indices &&
                    share_a.triangle_indices == share_b.triangle_indices,
                "shared triangles");
    expect_true(model_share.rt30_samples && model_share.rt30_samples == share_a.rt30_samples &&
                    share_a.rt30_samples == share_b.rt30_samples,
                "shared rt30 samples");
    expect_true(model_share.animation_frame &&
                    model_share.animation_frame == share_a.animation_frame &&
                    share_a.animation_frame == share_b.animation_frame,
                "shared animation");
    expect_true(share_a.mutable_vertices && share_b.mutable_vertices &&
                    share_a.mutable_vertices != share_b.mutable_vertices &&
                    share_a.mutable_vertices != model_share.mutable_vertices,
                "private vertices");
    expect_true(share_a.pool_used * 2u < model_share.pool_used, "instance a is not a model copy");
    expect_true(share_b.pool_used * 2u < model_share.pool_used, "instance b is not a model copy");
    l2d_instance_reset_rt30(b);
    crc_b_before = render_pose(b, pixels, w, h, "B_STATIC", &hit_b, &miss_b);
    expect_true(crc_b_before == 0x6793bec7u, "b static");
    l2d_instance_reset_rt30(a);
    set_all(a, handles, 5, 14.5f);
    crc_again = render_pose(a, pixels, w, h, "A_AGAIN", &hit, &miss);
    expect_true(crc_again == crc_145, "instance a repeats");
    crc_again = render_pose(b, pixels, w, h, "B_UNTOUCHED", &hit_b, &miss_b);
    expect_true(crc_again == crc_b_before, "a pose did not change b");
    l2d_instance_reset_rt30(b);
    l2d_instance_set_axis_position(b, 2, 15.f, 32767);
    render_pose(b, pixels, w, h, "B_NECK", &hit_b, &miss_b);
    l2d_instance_trig_cache(a, &hit, &miss);
    {
        uint32_t hit_a = 0, miss_a = 0;
        l2d_instance_trig_cache(a, &hit_a, &miss_a);
        expect_true(hit_a == 29 && miss_a == 22, "b did not wipe a");
    }
    crc_again = render_pose(a, pixels, w, h, "A_AFTER_B", &hit, &miss);
    expect_true(crc_again == crc_145, "a pose survived b");

    expect_true(l2d_model_load_memory(truncated, sizeof(truncated), &bad_model) != L2D_OK,
                "reload bad");
    expect_true(bad_model == NULL, "failed load creates nothing");
    expect_true(l2d_instance_is_loaded(a) && l2d_instance_is_loaded(b), "live model remains");
    l2d_instance_reset_rt30(b);
    crc_again = render_pose(b, pixels, w, h, "B_AFTER_BAD_LOAD", &hit_b, &miss_b);
    expect_true(crc_again == crc_b_before, "failed load keeps b");

    l2d_port_alloc_trap(1);
    {
        l2d_model_t *c = NULL;
        l2d_instance_t *d = NULL;
        expect_true(l2d_model_load_memory(model, size, &c) == L2D_ERR_NO_MEM, "alloc trap");
        expect_true(c == NULL, "trap creates nothing");
        expect_true(l2d_instance_create(model_obj, &d) == L2D_ERR_NO_MEM, "instance trap");
        expect_true(d == NULL, "trap instance is null");
    }
    l2d_port_alloc_trap(0);

    l2d_instance_destroy(a);
    l2d_instance_destroy(b);
    l2d_model_destroy(model_obj);
    free(pixels);
    free(model);
}

static void test_release_live(const char *path)
{
    void *bytes = NULL;
    size_t size = 0;
    char hex[65];
    l2d_model_t *model = NULL;
    l2d_instance_t *a = NULL;
    l2d_instance_t *b = NULL;
    l2d_model_info_t info;
    l2d_share_view_t model_share;
    l2d_share_view_t share_a;
    l2d_share_view_t share_b;
    if (!path || read_file(path, &bytes, &size) != 0) {
        fprintf(stderr, "FAIL release.live missing\n");
        g_fails++;
        return;
    }
    sha256_hex(bytes, size, hex);
    expect_true(size == 3747332u, "release size");
    expect_true(strcmp(hex, "5cfd67979fb365cc9edbca7b955aac391ebb39bca062fa855e4209faa02498b1") == 0,
                "release sha");
    expect_true(l2d_model_load_memory(bytes, size, &model) == L2D_OK, "release load");
    l2d_model_info(model, &info);
    printf("RELEASE layers=%d textures=%d anim=%d axes=%d\n", info.layer_count, info.texture_count,
           info.animation_count, info.axis_count);
    expect_true(info.layer_count == 34 && info.texture_count == 54 && info.animation_count == 32 &&
                    info.axis_count == 0,
                "release topology");
    expect_true(l2d_instance_create(model, &a) == L2D_OK, "release instance a");
    expect_true(l2d_instance_create(model, &b) == L2D_OK, "release instance b");
    expect_true(l2d_instance_enter_rt30(a) && l2d_instance_enter_rt30(b), "release enter");
    l2d_model_share_view(model, &model_share);
    l2d_instance_share_view(a, &share_a);
    l2d_instance_share_view(b, &share_b);
    printf("RELEASE_MEM model_bytes=%zu instance_a_bytes=%zu instance_b_bytes=%zu\n",
           model_share.pool_used, share_a.pool_used, share_b.pool_used);
    expect_true(model_share.texture_pixels && model_share.texture_pixels == share_a.texture_pixels &&
                    share_a.texture_pixels == share_b.texture_pixels,
                "release shared texture");
    expect_true(share_a.pool_used * 2u < model_share.pool_used, "release instance is smaller");
    l2d_instance_destroy(a);
    l2d_instance_destroy(b);
    l2d_model_destroy(model);
    free(bytes);
}

int main(void)
{
    const char *path = L2D_FIXTURE_4AXIS;
    test_pixels();
    test_roi();
    test_model(path);
    test_release_live(L2D_RELEASE_LIVE);
    if (g_fails) {
        fprintf(stderr, "%d failure(s)\n", g_fails);
        return 1;
    }
    printf("L2D_RUNTIME_TESTS_OK\n");
    return 0;
}
