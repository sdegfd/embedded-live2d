/**
 * @file live2d_engine.c
 * @brief Live2D PainterEngine封装层实现（待集成）
 *
 * ── 渲染管线位置 ──
 *   PainterEngine -> live2d_engine（动画/模型/着色器） -> live2d_renderer（格式转换） -> 显示缓冲区
 *
 * 本模块实现了基于PainterEngine的Live2D模型引擎封装。
 * 负责在PSRAM中分配内存池、导入Live2D模型、管理动画播放，
 * 并提供像素着色器优化和渲染接口。
 *
 * @note 待集成：此模块尚未与 Game UI 框架完全集成，当前为独立运行的 Live2D 演示模式。
 */

#include "live2d_engine.h"
#include "l2d_format.h"

#include "PX_LiveFramework.h"
#include "l2d_allocator.h"
#include "l2d_pe_port.h"
#include "live2d_engine_diag.h"

#include <stdlib.h>
#include <string.h>

/* ── 内部数据结构 ────────────────────────────────── */

/** Live2D引擎上下文结构体（不透明类型，对外隐藏实现细节） */
struct live2d_engine {
    l2d_allocator_t allocator;
    l2d_memory_class_t mem_class;
    void *pool_mem;
    size_t pool_size;
    px_memorypool pool;
    PX_LiveFramework live;
    bool loaded;
    uint32_t roi_revision;
};

static const char *TAG = "live2d_engine";

_Static_assert(LIVE2D_ENGINE_ID_MAX == PX_LIVE_ID_MAX_LEN, "id capacity");
_Static_assert(sizeof(((live2d_engine_frame_profile_t *)0)->poseUs) == sizeof(px_dword), "profile unit");

/* ── 内部辅助函数 ────────────────────────────────── */

/** 将渲染缩放比例限制在[0.1, 1.0]范围内。 */
static float live2d_engine_clamp_render_scale(float render_scale)
{
    if (render_scale < 0.1f) {
        return 0.1f;
    }
    if (render_scale > 1.0f) {
        return 1.0f;
    }
    return render_scale;
}

/** 查找下一个可播放的动画索引，跳过空帧列表的动画。 */
static int live2d_engine_find_next_playable_animation(const live2d_engine_t *engine,
                                                      int start_index)
{
    if (!engine || !engine->loaded) {
        return -1;
    }

    int animation_count = engine->live.liveAnimations.size;
    if (animation_count <= 0) {
        return -1;
    }

    if (start_index < 0) {
        start_index = 0;
    }

    for (int offset = 0; offset < animation_count; ++offset) {
        int animation_index = (start_index + offset) % animation_count;
        PX_LiveAnimation *animation =
            PX_VECTORAT(PX_LiveAnimation, &engine->live.liveAnimations, animation_index);
        if (animation && animation->framesMemPtr.size > 0) {
            return animation_index;
        }
    }

    return -1;
}

/** 播放指定索引的动画。先重置框架状态，再播放新动画。 */
bool live2d_engine_play_index(live2d_engine_t *engine, int animation_index)
{
    if (!engine || !engine->loaded) {
        return false;
    }

    if (animation_index < 0 || animation_index >= engine->live.liveAnimations.size) {
        return false;
    }

    PX_LiveAnimation *animation =
        PX_VECTORAT(PX_LiveAnimation, &engine->live.liveAnimations, animation_index);
    if (!animation || animation->framesMemPtr.size <= 0) {
        return false;
    }

    PX_LiveFrameworkReset(&engine->live);
    return PX_LiveFrameworkPlayAnimation(&engine->live, animation_index) == PX_TRUE;
}

void live2d_engine_pause_animation(live2d_engine_t *engine)
{
    if (engine && engine->loaded && engine->live.animationMode == PX_LIVE_MODE_TIMELINE) {
        PX_LiveFrameworkPause(&engine->live);
    }
}

bool live2d_engine_resume_animation(live2d_engine_t *engine)
{
    if (!engine || !engine->loaded || engine->live.animationMode != PX_LIVE_MODE_TIMELINE) {
        return false;
    }
    PX_LiveFrameworkPlay(&engine->live);
    return true;
}

void live2d_engine_stop_animation(live2d_engine_t *engine)
{
    if (engine && engine->loaded && engine->live.animationMode == PX_LIVE_MODE_TIMELINE) {
        PX_LiveFrameworkStop(&engine->live);
    }
}

int live2d_engine_playback_mode(const live2d_engine_t *engine)
{
    return engine && engine->loaded ? (int)engine->live.animationMode : (int)PX_LIVE_MODE_NEUTRAL;
}

