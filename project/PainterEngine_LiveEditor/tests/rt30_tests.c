#include <stdio.h>
#include <string.h>

#include "kernel/PX_LiveDeviceFormat.h"
#include "kernel/PX_LiveFramework.h"

/*
 * Standalone MSVC command (run after vcvars64.bat):
 * cl /nologo /W4 /TC /utf-8 /I D:\live2d\PainterEngine rt30_tests.c \
 *   PX_LiveDeviceFormat.c PX_LiveRealtime.c PX_MemoryPool.c PX_Typedef.c \
 *   PX_Log.c /Fe:rt30_tests.exe
 */

#define TEST_BUFFER_SIZE 512u
#define TEST_FILE_SIZE 400u
#define TEST_TABLE_OFFSET 64u
#define TEST_RT30_OFFSET 144u
#define TEST_RT30_SIZE 200u
#define TEST_LAYOUT_OFFSET 344u
#define TEST_BASE_OFFSET 384u
#define TEST_TEXTURE_OFFSET 392u
#define TEST_RUNTIME_POOL_SIZE (256u * 1024u)

static int g_checks;
static int g_failures;

typedef struct
{
	px_byte storage[TEST_RUNTIME_POOL_SIZE + PX_LIVE_DEVICE_ARENA_ALIGNMENT];
	px_memorypool pool;
	PX_LiveFramework framework;
	PX_LiveLayer layers[2];
	PX_LiveVertex vertices[2];
} TestRuntimeContext;

/* The standalone test links PX_LiveRealtime.c without the renderer-heavy
 * PX_LiveFramework.c.  This is the reset contract used by Enter/Leave. */
#ifndef PX_RT30_TEST_LINK_FULL_FRAMEWORK
px_void PX_LiveFrameworkReset(PX_LiveFramework *plive)
{
	px_int layerIndex;
	if (!plive)
	{
		return;
	}
	plive->animationMode = PX_LIVE_MODE_NEUTRAL;
	PX_LiveRealtimeResetAll(plive);
	for (layerIndex = 0; layerIndex < plive->layers.size; layerIndex++)
	{
		px_int vertexIndex;
		PX_LiveLayer *layer = PX_VECTORAT(PX_LiveLayer, &plive->layers, layerIndex);
		layer->rel_beginTranslation = PX_POINT(0, 0, 0);
		layer->rel_currentTranslation = PX_POINT(0, 0, 0);
		layer->rel_endTranslation = PX_POINT(0, 0, 0);
		layer->rel_beginRotationAngle = 0;
		layer->rel_currentRotationAngle = 0;
		layer->rel_endRotationAngle = 0;
		layer->rel_beginStretch = 1;
		layer->rel_currentStretch = 1;
		layer->rel_endStretch = 1;
		for (vertexIndex = 0; vertexIndex < layer->vertices.size; vertexIndex++)
		{
			PX_LiveVertex *vertex = PX_VECTORAT(PX_LiveVertex, &layer->vertices,
				vertexIndex);
			vertex->beginTranslation = PX_POINT(0, 0, 0);
			vertex->currentTranslation = PX_POINT(0, 0, 0);
			vertex->endTranslation = PX_POINT(0, 0, 0);
		}
	}
}

px_void PX_LiveFrameworkUpdate(PX_LiveFramework *plive, px_dword elapsed)
{
	px_int i;
	(void)elapsed;
	if (!plive || plive->animationMode != PX_LIVE_MODE_REALTIME30 ||
		!PX_LiveRealtimeUpdate(plive))
	{
		return;
	}
	for (i = 0; i < plive->layers.size; i++)
	{
		PX_LiveLayer *layer = PX_VECTORAT(PX_LiveLayer, &plive->layers, i);
		if (layer->parent_index == -1)
		{
			layer->currentKeyPoint = PX_PointAdd(layer->keyPoint,
				layer->rel_currentTranslation);
		}
		else
		{
			PX_LiveLayer *parent = PX_VECTORAT(PX_LiveLayer, &plive->layers,
				layer->parent_index);
			px_point local = PX_PointMul(PX_PointSub(layer->keyPoint,
				parent->keyPoint), layer->rel_currentStretch);
			local = PX_PointAdd(local, layer->rel_currentTranslation);
			local = PX_PointRotate(local, parent->rel_currentRotationAngle);
			layer->currentKeyPoint = PX_PointAdd(parent->currentKeyPoint, local);
		}
		layer->currentKeyPoint.z = layer->keyPoint.z;
	}
}
#endif

#define CHECK(expression) do { \
	g_checks++; \
	if (!(expression)) { \
		printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
		g_failures++; \
	} \
} while (0)

static void WriteU16(px_byte *buffer, px_uint32 offset, px_uint16 value)
{
	buffer[offset] = (px_byte)(value & 0xffu);
	buffer[offset + 1] = (px_byte)((value >> 8) & 0xffu);
}

static void WriteU32(px_byte *buffer, px_uint32 offset, px_uint32 value)
{
	buffer[offset] = (px_byte)(value & 0xffu);
	buffer[offset + 1] = (px_byte)((value >> 8) & 0xffu);
	buffer[offset + 2] = (px_byte)((value >> 16) & 0xffu);
	buffer[offset + 3] = (px_byte)((value >> 24) & 0xffu);
}

static void InitializeRuntimeContext(TestRuntimeContext *context)
{
	px_uint64 address;
	px_uint32 adjustment;
	px_byte *arena;
	memset(context, 0, sizeof(*context));
	address = (px_uint64)context->storage;
	adjustment = (px_uint32)((PX_LIVE_DEVICE_ARENA_ALIGNMENT -
		(address & (PX_LIVE_DEVICE_ARENA_ALIGNMENT - 1))) &
		(PX_LIVE_DEVICE_ARENA_ALIGNMENT - 1));
	arena = context->storage + adjustment;
	context->pool = MP_Create(arena, TEST_RUNTIME_POOL_SIZE);
	context->framework.mp = &context->pool;
	context->framework.animationMode = PX_LIVE_MODE_NEUTRAL;
	context->framework.layers.data = context->layers;
	context->framework.layers.nodesize = sizeof(context->layers[0]);
	context->framework.layers.size = 1;
	context->framework.layers.allocsize = 1;
	context->framework.layers.mp = &context->pool;
	context->layers[0].vertices.data = &context->vertices[0];
	context->layers[0].vertices.nodesize = sizeof(context->vertices[0]);
	context->layers[0].vertices.size = 1;
	context->layers[0].vertices.allocsize = 1;
	context->layers[0].vertices.mp = &context->pool;
	context->layers[0].rel_beginStretch = 1;
	context->layers[0].rel_currentStretch = 1;
	context->layers[0].rel_endStretch = 1;
	PX_LiveRealtimeInitialize(&context->framework.realtime, &context->pool);
}

