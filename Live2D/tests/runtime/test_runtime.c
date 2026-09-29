#include "l2d/l2d.h"
#include "live2d_engine_internal.h"
#include "l2d_test_allocator.h"

#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

/* Formal baseline: worktree models/esp.live, same file as /sdcard/esp.live. */
#define L2D_BASELINE_SHA "1d7f21471dcedee2d205904791df7147c63760269462ad5d7169a97afe386100"
#define L2D_BASELINE_SIZE 800796u
#define L2D_FORBIDDEN_4AXIS_SHA "786b18e342f3a7b3e67042822f138824aaad610d6c46130f58f5c59bbb379b0b"
#define L2D_FORBIDDEN_RELEASE_SHA "5cfd67979fb365cc9edbca7b955aac391ebb39bca062fa855e4209faa02498b1"
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

static void test_arena(void)
{
    l2d_test_allocator_t heap;
    l2d_arena_t *arena = NULL;
    uint32_t calls;
    void *first;
    void *second;
    l2d_test_allocator_init(&heap);
    expect_true(l2d_arena_create(&heap.allocator, L2D_MEM_SCRATCH, 256, &arena) == L2D_OK, "arena");
    expect_true(heap.alloc_calls == 2, "arena reserves once");
    first = l2d_arena_alloc(arena, 16, 32);
    second = l2d_arena_alloc(arena, 8, 24);
    expect_true(first && second && first != second, "arena bump");
    calls = heap.alloc_calls;
    expect_true(l2d_arena_alloc(arena, 4, 16) != NULL, "arena still inside");
    expect_true(heap.alloc_calls == calls, "bump does not allocate");
    expect_true(l2d_arena_alloc(arena, 8, 1024) == NULL, "arena full");
    l2d_arena_reset(arena);
    expect_true(l2d_arena_used(arena) == 0, "arena reset");
    l2d_arena_destroy(arena);
    expect_true(heap.free_calls == 2, "arena freed");
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
    l2d_test_allocator_t heap;
    l2d_model_config_t model_cfg;
    l2d_instance_config_t instance_cfg;
    l2d_memory_requirements_t requirements;
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
    uint32_t hit_145 = 0, miss_145 = 0;
    uint32_t calls;
    l2d_surface_t bad;
    unsigned char truncated[64];
    expect_true(read_file(path, &model, &size) == 0, "read fixture");
    if (!model) {
        return;
    }
    sha256_hex(model, size, hex);
    printf("MODEL_BASELINE path=models/esp.live device_path=/sdcard/esp.live sha256=%s size=%zu\n",
           hex, size);
    if (strcmp(hex, L2D_FORBIDDEN_4AXIS_SHA) == 0 || strcmp(hex, L2D_FORBIDDEN_RELEASE_SHA) == 0) {
        fprintf(stderr,
                "STOP formal test: %s is not the five-axis esp.live baseline. Not switching models.\n",
                hex);
        g_fails++;
        free(model);
        return;
    }
    expect_true(size == L2D_BASELINE_SIZE && strcmp(hex, L2D_BASELINE_SHA) == 0,
                "five-axis esp.live sha256");
    if (size != L2D_BASELINE_SIZE || strcmp(hex, L2D_BASELINE_SHA) != 0) {
        fprintf(stderr, "STOP formal test: models/esp.live sha/size does not match the baseline. "
                        "Not switching models.\n");
        free(model);
        return;
    }

    l2d_test_allocator_init(&heap);
    model_cfg.allocator = &heap.allocator;
    instance_cfg.allocator = &heap.allocator;
    memcpy(truncated, model, sizeof(truncated));
    expect_true(l2d_model_load_memory(&model_cfg, truncated, sizeof(truncated), &model_obj) != L2D_OK,
                "truncated");
    expect_true(model_obj == NULL, "truncated not loaded");
    expect_true(l2d_model_load_memory(&model_cfg, model, size, &model_obj) == L2D_OK, "load model");
    expect_true(l2d_instance_create(model_obj, &instance_cfg, &a) == L2D_OK, "create a");
    expect_true(l2d_instance_enter_rt30(a), "enter a");
    l2d_instance_info(a, &info);
    printf("INFO %dx%d layers=%d textures=%d anim=%d axes=%d\n", info.width, info.height,
           info.layer_count, info.texture_count, info.animation_count, info.axis_count);
    expect_true(info.width == 320 && info.height == 320, "canvas");
    expect_true(info.layer_count == 11 && info.texture_count == 12 && info.axis_count == 5,
                "five-axis topology");
    expect_true(info.animation_count == 1, "timeline animation imported");
    expect_true(l2d_instance_play_animation(a, 0), "timeline play");
    expect_true(l2d_instance_playback_mode(a) == L2D_PLAYBACK_TIMELINE,
                "timeline mode");
    expect_true(l2d_instance_current_animation(a) == 0, "timeline index");
    l2d_instance_update(a, 16);
    l2d_instance_pause_animation(a);
    expect_true(l2d_instance_playback_mode(a) == L2D_PLAYBACK_TIMELINE,
                "timeline pause preserves mode");
    expect_true(l2d_instance_resume_animation(a), "timeline resume");
    l2d_instance_stop_animation(a);
    expect_true(l2d_instance_playback_mode(a) == L2D_PLAYBACK_NEUTRAL,
                "timeline stop returns neutral");
    expect_true(!l2d_instance_play_animation(a, 1), "invalid animation rejected");
    w = info.width;
    h = info.height;
    for (i = 0; i < 5; ++i) {
        handles[i] = l2d_instance_find_axis(a, ids[i]);
    }
    expect_true(handles[0] >= 0 && handles[1] >= 0 && handles[2] >= 0 && handles[3] >= 0 &&
                    handles[4] >= 0,
                "five axis ids");
    {
        l2d_mesh_counts_t geo;
        l2d_instance_mesh_counts(a, &geo);
        printf("MODEL_BASELINE axis_count=%d layer_count=%d vertex_count=%u triangle_count=%u "
               "texture_count=%d\n",
               info.axis_count, info.layer_count, geo.vertices, geo.triangles, info.texture_count);
        expect_true(geo.vertices == 227 && geo.triangles == 257, "mesh counts");
    }

    pixels = calloc((size_t)w * (size_t)h, 4u);
    expect_true(pixels != NULL, "pixels");
    if (!pixels) {
        l2d_instance_destroy(a);
        l2d_model_destroy(model_obj);
        free(model);
        return;
    }

    {
        l2d_surface_t surface = tight_surface(pixels, w, h);
        expect_true(l2d_instance_play_animation(a, 0), "timeline restart");
        l2d_instance_update(a, 33);
        expect_true(l2d_instance_render_current(a, &surface, 0, 0) == L2D_OK,
                    "timeline render");
        expect_true(crc_pixels(pixels, w, h) != 0, "timeline visible frame");
        l2d_instance_stop_animation(a);
    }

    expect_true(l2d_instance_enter_rt30(a), "enter RT30 after timeline");
    l2d_instance_reset_rt30(a);
    expect_true(l2d_instance_playback_mode(a) == L2D_PLAYBACK_RT30,
                "RT30 mode after transition");
    crc_static = render_pose(a, pixels, w, h, "STATIC", &hit, &miss);
    expect_true(crc_static == 0x6793bec7u && hit == 29 && miss == 22, "STATIC");

    l2d_instance_reset_rt30(a);
    set_all(a, handles, 5, 14.5f);
    crc_145 = render_pose(a, pixels, w, h, "ALL_14_5", &hit_145, &miss_145);
    expect_true(crc_145 == 0x540d2664u && hit_145 == 29 && miss_145 == 22, "ALL_14_5");

    l2d_instance_reset_rt30(a);
    set_all(a, handles, 5, 29.f);
    crc_29 = render_pose(a, pixels, w, h, "ALL_29", &hit, &miss);
    expect_true(crc_29 == 0x7a64b4adu && hit == 29 && miss == 22, "ALL_29");

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

    calls = heap.alloc_calls;
    {
        l2d_surface_t surface = tight_surface(pixels, w, h);
        int frame;
        for (frame = 0; frame < 3; ++frame) {
            expect_true(l2d_pipeline_frame(a, &surface, 0, 0, 33) == L2D_OK, "steady frame");
        }
    }
    expect_true(heap.alloc_calls == calls, "steady state zero alloc");

    expect_true(l2d_instance_create(model_obj, &instance_cfg, &b) == L2D_OK, "create b");
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
    l2d_model_memory_requirements(model_obj, &requirements);
    expect_true(requirements.persistent_immutable.minimum == model_share.pool_used, "model minimum");
    expect_true(requirements.persistent_immutable.upper_bound >= requirements.persistent_immutable.minimum,
                "model upper");
    expect_true(requirements.persistent_immutable.upper_bound < 4u * 1024u * 1024u,
                "formal model reserves under 4 MiB");
    expect_true(requirements.per_instance_mutable.upper_bound >= share_a.pool_used, "instance reserve");
    l2d_instance_memory_requirements(a, &requirements);
    expect_true(requirements.per_instance_mutable.minimum == share_a.pool_used, "instance used");
    expect_true(requirements.renderer_scratch.minimum == 0, "no renderer scratch");
    l2d_instance_reset_rt30(b);
    crc_b_before = render_pose(b, pixels, w, h, "B_STATIC", &hit_b, &miss_b);
    expect_true(crc_b_before == crc_static, "b static");
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
        expect_true(hit_a == hit_145 && miss_a == miss_145, "b did not wipe a");
    }
    crc_again = render_pose(a, pixels, w, h, "A_AFTER_B", &hit, &miss);
    expect_true(crc_again == crc_145, "a pose survived b");

    {
        l2d_model_t *fresh_model = NULL;
        l2d_instance_t *fresh_instance = NULL;
        expect_true(l2d_model_load_memory(&model_cfg, model, size, &fresh_model) == L2D_OK,
                    "reload while original model lives");
        if (fresh_model) {
            expect_true(l2d_instance_create(fresh_model, &instance_cfg, &fresh_instance) == L2D_OK,
                        "reload instance");
            if (fresh_instance) {
                expect_true(l2d_instance_enter_rt30(fresh_instance), "reload RT30");
                crc_again = render_pose(fresh_instance, pixels, w, h, "RELOAD_STATIC", &hit, &miss);
                expect_true(crc_again == crc_static, "reload pixels");
            }
        }
        l2d_instance_destroy(fresh_instance);
        l2d_model_destroy(fresh_model);
    }

    expect_true(l2d_model_load_memory(&model_cfg, truncated, sizeof(truncated), &bad_model) != L2D_OK,
                "reload bad");
    expect_true(bad_model == NULL, "failed load creates nothing");
    expect_true(l2d_instance_is_loaded(a) && l2d_instance_is_loaded(b), "live model remains");
    l2d_instance_reset_rt30(b);
    crc_again = render_pose(b, pixels, w, h, "B_AFTER_BAD_LOAD", &hit_b, &miss_b);
    expect_true(crc_again == crc_b_before, "failed load keeps b");

    heap.trap = 1;
    {
        l2d_model_t *c = NULL;
        l2d_instance_t *d = NULL;
        expect_true(l2d_model_load_memory(&model_cfg, model, size, &c) == L2D_ERR_NO_MEM, "alloc trap");
        expect_true(c == NULL, "trap creates nothing");
        expect_true(l2d_instance_create(model_obj, &instance_cfg, &d) == L2D_ERR_NO_MEM, "instance trap");
        expect_true(d == NULL, "trap instance is null");
    }
    heap.trap = 0;

    l2d_instance_destroy(a);
    l2d_instance_destroy(b);
    l2d_model_destroy(model_obj);
    free(pixels);
    free(model);
}

