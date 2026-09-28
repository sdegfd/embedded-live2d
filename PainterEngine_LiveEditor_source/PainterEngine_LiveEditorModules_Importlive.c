#include "PainterEngine_LiveEditorModules_Importlive.h"

px_byte live2d_buffer[1024 * 1024 * 16];



PX_Object * PX_LiveEditorModule_ImportLiveInstall(PX_Object *parent,PX_Runtime *pruntime,PX_FontModule *fm,PX_LiveFramework *pLiveFramework,PX_Json *pLanguageJson)
{
	PX_Object *pObject;
	PX_LiveEditorModule_ImportLive ImportLive,*pImportLive;
	PX_memset(&ImportLive,0,sizeof(ImportLive));
	pObject=PX_ObjectCreateEx(&pruntime->mp_dynamic,parent,0,0,0,0,0,0,0,0,0,0,&ImportLive,sizeof(ImportLive));
	pImportLive=(PX_LiveEditorModule_ImportLive *)pObject->pObjectDesc[0];

	pImportLive->fontmodule=fm;
	pImportLive->pLanguageJson=pLanguageJson;
	pImportLive->pLiveFramework=pLiveFramework;
	pImportLive->pruntime=pruntime;
	
	return pObject;
}

px_void PX_LiveEditorModule_ImportLiveUninstall(PX_Object *pObject)
{
	PX_ObjectDelete(pObject);
}

px_void PX_Application_OnImportLive_OnFileConfirm(px_void* buffer, px_int size, px_void* ptr)
{
	PX_Object* pImportLiveObject = (PX_Object*)ptr;
	PX_LiveEditorModule_ImportLive* pImportLive = (PX_LiveEditorModule_ImportLive*)pImportLiveObject->pObjectDesc[0];
	do
	{
		if (!size)
		{
			break;
		}
		do
		{
			PX_LiveFrameworkFree(pImportLive->pLiveFramework);
			PX_memset(pImportLive->pLiveFramework, 0, sizeof(PX_LiveFramework));
			if (!PX_LiveFrameworkImport(&pImportLive->pruntime->mp_static, pImportLive->pLiveFramework, live2d_buffer, size))
			{
				px_char content[260];
				PX_sprintf2(content, sizeof(content), "%1 %2", PX_STRINGFORMAT_STRING(PX_JsonGetString(pImportLive->pLanguageJson, "importlive.Invalid live file")), PX_STRINGFORMAT_STRING(("")));
				PX_ObjectExecuteEvent(pImportLiveObject->pParent, PX_OBJECT_BUILD_EVENT_STRING(PX_LIVEEDITORMODULE_IMPORTLIVE_EVENT_MESSAGE, content));
			}
			PX_ObjectExecuteEvent(pImportLiveObject->pParent, PX_OBJECT_BUILD_EVENT(PX_LIVEEDITORMODULE_IMPORTLIVE_EVENT_CONFIRM));
		} while (0);
	} while (0);
	
}

px_void PX_LiveEditorModule_ImportLiveEnable(PX_Object *pObject)
{
	PX_LiveEditorModule_ImportLive *pImportLive=(PX_LiveEditorModule_ImportLive *)pObject->pObjectDesc[0];
	PX_RequestData("open:.live", live2d_buffer, sizeof(live2d_buffer), pObject, PX_Application_OnImportLive_OnFileConfirm);
	//PX_Object_ExplorerOpen(pImportLive->explorer);
}

px_void PX_LiveEditorModule_ImportLiveDisable(PX_Object *pObject)
{
	//PX_LiveEditorModule_ImportLive *pImportLive=(PX_LiveEditorModule_ImportLive *)pObject->pObjectDesc[0];
	//PX_ObjectReleaseFocus(pObject);
	//PX_Object_ExplorerClose(pImportLive->explorer);
}
