#include "PX_LiveDeviceFormat.h"

typedef char PX_LiveDeviceFormat_U16MustBe2Bytes[(sizeof(px_uint16) == 2) ? 1 : -1];
typedef char PX_LiveDeviceFormat_I16MustBe2Bytes[(sizeof(px_int16) == 2) ? 1 : -1];
typedef char PX_LiveDeviceFormat_U32MustBe4Bytes[(sizeof(px_uint32) == 4) ? 1 : -1];
typedef char PX_LiveDeviceFormat_I32MustBe4Bytes[(sizeof(px_int32) == 4) ? 1 : -1];

typedef struct
{
	px_uint32 idHash;
	px_byte middleKeyIndex;
	px_byte defaultSampleIndex;
	px_byte sampleCount;
	px_uint16 bindingCount;
	px_uint16 flags;
	px_uint32 sampleStride;
	px_uint32 bindingsOffset;
	px_uint32 samplesOffset;
} PX_LiveDeviceRT30Axis;

typedef struct
{
	px_uint16 targetLayerIndex;
	px_uint16 propertyMask;
	px_uint16 affectedVertexCount;
	px_uint32 vertexIndexOffset;
	px_uint32 dataOffsetWithinSample;
	px_uint32 dataSizeWithinSample;
} PX_LiveDeviceRT30Binding;

static px_void PX_LiveDeviceSetError(PX_LIVE_DEVICE_ERROR *error, PX_LIVE_DEVICE_ERROR value)
{
	if (error)
	{
		*error = value;
	}
}

static px_bool PX_LiveDeviceFail(PX_LIVE_DEVICE_ERROR *error, PX_LIVE_DEVICE_ERROR value)
{
	PX_LiveDeviceSetError(error, value);
	return PX_FALSE;
}

static px_bool PX_LiveDeviceRangesOverlap(px_uint32 offset1, px_uint32 size1,
	px_uint32 offset2, px_uint32 size2)
{
	px_uint32 end1, end2;
	if (!PX_LiveDeviceSafeAddU32(offset1, size1, &end1) ||
		!PX_LiveDeviceSafeAddU32(offset2, size2, &end2))
	{
		return PX_TRUE;
	}
	return offset1 < end2 && offset2 < end1;
}

px_bool PX_LiveDeviceSafeAddU32(px_uint32 a, px_uint32 b, px_uint32 *result)
{
	px_uint32 value;
	if (!result)
	{
		return PX_FALSE;
	}
	value = a + b;
	if (value < a)
	{
		return PX_FALSE;
	}
	*result = value;
	return PX_TRUE;
}

px_bool PX_LiveDeviceSafeMulU32(px_uint32 a, px_uint32 b, px_uint32 *result)
{
	if (!result)
	{
		return PX_FALSE;
	}
	if (a != 0 && b > ((px_uint32)0xffffffffu) / a)
	{
		return PX_FALSE;
	}
	*result = a * b;
	return PX_TRUE;
}

px_bool PX_LiveDeviceSafeAlignU32(px_uint32 value, px_uint32 alignment, px_uint32 *result)
{
	px_uint32 mask, sum;
	if (!result || alignment == 0 || (alignment & (alignment - 1)) != 0)
	{
		return PX_FALSE;
	}
	mask = alignment - 1;
	if (!PX_LiveDeviceSafeAddU32(value, mask, &sum))
	{
		return PX_FALSE;
	}
	*result = sum & ~mask;
	return PX_TRUE;
}

px_bool PX_LiveDeviceRangeIsValid(px_uint32 totalSize, px_uint32 offset, px_uint32 size)
{
	return offset <= totalSize && size <= totalSize - offset;
}

px_bool PX_LiveDeviceArrayRangeIsValid(px_uint32 totalSize, px_uint32 offset,
	px_uint32 count, px_uint32 stride)
{
	px_uint32 bytes;
	if (!PX_LiveDeviceSafeMulU32(count, stride, &bytes))
	{
		return PX_FALSE;
	}
	return PX_LiveDeviceRangeIsValid(totalSize, offset, bytes);
}

px_bool PX_LiveDeviceArenaIsAligned(const px_void *arena)
{
	px_uint64 address;
	if (!arena)
	{
		return PX_FALSE;
	}
	address = (px_uint64)arena;
	return (address & (PX_LIVE_DEVICE_ARENA_ALIGNMENT - 1)) == 0;
}

px_bool PX_LiveDeviceReaderInitialize(PX_LiveDeviceReader *reader, const px_void *data,
	px_uint32 size)
{
	if (!reader || (!data && size != 0))
	{
		return PX_FALSE;
	}
	reader->data = (const px_byte *)data;
	reader->size = size;
	reader->cursor = 0;
	reader->error = PX_LIVE_DEVICE_ERROR_NONE;
	return PX_TRUE;
}

static px_bool PX_LiveDeviceReaderReserve(PX_LiveDeviceReader *reader, px_uint32 size)
{
	if (!reader || reader->error != PX_LIVE_DEVICE_ERROR_NONE)
	{
		return PX_FALSE;
	}
	if (!PX_LiveDeviceRangeIsValid(reader->size, reader->cursor, size))
	{
		reader->error = PX_LIVE_DEVICE_ERROR_TRUNCATED;
		return PX_FALSE;
	}
	return PX_TRUE;
}

px_bool PX_LiveDeviceReaderReadU8(PX_LiveDeviceReader *reader, px_byte *value)
{
	if (!value || !PX_LiveDeviceReaderReserve(reader, 1))
	{
		return PX_FALSE;
	}
	*value = reader->data[reader->cursor];
	reader->cursor++;
	return PX_TRUE;
}

px_bool PX_LiveDeviceReaderReadU16LE(PX_LiveDeviceReader *reader, px_uint16 *value)
{
	const px_byte *p;
	if (!value || !PX_LiveDeviceReaderReserve(reader, 2))
	{
		return PX_FALSE;
	}
	p = reader->data + reader->cursor;
	*value = (px_uint16)((px_uint16)p[0] | ((px_uint16)p[1] << 8));
	reader->cursor += 2;
	return PX_TRUE;
}