static px_bool BakeRuntimeAxis(PX_LiveFramework *framework, const px_char *id,
	px_byte middleKeyIndex, px_float key1Value, px_float key2Value,
	px_int *outHandle)
{
	px_uint16 vertexIndex = 0;
	PX_LiveRealtimeBindingDesc binding;
	PX_LiveRealtimeBindingPose bindingPoses[3];
	PX_LiveRealtimeKeyPose keyPoses[3];
	PX_LiveRealtimeAxisBakeDesc desc;
	px_point vertexDeltas[3];
	px_int i;
	memset(&binding, 0, sizeof(binding));
	memset(bindingPoses, 0, sizeof(bindingPoses));
	memset(keyPoses, 0, sizeof(keyPoses));
	memset(&desc, 0, sizeof(desc));
	vertexDeltas[0] = PX_POINT(0, 0, 0);
	vertexDeltas[1] = PX_POINT(key1Value, key1Value * 2, 0);
	vertexDeltas[2] = PX_POINT(key2Value, key2Value * 2, 0);
	binding.layerIndex = 0;
	binding.propertyMask = PX_LIVE_REALTIME_PROPERTY_TRANSLATION |
		PX_LIVE_REALTIME_PROPERTY_ROTATION |
		PX_LIVE_REALTIME_PROPERTY_STRETCH |
		PX_LIVE_REALTIME_PROPERTY_VERTICES;
	binding.vertexCount = 1;
	binding.vertexIndices = &vertexIndex;
	for (i = 0; i < 3; i++)
	{
		px_float value = i == 0 ? 0 : (i == 1 ? key1Value : key2Value);
		bindingPoses[i].translation = PX_POINT(value, value * 2, 0);
		bindingPoses[i].rotation = value;
		bindingPoses[i].stretch = 1 + value;
		bindingPoses[i].mapTexture = -1;
		bindingPoses[i].vertexDeltas = &vertexDeltas[i];
		keyPoses[i].bindings = &bindingPoses[i];
	}
	desc.id = id;
	desc.middleKeyIndex = middleKeyIndex;
	desc.defaultSampleIndex = 0;
	desc.coordFractionBits = 0;
	desc.rotationFractionBits = 0;
	desc.stretchFractionBits = 0;
	desc.bindingCount = 1;
	desc.bindings = &binding;
	for (i = 0; i < 3; i++)
	{
		desc.keyPoses[i] = keyPoses[i];
	}
	return PX_LiveRealtimeBakeAxis(framework, PX_LIVE_REALTIME_INVALID_HANDLE,
		&desc, outHandle);
}

static px_bool BakeLayerTransformAxis(PX_LiveFramework *framework, const px_char *id,
	px_uint16 layerIndex, px_uint16 propertyMask,
	px_point key1Translation, px_point key2Translation,
	px_float key1Rotation, px_float key2Rotation,
	px_float key1Stretch, px_float key2Stretch, px_int *outHandle)
{
	PX_LiveRealtimeBindingDesc binding;
	PX_LiveRealtimeBindingPose poses[3];
	PX_LiveRealtimeAxisBakeDesc desc;
	px_int i;
	memset(&binding, 0, sizeof(binding));
	memset(poses, 0, sizeof(poses));
	memset(&desc, 0, sizeof(desc));
	binding.layerIndex = layerIndex;
	binding.propertyMask = propertyMask;
	poses[0].translation = PX_POINT(0, 0, 0);
	poses[0].rotation = 0;
	poses[0].stretch = 1;
	poses[0].mapTexture = -1;
	poses[1].mapTexture = -1;
	poses[2].mapTexture = -1;
	poses[1].translation = key1Translation;
	poses[1].rotation = key1Rotation;
	poses[1].stretch = key1Stretch;
	poses[2].translation = key2Translation;
	poses[2].rotation = key2Rotation;
	poses[2].stretch = key2Stretch;
	desc.id = id;
	desc.middleKeyIndex = 15;
	desc.defaultSampleIndex = 0;
	desc.bindingCount = 1;
	desc.bindings = &binding;
	for (i = 0; i < 3; i++)
	{
		desc.keyPoses[i].bindings = &poses[i];
	}
	return PX_LiveRealtimeBakeAxis(framework, PX_LIVE_REALTIME_INVALID_HANDLE,
		&desc, outHandle);
}

static px_bool FloatNear(px_float a, px_float b)
{
	px_float difference = a - b;
	if (difference < 0)
	{
		difference = -difference;
	}
	return difference < 0.001f;
}

static px_int16 ReadRuntimeSampleI16(const PX_LiveRealtimeAxis *axis,
	px_uint32 sampleIndex, px_uint32 byteOffset)
{
	px_int16 value;
	memcpy(&value, (const px_byte *)axis->samples + sampleIndex * axis->sampleStride +
		byteOffset, sizeof(value));
	return value;
}

static void WriteChunk(px_byte *buffer, px_uint32 index, px_uint32 type,
	px_uint32 offset, px_uint32 size, px_uint32 count)
{
	px_uint32 entry = TEST_TABLE_OFFSET + index * PX_LIVE_DEVICE_CHUNK_ENTRY_SIZE;
	WriteU32(buffer, entry, type);
	WriteU16(buffer, entry + 4, PX_LIVE_DEVICE_VERSION);
	WriteU16(buffer, entry + 6, 0);
	WriteU32(buffer, entry + 8, offset);
	WriteU32(buffer, entry + 12, size);
	WriteU32(buffer, entry + 16, count);
}