/** 快速像素着色器：实现带混合模式的ARGB像素渲染，支持透明度混合。
 *
 * 该着色器替代 PainterEngine 默认像素着色器，针对 ESP32-P4 优化。
 * 执行逐像素的纹理采样、混合模式调制和 Alpha 预乘合成。
 * 不执行耗时的浮点数 gamma 校正，以性能为首要目标。
 *
 * @param surface   目标渲染表面
 * @param x,y       像素在表面的目标坐标
 * @param position  顶点位置（本实现中未使用）
 * @param u,v       纹理采样坐标 [0,1]
 * @param normal    法线（本实现中未使用）
 * @param texture   源纹理
 * @param blend     混合参数（含透明度及 HDR 增益），可为 NULL
 */
static void live2d_engine_fast_pixel_shader(px_surface *surface, px_int x, px_int y,
                                            px_point position, px_float u, px_float v,
                                            px_point normal, px_texture *texture,
                                            PX_TEXTURERENDER_BLEND *blend)
{
    (void)position;
    (void)normal;
    px_color *dst;

    if (!surface || !texture) {
        return;
    }
    if (x > surface->limit_right || x < surface->limit_left ||
        y > surface->limit_bottom || y < surface->limit_top) {
        return;
    }
    if (u < 0 || u > 1 || v < 0 || v > 1) {
        return;
    }

    px_int tx = (u >= 1.0f) ? (texture->width - 1) : (px_int)(u * texture->width);
    px_int ty = (v >= 1.0f) ? (texture->height - 1) : (px_int)(v * texture->height);
    px_color color = PX_SURFACECOLOR(texture, tx, ty);

    if (blend) {
        px_int a = (px_int)(color._argb.a * blend->alpha);
        px_int r = (px_int)(color._argb.r * blend->hdr_R);
        px_int g = (px_int)(color._argb.g * blend->hdr_G);
        px_int b = (px_int)(color._argb.b * blend->hdr_B);

        color._argb.a = (px_uchar)((a > 255) ? 255 : ((a < 0) ? 0 : a));
        color._argb.r = (px_uchar)((r > 255) ? 255 : ((r < 0) ? 0 : r));
        color._argb.g = (px_uchar)((g > 255) ? 255 : ((g < 0) ? 0 : g));
        color._argb.b = (px_uchar)((b > 255) ? 255 : ((b < 0) ? 0 : b));
    }

    if (color._argb.a == 0) {
        return;
    }

    dst = &surface->surfaceBuffer[x + surface->width * y];
    if (color._argb.a == 0xff) {
        *dst = color;
        return;
    }

    dst->_argb.r = (px_uchar)(((256 - color._argb.a) * dst->_argb.r +
                               color._argb.r * (color._argb.a + 1)) >> 8);
    dst->_argb.g = (px_uchar)(((256 - color._argb.a) * dst->_argb.g +
                               color._argb.g * (color._argb.a + 1)) >> 8);
    dst->_argb.b = (px_uchar)(((256 - color._argb.a) * dst->_argb.b +
                               color._argb.b * (color._argb.a + 1)) >> 8);
    dst->_argb.a = 255 - (((256 - dst->_argb.a) * (255 - color._argb.a)) >> 8);
}

/* ── 公开API ────────────────────────────────── */

/** 创建引擎对象和运行时内存池。分配器由调用方传入；NULL 使用端口默认分配器。
 *
 * @param pool_size   内存池大小（字节）
 * @param out_engine  输出参数，指向新创建的引擎实例指针
 * @return L2D_OK 成功；L2D_ERR_INVALID_ARG 参数无效；L2D_ERR_NO_MEM 内存不足
 */
l2d_status_t live2d_engine_create(const l2d_allocator_t *allocator, l2d_memory_class_t cls,
                                 size_t pool_size, live2d_engine_t **out_engine)
{
    l2d_allocator_t resolved;
    live2d_engine_t *engine;
    if (!out_engine || pool_size == 0) {
        return L2D_ERR_INVALID_ARG;
    }
    l2d_allocator_resolve(allocator, &resolved);

    engine = l2d_heap_alloc(&resolved, cls, sizeof(void *), sizeof(*engine));
    if (!engine) {
        L2D_LOGE(TAG, "Failed to allocate engine context");
        return L2D_ERR_NO_MEM;
    }
    memset(engine, 0, sizeof(*engine));
    engine->allocator = resolved;
    engine->mem_class = cls;

    engine->pool_mem = l2d_heap_alloc(&engine->allocator, cls, 64, pool_size);
    if (!engine->pool_mem) {
        L2D_LOGE(TAG, "Failed to allocate runtime pool, class=%d need %u bytes", (int)cls,
                 (unsigned)pool_size);
        l2d_heap_free(&engine->allocator, cls, engine);
        return L2D_ERR_NO_MEM;
    }

    engine->pool_size = pool_size;
    engine->pool = MP_Create(engine->pool_mem, (px_uint)pool_size);
    MP_NoCatchError(&engine->pool);

    L2D_LOGI(TAG, "allocated runtime pool: %u bytes", (unsigned)pool_size);
    *out_engine = engine;
    return L2D_OK;
}