px_bool PX_LiveDeviceReaderReadI16LE(PX_LiveDeviceReader *reader, px_int16 *value)
{
	px_uint16 raw;
	if (!value || !PX_LiveDeviceReaderReadU16LE(reader, &raw))
	{
		return PX_FALSE;
	}
	*value = (px_int16)raw;
	return PX_TRUE;
}

px_bool PX_LiveDeviceReaderReadU32LE(PX_LiveDeviceReader *reader, px_uint32 *value)
{
	const px_byte *p;
	if (!value || !PX_LiveDeviceReaderReserve(reader, 4))
	{
		return PX_FALSE;
	}
	p = reader->data + reader->cursor;
	*value = (px_uint32)p[0] |
		((px_uint32)p[1] << 8) |
		((px_uint32)p[2] << 16) |
		((px_uint32)p[3] << 24);
	reader->cursor += 4;
	return PX_TRUE;
}

px_bool PX_LiveDeviceReaderReadI32LE(PX_LiveDeviceReader *reader, px_int32 *value)
{
	px_uint32 raw;
	if (!value || !PX_LiveDeviceReaderReadU32LE(reader, &raw))
	{
		return PX_FALSE;
	}
	*value = (px_int32)raw;
	return PX_TRUE;
}

px_bool PX_LiveDeviceReaderReadBytes(PX_LiveDeviceReader *reader, px_void *output,
	px_uint32 size)
{
	px_byte *destination;
	px_uint32 i;
	if ((!output && size != 0) || !PX_LiveDeviceReaderReserve(reader, size))
	{
		return PX_FALSE;
	}
	destination = (px_byte *)output;
	for (i = 0; i < size; i++)
	{
		destination[i] = reader->data[reader->cursor + i];
	}
	reader->cursor += size;
	return PX_TRUE;
}

px_bool PX_LiveDeviceReaderSkip(PX_LiveDeviceReader *reader, px_uint32 size)
{
	if (!PX_LiveDeviceReaderReserve(reader, size))
	{
		return PX_FALSE;
	}
	reader->cursor += size;
	return PX_TRUE;
}

static px_void PX_LiveDeviceMemoryLayoutClear(PX_LiveDeviceMemoryLayout *layout)
{
	layout->baseOffset = 0;
	layout->baseAllocatedBytes = 0;
	layout->rt30SampleOffset = 0;
	layout->rt30SampleAllocatedBytes = 0;
	layout->rt30RuntimeOffset = 0;
	layout->rt30RuntimeAllocatedBytes = 0;
	layout->rt30MetadataOffset = 0;
	layout->rt30MetadataAllocatedBytes = 0;
	layout->requiredPsramBytes = 0;
}

px_bool PX_LiveDeviceMemoryLayoutCompute(const PX_LiveDeviceMemoryRequest *request,
	PX_LiveDeviceMemoryLayout *layout, PX_LIVE_DEVICE_ERROR *error)
{
	px_uint32 rt30Bytes, temporary;
	if (!request || !layout)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_INVALID_ARGUMENT);
	}
	PX_LiveDeviceSetError(error, PX_LIVE_DEVICE_ERROR_NONE);
	PX_LiveDeviceMemoryLayoutClear(layout);

	if (!PX_LiveDeviceSafeAlignU32(request->baseBytes, PX_LIVE_DEVICE_BLOCK_ALIGNMENT,
		&layout->baseAllocatedBytes) ||
		!PX_LiveDeviceSafeAlignU32(request->rt30SampleBytes, PX_LIVE_DEVICE_SAMPLE_ALIGNMENT,
		&layout->rt30SampleAllocatedBytes) ||
		!PX_LiveDeviceSafeAlignU32(request->rt30RuntimeBytes, PX_LIVE_DEVICE_BLOCK_ALIGNMENT,
		&layout->rt30RuntimeAllocatedBytes) ||
		!PX_LiveDeviceSafeAlignU32(request->rt30MetadataBytes, PX_LIVE_DEVICE_BLOCK_ALIGNMENT,
		&layout->rt30MetadataAllocatedBytes))
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_ARITHMETIC_OVERFLOW);
	}

	if (layout->baseAllocatedBytes > PX_LIVE_DEVICE_BASE_LIMIT_BYTES)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_BASE_BUDGET_EXCEEDED);
	}
	if (!PX_LiveDeviceSafeAddU32(layout->rt30SampleAllocatedBytes,
		layout->rt30RuntimeAllocatedBytes, &temporary) ||
		!PX_LiveDeviceSafeAddU32(temporary, layout->rt30MetadataAllocatedBytes, &rt30Bytes))
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_ARITHMETIC_OVERFLOW);
	}
	if (rt30Bytes > PX_LIVE_DEVICE_RT30_LIMIT_BYTES)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_RT30_BUDGET_EXCEEDED);
	}
	if (layout->rt30SampleAllocatedBytes > PX_LIVE_DEVICE_SAMPLE_LIMIT_BYTES)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_SAMPLE_BUDGET_EXCEEDED);
	}
	if (layout->rt30RuntimeAllocatedBytes > PX_LIVE_DEVICE_RUNTIME_LIMIT_BYTES)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_RUNTIME_BUDGET_EXCEEDED);
	}
	if (layout->rt30MetadataAllocatedBytes > PX_LIVE_DEVICE_METADATA_LIMIT_BYTES)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_METADATA_BUDGET_EXCEEDED);
	}

	layout->baseOffset = 0;
	if (!PX_LiveDeviceSafeAlignU32(layout->baseAllocatedBytes,
		PX_LIVE_DEVICE_SAMPLE_ALIGNMENT, &layout->rt30SampleOffset) ||
		!PX_LiveDeviceSafeAddU32(layout->rt30SampleOffset,
		layout->rt30SampleAllocatedBytes, &temporary) ||
		!PX_LiveDeviceSafeAlignU32(temporary, PX_LIVE_DEVICE_BLOCK_ALIGNMENT,
		&layout->rt30RuntimeOffset) ||
		!PX_LiveDeviceSafeAddU32(layout->rt30RuntimeOffset,
		layout->rt30RuntimeAllocatedBytes, &temporary) ||
		!PX_LiveDeviceSafeAlignU32(temporary, PX_LIVE_DEVICE_BLOCK_ALIGNMENT,
		&layout->rt30MetadataOffset) ||
		!PX_LiveDeviceSafeAddU32(layout->rt30MetadataOffset,
		layout->rt30MetadataAllocatedBytes, &layout->requiredPsramBytes))
	{
		PX_LiveDeviceMemoryLayoutClear(layout);
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_ARITHMETIC_OVERFLOW);
	}
	if (layout->requiredPsramBytes > PX_LIVE_DEVICE_PSRAM_LIMIT_BYTES)
	{
		PX_LiveDeviceMemoryLayoutClear(layout);
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_PSRAM_LIMIT_EXCEEDED);
	}
	return PX_TRUE;
}

