#include "PainterEngine_LiveEditorModules_Exportlive.h"

#ifdef __linux__
#include "platform/modules/px_request.h"
#endif


static  px_int PX_ExportLive_ExplorerGetPathFolderCount(const px_char *path,const char *filter)
{
	return PX_FileGetDirectoryFileCount(path,PX_FILEENUM_TYPE_FOLDER,filter);
}
static px_int PX_ExportLive_ExplorerGetPathFileCount(const px_char *path,const char *filter)
{
	return 0;
}
static px_int PX_ExportLive_ExplorerGetPathFolderName(const char path[],int count,char FileName[][260],const char *filter)
{
	return PX_FileGetDirectoryFileName(path,count,FileName,PX_FILEENUM_TYPE_FOLDER,filter);
}
static px_int PX_ExportLive_ExplorerGetPathFileName(const char path[],int count,char FileName[][260],const char *filter)
{
	return 0;
}


px_void PX_Application_OnExportLive_OnConfirm(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	px_char path[260];
	px_int index=0;
	PX_Object *pExportLiveObject=(PX_Object *)ptr;
	PX_LiveEditorModule_ExportLive *pExportLive=(PX_LiveEditorModule_ExportLive *)pExportLiveObject->pObjectDesc[0];

		PX_Object_ExplorerGetPath(pExportLive->explorer,path,index);
		if (!path[0])
		{
			return;
		}
		PX_strcat(path,"\\release.live");
		do 
		{
			px_byte *cacheBuffer=(px_byte *)malloc(64*1024*1024);
			px_memorypool mp=MP_Create(cacheBuffer,64*1024*1024);
			px_memory data;
			PX_MemoryInitialize(&mp,&data);
			if(!PX_LiveFrameworkExport(pExportLive->pLiveFramework,&data))
			{
				px_char content[260];
				PX_sprintf2(content,sizeof(content),"%1 %2",PX_STRINGFORMAT_STRING(PX_JsonGetString(pExportLive->pLanguageJson,"exportlive.out of memory")),PX_STRINGFORMAT_STRING((path)));
				PX_ObjectExecuteEvent(pExportLiveObject->pParent,PX_OBJECT_BUILD_EVENT_STRING(PX_LIVEEDITORMODULE_EXPORTLIVE_EVENT_MESSAGE,content));
			}

			if(!PX_SaveDataToFile(data.buffer,data.usedsize,path))
			{
				px_char content[260];
				PX_sprintf2(content,sizeof(content),"%1 %2",PX_STRINGFORMAT_STRING(PX_JsonGetString(pExportLive->pLanguageJson,"exportlive.could not export file")),PX_STRINGFORMAT_STRING((path)));
				PX_ObjectExecuteEvent(pExportLiveObject->pParent,PX_OBJECT_BUILD_EVENT_STRING(PX_LIVEEDITORMODULE_EXPORTLIVE_EVENT_MESSAGE,content));
			}
			else 
			{
				px_char content[260];
				PX_sprintf2(content,sizeof(content),"%1 %2",PX_STRINGFORMAT_STRING(PX_JsonGetString(pExportLive->pLanguageJson,"exportlive.succeeded")),PX_STRINGFORMAT_STRING((path)));
				PX_ObjectExecuteEvent(pExportLiveObject->pParent,PX_OBJECT_BUILD_EVENT_STRING(PX_LIVEEDITORMODULE_EXPORTLIVE_EVENT_MESSAGE,content));
			} while (0);
		} while (0);
	

}


