#define _POSIX_C_SOURCE 200112L

#include "l2d_test_allocator.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static size_t test_align(size_t alignment)
{
    size_t value = alignment < sizeof(void *) ? sizeof(void *) : alignment;
    size_t power = 1;
    while (power < value) {
        power <<= 1;
        if (power == 0) {
            return sizeof(void *);
        }
    }
    return power;
}

static void *test_alloc(void *user, l2d_memory_class_t cls, size_t alignment, size_t size)
{
    l2d_test_allocator_t *allocator = (l2d_test_allocator_t *)user;
    void *ptr = NULL;
    (void)cls;
    if (allocator) {
        allocator->alloc_calls++;
        if (allocator->trap) {
            return NULL;
        }
    }
    if (size == 0 || posix_memalign(&ptr, test_align(alignment), size) != 0) {
        return NULL;
    }
    return ptr;
}

static void test_free(void *user, l2d_memory_class_t cls, void *ptr)
{
    l2d_test_allocator_t *allocator = (l2d_test_allocator_t *)user;
    (void)cls;
    if (!ptr) {
        return;
    }
    if (allocator) {
        allocator->free_calls++;
    }
    free(ptr);
}

void l2d_test_allocator_init(l2d_test_allocator_t *allocator)
{
    if (!allocator) {
        return;
    }
    memset(allocator, 0, sizeof(*allocator));
    allocator->allocator.alloc = test_alloc;
    allocator->allocator.free = test_free;
    allocator->allocator.user = allocator;
}
