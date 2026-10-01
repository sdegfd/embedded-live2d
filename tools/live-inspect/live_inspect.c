/*
 * Read a PainterEngine .live file and write a JSON description of the
 * canvas, textures, control-point tree, timeline frames and RT30 axes.
 * The executable is standalone: it does not link PainterEngine.
 *
 * Wire layout matches PX_LiveFrameworkExport / the RT30 trailer in
 * PainterEngine/kernel/PX_LiveFramework.c. Payload fields inside the old
 * reserve area are the visual transform (magic "LTF1").
 */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ID_BYTES 32
#define MAX_CHILDREN 16
#define SAMPLE_COUNT 30
#define PAYLOAD_BYTES 200
#define LOCAL_MAGIC 0x3146544Cu /* "LTF1" */
#define RT30_MAGIC 0x30335452u

enum {
	PROP_TRANSLATION = 1 << 0,
	PROP_ROTATION = 1 << 1,
	PROP_STRETCH = 1 << 2,
	PROP_VERTICES = 1 << 3,
	PROP_LOCAL_TRANSLATION = 1 << 4,
	PROP_LOCAL_ROTATION = 1 << 5,
	PROP_LOCAL_SCALE = 1 << 6,
	PROP_TEXTURE = 1 << 7,
	PROP_IMPULSE = 1 << 8,
	PROP_ALL = (1 << 9) - 1
};

typedef struct {
	uint32_t s[8];
	uint64_t bits;
	uint8_t buf[64];
	unsigned n;
} Sha256;

typedef struct {
	FILE *fp;
	int sp;
	int indent;
	int pending;
	unsigned char first[48];
	unsigned char kind[48]; /* 0 object, 1 array */
	int error;
} Json;

typedef struct {
	char id[ID_BYTES + 1];
	int width;
	int height;
	int offset_x;
	int offset_y;
	int opaque;
	int pixels;
} Texture;

typedef struct {
	char id[ID_BYTES + 1];
	int parent;
	int children[MAX_CHILDREN];
	int child_count;
	int triangles;
	int vertices;
	float key_x, key_y, key_z;
	int link;
	int k_min, k_max, k_nonzero;
	int uv_outside;
	int source_degenerate;
	int leaving_rest;
} Layer;

typedef struct {
	int layer;
	unsigned mask;
	int vertex_count;
	unsigned sample_offset;
	unsigned index_offset;
} Binding;

typedef struct {
	char id[ID_BYTES + 1];
	int middle;
	int default_sample;
	int coord_bits, rotation_bits, stretch_bits;
	int binding_count;
	int vertex_index_count;
	unsigned stride;
	Binding *bindings;
	uint16_t *indices;
	int16_t *samples;
} Axis;

typedef struct {
	int layer;
	int texture;
	int texture_differs;
	int has_translation, has_stretch, has_rotation, has_impulse, has_panc;
	int has_local_move, has_local_rotation, has_local_scale;
	float translation[2], stretch, rotation, impulse[2], panc[8];
	float local_move[2], local_rotation, local_scale;
	int vertex_affected;
	float vertex_max;
} Delta;

typedef struct {
	char id[ID_BYTES + 1];
	unsigned duration_ms;
	Delta *deltas;
	int delta_count;
} Frame;

typedef struct {
	char id[ID_BYTES + 1];
	Frame *frames;
	int frame_count;
} Animation;

typedef struct {
	char id[ID_BYTES + 1];
	unsigned version;
	int width, height;
	Texture *textures;
	int texture_count;
	Layer *layers;
	int layer_count;
	Animation *animations;
	int animation_count;
	Axis *axes;
	int axis_count;
	char (*warnings)[160];
	int warning_count;
	int warning_cap;
} Model;

static const char *g_error;

static void set_error(const char *message)
{
	if (!g_error) g_error = message;
}

static int range_ok(size_t size, size_t offset, size_t bytes)
{
	return bytes <= size && offset <= size - bytes;
}

static int read_u16(const uint8_t *p)
{
	return (int)p[0] | ((int)p[1] << 8);
}