px_bool PX_LiveDeviceFormatReadHeader(const px_void *data, px_uint32 size,
	PX_LiveDeviceHeader *header, PX_LIVE_DEVICE_ERROR *error)
{
	PX_LiveDeviceReader reader;
	if (!data || !header)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_INVALID_ARGUMENT);
	}
	PX_LiveDeviceSetError(error, PX_LIVE_DEVICE_ERROR_NONE);
	if (!PX_LiveDeviceReaderInitialize(&reader, data, size) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &header->magic) ||
		!PX_LiveDeviceReaderReadU16LE(&reader, &header->version) ||
		!PX_LiveDeviceReaderReadU16LE(&reader, &header->headerSize) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &header->fileSize) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &header->requiredPsramBytes) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &header->baseBytes) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &header->rt30SampleBytes) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &header->rt30RuntimeBytes) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &header->rt30MetadataBytes) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &header->chunkTableOffset) ||
		!PX_LiveDeviceReaderReadU16LE(&reader, &header->chunkCount) ||
		!PX_LiveDeviceReaderReadU16LE(&reader, &header->axisCount) ||
		!PX_LiveDeviceReaderReadU16LE(&reader, &header->layerCount) ||
		!PX_LiveDeviceReaderReadU16LE(&reader, &header->vertexCount) ||
		!PX_LiveDeviceReaderReadU8(&reader, &header->coordFractionBits) ||
		!PX_LiveDeviceReaderReadU8(&reader, &header->rotationFractionBits) ||
		!PX_LiveDeviceReaderReadU8(&reader, &header->stretchFractionBits) ||
		!PX_LiveDeviceReaderReadU8(&reader, &header->reserved0) ||
		!PX_LiveDeviceReaderReadU16LE(&reader, &header->flags) ||
		!PX_LiveDeviceReaderReadU16LE(&reader, &header->chunkEntrySize) ||
		!PX_LiveDeviceReaderSkip(&reader, PX_LIVE_DEVICE_HEADER_SIZE - 52u))
	{
		return PX_LiveDeviceFail(error, reader.error == PX_LIVE_DEVICE_ERROR_NONE ?
			PX_LIVE_DEVICE_ERROR_TRUNCATED : reader.error);
	}
	if (header->magic != PX_LIVE_DEVICE_MAGIC)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_BAD_MAGIC);
	}
	if (header->version != PX_LIVE_DEVICE_VERSION)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_UNSUPPORTED_VERSION);
	}
	if (header->headerSize != PX_LIVE_DEVICE_HEADER_SIZE ||
		header->chunkEntrySize != PX_LIVE_DEVICE_CHUNK_ENTRY_SIZE)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_INVALID_HEADER_SIZE);
	}
	if (header->fileSize != size)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_FILE_SIZE_MISMATCH);
	}
	return PX_TRUE;
}

px_bool PX_LiveDeviceFormatReadChunk(const PX_LiveDeviceFormatView *view, px_uint32 index,
	PX_LiveDeviceChunk *chunk, PX_LIVE_DEVICE_ERROR *error)
{
	PX_LiveDeviceReader reader;
	px_uint32 relativeOffset, offset;
	if (!view || !view->data || !chunk)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_INVALID_ARGUMENT);
	}
	PX_LiveDeviceSetError(error, PX_LIVE_DEVICE_ERROR_NONE);
	if (index >= view->header.chunkCount)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_INVALID_COUNT);
	}
	if (!PX_LiveDeviceSafeMulU32(index, view->header.chunkEntrySize, &relativeOffset) ||
		!PX_LiveDeviceSafeAddU32(view->header.chunkTableOffset, relativeOffset, &offset))
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_ARITHMETIC_OVERFLOW);
	}
	if (!PX_LiveDeviceRangeIsValid(view->size, offset, PX_LIVE_DEVICE_CHUNK_ENTRY_SIZE))
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_RANGE_OUTSIDE_FILE);
	}
	PX_LiveDeviceReaderInitialize(&reader, view->data, view->size);
	if (!PX_LiveDeviceReaderSkip(&reader, offset) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &chunk->type) ||
		!PX_LiveDeviceReaderReadU16LE(&reader, &chunk->version) ||
		!PX_LiveDeviceReaderReadU16LE(&reader, &chunk->flags) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &chunk->offset) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &chunk->size) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &chunk->count))
	{
		return PX_LiveDeviceFail(error, reader.error);
	}
	return PX_TRUE;
}

px_bool PX_LiveDeviceFormatFindChunk(const PX_LiveDeviceFormatView *view, px_uint32 type,
	PX_LiveDeviceChunk *chunk, PX_LIVE_DEVICE_ERROR *error)
{
	px_uint32 i;
	PX_LiveDeviceChunk current;
	if (!view || !chunk)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_INVALID_ARGUMENT);
	}
	for (i = 0; i < view->header.chunkCount; i++)
	{
		if (!PX_LiveDeviceFormatReadChunk(view, i, &current, error))
		{
			return PX_FALSE;
		}
		if (current.type == type)
		{
			*chunk = current;
			PX_LiveDeviceSetError(error, PX_LIVE_DEVICE_ERROR_NONE);
			return PX_TRUE;
		}
	}
	return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_MISSING_CHUNK);
}

