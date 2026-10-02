#define _GNU_SOURCE
#include "l2d_pe_port.h"
#include "l2d_raster_nearest.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static size_t l2d_port_align(size_t alignment)
{
    size_t a = alignment < sizeof(void *) ? sizeof(void *) : alignment;
    size_t p = 1;
    while (p < a) {
        p <<= 1;
        if (p == 0) {
            return sizeof(void *);
        }
    }
    return p;
}

void *l2d_port_alloc(void *user, l2d_memory_class_t cls, size_t alignment, size_t size)
{
    void *ptr = NULL;
    (void)user;
    (void)cls;
    if (size == 0) {
        return NULL;
    }
    if (posix_memalign(&ptr, l2d_port_align(alignment), size) != 0) {
        return NULL;
    }
    return ptr;
}

void l2d_port_free(void *user, l2d_memory_class_t cls, void *ptr)
{
    (void)user;
    (void)cls;
    free(ptr);
}

void l2d_port_log(int level, const char *tag, const char *fmt, ...)
{
    va_list ap;
    const char *mark = level <= L2D_LOG_ERROR ? "E" : (level == L2D_LOG_WARN ? "W" : "I");
    va_start(ap, fmt);
    fprintf(stderr, "%s (%s) ", mark, tag ? tag : "l2d");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

void l2d_pe_log_info(const char *tag, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "I (%s) ", tag ? tag : "l2d");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

int64_t l2d_pe_time_us(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

void l2d_port_raster_dispatch(const l2d_raster_job *jobs, int count, const px_surface *surface)
{
    /* Same scratch bands and painter order as the P4, serialized on Host. */
    _Alignas(16) unsigned char scratch[32768];
    if (surface->rgb565_sink) {
        l2d_raster_jobs_rgb565(jobs,count,surface,scratch,sizeof(scratch),2,0);
        l2d_raster_jobs_rgb565(jobs,count,surface,scratch,sizeof(scratch),2,1);
        return;
    }
    l2d_raster_jobs_scratch(jobs,count,surface,scratch,sizeof(scratch),2,0);
    l2d_raster_jobs_scratch(jobs,count,surface,scratch,sizeof(scratch),2,1);
}

int l2d_port_rgb565_supported(void) { return 1; }