static void BuildValidPackage(px_byte *buffer)
{
	px_uint32 i;
	memset(buffer, 0, TEST_BUFFER_SIZE);

	/* Package header. */
	WriteU32(buffer, 0, PX_LIVE_DEVICE_MAGIC);
	WriteU16(buffer, 4, PX_LIVE_DEVICE_VERSION);
	WriteU16(buffer, 6, PX_LIVE_DEVICE_HEADER_SIZE);
	WriteU32(buffer, 8, TEST_FILE_SIZE);
	WriteU32(buffer, 12, 368u);
	WriteU32(buffer, 16, 100u);
	WriteU32(buffer, 20, 120u);
	WriteU32(buffer, 24, 64u);
	WriteU32(buffer, 28, 64u);
	WriteU32(buffer, 32, TEST_TABLE_OFFSET);
	WriteU16(buffer, 36, 4u);
	WriteU16(buffer, 38, 1u);
	WriteU16(buffer, 40, 1u);
	WriteU16(buffer, 42, 1u);
	buffer[44] = 8u;
	buffer[45] = 8u;
	buffer[46] = 8u;
	buffer[47] = 0u;
	WriteU16(buffer, 48, 0u);
	WriteU16(buffer, 50, PX_LIVE_DEVICE_CHUNK_ENTRY_SIZE);

	WriteChunk(buffer, 0, PX_LIVE_DEVICE_CHUNK_BASE,
		TEST_BASE_OFFSET, 8u, 1u);
	WriteChunk(buffer, 1, PX_LIVE_DEVICE_CHUNK_TEXTURE,
		TEST_TEXTURE_OFFSET, 8u, 1u);
	WriteChunk(buffer, 2, PX_LIVE_DEVICE_CHUNK_RT30,
		TEST_RT30_OFFSET, TEST_RT30_SIZE, 1u);
	WriteChunk(buffer, 3, PX_LIVE_DEVICE_CHUNK_LAYOUT,
		TEST_LAYOUT_OFFSET, PX_LIVE_DEVICE_LAYOUT_HEADER_SIZE, 1u);

	/* RT30 chunk header. */
	WriteU16(buffer, TEST_RT30_OFFSET, PX_LIVE_DEVICE_VERSION);
	WriteU16(buffer, TEST_RT30_OFFSET + 2, PX_LIVE_DEVICE_RT30_HEADER_SIZE);
	WriteU16(buffer, TEST_RT30_OFFSET + 4, 1u);
	WriteU16(buffer, TEST_RT30_OFFSET + 6, PX_LIVE_DEVICE_RT30_AXIS_ENTRY_SIZE);
	WriteU32(buffer, TEST_RT30_OFFSET + 8, 16u);

	/* One axis at relative offset 16. */
	WriteU32(buffer, TEST_RT30_OFFSET + 16, 0x11223344u);
	buffer[TEST_RT30_OFFSET + 20] = 15u;
	buffer[TEST_RT30_OFFSET + 21] = 0u;
	buffer[TEST_RT30_OFFSET + 22] = PX_LIVE_DEVICE_RT30_SAMPLE_COUNT;
	WriteU16(buffer, TEST_RT30_OFFSET + 24, 1u);
	WriteU16(buffer, TEST_RT30_OFFSET + 26, 0u);
	WriteU32(buffer, TEST_RT30_OFFSET + 28, 4u);
	WriteU32(buffer, TEST_RT30_OFFSET + 32, 44u);
	WriteU32(buffer, TEST_RT30_OFFSET + 36, 80u);

	/* One vertex-delta binding at relative offset 44. */
	WriteU16(buffer, TEST_RT30_OFFSET + 44, 0u);
	WriteU16(buffer, TEST_RT30_OFFSET + 46, PX_LIVE_DEVICE_PROPERTY_VERTEX_DELTA);
	WriteU16(buffer, TEST_RT30_OFFSET + 48, 1u);
	WriteU16(buffer, TEST_RT30_OFFSET + 50, 0u);
	WriteU32(buffer, TEST_RT30_OFFSET + 52, 64u);
	WriteU32(buffer, TEST_RT30_OFFSET + 56, 0u);
	WriteU32(buffer, TEST_RT30_OFFSET + 60, 4u);
	WriteU16(buffer, TEST_RT30_OFFSET + 64, 0u);

	/* Thirty sample-major int16 x/y pairs. */
	for (i = 0; i < PX_LIVE_DEVICE_RT30_SAMPLE_COUNT; i++)
	{
		WriteU16(buffer, TEST_RT30_OFFSET + 80 + i * 4, (px_uint16)i);
		WriteU16(buffer, TEST_RT30_OFFSET + 82 + i * 4, (px_uint16)(i * 2));
	}

	/* Recomputed arena layout. */
	WriteU16(buffer, TEST_LAYOUT_OFFSET, PX_LIVE_DEVICE_VERSION);
	WriteU16(buffer, TEST_LAYOUT_OFFSET + 2, PX_LIVE_DEVICE_LAYOUT_HEADER_SIZE);
	WriteU32(buffer, TEST_LAYOUT_OFFSET + 4, 0u);
	WriteU32(buffer, TEST_LAYOUT_OFFSET + 8, 100u);
	WriteU32(buffer, TEST_LAYOUT_OFFSET + 12, 112u);
	WriteU32(buffer, TEST_LAYOUT_OFFSET + 16, 128u);
	WriteU32(buffer, TEST_LAYOUT_OFFSET + 20, 240u);
	WriteU32(buffer, TEST_LAYOUT_OFFSET + 24, 64u);
	WriteU32(buffer, TEST_LAYOUT_OFFSET + 28, 304u);
	WriteU32(buffer, TEST_LAYOUT_OFFSET + 32, 64u);
	WriteU32(buffer, TEST_LAYOUT_OFFSET + 36, 368u);
}

static void ExpectFormatError(px_byte *buffer, PX_LIVE_DEVICE_ERROR expected)
{
	PX_LiveDeviceFormatView view;
	PX_LIVE_DEVICE_ERROR error = PX_LIVE_DEVICE_ERROR_NONE;
	CHECK(!PX_LiveDeviceFormatValidate(buffer, TEST_FILE_SIZE, &view, &error));
	CHECK(error == expected);
}

static void TestReader(void)
{
	px_byte bytes[] = {0x7fu, 0x34u, 0x12u, 0x78u, 0x56u, 0x34u, 0x12u,
		0xffu, 0xffu};
	PX_LiveDeviceReader reader;
	px_byte u8;
	px_uint16 u16;
	px_uint32 u32;
	px_int16 i16;
	CHECK(PX_LiveDeviceReaderInitialize(&reader, bytes, (px_uint32)sizeof(bytes)));
	CHECK(PX_LiveDeviceReaderReadU8(&reader, &u8) && u8 == 0x7fu);
	CHECK(PX_LiveDeviceReaderReadU16LE(&reader, &u16) && u16 == 0x1234u);
	CHECK(PX_LiveDeviceReaderReadU32LE(&reader, &u32) && u32 == 0x12345678u);
	CHECK(PX_LiveDeviceReaderReadI16LE(&reader, &i16) && i16 == -1);
	CHECK(!PX_LiveDeviceReaderReadU8(&reader, &u8));
	CHECK(reader.error == PX_LIVE_DEVICE_ERROR_TRUNCATED);
	CHECK(!PX_LiveDeviceReaderReadU8(&reader, &u8));
	CHECK(!PX_LiveDeviceReaderInitialize(&reader, PX_NULL, 1u));
}

static void TestSafeArithmeticAndAlignment(void)
{
	px_uint32 value;
	px_byte storage[192];
	px_byte *aligned;
	px_uint64 address = (px_uint64)storage;
	px_uint32 adjustment = (px_uint32)((PX_LIVE_DEVICE_ARENA_ALIGNMENT -
		(address & (PX_LIVE_DEVICE_ARENA_ALIGNMENT - 1))) &
		(PX_LIVE_DEVICE_ARENA_ALIGNMENT - 1));
	CHECK(PX_LiveDeviceSafeAddU32(10u, 20u, &value) && value == 30u);
	CHECK(!PX_LiveDeviceSafeAddU32(0xffffffffu, 1u, &value));
	CHECK(PX_LiveDeviceSafeMulU32(1024u, 1024u, &value) && value == 1048576u);
	CHECK(!PX_LiveDeviceSafeMulU32(0xffffffffu, 2u, &value));
	CHECK(PX_LiveDeviceSafeAlignU32(17u, 16u, &value) && value == 32u);
	CHECK(!PX_LiveDeviceSafeAlignU32(17u, 3u, &value));
	CHECK(!PX_LiveDeviceSafeAlignU32(0xfffffff9u, 16u, &value));
	CHECK(PX_LiveDeviceRangeIsValid(10u, 10u, 0u));
	CHECK(!PX_LiveDeviceRangeIsValid(10u, 9u, 2u));
	CHECK(!PX_LiveDeviceArrayRangeIsValid(100u, 0u, 0xffffffffu, 8u));
	aligned = storage + adjustment;
	CHECK(PX_LiveDeviceArenaIsAligned(aligned));
	CHECK(!PX_LiveDeviceArenaIsAligned(aligned + 1));
	CHECK(!PX_LiveDeviceArenaIsAligned(PX_NULL));
}