static size_t live2d_add_size(size_t a, size_t b, int *ok)
{
    if (!*ok || a > (size_t)-1 - b) {
        *ok = 0;
        return 0;
    }
    return a + b;
}

static size_t live2d_vector_payload(const px_vector *vec, int *ok)
{
    size_t count;
    size_t node;
    if (!*ok || !vec || vec->size <= 0 || vec->nodesize <= 0) {
        return 0;
    }
    count = (size_t)vec->size;
    node = (size_t)vec->nodesize;
    if (count > (size_t)-1 / node) {
        *ok = 0;
        return 0;
    }
    return count * node;
}

static size_t live2d_instance_pool_size(const PX_LiveFramework *live, int *ok)
{
    size_t bytes = (256u * 1024u) + (size_t)PX_LIVE_REALTIME_RUNTIME_BUDGET_BYTES;
    int i;
    bytes = live2d_add_size(bytes, live2d_vector_payload(&live->layers, ok), ok);
    bytes = live2d_add_size(bytes, live2d_vector_payload(&live->livetextures, ok), ok);
    bytes = live2d_add_size(bytes, live2d_vector_payload(&live->liveAnimations, ok), ok);
    for (i = 0; i < live->layers.size; ++i) {
        const PX_LiveLayer *layer = PX_VECTORAT(PX_LiveLayer, &live->layers, i);
        bytes = live2d_add_size(bytes, live2d_vector_payload(&layer->vertices, ok), ok);
        bytes = live2d_add_size(bytes, 256u, ok);
    }
    for (i = 0; i < live->liveAnimations.size; ++i) {
        const PX_LiveAnimation *animation =
            PX_VECTORAT(PX_LiveAnimation, &live->liveAnimations, i);
        bytes = live2d_add_size(bytes, live2d_vector_payload(&animation->framesMemPtr, ok), ok);
        bytes = live2d_add_size(bytes, 256u, ok);
    }
    if (!*ok || bytes > ((size_t)-1) / 2u) {
        *ok = 0;
        return 0;
    }
    bytes *= 2u;
    if (bytes < (1024u * 1024u)) {
        bytes = 1024u * 1024u;
    }
    return (bytes + 63u) & ~(size_t)63u;
}

size_t live2d_engine_mutable_reserve(const live2d_engine_t *engine)
{
    int ok = 1;
    size_t bytes;
    if (!engine || !engine->loaded) {
        return 0;
    }
    bytes = live2d_instance_pool_size(&engine->live, &ok);
    return ok ? bytes : 0;
}

const l2d_allocator_t *live2d_engine_allocator(const live2d_engine_t *engine)
{
    return engine ? &engine->allocator : NULL;
}

static int live2d_copy_vector_payload(px_memorypool *mp, px_vector *dest, const px_vector *src)
{
    px_int count = src ? src->size : 0;
    px_int node = src ? src->nodesize : 0;
    if (!src) {
        return 0;
    }
    if (node <= 0) {
        memset(dest, 0, sizeof(*dest));
        return count <= 0;
    }
    if (!PX_VectorInitialize(mp, dest, node, count > 0 ? count : 0)) {
        return 0;
    }
    if (count > 0) {
        memcpy(dest->data, src->data, (size_t)count * (size_t)node);
        dest->size = count;
    }
    return 1;
}