PX_Object * PX_LiveEditorModule_ExportLiveInstall(PX_Object *parent,PX_Runtime *pruntime,PX_FontModule *fm,PX_LiveFramework *pLiveFramework,PX_Json *pLanguageJson)
{
	PX_Object *pObject;
	PX_LiveEditorModule_ExportLive ExportLive,*pExportLive;
	PX_memset(&ExportLive,0,sizeof(ExportLive));
	pObject=PX_ObjectCreateEx(&pruntime->mp_dynamic,parent,0,0,0,0,0,0,0,0,0,0,&ExportLive,sizeof(ExportLive));
	pExportLive=(PX_LiveEditorModule_ExportLive *)pObject->pObjectDesc[0];

	pExportLive->fontmodule=fm;
	pExportLive->pLanguageJson=pLanguageJson;
	pExportLive->pLiveFramework=pLiveFramework;
	pExportLive->pruntime=pruntime;
	#ifndef __linux__
	pExportLive->explorer=PX_Object_ExplorerCreate(&pruntime->mp_dynamic,pObject,0,0,pruntime->surface_width,pruntime->surface_height,fm,\
	PX_ExportLive_ExplorerGetPathFolderCount,\
	PX_ExportLive_ExplorerGetPathFileCount,\
	PX_ExportLive_ExplorerGetPathFolderName,\
	PX_ExportLive_ExplorerGetPathFileName,\
	"");
	PX_Object_ExplorerSetFilter(pExportLive->explorer,".live\0.LIVE\0.Live");
	PX_Object_ExplorerSetMaxSelectCount(pExportLive->explorer,1);

	PX_ObjectRegisterEvent(pExportLive->explorer,PX_OBJECT_EVENT_EXECUTE,PX_Application_OnExportLive_OnConfirm,pObject);
	#endif
	return pObject;
}

px_void PX_LiveEditorModule_ExportLiveUninstall(PX_Object *pObject)
{
	PX_ObjectDelete(pObject);
}

px_void PX_LiveEditorModule_ExportLiveEnable(PX_Object *pObject)
{
	PX_LiveEditorModule_ExportLive *pExportLive=(PX_LiveEditorModule_ExportLive *)pObject->pObjectDesc[0];
	#ifdef __linux__
	px_char path[4096] = {0};
	if (PX_RequestSaveFile(".live", "release.live", path, sizeof(path)))
	{
		px_byte *cacheBuffer=(px_byte *)malloc(64*1024*1024);
		if (cacheBuffer)
		{
			px_memorypool mp=MP_Create(cacheBuffer,64*1024*1024);
			px_memory data;
			PX_MemoryInitialize(&mp,&data);
			if (PX_LiveFrameworkExport(pExportLive->pLiveFramework,&data) && PX_SaveDataToFile(data.buffer,data.usedsize,path))
			{
				px_char content[260];
				PX_sprintf2(content,sizeof(content),"%1 %2",PX_STRINGFORMAT_STRING(PX_JsonGetString(pExportLive->pLanguageJson,"exportlive.succeeded")),PX_STRINGFORMAT_STRING(path));
				PX_ObjectExecuteEvent(pObject->pParent,PX_OBJECT_BUILD_EVENT_STRING(PX_LIVEEDITORMODULE_EXPORTLIVE_EVENT_MESSAGE,content));
				fprintf(stderr,"LiveEditor: exported %u bytes to %s\n",data.usedsize,path);
			}
			else
			{
				px_char content[260];
				PX_sprintf2(content,sizeof(content),"%1 %2",PX_STRINGFORMAT_STRING(PX_JsonGetString(pExportLive->pLanguageJson,"exportlive.could not export file")),PX_STRINGFORMAT_STRING(path));
				PX_ObjectExecuteEvent(pObject->pParent,PX_OBJECT_BUILD_EVENT_STRING(PX_LIVEEDITORMODULE_EXPORTLIVE_EVENT_MESSAGE,content));
				fprintf(stderr,"LiveEditor: export failed for %s\n",path);
			}
			free(cacheBuffer);
		}
	}
	#else
	PX_Object_ExplorerSave(pExportLive->explorer);
	#endif
}

px_void PX_LiveEditorModule_ExportLiveDisable(PX_Object *pObject)
{
	PX_LiveEditorModule_ExportLive *pExportLive=(PX_LiveEditorModule_ExportLive *)pObject->pObjectDesc[0];
	PX_ObjectReleaseFocus(pObject);
	if (pExportLive->explorer) PX_Object_ExplorerClose(pExportLive->explorer);
}