static void TestMemoryBudgets(void)
{
	PX_LiveDeviceMemoryRequest request;
	PX_LiveDeviceMemoryLayout layout;
	PX_LIVE_DEVICE_ERROR error;
	request.baseBytes = PX_LIVE_DEVICE_BASE_LIMIT_BYTES;
	request.rt30SampleBytes = PX_LIVE_DEVICE_SAMPLE_LIMIT_BYTES;
	request.rt30RuntimeBytes = PX_LIVE_DEVICE_RUNTIME_LIMIT_BYTES;
	request.rt30MetadataBytes = PX_LIVE_DEVICE_METADATA_LIMIT_BYTES;
	CHECK(PX_LiveDeviceMemoryLayoutCompute(&request, &layout, &error));
	CHECK(error == PX_LIVE_DEVICE_ERROR_NONE);
	CHECK(layout.requiredPsramBytes == PX_LIVE_DEVICE_PSRAM_LIMIT_BYTES);
	CHECK(layout.rt30SampleOffset == PX_LIVE_DEVICE_BASE_LIMIT_BYTES);

	request.baseBytes = PX_LIVE_DEVICE_BASE_LIMIT_BYTES + 1u;
	request.rt30SampleBytes = request.rt30RuntimeBytes = request.rt30MetadataBytes = 0u;
	CHECK(!PX_LiveDeviceMemoryLayoutCompute(&request, &layout, &error));
	CHECK(error == PX_LIVE_DEVICE_ERROR_BASE_BUDGET_EXCEEDED);

	request.baseBytes = 0u;
	request.rt30SampleBytes = PX_LIVE_DEVICE_SAMPLE_LIMIT_BYTES + 1u;
	request.rt30RuntimeBytes = request.rt30MetadataBytes = 0u;
	CHECK(!PX_LiveDeviceMemoryLayoutCompute(&request, &layout, &error));
	CHECK(error == PX_LIVE_DEVICE_ERROR_SAMPLE_BUDGET_EXCEEDED);

	request.rt30SampleBytes = 0u;
	request.rt30RuntimeBytes = PX_LIVE_DEVICE_RUNTIME_LIMIT_BYTES + 1u;
	CHECK(!PX_LiveDeviceMemoryLayoutCompute(&request, &layout, &error));
	CHECK(error == PX_LIVE_DEVICE_ERROR_RUNTIME_BUDGET_EXCEEDED);

	request.rt30RuntimeBytes = 0u;
	request.rt30MetadataBytes = PX_LIVE_DEVICE_METADATA_LIMIT_BYTES + 1u;
	CHECK(!PX_LiveDeviceMemoryLayoutCompute(&request, &layout, &error));
	CHECK(error == PX_LIVE_DEVICE_ERROR_METADATA_BUDGET_EXCEEDED);

	request.rt30SampleBytes = PX_LIVE_DEVICE_SAMPLE_LIMIT_BYTES;
	request.rt30RuntimeBytes = PX_LIVE_DEVICE_RUNTIME_LIMIT_BYTES;
	request.rt30MetadataBytes = PX_LIVE_DEVICE_METADATA_LIMIT_BYTES + 1u;
	CHECK(!PX_LiveDeviceMemoryLayoutCompute(&request, &layout, &error));
	CHECK(error == PX_LIVE_DEVICE_ERROR_RT30_BUDGET_EXCEEDED);

	request.baseBytes = 0xffffffffu;
	request.rt30SampleBytes = request.rt30RuntimeBytes = request.rt30MetadataBytes = 0u;
	CHECK(!PX_LiveDeviceMemoryLayoutCompute(&request, &layout, &error));
	CHECK(error == PX_LIVE_DEVICE_ERROR_ARITHMETIC_OVERFLOW);
}

static void TestValidFormat(void)
{
	px_byte buffer[TEST_BUFFER_SIZE];
	PX_LiveDeviceFormatView view;
	PX_LiveDeviceChunk chunk;
	PX_LIVE_DEVICE_ERROR error;
	px_uint32 i;
	BuildValidPackage(buffer);
	for (i = 0; i < 32u; i++)
	{
		CHECK(PX_LiveDeviceFormatValidate(buffer, TEST_FILE_SIZE, &view, &error));
		CHECK(error == PX_LIVE_DEVICE_ERROR_NONE);
	}
	CHECK(view.header.axisCount == 1u);
	CHECK(view.header.requiredPsramBytes == 368u);
	CHECK(PX_LiveDeviceFormatFindChunk(&view, PX_LIVE_DEVICE_CHUNK_RT30,
		&chunk, &error));
	CHECK(chunk.offset == TEST_RT30_OFFSET && chunk.size == TEST_RT30_SIZE);
	CHECK(strcmp(PX_LiveDeviceFormatErrorString(PX_LIVE_DEVICE_ERROR_NONE), "none") == 0);
}

static void TestHeaderAndChunkFailures(void)
{
	px_byte buffer[TEST_BUFFER_SIZE];
	PX_LiveDeviceHeader header;
	PX_LIVE_DEVICE_ERROR error;
	BuildValidPackage(buffer);
	CHECK(!PX_LiveDeviceFormatReadHeader(buffer, 63u, &header, &error));
	CHECK(error == PX_LIVE_DEVICE_ERROR_TRUNCATED);

	BuildValidPackage(buffer);
	WriteU32(buffer, 0, 0u);
	ExpectFormatError(buffer, PX_LIVE_DEVICE_ERROR_BAD_MAGIC);

	BuildValidPackage(buffer);
	WriteU32(buffer, 8, TEST_FILE_SIZE - 1u);
	ExpectFormatError(buffer, PX_LIVE_DEVICE_ERROR_FILE_SIZE_MISMATCH);

	BuildValidPackage(buffer);
	WriteU32(buffer, 12, 369u);
	ExpectFormatError(buffer, PX_LIVE_DEVICE_ERROR_REQUIRED_PSRAM_MISMATCH);

	BuildValidPackage(buffer);
	WriteU32(buffer, 32, 0xfffffffcu);
	ExpectFormatError(buffer, PX_LIVE_DEVICE_ERROR_RANGE_OUTSIDE_FILE);

	BuildValidPackage(buffer);
	WriteU32(buffer, TEST_TABLE_OFFSET + PX_LIVE_DEVICE_CHUNK_ENTRY_SIZE + 8,
		TEST_BASE_OFFSET);
	ExpectFormatError(buffer, PX_LIVE_DEVICE_ERROR_CHUNK_OVERLAP);

	BuildValidPackage(buffer);
	WriteU32(buffer, TEST_TABLE_OFFSET + PX_LIVE_DEVICE_CHUNK_ENTRY_SIZE,
		PX_LIVE_DEVICE_CHUNK_BASE);
	ExpectFormatError(buffer, PX_LIVE_DEVICE_ERROR_DUPLICATE_CHUNK);

	BuildValidPackage(buffer);
	WriteU32(buffer, TEST_TABLE_OFFSET + PX_LIVE_DEVICE_CHUNK_ENTRY_SIZE,
		PX_LIVE_DEVICE_FOURCC('U','N','K','N'));
	ExpectFormatError(buffer, PX_LIVE_DEVICE_ERROR_MISSING_CHUNK);

	BuildValidPackage(buffer);
	WriteU32(buffer, TEST_LAYOUT_OFFSET + 4, 4u);
	ExpectFormatError(buffer, PX_LIVE_DEVICE_ERROR_LAYOUT_MISMATCH);
}