l2d_status_t live2d_engine_clone_shared(const live2d_engine_t *source,
                                        const l2d_allocator_t *allocator,
                                        live2d_engine_t **out_engine)
{
    int ok = 1;
    int initialized = 0;
    size_t pool_size;
    int i;
    live2d_engine_t *engine;
    l2d_status_t status;
    if (!out_engine) {
        return L2D_ERR_INVALID_ARG;
    }
    *out_engine = NULL;
    if (!source || !source->loaded) {
        return L2D_ERR_INVALID_ARG;
    }
    pool_size = live2d_instance_pool_size(&source->live, &ok);
    if (!ok || pool_size == 0) {
        return L2D_ERR_FAIL;
    }
    status = live2d_engine_create(allocator ? allocator : &source->allocator, L2D_MEM_INSTANCE,
                                  pool_size, &engine);
    if (status != L2D_OK) {
        return status;
    }
    if (!PX_LiveFrameworkInitialize(&engine->pool, &engine->live, source->live.width,
                                    source->live.height)) {
        live2d_engine_destroy(engine);
        return L2D_ERR_NO_MEM;
    }
    initialized = 1;
    engine->live.sharesImmutablePayloads = PX_TRUE;
    engine->live.realtime.sharedBaked = PX_TRUE;
    memcpy(engine->live.id, source->live.id, sizeof(engine->live.id));
    engine->live.pixelShader = source->live.pixelShader;
    engine->live.fastNearestSampling = source->live.fastNearestSampling;
    engine->live.renderScale = source->live.renderScale;
    engine->live.showRange = PX_FALSE;
    engine->live.showKeypoint = PX_FALSE;
    engine->live.showlinker = PX_FALSE;
    engine->live.showFocusLayer = PX_FALSE;
    engine->live.showRootHelperLine = PX_FALSE;

    for (i = 0; i < source->live.layers.size; ++i) {
        const PX_LiveLayer *src_layer = PX_VECTORAT(PX_LiveLayer, &source->live.layers, i);
        PX_LiveLayer layer = *src_layer;
        PX_LiveLayer *dst_layer;
        memset(&layer.vertices, 0, sizeof(layer.vertices));
        memset(&layer.triangles, 0, sizeof(layer.triangles));
        layer.rel_rotationTrigValid = PX_FALSE;
        layer.rel_localRotationTrigValid = PX_FALSE;
        if (!PX_VectorPushback(&engine->live.layers, &layer)) {
            goto fail;
        }
        dst_layer = PX_VECTORLAST(PX_LiveLayer, &engine->live.layers);
        if (!live2d_copy_vector_payload(&engine->pool, &dst_layer->vertices, &src_layer->vertices)) {
            goto fail;
        }
        dst_layer->triangles = src_layer->triangles;
        dst_layer->triangles.mp = NULL;
    }

    for (i = 0; i < source->live.livetextures.size; ++i) {
        PX_LiveTexture texture = *PX_VECTORAT(PX_LiveTexture, &source->live.livetextures, i);
        texture.Texture.MP = NULL;
        if (!PX_VectorPushback(&engine->live.livetextures, &texture)) {
            goto fail;
        }
    }

    for (i = 0; i < source->live.liveAnimations.size; ++i) {
        const PX_LiveAnimation *src_animation =
            PX_VECTORAT(PX_LiveAnimation, &source->live.liveAnimations, i);
        PX_LiveAnimation animation;
        PX_LiveAnimation *dst_animation;
        memset(&animation, 0, sizeof(animation));
        memcpy(animation.id, src_animation->id, sizeof(animation.id));
        if (!PX_VectorPushback(&engine->live.liveAnimations, &animation)) {
            goto fail;
        }
        dst_animation = PX_VECTORLAST(PX_LiveAnimation, &engine->live.liveAnimations);
        if (!live2d_copy_vector_payload(&engine->pool, &dst_animation->framesMemPtr,
                                        &src_animation->framesMemPtr)) {
            goto fail;
        }
    }

    engine->live.realtime = source->live.realtime;
    engine->live.realtime.mp = &engine->pool;
    engine->live.realtime.sharedBaked = PX_TRUE;
    engine->live.realtime.runtimeBlock = NULL;
    engine->live.realtime.vertexOffsets = NULL;
    engine->live.realtime.layerAccumulators = NULL;
    engine->live.realtime.vertexAccumulators = NULL;
    engine->live.realtime.runtimeLayerCount = 0;
    engine->live.realtime.runtimeVertexCount = 0;
    engine->live.realtime.runtimeBytes = 0;
    engine->live.realtime.contributionValid = PX_FALSE;
    engine->live.realtime.dirty = PX_TRUE;
    memset(engine->live.realtime.appliedSample, 0, sizeof(engine->live.realtime.appliedSample));
    engine->live.trigCacheHit = 0;
    engine->live.trigCacheMiss = 0;
    engine->loaded = true;
    *out_engine = engine;
    return L2D_OK;

fail:
    if (initialized) {
        PX_LiveFrameworkFree(&engine->live);
    }
    live2d_engine_destroy(engine);
    return L2D_ERR_NO_MEM;
}

void live2d_engine_share_view(const live2d_engine_t *engine, live2d_engine_share_view_t *out)
{
    int i;
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (!engine || !engine->loaded) {
        return;
    }
    if (engine->pool.FreeSize <= engine->pool_size) {
        out->pool_used = engine->pool_size - (size_t)engine->pool.FreeSize;
    }
    if (engine->live.livetextures.size > 0) {
        const PX_LiveTexture *texture = PX_VECTORAT(PX_LiveTexture, &engine->live.livetextures, 0);
        out->texture_pixels = texture->Texture.surfaceBuffer;
    }
    for (i = 0; i < engine->live.layers.size; ++i) {
        const PX_LiveLayer *layer = PX_VECTORAT(PX_LiveLayer, &engine->live.layers, i);
        if (layer->vertices.data && layer->vertices.size > 0) {
            out->mutable_vertices = layer->vertices.data;
            break;
        }
    }
    for (i = 0; i < engine->live.layers.size; ++i) {
        const PX_LiveLayer *layer = PX_VECTORAT(PX_LiveLayer, &engine->live.layers, i);
        if (layer->triangles.data && layer->triangles.size > 0) {
            out->triangle_indices = layer->triangles.data;
            break;
        }
    }
    if (engine->live.realtime.axisCount > 0) {
        out->rt30_samples = engine->live.realtime.axes[0].samples;
    }
    if (engine->live.liveAnimations.size > 0) {
        const PX_LiveAnimation *animation =
            PX_VECTORAT(PX_LiveAnimation, &engine->live.liveAnimations, 0);
        if (animation->framesMemPtr.size > 0 && animation->framesMemPtr.data) {
            out->animation_frame = *PX_VECTORAT(px_void *, &animation->framesMemPtr, 0);
        }
    }
}

