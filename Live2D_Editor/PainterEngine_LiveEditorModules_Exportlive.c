#include "PainterEngine_LiveEditorModules_Exportlive.h"
#include <stdlib.h>


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


static px_void PX_ExportLive_Report(PX_Object *pObject,PX_Json *pLanguageJson,const px_char *key,const px_char *path)
{
	px_char content[260];
	PX_sprintf2(content,sizeof(content),"%1 %2",PX_STRINGFORMAT_STRING(PX_JsonGetString(pLanguageJson,key)),PX_STRINGFORMAT_STRING(path));
	PX_ObjectExecuteEvent(pObject->pParent,PX_OBJECT_BUILD_EVENT_STRING(PX_LIVEEDITORMODULE_EXPORTLIVE_EVENT_MESSAGE,content));
}

static px_bool PX_ExportLive_Join(px_char *out,px_int cap,const px_char *folder,const px_char *leaf)
{
	px_int folderLen;
	px_int leafLen;
	if (!out || cap <= 1 || !folder || !leaf)
	{
		return PX_FALSE;
	}
	folderLen = PX_strlen(folder);
	leafLen = PX_strlen(leaf);
	if (folderLen < 0 || leafLen < 0 || folderLen + leafLen >= cap)
	{
		return PX_FALSE;
	}
	PX_strcpy(out,folder,cap);
	PX_strcat(out,leaf);
	return PX_TRUE;
}

static px_bool PX_ExportLive_ReloadOk(px_void *buffer,px_int size)
{
	px_uint poolBytes;
	px_byte *block;
	px_memorypool pool;
	PX_LiveFramework temp;
	px_bool ok;
	if (!buffer || size <= 0)
	{
		return PX_FALSE;
	}
	poolBytes = (px_uint)size * 6u + 8u * 1024u * 1024u;
	block = (px_byte *)malloc(poolBytes);
	if (!block)
	{
		return PX_FALSE;
	}
	pool = MP_Create(block,poolBytes);
	PX_memset(&temp,0,sizeof(temp));
	ok = PX_LiveFrameworkImport(&pool,&temp,buffer,size);
	if (ok)
	{
		PX_LiveFrameworkFree(&temp);
	}
	free(block);
	return ok;
}

static px_bool PX_ExportLive_WriteFramework(PX_LiveFramework *plive,px_byte **blockOut,px_memory *dataOut)
{
	static const px_uint sizes[4] = { 8u * 1024u * 1024u,16u * 1024u * 1024u,32u * 1024u * 1024u,64u * 1024u * 1024u };
	px_int i;
	*blockOut = PX_NULL;
	for (i = 0; i < 4; i++)
	{
		px_byte *block = (px_byte *)malloc(sizes[i]);
		px_memorypool mp;
		px_memory data;
		if (!block)
		{
			continue;
		}
		mp = MP_Create(block,sizes[i]);
		PX_MemoryInitialize(&mp,&data);
		if (PX_LiveFrameworkExport(plive,&data) && data.buffer && data.usedsize > 0)
		{
			*blockOut = block;
			*dataOut = data;
			return PX_TRUE;
		}
		free(block);
	}
	return PX_FALSE;
}

