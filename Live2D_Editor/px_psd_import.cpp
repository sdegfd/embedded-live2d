#include "px_psd.h"

#include "PsdPch.h"
#include "PsdDocument.h"
#include "PsdParseDocument.h"
#include "PsdLayerMaskSection.h"
#include "PsdParseLayerMaskSection.h"
#include "PsdMallocAllocator.h"
#include "PsdMemoryFile.h"
#include "PsdLayer.h"
#include "PsdChannel.h"
#include "PsdChannelType.h"
#include "PsdColorMode.h"
#include "PsdLayerType.h"

#include <stdlib.h>
#include <string.h>

/* Character sheets are 720p: 1280x720, or 720x1280 when the canvas is portrait. */
static const unsigned kMaxEdge = 1280u;
static const unsigned kMaxLayerBytes = 1280u * 1280u * 4u;

namespace
{

	struct ChannelRef
	{
		const psd::Channel *channel;
		int present;
	};

	int SampleChannel(const psd::Channel *channel, unsigned bits, int x, int y, int width)
	{
		const size_t index = (size_t)y * (size_t)width + (size_t)x;
		if (!channel || !channel->data)
		{
			return -1;
		}
		if (bits == 8u)
		{
			return static_cast<const uint8_t *>(channel->data)[index];
		}
		if (bits == 16u)
		{
			return static_cast<const uint16_t *>(channel->data)[index] >> 8;
		}
		if (bits == 32u)
		{
			float value = static_cast<const float *>(channel->data)[index];
			if (value < 0.0f) value = 0.0f;
			if (value > 1.0f) value = 1.0f;
			return (int)(value * 255.0f + 0.5f);
		}
		return -1;
	}

	ChannelRef FindChannel(const psd::Layer *layer, int16_t type)
	{
		ChannelRef result;
		result.channel = 0;
		result.present = 0;
		if (!layer)
		{
			return result;
		}
		for (unsigned int i = 0; i < layer->channelCount; ++i)
		{
			if (layer->channels[i].type == type && layer->channels[i].data)
			{
				result.channel = &layer->channels[i];
				result.present = 1;
				return result;
			}
		}
		return result;
	}

	int AncestorVisible(const psd::Layer *layer)
	{
		for (const psd::Layer *parent = layer; parent; parent = parent->parent)
		{
			if (!parent->isVisible)
			{
				return 0;
			}
		}
		return 1;
	}

	int CombinedOpacity(const psd::Layer *layer)
	{
		int opacity = layer->opacity;
		for (const psd::Layer *parent = layer->parent; parent; parent = parent->parent)
		{
			opacity = opacity * (int)parent->opacity / 255;
		}
		return opacity;
	}

	char *DuplicateAscii(const char *text)
	{
		size_t length;
		char *copy;
		if (!text)
		{
			text = "";
		}
		length = strlen(text);
		copy = (char *)malloc(length + 1u);
		if (!copy)
		{
			return 0;
		}
		memcpy(copy, text, length + 1u);
		return copy;
	}

	char *Utf16ToUtf8(const uint16_t *text)
	{
		size_t units = 0;
		size_t bytes = 1;
		size_t index;
		char *out;
		char *cursor;
		if (!text)
		{
			return DuplicateAscii("");
		}
		while (text[units] != 0)
		{
			++units;
		}
		for (index = 0; index < units; ++index)
		{
			uint32_t code = text[index];
			if (code >= 0xD800u && code <= 0xDBFFu && index + 1 < units)
			{
				uint32_t low = text[index + 1];
				if (low >= 0xDC00u && low <= 0xDFFFu)
				{
					code = 0x10000u + (((code - 0xD800u) << 10) | (low - 0xDC00u));
					++index;
				}
			}
			if (code < 0x80u) bytes += 1;
			else if (code < 0x800u) bytes += 2;
			else if (code < 0x10000u) bytes += 3;
			else bytes += 4;
		}
		out = (char *)malloc(bytes);
		if (!out)
		{
			return 0;
		}
		cursor = out;
		for (index = 0; index < units; ++index)
		{
			uint32_t code = text[index];
			if (code >= 0xD800u && code <= 0xDBFFu && index + 1 < units)
			{
				uint32_t low = text[index + 1];
				if (low >= 0xDC00u && low <= 0xDFFFu)
				{
					code = 0x10000u + (((code - 0xD800u) << 10) | (low - 0xDC00u));
					++index;
				}
			}
			if (code < 0x80u)
			{
				*cursor++ = (char)code;
			}
			else if (code < 0x800u)
			{
				*cursor++ = (char)(0xC0u | (code >> 6));
				*cursor++ = (char)(0x80u | (code & 0x3Fu));
			}
			else if (code < 0x10000u)
			{
				*cursor++ = (char)(0xE0u | (code >> 12));
				*cursor++ = (char)(0x80u | ((code >> 6) & 0x3Fu));
				*cursor++ = (char)(0x80u | (code & 0x3Fu));
			}
			else
			{
				*cursor++ = (char)(0xF0u | (code >> 18));
				*cursor++ = (char)(0x80u | ((code >> 12) & 0x3Fu));
				*cursor++ = (char)(0x80u | ((code >> 6) & 0x3Fu));
				*cursor++ = (char)(0x80u | (code & 0x3Fu));
			}
		}
		*cursor = 0;
		return out;
	}