static px_bool PX_LiveDeviceReadRT30Axis(const px_byte *data, px_uint32 size,
	px_uint32 axesOffset, px_uint32 index, PX_LiveDeviceRT30Axis *axis,
	PX_LIVE_DEVICE_ERROR *error)
{
	PX_LiveDeviceReader reader;
	px_uint32 relativeOffset, offset;
	px_byte reserved8;
	px_uint32 reserved32;
	if (!PX_LiveDeviceSafeMulU32(index, PX_LIVE_DEVICE_RT30_AXIS_ENTRY_SIZE, &relativeOffset) ||
		!PX_LiveDeviceSafeAddU32(axesOffset, relativeOffset, &offset))
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_ARITHMETIC_OVERFLOW);
	}
	if (!PX_LiveDeviceRangeIsValid(size, offset, PX_LIVE_DEVICE_RT30_AXIS_ENTRY_SIZE))
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_RANGE_OUTSIDE_FILE);
	}
	PX_LiveDeviceReaderInitialize(&reader, data, size);
	if (!PX_LiveDeviceReaderSkip(&reader, offset) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &axis->idHash) ||
		!PX_LiveDeviceReaderReadU8(&reader, &axis->middleKeyIndex) ||
		!PX_LiveDeviceReaderReadU8(&reader, &axis->defaultSampleIndex) ||
		!PX_LiveDeviceReaderReadU8(&reader, &axis->sampleCount) ||
		!PX_LiveDeviceReaderReadU8(&reader, &reserved8) ||
		!PX_LiveDeviceReaderReadU16LE(&reader, &axis->bindingCount) ||
		!PX_LiveDeviceReaderReadU16LE(&reader, &axis->flags) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &axis->sampleStride) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &axis->bindingsOffset) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &axis->samplesOffset) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &reserved32))
	{
		return PX_LiveDeviceFail(error, reader.error);
	}
	return PX_TRUE;
}

static px_bool PX_LiveDeviceReadRT30Binding(const px_byte *data, px_uint32 size,
	px_uint32 bindingsOffset, px_uint32 index, PX_LiveDeviceRT30Binding *binding,
	PX_LIVE_DEVICE_ERROR *error)
{
	PX_LiveDeviceReader reader;
	px_uint32 relativeOffset, offset;
	px_uint16 reserved16;
	if (!PX_LiveDeviceSafeMulU32(index, PX_LIVE_DEVICE_RT30_BINDING_ENTRY_SIZE, &relativeOffset) ||
		!PX_LiveDeviceSafeAddU32(bindingsOffset, relativeOffset, &offset))
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_ARITHMETIC_OVERFLOW);
	}
	if (!PX_LiveDeviceRangeIsValid(size, offset, PX_LIVE_DEVICE_RT30_BINDING_ENTRY_SIZE))
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_RANGE_OUTSIDE_FILE);
	}
	PX_LiveDeviceReaderInitialize(&reader, data, size);
	if (!PX_LiveDeviceReaderSkip(&reader, offset) ||
		!PX_LiveDeviceReaderReadU16LE(&reader, &binding->targetLayerIndex) ||
		!PX_LiveDeviceReaderReadU16LE(&reader, &binding->propertyMask) ||
		!PX_LiveDeviceReaderReadU16LE(&reader, &binding->affectedVertexCount) ||
		!PX_LiveDeviceReaderReadU16LE(&reader, &reserved16) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &binding->vertexIndexOffset) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &binding->dataOffsetWithinSample) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &binding->dataSizeWithinSample))
	{
		return PX_LiveDeviceFail(error, reader.error);
	}
	return PX_TRUE;
}

static px_bool PX_LiveDeviceValidateLayoutChunk(const PX_LiveDeviceFormatView *view,
	const PX_LiveDeviceChunk *chunk, const PX_LiveDeviceMemoryLayout *expected,
	PX_LIVE_DEVICE_ERROR *error)
{
	PX_LiveDeviceReader reader;
	PX_LiveDeviceMemoryLayout actual;
	px_uint16 version, headerSize;
	if (chunk->size != PX_LIVE_DEVICE_LAYOUT_HEADER_SIZE || chunk->count != 1)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_BAD_CHUNK_SIZE);
	}
	PX_LiveDeviceReaderInitialize(&reader, view->data + chunk->offset, chunk->size);
	if (!PX_LiveDeviceReaderReadU16LE(&reader, &version) ||
		!PX_LiveDeviceReaderReadU16LE(&reader, &headerSize) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &actual.baseOffset) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &actual.baseAllocatedBytes) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &actual.rt30SampleOffset) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &actual.rt30SampleAllocatedBytes) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &actual.rt30RuntimeOffset) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &actual.rt30RuntimeAllocatedBytes) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &actual.rt30MetadataOffset) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &actual.rt30MetadataAllocatedBytes) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &actual.requiredPsramBytes))
	{
		return PX_LiveDeviceFail(error, reader.error);
	}
	if (version != PX_LIVE_DEVICE_VERSION)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_UNSUPPORTED_VERSION);
	}
	if (headerSize != PX_LIVE_DEVICE_LAYOUT_HEADER_SIZE)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_INVALID_HEADER_SIZE);
	}
	if (actual.baseOffset != expected->baseOffset ||
		actual.baseAllocatedBytes != expected->baseAllocatedBytes ||
		actual.rt30SampleOffset != expected->rt30SampleOffset ||
		actual.rt30SampleAllocatedBytes != expected->rt30SampleAllocatedBytes ||
		actual.rt30RuntimeOffset != expected->rt30RuntimeOffset ||
		actual.rt30RuntimeAllocatedBytes != expected->rt30RuntimeAllocatedBytes ||
		actual.rt30MetadataOffset != expected->rt30MetadataOffset ||
		actual.rt30MetadataAllocatedBytes != expected->rt30MetadataAllocatedBytes ||
		actual.requiredPsramBytes != expected->requiredPsramBytes)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_LAYOUT_MISMATCH);
	}
	return PX_TRUE;
}