px_void PX_Application_OnExportLive_OnConfirm(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	px_char folder[260];
	px_char finalPath[260];
	px_char tmpPath[260];
	px_char bakPath[260];
	px_int index = 0;
	px_byte *block = PX_NULL;
	px_memory data;
	PX_IO_Data written;
	PX_Object *pExportLiveObject = (PX_Object *)ptr;
	PX_LiveEditorModule_ExportLive *pExportLive = (PX_LiveEditorModule_ExportLive *)pExportLiveObject->pObjectDesc[0];
	(void)pObject;
	(void)e;

	PX_memset(folder,0,sizeof(folder));
	PX_Object_ExplorerGetPath(pExportLive->explorer,folder,index);
	if (!folder[0])
	{
		return;
	}
	if (!PX_ExportLive_Join(finalPath,sizeof(finalPath),folder,"\\release.live") ||
		!PX_ExportLive_Join(tmpPath,sizeof(tmpPath),folder,"\\release.live.tmp") ||
		!PX_ExportLive_Join(bakPath,sizeof(bakPath),folder,"\\release.live.bak"))
	{
		PX_ExportLive_Report(pExportLiveObject,pExportLive->pLanguageJson,"exportlive.path too long",folder);
		return;
	}
	PX_memset(&data,0,sizeof(data));
	if (!PX_ExportLive_WriteFramework(pExportLive->pLiveFramework,&block,&data) ||
		!PX_ExportLive_ReloadOk(data.buffer,data.usedsize))
	{
		free(block);
		PX_ExportLive_Report(pExportLiveObject,pExportLive->pLanguageJson,"exportlive.out of memory",finalPath);
		return;
	}
	if (!PX_SaveDataToFile(data.buffer,data.usedsize,tmpPath))
	{
		free(block);
		PX_ExportLive_Report(pExportLiveObject,pExportLive->pLanguageJson,"exportlive.could not export file",finalPath);
		return;
	}
	free(block);
	block = PX_NULL;
	written = PX_LoadFileToIOData(tmpPath);
	if (!written.buffer || written.size == 0 || written.size > 0x7fffffffu ||
		!PX_ExportLive_ReloadOk(written.buffer,(px_int)written.size))
	{
		PX_FreeIOData(&written);
		PX_FileDelete(tmpPath);
		PX_ExportLive_Report(pExportLiveObject,pExportLive->pLanguageJson,"exportlive.verify failed",finalPath);
		return;
	}
	PX_FreeIOData(&written);
	if (PX_FileExist(finalPath))
	{
		if (PX_FileExist(bakPath))
		{
			PX_FileDelete(bakPath);
		}
		if (!PX_FileMove(finalPath,bakPath))
		{
			PX_FileDelete(tmpPath);
			PX_ExportLive_Report(pExportLiveObject,pExportLive->pLanguageJson,"exportlive.could not export file",finalPath);
			return;
		}
		if (!PX_FileMove(tmpPath,finalPath))
		{
			PX_FileMove(bakPath,finalPath);
			PX_FileDelete(tmpPath);
			PX_ExportLive_Report(pExportLiveObject,pExportLive->pLanguageJson,"exportlive.could not export file",finalPath);
			return;
		}
		PX_FileDelete(bakPath);
	}
	else if (!PX_FileMove(tmpPath,finalPath))
	{
		PX_FileDelete(tmpPath);
		PX_ExportLive_Report(pExportLiveObject,pExportLive->pLanguageJson,"exportlive.could not export file",finalPath);
		return;
	}
	PX_ExportLive_Report(pExportLiveObject,pExportLive->pLanguageJson,"exportlive.succeeded",finalPath);
	if (pExportLive->poseAccessor && PX_LiveEditorRealtimePoseAccessorHasUnbakedEdits(pExportLive->poseAccessor))
	{
		PX_ExportLive_Report(pExportLiveObject,pExportLive->pLanguageJson,"exportlive.dirty baked",finalPath);
	}
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
	pExportLive->explorer=PX_Object_ExplorerCreate(&pruntime->mp_dynamic,pObject,0,0,pruntime->surface_width,pruntime->surface_height,fm,\
	PX_ExportLive_ExplorerGetPathFolderCount,\
	PX_ExportLive_ExplorerGetPathFileCount,\
	PX_ExportLive_ExplorerGetPathFolderName,\
	PX_ExportLive_ExplorerGetPathFileName,\
	"");
	PX_Object_ExplorerSetFilter(pExportLive->explorer,".live\0.LIVE\0.Live");
	PX_Object_ExplorerSetMaxSelectCount(pExportLive->explorer,1);

	PX_ObjectRegisterEvent(pExportLive->explorer,PX_OBJECT_EVENT_EXECUTE,PX_Application_OnExportLive_OnConfirm,pObject);
	return pObject;
}

px_void PX_LiveEditorModule_ExportLiveSetPoseAccessor(PX_Object *pObject,PX_LiveEditorRealtimePoseAccessor *accessor)
{
	PX_LiveEditorModule_ExportLive *pExportLive;
	if (!pObject)
	{
		return;
	}
	pExportLive = (PX_LiveEditorModule_ExportLive *)pObject->pObjectDesc[0];
	pExportLive->poseAccessor = accessor;
}

px_void PX_LiveEditorModule_ExportLiveUninstall(PX_Object *pObject)
{
	PX_ObjectDelete(pObject);
}

px_void PX_LiveEditorModule_ExportLiveEnable(PX_Object *pObject)
{
	PX_LiveEditorModule_ExportLive *pExportLive=(PX_LiveEditorModule_ExportLive *)pObject->pObjectDesc[0];
	PX_Object_ExplorerSave(pExportLive->explorer);
}

px_void PX_LiveEditorModule_ExportLiveDisable(PX_Object *pObject)
{
	PX_LiveEditorModule_ExportLive *pExportLive=(PX_LiveEditorModule_ExportLive *)pObject->pObjectDesc[0];
	PX_ObjectReleaseFocus(pObject);
	PX_Object_ExplorerClose(pExportLive->explorer);
}
