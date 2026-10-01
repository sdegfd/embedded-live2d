#include "PainterEngine_LiveEditorModules_Importlive.h"
#include <stdlib.h>

#define PX_LIVEEDITOR_IMPORT_MAX_FILE (64u * 1024u * 1024u)

extern char *PX_OpenFileDialog(const char Filter[]);

typedef enum
{
	PX_LIVEEDITOR_LIVECHECK_OK = 0,
	PX_LIVEEDITOR_LIVECHECK_INVALID,
	PX_LIVEEDITOR_LIVECHECK_OOM
}PX_LIVEEDITOR_LIVECHECK;

static px_void PX_LiveEditorImport_Report(PX_Object *pObject,PX_Json *pLanguageJson,const px_char *key)
{
	px_char content[260];
	PX_sprintf1(content,sizeof(content),"%1",PX_STRINGFORMAT_STRING(PX_JsonGetString(pLanguageJson,key)));
	PX_ObjectExecuteEvent(pObject->pParent,PX_OBJECT_BUILD_EVENT_STRING(PX_LIVEEDITORMODULE_IMPORTLIVE_EVENT_MESSAGE,content));
}

static px_void PX_LiveEditorImport_OnPoolError(px_void *ptr,PX_MEMORYPOOL_ERROR err)
{
	px_bool *oom = (px_bool *)ptr;
	if (oom && err == PX_MEMORYPOOL_ERROR_OUTOFMEMORY)
	{
		*oom = PX_TRUE;
	}
}

static PX_LIVEEDITOR_LIVECHECK PX_LiveEditorImport_Check(px_void *buffer,px_int size)
{
	px_uint poolBytes;
	px_byte *block;
	px_memorypool pool;
	PX_LiveFramework temp;
	px_bool ok;
	px_bool oom = PX_FALSE;
	if (!buffer || size <= 0)
	{
		return PX_LIVEEDITOR_LIVECHECK_INVALID;
	}
	if ((px_uint)size > 0x0fffffffu)
	{
		return PX_LIVEEDITOR_LIVECHECK_OOM;
	}
	poolBytes = (px_uint)size * 6u + 8u * 1024u * 1024u;
	block = (px_byte *)malloc(poolBytes);
	if (!block)
	{
		return PX_LIVEEDITOR_LIVECHECK_OOM;
	}
	pool = MP_Create(block,poolBytes);
	MP_ErrorCatch(&pool,PX_LiveEditorImport_OnPoolError,&oom);
	PX_memset(&temp,0,sizeof(temp));
	ok = PX_LiveFrameworkImport(&pool,&temp,buffer,size);
	if (ok)
	{
		PX_LiveFrameworkFree(&temp);
	}
	free(block);
	if (ok)
	{
		return PX_LIVEEDITOR_LIVECHECK_OK;
	}
	return oom ? PX_LIVEEDITOR_LIVECHECK_OOM : PX_LIVEEDITOR_LIVECHECK_INVALID;
}

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

px_void PX_LiveEditorModule_ImportLiveEnable(PX_Object *pObject)
{
	static const char filter[] = "PainterEngine Live\0*.live\0\0";
	PX_LiveEditorModule_ImportLive *pImportLive = (PX_LiveEditorModule_ImportLive *)pObject->pObjectDesc[0];
	char *path;
	PX_IO_Data io;
	PX_LiveFramework incoming;
	PX_LIVEEDITOR_LIVECHECK check;

	path = PX_OpenFileDialog(filter);
	if (!path || !path[0])
	{
		return;
	}
	io = PX_LoadFileToIOData(path);
	if (!io.buffer || io.size == 0)
	{
		PX_LiveEditorImport_Report(pObject,pImportLive->pLanguageJson,"importlive.Invalid live file");
		PX_FreeIOData(&io);
		return;
	}
	if (io.size > PX_LIVEEDITOR_IMPORT_MAX_FILE || io.size > 0x7fffffffu)
	{
		PX_LiveEditorImport_Report(pObject,pImportLive->pLanguageJson,"importlive.file too large");
		PX_FreeIOData(&io);
		return;
	}
	check = PX_LiveEditorImport_Check(io.buffer,(px_int)io.size);
	if (check == PX_LIVEEDITOR_LIVECHECK_OOM)
	{
		PX_LiveEditorImport_Report(pObject,pImportLive->pLanguageJson,"importlive.out of memory");
		PX_FreeIOData(&io);
		return;
	}
	if (check != PX_LIVEEDITOR_LIVECHECK_OK)
	{
		PX_LiveEditorImport_Report(pObject,pImportLive->pLanguageJson,"importlive.Invalid live file");
		PX_FreeIOData(&io);
		return;
	}
	PX_memset(&incoming,0,sizeof(incoming));
	if (!PX_LiveFrameworkImport(&pImportLive->pruntime->mp_static,&incoming,io.buffer,(px_int)io.size))
	{
		PX_LiveEditorImport_Report(pObject,pImportLive->pLanguageJson,"importlive.out of memory");
		PX_FreeIOData(&io);
		return;
	}
	PX_LiveFrameworkFree(pImportLive->pLiveFramework);
	*pImportLive->pLiveFramework = incoming;
	PX_FreeIOData(&io);
	PX_ObjectExecuteEvent(pObject->pParent,PX_OBJECT_BUILD_EVENT(PX_LIVEEDITORMODULE_IMPORTLIVE_EVENT_CONFIRM));
}

px_void PX_LiveEditorModule_ImportLiveDisable(PX_Object *pObject)
{
	(void)pObject;
}