	char *LayerName(const psd::Layer *layer)
	{
		if (layer->utf16Name)
		{
			return Utf16ToUtf8(layer->utf16Name);
		}
		if (layer->name.GetLength() > 0u)
		{
			return DuplicateAscii(layer->name.c_str());
		}
		return DuplicateAscii("");
	}

	int IsGroup(const psd::Layer *layer)
	{
		return layer->type == psd::layerType::OPEN_FOLDER
			|| layer->type == psd::layerType::CLOSED_FOLDER
			|| layer->type == psd::layerType::SECTION_DIVIDER;
	}

	int AppendLayer(PX_PsdDocument *document, PX_PsdRaster *raster)
	{
		PX_PsdRaster *grown = (PX_PsdRaster *)realloc(document->layers, (size_t)(document->count + 1) * sizeof(PX_PsdRaster));
		if (!grown)
		{
			return 0;
		}
		document->layers = grown;
		document->layers[document->count] = *raster;
		document->count += 1;
		return 1;
	}
}

extern "C" void PX_PsdFreeDocument(PX_PsdDocument *document)
{
	int i;
	if (!document)
	{
		return;
	}
	for (i = 0; i < document->count; ++i)
	{
		free(document->layers[i].name);
		free(document->layers[i].rgba);
	}
	free(document->layers);
	memset(document, 0, sizeof(*document));
}