/** 销毁Live2D引擎实例，释放运行时内存池和上下文。
 *
 * @param engine  引擎实例指针（可为 NULL，此时函数无操作）
 */
void live2d_engine_destroy(live2d_engine_t *engine)
{
    if (!engine) {
        return;
    }
    if (engine->loaded) {
        PX_LiveFrameworkFree(&engine->live);
        engine->loaded = false;
    }
    if (engine->pool_mem) {
        l2d_heap_free(&engine->allocator, engine->mem_class, engine->pool_mem);
    }
    l2d_heap_free(&engine->allocator, engine->mem_class, engine);
}

/** 从内存数据加载模型。失败时保留原来已经加载的模型，不重置内存池。 */
l2d_status_t live2d_engine_load(live2d_engine_t *engine, const void *model_data, size_t model_size)
{
    PX_LiveFramework incoming;
    l2d_status_t status;
    if (!engine || !model_data || model_size == 0) {
        return L2D_ERR_INVALID_ARG;
    }
    memset(&incoming, 0, sizeof(incoming));
    L2D_LOGI(TAG, "Importing PainterEngine Live2D model, file size=%u bytes, pool free=%u bytes",
             (unsigned)model_size, (unsigned)engine->pool.FreeSize);
    status = l2d_format_import(&engine->pool, &incoming, model_data, model_size);
    if (status != L2D_OK) {
        L2D_LOGE(TAG, "import failed status=%d, pool free=%u bytes", (int)status,
                 (unsigned)engine->pool.FreeSize);
        return status;
    }
    if (engine->loaded) {
        PX_LiveFrameworkFree(&engine->live);
    }
    engine->live = incoming;
    engine->loaded = true;
    ++engine->roi_revision;

    /* 关闭 PainterEngine Live2D 的调试辅助线/关键点/连接线，仅保留模型本体渲染 */
    engine->live.showRange = PX_FALSE;
    engine->live.showKeypoint = PX_FALSE;
    engine->live.showlinker = PX_FALSE;
    engine->live.showFocusLayer = PX_FALSE;
    engine->live.showRootHelperLine = PX_FALSE;
    engine->live.pixelShader = live2d_engine_fast_pixel_shader;
    engine->live.fastNearestSampling = PX_TRUE;
    engine->live.renderScale = 1.0f;

    L2D_LOGI(TAG,
             "Model loaded: id='%.*s', size=%dx%d, layers=%d, textures=%d, animations=%d, pool free=%u bytes",
             PX_LIVE_ID_MAX_LEN, engine->live.id, engine->live.width, engine->live.height,
             engine->live.layers.size, engine->live.livetextures.size,
             engine->live.liveAnimations.size, (unsigned)engine->pool.FreeSize);
    L2D_LOGI(TAG, "Using nearest-neighbor pixel shader for ESP renderer");

    /* 报告 RT30 实时轴：必须明确打印轴数与轴 ID，防止静默忽略 RT30 尾部 */
    {
        int axis_count = (int)PX_LiveRealtimeGetAxisCount(&engine->live);
        int h;
        L2D_LOGI(TAG, "RT30 realtime axes: %d", axis_count);
        for (h = 0; h < axis_count; h++) {
            PX_LiveRealtimeAxis *axis = PX_LiveRealtimeGetAxis(&engine->live, h);
            if (axis) {
                L2D_LOGI(TAG,
                         "  axis[%d] id='%.*s' default=%u middle=%u bindings=%u sampleBytes=%u stride=%u vidx=%u Q(coord=%u rot=%u stretch=%u)",
                         h, PX_LIVE_REALTIME_AXIS_ID_MAX_LEN, axis->id,
                         axis->defaultSampleIndex, axis->middleKeyIndex,
                         axis->bindingCount, axis->sampleBytes, axis->sampleStride,
                         axis->vertexIndexCount,
                         axis->coordFractionBits, axis->rotationFractionBits,
                         axis->stretchFractionBits);
                for (int b = 0; b < axis->bindingCount; ++b) {
                    const PX_LiveRealtimeBinding *binding = &axis->bindings[b];
                    const PX_LiveLayer *layer =
                        (binding->layerIndex < engine->live.layers.size)
                            ? PX_VECTORAT(PX_LiveLayer, &engine->live.layers,
                                          binding->layerIndex)
                            : NULL;
                    L2D_LOGI(TAG,
                             "    binding[%d] layer=%u id='%.*s' mask=0x%04x vertices=%u sample_offset=%u",
                             b, binding->layerIndex, PX_LIVE_ID_MAX_LEN,
                             layer ? layer->id : "<invalid>", binding->propertyMask,
                             binding->vertexCount, (unsigned)binding->sampleOffset);
                }
            }
        }
    }

    live2d_engine_play_default(engine);
    return L2D_OK;
}

