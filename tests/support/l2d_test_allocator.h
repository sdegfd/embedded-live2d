#ifndef L2D_TEST_ALLOCATOR_H
#define L2D_TEST_ALLOCATOR_H

#include "l2d/l2d_memory.h"

#include <stdint.h>

typedef struct l2d_test_allocator {
    l2d_allocator_t allocator;
    uint32_t alloc_calls;
    uint32_t free_calls;
    int trap;
} l2d_test_allocator_t;

void l2d_test_allocator_init(l2d_test_allocator_t *allocator);

#endif
