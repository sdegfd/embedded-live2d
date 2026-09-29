#include "PainterEngine_LiveEditorModules_ImportImage.h"
#include <stdlib.h>

extern char *PX_OpenFileDialog(const char Filter[]);

static px_void PX_LiveEditorImportImage_Report(PX_Object *pObject,PX_Json *pLanguageJson,const px_char *key)
{
	px_char content[260];
	PX_sprintf1(content,sizeof(content),"%1",PX_STRINGFORMAT_STRING(PX_JsonGetString(pLanguageJson,key)));
	PX_ObjectExecuteEvent(pObject->pParent,PX_OBJECT_BUILD_EVENT_STRING(PX_LIVEEDITORMODULE_IMPORTIMAGE_EVENT_MESSAGE,content));
	PX_ObjectExecuteEvent(pObject->pParent,PX_OBJECT_BUILD_EVENT(PX_LIVEEDITORMODULE_IMPORTIMAGE_EVENT_EXIT));
}

static px_void PX_LiveEditorImportImage_UpdateLinkedUV(PX_LiveFramework *plive,px_int textureIndex)
{
	px_int i;
	for (i = 0; i < plive->layers.size; i++)
	{
		PX_LiveLayer *pLayer = PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
		if (pLayer->LinkTextureIndex == textureIndex)
		{
			PX_LiveFrameworkUpdateLayerSourceVerticesUV(plive,pLayer);
		}
	}
}

PX_Object * PX_LiveEditorModule_ImportImageInstall(PX_Object *parent,PX_Runtime *pruntime,PX_FontModule *fm,PX_LiveFramework *pLiveFramework,PX_Json *pLanguageJson)
{
	PX_Object *pObject;
	PX_LiveEditorModule_ImportImage ImportImage,*pImportImage;
	PX_memset(&ImportImage,0,sizeof(ImportImage));
	pObject=PX_ObjectCreateEx(&pruntime->mp_dynamic,parent,0,0,0,0,0,0,0,0,0,0,&ImportImage,sizeof(ImportImage));
	pImportImage=(PX_LiveEditorModule_ImportImage *)pObject->pObjectDesc[0];

	pImportImage->fontmodule=fm;
	pImportImage->pLanguageJson=pLanguageJson;
	pImportImage->pLiveFramework=pLiveFramework;
	pImportImage->pruntime=pruntime;

	return pObject;
}

px_void PX_LiveEditorModule_ImportImageUninstall(PX_Object *pObject)
{
	PX_ObjectDelete(pObject);
}

px_void PX_LiveEditorModule_ImportImageEnable(PX_Object *pObject)
{
	static const char filter[] = "Image\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0\0";
	PX_LiveEditorModule_ImportImage *pImportImage = (PX_LiveEditorModule_ImportImage *)pObject->pObjectDesc[0];
	PX_LiveFramework *plive = pImportImage->pLiveFramework;
	char *path;
	px_char id[PX_LIVE_ID_MAX_LEN];
	PX_IO_Data io;
	px_texture decoded;
	px_texture cropped;
	px_int left,right,top,bottom,width,height;
	px_int textureIndex;
	PX_LiveTexture *pLiveTexture;
	PX_LiveTexture created;

	path = PX_OpenFileDialog(filter);
	if (!path || !path[0])
	{
		return;
	}
	io = PX_LoadFileToIOData(path);
	if (!io.buffer || io.size == 0 || io.size > 0x7fffffffu)
	{
		PX_LiveEditorImportImage_Report(pObject,pImportImage->pLanguageJson,"importimage.Could not load");
		PX_FreeIOData(&io);
		return;
	}
	PX_memset(&decoded,0,sizeof(decoded));
	if (!PX_TextureCreateFromMemory(&pImportImage->pruntime->mp_static,io.buffer,(px_int)io.size,&decoded))
	{
		PX_FreeIOData(&io);
		PX_LiveEditorImportImage_Report(pObject,pImportImage->pLanguageJson,"importimage.Could not load");
		return;
	}
	PX_FreeIOData(&io);
	if (decoded.width != plive->width || decoded.height != plive->height)
	{
		PX_TextureFree(&decoded);
		PX_LiveEditorImportImage_Report(pObject,pImportImage->pLanguageJson,"importimage.Invalid image size");
		return;
	}
	PX_TextureGetVisibleRange(&decoded,&left,&right,&top,&bottom);
	width = right - left + 1;
	height = bottom - top + 1;
	if (width <= 0 || height <= 0)
	{
		PX_TextureFree(&decoded);
		PX_LiveEditorImportImage_Report(pObject,pImportImage->pLanguageJson,"importimage.Could not load");
		return;
	}
	PX_memset(&cropped,0,sizeof(cropped));
	if (!PX_TextureCreate(&pImportImage->pruntime->mp_static,&cropped,width,height))
	{
		PX_TextureFree(&decoded);
		PX_LiveEditorImportImage_Report(pObject,pImportImage->pLanguageJson,"importimage.out of memory");
		return;
	}
	PX_TextureRegionCopy(&cropped,&decoded,0,0,left,top,right,bottom,PX_ALIGN_LEFTTOP,PX_NULL);
	PX_TextureFree(&decoded);

	PX_memset(id,0,sizeof(id));
	PX_FileGetName(path,id,sizeof(id));
	if (!id[0])
	{
		PX_TextureFree(&cropped);
		PX_LiveEditorImportImage_Report(pObject,pImportImage->pLanguageJson,"importimage.Could not load");
		return;
	}
	pLiveTexture = PX_LiveFrameworkGetLiveTextureById(plive,id);
	if (pLiveTexture)
	{
		PX_TextureFree(&pLiveTexture->Texture);
		pLiveTexture->Texture = cropped;
		pLiveTexture->textureOffsetX = left;
		pLiveTexture->textureOffsetY = top;
		PX_strcpy(pLiveTexture->id,id,sizeof(pLiveTexture->id));
		textureIndex = (px_int)(pLiveTexture - (PX_LiveTexture *)plive->livetextures.data);
	}
	else
	{
		PX_memset(&created,0,sizeof(created));
		created.Texture = cropped;
		created.textureOffsetX = left;
		created.textureOffsetY = top;
		PX_strcpy(created.id,id,sizeof(created.id));
		if (!PX_VectorPushback(&plive->livetextures,&created))
		{
			PX_TextureFree(&created.Texture);
			PX_LiveEditorImportImage_Report(pObject,pImportImage->pLanguageJson,"importimage.out of memory");
			return;
		}
		textureIndex = plive->livetextures.size - 1;
	}
	PX_LiveEditorImportImage_UpdateLinkedUV(plive,textureIndex);
	PX_ObjectExecuteEvent(pObject->pParent,PX_OBJECT_BUILD_EVENT(PX_LIVEEDITORMODULE_IMPORTIMAGE_EVENT_EXIT));
}

px_void PX_LiveEditorModule_ImportImageDisable(PX_Object *pObject)
{
	(void)pObject;
}
