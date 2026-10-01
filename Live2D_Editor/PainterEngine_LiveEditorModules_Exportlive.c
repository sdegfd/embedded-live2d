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

static px_bool PX_ExportLive_AddBytes(px_uint64 *total,px_uint64 more)
{
	if (more > ~(px_uint64)0 - *total)
	{
		return PX_FALSE;
	}
	*total += more;
	return PX_TRUE;
}

static px_bool PX_ExportLive_AddSpan(px_uint64 *total,px_int count,px_int nodeSize)
{
	if (count < 0 || nodeSize < 0)
	{
		return PX_FALSE;
	}
	if (count == 0 || nodeSize == 0)
	{
		return PX_TRUE;
	}
	return PX_ExportLive_AddBytes(total,(px_uint64)count * (px_uint64)nodeSize);
}

static px_uint PX_ExportLive_Pow2AtLeast(px_uint64 bytes)
{
	px_uint n = 1u;
	if (bytes == 0 || bytes > 0x40000000ull)
	{
		return 0;
	}
	while ((px_uint64)n < bytes)
	{
		n <<= 1;
	}
	return n;
}

/* The export buffer grows by powers of two. MP_Free of a block that is not the
 * tail does not return its bytes to FreeSize, so an 8MB pool cannot grow a
 * buffer past 2MB. Allocate the final size once instead. */
static px_bool PX_ExportLive_EstimateAlloc(PX_LiveFramework *plive,px_uint *allocOut)
{
	px_uint64 bytes = 1024;
	px_int i;
	if (!plive || !allocOut)
	{
		return PX_FALSE;
	}
	for (i = 0; i < plive->livetextures.size; i++)
	{
		PX_LiveTexture *pTexture = PX_VECTORAT(PX_LiveTexture,&plive->livetextures,i);
		px_int width;
		px_int height;
		if (!pTexture)
		{
			return PX_FALSE;
		}
		width = pTexture->Texture.width;
		height = pTexture->Texture.height;
		if (width < 0 || height < 0)
		{
			return PX_FALSE;
		}
		if (!PX_ExportLive_AddBytes(&bytes,128) ||
			!PX_ExportLive_AddBytes(&bytes,(px_uint64)width * (px_uint64)height * 4ull))
		{
			return PX_FALSE;
		}
	}
	for (i = 0; i < plive->layers.size; i++)
	{
		PX_LiveLayer *pLayer = PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
		if (!pLayer ||
			!PX_ExportLive_AddBytes(&bytes,256) ||
			!PX_ExportLive_AddSpan(&bytes,pLayer->triangles.size,pLayer->triangles.nodesize) ||
			!PX_ExportLive_AddSpan(&bytes,pLayer->vertices.size,pLayer->vertices.nodesize))
		{
			return PX_FALSE;
		}
	}
	for (i = 0; i < plive->liveAnimations.size; i++)
	{
		PX_LiveAnimation *pAnimation = PX_VECTORAT(PX_LiveAnimation,&plive->liveAnimations,i);
		px_int frame;
		if (!pAnimation || !PX_ExportLive_AddBytes(&bytes,64))
		{
			return PX_FALSE;
		}
		for (frame = 0; frame < pAnimation->framesMemPtr.size; frame++)
		{
			px_void *pdata = *PX_VECTORAT(px_void *,&pAnimation->framesMemPtr,frame);
			PX_LiveAnimationFrameHeader *pheader;
			if (!pdata)
			{
				return PX_FALSE;
			}
			pheader = (PX_LiveAnimationFrameHeader *)pdata;
			if (pheader->size < 0 ||
				!PX_ExportLive_AddBytes(&bytes,(px_uint64)sizeof(PX_LiveAnimationFrameHeader) + (px_uint64)pheader->size))
			{
				return PX_FALSE;
			}
		}
	}
	if (plive->realtime.axisCount)
	{
		px_int axisIndex;
		if (plive->realtime.axisCount > PX_LIVE_REALTIME_MAX_AXES ||
			!PX_ExportLive_AddBytes(&bytes,64ull + (px_uint64)plive->realtime.axisCount * PX_LIVE_RT30_TRAILER_AXIS_ENTRY_SIZE))
		{
			return PX_FALSE;
		}
		for (axisIndex = 0; axisIndex < plive->realtime.axisCount; axisIndex++)
		{
			const PX_LiveRealtimeAxis *axis = &plive->realtime.axes[axisIndex];
			if (!PX_ExportLive_AddBytes(&bytes,
				(px_uint64)axis->bindingCount * PX_LIVE_RT30_TRAILER_BINDING_ENTRY_SIZE +
				(px_uint64)axis->vertexIndexCount * 2ull +
				(px_uint64)axis->sampleBytes + 16ull))
			{
				return PX_FALSE;
			}
		}
	}
	if (!PX_ExportLive_AddBytes(&bytes,65536))
	{
		return PX_FALSE;
	}
	*allocOut = PX_ExportLive_Pow2AtLeast(bytes);
	return *allocOut != 0;
}