/** 设置内部渲染缩放比例，限制在[0.1, 1.0]之间。
 *
 * @param engine       引擎实例
 * @param render_scale 缩放比例（0.1 ~ 1.0），越小性能越高但画质越低
 */
void live2d_engine_set_render_scale(live2d_engine_t *engine, float render_scale)
{
    if (!engine || !engine->loaded) {
        return;
    }

    float next_scale=live2d_engine_clamp_render_scale(render_scale);
    if (engine->live.renderScale != next_scale) ++engine->roi_revision;
    engine->live.renderScale = next_scale;
    L2D_LOGI(TAG, "Internal render scale set to %.3f", (double)engine->live.renderScale);
}

/** 播放默认动画：查找第一个可播放的动画，若无则渲染静态模型。
 *
 * @param engine  引擎实例
 */
void live2d_engine_play_default(live2d_engine_t *engine)
{
    if (!engine || !engine->loaded) {
        return;
    }

    int default_animation = live2d_engine_find_next_playable_animation(engine, 0);
    if (default_animation >= 0) {
        if (live2d_engine_play_index(engine, default_animation)) {
            L2D_LOGI(TAG, "Playing default animation index %d/%d",
                     default_animation + 1, engine->live.liveAnimations.size);
            return;
        }
    }

    PX_LiveFrameworkReset(&engine->live);
    PX_LiveFrameworkPlay(&engine->live);
    L2D_LOGI(TAG, "No animation frame list found, rendering static model continuously");
}

bool live2d_engine_is_animation_finished(const live2d_engine_t *engine)
{
    return engine && engine->loaded &&
           engine->live.status == PX_LIVEFRAMEWORK_STATUS_STOP;
}

void live2d_engine_play_default_index(live2d_engine_t *engine, int animation_index)
{
    /* 播放指定 index 作为默认动画（如空闲动画 11）；失败回退到首个可播放动画。 */
    if (live2d_engine_play_index(engine, animation_index)) {
        L2D_LOGI(TAG, "Playing default animation index %d/%d",
                 animation_index + 1,
                 engine->live.liveAnimations.size);
        return;
    }
    live2d_engine_play_default(engine);
}

/** 当前动画停止时自动切换到下一个可播放动画，实现循环播放。
 *
 * @param engine  引擎实例
 * @return true   动画已切换；false 未切换或引擎无效
 */
bool live2d_engine_cycle_animation_if_needed(live2d_engine_t *engine)
{
    if (!engine || !engine->loaded) {
        return false;
    }

    /* 仅在动画播放完毕（STOP 状态）时才触发切换 */
    if (engine->live.status != PX_LIVEFRAMEWORK_STATUS_STOP) {
        return false;
    }

    int animation_count = engine->live.liveAnimations.size;
    if (animation_count <= 0) {
        /* 无动画列表时，以静态模式持续渲染模型 */
        PX_LiveFrameworkReset(&engine->live);
        PX_LiveFrameworkPlay(&engine->live);
        return true;
    }

    int current_animation = engine->live.reg_animation;
    int next_animation = live2d_engine_find_next_playable_animation(engine, current_animation + 1);
    if (next_animation < 0) {
        return false;
    }

    if (!live2d_engine_play_index(engine, next_animation)) {
        return false;
    }

    if (next_animation == current_animation) {
        L2D_LOGI(TAG, "Looping animation index %d/%d",
                 next_animation + 1, animation_count);
    } else {
        L2D_LOGI(TAG, "Switching animation %d -> %d/%d",
                 current_animation + 1, next_animation + 1, animation_count);
    }
    return true;
}

/** 获取当前正在播放的动画索引。
 *
 * @param engine  引擎实例
 * @return 当前动画索引（从 0 开始），无动画或参数无效时返回 -1
 */
int live2d_engine_get_current_animation_index(const live2d_engine_t *engine)
{
    if (!engine || !engine->loaded || engine->live.liveAnimations.size <= 0) {
        return -1;
    }

    if (engine->live.reg_animation < 0 ||
        engine->live.reg_animation >= engine->live.liveAnimations.size) {
        return -1;
    }

    return engine->live.reg_animation;
}

/** 渲染Live2D模型到指定的表面缓冲区。
 *
 * @param engine      引擎实例
 * @param surface     目标 PainterEngine 表面
 * @param x,y         在 surface 上的绘制坐标
 * @param align       对齐方式（通常为 PX_ALIGN_LEFTTOP）
 * @param elapsed_ms  距上次渲染的毫秒数，用于驱动动画进度
 */
