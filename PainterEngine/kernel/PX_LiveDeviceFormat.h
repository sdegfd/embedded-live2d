#ifndef PX_LIVEDEVICEFORMAT_H
#define PX_LIVEDEVICEFORMAT_H

#include "../core/PX_Typedef.h"

/*
 * RT30 device packages are decoded field-by-field.  The declarations below
 * describe host-side values, not structs that may be cast over file bytes.
 * This keeps the format independent of compiler packing and host endian.
 */

#define PX_LIVE_DEVICE_FOURCC(a,b,c,d) \
	((px_uint32)(px_byte)(a) | ((px_uint32)(px_byte)(b) << 8) | \
	((px_uint32)(px_byte)(c) << 16) | ((px_uint32)(px_byte)(d) << 24))

#define PX_LIVE_DEVICE_MAGIC                  PX_LIVE_DEVICE_FOURCC('P','X','R','3')
#define PX_LIVE_DEVICE_VERSION                1u
#define PX_LIVE_DEVICE_HEADER_SIZE            64u
#define PX_LIVE_DEVICE_CHUNK_ENTRY_SIZE       20u
#define PX_LIVE_DEVICE_LAYOUT_HEADER_SIZE     40u
#define PX_LIVE_DEVICE_RT30_HEADER_SIZE       16u
#define PX_LIVE_DEVICE_RT30_AXIS_ENTRY_SIZE   28u
#define PX_LIVE_DEVICE_RT30_BINDING_ENTRY_SIZE 20u

#define PX_LIVE_DEVICE_CHUNK_BASE             PX_LIVE_DEVICE_FOURCC('B','A','S','E')
#define PX_LIVE_DEVICE_CHUNK_TEXTURE          PX_LIVE_DEVICE_FOURCC('T','E','X',' ')
#define PX_LIVE_DEVICE_CHUNK_RT30             PX_LIVE_DEVICE_FOURCC('R','T','3','0')
#define PX_LIVE_DEVICE_CHUNK_LAYOUT           PX_LIVE_DEVICE_FOURCC('L','A','Y','O')

#define PX_LIVE_DEVICE_RT30_SAMPLE_COUNT      30u
#define PX_LIVE_DEVICE_RT30_MAX_AXES          32u
#define PX_LIVE_DEVICE_RT30_Q15_ONE           32767u
#define PX_LIVE_DEVICE_MAX_FRACTION_BITS      14u
#define PX_LIVE_DEVICE_MAX_CHUNKS              16u
#define PX_LIVE_DEVICE_MAX_LAYERS             256u
#define PX_LIVE_DEVICE_MAX_VERTICES           65535u
#define PX_LIVE_DEVICE_MAX_BINDINGS_PER_AXIS  256u

#define PX_LIVE_DEVICE_ARENA_ALIGNMENT        64u
#define PX_LIVE_DEVICE_BLOCK_ALIGNMENT         4u
#define PX_LIVE_DEVICE_SAMPLE_ALIGNMENT       16u

#define PX_LIVE_DEVICE_PSRAM_LIMIT_BYTES      (6u * 1024u * 1024u)
#define PX_LIVE_DEVICE_BASE_LIMIT_BYTES       (5u * 1024u * 1024u)
#define PX_LIVE_DEVICE_RT30_LIMIT_BYTES       (1u * 1024u * 1024u)
#define PX_LIVE_DEVICE_SAMPLE_LIMIT_BYTES     (640u * 1024u)
#define PX_LIVE_DEVICE_RUNTIME_LIMIT_BYTES    (128u * 1024u)
#define PX_LIVE_DEVICE_METADATA_LIMIT_BYTES   (256u * 1024u)

#define PX_LIVE_DEVICE_PROPERTY_TRANSLATION   0x0001u
#define PX_LIVE_DEVICE_PROPERTY_ROTATION      0x0002u
#define PX_LIVE_DEVICE_PROPERTY_STRETCH       0x0004u
#define PX_LIVE_DEVICE_PROPERTY_VERTEX_DELTA  0x0008u
#define PX_LIVE_DEVICE_PROPERTY_LOCAL_TRANSLATION 0x0010u
#define PX_LIVE_DEVICE_PROPERTY_LOCAL_ROTATION    0x0020u
#define PX_LIVE_DEVICE_PROPERTY_LOCAL_SCALE       0x0040u
#define PX_LIVE_DEVICE_PROPERTY_TEXTURE           0x0080u
#define PX_LIVE_DEVICE_PROPERTY_IMPULSE           0x0100u
#define PX_LIVE_DEVICE_PROPERTY_ALL           0x01ffu

