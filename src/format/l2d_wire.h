/**
 * Cursor over a legacy .live byte image.
 * Reads do not cast file bytes onto a runtime struct.
 */
#ifndef L2D_WIRE_H
#define L2D_WIRE_H

#include <stddef.h>
#include <stdint.h>

typedef struct l2d_wire {
    const uint8_t *data;
    size_t size;
    size_t offset;
    int failed;
} l2d_wire_t;

void l2d_wire_init(l2d_wire_t *wire, const void *data, size_t size);
int l2d_wire_u8(l2d_wire_t *wire, uint8_t *out);
int l2d_wire_u16le(l2d_wire_t *wire, uint16_t *out);
int l2d_wire_u32le(l2d_wire_t *wire, uint32_t *out);
int l2d_wire_i32le(l2d_wire_t *wire, int32_t *out);
int l2d_wire_f32le(l2d_wire_t *wire, float *out);
int l2d_wire_bytes(l2d_wire_t *wire, void *dst, size_t size);
int l2d_wire_slice(l2d_wire_t *wire, size_t size, const uint8_t **out);

#endif
