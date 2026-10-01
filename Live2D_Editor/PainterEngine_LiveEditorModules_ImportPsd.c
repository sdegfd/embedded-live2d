#include "PainterEngine_LiveEditorModules_ImportPsd.h"
#include "px_psd.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <commdlg.h>

static px_void PX_ImportPsd_Report(PX_Object *pObject, PX_Json *language, const px_char *key)
{
	px_char content[260];
	PX_sprintf1(content, sizeof(content), "%1", PX_STRINGFORMAT_STRING(PX_JsonGetString(language, key)));
	PX_ObjectExecuteEvent(pObject->pParent, PX_OBJECT_BUILD_EVENT_STRING(PX_LIVEEDITORMODULE_IMPORTPSD_EVENT_MESSAGE, content));
	PX_ObjectExecuteEvent(pObject->pParent, PX_OBJECT_BUILD_EVENT(PX_LIVEEDITORMODULE_IMPORTPSD_EVENT_EXIT));
}

static px_void PX_ImportPsd_FitUtf8(px_char *dst, px_int dstBytes, const px_char *src)
{
	px_int written = 0;
	if (dstBytes <= 0)
	{
		return;
	}
	if (!src)
	{
		src = "";
	}
	while (*src && written < dstBytes - 1)
	{
		unsigned char lead = (unsigned char)*src;
		px_int need = 1;
		px_int k;
		if ((lead & 0x80u) == 0)
		{
			need = 1;
		}
		else if ((lead & 0xE0u) == 0xC0u)
		{
			need = 2;
		}
		else if ((lead & 0xF0u) == 0xE0u)
		{
			need = 3;
		}
		else if ((lead & 0xF8u) == 0xF0u)
		{
			need = 4;
		}
		else
		{
			break;
		}
		if (written + need >= dstBytes)
		{
			break;
		}
		for (k = 1; k < need; k++)
		{
			if (((unsigned char)src[k] & 0xC0u) != 0x80u)
			{
				need = 0;
				break;
			}
		}
		if (need <= 0)
		{
			break;
		}
		for (k = 0; k < need; k++)
		{
			dst[written++] = src[k];
		}
		src += need;
	}
	dst[written] = 0;
}

static px_void PX_ImportPsd_Sanitize(px_char *text)
{
	unsigned char *cursor = (unsigned char *)text;
	while (*cursor)
	{
		if (*cursor < 128u && (*cursor < 32u || *cursor == 127u || *cursor == '\\' || *cursor == '/' || *cursor == ':' || *cursor == '*' || *cursor == '?' || *cursor == '"' || *cursor == '<' || *cursor == '>' || *cursor == '|'))
		{
			*cursor = '_';
		}
		cursor++;
	}
}

static px_bool PX_ImportPsd_IdUsed(PX_LiveFramework *live, const px_char id[], px_char taken[][PX_LIVE_ID_MAX_LEN], px_int takenCount)
{
	PX_LiveLayer *layer = PX_LiveFrameworkGetLayerById(live, id);
	PX_LiveTexture *texture = PX_LiveFrameworkGetLiveTextureById(live, id);
	px_int i;
	if ((layer != PX_NULL) != (texture != PX_NULL))
	{
		return PX_TRUE;
	}
	for (i = 0; i < takenCount; i++)
	{
		if (PX_strequ(taken[i], id))
		{
			return PX_TRUE;
		}
	}
	return PX_FALSE;
}