typedef enum
{
	PX_LIVE_DEVICE_ERROR_NONE = 0,
	PX_LIVE_DEVICE_ERROR_INVALID_ARGUMENT,
	PX_LIVE_DEVICE_ERROR_TRUNCATED,
	PX_LIVE_DEVICE_ERROR_ARITHMETIC_OVERFLOW,
	PX_LIVE_DEVICE_ERROR_RANGE_OUTSIDE_FILE,
	PX_LIVE_DEVICE_ERROR_BAD_MAGIC,
	PX_LIVE_DEVICE_ERROR_UNSUPPORTED_VERSION,
	PX_LIVE_DEVICE_ERROR_INVALID_HEADER_SIZE,
	PX_LIVE_DEVICE_ERROR_FILE_SIZE_MISMATCH,
	PX_LIVE_DEVICE_ERROR_INVALID_COUNT,
	PX_LIVE_DEVICE_ERROR_INVALID_ALIGNMENT,
	PX_LIVE_DEVICE_ERROR_DUPLICATE_CHUNK,
	PX_LIVE_DEVICE_ERROR_MISSING_CHUNK,
	PX_LIVE_DEVICE_ERROR_CHUNK_OVERLAP,
	PX_LIVE_DEVICE_ERROR_BAD_CHUNK_SIZE,
	PX_LIVE_DEVICE_ERROR_BASE_BUDGET_EXCEEDED,
	PX_LIVE_DEVICE_ERROR_RT30_BUDGET_EXCEEDED,
	PX_LIVE_DEVICE_ERROR_SAMPLE_BUDGET_EXCEEDED,
	PX_LIVE_DEVICE_ERROR_RUNTIME_BUDGET_EXCEEDED,
	PX_LIVE_DEVICE_ERROR_METADATA_BUDGET_EXCEEDED,
	PX_LIVE_DEVICE_ERROR_PSRAM_LIMIT_EXCEEDED,
	PX_LIVE_DEVICE_ERROR_REQUIRED_PSRAM_MISMATCH,
	PX_LIVE_DEVICE_ERROR_LAYOUT_MISMATCH,
	PX_LIVE_DEVICE_ERROR_AXIS_LIMIT_EXCEEDED,
	PX_LIVE_DEVICE_ERROR_SAMPLE_COUNT_INVALID,
	PX_LIVE_DEVICE_ERROR_MIDDLE_KEY_INVALID,
	PX_LIVE_DEVICE_ERROR_DEFAULT_SAMPLE_INVALID,
	PX_LIVE_DEVICE_ERROR_SAMPLE_STRIDE_INVALID,
	PX_LIVE_DEVICE_ERROR_BINDING_INVALID,
	PX_LIVE_DEVICE_ERROR_PROPERTY_MASK_INVALID,
	PX_LIVE_DEVICE_ERROR_INDEX_OUT_OF_RANGE,
	PX_LIVE_DEVICE_ERROR_HASH_COLLISION,
	PX_LIVE_DEVICE_ERROR_QUANTIZATION_INVALID
} PX_LIVE_DEVICE_ERROR;

typedef struct
{
	const px_byte *data;
	px_uint32 size;
	px_uint32 cursor;
	PX_LIVE_DEVICE_ERROR error;
} PX_LiveDeviceReader;

typedef struct
{
	px_uint32 magic;
	px_uint16 version;
	px_uint16 headerSize;
	px_uint32 fileSize;
	px_uint32 requiredPsramBytes;
	px_uint32 baseBytes;
	px_uint32 rt30SampleBytes;
	px_uint32 rt30RuntimeBytes;
	px_uint32 rt30MetadataBytes;
	px_uint32 chunkTableOffset;
	px_uint16 chunkCount;
	px_uint16 axisCount;
	px_uint16 layerCount;
	px_uint16 vertexCount;
	px_byte coordFractionBits;
	px_byte rotationFractionBits;
	px_byte stretchFractionBits;
	px_byte reserved0;
	px_uint16 flags;
	px_uint16 chunkEntrySize;
} PX_LiveDeviceHeader;