static px_bool PX_LiveDeviceValidateRT30Binding(const PX_LiveDeviceFormatView *view,
	const px_byte *rtData, px_uint32 rtSize, const PX_LiveDeviceRT30Axis *axis,
	px_uint32 axesOffset, px_uint32 axesBytes, px_uint32 bindingIndex,
	PX_LIVE_DEVICE_ERROR *error)
{
	PX_LiveDeviceRT30Binding binding, previous;
	PX_LiveDeviceReader reader;
	px_uint32 expectedSize = 0, vertexBytes = 0, dataEnd, i;
	px_uint16 vertexIndex;
	if (!PX_LiveDeviceReadRT30Binding(rtData, rtSize, axis->bindingsOffset,
		bindingIndex, &binding, error))
	{
		return PX_FALSE;
	}
	if (binding.targetLayerIndex >= view->header.layerCount)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_INDEX_OUT_OF_RANGE);
	}
	if (binding.propertyMask == 0 ||
		(binding.propertyMask & ~PX_LIVE_DEVICE_PROPERTY_ALL) != 0)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_PROPERTY_MASK_INVALID);
	}
	if (binding.propertyMask & PX_LIVE_DEVICE_PROPERTY_TRANSLATION)
	{
		expectedSize += 4;
	}
	if (binding.propertyMask & PX_LIVE_DEVICE_PROPERTY_ROTATION)
	{
		expectedSize += 2;
	}
	if (binding.propertyMask & PX_LIVE_DEVICE_PROPERTY_STRETCH)
	{
		expectedSize += 2;
	}
	if (binding.propertyMask & PX_LIVE_DEVICE_PROPERTY_LOCAL_TRANSLATION)
	{
		expectedSize += 4;
	}
	if (binding.propertyMask & PX_LIVE_DEVICE_PROPERTY_LOCAL_ROTATION)
	{
		expectedSize += 2;
	}
	if (binding.propertyMask & PX_LIVE_DEVICE_PROPERTY_LOCAL_SCALE)
	{
		expectedSize += 2;
	}
	if (binding.propertyMask & PX_LIVE_DEVICE_PROPERTY_TEXTURE)
	{
		expectedSize += 2;
	}
	if (binding.propertyMask & PX_LIVE_DEVICE_PROPERTY_IMPULSE)
	{
		expectedSize += 4;
	}
	if (binding.propertyMask & PX_LIVE_DEVICE_PROPERTY_VERTEX_DELTA)
	{
		if (binding.affectedVertexCount == 0 ||
			binding.affectedVertexCount > view->header.vertexCount ||
			(binding.vertexIndexOffset & 1u) != 0 ||
			!PX_LiveDeviceSafeMulU32(binding.affectedVertexCount, 2u, &vertexBytes) ||
			!PX_LiveDeviceRangeIsValid(rtSize, binding.vertexIndexOffset, vertexBytes))
		{
			return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_BINDING_INVALID);
		}
		if (PX_LiveDeviceRangesOverlap(binding.vertexIndexOffset, vertexBytes, 0,
			PX_LIVE_DEVICE_RT30_HEADER_SIZE) ||
			PX_LiveDeviceRangesOverlap(binding.vertexIndexOffset, vertexBytes,
				axesOffset, axesBytes) ||
			PX_LiveDeviceRangesOverlap(binding.vertexIndexOffset, vertexBytes,
				axis->bindingsOffset,
				(px_uint32)axis->bindingCount * PX_LIVE_DEVICE_RT30_BINDING_ENTRY_SIZE))
		{
			return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_BINDING_INVALID);
		}
		PX_LiveDeviceReaderInitialize(&reader, rtData, rtSize);
		if (!PX_LiveDeviceReaderSkip(&reader, binding.vertexIndexOffset))
		{
			return PX_LiveDeviceFail(error, reader.error);
		}
		for (i = 0; i < binding.affectedVertexCount; i++)
		{
			if (!PX_LiveDeviceReaderReadU16LE(&reader, &vertexIndex))
			{
				return PX_LiveDeviceFail(error, reader.error);
			}
			if (vertexIndex >= view->header.vertexCount)
			{
				return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_INDEX_OUT_OF_RANGE);
			}
		}
		if (!PX_LiveDeviceSafeMulU32(binding.affectedVertexCount, 4u, &vertexBytes) ||
			!PX_LiveDeviceSafeAddU32(expectedSize, vertexBytes, &expectedSize))
		{
			return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_ARITHMETIC_OVERFLOW);
		}
	}
	else if (binding.affectedVertexCount != 0 || binding.vertexIndexOffset != 0)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_BINDING_INVALID);
	}
	if ((binding.dataOffsetWithinSample & 1u) != 0 ||
		binding.dataSizeWithinSample != expectedSize ||
		!PX_LiveDeviceSafeAddU32(binding.dataOffsetWithinSample,
			binding.dataSizeWithinSample, &dataEnd) ||
		dataEnd > axis->sampleStride)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_BINDING_INVALID);
	}
	for (i = 0; i < bindingIndex; i++)
	{
		if (!PX_LiveDeviceReadRT30Binding(rtData, rtSize, axis->bindingsOffset,
			i, &previous, error))
		{
			return PX_FALSE;
		}
		if (PX_LiveDeviceRangesOverlap(binding.dataOffsetWithinSample,
			binding.dataSizeWithinSample, previous.dataOffsetWithinSample,
			previous.dataSizeWithinSample))
		{
			return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_BINDING_INVALID);
		}
	}
	return PX_TRUE;
}