static void TestRT30Failures(void)
{
	px_byte buffer[TEST_BUFFER_SIZE];
	BuildValidPackage(buffer);
	buffer[TEST_RT30_OFFSET + 22] = 29u;
	ExpectFormatError(buffer, PX_LIVE_DEVICE_ERROR_SAMPLE_COUNT_INVALID);

	BuildValidPackage(buffer);
	buffer[TEST_RT30_OFFSET + 20] = 0u;
	ExpectFormatError(buffer, PX_LIVE_DEVICE_ERROR_MIDDLE_KEY_INVALID);

	BuildValidPackage(buffer);
	buffer[TEST_RT30_OFFSET + 21] = 7u;
	ExpectFormatError(buffer, PX_LIVE_DEVICE_ERROR_DEFAULT_SAMPLE_INVALID);

	BuildValidPackage(buffer);
	WriteU16(buffer, TEST_RT30_OFFSET + 46, 0x8000u);
	ExpectFormatError(buffer, PX_LIVE_DEVICE_ERROR_PROPERTY_MASK_INVALID);

	BuildValidPackage(buffer);
	WriteU16(buffer, TEST_RT30_OFFSET + 64, 1u);
	ExpectFormatError(buffer, PX_LIVE_DEVICE_ERROR_INDEX_OUT_OF_RANGE);

	BuildValidPackage(buffer);
	WriteU32(buffer, TEST_RT30_OFFSET + 28, 0xffffffffu);
	ExpectFormatError(buffer, PX_LIVE_DEVICE_ERROR_SAMPLE_STRIDE_INVALID);

	BuildValidPackage(buffer);
	WriteU32(buffer, 20, 124u);
	ExpectFormatError(buffer, PX_LIVE_DEVICE_ERROR_LAYOUT_MISMATCH);

	BuildValidPackage(buffer);
	WriteU32(buffer, TEST_RT30_OFFSET + 16, 0u);
	ExpectFormatError(buffer, PX_LIVE_DEVICE_ERROR_HASH_COLLISION);
}

static void TestBakeMiddleCase(px_byte middleKeyIndex, px_uint32 processSample,
	px_int16 expectedProcess)
{
	TestRuntimeContext context;
	const PX_LiveRealtimeAxis *axis;
	px_int handle = PX_LIVE_REALTIME_INVALID_HANDLE;
	InitializeRuntimeContext(&context);
	CHECK(BakeRuntimeAxis(&context.framework, "BakeAxis", middleKeyIndex,
		100.0f, 200.0f, &handle));
	axis = PX_LiveRealtimeGetAxisConst(&context.framework, handle);
	CHECK(axis != PX_NULL);
	if (axis)
	{
		CHECK(axis->sampleStride == 12u);
		CHECK(axis->sampleBytes == axis->sampleStride * PX_LIVE_REALTIME_SAMPLE_COUNT);
		CHECK(ReadRuntimeSampleI16(axis, 0u, 0u) == 0);
		CHECK(ReadRuntimeSampleI16(axis, 0u, 4u) == 0);
		CHECK(ReadRuntimeSampleI16(axis, 0u, 6u) == 0);
		CHECK(ReadRuntimeSampleI16(axis, 0u, 8u) == 0);
		CHECK(ReadRuntimeSampleI16(axis, middleKeyIndex, 0u) == 100);
		CHECK(ReadRuntimeSampleI16(axis, middleKeyIndex, 4u) == 100);
		CHECK(ReadRuntimeSampleI16(axis, middleKeyIndex, 6u) == 100);
		CHECK(ReadRuntimeSampleI16(axis, middleKeyIndex, 8u) == 100);
		CHECK(ReadRuntimeSampleI16(axis, 29u, 0u) == 200);
		CHECK(ReadRuntimeSampleI16(axis, 29u, 4u) == 200);
		CHECK(ReadRuntimeSampleI16(axis, 29u, 6u) == 200);
		CHECK(ReadRuntimeSampleI16(axis, 29u, 8u) == 200);
		CHECK(ReadRuntimeSampleI16(axis, processSample, 0u) == expectedProcess);
		CHECK(ReadRuntimeSampleI16(axis, processSample, 8u) == expectedProcess);
	}
	PX_LiveRealtimeFree(&context.framework.realtime);
}

static void TestThreeKeyBake(void)
{
	/* M=1: sample 15 is halfway from K1 to K2. */
	TestBakeMiddleCase(1u, 15u, 150);
	/* M=15: sample 22 is halfway from K1 to K2. */
	TestBakeMiddleCase(15u, 22u, 150);
	/* M=28: sample 14 is halfway from K0 to K1. */
	TestBakeMiddleCase(28u, 14u, 50);
}

static void TestStrictDiscreteSelection(void)
{
	TestRuntimeContext context;
	PX_LiveRealtimeAxis *axis;
	PX_LiveRealtimeMemoryStats stats;
	px_int handle = PX_LIVE_REALTIME_INVALID_HANDLE;
	px_int16 selected, adjacent;
	InitializeRuntimeContext(&context);
	CHECK(BakeRuntimeAxis(&context.framework, "DiscreteAxis", 15u,
		100.0f, 200.0f, &handle));
	CHECK(PX_LiveRealtimeEnter(&context.framework));
	axis = PX_LiveRealtimeGetAxis(&context.framework, handle);
	CHECK(axis != PX_NULL);
	if (!axis)
	{
		PX_LiveRealtimeFree(&context.framework.realtime);
		return;
	}
	CHECK(PX_LiveRealtimeSetAxisSample(&context.framework, handle, 7u,
		PX_LIVE_REALTIME_WEIGHT_ONE_Q15));
	CHECK(PX_LiveRealtimeUpdate(&context.framework));
	selected = ReadRuntimeSampleI16(axis, 7u, 0u);
	adjacent = ReadRuntimeSampleI16(axis, 8u, 0u);
	CHECK(axis->sampleIndex == 7u);
	CHECK(selected != adjacent);
	CHECK(context.layers[0].rel_currentTranslation.x == (px_float)selected);
	CHECK(context.layers[0].rel_currentTranslation.x != (px_float)adjacent);
	PX_LiveRealtimeGetMemoryStats(&context.framework, &stats);
	CHECK(stats.activeAxisCount == 1u);
	CHECK(stats.selectedSampleBytes == axis->sampleStride);
	CHECK(!PX_LiveRealtimeSetAxisSample(&context.framework, handle, 30u,
		PX_LIVE_REALTIME_WEIGHT_ONE_Q15));
	CHECK(axis->sampleIndex == 7u);

	CHECK(PX_LiveRealtimeSetAxisNormalizedQ15(&context.framework, handle, 0u,
		PX_LIVE_REALTIME_WEIGHT_ONE_Q15));
	CHECK(axis->sampleIndex == 0u);
	CHECK(PX_LiveRealtimeSetAxisNormalizedQ15(&context.framework, handle, 16384u,
		PX_LIVE_REALTIME_WEIGHT_ONE_Q15));
	CHECK(axis->sampleIndex == 15u);
	CHECK(PX_LiveRealtimeSetAxisNormalizedQ15(&context.framework, handle, 32767u,
		PX_LIVE_REALTIME_WEIGHT_ONE_Q15));
	CHECK(axis->sampleIndex == 29u);
	CHECK(PX_LiveRealtimeUpdate(&context.framework));
	CHECK(context.layers[0].rel_currentTranslation.x == 200.0f);
	PX_LiveRealtimeFree(&context.framework.realtime);
}

