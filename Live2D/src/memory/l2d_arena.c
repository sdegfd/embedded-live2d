#include "l2d/l2d_memory.h"

#include "l2d_allocator.h"

#include <string.h>

struct l2d_arena {
    uint8_t *base;
    size_t capacity;
    size_t offset;
    l2d_allocator_t allocator;
    l2d_memory_class_t cls;
};

l2d_status_t l2d_arena_create(const l2d_allocator_t *allocator, l2d_memory_class_t cls,
                              size_t capacity, l2d_arena_t **out)
{
    l2d_arena_t *arena;
    l2d_allocator_t resolved;
    if (!out || capacity == 0) {
        return L2D_ERR_INVALID_ARG;
    }
    *out = NULL;
    l2d_allocator_resolve(allocator, &resolved);
    arena = (l2d_arena_t *)l2d_heap_alloc(&resolved, cls, sizeof(void *), sizeof(*arena));
    if (!arena) {
        return L2D_ERR_NO_MEM;
    }
    memset(arena, 0, sizeof(*arena));
    arena->allocator = resolved;
    arena->cls = cls;
    arena->base = (uint8_t *)l2d_heap_alloc(&resolved, cls, sizeof(void *), capacity);
    if (!arena->base) {
        l2d_heap_free(&resolved, cls, arena);
        return L2D_ERR_NO_MEM;
    }
    arena->capacity = capacity;
    *out = arena;
    return L2D_OK;
}

void l2d_arena_destroy(l2d_arena_t *arena)
{
    l2d_allocator_t allocator;
    l2d_memory_class_t cls;
    if (!arena) {
        return;
    }
    allocator = arena->allocator;
    cls = arena->cls;
    l2d_heap_free(&allocator, cls, arena->base);
    l2d_heap_free(&allocator, cls, arena);
}

void *l2d_arena_alloc(l2d_arena_t *arena, size_t alignment, size_t size)
{
    size_t align;
    size_t start;
    size_t next;
    if (!arena || !arena->base || size == 0) {
        return NULL;
    }
    align = alignment < 1 ? 1 : alignment;
    start = arena->offset;
    if (align > 1) {
        size_t rem = start % align;
        if (rem) {
            start += align - rem;
        }
    }
    if (start > arena->capacity || size > arena->capacity - start) {
        return NULL;
    }
    next = start + size;
    arena->offset = next;
    return arena->base + start;
}

void l2d_arena_reset(l2d_arena_t *arena)
{
    if (arena) {
        arena->offset = 0;
    }
}

size_t l2d_arena_used(const l2d_arena_t *arena)
{
    return arena ? arena->offset : 0;
}