static void write_u32(unsigned char *p, uint32_t value)
{
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
    p[2] = (unsigned char)(value >> 16);
    p[3] = (unsigned char)(value >> 24);
}

static void write_i32(unsigned char *p, int32_t value)
{
    write_u32(p, (uint32_t)value);
}

static void expect_load_error(const void *bytes, size_t size, l2d_status_t want, const char *msg)
{
    l2d_model_t *model = NULL;
    l2d_status_t status = l2d_model_load_memory(NULL, bytes, size, &model);
    expect_true(status == want, msg);
    expect_true(model == NULL, msg);
    l2d_model_destroy(model);
}

static unsigned char *live_header(uint32_t version, int32_t width, int32_t height, int32_t layers,
                                  int32_t animations, int32_t textures, size_t *size)
{
    unsigned char *bytes = calloc(1, 80);
    expect_true(bytes != NULL, "header bytes");
    if (!bytes) {
        return NULL;
    }
    memcpy(bytes, "PainterEngineLiveDBinary", 24);
    memcpy(bytes + 24, "esp", 3);
    write_u32(bytes + 56, version);
    write_i32(bytes + 60, width);
    write_i32(bytes + 64, height);
    write_i32(bytes + 68, layers);
    write_i32(bytes + 72, animations);
    write_i32(bytes + 76, textures);
    *size = 80;
    return bytes;
}