static void TestMultiAxisQ15AndZeroAllocationUpdate(void)
{
	TestRuntimeContext context;
	PX_LiveRealtimeMemoryStats stats;
	px_int handles[3];
	px_uint32 freeSizeBefore, peakBefore;
	px_int i;
	InitializeRuntimeContext(&context);
	handles[0] = handles[1] = handles[2] = PX_LIVE_REALTIME_INVALID_HANDLE;
	CHECK(BakeRuntimeAxis(&context.framework, "AxisA", 15u, 5.0f, 10.0f,
		&handles[0]));
	CHECK(BakeRuntimeAxis(&context.framework, "AxisB", 15u, 10.0f, 20.0f,
		&handles[1]));
	CHECK(BakeRuntimeAxis(&context.framework, "AxisC", 15u, -2.0f, -4.0f,
		&handles[2]));
	CHECK(PX_LiveRealtimeEnter(&context.framework));
	CHECK(PX_LiveRealtimeSetAxisSample(&context.framework, handles[0], 29u, 32767u));
	CHECK(PX_LiveRealtimeSetAxisSample(&context.framework, handles[1], 29u, 16384u));
	CHECK(PX_LiveRealtimeSetAxisSample(&context.framework, handles[2], 29u, 8192u));
	CHECK(PX_LiveRealtimeUpdate(&context.framework));
	CHECK(context.layers[0].rel_currentTranslation.x == 19.0f);
	CHECK(context.layers[0].rel_currentTranslation.y == 38.0f);
	CHECK(context.layers[0].rel_currentRotationAngle == 19.0f);
	CHECK(context.layers[0].rel_currentStretch == 20.0f);
	CHECK(context.vertices[0].currentTranslation.x == 19.0f);
	CHECK(context.vertices[0].currentTranslation.y == 38.0f);
	PX_LiveRealtimeGetMemoryStats(&context.framework, &stats);
	CHECK(stats.activeAxisCount == 3u);
	CHECK(stats.selectedSampleBytes == 36u);

	freeSizeBefore = context.pool.FreeSize;
	peakBefore = MP_GetPeakUsage(&context.pool);
	for (i = 0; i < 1000; i++)
	{
		CHECK(PX_LiveRealtimeSetAxisSample(&context.framework, handles[0],
			(px_byte)((i & 1) ? 29 : 28), 32767u));
		CHECK(PX_LiveRealtimeUpdate(&context.framework));
	}
	CHECK(context.pool.FreeSize == freeSizeBefore);
	CHECK(MP_GetPeakUsage(&context.pool) == peakBefore);
	CHECK(PX_LiveRealtimeUpdate(&context.framework));
	CHECK(context.pool.FreeSize == freeSizeBefore);
	CHECK(MP_GetPeakUsage(&context.pool) == peakBefore);
	PX_LiveRealtimeFree(&context.framework.realtime);
}

static void TestSelectedAxisSoloReset(void)
{
	TestRuntimeContext context;
	PX_LiveRealtimeMemoryStats stats;
	px_int eyeHandle = PX_LIVE_REALTIME_INVALID_HANDLE;
	px_int neckHandle = PX_LIVE_REALTIME_INVALID_HANDLE;
	InitializeRuntimeContext(&context);
	CHECK(BakeRuntimeAxis(&context.framework, "EyeAxis", 15u, 5.0f, 10.0f,
		&eyeHandle));
	CHECK(BakeRuntimeAxis(&context.framework, "NeckAxis", 15u, 10.0f, 20.0f,
		&neckHandle));
	CHECK(PX_LiveRealtimeEnter(&context.framework));
	CHECK(PX_LiveRealtimeSetAxisSample(&context.framework, neckHandle, 29u,
		PX_LIVE_REALTIME_WEIGHT_ONE_Q15));
	CHECK(PX_LiveRealtimeUpdate(&context.framework));
	CHECK(context.layers[0].rel_currentTranslation.x == 20.0f);

	/* Editor solo preview contract: clear every runtime weight, then activate
	   only the currently selected axis.  The previous neck endpoint must not
	   be restored while inspecting the eye's 30 samples. */
	PX_LiveRealtimeResetAll(&context.framework);
	CHECK(PX_LiveRealtimeSetAxisSample(&context.framework, eyeHandle, 29u,
		PX_LIVE_REALTIME_WEIGHT_ONE_Q15));
	CHECK(PX_LiveRealtimeUpdate(&context.framework));
	CHECK(context.layers[0].rel_currentTranslation.x == 10.0f);
	PX_LiveRealtimeGetMemoryStats(&context.framework, &stats);
	CHECK(stats.activeAxisCount == 1u);
	PX_LiveRealtimeFree(&context.framework.realtime);
}

static void TestChildLocalTranslation(void)
{
	TestRuntimeContext context;
	px_int parentHandle = PX_LIVE_REALTIME_INVALID_HANDLE;
	px_int childHandle = PX_LIVE_REALTIME_INVALID_HANDLE;
	px_point expectedLocal, expectedWorld;
	px_int layerIndex, childIndex;
	InitializeRuntimeContext(&context);
	context.framework.layers.size = 2;
	for (layerIndex = 0; layerIndex < 2; layerIndex++)
	{
		context.layers[layerIndex].parent_index = -1;
		context.layers[layerIndex].RenderTextureIndex = -1;
		context.layers[layerIndex].LinkTextureIndex = -1;
		context.layers[layerIndex].vertices.data = &context.vertices[layerIndex];
		context.layers[layerIndex].vertices.nodesize = sizeof(context.vertices[0]);
		context.layers[layerIndex].vertices.size = 0;
		context.layers[layerIndex].vertices.allocsize = 1;
		context.layers[layerIndex].vertices.mp = &context.pool;
		for (childIndex = 0; childIndex < PX_LIVE_LAYER_MAX_LINK_NODE; childIndex++)
		{
			context.layers[layerIndex].child_index[childIndex] = -1;
		}
	}
	context.layers[0].child_index[0] = 1;
	context.layers[1].parent_index = 0;
	context.layers[0].keyPoint = PX_POINT(10, 10, 0);
	context.layers[1].keyPoint = PX_POINT(20, 10, 0);
	CHECK(BakeLayerTransformAxis(&context.framework, "ParentRotation", 0,
		PX_LIVE_REALTIME_PROPERTY_ROTATION, PX_POINT(0, 0, 0),
		PX_POINT(0, 0, 0), 45, 90, 1, 1, &parentHandle));
	CHECK(BakeLayerTransformAxis(&context.framework, "ChildLocal", 1,
		PX_LIVE_REALTIME_PROPERTY_TRANSLATION | PX_LIVE_REALTIME_PROPERTY_STRETCH,
		PX_POINT(0, 2, 0), PX_POINT(0, 5, 0), 0, 0, 1, 2, &childHandle));
	CHECK(PX_LiveRealtimeEnter(&context.framework));
	CHECK(PX_LiveRealtimeSetAxisSample(&context.framework, parentHandle, 29,
		PX_LIVE_REALTIME_WEIGHT_ONE_Q15));
	CHECK(PX_LiveRealtimeSetAxisSample(&context.framework, childHandle, 29,
		PX_LIVE_REALTIME_WEIGHT_ONE_Q15));
	PX_LiveFrameworkUpdate(&context.framework, 0);
	CHECK(FloatNear(context.layers[0].rel_currentRotationAngle, 90));
	CHECK(FloatNear(context.layers[1].rel_currentTranslation.x, 0));
	CHECK(FloatNear(context.layers[1].rel_currentTranslation.y, 5));
	CHECK(FloatNear(context.layers[1].rel_currentStretch, 2));
	expectedLocal = PX_PointMul(PX_PointSub(context.layers[1].keyPoint,
		context.layers[0].keyPoint), context.layers[1].rel_currentStretch);
	expectedLocal = PX_PointAdd(expectedLocal,
		context.layers[1].rel_currentTranslation);
	expectedLocal = PX_PointRotate(expectedLocal,
		context.layers[0].rel_currentRotationAngle);
	expectedWorld = PX_PointAdd(context.layers[0].currentKeyPoint, expectedLocal);
	CHECK(FloatNear(context.layers[1].currentKeyPoint.x, expectedWorld.x));
	CHECK(FloatNear(context.layers[1].currentKeyPoint.y, expectedWorld.y));
	PX_LiveRealtimeFree(&context.framework.realtime);
}