static int live2d_engine_bind_surface(px_surface *surface, void *pixels, int width, int height)
{
    if (!surface || !pixels || width <= 0 || height <= 0) {
        return 0;
    }
    memset(surface, 0, sizeof(*surface));
    surface->surfaceBuffer = (px_color *)pixels;
    surface->width = width;
    surface->height = height;
    surface->limit_left = 0;
    surface->limit_top = 0;
    surface->limit_right = width - 1;
    surface->limit_bottom = height - 1;
    return 1;
}

void live2d_engine_render(live2d_engine_t *engine, void *pixels, int width, int height,
                          int x, int y, uint32_t elapsed_ms)
{
    px_surface surface;
    if (!engine || !engine->loaded || !live2d_engine_bind_surface(&surface, pixels, width, height)) {
        return;
    }
    PX_LiveFrameworkRender(&surface, &engine->live, x, y, PX_ALIGN_LEFTTOP, elapsed_ms);
}

void live2d_engine_update(live2d_engine_t *engine, uint32_t elapsed_ms)
{
    if (!engine || !engine->loaded) {
        return;
    }
    PX_LiveFrameworkUpdate(&engine->live, elapsed_ms);
}

void live2d_engine_render_current(live2d_engine_t *engine, void *pixels, int width, int height,
                                  int x, int y)
{
    px_surface surface;
    if (!engine || !engine->loaded || !live2d_engine_bind_surface(&surface, pixels, width, height)) {
        return;
    }
    PX_LiveFrameworkRenderCurrent(&surface, &engine->live, x, y, PX_ALIGN_LEFTTOP);
}

void live2d_engine_get_frame_profile(const live2d_engine_t *engine,
                                     live2d_engine_frame_profile_t *out_profile)
{
    if (!out_profile) return;
    memset(out_profile, 0, sizeof(*out_profile));
    if (engine && engine->loaded) {
        const PX_LiveFrameworkFrameProfile *src = &engine->live.frameProfile;
        out_profile->poseUs = src->poseUs;
        out_profile->physicalUs = src->physicalUs;
        out_profile->keypointUs = src->keypointUs;
        out_profile->visualTransformUs = src->visualTransformUs;
        out_profile->stretchUs = src->stretchUs;
        out_profile->vertexTransformUs = src->vertexTransformUs;
        out_profile->uvUpdateUs = src->uvUpdateUs;
        out_profile->sortUs = src->sortUs;
        out_profile->drawUs = src->drawUs;
    }
}

void live2d_engine_get_geometry_bounds(const live2d_engine_t *engine,
                                       l2d_geometry_bounds_t *out)
{
    PX_LiveGeometryBounds bounds;
    if (!out) return;
    memset(out, 0, sizeof(*out));
    memset(&bounds, 0, sizeof(bounds));
    PX_LiveFrameworkGetGeometryBounds(engine && engine->loaded ? &engine->live : NULL, &bounds);
    out->min_x = bounds.min_x;
    out->min_y = bounds.min_y;
    out->max_x = bounds.max_x;
    out->max_y = bounds.max_y;
    out->valid = bounds.valid ? 1 : 0;
    out->unsafe = bounds.unsafe ? 1 : 0;
}

uint32_t live2d_engine_get_roi_revision(const live2d_engine_t *engine)
{
    return engine ? engine->roi_revision : 0;
}

void live2d_engine_get_trig_cache(const live2d_engine_t *engine,
                                  uint32_t *hit, uint32_t *miss)
{
    px_dword h = 0, m = 0;
    PX_LiveFrameworkGetTrigCacheFrame(engine && engine->loaded ? &engine->live : NULL, &h, &m);
    if (hit) *hit = (uint32_t)h;
    if (miss) *miss = (uint32_t)m;
}

#if L2D_CFG_PROFILE_VISUAL
void live2d_engine_visual_diag_copy(live2d_engine_t *engine, PX_LiveVisualDiagFrame *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (engine && engine->loaded) PX_LiveFrameworkVisualDiagCopy(&engine->live, out);
}

int live2d_engine_visual_diag_topology(live2d_engine_t *engine, PX_LiveVisualDiagLayer *out,
                                       int capacity)
{
    if (!engine || !engine->loaded) return 0;
    return PX_LiveFrameworkVisualDiagTopology(&engine->live, out, capacity);
}
#endif

void live2d_engine_get_geometry(const live2d_engine_t *engine,
                                live2d_engine_geometry_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!engine || !engine->loaded) return;
    for (int i = 0; i < engine->live.layers.size; ++i) {
        const PX_LiveLayer *layer = PX_VECTORAT(PX_LiveLayer, &engine->live.layers, i);
        out->vertices += (uint32_t)layer->vertices.size;
        out->triangles += (uint32_t)layer->triangles.size;
    }
    for (int i = 0; i < engine->live.livetextures.size; ++i) {
        const PX_LiveTexture *texture = PX_VECTORAT(PX_LiveTexture, &engine->live.livetextures, i);
        out->texture_pixels += (uint32_t)texture->Texture.width * (uint32_t)texture->Texture.height;
    }
}

