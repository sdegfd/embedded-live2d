#include "l2d_wire.h"

#include <string.h>

void l2d_wire_init(l2d_wire_t *wire, const void *data, size_t size)
{
    wire->data = (const uint8_t *)data;
    wire->size = data ? size : 0;
    wire->offset = 0;
    wire->failed = data ? 0 : 1;
}

static int l2d_wire_take(l2d_wire_t *wire, size_t size, const uint8_t **out)
{
    if (!wire || wire->failed || size > wire->size - wire->offset) {
        if (wire) {
            wire->failed = 1;
        }
        return -1;
    }
    *out = wire->data + wire->offset;
    wire->offset += size;
    return 0;
}

int l2d_wire_u8(l2d_wire_t *wire, uint8_t *out)
{
    const uint8_t *p;
    if (!out || l2d_wire_take(wire, 1, &p) != 0) {
        return -1;
    }
    *out = p[0];
    return 0;
}

int l2d_wire_u16le(l2d_wire_t *wire, uint16_t *out)
{
    const uint8_t *p;
    if (!out || l2d_wire_take(wire, 2, &p) != 0) {
        return -1;
    }
    *out = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
    return 0;
}

int l2d_wire_u32le(l2d_wire_t *wire, uint32_t *out)
{
    const uint8_t *p;
    if (!out || l2d_wire_take(wire, 4, &p) != 0) {
        return -1;
    }
    *out = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
    return 0;
}

int l2d_wire_i32le(l2d_wire_t *wire, int32_t *out)
{
    uint32_t bits;
    if (!out || l2d_wire_u32le(wire, &bits) != 0) {
        return -1;
    }
    *out = (int32_t)bits;
    return 0;
}

int l2d_wire_f32le(l2d_wire_t *wire, float *out)
{
    uint32_t bits;
    if (!out || l2d_wire_u32le(wire, &bits) != 0) {
        return -1;
    }
    memcpy(out, &bits, sizeof(*out));
    return 0;
}

int l2d_wire_bytes(l2d_wire_t *wire, void *dst, size_t size)
{
    const uint8_t *p;
    if (!dst || l2d_wire_take(wire, size, &p) != 0) {
        return -1;
    }
    memcpy(dst, p, size);
    return 0;
}

int l2d_wire_slice(l2d_wire_t *wire, size_t size, const uint8_t **out)
{
    if (!out) {
        if (wire) {
            wire->failed = 1;
        }
        return -1;
    }
    return l2d_wire_take(wire, size, out);
}