static px_void PX_ImportPsd_AssignId(PX_LiveFramework *live, const px_char *name, px_int ordinal, px_char out[PX_LIVE_ID_MAX_LEN], px_char taken[][PX_LIVE_ID_MAX_LEN], px_int takenCount)
{
	px_char clean[256];
	px_char full[PX_LIVE_ID_MAX_LEN];
	px_char stem[PX_LIVE_ID_MAX_LEN];
	px_char candidate[PX_LIVE_ID_MAX_LEN];
	px_int suffix;

	memset(clean, 0, sizeof(clean));
	memset(full, 0, sizeof(full));
	memset(out, 0, PX_LIVE_ID_MAX_LEN);
	PX_ImportPsd_FitUtf8(clean, (px_int)sizeof(clean), name && name[0] ? name : "layer");
	PX_ImportPsd_Sanitize(clean);
	if (!clean[0])
	{
		PX_strcpy(clean, "layer", sizeof(clean));
	}
	PX_ImportPsd_FitUtf8(full, PX_LIVE_ID_MAX_LEN, clean);
	if (!PX_ImportPsd_IdUsed(live, full, taken, takenCount))
	{
		memcpy(out, full, PX_LIVE_ID_MAX_LEN);
		return;
	}

	PX_ImportPsd_FitUtf8(stem, 28, full);
	for (suffix = 2; suffix < 1000; suffix++)
	{
		px_char tail[8];
		px_int tailLength;
		sprintf(tail, "_%d", suffix);
		tailLength = (px_int)strlen(tail);
		memset(candidate, 0, sizeof(candidate));
		PX_ImportPsd_FitUtf8(candidate, PX_LIVE_ID_MAX_LEN - tailLength, stem);
		strcat(candidate, tail);
		if (!PX_ImportPsd_IdUsed(live, candidate, taken, takenCount))
		{
			memcpy(out, candidate, PX_LIVE_ID_MAX_LEN);
			return;
		}
	}
	memset(out, 0, PX_LIVE_ID_MAX_LEN);
	sprintf(out, "p%04d", ordinal);
}

static px_bool PX_ImportPsd_CreateTexture(px_memorypool *mp, const PX_PsdRaster *raster, px_texture *texture)
{
	px_int i, count;
	px_color *pixels;
	memset(texture, 0, sizeof(*texture));
	if (!PX_TextureCreate(mp, texture, raster->width, raster->height))
	{
		return PX_FALSE;
	}
	pixels = texture->surfaceBuffer;
	count = raster->width * raster->height;
	for (i = 0; i < count; i++)
	{
		const unsigned char *source = raster->rgba + (size_t)i * 4u;
		pixels[i]._argb.r = source[0];
		pixels[i]._argb.g = source[1];
		pixels[i]._argb.b = source[2];
		pixels[i]._argb.a = source[3];
	}
	return PX_TRUE;
}

static px_byte *PX_ImportPsd_ReadFile(const wchar_t *path, unsigned int *sizeOut)
{
	HANDLE file;
	LARGE_INTEGER size;
	DWORD readBytes = 0;
	px_byte *data;
	const LONGLONG kMaxFile = 64ll * 1024ll * 1024ll;

	file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, PX_NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, PX_NULL);
	if (file == INVALID_HANDLE_VALUE)
	{
		return PX_NULL;
	}
	if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > kMaxFile)
	{
		CloseHandle(file);
		return PX_NULL;
	}
	data = (px_byte *)malloc((size_t)size.QuadPart);
	if (!data)
	{
		CloseHandle(file);
		return PX_NULL;
	}
	if (!ReadFile(file, data, (DWORD)size.QuadPart, &readBytes, PX_NULL) || readBytes != (DWORD)size.QuadPart)
	{
		free(data);
		CloseHandle(file);
		return PX_NULL;
	}
	CloseHandle(file);
	*sizeOut = (unsigned int)size.QuadPart;
	return data;
}

static px_bool PX_ImportPsd_PickFile(wchar_t *path, px_int pathChars)
{
	OPENFILENAMEW dialog;
	memset(&dialog, 0, sizeof(dialog));
	path[0] = 0;
	dialog.lStructSize = sizeof(dialog);
	dialog.hwndOwner = GetActiveWindow();
	dialog.lpstrFilter = L"Photoshop PSD\0*.psd;*.psb\0\0";
	dialog.nFilterIndex = 1;
	dialog.lpstrFile = path;
	dialog.nMaxFile = (DWORD)pathChars;
	dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	return GetOpenFileNameW(&dialog) ? PX_TRUE : PX_FALSE;
}

typedef struct
{
	px_int index;
	px_bool isNew;
	px_texture previous;
	px_int previousX;
	px_int previousY;
} PX_ImportPsdTextureUndo;

