#include "l2d_allocator.h"

#include "l2d_pe_port.h"

#include <string.h>

void l2d_allocator_resolve(const l2d_allocator_t *in, l2d_allocator_t *out)
{
    if (!out) {
        return;
    }
    if (in && in->alloc && in->free) {
        *out = *in;
        return;
    }
    out->alloc = l2d_port_alloc;
    out->free = l2d_port_free;
    out->user = NULL;
}

void *l2d_heap_alloc(const l2d_allocator_t *allocator, l2d_memory_class_t cls, size_t alignment,
                     size_t size)
{
    l2d_allocator_t resolved;
    l2d_allocator_resolve(allocator, &resolved);
    if (!resolved.alloc || size == 0) {
        return NULL;
    }
    return resolved.alloc(resolved.user, cls, alignment, size);
}

void l2d_heap_free(const l2d_allocator_t *allocator, l2d_memory_class_t cls, void *ptr)
{
    l2d_allocator_t resolved;
    if (!ptr) {
        return;
    }
    l2d_allocator_resolve(allocator, &resolved);
    if (resolved.free) {
        resolved.free(resolved.user, cls, ptr);
    }
}
