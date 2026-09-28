#include "PainterEngine_LiveEditorModules_ImportImage.h"

px_byte file_buffer[1024 * 1024 * 8];



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
#ifdef _MSC_VER
extern px_char PX_RequestData_Extern[128];
#endif // _MSC_VER
px_void PX_Application_OnImportImage_OnFileConfirm(px_void *buffer,px_int size,px_void *ptr)
{
	PX_Object* pImportImageObject = (PX_Object*)ptr;
	PX_LiveEditorModule_ImportImage* pImportImage = (PX_LiveEditorModule_ImportImage*)pImportImageObject->pObjectDesc[0];
	if(size>0)
	{
		static px_int image_id = 0;
		px_char id[PX_LIVE_ID_MAX_LEN] = { 0 };
		PX_LiveTexture* pLiveTexture = PX_NULL, LiveTextureInstance;

		PX_sprintf1(id,sizeof(id),"texture%1",PX_STRINGFORMAT_INT(image_id));
		image_id++;
#ifdef _MSC_VER
		PX_strcpy(id, PX_RequestData_Extern, sizeof(id));
#endif // _MSC_VER

		pLiveTexture = PX_LiveFrameworkGetLiveTextureById(pImportImage->pLiveFramework, id);
		if (pLiveTexture)
		{
			PX_TextureFree(&pLiveTexture->Texture);
			PX_memset(pLiveTexture, 0, sizeof(PX_LiveTexture));
		}
		else
		{
			PX_memset(&LiveTextureInstance, 0, sizeof(LiveTextureInstance));
			PX_VectorPushback(&pImportImage->pLiveFramework->livetextures, &LiveTextureInstance);
			pLiveTexture = PX_VECTORLAST(PX_LiveTexture, &pImportImage->pLiveFramework->livetextures);
		}

		do
		{
			px_texture loadtexture;
			
			//texture
			if (PX_TextureCreateFromMemory( &pImportImage->pruntime->mp_static,file_buffer,size, &loadtexture))
			{
				px_int left, right, top, bottom;

				if (loadtexture.width != pImportImage->pLiveFramework->width || loadtexture.height != pImportImage->pLiveFramework->height)
				{
					px_char content[260];
					PX_sprintf2(content, sizeof(content), "%1 %2", PX_STRINGFORMAT_STRING(PX_JsonGetString(pImportImage->pLanguageJson, "importimage.Invalid image size")), PX_STRINGFORMAT_INT(image_id));
					PX_TextureFree(&loadtexture);
					PX_ObjectExecuteEvent(pImportImageObject->pParent, PX_OBJECT_BUILD_EVENT_STRING(PX_LIVEEDITORMODULE_IMPORTIMAGE_EVENT_MESSAGE, content));
					PX_ObjectExecuteEvent(pImportImageObject->pParent, PX_OBJECT_BUILD_EVENT(PX_LIVEEDITORMODULE_IMPORTIMAGE_EVENT_EXIT));
					return;
				}
				PX_TextureGetVisibleRange(&loadtexture, &left, &right, &top, &bottom);
				PX_TextureCreate(&pImportImage->pruntime->mp_static, &pLiveTexture->Texture, right - left + 1, bottom - top + 1);
				PX_TextureRegionCopy(&pLiveTexture->Texture, &loadtexture, 0, 0, left, top, right, bottom, PX_ALIGN_LEFTTOP, PX_NULL);
				pLiveTexture->textureOffsetX = left;
				pLiveTexture->textureOffsetY = top;
				PX_strcpy(pLiveTexture->id, id, sizeof(pLiveTexture->id));


				PX_TextureFree(&loadtexture);
			}
			else
			{
				px_char content[260];
				PX_sprintf2(content, sizeof(content), "%1 %2", PX_STRINGFORMAT_STRING(PX_JsonGetString(pImportImage->pLanguageJson, "importimage.Could not load")), PX_STRINGFORMAT_INT((image_id)));
				PX_ObjectExecuteEvent(pImportImageObject->pParent, PX_OBJECT_BUILD_EVENT_STRING(PX_LIVEEDITORMODULE_IMPORTIMAGE_EVENT_MESSAGE, content));
				PX_ObjectExecuteEvent(pImportImageObject->pParent, PX_OBJECT_BUILD_EVENT(PX_LIVEEDITORMODULE_IMPORTIMAGE_EVENT_EXIT));
				return;
			}
		} while (0);

		PX_LiveFrameworkUpdateSourceVerticesUV(pImportImage->pLiveFramework);
		PX_ObjectExecuteEvent(pImportImageObject->pParent, PX_OBJECT_BUILD_EVENT(PX_LIVEEDITORMODULE_IMPORTIMAGE_EVENT_EXIT));
	}
	
}

px_void PX_LiveEditorModule_ImportImageEnable(PX_Object *pObject)
{
	PX_LiveEditorModule_ImportImage *pImportImage=(PX_LiveEditorModule_ImportImage *)pObject->pObjectDesc[0];
	PX_RequestData("open-multiple:.png", file_buffer, sizeof(file_buffer), pObject, PX_Application_OnImportImage_OnFileConfirm);
	//PX_Object_ExplorerOpen(pImportImage->explorer);
	//PX_ObjectSetFocus(pObject);
}

px_void PX_LiveEditorModule_ImportImageDisable(PX_Object *pObject)
{
	//PX_LiveEditorModule_ImportImage *pImportImage=(PX_LiveEditorModule_ImportImage *)pObject->pObjectDesc[0];
	//PX_ObjectReleaseFocus(pObject);
	//PX_Object_ExplorerClose(pImportImage->explorer);
}