typedef struct
{
	px_int index;
	px_bool isNew;
	px_float z;
	px_point keyPoint;
	px_point currentKeyPoint;
	px_bool visible;
	px_int link;
	px_int render;
} PX_ImportPsdLayerUndo;

static px_void PX_ImportPsd_Undo(PX_LiveFramework *live, px_int oldWidth, px_int oldHeight, px_int oldTextureCount, px_int oldLayerCount, PX_ImportPsdTextureUndo *textures, px_int textureCount, PX_ImportPsdLayerUndo *layers, px_int layerCount)
{
	px_int i;
	for (i = layerCount - 1; i >= 0; i--)
	{
		if (!layers[i].isNew && layers[i].index >= 0 && layers[i].index < live->layers.size)
		{
			PX_LiveLayer *layer = PX_LiveFrameworkGetLayer(live, layers[i].index);
			layer->keyPoint = layers[i].keyPoint;
			layer->currentKeyPoint = layers[i].currentKeyPoint;
			layer->visible = layers[i].visible;
			layer->LinkTextureIndex = layers[i].link;
			layer->RenderTextureIndex = layers[i].render;
		}
	}
	while (live->layers.size > oldLayerCount)
	{
		PX_LiveFrameworkDeleteLayer(live, live->layers.size - 1);
	}
	for (i = 0; i < textureCount; i++)
	{
		if (!textures[i].isNew && textures[i].index >= 0 && textures[i].index < live->livetextures.size)
		{
			PX_LiveTexture *texture = PX_LiveFrameworkGetLiveTexture(live, textures[i].index);
			PX_TextureFree(&texture->Texture);
			texture->Texture = textures[i].previous;
			texture->textureOffsetX = textures[i].previousX;
			texture->textureOffsetY = textures[i].previousY;
			memset(&textures[i].previous, 0, sizeof(textures[i].previous));
		}
	}
	while (live->livetextures.size > oldTextureCount)
	{
		PX_LiveFrameworkDeleteLiveTexture(live, live->livetextures.size - 1);
	}
	live->width = oldWidth;
	live->height = oldHeight;
}

static const px_char *PX_ImportPsd_StatusKey(int status)
{
	switch (status)
	{
	case PX_PSD_ERR_COLOR:
	case PX_PSD_ERR_LIMIT:
		return "importpsd.Unsupported document";
	case PX_PSD_ERR_EMPTY:
		return "importpsd.No layers";
	case PX_PSD_ERR_MEMORY:
		return "importpsd.Out of memory";
	default:
		return "importpsd.Could not load";
	}
}