static void TestCloneAndIndependentFree(void)
{
	TestRuntimeContext sourceContext;
	TestRuntimeContext cloneContext;
	const PX_LiveRealtimeAxis *sourceAxis;
	PX_LiveRealtimeAxis *cloneAxis;
	px_int handle = PX_LIVE_REALTIME_INVALID_HANDLE;
	px_int16 replacement = 123;
	InitializeRuntimeContext(&sourceContext);
	InitializeRuntimeContext(&cloneContext);
	CHECK(BakeRuntimeAxis(&sourceContext.framework, "CloneAxis", 15u,
		100.0f, 200.0f, &handle));
	CHECK(PX_LiveRealtimePrepareRuntime(&sourceContext.framework));
	CHECK(PX_LiveRealtimeClone(&cloneContext.framework.realtime,
		&cloneContext.pool, &sourceContext.framework.realtime));
	CHECK(PX_LiveRealtimePrepareRuntime(&cloneContext.framework));
	sourceAxis = PX_LiveRealtimeGetAxisConst(&sourceContext.framework, handle);
	cloneAxis = PX_LiveRealtimeGetAxis(&cloneContext.framework, handle);
	CHECK(sourceAxis != PX_NULL && cloneAxis != PX_NULL);
	if (!sourceAxis || !cloneAxis)
	{
		PX_LiveRealtimeFree(&sourceContext.framework.realtime);
		PX_LiveRealtimeFree(&cloneContext.framework.realtime);
		return;
	}
	CHECK(sourceAxis->bindings != cloneAxis->bindings);
	CHECK(sourceAxis->vertexIndices != cloneAxis->vertexIndices);
	CHECK(sourceAxis->samples != cloneAxis->samples);
	CHECK(sourceContext.framework.realtime.runtimeBlock !=
		cloneContext.framework.realtime.runtimeBlock);
	CHECK(memcmp(sourceAxis->bindings, cloneAxis->bindings,
		sourceAxis->bindingCount * sizeof(sourceAxis->bindings[0])) == 0);
	CHECK(memcmp(sourceAxis->vertexIndices, cloneAxis->vertexIndices,
		sourceAxis->vertexIndexCount * sizeof(sourceAxis->vertexIndices[0])) == 0);
	CHECK(memcmp(sourceAxis->samples, cloneAxis->samples,
		sourceAxis->sampleBytes) == 0);
	memcpy(cloneAxis->samples, &replacement, sizeof(replacement));
	CHECK(ReadRuntimeSampleI16(sourceAxis, 0, 0) == 0);
	CHECK(ReadRuntimeSampleI16(cloneAxis, 0, 0) == replacement);
	PX_LiveRealtimeFree(&sourceContext.framework.realtime);
	CHECK(MP_GetCurrentUsage(&sourceContext.pool) == 0u);
	CHECK(PX_LiveRealtimeEnter(&cloneContext.framework));
	CHECK(PX_LiveRealtimeSetAxisSample(&cloneContext.framework, handle, 29,
		PX_LIVE_REALTIME_WEIGHT_ONE_Q15));
	CHECK(PX_LiveRealtimeUpdate(&cloneContext.framework));
	CHECK(cloneContext.layers[0].rel_currentTranslation.x == 200.0f);
	PX_LiveRealtimeFree(&cloneContext.framework.realtime);
	CHECK(MP_GetCurrentUsage(&cloneContext.pool) == 0u);
}

#ifdef PX_RT30_TEST_LINK_FULL_FRAMEWORK
static px_uint32 FindRT30Trailer(const px_byte *data, px_uint32 size)
{
	px_uint32 i;
	if (size < 4) return 0xffffffffu;
	for (i = 0; i <= size - 4; i++)
	{
		px_uint32 value = (px_uint32)data[i] |
			((px_uint32)data[i + 1] << 8) |
			((px_uint32)data[i + 2] << 16) |
			((px_uint32)data[i + 3] << 24);
		if (value == PX_LIVE_RT30_TRAILER_MAGIC) return i;
	}
	return 0xffffffffu;
}