static px_int PX_ExportLive_BuildSavePaths(PX_Object *explorer,px_char *finalPath,px_char *tmpPath,px_char *bakPath,px_int cap)
{
	PX_Object_Explorer *pExp;
	const px_char *dir;
	const px_char *file;
	px_int dirLen;
	px_int fileLen;
	px_int i;
	pExp = PX_Object_GetExplorer(explorer);
	if (!pExp || cap <= 8)
	{
		return 1;
	}
	dir = PX_Object_EditGetText(pExp->edit_Path);
	file = PX_Object_EditGetText(pExp->edit_FileName);
	if (!dir || !file || !dir[0] || !file[0] || PX_strequ(file,".") || PX_strequ(file,".."))
	{
		return 1;
	}
	fileLen = PX_strlen(file);
	for (i = 0; i < fileLen; i++)
	{
		if (file[i] == '\\' || file[i] == '/' || file[i] == ':' || file[i] == '*' ||
			file[i] == '?' || file[i] == '"' || file[i] == '<' || file[i] == '>' || file[i] == '|')
		{
			return 1;
		}
	}
	dirLen = PX_strlen(dir);
	if (dirLen <= 0 || dirLen >= cap)
	{
		return dirLen <= 0 ? 1 : 2;
	}
	PX_strcpy(finalPath,dir,cap);
	if (finalPath[dirLen - 1] != '\\' && finalPath[dirLen - 1] != '/')
	{
		if (dirLen + 1 >= cap)
		{
			return 2;
		}
		PX_strcat(finalPath,"\\");
		dirLen++;
	}
	if (dirLen + fileLen >= cap)
	{
		return 2;
	}
	PX_strcat(finalPath,file);
	if (fileLen < 5 || !PX_strequ2(file + fileLen - 5,".live"))
	{
		if (PX_strlen(finalPath) + 5 >= cap)
		{
			return 2;
		}
		PX_strcat(finalPath,".live");
	}
	if (PX_strlen(finalPath) + 4 >= cap)
	{
		return 2;
	}
	PX_strcpy(tmpPath,finalPath,cap);
	PX_strcat(tmpPath,".tmp");
	PX_strcpy(bakPath,finalPath,cap);
	PX_strcat(bakPath,".bak");
	return 0;
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
	MP_NoCatchError(&pool);
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
	px_uint allocSize;
	px_uint poolBytes;
	px_byte *block;
	px_byte *buffer;
	px_memorypool mp;
	px_memory data;
	*blockOut = PX_NULL;
	if (!PX_ExportLive_EstimateAlloc(plive,&allocSize))
	{
		return PX_FALSE;
	}
	poolBytes = allocSize + 1024u;
	block = (px_byte *)malloc(poolBytes);
	if (!block)
	{
		return PX_FALSE;
	}
	mp = MP_Create(block,poolBytes);
	MP_NoCatchError(&mp);
	PX_MemoryInitialize(&mp,&data);
	buffer = (px_byte *)MP_Malloc(&mp,allocSize);
	if (!buffer)
	{
		free(block);
		return PX_FALSE;
	}
	data.buffer = buffer;
	data.allocsize = (px_int)allocSize;
	data.usedsize = 0;
	if (PX_LiveFrameworkExport(plive,&data) && data.buffer && data.usedsize > 0)
	{
		*blockOut = block;
		*dataOut = data;
		return PX_TRUE;
	}
	free(block);
	return PX_FALSE;
}

px_bool PX_LiveEditorModule_ExportLiveCapture(PX_LiveFramework *plive,px_byte **blockOut,px_memory *dataOut)
{
	return PX_ExportLive_WriteFramework(plive,blockOut,dataOut);
}

px_void PX_Application_OnExportLive_OnConfirm(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	px_char finalPath[260];
	px_char tmpPath[260];
	px_char bakPath[260];
	px_int pathStatus;
	px_byte *block = PX_NULL;
	px_memory data;
	PX_IO_Data written;
	PX_Object *pExportLiveObject = (PX_Object *)ptr;
	PX_LiveEditorModule_ExportLive *pExportLive = (PX_LiveEditorModule_ExportLive *)pExportLiveObject->pObjectDesc[0];
	(void)pObject;
	(void)e;

	PX_memset(finalPath,0,sizeof(finalPath));
	PX_memset(tmpPath,0,sizeof(tmpPath));
	PX_memset(bakPath,0,sizeof(bakPath));
	pathStatus = PX_ExportLive_BuildSavePaths(pExportLive->explorer,finalPath,tmpPath,bakPath,(px_int)sizeof(finalPath));
	if (pathStatus == 2)
	{
		PX_ExportLive_Report(pExportLiveObject,pExportLive->pLanguageJson,"exportlive.path too long",finalPath);
		return;
	}
	if (pathStatus != 0)
	{
		PX_ExportLive_Report(pExportLiveObject,pExportLive->pLanguageJson,"exportlive.invalid name",finalPath);
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