static void write_layer(unsigned char *p, int32_t parent, int32_t triangles, int32_t vertices)
{
    int i;
    memset(p, 0, 124);
    memcpy(p, "layer", 5);
    write_i32(p + 32, parent);
    for (i = 0; i < 16; ++i) {
        write_i32(p + 36 + i * 4, -1);
    }
    write_i32(p + 100, triangles);
    write_i32(p + 104, vertices);
    write_i32(p + 120, -1);
}

static void test_format_corpus(const char *model_path)
{
    size_t size = 0;
    unsigned char *bytes;
    unsigned char short_magic[8] = {0};
    void *model = NULL;
    live2d_engine_t *engine = NULL;
    live2d_engine_info_t info;

    bytes = live_header(99, 320, 320, 0, 0, 0, &size);
    expect_load_error(bytes, size, L2D_ERR_VERSION, "version");
    free(bytes);
    bytes = live_header(1, 0, 320, 0, 0, 0, &size);
    expect_load_error(bytes, size, L2D_ERR_FORMAT, "zero width");
    free(bytes);
    bytes = live_header(1, 100000, 320, 0, 0, 0, &size);
    expect_load_error(bytes, size, L2D_ERR_FORMAT, "huge canvas");
    free(bytes);
    bytes = live_header(1, 320, 320, -1, 0, 0, &size);
    expect_load_error(bytes, size, L2D_ERR_FORMAT, "negative layers");
    free(bytes);
    expect_load_error(short_magic, sizeof(short_magic), L2D_ERR_FORMAT, "truncated header");

    bytes = live_header(1, 320, 320, 0, 0, 1, &size);
    bytes = realloc(bytes, 80 + 48);
    memset(bytes + 80, 0, 48);
    write_i32(bytes + 80 + 32, 2);
    write_i32(bytes + 80 + 36, 2);
    expect_load_error(bytes, 80 + 48, L2D_ERR_FORMAT, "truncated texture");
    free(bytes);

    bytes = live_header(1, 320, 320, 0, 0, 1, &size);
    bytes = realloc(bytes, 80 + 48);
    memset(bytes + 80, 0, 48);
    write_i32(bytes + 80 + 32, 0x40000000);
    write_i32(bytes + 80 + 36, 4);
    expect_load_error(bytes, 80 + 48, L2D_ERR_OVERFLOW, "texture overflow");
    free(bytes);

    bytes = live_header(1, 320, 320, 1, 0, 0, &size);
    bytes = realloc(bytes, 80 + 124);
    write_layer(bytes + 80, 0, 0, 0);
    expect_load_error(bytes, 80 + 124, L2D_ERR_CORRUPT, "self parent");
    free(bytes);

    bytes = live_header(1, 320, 320, 2, 0, 0, &size);
    bytes = realloc(bytes, 80 + 248);
    write_layer(bytes + 80, 1, 0, 0);
    write_layer(bytes + 80 + 124, 0, 0, 0);
    expect_load_error(bytes, 80 + 248, L2D_ERR_CORRUPT, "parent cycle");
    free(bytes);

    bytes = live_header(1, 320, 320, 1, 0, 0, &size);
    bytes = realloc(bytes, 80 + 124 + 12 + 96);
    memset(bytes + 80, 0, 124 + 12 + 96);
    write_layer(bytes + 80, -1, 1, 1);
    write_i32(bytes + 80 + 124, 9);
    expect_load_error(bytes, 80 + 124 + 12 + 96, L2D_ERR_CORRUPT, "bad triangle");
    free(bytes);

    bytes = live_header(1, 320, 320, 1, 0, 0, &size);
    bytes = realloc(bytes, 80 + 124 + 96);
    memset(bytes + 80, 0, 124 + 96);
    write_layer(bytes + 80, -1, 0, 1);
    write_u32(bytes + 80 + 124 + 88, 0x7fc00000u);
    expect_load_error(bytes, 80 + 124 + 96, L2D_ERR_CORRUPT, "nan vertex");
    free(bytes);

    bytes = live_header(1, 320, 320, 0, 0, 0, &size);
    bytes = realloc(bytes, 84);
    memcpy(bytes + 80, "RT30", 4);
    expect_load_error(bytes, 84, L2D_ERR_CORRUPT, "bad rt30");
    free(bytes);

    expect_true(read_file(model_path, &model, &size) == 0, "baseline for reload");
    if (!model) {
        return;
    }
    expect_true(live2d_engine_create(NULL, L2D_MEM_MODEL, 16u * 1024u * 1024u, &engine) == L2D_OK,
                "engine create");
    expect_true(live2d_engine_load(engine, model, size) == L2D_OK, "engine load");
    expect_true(live2d_engine_load(engine, short_magic, sizeof(short_magic)) == L2D_ERR_FORMAT,
                "failed reload");
    expect_true(live2d_engine_is_loaded(engine), "previous model remains");
    live2d_engine_get_info(engine, &info);
    expect_true(info.layer_count == 11 && info.width == 320, "previous topology");
    expect_true(live2d_engine_get_realtime_axis_count(engine) == 5, "previous axes");
    live2d_engine_destroy(engine);
    free(model);
}

int main(void)
{
    const char *path = L2D_MODEL_ESP_LIVE;
    test_pixels();
    test_arena();
    test_roi();
    test_format_corpus(path);
    test_model(path);
    if (g_fails) {
        fprintf(stderr, "%d failure(s)\n", g_fails);
        return 1;
    }
    printf("L2D_RUNTIME_TESTS_OK\n");
    return 0;
}