static px_bool PX_ImportPsd_Apply(PX_Object *pObject, PX_LiveEditorModule_ImportPsd *importer, const PX_PsdDocument *document, px_char ids[][PX_LIVE_ID_MAX_LEN])
{
	PX_LiveFramework *live = importer->pLiveFramework;
	PX_ImportPsdTextureUndo textureUndo[PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER];
	PX_ImportPsdLayerUndo layerUndo[PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER];
	px_int textureUndoCount = 0;
	px_int layerUndoCount = 0;
	px_int oldTextureCount = live->livetextures.size;
	px_int oldLayerCount = live->layers.size;
	px_int oldWidth = live->width;
	px_int oldHeight = live->height;
	px_int i;
	px_int additions = 0;
	px_bool emptyProject = (live->layers.size == 0 && live->livetextures.size == 0);

	for (i = 0; i < document->count; i++)
	{
		if (!PX_LiveFrameworkGetLayerById(live, ids[i]))
		{
			additions++;
		}
	}
	if (document->count > PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER || oldLayerCount + additions > PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER)
	{
		PX_ImportPsd_Report(pObject, importer->pLanguageJson, "importpsd.Too many layers");
		return PX_FALSE;
	}
	if (!emptyProject && (live->width != document->width || live->height != document->height))
	{
		PX_ImportPsd_Report(pObject, importer->pLanguageJson, "importpsd.Canvas mismatch");
		return PX_FALSE;
	}
	if (emptyProject)
	{
		live->width = document->width;
		live->height = document->height;
	}

	for (i = 0; i < document->count; i++)
	{
		const PX_PsdRaster *raster = &document->layers[i];
		PX_LiveTexture *existing = PX_LiveFrameworkGetLiveTextureById(live, ids[i]);
		px_texture created;
		px_int textureIndex;
		if (!PX_ImportPsd_CreateTexture(&importer->pruntime->mp_static, raster, &created))
		{
			PX_ImportPsd_Undo(live, oldWidth, oldHeight, oldTextureCount, oldLayerCount, textureUndo, textureUndoCount, layerUndo, layerUndoCount);
			PX_ImportPsd_Report(pObject, importer->pLanguageJson, "importpsd.Out of memory");
			return PX_FALSE;
		}
		if (existing && PX_LiveFrameworkGetLayerById(live, ids[i]))
		{
			textureUndo[textureUndoCount].index = PX_LiveFrameworkGetLiveTextureIndexById(live, ids[i]);
			textureUndo[textureUndoCount].isNew = PX_FALSE;
			textureUndo[textureUndoCount].previous = existing->Texture;
			textureUndo[textureUndoCount].previousX = existing->textureOffsetX;
			textureUndo[textureUndoCount].previousY = existing->textureOffsetY;
			existing->Texture = created;
			existing->textureOffsetX = raster->left;
			existing->textureOffsetY = raster->top;
			textureIndex = textureUndo[textureUndoCount].index;
			textureUndoCount++;
		}
		else
		{
			PX_LiveTexture createdLive;
			memset(&createdLive, 0, sizeof(createdLive));
			createdLive.Texture = created;
			createdLive.textureOffsetX = raster->left;
			createdLive.textureOffsetY = raster->top;
			memcpy(createdLive.id, ids[i], PX_LIVE_ID_MAX_LEN);
			if (!PX_LiveFrameworkAddLiveTexture(live, createdLive))
			{
				PX_TextureFree(&created);
				PX_ImportPsd_Undo(live, oldWidth, oldHeight, oldTextureCount, oldLayerCount, textureUndo, textureUndoCount, layerUndo, layerUndoCount);
				PX_ImportPsd_Report(pObject, importer->pLanguageJson, "importpsd.Out of memory");
				return PX_FALSE;
			}
			textureIndex = live->livetextures.size - 1;
			textureUndo[textureUndoCount].index = textureIndex;
			textureUndo[textureUndoCount].isNew = PX_TRUE;
			memset(&textureUndo[textureUndoCount].previous, 0, sizeof(textureUndo[textureUndoCount].previous));
			textureUndoCount++;
		}

		{
			PX_LiveLayer *layer = PX_LiveFrameworkGetLayerById(live, ids[i]);
			px_bool isNew = PX_FALSE;
			px_float z = (px_float)(document->count - i);
			if (!layer)
			{
				layer = PX_LiveFrameworkCreateLayer(live, ids[i]);
				isNew = PX_TRUE;
			}
			if (!layer)
			{
				PX_ImportPsd_Undo(live, oldWidth, oldHeight, oldTextureCount, oldLayerCount, textureUndo, textureUndoCount, layerUndo, layerUndoCount);
				PX_ImportPsd_Report(pObject, importer->pLanguageJson, "importpsd.Out of memory");
				return PX_FALSE;
			}
			layerUndo[layerUndoCount].index = PX_LiveFrameworkGetLayerIndex(live, layer);
			layerUndo[layerUndoCount].isNew = isNew;
			layerUndo[layerUndoCount].z = layer->keyPoint.z;
			layerUndo[layerUndoCount].keyPoint = layer->keyPoint;
			layerUndo[layerUndoCount].currentKeyPoint = layer->currentKeyPoint;
			layerUndo[layerUndoCount].visible = layer->visible;
			layerUndo[layerUndoCount].link = layer->LinkTextureIndex;
			layerUndo[layerUndoCount].render = layer->RenderTextureIndex;
			layerUndoCount++;

			layer->visible = raster->visible ? PX_TRUE : PX_FALSE;
			{
				px_bool meshed = (layer->vertices.size != 0);
				px_point savedKey = layer->keyPoint;
				px_point savedTranslation = layer->rel_currentTranslation;
				if (!PX_LiveFrameworkLinkLayerTexture(live, ids[i], ids[i]))
				{
					PX_ImportPsd_Undo(live, oldWidth, oldHeight, oldTextureCount, oldLayerCount, textureUndo, textureUndoCount, layerUndo, layerUndoCount);
					PX_ImportPsd_Report(pObject, importer->pLanguageJson, "importpsd.Could not load");
					return PX_FALSE;
				}
				if (meshed)
				{
					layer->keyPoint.x = savedKey.x;
					layer->keyPoint.y = savedKey.y;
					layer->rel_currentTranslation = savedTranslation;
					PX_LiveFrameworkUpdateLayerSourceVerticesUV(live, layer);
				}
				layer->keyPoint.z = z;
				layer->currentKeyPoint.z = z;
				if (!meshed)
				{
					layer->currentKeyPoint.x = layer->keyPoint.x;
					layer->currentKeyPoint.y = layer->keyPoint.y;
				}
			}
		}
	}
	for (i = 0; i < textureUndoCount; i++)
	{
		if (!textureUndo[i].isNew)
		{
			PX_TextureFree(&textureUndo[i].previous);
		}
	}
	return PX_TRUE;
}