static unsigned read_u32(const uint8_t *p)
{
	return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

static int read_i32(const uint8_t *p)
{
	return (int)read_u32(p);
}

static float read_f32(const uint8_t *p)
{
	uint32_t u = read_u32(p);
	float value;
	memcpy(&value, &u, 4);
	return value;
}

static void copy_id(char *dst, const uint8_t *src)
{
	memcpy(dst, src, ID_BYTES);
	dst[ID_BYTES] = 0;
}

static uint32_t sha_rotr(uint32_t value, int bits)
{
	return (value >> bits) | (value << (32 - bits));
}

static void sha_block(Sha256 *sha, const uint8_t block[64])
{
	static const uint32_t k[64] = {
		0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
		0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
		0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
		0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
		0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
		0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
		0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
		0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
	};
	uint32_t w[64];
	uint32_t a, b, c, d, e, f, g, h;
	int i;
	for (i = 0; i < 16; i++)
		w[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) |
			((uint32_t)block[i * 4 + 2] << 8) | block[i * 4 + 3];
	for (i = 16; i < 64; i++) {
		uint32_t s0 = sha_rotr(w[i - 15], 7) ^ sha_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
		uint32_t s1 = sha_rotr(w[i - 2], 17) ^ sha_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}
	a = sha->s[0]; b = sha->s[1]; c = sha->s[2]; d = sha->s[3];
	e = sha->s[4]; f = sha->s[5]; g = sha->s[6]; h = sha->s[7];
	for (i = 0; i < 64; i++) {
		uint32_t s1 = sha_rotr(e, 6) ^ sha_rotr(e, 11) ^ sha_rotr(e, 25);
		uint32_t ch = (e & f) ^ (~e & g);
		uint32_t t1 = h + s1 + ch + k[i] + w[i];
		uint32_t s0 = sha_rotr(a, 2) ^ sha_rotr(a, 13) ^ sha_rotr(a, 22);
		uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
		uint32_t t2 = s0 + maj;
		h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
	}
	sha->s[0] += a; sha->s[1] += b; sha->s[2] += c; sha->s[3] += d;
	sha->s[4] += e; sha->s[5] += f; sha->s[6] += g; sha->s[7] += h;
}

static void sha_init(Sha256 *sha)
{
	memset(sha, 0, sizeof(*sha));
	sha->s[0] = 0x6a09e667u; sha->s[1] = 0xbb67ae85u; sha->s[2] = 0x3c6ef372u; sha->s[3] = 0xa54ff53au;
	sha->s[4] = 0x510e527fu; sha->s[5] = 0x9b05688cu; sha->s[6] = 0x1f83d9abu; sha->s[7] = 0x5be0cd19u;
}

static void sha_update(Sha256 *sha, const uint8_t *data, size_t size)
{
	size_t i = 0;
	while (i < size) {
		sha->buf[sha->n++] = data[i++];
		if (sha->n == 64) {
			sha_block(sha, sha->buf);
			sha->bits += 512;
			sha->n = 0;
		}
	}
}

static void sha_final(Sha256 *sha, uint8_t out[32])
{
	unsigned i;
	uint64_t total = sha->bits + (uint64_t)sha->n * 8u;
	sha->buf[sha->n++] = 0x80;
	if (sha->n > 56) {
		while (sha->n < 64) sha->buf[sha->n++] = 0;
		sha_block(sha, sha->buf);
		sha->n = 0;
	}
	while (sha->n < 56) sha->buf[sha->n++] = 0;
	for (i = 0; i < 8; i++) sha->buf[63 - i] = (uint8_t)(total >> (8 * i));
	sha_block(sha, sha->buf);
	for (i = 0; i < 8; i++) {
		out[i * 4] = (uint8_t)(sha->s[i] >> 24);
		out[i * 4 + 1] = (uint8_t)(sha->s[i] >> 16);
		out[i * 4 + 2] = (uint8_t)(sha->s[i] >> 8);
		out[i * 4 + 3] = (uint8_t)sha->s[i];
	}
}

static void add_warning(Model *model, const char *text)
{
	if (model->warning_count == model->warning_cap) {
		int cap = model->warning_cap ? model->warning_cap * 2 : 8;
		char (*next)[160] = (char (*)[160])realloc(model->warnings, (size_t)cap * 160);
		if (!next) { set_error("out of memory"); return; }
		model->warnings = next;
		model->warning_cap = cap;
	}
	strncpy(model->warnings[model->warning_count], text, 159);
	model->warnings[model->warning_count][159] = 0;
	model->warning_count++;
}

static int parse_model(const uint8_t *data, size_t size, Model *model)
{
	size_t offset = 0;
	int i, j;
	memset(model, 0, sizeof(*model));
	if (!range_ok(size, 0, 24) || memcmp(data, "PainterEngineLiveDBinary", 24) != 0) {
		set_error("not a PainterEngine live file");
		return 0;
	}
	offset = 24;
	if (!range_ok(size, offset, 56)) { set_error("truncated file header"); return 0; }
	copy_id(model->id, data + offset);
	model->version = read_u32(data + offset + 32);
	model->width = read_i32(data + offset + 36);
	model->height = read_i32(data + offset + 40);
	model->layer_count = read_i32(data + offset + 44);
	model->animation_count = read_i32(data + offset + 48);
	model->texture_count = read_i32(data + offset + 52);
	offset += 56;
	if (model->version != 1) { set_error("unsupported live version"); return 0; }
	if (model->width < 1 || model->height < 1 || model->layer_count < 0 || model->layer_count > 256 ||
		model->texture_count < 0 || model->texture_count > 256 ||
		model->animation_count < 0 || model->animation_count > 256) {
		set_error("file counts are out of range");
		return 0;
	}
	model->textures = (Texture *)calloc((size_t)model->texture_count, sizeof(Texture));
	model->layers = (Layer *)calloc((size_t)model->layer_count, sizeof(Layer));
	model->animations = (Animation *)calloc((size_t)(model->animation_count ? model->animation_count : 1), sizeof(Animation));
	if ((model->texture_count && !model->textures) || (model->layer_count && !model->layers) || !model->animations) {
		set_error("out of memory");
		return 0;
	}
	for (i = 0; i < model->texture_count; i++) {
		int width, height, pixels, opaque = 0, p;
		size_t bytes;
		Texture *texture = &model->textures[i];
		if (!range_ok(size, offset, 48)) { set_error("truncated texture header"); return 0; }
		copy_id(texture->id, data + offset);
		width = read_i32(data + offset + 32);
		height = read_i32(data + offset + 36);
		texture->width = width;
		texture->height = height;
		texture->offset_x = read_i32(data + offset + 40);
		texture->offset_y = read_i32(data + offset + 44);
		offset += 48;
		if (width < 0 || height < 0 || width > 8192 || height > 8192) { set_error("texture size is out of range"); return 0; }
		pixels = width * height;
		bytes = (size_t)pixels * 4u;
		if (!range_ok(size, offset, bytes)) { set_error("truncated texture pixels"); return 0; }
		for (p = 0; p < pixels; p++) if (data[offset + (size_t)p * 4u + 3u]) opaque++;
		texture->opaque = opaque;
		texture->pixels = pixels;
		offset += bytes;
	}
	for (i = 0; i < model->layer_count; i++) {
		Layer *layer = &model->layers[i];
		int tri, vert;
		const uint8_t *header;
		if (!range_ok(size, offset, 124)) { set_error("truncated layer header"); return 0; }
		header = data + offset;
		copy_id(layer->id, header);
		layer->parent = read_i32(header + 32);
		layer->child_count = 0;
		for (j = 0; j < MAX_CHILDREN; j++) {
			int child = read_i32(header + 36 + j * 4);
			if (child == -1) break;
			layer->children[layer->child_count++] = child;
		}
		layer->triangles = read_i32(header + 100);
		layer->vertices = read_i32(header + 104);
		layer->key_x = read_f32(header + 108);
		layer->key_y = read_f32(header + 112);
		layer->key_z = read_f32(header + 116);
		layer->link = read_i32(header + 120);
		offset += 124;
		tri = layer->triangles;
		vert = layer->vertices;
		if (tri < 0 || vert < 0 || tri > 200000 || vert > 200000) { set_error("mesh size is out of range"); return 0; }
		if (!range_ok(size, offset, (size_t)tri * 12u + (size_t)vert * 96u)) { set_error("truncated mesh"); return 0; }
		layer->k_min = 0;
		layer->k_max = 0;
		{
			size_t vertex_at = offset + (size_t)tri * 12u;
			for (j = 0; j < tri; j++) {
				int a = read_i32(data + offset + (size_t)j * 12u);
				int b = read_i32(data + offset + (size_t)j * 12u + 4u);
				int c = read_i32(data + offset + (size_t)j * 12u + 8u);
				float ax, ay, bx, by, cx, cy, cross;
				if (a < 0 || b < 0 || c < 0 || a >= vert || b >= vert || c >= vert) {
					layer->source_degenerate++;
					continue;
				}
				ax = read_f32(data + vertex_at + (size_t)a * 96u);
				ay = read_f32(data + vertex_at + (size_t)a * 96u + 4u);
				bx = read_f32(data + vertex_at + (size_t)b * 96u);
				by = read_f32(data + vertex_at + (size_t)b * 96u + 4u);
				cx = read_f32(data + vertex_at + (size_t)c * 96u);
				cy = read_f32(data + vertex_at + (size_t)c * 96u + 4u);
				cross = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
				if (fabsf(cross) < 0.05f) layer->source_degenerate++;
			}
			for (j = 0; j < vert; j++) {
				const uint8_t *vertex = data + vertex_at + (size_t)j * 96u;
				float sx = read_f32(vertex);
				float sy = read_f32(vertex + 4);
				float cx = read_f32(vertex + 12);
				float cy = read_f32(vertex + 16);
				float tx = read_f32(vertex + 48);
				float ty = read_f32(vertex + 52);
				int k = read_i32(vertex + 84);
				float u = read_f32(vertex + 88);
				float v = read_f32(vertex + 92);
				if (j == 0) { layer->k_min = k; layer->k_max = k; }
				if (k < layer->k_min) layer->k_min = k;
				if (k > layer->k_max) layer->k_max = k;
				if (k) layer->k_nonzero++;
				if (u < -0.02f || u > 1.02f || v < -0.02f || v > 1.02f) layer->uv_outside++;
				if (fabsf(sx - cx) > 0.01f || fabsf(sy - cy) > 0.01f || fabsf(tx) > 0.01f || fabsf(ty) > 0.01f)
					layer->leaving_rest++;
			}
		}
		offset += (size_t)tri * 12u + (size_t)vert * 96u;
		if (layer->parent < -1 || layer->parent >= model->layer_count ||
			(layer->parent >= 0 && layer->parent == i)) {
			char text[160];
			snprintf(text, sizeof text, "layer %s has parent index %d", layer->id, layer->parent);
			add_warning(model, text);
		}
		for (j = 0; j < layer->child_count; j++) {
			int child = layer->children[j];
			if (child < 0 || child >= model->layer_count || child == i) {
				char text[160];
				snprintf(text, sizeof text, "layer %s has child index %d", layer->id, child);
				add_warning(model, text);
			}
		}
	}
	for (i = 0; i < model->animation_count; i++) {
		Animation *animation = &model->animations[i];
		int frame_count;
		if (!range_ok(size, offset, 36)) { set_error("truncated animation header"); return 0; }
		copy_id(animation->id, data + offset);
		frame_count = read_i32(data + offset + 32);
		offset += 36;
		if (frame_count < 0 || frame_count > 100000) { set_error("animation frame count is out of range"); return 0; }
		animation->frame_count = frame_count;
		animation->frames = (Frame *)calloc((size_t)(frame_count ? frame_count : 1), sizeof(Frame));
		if (!animation->frames) { set_error("out of memory"); return 0; }
		for (j = 0; j < frame_count; j++) {
			Frame *frame = &animation->frames[j];
			unsigned payload_size;
			size_t frame_end, cursor;
			int layer_index;
			Delta *deltas;
			if (!range_ok(size, offset, 40)) { set_error("truncated frame header"); return 0; }
			copy_id(frame->id, data + offset);
			payload_size = read_u32(data + offset + 32);
			frame->duration_ms = read_u32(data + offset + 36);
			offset += 40;
			if (!range_ok(size, offset, payload_size)) { set_error("truncated frame"); return 0; }
			frame_end = offset + payload_size;
			cursor = offset;
			deltas = (Delta *)calloc((size_t)(model->layer_count ? model->layer_count : 1), sizeof(Delta));
			if (!deltas) { set_error("out of memory"); return 0; }
			frame->deltas = deltas;
			for (layer_index = 0; layer_index < model->layer_count; layer_index++) {
				const uint8_t *payload;
				int vertex_count, vertex;
				unsigned magic;
				Delta delta;
				size_t vertex_bytes;
				if (!range_ok(size, cursor, PAYLOAD_BYTES) || cursor + PAYLOAD_BYTES > frame_end) {
					free(deltas);
					frame->deltas = NULL;
					set_error("frame payload does not match the layer count");
					return 0;
				}
				payload = data + cursor;
				memset(&delta, 0, sizeof(delta));
				delta.layer = layer_index;
				delta.translation[0] = read_f32(payload);
				delta.translation[1] = read_f32(payload + 4);
				delta.stretch = read_f32(payload + 12);
				delta.rotation = read_f32(payload + 16);
				delta.texture = read_i32(payload + 20);
				vertex_count = read_i32(payload + 24);
				delta.impulse[0] = read_f32(payload + 28);
				delta.impulse[1] = read_f32(payload + 32);
				for (vertex = 0; vertex < 8; vertex++) delta.panc[vertex] = read_f32(payload + 40 + vertex * 4);
				delta.local_move[0] = read_f32(payload + 72);
				delta.local_move[1] = read_f32(payload + 76);
				delta.local_rotation = read_f32(payload + 84);
				delta.local_scale = read_f32(payload + 88);
				magic = read_u32(payload + 92);
				if (vertex_count != model->layers[layer_index].vertices) {
					free(deltas);
					frame->deltas = NULL;
					set_error("frame vertex count does not match the layer");
					return 0;
				}
				vertex_bytes = (size_t)vertex_count * 12u;
				if (!range_ok(size, cursor + PAYLOAD_BYTES, vertex_bytes) || cursor + PAYLOAD_BYTES + vertex_bytes > frame_end) {
					free(deltas);
					frame->deltas = NULL;
					set_error("truncated frame vertices");
					return 0;
				}
				for (vertex = 0; vertex < vertex_count; vertex++) {
					float x = read_f32(data + cursor + PAYLOAD_BYTES + (size_t)vertex * 12u);
					float y = read_f32(data + cursor + PAYLOAD_BYTES + (size_t)vertex * 12u + 4u);
					float length = sqrtf(x * x + y * y);
					if (fabsf(x) > 0.0001f || fabsf(y) > 0.0001f) {
						delta.vertex_affected++;
						if (length > delta.vertex_max) delta.vertex_max = length;
					}
				}
				delta.has_translation = fabsf(delta.translation[0]) > 0.0001f || fabsf(delta.translation[1]) > 0.0001f;
				delta.has_stretch = fabsf(delta.stretch - 1.0f) > 0.0001f;
				delta.has_rotation = fabsf(delta.rotation) > 0.0001f;
				delta.has_impulse = fabsf(delta.impulse[0]) > 0.0001f || fabsf(delta.impulse[1]) > 0.0001f;
				delta.has_panc = 0;
				for (vertex = 0; vertex < 8; vertex++) if (fabsf(delta.panc[vertex]) > 0.0001f) delta.has_panc = 1;
				delta.texture_differs = delta.texture != model->layers[layer_index].link;
				if (magic == LOCAL_MAGIC) {
					delta.has_local_move = fabsf(delta.local_move[0]) > 0.0001f || fabsf(delta.local_move[1]) > 0.0001f;
					delta.has_local_rotation = fabsf(delta.local_rotation) > 0.0001f;
					delta.has_local_scale = fabsf(delta.local_scale) > 0.0001f;
				}
				if (delta.has_translation || delta.has_stretch || delta.has_rotation || delta.has_impulse ||
					delta.has_panc || delta.texture_differs || delta.has_local_move || delta.has_local_rotation ||
					delta.has_local_scale || delta.vertex_affected) {
					deltas[frame->delta_count++] = delta;
				}
				cursor += PAYLOAD_BYTES + vertex_bytes;
			}
			if (cursor != frame_end) {
				free(deltas);
				frame->deltas = NULL;
				set_error("frame size does not match its payloads");
				return 0;
			}
			offset = frame_end;
		}
	}
	if (offset < size) {
		unsigned magic, chunk, axes_offset;
		int axis_count, entry_size, version, header_size;
		size_t trailer = offset;
		if (!range_ok(size, offset, 24)) { set_error("truncated realtime trailer"); return 0; }
		magic = read_u32(data + offset);
		if (magic != RT30_MAGIC) { set_error("unrecognized bytes after the live body"); return 0; }
		version = read_u16(data + offset + 4);
		header_size = read_u16(data + offset + 6);
		chunk = read_u32(data + offset + 8);
		axis_count = read_u16(data + offset + 12);
		entry_size = read_u16(data + offset + 14);
		axes_offset = read_u32(data + offset + 16);
		if (version != 1 || header_size != 24 || entry_size != 68 || axes_offset != 24 ||
			chunk != (unsigned)(size - trailer) || axis_count < 1 || axis_count > 32) {
			set_error("realtime trailer header is invalid");
			return 0;
		}
		model->axis_count = axis_count;
		model->axes = (Axis *)calloc((size_t)axis_count, sizeof(Axis));
		if (!model->axes) { set_error("out of memory"); return 0; }
		for (i = 0; i < axis_count; i++) {
			size_t at = trailer + 24u + (size_t)i * 68u;
			Axis *axis = &model->axes[i];
			unsigned bindings_off, indices_off, samples_off, sample_bytes;
			int b;
			if (!range_ok(size, at, 68)) { set_error("truncated axis table"); return 0; }
			copy_id(axis->id, data + at);
			axis->middle = data[at + 36];
			axis->default_sample = data[at + 37];
			axis->coord_bits = data[at + 38];
			axis->rotation_bits = data[at + 39];
			axis->stretch_bits = data[at + 40];
			axis->binding_count = read_u16(data + at + 42);
			axis->vertex_index_count = (int)read_u32(data + at + 44);
			axis->stride = read_u32(data + at + 48);
			sample_bytes = read_u32(data + at + 52);
			bindings_off = read_u32(data + at + 56);
			indices_off = read_u32(data + at + 60);
			samples_off = read_u32(data + at + 64);
			if (axis->id[0] == 0 || axis->binding_count < 1 || axis->binding_count > 512 ||
				axis->vertex_index_count < 0 || axis->coord_bits > 14 || axis->rotation_bits > 14 ||
				axis->stretch_bits > 14 || axis->stride == 0 || (axis->stride & 1u) ||
				sample_bytes != axis->stride * SAMPLE_COUNT) {
				set_error("axis table entry is invalid");
				return 0;
			}
			if (axis->middle < 1 || axis->middle > 28 ||
				(axis->default_sample != 0 && axis->default_sample != axis->middle && axis->default_sample != 29)) {
				char text[160];
				snprintf(text, sizeof text, "axis %s has an unusual default sample %d or middle %d",
					axis->id, axis->default_sample, axis->middle);
				add_warning(model, text);
			}
			axis->bindings = (Binding *)calloc((size_t)axis->binding_count, sizeof(Binding));
			axis->samples = (int16_t *)malloc(sample_bytes);
			if (axis->vertex_index_count)
				axis->indices = (uint16_t *)malloc((size_t)axis->vertex_index_count * sizeof(uint16_t));
			if (!axis->bindings || !axis->samples || (axis->vertex_index_count && !axis->indices)) {
				set_error("out of memory");
				return 0;
			}
			if (!range_ok(size, trailer + bindings_off, (size_t)axis->binding_count * 16u) ||
				!range_ok(size, trailer + indices_off, (size_t)axis->vertex_index_count * 2u) ||
				!range_ok(size, trailer + samples_off, sample_bytes)) {
				set_error("axis data is outside the trailer");
				return 0;
			}
			for (b = 0; b < axis->binding_count; b++) {
				const uint8_t *binding_data = data + trailer + bindings_off + (size_t)b * 16u;
				Binding *binding = &axis->bindings[b];
				int used = 0;
				binding->layer = read_u16(binding_data);
				binding->mask = read_u16(binding_data + 2);
				binding->vertex_count = read_u16(binding_data + 4);
				binding->index_offset = read_u32(binding_data + 8);
				binding->sample_offset = read_u32(binding_data + 12);
				if (binding->layer < 0 || binding->layer >= model->layer_count ||
					!binding->mask || (binding->mask & ~PROP_ALL) ||
					(binding->sample_offset & 1u) || binding->sample_offset > axis->stride) {
					set_error("axis binding is invalid");
					return 0;
				}
				if (binding->mask & PROP_TRANSLATION) used += 4;
				if (binding->mask & PROP_ROTATION) used += 2;
				if (binding->mask & PROP_STRETCH) used += 2;
				if (binding->mask & PROP_LOCAL_TRANSLATION) used += 4;
				if (binding->mask & PROP_LOCAL_ROTATION) used += 2;
				if (binding->mask & PROP_LOCAL_SCALE) used += 2;
				if (binding->mask & PROP_TEXTURE) used += 2;
				if (binding->mask & PROP_IMPULSE) used += 4;
				if (binding->mask & PROP_VERTICES) {
					int v;
					if (binding->vertex_count < 1 ||
						binding->index_offset > (unsigned)axis->vertex_index_count ||
						binding->vertex_count > axis->vertex_index_count - (int)binding->index_offset) {
						set_error("axis vertex indices are out of range");
						return 0;
					}
					for (v = 0; v < binding->vertex_count; v++) {
						int index = read_u16(data + trailer + indices_off + (binding->index_offset + (unsigned)v) * 2u);
						if (index >= model->layers[binding->layer].vertices) {
							set_error("axis vertex index does not exist on the layer");
							return 0;
						}
					}
					used += binding->vertex_count * 4;
				} else if (binding->vertex_count) {
					set_error("axis binding counts vertices without the vertex channel");
					return 0;
				}
				if (binding->sample_offset + (unsigned)used > axis->stride) {
					set_error("axis sample does not fit the stride");
					return 0;
				}
			}
			for (b = 0; b < axis->vertex_index_count; b++)
				axis->indices[b] = (uint16_t)read_u16(data + trailer + indices_off + (size_t)b * 2u);
			for (b = 0; b < (int)(sample_bytes / 2u); b++)
				axis->samples[b] = (int16_t)read_u16(data + trailer + samples_off + (size_t)b * 2u);
		}
		offset = size;
	}
	if (offset != size) { set_error("file has unread bytes"); return 0; }
	for (i = 0; i < model->layer_count; i++) {
		Layer *layer = &model->layers[i];
		if (layer->parent >= 0) {
			Layer *parent = &model->layers[layer->parent];
			int found = 0;
			for (j = 0; j < parent->child_count; j++) if (parent->children[j] == i) found = 1;
			if (!found) {
				char text[160];
				snprintf(text, sizeof text, "%s lists parent %s, but that parent does not list it as a child",
					layer->id, parent->id);
				add_warning(model, text);
			}
		}
		for (j = 0; j < layer->child_count; j++) {
			int child = layer->children[j];
			if (child >= 0 && child < model->layer_count && model->layers[child].parent != i) {
				char text[160];
				snprintf(text, sizeof text, "%s lists child %s, but that child has another parent",
					layer->id, model->layers[child].id);
				add_warning(model, text);
			}
		}
	}
	return g_error == NULL;
}

static void json_indent(Json *json)
{
	int i;
	fputc('\n', json->fp);
	for (i = 0; i < json->indent; i++) fputs("  ", json->fp);
}

static void json_value_start(Json *json)
{
	if (json->error) return;
	if (json->pending) { json->pending = 0; return; }
	if (json->sp < 0) return;
	if (!json->first[json->sp]) fputc(',', json->fp);
	json->first[json->sp] = 0;
	json_indent(json);
}

static void json_emit_codepoint(Json *json, unsigned int cp)
{
	if (cp >= 0x10000u) {
		unsigned int u = cp - 0x10000u;
		fprintf(json->fp, "\\u%04x\\u%04x", 0xD800u + (u >> 10), 0xDC00u + (u & 0x3ffu));
	} else {
		fprintf(json->fp, "\\u%04x", cp);
	}
}

static void json_string_raw(Json *json, const char *text)
{
	const unsigned char *cursor = (const unsigned char *)text;
	fputc('"', json->fp);
	while (*cursor) {
		unsigned char c = *cursor;
		unsigned int cp;
		int extra;
		int k;
		if (c < 0x80) {
			if (c == '"' || c == '\\') fprintf(json->fp, "\\%c", c);
			else if (c == '\n') fputs("\\n", json->fp);
			else if (c == '\r') fputs("\\r", json->fp);
			else if (c == '\t') fputs("\\t", json->fp);
			else if (c < 0x20) fprintf(json->fp, "\\u%04x", c);
			else fputc(c, json->fp);
			cursor++;
			continue;
		}
		if ((c & 0xe0) == 0xc0) { cp = c & 0x1fu; extra = 1; }
		else if ((c & 0xf0) == 0xe0) { cp = c & 0x0fu; extra = 2; }
		else if ((c & 0xf8) == 0xf0) { cp = c & 0x07u; extra = 3; }
		else { fprintf(json->fp, "\\u%04x", c); cursor++; continue; }
		for (k = 1; k <= extra; k++) {
			unsigned char next = cursor[k];
			if ((next & 0xc0) != 0x80) break;
			cp = (cp << 6) | (next & 0x3fu);
		}
		if (k <= extra || (extra == 1 && cp < 0x80u) || (extra == 2 && cp < 0x800u) ||
			(extra == 3 && cp < 0x10000u) || cp > 0x10ffffu) {
			fprintf(json->fp, "\\u%04x", c);
			cursor++;
			continue;
		}
		json_emit_codepoint(json, cp);
		cursor += (size_t)extra + 1u;
	}
	fputc('"', json->fp);
}

static void json_begin(Json *json, int array)
{
	json_value_start(json);
	fputc(array ? '[' : '{', json->fp);
	if (json->sp + 1 >= (int)sizeof(json->kind)) { json->error = 1; set_error("json nesting is too deep"); return; }
	json->sp++;
	json->kind[json->sp] = (unsigned char)array;
	json->first[json->sp] = 1;
	json->indent++;
}

static void json_end(Json *json)
{
	int array;
	if (json->error || json->sp < 0) return;
	array = json->kind[json->sp];
	json->indent--;
	if (!json->first[json->sp]) json_indent(json);
	json->sp--;
	fputc(array ? ']' : '}', json->fp);
}

static void json_key(Json *json, const char *key)
{
	json_value_start(json);
	json_string_raw(json, key);
	fputs(": ", json->fp);
	json->pending = 1;
}

static void json_string(Json *json, const char *text)
{
	json_value_start(json);
	json_string_raw(json, text);
}

static void json_int(Json *json, int value)
{
	json_value_start(json);
	fprintf(json->fp, "%d", value);
}

static void json_uint(Json *json, unsigned value)
{
	json_value_start(json);
	fprintf(json->fp, "%u", value);
}

static void json_null(Json *json)
{
	json_value_start(json);
	fputs("null", json->fp);
}

static void json_number(Json *json, double value)
{
	char buf[64];
	char *dot;
	char *end;
	json_value_start(json);
	if (value == 0) { fputc('0', json->fp); return; }
	snprintf(buf, sizeof buf, "%.10f", value);
	dot = strchr(buf, '.');
	if (dot) {
		end = buf + strlen(buf) - 1;
		while (end > dot && *end == '0') *end-- = 0;
		if (end == dot) *end = 0;
	}
	fputs(buf, json->fp);
}

static void json_note(Json *json, const char *key, const char *value)
{
	json_key(json, key);
	json_string(json, value);
}

static double decode_fixed(int value, int bits)
{
	return (double)value / (double)(1u << bits);
}

static const int16_t *sample_values(const Axis *axis, int sample, unsigned byte_offset)
{
	return axis->samples + (size_t)sample * (axis->stride / 2u) + byte_offset / 2u;
}

static void json_vec2_from_fixed(Json *json, const char *key, int x, int y, int bits)
{
	json_key(json, key);
	json_begin(json, 0);
	json_key(json, "x");
	json_number(json, decode_fixed(x, bits));
	json_key(json, "y");
	json_number(json, decode_fixed(y, bits));
	json_end(json);
}

static void json_properties(Json *json, unsigned mask)
{
	static const struct { unsigned bit; const char *name; } names[] = {
		{ PROP_TRANSLATION, "translation" },
		{ PROP_ROTATION, "rotation" },
		{ PROP_STRETCH, "stretch" },
		{ PROP_VERTICES, "vertices" },
		{ PROP_LOCAL_TRANSLATION, "localTranslation" },
		{ PROP_LOCAL_ROTATION, "localRotation" },
		{ PROP_LOCAL_SCALE, "localScale" },
		{ PROP_TEXTURE, "texture" },
		{ PROP_IMPULSE, "impulse" }
	};
	int i;
	int count = (int)(sizeof(names) / sizeof(names[0]));
	json_key(json, "properties");
	json_begin(json, 1);
	for (i = 0; i < count; i++) if (mask & names[i].bit) json_string(json, names[i].name);
	json_end(json);
}

static void json_vertex_summary(Json *json, int affected, int count, double max_length)
{
	json_key(json, "vertices");
	json_begin(json, 0);
	json_key(json, "affected");
	json_int(json, affected);
	json_key(json, "count");
	json_int(json, count);
	json_key(json, "max");
	json_number(json, max_length);
	json_end(json);
}

static void json_axis_sample(Json *json, const Model *model, const Axis *axis, const Binding *binding, int sample)
{
	const int16_t *values = sample_values(axis, sample, binding->sample_offset);
	int cursor = 0;
	int affected = 0;
	double max_length = 0;
	json_begin(json, 0);
	json_key(json, "index");
	json_int(json, sample);
	if (binding->mask & PROP_TRANSLATION) {
		int x = values[cursor++];
		int y = values[cursor++];
		if (x || y) json_vec2_from_fixed(json, "translation", x, y, axis->coord_bits);
	}
	if (binding->mask & PROP_ROTATION) {
		int raw = values[cursor++];
		if (raw) {
			json_key(json, "rotation");
			json_number(json, decode_fixed(raw, axis->rotation_bits));
		}
	}
	if (binding->mask & PROP_STRETCH) {
		int raw = values[cursor++];
		if (raw) {
			json_key(json, "stretch");
			json_number(json, 1.0 + decode_fixed(raw, axis->stretch_bits));
		}
	}
	if (binding->mask & PROP_LOCAL_TRANSLATION) {
		int x = values[cursor++];
		int y = values[cursor++];
		if (x || y) json_vec2_from_fixed(json, "localTranslation", x, y, axis->coord_bits);
	}
	if (binding->mask & PROP_LOCAL_ROTATION) {
		int raw = values[cursor++];
		if (raw) {
			json_key(json, "localRotation");
			json_number(json, decode_fixed(raw, axis->rotation_bits));
		}
	}
	if (binding->mask & PROP_LOCAL_SCALE) {
		int raw = values[cursor++];
		if (raw) {
			json_key(json, "localScale");
			json_number(json, 1.0 + decode_fixed(raw, axis->stretch_bits));
		}
	}
	if (binding->mask & PROP_TEXTURE) {
		int texture = values[cursor++];
		if (texture != model->layers[binding->layer].link) {
			json_key(json, "texture");
			json_int(json, texture);
		}
	}
	if (binding->mask & PROP_IMPULSE) {
		int x = values[cursor++];
		int y = values[cursor++];
		if (x || y) json_vec2_from_fixed(json, "impulse", x, y, axis->coord_bits);
	}
	if (binding->mask & PROP_VERTICES) {
		int v;
		for (v = 0; v < binding->vertex_count; v++) {
			int x = values[cursor++];
			int y = values[cursor++];
			if (x || y) {
				double dx = decode_fixed(x, axis->coord_bits);
				double dy = decode_fixed(y, axis->coord_bits);
				double length = sqrt(dx * dx + dy * dy);
				affected++;
				if (length > max_length) max_length = length;
			}
		}
		if (affected) json_vertex_summary(json, affected, binding->vertex_count, max_length);
	}
	json_end(json);
}

static void json_delta_channels(Json *json, const Delta *delta, int vertex_count)
{
	if (delta->has_translation) {
		json_key(json, "translation");
		json_begin(json, 0);
		json_key(json, "x"); json_number(json, delta->translation[0]);
		json_key(json, "y"); json_number(json, delta->translation[1]);
		json_end(json);
	}
	if (delta->has_rotation) { json_key(json, "rotation"); json_number(json, delta->rotation); }
	if (delta->has_stretch) { json_key(json, "stretch"); json_number(json, delta->stretch); }
	if (delta->has_local_move) {
		json_key(json, "localTranslation");
		json_begin(json, 0);
		json_key(json, "x"); json_number(json, delta->local_move[0]);
		json_key(json, "y"); json_number(json, delta->local_move[1]);
		json_end(json);
	}
	if (delta->has_local_rotation) { json_key(json, "localRotation"); json_number(json, delta->local_rotation); }
	if (delta->has_local_scale) { json_key(json, "localScale"); json_number(json, 1.0 + delta->local_scale); }
	if (delta->has_impulse) {
		json_key(json, "impulse");
		json_begin(json, 0);
		json_key(json, "x"); json_number(json, delta->impulse[0]);
		json_key(json, "y"); json_number(json, delta->impulse[1]);
		json_end(json);
	}
	if (delta->texture_differs) { json_key(json, "texture"); json_int(json, delta->texture); }
	if (delta->has_panc) {
		json_key(json, "panc");
		json_begin(json, 0);
		json_key(json, "x"); json_number(json, delta->panc[0]);
		json_key(json, "y"); json_number(json, delta->panc[1]);
		json_key(json, "width"); json_number(json, delta->panc[2]);
		json_key(json, "height"); json_number(json, delta->panc[3]);
		json_key(json, "sourceX"); json_number(json, delta->panc[4]);
		json_key(json, "sourceY"); json_number(json, delta->panc[5]);
		json_key(json, "currentX"); json_number(json, delta->panc[6]);
		json_key(json, "currentY"); json_number(json, delta->panc[7]);
		json_end(json);
	}
	if (delta->vertex_affected)
		json_vertex_summary(json, delta->vertex_affected, vertex_count, delta->vertex_max);
}

static void json_hierarchy(Json *json, const Model *model, int layer, int *seen)
{
	int i;
	if (layer < 0 || layer >= model->layer_count || seen[layer]) {
		json_begin(json, 0);
		json_key(json, "index");
		json_int(json, layer);
		json_end(json);
		return;
	}
	seen[layer] = 1;
	json_begin(json, 0);
	json_key(json, "id");
	json_string(json, model->layers[layer].id);
	json_key(json, "children");
	json_begin(json, 1);
	for (i = 0; i < model->layers[layer].child_count; i++)
		json_hierarchy(json, model, model->layers[layer].children[i], seen);
	json_end(json);
	json_end(json);
}

static int write_model(Json *json, const Model *model, const char *file_name, const uint8_t hash[32], size_t size)
{
	int i, j, vertices = 0, triangles = 0, frames = 0, uv = 0, degenerate = 0, elastic = 0, leaving = 0;
	char hash_text[65];
	int *seen;
	for (i = 0; i < 32; i++) sprintf(hash_text + i * 2, "%02x", hash[i]);
	for (i = 0; i < model->layer_count; i++) {
		vertices += model->layers[i].vertices;
		triangles += model->layers[i].triangles;
		uv += model->layers[i].uv_outside;
		degenerate += model->layers[i].source_degenerate;
		elastic += model->layers[i].k_nonzero;
		leaving += model->layers[i].leaving_rest;
	}
	for (i = 0; i < model->animation_count; i++) frames += model->animations[i].frame_count;
	json_begin(json, 0);
	json_key(json, "schema"); json_string(json, "live-inspect-v1");
	json_key(json, "fileName"); json_string(json, file_name);
	json_key(json, "sha256"); json_string(json, hash_text);
	json_key(json, "sizeBytes"); json_uint(json, (unsigned)size);
	json_key(json, "id"); json_string(json, model->id);
	json_key(json, "version"); json_uint(json, model->version);
	json_key(json, "canvas");
	json_begin(json, 0);
	json_key(json, "width"); json_int(json, model->width);
	json_key(json, "height"); json_int(json, model->height);
	json_end(json);
	json_key(json, "counts");
	json_begin(json, 0);
	json_key(json, "layers"); json_int(json, model->layer_count);
	json_key(json, "textures"); json_int(json, model->texture_count);
	json_key(json, "vertices"); json_int(json, vertices);
	json_key(json, "triangles"); json_int(json, triangles);
	json_key(json, "animations"); json_int(json, model->animation_count);
	json_key(json, "frames"); json_int(json, frames);
	json_key(json, "axes"); json_int(json, model->axis_count);
	json_key(json, "uvOutside"); json_int(json, uv);
	json_key(json, "sourceDegenerateTriangles"); json_int(json, degenerate);
	json_key(json, "elasticVertices"); json_int(json, elastic);
	json_key(json, "verticesLeavingRest"); json_int(json, leaving);
	json_end(json);
	json_key(json, "notes");
	json_begin(json, 0);
	json_note(json, "yAxis", "Y 向下增大。");
	json_note(json, "zOrder", "keyPoint.z 越大越先画，画面上越靠后。");
	json_note(json, "samples", "每条实时轴有 30 个采样，下标 0 到 29。defaultSample 是锁定参考档。middleKey 是中间档，通常是 15。");
	json_note(json, "omittedChannels", "和静止值相同的通道不写出。静止值：平移 0，骨骼角 0，骨骼伸缩 1，画面位移 0，画面旋转 0，画面缩放 1，冲量 0，顶点不动，纹理等于图层绑定的纹理。");
	json_note(json, "translation", "控制点位移。根图层直接移动根控制点。实时轴里，子图层的这项会先加到父子控制点的连线上，再绕父控制点旋转。");
	json_note(json, "rotation", "骨骼角，单位度，绕本层控制点转本层网格。父层骨骼角还会让子控制点绕父控制点公转。");
	json_note(json, "stretch", "骨骼伸缩，1 是原长。它改变子控制点到父控制点的距离，并拉长父网格朝向这个子图层的一侧。");
	json_note(json, "localTranslation", "画面位移。子孙图层跟着画面移动，控制点之间的骨骼长度不变。");
	json_note(json, "localRotation", "画面旋转，单位度，绕本层控制点。子孙跟着画面转，不改它们自己的骨骼角。");
	json_note(json, "localScale", "画面等比缩放，1 是原大，绕本层控制点。子孙跟着缩放。");
	json_note(json, "impulse", "冲量。只有弹性 k 不为 0 的顶点会被动。");
	json_note(json, "vertices", "相对静止网格的顶点偏移。affected 是被动顶点数，count 是参与这次变形的顶点数，max 是最大偏移长度。");
	json_note(json, "panc", "位置追踪矩形。框边不动，source 十字被移动到 current，框内按横竖分别挤压。");
	json_note(json, "pivotDelta", "控制点减去纹理中心。Y 为负表示控制点在纹理中心上方。");
	json_note(json, "verticesLeavingRest", "图层网格里，当前位置或当前顶点位移已经离开静止源网格的顶点数。实时轴的顶点通道仍相对静止源网格，不以这个当前位置为基准。");
	json_end(json);

	json_key(json, "textures");
	json_begin(json, 1);
	for (i = 0; i < model->texture_count; i++) {
		const Texture *texture = &model->textures[i];
		json_begin(json, 0);
		json_key(json, "index"); json_int(json, i);
		json_key(json, "id"); json_string(json, texture->id);
		json_key(json, "width"); json_int(json, texture->width);
		json_key(json, "height"); json_int(json, texture->height);
		json_key(json, "offsetX"); json_int(json, texture->offset_x);
		json_key(json, "offsetY"); json_int(json, texture->offset_y);
		json_key(json, "opaquePixels"); json_int(json, texture->opaque);
		json_key(json, "pixels"); json_int(json, texture->pixels);
		json_end(json);
	}
	json_end(json);

	json_key(json, "layers");
	json_begin(json, 1);
	for (i = 0; i < model->layer_count; i++) {
		const Layer *layer = &model->layers[i];
		const Texture *texture = (layer->link >= 0 && layer->link < model->texture_count) ? &model->textures[layer->link] : NULL;
		json_begin(json, 0);
		json_key(json, "index"); json_int(json, i);
		json_key(json, "id"); json_string(json, layer->id);
		json_key(json, "parent");
		if (layer->parent >= 0 && layer->parent < model->layer_count) json_string(json, model->layers[layer->parent].id);
		else json_null(json);
		json_key(json, "parentIndex"); json_int(json, layer->parent);
		json_key(json, "children");
		json_begin(json, 1);
		for (j = 0; j < layer->child_count; j++) {
			int child = layer->children[j];
			if (child >= 0 && child < model->layer_count) json_string(json, model->layers[child].id);
			else json_int(json, child);
		}
		json_end(json);
		json_key(json, "keyPoint");
		json_begin(json, 0);
		json_key(json, "x"); json_number(json, layer->key_x);
		json_key(json, "y"); json_number(json, layer->key_y);
		json_key(json, "z"); json_number(json, layer->key_z);
		json_end(json);
		json_key(json, "texture");
		if (!texture) json_null(json);
		else {
			json_begin(json, 0);
			json_key(json, "index"); json_int(json, layer->link);
			json_key(json, "id"); json_string(json, texture->id);
			json_key(json, "width"); json_int(json, texture->width);
			json_key(json, "height"); json_int(json, texture->height);
			json_key(json, "offsetX"); json_int(json, texture->offset_x);
			json_key(json, "offsetY"); json_int(json, texture->offset_y);
			json_end(json);
			json_key(json, "pivotDeltaFromTextureCenter");
			json_begin(json, 0);
			json_key(json, "x"); json_number(json, layer->key_x - (texture->offset_x + texture->width / 2.0));
			json_key(json, "y"); json_number(json, layer->key_y - (texture->offset_y + texture->height / 2.0));
			json_end(json);
		}
		json_key(json, "vertices"); json_int(json, layer->vertices);
		json_key(json, "triangles"); json_int(json, layer->triangles);
		json_key(json, "elasticity");
		json_begin(json, 0);
		json_key(json, "min"); json_int(json, layer->k_min);
		json_key(json, "max"); json_int(json, layer->k_max);
		json_key(json, "nonzero"); json_int(json, layer->k_nonzero);
		json_end(json);
		json_key(json, "uvOutside"); json_int(json, layer->uv_outside);
		json_key(json, "sourceDegenerateTriangles"); json_int(json, layer->source_degenerate);
		json_key(json, "verticesLeavingRest"); json_int(json, layer->leaving_rest);
		json_end(json);
	}
	json_end(json);

	seen = (int *)calloc((size_t)(model->layer_count ? model->layer_count : 1), sizeof(int));
	json_key(json, "hierarchy");
	json_begin(json, 1);
	if (seen) {
		for (i = 0; i < model->layer_count; i++)
			if (model->layers[i].parent < 0) json_hierarchy(json, model, i, seen);
		for (i = 0; i < model->layer_count; i++)
			if (!seen[i]) json_hierarchy(json, model, i, seen);
	}
	json_end(json);
	free(seen);

	json_key(json, "animations");
	json_begin(json, 1);
	for (i = 0; i < model->animation_count; i++) {
		const Animation *animation = &model->animations[i];
		json_begin(json, 0);
		json_key(json, "id"); json_string(json, animation->id);
		json_key(json, "frames");
		json_begin(json, 1);
		for (j = 0; j < animation->frame_count; j++) {
			const Frame *frame = &animation->frames[j];
			int d;
			json_begin(json, 0);
			json_key(json, "index"); json_int(json, j);
			json_key(json, "id"); json_string(json, frame->id);
			json_key(json, "durationMs"); json_uint(json, frame->duration_ms);
			json_key(json, "layers");
			json_begin(json, 1);
			for (d = 0; d < frame->delta_count; d++) {
				const Delta *delta = &frame->deltas[d];
				json_begin(json, 0);
				json_key(json, "layer"); json_string(json, model->layers[delta->layer].id);
				json_delta_channels(json, delta, model->layers[delta->layer].vertices);
				json_end(json);
			}
			json_end(json);
			json_end(json);
		}
		json_end(json);
		json_end(json);
	}
	json_end(json);

	json_key(json, "axes");
	json_begin(json, 1);
	for (i = 0; i < model->axis_count; i++) {
		const Axis *axis = &model->axes[i];
		json_begin(json, 0);
		json_key(json, "id"); json_string(json, axis->id);
		json_key(json, "sampleCount"); json_int(json, SAMPLE_COUNT);
		json_key(json, "middleKey"); json_int(json, axis->middle);
		json_key(json, "defaultSample"); json_int(json, axis->default_sample);
		json_key(json, "fractionBits");
		json_begin(json, 0);
		json_key(json, "coord"); json_int(json, axis->coord_bits);
		json_key(json, "rotation"); json_int(json, axis->rotation_bits);
		json_key(json, "stretch"); json_int(json, axis->stretch_bits);
		json_end(json);
		json_key(json, "bindings");
		json_begin(json, 1);
		for (j = 0; j < axis->binding_count; j++) {
			const Binding *binding = &axis->bindings[j];
			int sample;
			json_begin(json, 0);
			json_key(json, "layer"); json_string(json, model->layers[binding->layer].id);
			json_key(json, "layerIndex"); json_int(json, binding->layer);
			json_properties(json, binding->mask);
			json_key(json, "samples");
			json_begin(json, 1);
			for (sample = 0; sample < SAMPLE_COUNT; sample++)
				json_axis_sample(json, model, axis, binding, sample);
			json_end(json);
			json_end(json);
		}
		json_end(json);
		json_end(json);
	}
	json_end(json);

	json_key(json, "warnings");
	json_begin(json, 1);
	for (i = 0; i < model->warning_count; i++) json_string(json, model->warnings[i]);
	json_end(json);
	json_end(json);
	fputc('\n', json->fp);
	return !json->error && !ferror(json->fp);
}

static void free_model(Model *model)
{
	int i, j;
	for (i = 0; i < model->animation_count; i++) {
		for (j = 0; j < model->animations[i].frame_count; j++) free(model->animations[i].frames[j].deltas);
		free(model->animations[i].frames);
	}
	for (i = 0; i < model->axis_count; i++) {
		free(model->axes[i].bindings);
		free(model->axes[i].indices);
		free(model->axes[i].samples);
	}
	free(model->textures);
	free(model->layers);
	free(model->animations);
	free(model->axes);
	free(model->warnings);
}

static const char *file_name_of(const char *path)
{
	const char *slash = strrchr(path, '\\');
	const char *slash2 = strrchr(path, '/');
	if (slash2 && (!slash || slash2 > slash)) slash = slash2;
	return slash ? slash + 1 : path;
}

static int ends_with_live(const char *path)
{
	size_t n = strlen(path);
	return n >= 5 && _stricmp(path + n - 5, ".live") == 0;
}

static void default_output(const char *input, char *output, size_t cap)
{
	size_t n = strlen(input);
	if (ends_with_live(input) && n - 5 + 6 < cap) {
		memcpy(output, input, n - 5);
		memcpy(output + n - 5, ".json", 6);
		return;
	}
	snprintf(output, cap, "%s.json", input);
}

static void usage(void)
{
	fprintf(stderr,
		"用法: live-inspect.exe <模型.live> [-o 输出.json]\n"
		"不写 -o 时，在模型旁边生成同名 .json。\n"
		"JSON 含画布、纹理、控制点层级、时间轴帧和 30 档实时轴。静止通道会省略。\n");
}

int main(int argc, char **argv)
{
	const char *input = NULL;
	const char *output_arg = NULL;
	char output_buf[1024];
	const char *output;
	FILE *in, *out;
	uint8_t *data = NULL;
	long length;
	size_t size;
	Sha256 sha;
	uint8_t hash[32];
	Model model;
	Json json;
	int i, frames = 0;
	int ok;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "/?") == 0) {
			usage();
			return 0;
		}
		if (strcmp(argv[i], "-o") == 0) {
			if (i + 1 >= argc) { usage(); return 1; }
			output_arg = argv[++i];
			continue;
		}
		if (!input) input = argv[i];
		else { usage(); return 1; }
	}
	if (!input) { usage(); return 1; }
	if (output_arg) output = output_arg;
	else { default_output(input, output_buf, sizeof output_buf); output = output_buf; }
	{
		char input_full[1024], output_full[1024];
		const char *left = _fullpath(input_full, input, sizeof input_full) ? input_full : input;
		const char *right = _fullpath(output_full, output, sizeof output_full) ? output_full : output;
		if (_stricmp(left, right) == 0) {
			fprintf(stderr, "输出路径和模型路径相同。\n");
			return 1;
		}
	}
	in = fopen(input, "rb");
	if (!in) { fprintf(stderr, "打不开模型: %s\n", input); return 1; }
	if (fseek(in, 0, SEEK_END) != 0 || (length = ftell(in)) < 0 || fseek(in, 0, SEEK_SET) != 0) {
		fclose(in);
		fprintf(stderr, "读不到模型长度。\n");
		return 1;
	}
	size = (size_t)length;
	data = (uint8_t *)malloc(size ? size : 1);
	if (!data || fread(data, 1, size, in) != size) {
		free(data);
		fclose(in);
		fprintf(stderr, "读取模型失败。\n");
		return 1;
	}
	fclose(in);
	sha_init(&sha);
	sha_update(&sha, data, size);
	sha_final(&sha, hash);
	memset(&model, 0, sizeof(model));
	ok = parse_model(data, size, &model);
	free(data);
	if (!ok) {
		fprintf(stderr, "%s\n", g_error ? g_error : "解析失败");
		free_model(&model);
		return 1;
	}
	out = fopen(output, "wb");
	if (!out) {
		fprintf(stderr, "写不出 JSON: %s\n", output);
		free_model(&model);
		return 1;
	}
	memset(&json, 0, sizeof(json));
	json.fp = out;
	json.sp = -1;
	ok = write_model(&json, &model, file_name_of(input), hash, size);
	if (fclose(out) != 0) ok = 0;
	for (i = 0; i < model.animation_count; i++) frames += model.animations[i].frame_count;
	if (!ok) {
		remove(output);
		fprintf(stderr, "%s\n", g_error ? g_error : "写出 JSON 失败");
		free_model(&model);
		return 1;
	}
	printf("%s -> %s  layers %d  textures %d  animations %d  frames %d  axes %d\n",
		file_name_of(input), output, model.layer_count, model.texture_count,
		model.animation_count, frames, model.axis_count);
	free_model(&model);
	return 0;
}