static px_bool PX_LiveDeviceValidateRT30Chunk(const PX_LiveDeviceFormatView *view,
	const PX_LiveDeviceChunk *chunk, PX_LIVE_DEVICE_ERROR *error)
{
	PX_LiveDeviceReader reader;
	PX_LiveDeviceRT30Axis axis, previous;
	const px_byte *rtData;
	px_uint16 version, headerSize, axisCount, axisEntrySize;
	px_uint32 axesOffset, axesBytes, reserved32;
	px_uint32 i, j, sampleBytes, totalSampleBytes = 0, bindingBytes;
	if (chunk->size < PX_LIVE_DEVICE_RT30_HEADER_SIZE ||
		chunk->count != view->header.axisCount)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_BAD_CHUNK_SIZE);
	}
	rtData = view->data + chunk->offset;
	PX_LiveDeviceReaderInitialize(&reader, rtData, chunk->size);
	if (!PX_LiveDeviceReaderReadU16LE(&reader, &version) ||
		!PX_LiveDeviceReaderReadU16LE(&reader, &headerSize) ||
		!PX_LiveDeviceReaderReadU16LE(&reader, &axisCount) ||
		!PX_LiveDeviceReaderReadU16LE(&reader, &axisEntrySize) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &axesOffset) ||
		!PX_LiveDeviceReaderReadU32LE(&reader, &reserved32))
	{
		return PX_LiveDeviceFail(error, reader.error);
	}
	if (version != PX_LIVE_DEVICE_VERSION)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_UNSUPPORTED_VERSION);
	}
	if (headerSize != PX_LIVE_DEVICE_RT30_HEADER_SIZE ||
		axisEntrySize != PX_LIVE_DEVICE_RT30_AXIS_ENTRY_SIZE)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_INVALID_HEADER_SIZE);
	}
	if (axisCount != view->header.axisCount || axisCount == 0)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_INVALID_COUNT);
	}
	if (axisCount > PX_LIVE_DEVICE_RT30_MAX_AXES)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_AXIS_LIMIT_EXCEEDED);
	}
	if ((axesOffset & (PX_LIVE_DEVICE_BLOCK_ALIGNMENT - 1)) != 0 ||
		axesOffset < headerSize ||
		!PX_LiveDeviceSafeMulU32(axisCount, axisEntrySize, &axesBytes) ||
		!PX_LiveDeviceRangeIsValid(chunk->size, axesOffset, axesBytes))
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_RANGE_OUTSIDE_FILE);
	}

	for (i = 0; i < axisCount; i++)
	{
		if (!PX_LiveDeviceReadRT30Axis(rtData, chunk->size, axesOffset, i, &axis, error))
		{
			return PX_FALSE;
		}
		if (axis.idHash == 0)
		{
			return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_HASH_COLLISION);
		}
		for (j = 0; j < i; j++)
		{
			if (!PX_LiveDeviceReadRT30Axis(rtData, chunk->size, axesOffset, j,
				&previous, error))
			{
				return PX_FALSE;
			}
			if (previous.idHash == axis.idHash)
			{
				return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_HASH_COLLISION);
			}
		}
		if (axis.sampleCount != PX_LIVE_DEVICE_RT30_SAMPLE_COUNT)
		{
			return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_SAMPLE_COUNT_INVALID);
		}
		if (axis.middleKeyIndex < 1 || axis.middleKeyIndex > 28)
		{
			return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_MIDDLE_KEY_INVALID);
		}
		if (axis.defaultSampleIndex != 0 &&
			axis.defaultSampleIndex != axis.middleKeyIndex &&
			axis.defaultSampleIndex != PX_LIVE_DEVICE_RT30_SAMPLE_COUNT - 1)
		{
			return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_DEFAULT_SAMPLE_INVALID);
		}
		if (axis.bindingCount == 0 ||
			axis.bindingCount > PX_LIVE_DEVICE_MAX_BINDINGS_PER_AXIS)
		{
			return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_INVALID_COUNT);
		}
		if (axis.sampleStride == 0 || (axis.sampleStride & 1u) != 0)
		{
			return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_SAMPLE_STRIDE_INVALID);
		}
		if (!PX_LiveDeviceSafeMulU32(axis.bindingCount,
			PX_LIVE_DEVICE_RT30_BINDING_ENTRY_SIZE, &bindingBytes) ||
			(axis.bindingsOffset & (PX_LIVE_DEVICE_BLOCK_ALIGNMENT - 1)) != 0 ||
			axis.bindingsOffset < headerSize ||
			!PX_LiveDeviceRangeIsValid(chunk->size, axis.bindingsOffset, bindingBytes))
		{
			return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_BINDING_INVALID);
		}
		if (!PX_LiveDeviceSafeMulU32(axis.sampleStride,
			PX_LIVE_DEVICE_RT30_SAMPLE_COUNT, &sampleBytes) ||
			(axis.samplesOffset & (PX_LIVE_DEVICE_SAMPLE_ALIGNMENT - 1)) != 0 ||
			axis.samplesOffset < headerSize ||
			!PX_LiveDeviceRangeIsValid(chunk->size, axis.samplesOffset, sampleBytes))
		{
			return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_SAMPLE_STRIDE_INVALID);
		}
		if (PX_LiveDeviceRangesOverlap(axis.bindingsOffset, bindingBytes, 0, headerSize) ||
			PX_LiveDeviceRangesOverlap(axis.bindingsOffset, bindingBytes, axesOffset, axesBytes) ||
			PX_LiveDeviceRangesOverlap(axis.samplesOffset, sampleBytes, 0, headerSize) ||
			PX_LiveDeviceRangesOverlap(axis.samplesOffset, sampleBytes, axesOffset, axesBytes) ||
			PX_LiveDeviceRangesOverlap(axis.samplesOffset, sampleBytes,
				axis.bindingsOffset, bindingBytes))
		{
			return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_CHUNK_OVERLAP);
		}
		for (j = 0; j < i; j++)
		{
			px_uint32 previousSampleBytes, previousBindingBytes;
			if (!PX_LiveDeviceReadRT30Axis(rtData, chunk->size, axesOffset, j,
				&previous, error) ||
				!PX_LiveDeviceSafeMulU32(previous.sampleStride,
					PX_LIVE_DEVICE_RT30_SAMPLE_COUNT, &previousSampleBytes) ||
				!PX_LiveDeviceSafeMulU32(previous.bindingCount,
					PX_LIVE_DEVICE_RT30_BINDING_ENTRY_SIZE, &previousBindingBytes))
			{
				return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_ARITHMETIC_OVERFLOW);
			}
			if (PX_LiveDeviceRangesOverlap(axis.samplesOffset, sampleBytes,
					previous.samplesOffset, previousSampleBytes) ||
				PX_LiveDeviceRangesOverlap(axis.bindingsOffset, bindingBytes,
					previous.bindingsOffset, previousBindingBytes) ||
				PX_LiveDeviceRangesOverlap(axis.samplesOffset, sampleBytes,
					previous.bindingsOffset, previousBindingBytes) ||
				PX_LiveDeviceRangesOverlap(axis.bindingsOffset, bindingBytes,
					previous.samplesOffset, previousSampleBytes))
			{
				return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_CHUNK_OVERLAP);
			}
		}
		for (j = 0; j < axis.bindingCount; j++)
		{
			if (!PX_LiveDeviceValidateRT30Binding(view, rtData, chunk->size, &axis,
				axesOffset, axesBytes, j, error))
			{
				return PX_FALSE;
			}
		}
		if (!PX_LiveDeviceSafeAddU32(totalSampleBytes, sampleBytes, &totalSampleBytes))
		{
			return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_ARITHMETIC_OVERFLOW);
		}
	}
	if (totalSampleBytes != view->header.rt30SampleBytes)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_LAYOUT_MISMATCH);
	}
	return PX_TRUE;
}