typedef struct
{
	px_uint32 type;
	px_uint16 version;
	px_uint16 flags;
	px_uint32 offset;
	px_uint32 size;
	px_uint32 count;
} PX_LiveDeviceChunk;

typedef struct
{
	px_uint32 baseBytes;
	px_uint32 rt30SampleBytes;
	px_uint32 rt30RuntimeBytes;
	px_uint32 rt30MetadataBytes;
} PX_LiveDeviceMemoryRequest;

typedef struct
{
	px_uint32 baseOffset;
	px_uint32 baseAllocatedBytes;
	px_uint32 rt30SampleOffset;
	px_uint32 rt30SampleAllocatedBytes;
	px_uint32 rt30RuntimeOffset;
	px_uint32 rt30RuntimeAllocatedBytes;
	px_uint32 rt30MetadataOffset;
	px_uint32 rt30MetadataAllocatedBytes;
	px_uint32 requiredPsramBytes;
} PX_LiveDeviceMemoryLayout;

typedef struct
{
	const px_byte *data;
	px_uint32 size;
	PX_LiveDeviceHeader header;
} PX_LiveDeviceFormatView;

px_bool PX_LiveDeviceSafeAddU32(px_uint32 a, px_uint32 b, px_uint32 *result);
px_bool PX_LiveDeviceSafeMulU32(px_uint32 a, px_uint32 b, px_uint32 *result);
px_bool PX_LiveDeviceSafeAlignU32(px_uint32 value, px_uint32 alignment, px_uint32 *result);
px_bool PX_LiveDeviceRangeIsValid(px_uint32 totalSize, px_uint32 offset, px_uint32 size);
px_bool PX_LiveDeviceArrayRangeIsValid(px_uint32 totalSize, px_uint32 offset, px_uint32 count, px_uint32 stride);
px_bool PX_LiveDeviceArenaIsAligned(const px_void *arena);

px_bool PX_LiveDeviceReaderInitialize(PX_LiveDeviceReader *reader, const px_void *data, px_uint32 size);
px_bool PX_LiveDeviceReaderReadU8(PX_LiveDeviceReader *reader, px_byte *value);
px_bool PX_LiveDeviceReaderReadU16LE(PX_LiveDeviceReader *reader, px_uint16 *value);
px_bool PX_LiveDeviceReaderReadI16LE(PX_LiveDeviceReader *reader, px_int16 *value);
px_bool PX_LiveDeviceReaderReadU32LE(PX_LiveDeviceReader *reader, px_uint32 *value);
px_bool PX_LiveDeviceReaderReadI32LE(PX_LiveDeviceReader *reader, px_int32 *value);
px_bool PX_LiveDeviceReaderReadBytes(PX_LiveDeviceReader *reader, px_void *output, px_uint32 size);
px_bool PX_LiveDeviceReaderSkip(PX_LiveDeviceReader *reader, px_uint32 size);

px_bool PX_LiveDeviceMemoryLayoutCompute(const PX_LiveDeviceMemoryRequest *request,
	PX_LiveDeviceMemoryLayout *layout, PX_LIVE_DEVICE_ERROR *error);

px_bool PX_LiveDeviceFormatReadHeader(const px_void *data, px_uint32 size,
	PX_LiveDeviceHeader *header, PX_LIVE_DEVICE_ERROR *error);
px_bool PX_LiveDeviceFormatReadChunk(const PX_LiveDeviceFormatView *view, px_uint32 index,
	PX_LiveDeviceChunk *chunk, PX_LIVE_DEVICE_ERROR *error);
px_bool PX_LiveDeviceFormatFindChunk(const PX_LiveDeviceFormatView *view, px_uint32 type,
	PX_LiveDeviceChunk *chunk, PX_LIVE_DEVICE_ERROR *error);
px_bool PX_LiveDeviceFormatValidate(const px_void *data, px_uint32 size,
	PX_LiveDeviceFormatView *view, PX_LIVE_DEVICE_ERROR *error);

const px_char *PX_LiveDeviceFormatErrorString(PX_LIVE_DEVICE_ERROR error);

#endif
