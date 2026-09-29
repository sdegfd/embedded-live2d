/**
 * Allocator and memory classes.
 *
 * The runtime core does not know about PSRAM, SRAM, or platform heap flags.
 * NULL config selects the linked port allocator.
 *
 * Bound fields:
 *   minimum     bytes retained by a live object
 *   recommended bytes the caller should expect to keep
 *   upper_bound arena size this runtime reserves from the allocator
 * Before an instance exists, per-instance minimum is 0 and upper_bound is
 * the arena that create will request. After create, minimum is the used size.
 * Loader temporary and renderer scratch are 0 after a successful load:
 * import space is returned to the model arena, and embedded-nearest does not
 * keep a second scratch buffer.
 */
#ifndef L2D_MEMORY_H
#define L2D_MEMORY_H

#include "l2d_types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    L2D_MEM_MODEL = 1,
    L2D_MEM_INSTANCE = 2,
    L2D_MEM_FAST = 3,
    L2D_MEM_LARGE = 4,
    L2D_MEM_SCRATCH = 5,
    L2D_MEM_FRAMEBUFFER = 6
} l2d_memory_class_t;

typedef void *(*l2d_alloc_fn)(void *user, l2d_memory_class_t cls, size_t alignment, size_t size);
typedef void (*l2d_free_fn)(void *user, l2d_memory_class_t cls, void *ptr);

typedef struct l2d_allocator {
    l2d_alloc_fn alloc;
    l2d_free_fn free;
    void *user;
} l2d_allocator_t;

typedef struct {
    size_t minimum;
    size_t recommended;
    size_t upper_bound;
} l2d_bytes_bound_t;

typedef struct {
    l2d_bytes_bound_t persistent_immutable;
    l2d_bytes_bound_t per_instance_mutable;
    l2d_bytes_bound_t loader_temporary;
    l2d_bytes_bound_t renderer_scratch;
} l2d_memory_requirements_t;

typedef struct {
    const l2d_allocator_t *allocator;
} l2d_model_config_t;

typedef struct {
    const l2d_allocator_t *allocator;
} l2d_instance_config_t;

/** One-shot bump arena. alloc does not call the system allocator. */
typedef struct l2d_arena l2d_arena_t;

l2d_status_t l2d_arena_create(const l2d_allocator_t *allocator, l2d_memory_class_t cls,
                              size_t capacity, l2d_arena_t **out);
void l2d_arena_destroy(l2d_arena_t *arena);
void *l2d_arena_alloc(l2d_arena_t *arena, size_t alignment, size_t size);
void l2d_arena_reset(l2d_arena_t *arena);
size_t l2d_arena_used(const l2d_arena_t *arena);

#ifdef __cplusplus
}
#endif

#endif