px_bool PX_LiveDeviceFormatValidate(const px_void *data, px_uint32 size,
	PX_LiveDeviceFormatView *view, PX_LIVE_DEVICE_ERROR *error)
{
	PX_LiveDeviceFormatView localView;
	PX_LiveDeviceMemoryRequest request;
	PX_LiveDeviceMemoryLayout layout;
	PX_LiveDeviceChunk chunk, previous, rt30Chunk, layoutChunk;
	px_uint32 tableBytes, i, j;
	px_bool hasBase = PX_FALSE, hasTexture = PX_FALSE;
	px_bool hasRT30 = PX_FALSE, hasLayout = PX_FALSE;
	if (view)
	{
		view->data = PX_NULL;
		view->size = 0;
	}
	if (!data || !view)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_INVALID_ARGUMENT);
	}
	PX_LiveDeviceSetError(error, PX_LIVE_DEVICE_ERROR_NONE);
	if (!PX_LiveDeviceFormatReadHeader(data, size, &localView.header, error))
	{
		return PX_FALSE;
	}
	localView.data = (const px_byte *)data;
	localView.size = size;
	if (localView.header.chunkCount < 4 ||
		localView.header.chunkCount > PX_LIVE_DEVICE_MAX_CHUNKS)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_INVALID_COUNT);
	}
	if (localView.header.axisCount == 0 ||
		localView.header.axisCount > PX_LIVE_DEVICE_RT30_MAX_AXES)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_AXIS_LIMIT_EXCEEDED);
	}
	if (localView.header.layerCount == 0 ||
		localView.header.layerCount > PX_LIVE_DEVICE_MAX_LAYERS)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_INVALID_COUNT);
	}
	if (localView.header.coordFractionBits > PX_LIVE_DEVICE_MAX_FRACTION_BITS ||
		localView.header.rotationFractionBits > PX_LIVE_DEVICE_MAX_FRACTION_BITS ||
		localView.header.stretchFractionBits > PX_LIVE_DEVICE_MAX_FRACTION_BITS)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_QUANTIZATION_INVALID);
	}
	request.baseBytes = localView.header.baseBytes;
	request.rt30SampleBytes = localView.header.rt30SampleBytes;
	request.rt30RuntimeBytes = localView.header.rt30RuntimeBytes;
	request.rt30MetadataBytes = localView.header.rt30MetadataBytes;
	if (!PX_LiveDeviceMemoryLayoutCompute(&request, &layout, error))
	{
		return PX_FALSE;
	}
	if (localView.header.requiredPsramBytes != layout.requiredPsramBytes)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_REQUIRED_PSRAM_MISMATCH);
	}
	if ((localView.header.chunkTableOffset & (PX_LIVE_DEVICE_BLOCK_ALIGNMENT - 1)) != 0 ||
		localView.header.chunkTableOffset < localView.header.headerSize ||
		!PX_LiveDeviceSafeMulU32(localView.header.chunkCount,
			localView.header.chunkEntrySize, &tableBytes) ||
		!PX_LiveDeviceRangeIsValid(size, localView.header.chunkTableOffset, tableBytes))
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_RANGE_OUTSIDE_FILE);
	}
	for (i = 0; i < localView.header.chunkCount; i++)
	{
		if (!PX_LiveDeviceFormatReadChunk(&localView, i, &chunk, error))
		{
			return PX_FALSE;
		}
		if (chunk.version != PX_LIVE_DEVICE_VERSION)
		{
			return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_UNSUPPORTED_VERSION);
		}
		if (chunk.size == 0)
		{
			return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_BAD_CHUNK_SIZE);
		}
		if ((chunk.offset & (PX_LIVE_DEVICE_BLOCK_ALIGNMENT - 1)) != 0 ||
			(chunk.type == PX_LIVE_DEVICE_CHUNK_RT30 &&
				(chunk.offset & (PX_LIVE_DEVICE_SAMPLE_ALIGNMENT - 1)) != 0))
		{
			return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_INVALID_ALIGNMENT);
		}
		if (!PX_LiveDeviceRangeIsValid(size, chunk.offset, chunk.size))
		{
			return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_RANGE_OUTSIDE_FILE);
		}
		if (PX_LiveDeviceRangesOverlap(chunk.offset, chunk.size, 0,
			localView.header.headerSize) ||
			PX_LiveDeviceRangesOverlap(chunk.offset, chunk.size,
				localView.header.chunkTableOffset, tableBytes))
		{
			return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_CHUNK_OVERLAP);
		}
		for (j = 0; j < i; j++)
		{
			if (!PX_LiveDeviceFormatReadChunk(&localView, j, &previous, error))
			{
				return PX_FALSE;
			}
			if (previous.type == chunk.type)
			{
				return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_DUPLICATE_CHUNK);
			}
			if (PX_LiveDeviceRangesOverlap(previous.offset, previous.size,
				chunk.offset, chunk.size))
			{
				return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_CHUNK_OVERLAP);
			}
		}
		switch (chunk.type)
		{
		case PX_LIVE_DEVICE_CHUNK_BASE:
			hasBase = PX_TRUE;
			break;
		case PX_LIVE_DEVICE_CHUNK_TEXTURE:
			hasTexture = PX_TRUE;
			break;
		case PX_LIVE_DEVICE_CHUNK_RT30:
			hasRT30 = PX_TRUE;
			rt30Chunk = chunk;
			break;
		case PX_LIVE_DEVICE_CHUNK_LAYOUT:
			hasLayout = PX_TRUE;
			layoutChunk = chunk;
			break;
		default:
			break;
		}
	}
	if (!hasBase || !hasTexture || !hasRT30 || !hasLayout)
	{
		return PX_LiveDeviceFail(error, PX_LIVE_DEVICE_ERROR_MISSING_CHUNK);
	}
	if (!PX_LiveDeviceValidateLayoutChunk(&localView, &layoutChunk, &layout, error) ||
		!PX_LiveDeviceValidateRT30Chunk(&localView, &rt30Chunk, error))
	{
		return PX_FALSE;
	}
	*view = localView;
	PX_LiveDeviceSetError(error, PX_LIVE_DEVICE_ERROR_NONE);
	return PX_TRUE;
}

