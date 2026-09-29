#ifndef L2D_ALLOCATOR_INTERNAL_H
#define L2D_ALLOCATOR_INTERNAL_H

#include "l2d/l2d_memory.h"

void l2d_allocator_resolve(const l2d_allocator_t *in, l2d_allocator_t *out);
void *l2d_heap_alloc(const l2d_allocator_t *allocator, l2d_memory_class_t cls, size_t alignment,
                     size_t size);
void l2d_heap_free(const l2d_allocator_t *allocator, l2d_memory_class_t cls, void *ptr);

#endif