extern "C" int PX_PsdReadDocument(const void *bytes, unsigned int size, PX_PsdDocument *out)
{
	psd::MallocAllocator allocator;
	psd::MemoryFile file(&allocator);
	psd::Document *document = 0;
	psd::LayerMaskSection *section = 0;
	int status = PX_PSD_ERR_FORMAT;
	unsigned int layerIndex;

	if (!out)
	{
		return PX_PSD_ERR_FORMAT;
	}
	memset(out, 0, sizeof(*out));
	if (!bytes || size < 26u)
	{
		return PX_PSD_ERR_FORMAT;
	}
	if (!file.Open(bytes, size))
	{
		return PX_PSD_ERR_FORMAT;
	}

	document = psd::CreateDocument(&file, &allocator);
	if (!document)
	{
		return PX_PSD_ERR_FORMAT;
	}
	if (document->colorMode != psd::colorMode::RGB || (document->bitsPerChannel != 8u && document->bitsPerChannel != 16u && document->bitsPerChannel != 32u))
	{
		status = PX_PSD_ERR_COLOR;
		goto cleanup;
	}
	if (document->width == 0u || document->height == 0u || document->width > kMaxEdge || document->height > kMaxEdge)
	{
		status = PX_PSD_ERR_LIMIT;
		goto cleanup;
	}

	section = psd::ParseLayerMaskSection(document, &file, &allocator);
	if (!section)
	{
		status = PX_PSD_ERR_FORMAT;
		goto cleanup;
	}

	out->width = (int)document->width;
	out->height = (int)document->height;
	for (layerIndex = 0; layerIndex < section->layerCount; ++layerIndex)
	{
		psd::Layer *layer = &section->layers[layerIndex];
		ChannelRef red, green, blue, alpha;
		int width, height, opacity, visible, top, left, bottom, right;
		int x, y;
		unsigned char *pixels;
		PX_PsdRaster raster;

		if (IsGroup(layer))
		{
			continue;
		}
		width = layer->right - layer->left;
		height = layer->bottom - layer->top;
		if (width <= 0 || height <= 0)
		{
			continue;
		}
		if ((unsigned)width > kMaxEdge || (unsigned)height > kMaxEdge)
		{
			status = PX_PSD_ERR_LIMIT;
			goto cleanup;
		}

		psd::ExtractLayer(document, &file, &allocator, layer);
		red = FindChannel(layer, psd::channelType::R);
		green = FindChannel(layer, psd::channelType::G);
		blue = FindChannel(layer, psd::channelType::B);
		alpha = FindChannel(layer, psd::channelType::TRANSPARENCY_MASK);
		if (!red.present && !green.present && !blue.present)
		{
			continue;
		}

		opacity = CombinedOpacity(layer);
		visible = AncestorVisible(layer);
		if (opacity <= 0)
		{
			visible = 0;
			opacity = 255;
		}

		left = width;
		top = height;
		right = -1;
		bottom = -1;
		for (y = 0; y < height; ++y)
		{
			for (x = 0; x < width; ++x)
			{
				int sample = alpha.present ? SampleChannel(alpha.channel, document->bitsPerChannel, x, y, width) : 255;
				int covered;
				if (sample < 0) sample = 255;
				covered = sample * opacity / 255;
				if (covered > 0)
				{
					if (x < left) left = x;
					if (y < top) top = y;
					if (x > right) right = x;
					if (y > bottom) bottom = y;
				}
			}
		}
		if (right < left || bottom < top)
		{
			continue;
		}

		width = right - left + 1;
		height = bottom - top + 1;
		if ((unsigned long long)width * (unsigned long long)height * 4ull > kMaxLayerBytes)
		{
			status = PX_PSD_ERR_LIMIT;
			goto cleanup;
		}
		pixels = (unsigned char *)malloc((size_t)width * (size_t)height * 4u);
		if (!pixels)
		{
			status = PX_PSD_ERR_MEMORY;
			goto cleanup;
		}
		for (y = 0; y < height; ++y)
		{
			for (x = 0; x < width; ++x)
			{
				int sx = left + x;
				int sy = top + y;
				int r = red.present ? SampleChannel(red.channel, document->bitsPerChannel, sx, sy, layer->right - layer->left) : 0;
				int g = green.present ? SampleChannel(green.channel, document->bitsPerChannel, sx, sy, layer->right - layer->left) : 0;
				int b = blue.present ? SampleChannel(blue.channel, document->bitsPerChannel, sx, sy, layer->right - layer->left) : 0;
				int a = alpha.present ? SampleChannel(alpha.channel, document->bitsPerChannel, sx, sy, layer->right - layer->left) : 255;
				unsigned char *pixel = pixels + ((size_t)y * (size_t)width + (size_t)x) * 4u;
				if (r < 0) r = 0;
				if (g < 0) g = 0;
				if (b < 0) b = 0;
				if (a < 0) a = 255;
				pixel[0] = (unsigned char)r;
				pixel[1] = (unsigned char)g;
				pixel[2] = (unsigned char)b;
				pixel[3] = (unsigned char)(a * opacity / 255);
			}
		}

		memset(&raster, 0, sizeof(raster));
		raster.name = LayerName(layer);
		raster.left = layer->left + left;
		raster.top = layer->top + top;
		raster.width = width;
		raster.height = height;
		raster.visible = visible;
		raster.rgba = pixels;
		if (!raster.name || !AppendLayer(out, &raster))
		{
			free(raster.name);
			free(pixels);
			status = PX_PSD_ERR_MEMORY;
			goto cleanup;
		}
	}

	if (out->count <= 0)
	{
		status = PX_PSD_ERR_EMPTY;
		goto cleanup;
	}
	status = PX_PSD_OK;

cleanup:
	if (status != PX_PSD_OK)
	{
		PX_PsdFreeDocument(out);
	}
	if (section)
	{
		psd::DestroyLayerMaskSection(section, &allocator);
	}
	if (document)
	{
		psd::DestroyDocument(document, &allocator);
	}
	return status;
}