/** 获取引擎信息：模型尺寸、图层数、纹理数、动画数和内存池状态。
 *
 * @param engine    引擎实例
 * @param out_info  输出参数，填充引擎信息结构体
 */
void live2d_engine_get_info(live2d_engine_t *engine, live2d_engine_info_t *out_info)
{
    if (!engine || !out_info) {
        return;
    }

    memset(out_info, 0, sizeof(*out_info));
    if (engine->loaded) {
        memcpy(out_info->id, engine->live.id, sizeof(out_info->id));
        out_info->width = engine->live.width;
        out_info->height = engine->live.height;
        out_info->layer_count = engine->live.layers.size;
        out_info->texture_count = engine->live.livetextures.size;
        out_info->animation_count = engine->live.liveAnimations.size;
    }
    out_info->pool_size = engine->pool_size;
    out_info->pool_free = engine->pool.FreeSize;
}

/** 检查模型是否已成功加载。
 *
 * @param engine  引擎实例
 * @return true 模型已加载；false 未加载或引擎无效
 */
bool live2d_engine_is_loaded(const live2d_engine_t *engine)
{
    return engine && engine->loaded;
}

/* ── RT30 实时多轴接口实现 ────────────────────── */

bool live2d_engine_enter_realtime(live2d_engine_t *engine)
{
    if (!engine || !engine->loaded) {
        return false;
    }
    return PX_LiveEnterRealtime30(&engine->live) == PX_TRUE;
}

void live2d_engine_leave_realtime(live2d_engine_t *engine)
{
    if (!engine || !engine->loaded) {
        return;
    }
    PX_LiveLeaveRealtime30(&engine->live);
}

void live2d_engine_reset_realtime(live2d_engine_t *engine)
{
    if (!engine || !engine->loaded) {
        return;
    }
    ++engine->roi_revision;
    PX_LiveRealtimeResetAll(&engine->live);
}

int live2d_engine_get_realtime_axis_count(live2d_engine_t *engine)
{
    if (!engine || !engine->loaded) {
        return 0;
    }
    return (int)PX_LiveRealtimeGetAxisCount(&engine->live);
}

int live2d_engine_find_realtime_axis(live2d_engine_t *engine, const char *id)
{
    if (!engine || !engine->loaded || !id) {
        return -1;
    }
    return (int)PX_LiveFindRealtimeAxisById(&engine->live, id);
}

bool live2d_engine_set_axis_sample(live2d_engine_t *engine, int handle, uint8_t sample,
                                   uint16_t weight_q15)
{
    if (!engine || !engine->loaded || handle < 0) {
        return false;
    }
    return PX_LiveSetRealtimeAxisSample(&engine->live, handle, sample, weight_q15) == PX_TRUE;
}

bool live2d_engine_set_axis_position(live2d_engine_t *engine, int handle, float position,
                                     uint16_t weight_q15)
{
    if (!engine || !engine->loaded || handle < 0) return false;
    return PX_LiveRealtimeSetAxisPosition(&engine->live, handle, position, weight_q15) == PX_TRUE;
}

bool live2d_engine_set_axes_batch(live2d_engine_t *engine, const live2d_axis_state_t *states,
                                  size_t count)
{
    size_t i;
    if (!engine || !engine->loaded || (!states && count)) {
        return false;
    }
    for (i = 0; i < count; i++) {
        if (states[i].handle >= (int)PX_LiveRealtimeGetAxisCount(&engine->live) ||
            states[i].sample >= PX_LIVE_REALTIME_SAMPLE_COUNT) return false;
    }
    /* 先写全部轴的 sample/weight（仅置 dirty），求值在下次 render 时由
     * PX_LiveFrameworkRender 的 REALTIME30 分支统一触发一次，避免每轴刷新整套模型。 */
    for (i = 0; i < count; i++) {
        if (states[i].handle < 0) {
            continue;
        }
        if (!PX_LiveSetRealtimeAxisSample(&engine->live, states[i].handle, states[i].sample,
                                          states[i].weight_q15)) return false;
    }
    return true;
}

void live2d_engine_get_realtime_stats(live2d_engine_t *engine, live2d_realtime_stats_t *out)
{
    PX_LiveRealtimeMemoryStats stats;
    if (out) {
        memset(out, 0, sizeof(*out));
    }
    if (!engine || !engine->loaded || !out) {
        return;
    }
    PX_LiveRealtimeGetMemoryStats(&engine->live, &stats);
    out->axis_count = (int)stats.activeAxisCount;
    out->static_bytes = stats.staticBytes;
    out->runtime_bytes = stats.runtimeBytes;
    out->selected_sample_bytes = stats.selectedSampleBytes;
}