const px_char *PX_LiveDeviceFormatErrorString(PX_LIVE_DEVICE_ERROR error)
{
	switch (error)
	{
	case PX_LIVE_DEVICE_ERROR_NONE: return "none";
	case PX_LIVE_DEVICE_ERROR_INVALID_ARGUMENT: return "invalid argument";
	case PX_LIVE_DEVICE_ERROR_TRUNCATED: return "truncated input";
	case PX_LIVE_DEVICE_ERROR_ARITHMETIC_OVERFLOW: return "arithmetic overflow";
	case PX_LIVE_DEVICE_ERROR_RANGE_OUTSIDE_FILE: return "range outside file";
	case PX_LIVE_DEVICE_ERROR_BAD_MAGIC: return "bad magic";
	case PX_LIVE_DEVICE_ERROR_UNSUPPORTED_VERSION: return "unsupported version";
	case PX_LIVE_DEVICE_ERROR_INVALID_HEADER_SIZE: return "invalid header size";
	case PX_LIVE_DEVICE_ERROR_FILE_SIZE_MISMATCH: return "file size mismatch";
	case PX_LIVE_DEVICE_ERROR_INVALID_COUNT: return "invalid count";
	case PX_LIVE_DEVICE_ERROR_INVALID_ALIGNMENT: return "invalid alignment";
	case PX_LIVE_DEVICE_ERROR_DUPLICATE_CHUNK: return "duplicate chunk";
	case PX_LIVE_DEVICE_ERROR_MISSING_CHUNK: return "missing chunk";
	case PX_LIVE_DEVICE_ERROR_CHUNK_OVERLAP: return "overlapping chunks";
	case PX_LIVE_DEVICE_ERROR_BAD_CHUNK_SIZE: return "bad chunk size";
	case PX_LIVE_DEVICE_ERROR_BASE_BUDGET_EXCEEDED: return "base model budget exceeded";
	case PX_LIVE_DEVICE_ERROR_RT30_BUDGET_EXCEEDED: return "RT30 added budget exceeded";
	case PX_LIVE_DEVICE_ERROR_SAMPLE_BUDGET_EXCEEDED: return "RT30 sample budget exceeded";
	case PX_LIVE_DEVICE_ERROR_RUNTIME_BUDGET_EXCEEDED: return "RT30 runtime budget exceeded";
	case PX_LIVE_DEVICE_ERROR_METADATA_BUDGET_EXCEEDED: return "RT30 metadata budget exceeded";
	case PX_LIVE_DEVICE_ERROR_PSRAM_LIMIT_EXCEEDED: return "PSRAM hard limit exceeded";
	case PX_LIVE_DEVICE_ERROR_REQUIRED_PSRAM_MISMATCH: return "required PSRAM mismatch";
	case PX_LIVE_DEVICE_ERROR_LAYOUT_MISMATCH: return "layout mismatch";
	case PX_LIVE_DEVICE_ERROR_AXIS_LIMIT_EXCEEDED: return "axis limit exceeded";
	case PX_LIVE_DEVICE_ERROR_SAMPLE_COUNT_INVALID: return "sample count must be 30";
	case PX_LIVE_DEVICE_ERROR_MIDDLE_KEY_INVALID: return "middle key must be 1..28";
	case PX_LIVE_DEVICE_ERROR_DEFAULT_SAMPLE_INVALID: return "default sample is not a key";
	case PX_LIVE_DEVICE_ERROR_SAMPLE_STRIDE_INVALID: return "invalid sample stride";
	case PX_LIVE_DEVICE_ERROR_BINDING_INVALID: return "invalid binding";
	case PX_LIVE_DEVICE_ERROR_PROPERTY_MASK_INVALID: return "invalid property mask";
	case PX_LIVE_DEVICE_ERROR_INDEX_OUT_OF_RANGE: return "index out of range";
	case PX_LIVE_DEVICE_ERROR_HASH_COLLISION: return "axis hash is zero or duplicated";
	case PX_LIVE_DEVICE_ERROR_QUANTIZATION_INVALID: return "invalid quantization";
	default: return "unknown device format error";
	}
}