static void TestLiveRT30PersistenceSource(void)
{
	TestRuntimeContext source;
	TestRuntimeContext imported;
	TestRuntimeContext legacy;
	TestRuntimeContext corruptOffset;
	TestRuntimeContext corruptCount;
	PX_LiveRealtimeAxis *sourceAxis;
	const PX_LiveRealtimeAxis *importedAxis;
	px_memory exported;
	px_byte damaged[8192];
	px_uint32 trailerOffset;
	px_int handle = PX_LIVE_REALTIME_INVALID_HANDLE;
	px_int i;
	InitializeRuntimeContext(&source);
	for (i = 0; i < PX_LIVE_LAYER_MAX_LINK_NODE; i++)
	{
		source.layers[0].child_index[i] = -1;
	}
	source.layers[0].parent_index = -1;
	CHECK(BakeRuntimeAxis(&source.framework, "PersistAxis", 15u,
		100.0f, 200.0f, &handle));
	sourceAxis = PX_LiveRealtimeGetAxis(&source.framework, handle);
	CHECK(sourceAxis != PX_NULL);
	PX_MemoryInitialize(&source.pool, &exported);
	CHECK(PX_LiveFrameworkExport(&source.framework, &exported));
	trailerOffset = FindRT30Trailer(exported.buffer, (px_uint32)exported.usedsize);
	CHECK(trailerOffset != 0xffffffffu);
	CHECK(exported.usedsize < (px_int)sizeof(damaged));

	InitializeRuntimeContext(&imported);
	CHECK(PX_LiveFrameworkImport(&imported.pool, &imported.framework,
		exported.buffer, exported.usedsize));
	CHECK(PX_LiveRealtimeGetAxisCount(&imported.framework) == 1);
	importedAxis = PX_LiveRealtimeGetAxisConst(&imported.framework, 0);
	CHECK(importedAxis != PX_NULL);
	if (sourceAxis && importedAxis)
	{
		CHECK(importedAxis->idHash == sourceAxis->idHash);
		CHECK(importedAxis->middleKeyIndex == sourceAxis->middleKeyIndex);
		CHECK(importedAxis->defaultSampleIndex == sourceAxis->defaultSampleIndex);
		CHECK(importedAxis->sampleIndex == importedAxis->defaultSampleIndex);
		CHECK(importedAxis->weightQ15 == 0);
		CHECK(importedAxis->bindingCount == sourceAxis->bindingCount);
		CHECK(importedAxis->vertexIndexCount == sourceAxis->vertexIndexCount);
		CHECK(importedAxis->sampleBytes == sourceAxis->sampleBytes);
		CHECK(memcmp(importedAxis->bindings, sourceAxis->bindings,
			sourceAxis->bindingCount * sizeof(sourceAxis->bindings[0])) == 0);
		CHECK(memcmp(importedAxis->vertexIndices, sourceAxis->vertexIndices,
			sourceAxis->vertexIndexCount * sizeof(sourceAxis->vertexIndices[0])) == 0);
		CHECK(memcmp(importedAxis->samples, sourceAxis->samples,
			sourceAxis->sampleBytes) == 0);
	}
	PX_LiveFrameworkFree(&imported.framework);

	/* The original v1 body remains importable without the optional trailer. */
	InitializeRuntimeContext(&legacy);
	CHECK(PX_LiveFrameworkImport(&legacy.pool, &legacy.framework,
		exported.buffer, (px_int)trailerOffset));
	CHECK(PX_LiveRealtimeGetAxisCount(&legacy.framework) == 0);
	PX_LiveFrameworkFree(&legacy.framework);

	memcpy(damaged, exported.buffer, exported.usedsize);
	WriteU32(damaged, trailerOffset + PX_LIVE_RT30_TRAILER_HEADER_SIZE + 56u,
		0xfffffffcu);
	InitializeRuntimeContext(&corruptOffset);
	CHECK(!PX_LiveFrameworkImport(&corruptOffset.pool, &corruptOffset.framework,
		damaged, exported.usedsize));

	memcpy(damaged, exported.buffer, exported.usedsize);
	WriteU16(damaged, trailerOffset + PX_LIVE_RT30_TRAILER_HEADER_SIZE + 42u,
		0xffffu);
	InitializeRuntimeContext(&corruptCount);
	CHECK(!PX_LiveFrameworkImport(&corruptCount.pool, &corruptCount.framework,
		damaged, exported.usedsize));

	PX_MemoryFree(&exported);
	PX_LiveRealtimeFree(&source.framework.realtime);
}
#endif

static void TestTopologyOffsetRebuild(void)
{
	TestRuntimeContext context;
	InitializeRuntimeContext(&context);
	context.framework.layers.size = 2;
	context.layers[1] = context.layers[0];
	context.layers[0].vertices.size = 1;
	context.layers[1].vertices.size = 1;
	context.layers[1].vertices.data = &context.vertices[1];
	CHECK(PX_LiveRealtimePrepareRuntime(&context.framework));
	CHECK(context.framework.realtime.vertexOffsets[0] == 0u);
	CHECK(context.framework.realtime.vertexOffsets[1] == 1u);
	CHECK(context.framework.realtime.vertexOffsets[2] == 2u);
	context.layers[0].vertices.size = 0;
	context.layers[1].vertices.size = 2;
	CHECK(PX_LiveRealtimePrepareRuntime(&context.framework));
	CHECK(context.framework.realtime.vertexOffsets[0] == 0u);
	CHECK(context.framework.realtime.vertexOffsets[1] == 0u);
	CHECK(context.framework.realtime.vertexOffsets[2] == 2u);
	PX_LiveRealtimeFree(&context.framework.realtime);
}

static void TestContinuousPositionEndpoints(void)
{
	TestRuntimeContext context;
	PX_LiveRealtimeTransition transition;
	px_int handle = PX_LIVE_REALTIME_INVALID_HANDLE;
	px_float atStart, atEnd, atMiddleTime;
	InitializeRuntimeContext(&context);
	CHECK(BakeRuntimeAxis(&context.framework, "EyeAxis", 15u, 0.0f, 29.0f, &handle));
	CHECK(PX_LiveRealtimeEnter(&context.framework));
	CHECK(PX_LiveRealtimeSetAxisSample(&context.framework, handle, 0,
		PX_LIVE_REALTIME_WEIGHT_ONE_Q15));
	CHECK(PX_LiveRealtimeUpdate(&context.framework));
	atStart = context.layers[0].rel_currentTranslation.x;
	CHECK(PX_LiveRealtimeSetAxisPosition(&context.framework, handle, 0.0f,
		PX_LIVE_REALTIME_WEIGHT_ONE_Q15));
	CHECK(PX_LiveRealtimeUpdate(&context.framework));
	CHECK(FloatNear(context.layers[0].rel_currentTranslation.x, atStart));
	CHECK(PX_LiveRealtimeSetAxisSample(&context.framework, handle, 29,
		PX_LIVE_REALTIME_WEIGHT_ONE_Q15));
	CHECK(PX_LiveRealtimeUpdate(&context.framework));
	atEnd = context.layers[0].rel_currentTranslation.x;
	CHECK(PX_LiveRealtimeSetAxisPosition(&context.framework, handle, 29.0f,
		PX_LIVE_REALTIME_WEIGHT_ONE_Q15));
	CHECK(PX_LiveRealtimeUpdate(&context.framework));
	CHECK(FloatNear(context.layers[0].rel_currentTranslation.x, atEnd));
	CHECK(PX_LiveRealtimeSetAxisPosition(&context.framework, handle, 14.5f,
		PX_LIVE_REALTIME_WEIGHT_ONE_Q15));
	CHECK(PX_LiveRealtimeUpdate(&context.framework));
	CHECK(context.layers[0].rel_currentTranslation.x == context.layers[0].rel_currentTranslation.x);
	memset(&transition, 0, sizeof(transition));
	transition.startValue = 0;
	transition.targetValue = 29;
	transition.startTimeMs = 1000;
	transition.durationMs = 1000;
	atMiddleTime = PX_LiveRealtimeTransitionEvaluate(&transition, 1500);
	CHECK(FloatNear(atMiddleTime, 14.5f));
	CHECK(FloatNear(PX_LiveRealtimeTransitionEvaluate(&transition, 1000), 0));
	CHECK(FloatNear(PX_LiveRealtimeTransitionEvaluate(&transition, 2000), 29));
	CHECK(FloatNear(PX_LiveRealtimeTransitionEvaluate(&transition, 2500), 29));
	PX_LiveRealtimeFree(&context.framework.realtime);
}

int main(void)
{
	TestReader();
	TestSafeArithmeticAndAlignment();
	TestMemoryBudgets();
	TestValidFormat();
	TestHeaderAndChunkFailures();
	TestRT30Failures();
	TestThreeKeyBake();
	TestStrictDiscreteSelection();
	TestMultiAxisQ15AndZeroAllocationUpdate();
	TestSelectedAxisSoloReset();
	TestChildLocalTranslation();
	TestCloneAndIndependentFree();
	TestTopologyOffsetRebuild();
	TestContinuousPositionEndpoints();
#ifdef PX_RT30_TEST_LINK_FULL_FRAMEWORK
	TestLiveRT30PersistenceSource();
#endif
	if (g_failures != 0)
	{
		printf("RT30 format tests: %d/%d checks failed\n", g_failures, g_checks);
		return 1;
	}
	printf("RT30 format tests: all %d checks passed\n", g_checks);
	return 0;
}