PX_Object *PX_LiveEditorModule_ImportPsdInstall(PX_Object *parent, PX_Runtime *pruntime, PX_FontModule *fm, PX_LiveFramework *pLiveFramework, PX_Json *pLanguageJson)
{
	PX_Object *pObject;
	PX_LiveEditorModule_ImportPsd module;
	PX_LiveEditorModule_ImportPsd *importer;
	memset(&module, 0, sizeof(module));
	pObject = PX_ObjectCreateEx(&pruntime->mp_dynamic, parent, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, &module, sizeof(module));
	importer = (PX_LiveEditorModule_ImportPsd *)pObject->pObjectDesc[0];
	importer->fontmodule = fm;
	importer->pLanguageJson = pLanguageJson;
	importer->pLiveFramework = pLiveFramework;
	importer->pruntime = pruntime;
	return pObject;
}

px_void PX_LiveEditorModule_ImportPsdUninstall(PX_Object *pObject)
{
	PX_ObjectDelete(pObject);
}

px_void PX_LiveEditorModule_ImportPsdEnable(PX_Object *pObject)
{
	PX_LiveEditorModule_ImportPsd *importer = (PX_LiveEditorModule_ImportPsd *)pObject->pObjectDesc[0];
	wchar_t path[1024];
	px_byte *file = PX_NULL;
	unsigned int fileSize = 0;
	PX_PsdDocument document;
	px_char ids[PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER][PX_LIVE_ID_MAX_LEN];
	int status;
	px_int i;

	if (!PX_ImportPsd_PickFile(path, (px_int)(sizeof(path) / sizeof(path[0]))))
	{
		return;
	}
	file = PX_ImportPsd_ReadFile(path, &fileSize);
	if (!file)
	{
		PX_ImportPsd_Report(pObject, importer->pLanguageJson, "importpsd.Could not load");
		return;
	}
	memset(&document, 0, sizeof(document));
	status = PX_PsdReadDocument(file, fileSize, &document);
	free(file);
	if (status != PX_PSD_OK)
	{
		PX_ImportPsd_Report(pObject, importer->pLanguageJson, PX_ImportPsd_StatusKey(status));
		return;
	}
	if (document.count > PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER)
	{
		PX_PsdFreeDocument(&document);
		PX_ImportPsd_Report(pObject, importer->pLanguageJson, "importpsd.Too many layers");
		return;
	}
	for (i = 0; i < document.count; i++)
	{
		PX_ImportPsd_AssignId(importer->pLiveFramework, document.layers[i].name, i, ids[i], ids, i);
	}
	if (PX_ImportPsd_Apply(pObject, importer, &document, ids))
	{
		PX_ObjectExecuteEvent(pObject->pParent, PX_OBJECT_BUILD_EVENT(PX_LIVEEDITORMODULE_IMPORTPSD_EVENT_EXIT));
	}
	PX_PsdFreeDocument(&document);
}

px_void PX_LiveEditorModule_ImportPsdDisable(PX_Object *pObject)
{
	(void)pObject;
}
