#include "PainterEngine.h"
#include <stdlib.h>

#include "PainterEngine_LiveEditorModules_Menu.h"
#include "PainterEngine_LiveEditorModules_CreateProject.h"
#include "PainterEngine_LiveEditorModules_ImportImage.h"
#include "PainterEngine_LiveEditorModules_ImportPsd.h"
#include "PainterEngine_LiveEditorModules_LiveController.h"
#include "PainterEngine_LiveEditorModules_RealtimeController.h"
#include "PainterEngine_LiveEditorModules_RealtimeAnimationController.h"
#include "PainterEngine_LiveEditorModules_RealtimeBlendPreview.h"
#include "PainterEngine_LiveEditorModules_Display.h"
#include "PainterEngine_LiveEditorModules_Translation.h"
#include "PainterEngine_LiveEditorModules_BuildMesh.h"
#include "PainterEngine_LiveEditorModules_Setkey.h"
#include "PainterEngine_LiveEditorModules_Linkkey.h"
#include "PainterEngine_LiveEditorModules_AnimationController.h"
#include "PainterEngine_LiveEditorModules_FrameController.h"
#include "PainterEngine_LiveEditorModules_TimeStampEditor.h"
#include "PainterEngine_LiveEditorModules_NewFrameEditor.h"
#include "PainterEngine_LiveEditorModules_KeyTransform.h"
#include "PainterEngine_LiveEditorModules_VertexTransform.h"
#include "PainterEngine_LiveEditorModules_FrameLiveTextureEditor.h"
#include "PainterEngine_LiveEditorModules_Importlive.h"
#include "PainterEngine_LiveEditorModules_Exportlive.h"
#include "PainterEngine_LiveEditorModules_GlobalRotation.h"
#include "PainterEngine_LiveEditorModules_ImpulseEditor.h"
#include "PainterEngine_LiveEditorModules_Panc.h"
#ifdef _WIN32
#include <windows.h>
#endif
typedef struct
{
	//////////////////////////////////////////////////////////////////////////
	//modules
	//////////////////////////////////////////////////////////////////////////
	//global
	PX_Object* module_messagebox;
	//others
	PX_Object* module_display;
	PX_Object* module_menu;
	PX_Object* module_createproject;
	PX_Object* module_importimage;
	PX_Object* module_importpsd;
	PX_Object* module_importlive;
	PX_Object* module_exportlive;
	PX_Object* module_livecontroller;
	PX_Object* module_realtimecontroller;
	PX_Object* module_realtimeanimationcontroller;
	PX_Object* module_translation;
	PX_Object* module_buildmesh;
	PX_Object* module_setkey;
	PX_Object* module_linkkey;
	PX_Object* module_animationcontroller;
	PX_Object* module_framecontroller;
	PX_Object* module_timestampeditor;
	PX_Object* module_translationeditor;
	PX_Object* module_newframeeditor;
	PX_Object* module_keytransform;
	PX_Object* module_vertextransform;
	PX_Object* module_framelivetextureeditor;
	PX_Object* module_globalrotation;
	PX_Object* module_impulse;
	PX_Object* module_panc;
	//////////////////////////////////////////////////////////////////////////
	//
	PX_Object* root;
	PX_Object* explorer;
	PX_Object* messagebox;

	PX_Json languageJson;
	PX_LiveFramework liveFramework;
	PX_FontModule fontmodule;
	PX_Runtime *pruntime;
}PX_Object_LiveEditor;


static px_void PX_Object_LiveEditorSwitchEditMode(PX_Object_LiveEditor* pApp)
{
	PX_LiveEditorModule_MenuEditMode(pApp->module_menu);

	PX_LiveEditorModule_DisplayEnable(pApp->module_display);
	PX_LiveEditorModule_DisplayResetPosition(pApp->module_display);

	PX_LiveEditorModule_LiveControllerEnable(pApp->module_livecontroller);
	PX_LiveEditorModule_TranslationEnable(pApp->module_translation);
}


px_void PX_Object_LiveEditorOnScheduleEvent_CreateProject(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_LiveEditorModule_CreateProjectEnable(pApp->module_createproject);
}

px_void PX_Object_LiveEditorOnScheduleEvent_Import(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_LiveEditorModule_ImportLiveEnable(pApp->module_importlive);
}
px_void PX_Object_LiveEditorOnScheduleEvent_ImportLive_exit(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_LiveEditorModule_ImportLiveDisable(pApp->module_importlive);
}

px_void PX_Object_LiveEditorOnScheduleEvent_ImportLive_confirm(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_LiveEditorModule_ImportLiveDisable(pApp->module_importlive);
	PX_LiveEditorModule_RealtimeControllerReload(pApp->module_realtimecontroller);
	PX_Object_LiveEditorSwitchEditMode(pApp);
}


px_void PX_Object_LiveEditorOnScheduleEvent_Export(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_LiveEditorModule_ExportLiveEnable(pApp->module_exportlive);
}

px_void PX_Object_LiveEditorOnScheduleEvent_ExportLive_exit(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_LiveEditorModule_ExportLiveDisable(pApp->module_exportlive);
}

px_void PX_Object_LiveEditorOnScheduleEvent_CreateProject_exit(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_LiveEditorModule_CreateProjectDisable(pApp->module_createproject);
}

px_void PX_Object_LiveEditorOnScheduleEvent_CreateProject_confirm(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_LiveEditorModule_RealtimeControllerReload(pApp->module_realtimecontroller);
	PX_Object_LiveEditorSwitchEditMode(pApp);
	PX_LiveEditorModule_CreateProjectDisable(pApp->module_createproject);
}



px_void PX_Object_LiveEditorOnScheduleEvent_LoadImage(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_LiveEditorModule_ImportImageEnable(pApp->module_importimage);
}

px_void PX_Object_LiveEditorOnScheduleEvent_LoadPsd(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_LiveEditorModule_ImportPsdEnable(pApp->module_importpsd);
}

px_void PX_Object_LiveEditorOnScheduleEvent_ImportPsd_exit(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_LiveEditorModule_ImportPsdDisable(pApp->module_importpsd);
}

px_void PX_Object_LiveEditorOnScheduleEvent_Message(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_Object_MessageBoxAlertOk(pApp->module_messagebox, PX_Object_Event_GetStringPtr(e), PX_NULL, PX_NULL);
}

px_void PX_Object_LiveEditorOnScheduleEvent_MenuLayer(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	if (pApp->module_livecontroller->Enabled)
	{
		PX_LiveEditorModule_LiveControllerDisable(pApp->module_livecontroller);
	}
	else
	{
		PX_LiveEditorModule_LiveControllerEnable(pApp->module_livecontroller);
	}
}

static px_int PX_Object_LiveEditorSnapshotChecksum(const px_byte *data,px_int size)
{
	px_dword hash=2166136261u;
	px_int i;
	if (!data||size<=0)
	{
		return 0;
	}
	for (i=0;i<size;i++)
	{
		hash^=data[i];
		hash*=16777619u;
	}
	return (px_int)hash;
}

static px_bool PX_Object_GetRealtimePreviewChecksum(px_int *checksum)
{
#ifdef _WIN32
	const px_char *flag="--rt30-sum=",*commandLine=GetCommandLineA(),*begin;
	px_char *end=PX_NULL;
	if (!checksum) return PX_FALSE;
	begin=strstr(commandLine,flag);
	if (!begin) return PX_FALSE;
	begin+=PX_strlen(flag);
	*checksum=(px_int)strtol(begin,&end,10);
	return end!=begin;
#else
	return PX_FALSE;
#endif
}

static px_bool PX_Object_LiveEditorLaunchRealtimePreview(PX_Object_LiveEditor *pApp)
{
#ifdef _WIN32
	px_byte *cacheBuffer=PX_NULL;
	px_memorypool snapshotPool;
	px_memory snapshot;
	px_char tempFolder[MAX_PATH],snapshotPath[MAX_PATH],modulePath[MAX_PATH],commandLine[MAX_PATH*3];
	STARTUPINFOA startupInfo;
	PROCESS_INFORMATION processInfo;
	px_bool result=PX_FALSE;
	cacheBuffer=(px_byte *)malloc(64*1024*1024);
	if (!cacheBuffer) return PX_FALSE;
	snapshotPool=MP_Create(cacheBuffer,64*1024*1024);
	PX_MemoryInitialize(&snapshotPool,&snapshot);
	if (!PX_LiveFrameworkExport(&pApp->liveFramework,&snapshot)) goto cleanup;
	if (!GetTempPathA(MAX_PATH,tempFolder)) goto cleanup;
	if (!GetTempFileNameA(tempFolder,"PLE",0,snapshotPath)) goto cleanup;
	if (!PX_SaveDataToFile(snapshot.buffer,snapshot.usedsize,snapshotPath))
	{
		PX_FileDelete(snapshotPath);
		goto cleanup;
	}
	if (!GetModuleFileNameA(PX_NULL,modulePath,MAX_PATH))
	{
		PX_FileDelete(snapshotPath);
		goto cleanup;
	}
	PX_sprintf3(commandLine,sizeof(commandLine),"\"%1\" --rt30-preview=\"%2\" --rt30-sum=%3",PX_STRINGFORMAT_STRING(modulePath),PX_STRINGFORMAT_STRING(snapshotPath),PX_STRINGFORMAT_INT((px_int)PX_Object_LiveEditorSnapshotChecksum(snapshot.buffer,snapshot.usedsize)));
	PX_memset(&startupInfo,0,sizeof(startupInfo));
	PX_memset(&processInfo,0,sizeof(processInfo));
	startupInfo.cb=sizeof(startupInfo);
	if (!CreateProcessA(modulePath,commandLine,PX_NULL,PX_NULL,PX_FALSE,0,PX_NULL,PX_NULL,&startupInfo,&processInfo))
	{
		PX_FileDelete(snapshotPath);
		goto cleanup;
	}
	CloseHandle(processInfo.hThread);
	CloseHandle(processInfo.hProcess);
	result=PX_TRUE;
cleanup:
	free(cacheBuffer);
	return result;
#else
	return PX_FALSE;
#endif
}

px_void PX_Object_LiveEditorOnScheduleEvent_RealtimePreview(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_LiveEditorRealtimePoseAccessor *accessor=PX_LiveEditorModule_RealtimeControllerGetPoseAccessor(pApp->module_realtimecontroller);
	if (accessor&&PX_LiveEditorRealtimePoseAccessorHasUnbakedEdits(accessor))
	{
		PX_Object_MessageBoxAlertOk(pApp->module_messagebox,PX_JsonGetString(&pApp->languageJson,"realtime.preview uses baked"),PX_NULL,PX_NULL);
	}
	if (!PX_Object_LiveEditorLaunchRealtimePreview(pApp))
	{
		PX_Object_MessageBoxAlertOk(pApp->module_messagebox,PX_JsonGetString(&pApp->languageJson,"realtime.preview launch failed"),PX_NULL,PX_NULL);
	}
}
px_void PX_Object_LiveEditorOnScheduleEvent_ImportImage_exit(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_LiveEditorModule_ImportImageDisable(pApp->module_importimage);
}

px_void PX_Object_LiveEditorOnScheduleEvent_BuildMesh(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	px_int index = PX_LiveEditorModule_LiveControllerGetCurrentEditLayer(pApp->module_livecontroller);
	if (index >= 0 && index < pApp->liveFramework.layers.size)
	{
		PX_LiveEditorModule_LiveControllerDisable(pApp->module_livecontroller);
		PX_LiveEditorModule_BuildMeshEnable(pApp->module_buildmesh);
	}
}

px_void PX_Object_LiveEditorOnScheduleEvent_BuildMeshExit(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_LiveEditorModule_LiveControllerEnable(pApp->module_livecontroller);
	PX_LiveEditorModule_BuildMeshDisable(pApp->module_buildmesh);
}

px_void PX_Object_LiveEditorOnScheduleEvent_SetKey(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	px_int index = PX_LiveEditorModule_LiveControllerGetCurrentEditLayer(pApp->module_livecontroller);
	if (index >= 0 && index < pApp->liveFramework.layers.size)
	{
		PX_LiveEditorModule_LiveControllerDisable(pApp->module_livecontroller);
		PX_LiveEditorModule_SetKeyEnable(pApp->module_setkey);
	}
}

px_void PX_Object_LiveEditorOnScheduleEvent_SetKeyExit(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_LiveEditorModule_LiveControllerEnable(pApp->module_livecontroller);
	PX_LiveEditorModule_SetKeyDisable(pApp->module_setkey);
}

px_void PX_Object_LiveEditorOnScheduleEvent_LinkKey(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{

	PX_LiveEditorModule_LiveControllerDisable(pApp->module_livecontroller);
	PX_LiveEditorModule_LinkkeyEnable(pApp->module_linkkey);

}

px_void PX_Object_LiveEditorOnScheduleEvent_LinkKeyExit(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_LiveEditorModule_LiveControllerEnable(pApp->module_livecontroller);
	PX_LiveEditorModule_LinkkeyDisable(pApp->module_linkkey);
}
//////////////////////////////////////////////////////////////////////////
//Animation Controller actions

static px_void PX_Object_LiveEditorOnScheduleEvent_DisableAllAnimationEditorControllers(PX_Object_LiveEditor* pApp)
{
	PX_LiveEditorModule_TimeStampEditorDisable(pApp->module_timestampeditor);
	PX_LiveEditorModule_KeyTransformDisable(pApp->module_keytransform);
	PX_LiveEditorModule_GlobalRotationDisable(pApp->module_globalrotation);
	PX_LiveEditorModule_VertexTransformDisable(pApp->module_vertextransform);
	PX_LiveEditorModule_ImpulseEditorDisable(pApp->module_impulse);
	PX_LiveEditorModule_FrameLiveTextureEditorDisable(pApp->module_framelivetextureeditor);
	PX_LiveEditorModule_PancDisable(pApp->module_panc);
}

px_void PX_Object_LiveEditorOnScheduleEvent_LiveControllerStateChanged(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_LiveEditorModule_LiveController* pController = (PX_LiveEditorModule_LiveController*)pApp->module_livecontroller->pObjectDesc[0];

	PX_Object_LiveEditorOnScheduleEvent_DisableAllAnimationEditorControllers(pApp);

	if (pController->state == PX_LIVEEDITORMODULE_LIVECONTROLLER_STATE_REALTIME)
	{
		PX_LiveEditorModule_AnimationControllerDisable(pApp->module_animationcontroller);
		PX_LiveEditorModule_FrameControllerDisable(pApp->module_framecontroller);
		PX_LiveEditorModule_TranslationDisable(pApp->module_translation);
		PX_LiveEditorModule_RealtimeControllerEnable(pApp->module_realtimecontroller);
		PX_LiveEditorModule_RealtimeAnimationControllerEnable(pApp->module_realtimeanimationcontroller);
	}
	else if (pController->state == PX_LIVEEDITORMODULE_LIVECONTROLLER_STATE_ANIMATION)
	{
		PX_LiveEditorModule_RealtimeControllerDisable(pApp->module_realtimecontroller);
		PX_LiveEditorModule_RealtimeAnimationControllerDisable(pApp->module_realtimeanimationcontroller);
		PX_LiveEditorModule_AnimationControllerEnable(pApp->module_animationcontroller);
		PX_LiveEditorModule_FrameControllerEnable(pApp->module_framecontroller);
		PX_LiveEditorModule_TranslationDisable(pApp->module_translation);

	}
	else
	{
		PX_LiveEditorModule_RealtimeControllerDisable(pApp->module_realtimecontroller);
		PX_LiveEditorModule_RealtimeAnimationControllerDisable(pApp->module_realtimeanimationcontroller);
		PX_LiveEditorModule_AnimationControllerDisable(pApp->module_animationcontroller);
		PX_LiveEditorModule_FrameControllerDisable(pApp->module_framecontroller);
		PX_LiveEditorModule_TranslationEnable(pApp->module_translation);
	}

}

px_void PX_Object_LiveEditorOnScheduleEvent_NewFrame(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_LiveEditorModule_NewFrameEditorEnable(pApp->module_newframeeditor, PX_FALSE);
}

px_void PX_Object_LiveEditorOnScheduleEvent_CopyFrame(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_LiveEditorModule_NewFrameEditorEnable(pApp->module_newframeeditor, PX_TRUE);
}

px_void PX_Object_LiveEditorOnScheduleEvent_NewFrameEditorExit(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_LiveEditorModule_NewFrameEditorDisable(pApp->module_newframeeditor);
}




//////////////////////////////////////////////////////////////////////////
//TimeStampEditor

//////////////////////////////////////////////////////////////////////////
//play / stop
px_void PX_Object_LiveEditorOnScheduleEvent_AnimationControllerPlayStop(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_Object_LiveEditorOnScheduleEvent_DisableAllAnimationEditorControllers(pApp);
	PX_LiveEditorModule_AnimationControllerFocusButton(pApp->module_animationcontroller, PX_LIVEEDITORMODULE_ANIMATIONCONTROLLER_FOCUS_NONE);
}
//////////////////////////////////////////////////////////////////////////
//time stamp
px_void PX_Object_LiveEditorOnScheduleEvent_AnimationControllerTimeStamp(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_Object_LiveEditorOnScheduleEvent_DisableAllAnimationEditorControllers(pApp);
	PX_LiveEditorModule_TimeStampEditorEnable(pApp->module_timestampeditor);
	PX_LiveEditorModule_AnimationControllerFocusButton(pApp->module_animationcontroller, PX_LIVEEDITORMODULE_ANIMATIONCONTROLLER_FOCUS_TIMESTAMP);
}

px_void PX_Object_LiveEditorOnScheduleEvent_TimeStampEditorExit(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_Object_LiveEditorOnScheduleEvent_DisableAllAnimationEditorControllers(pApp);
	PX_LiveEditorModule_AnimationControllerFocusButton(pApp->module_animationcontroller, PX_LIVEEDITORMODULE_ANIMATIONCONTROLLER_FOCUS_NONE);
}


//////////////////////////////////////////////////////////////////////////
//smart
px_void PX_Object_LiveEditorOnScheduleEvent_SmartTransform(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_Object_LiveEditorOnScheduleEvent_DisableAllAnimationEditorControllers(pApp);
	PX_LiveEditorModule_KeyTransformEnable(pApp->module_keytransform, PX_LIVEEDITORMODULE_KEYTRANSFORM_MODE_TRANSLATION);
	PX_LiveEditorModule_AnimationControllerFocusButton(pApp->module_animationcontroller, PX_LIVEEDITORMODULE_ANIMATIONCONTROLLER_FOCUS_SMART);
}


//////////////////////////////////////////////////////////////////////////
//global rotation
px_void PX_Object_LiveEditorOnScheduleEvent_GlobalRotation(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_Object_LiveEditorOnScheduleEvent_DisableAllAnimationEditorControllers(pApp);
	PX_LiveEditorModule_GlobalRotationEnable(pApp->module_globalrotation);
	PX_LiveEditorModule_AnimationControllerFocusButton(pApp->module_animationcontroller, PX_LIVEEDITORMODULE_ANIMATIONCONTROLLER_FOCUS_GLOBALROTATION);
}


//////////////////////////////////////////////////////////////////////////
//stretch
px_void PX_Object_LiveEditorOnScheduleEvent_Stretch(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_Object_LiveEditorOnScheduleEvent_DisableAllAnimationEditorControllers(pApp);
	PX_LiveEditorModule_KeyTransformEnable(pApp->module_keytransform, PX_LIVEEDITORMODULE_KEYTRANSFORM_MODE_STRETCH);
	PX_LiveEditorModule_AnimationControllerFocusButton(pApp->module_animationcontroller, PX_LIVEEDITORMODULE_ANIMATIONCONTROLLER_FOCUS_STRETCH);
}


//////////////////////////////////////////////////////////////////////////
//rotation
px_void PX_Object_LiveEditorOnScheduleEvent_Rotation(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_Object_LiveEditorOnScheduleEvent_DisableAllAnimationEditorControllers(pApp);
	PX_LiveEditorModule_KeyTransformEnable(pApp->module_keytransform, PX_LIVEEDITORMODULE_KEYTRANSFORM_MODE_ROTATION);
	PX_LiveEditorModule_AnimationControllerFocusButton(pApp->module_animationcontroller, PX_LIVEEDITORMODULE_ANIMATIONCONTROLLER_FOCUS_ROTATION);
}


//////////////////////////////////////////////////////////////////////////
//impulse
px_void PX_Object_LiveEditorOnScheduleEvent_Impulse(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_Object_LiveEditorOnScheduleEvent_DisableAllAnimationEditorControllers(pApp);
	PX_LiveEditorModule_ImpulseEditorEnable(pApp->module_impulse);
	PX_LiveEditorModule_AnimationControllerFocusButton(pApp->module_animationcontroller, PX_LIVEEDITORMODULE_ANIMATIONCONTROLLER_FOCUS_IMPULSE);
}



//////////////////////////////////////////////////////////////////////////
//vertices

px_void PX_Object_LiveEditorOnScheduleEvent_VertexTransfom(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_Object_LiveEditorOnScheduleEvent_DisableAllAnimationEditorControllers(pApp);
	PX_LiveEditorModule_VertexTransformEnable(pApp->module_vertextransform);
	PX_LiveEditorModule_AnimationControllerFocusButton(pApp->module_animationcontroller, PX_LIVEEDITORMODULE_ANIMATIONCONTROLLER_FOCUS_VERTICES);
}


//////////////////////////////////////////////////////////////////////////
//texture

px_void PX_Object_LiveEditorOnScheduleEvent_FrameLiveTextureEditor(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_Object_LiveEditorOnScheduleEvent_DisableAllAnimationEditorControllers(pApp);
	PX_LiveEditorModule_FrameLiveTextureEditorEnable(pApp->module_framelivetextureeditor);
	PX_LiveEditorModule_AnimationControllerFocusButton(pApp->module_animationcontroller, PX_LIVEEDITORMODULE_ANIMATIONCONTROLLER_FOCUS_SWITCHTEXTURE);
}


//////////////////////////////////////////////////////////////////////////
//Panc

px_void PX_Object_LiveEditorOnScheduleEvent_Panc(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_Object_LiveEditorOnScheduleEvent_DisableAllAnimationEditorControllers(pApp);
	PX_LiveEditorModule_PancEnable(pApp->module_panc);
	PX_LiveEditorModule_AnimationControllerFocusButton(pApp->module_animationcontroller, PX_LIVEEDITORMODULE_ANIMATIONCONTROLLER_FOCUS_PANC);
}

px_void PX_Object_LiveEditorOnScheduleEvent_PancExit(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_Object_LiveEditorOnScheduleEvent_DisableAllAnimationEditorControllers(pApp);
	PX_LiveEditorModule_AnimationControllerFocusButton(pApp->module_animationcontroller, PX_LIVEEDITORMODULE_ANIMATIONCONTROLLER_FOCUS_NONE);
}
//////////////////////////////////////////////////////////////////////////
//showfps
px_void PX_Object_LiveEditorOnScheduleEvent_ShowFPS(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_LiveEditorModule_DisplayShowFPS(pApp->module_display);
}

//////////////////////////////////////////////////////////////////////////
//reset position
px_void PX_Object_LiveEditorOnScheduleEvent_ResetPosition(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_LiveEditorModule_DisplayResetPosition(pApp->module_display);
}


//////////////////////////////////////////////////////////////////////////
//show helper line
px_void PX_Object_LiveEditorOnScheduleEvent_ShowHelperLine(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_LiveEditorModule_DisplayShowHelperLine(pApp->module_display);
}

px_void PX_Object_LiveEditorOnScheduleEvent(PX_Object* pObject, PX_Object_Event e, px_void* ptr)
{
	PX_Object_LiveEditor* pApp = (PX_Object_LiveEditor*)ptr;
	switch (e.Event)
	{
	case PX_LIVEFRAMEWORKMODULES_MENU_EVENT_CREATEPROJECT:
	{
		PX_Object_LiveEditorOnScheduleEvent_CreateProject(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_MENU_EVENT_IMPORT:
	{
		PX_Object_LiveEditorOnScheduleEvent_Import(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_MENU_EVENT_EXPORT:
	{
		PX_Object_LiveEditorOnScheduleEvent_Export(pApp, e, ptr);
	}
	break;
	case PX_LIVEEDITORMODULE_EVENT_CREATEPROJECT_EXIT:
	{
		PX_Object_LiveEditorOnScheduleEvent_CreateProject_exit(pApp, e, ptr);
	}
	break;
	case PX_LIVEEDITORMODULE_EVENT_CREATEPROJECT_CONFIRM:
	{
		PX_Object_LiveEditorOnScheduleEvent_CreateProject_confirm(pApp, e, ptr);
	}
	break;
	case PX_LIVEEDITORMODULE_IMPORTLIVE_EVENT_EXIT:
	{
		PX_Object_LiveEditorOnScheduleEvent_ImportLive_exit(pApp, e, ptr);
	}
	break;
	case PX_LIVEEDITORMODULE_IMPORTLIVE_EVENT_CONFIRM:
	{
		PX_Object_LiveEditorOnScheduleEvent_ImportLive_confirm(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_LIVECONTROLLER_EVENT_LOADIMAGE:
	case PX_LIVEFRAMEWORKMODULES_MENU_EVENT_LOADIMAGE:
	{
		PX_Object_LiveEditorOnScheduleEvent_LoadImage(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_MENU_EVENT_LOADPSD:
	{
		PX_Object_LiveEditorOnScheduleEvent_LoadPsd(pApp, e, ptr);
	}
	break;
	case PX_LIVEEDITORMODULE_IMPORTLIVE_EVENT_MESSAGE:
	case PX_LIVEEDITORMODULE_IMPORTIMAGE_EVENT_MESSAGE:
	case PX_LIVEEDITORMODULE_IMPORTPSD_EVENT_MESSAGE:
	case PX_LIVEEDITORMODULE_EXPORTLIVE_EVENT_MESSAGE:
	case PX_LIVEEDITORMODULE_BUILDMESH_EVENT_MESSAGE:
	case PX_LIVEEDITORMODULE_LIVECONTROLLER_EVENT_MESSAGE:
	case PX_LIVEFRAMEWORKMODULES_MENU_EVENT_MESSAGE:
	{
		PX_Object_LiveEditorOnScheduleEvent_Message(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_MENU_EVENT_WINDOW_LAYER:
	{
		PX_Object_LiveEditorOnScheduleEvent_MenuLayer(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_MENU_EVENT_REALTIME_PREVIEW:
	{
		PX_Object_LiveEditorOnScheduleEvent_RealtimePreview(pApp,e,ptr);
	}
	break;
	case PX_LIVEEDITORMODULE_IMPORTIMAGE_EVENT_EXIT:
	{
		PX_Object_LiveEditorOnScheduleEvent_ImportImage_exit(pApp, e, ptr);
	}
	break;
	case PX_LIVEEDITORMODULE_IMPORTPSD_EVENT_EXIT:
	{
		PX_Object_LiveEditorOnScheduleEvent_ImportPsd_exit(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_LIVECONTROLLER_EVENT_BUILDMESH:
	{
		PX_Object_LiveEditorOnScheduleEvent_BuildMesh(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_LIVECONTROLLER_EVENT_LINKKEY:
	{
		PX_Object_LiveEditorOnScheduleEvent_LinkKey(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_BUILDMESH_EVENT_EXIT:
	{
		PX_Object_LiveEditorOnScheduleEvent_BuildMeshExit(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_LIVECONTROLLER_EVENT_SETKEY:
	{
		PX_Object_LiveEditorOnScheduleEvent_SetKey(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_SETKEY_EVENT_EXIT:
	{
		PX_Object_LiveEditorOnScheduleEvent_SetKeyExit(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_LINKKEY_EVENT_EXIT:
	{
		PX_Object_LiveEditorOnScheduleEvent_LinkKeyExit(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_LIVECONTROLLER_EVENT_STATECHANGE:
	{
		PX_Object_LiveEditorOnScheduleEvent_LiveControllerStateChanged(pApp, e, ptr);
	}
	break;
	case PX_FRAMECONTROLLERMODULE_EVENT_NEWFRAME:
	{
		PX_Object_LiveEditorOnScheduleEvent_NewFrame(pApp, e, ptr);
	}
	break;
	case PX_FRAMECONTROLLERMODULE_EVENT_COPYFRAME:
	{
		PX_Object_LiveEditorOnScheduleEvent_CopyFrame(pApp, e, ptr);
	}
	break;
	case PX_LIVEEDITORMODULE_NEWFRAMEEDITOR_EVENT_EXIT:
	{
		PX_Object_LiveEditorOnScheduleEvent_NewFrameEditorExit(pApp, e, ptr);
	}
	break;
	//////////////////////////////////////////////////////////////////////////
	case PX_LIVEFRAMEWORKMODULES_ANIMATIONCONTROLLER_EVENT_PLAY:
	{
		PX_Object_LiveEditorOnScheduleEvent_AnimationControllerPlayStop(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_ANIMATIONCONTROLLER_EVENT_STOP:
	{
		PX_Object_LiveEditorOnScheduleEvent_AnimationControllerPlayStop(pApp, e, ptr);
	}
	break;

	case PX_LIVEFRAMEWORKMODULES_ANIMATIONCONTROLLER_EVENT_CLOCK:
	{
		PX_Object_LiveEditorOnScheduleEvent_AnimationControllerTimeStamp(pApp, e, ptr);
	}
	break;
	case PX_LIVEEDITORMODULE_TIMESTAMPEDITOR_EVENT_EXIT:
	{
		PX_Object_LiveEditorOnScheduleEvent_TimeStampEditorExit(pApp, e, ptr);
	}
	break;

	case PX_LIVEFRAMEWORKMODULES_ANIMATIONCONTROLLER_EVENT_SMARTTRANSFORM:
	{
		PX_Object_LiveEditorOnScheduleEvent_SmartTransform(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_ANIMATIONCONTROLLER_EVENT_STRETCH:
	{
		PX_Object_LiveEditorOnScheduleEvent_Stretch(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_ANIMATIONCONTROLLER_EVENT_ROTATION:
	{
		PX_Object_LiveEditorOnScheduleEvent_Rotation(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_ANIMATIONCONTROLLER_EVENT_VERTEXTRANSFORM:
	{
		PX_Object_LiveEditorOnScheduleEvent_VertexTransfom(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_ANIMATIONCONTROLLER_EVENT_SWITCHTEXTURE:
	{
		PX_Object_LiveEditorOnScheduleEvent_FrameLiveTextureEditor(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_ANIMATIONCONTROLLER_EVENT_GLOBALROTATION:
	{
		PX_Object_LiveEditorOnScheduleEvent_GlobalRotation(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_ANIMATIONCONTROLLER_EVENT_IMPULSE:
	{
		PX_Object_LiveEditorOnScheduleEvent_Impulse(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_ANIMATIONCONTROLLER_EVENT_PANC:
	{
		PX_Object_LiveEditorOnScheduleEvent_Panc(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_PANC_EVENT_EXIT:
	{
		PX_Object_LiveEditorOnScheduleEvent_PancExit(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_MENU_EVENT_SHOWFPS:
	{
		PX_Object_LiveEditorOnScheduleEvent_ShowFPS(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_MENU_EVENT_RESETPOSITION:
	{
		PX_Object_LiveEditorOnScheduleEvent_ResetPosition(pApp, e, ptr);
	}
	break;
	case PX_LIVEFRAMEWORKMODULES_MENU_EVENT_SHOWHELPERLINE:
	{
		PX_Object_LiveEditorOnScheduleEvent_ShowHelperLine(pApp, e, ptr);
	}
	break;
	}

}

PX_Object* PX_Object_LiveEditorCreate(PX_Runtime* pruntime, PX_Object* parent)
{
	PX_Object_LiveEditor* pApp;
	PX_Object* pObject;
	PX_LiveEditorModule_LiveController *pLiveController;
	pObject = PX_ObjectCreateEx(mp, parent, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, sizeof(PX_Object_LiveEditor));
	if (pObject == PX_NULL)
	{
		return PX_NULL;
	}
	pApp = (PX_Object_LiveEditor*)pObject->pObjectDesc[0];

	if (!PX_FontModuleInitialize(mp_static, &pApp->fontmodule))goto _ERROR;
	if (!PX_LoadFontModuleFromFile(&pApp->fontmodule, "assets/font18.pxf"))goto _ERROR;
	if (!PX_LiveFrameworkInitialize(mp, &pApp->liveFramework, 0, 0)) goto _ERROR;

	if (!PX_JsonInitialize(mp_static, &pApp->languageJson))goto _ERROR;
	if (!PX_LoadJsonFromFile(&pApp->languageJson, "assets/language.json")) goto _ERROR;

	//Modules Initialize
	PX_ObjectRegisterEvent(pObject, PX_OBJECT_EVENT_ANY, PX_Object_LiveEditorOnScheduleEvent, pApp);

	//////////////////////////////////////////////////////////////////////////
	//static modules
	if ((pApp->module_display = PX_LiveEditorModule_DisplayInstall(pObject, pruntime, &pApp->fontmodule, &pApp->liveFramework, &pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_DisplayDisable(pApp->module_display);

	if ((pApp->module_menu = PX_LiveEditorModule_MenuInstall(pObject, pruntime, &pApp->fontmodule, &pApp->liveFramework, &pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_MenuStandbyMode(pApp->module_menu);

	//////////////////////////////////////////////////////////////////////////
	//temporary
	if ((pApp->module_translation = PX_LiveEditorModule_TranslationInstall(pObject, pruntime, &pApp->fontmodule, &pApp->liveFramework, &pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_TranslationDisable(pApp->module_translation);

	if ((pApp->module_buildmesh = PX_LiveEditorModule_BuildMeshInstall(pObject, pruntime, &pApp->fontmodule, &pApp->liveFramework, &pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_BuildMeshDisable(pApp->module_buildmesh);

	if ((pApp->module_setkey = PX_LiveEditorModule_SetKeyInstall(pObject, pruntime, &pApp->fontmodule, &pApp->liveFramework, &pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_SetKeyDisable(pApp->module_setkey);

	if ((pApp->module_linkkey = PX_LiveEditorModule_LinkkeyInstall(pObject, pruntime, &pApp->fontmodule, &pApp->liveFramework, &pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_LinkkeyDisable(pApp->module_linkkey);


	//////////////////////////////////////////////////////////////////////////
	//auto adapt editor modules

	//static
	if ((pApp->module_animationcontroller = PX_LiveEditorModule_AnimationControllerInstall(pObject, pruntime, &pApp->fontmodule, &pApp->liveFramework, &pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_AnimationControllerDisable(pApp->module_animationcontroller);

	if ((pApp->module_livecontroller = PX_LiveEditorModule_LiveControllerInstall(pObject, pruntime, &pApp->fontmodule, &pApp->liveFramework, &pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_LiveControllerDisable(pApp->module_livecontroller);
	pLiveController=(PX_LiveEditorModule_LiveController *)pApp->module_livecontroller->pObjectDesc[0];

	if ((pApp->module_realtimecontroller = PX_LiveEditorModule_RealtimeControllerInstall(pLiveController->window_widget,pObject,pruntime,&pApp->fontmodule,&pApp->liveFramework,&pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_RealtimeControllerDisable(pApp->module_realtimecontroller);
	PX_LiveEditorModule_LiveControllerSetRealtimeController(pApp->module_livecontroller,pApp->module_realtimecontroller);
	if ((pApp->module_realtimeanimationcontroller = PX_LiveEditorModule_RealtimeAnimationControllerInstall(pObject,pruntime,&pApp->fontmodule,PX_LiveEditorModule_RealtimeControllerGetPoseAccessor(pApp->module_realtimecontroller),&pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_RealtimeAnimationControllerDisable(pApp->module_realtimeanimationcontroller);

	if ((pApp->module_framecontroller = PX_LiveEditorModule_FrameControllerInstall(pObject, pruntime, &pApp->fontmodule, &pApp->liveFramework, &pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_FrameControllerDisable(pApp->module_framecontroller);


	//////////////////////////////////////////////////////////////////////////
	//on focus editor modules

	if ((pApp->module_createproject = PX_LiveEditorModule_CreateProjectInstall(pObject, pruntime, &pApp->fontmodule, &pApp->liveFramework, &pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_CreateProjectDisable(pApp->module_createproject);

	if ((pApp->module_importimage = PX_LiveEditorModule_ImportImageInstall(pObject, pruntime, &pApp->fontmodule, &pApp->liveFramework, &pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_ImportImageDisable(pApp->module_importimage);

	if ((pApp->module_importpsd = PX_LiveEditorModule_ImportPsdInstall(pObject, pruntime, &pApp->fontmodule, &pApp->liveFramework, &pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_ImportPsdDisable(pApp->module_importpsd);

	if ((pApp->module_importlive = PX_LiveEditorModule_ImportLiveInstall(pObject, pruntime, &pApp->fontmodule, &pApp->liveFramework, &pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_ImportLiveDisable(pApp->module_importlive);

	if ((pApp->module_exportlive = PX_LiveEditorModule_ExportLiveInstall(pObject, pruntime, &pApp->fontmodule, &pApp->liveFramework, &pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_ExportLiveSetPoseAccessor(pApp->module_exportlive,PX_LiveEditorModule_RealtimeControllerGetPoseAccessor(pApp->module_realtimecontroller));
	PX_LiveEditorModule_ExportLiveDisable(pApp->module_exportlive);

	if ((pApp->module_timestampeditor = PX_LiveEditorModule_TimeStampEditorInstall(pObject, pruntime, &pApp->fontmodule, &pApp->liveFramework, &pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_TimeStampEditorDisable(pApp->module_timestampeditor);

	if ((pApp->module_newframeeditor = PX_LiveEditorModule_NewFrameEditorInstall(pObject, pruntime, &pApp->fontmodule, &pApp->liveFramework, &pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_NewFrameEditorDisable(pApp->module_newframeeditor);

	if ((pApp->module_keytransform = PX_LiveEditorModule_KeyTransformInstall(pObject, pruntime, &pApp->fontmodule, &pApp->liveFramework, &pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_KeyTransformDisable(pApp->module_keytransform);

	if ((pApp->module_vertextransform = PX_LiveEditorModule_VertexTransformInstall(pObject, pruntime, &pApp->fontmodule, &pApp->liveFramework, &pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_VertexTransformDisable(pApp->module_vertextransform);

	if ((pApp->module_framelivetextureeditor = PX_LiveEditorModule_FrameLiveTextureEditorInstall(pObject, pruntime, &pApp->fontmodule, &pApp->liveFramework, &pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_FrameLiveTextureEditorDisable(pApp->module_framelivetextureeditor);

	if ((pApp->module_globalrotation = PX_LiveEditorModule_GlobalRotationInstall(pObject, pruntime, &pApp->fontmodule, &pApp->liveFramework, &pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_GlobalRotationDisable(pApp->module_globalrotation);

	if ((pApp->module_impulse = PX_LiveEditorModule_ImpulseEditorInstall(pObject, pruntime, &pApp->fontmodule, &pApp->liveFramework, &pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_ImpulseEditorDisable(pApp->module_impulse);

	if ((pApp->module_panc = PX_LiveEditorModule_PancInstall(pObject, pruntime, &pApp->fontmodule, &pApp->liveFramework, &pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_PancDisable(pApp->module_panc);

	//////////////////////////////////////////////////////////////////////////
	//top modules
	if ((pApp->module_messagebox = PX_Object_MessageBoxCreate(mp, pObject, &pApp->fontmodule)) == PX_NULL)
		goto _ERROR;

	return pObject;

_ERROR:
	PX_ObjectDelete(pObject);
	return 0;
}

typedef struct
{
	PX_Runtime *pruntime;
	px_byte *uiMemory;
	px_memorypool uiPool;
	PX_FontModule fontmodule;
	PX_Json languageJson;
	PX_LiveFramework liveFramework;
	PX_LiveEditorRealtimePoseAccessor accessor;
	PX_Object *preview;
}PX_Object_RealtimePreviewWindow;

#define PX_REALTIME_PREVIEW_UI_POOL_SIZE (8u*1024u*1024u)

typedef enum
{
	PX_REALTIME_PREVIEW_INIT_CREATE_ROOT,
	PX_REALTIME_PREVIEW_INIT_FONT,
	PX_REALTIME_PREVIEW_INIT_FONT_FILE,
	PX_REALTIME_PREVIEW_INIT_LANGUAGE,
	PX_REALTIME_PREVIEW_INIT_LANGUAGE_FILE,
	PX_REALTIME_PREVIEW_INIT_SNAPSHOT_FILE,
	PX_REALTIME_PREVIEW_INIT_MODEL_IMPORT,
	PX_REALTIME_PREVIEW_INIT_POSE_ACCESSOR,
	PX_REALTIME_PREVIEW_INIT_UI_POOL,
	PX_REALTIME_PREVIEW_INIT_UI_ROOT,
	PX_REALTIME_PREVIEW_INIT_UI_TITLE,
	PX_REALTIME_PREVIEW_INIT_UI_CLOSE_BUTTON,
	PX_REALTIME_PREVIEW_INIT_UI_PANEL,
	PX_REALTIME_PREVIEW_INIT_UI_PANEL_HEADER,
	PX_REALTIME_PREVIEW_INIT_UI_SCROLLAREA
}PX_REALTIME_PREVIEW_INIT_STAGE;

static const px_char *PX_Object_RealtimePreviewInitStageText(PX_REALTIME_PREVIEW_INIT_STAGE stage)
{
	switch (stage)
	{
	case PX_REALTIME_PREVIEW_INIT_CREATE_ROOT: return "preview root allocation";
	case PX_REALTIME_PREVIEW_INIT_FONT: return "font module allocation";
	case PX_REALTIME_PREVIEW_INIT_FONT_FILE: return "assets/font18.pxf loading";
	case PX_REALTIME_PREVIEW_INIT_LANGUAGE: return "language module allocation";
	case PX_REALTIME_PREVIEW_INIT_LANGUAGE_FILE: return "assets/language.json loading";
	case PX_REALTIME_PREVIEW_INIT_SNAPSHOT_FILE: return "temporary model snapshot loading";
	case PX_REALTIME_PREVIEW_INIT_MODEL_IMPORT: return "temporary model snapshot import";
	case PX_REALTIME_PREVIEW_INIT_POSE_ACCESSOR: return "realtime pose accessor initialization";
	case PX_REALTIME_PREVIEW_INIT_UI_POOL: return "dedicated preview UI memory-pool allocation";
	case PX_REALTIME_PREVIEW_INIT_UI_ROOT: return "preview user-interface root allocation";
	case PX_REALTIME_PREVIEW_INIT_UI_TITLE: return "preview title allocation";
	case PX_REALTIME_PREVIEW_INIT_UI_CLOSE_BUTTON: return "preview close-button allocation";
	case PX_REALTIME_PREVIEW_INIT_UI_PANEL: return "preview axis-panel allocation";
	case PX_REALTIME_PREVIEW_INIT_UI_PANEL_HEADER: return "preview axis-panel header allocation";
	case PX_REALTIME_PREVIEW_INIT_UI_SCROLLAREA: return "preview axis scroll-area allocation";
	default: return "unknown stage";
	}
}

static PX_Object *PX_Object_RealtimePreviewInitializationFailed(PX_Object *object,const px_char snapshotPath[],PX_REALTIME_PREVIEW_INIT_STAGE stage)
{
	px_char message[320];
	px_byte *uiMemory=PX_NULL;
	if (snapshotPath && snapshotPath[0]) PX_FileDelete(snapshotPath);
	if (object && object->pObjectDesc[0]) uiMemory=((PX_Object_RealtimePreviewWindow *)object->pObjectDesc[0])->uiMemory;
	if (object) PX_ObjectDelete(object);
	if (uiMemory) free(uiMemory);
#ifdef _WIN32
	PX_sprintf1(message,sizeof(message),"Realtime preview initialization failed at:\r\n%1",PX_STRINGFORMAT_STRING(PX_Object_RealtimePreviewInitStageText(stage)));
	MessageBoxA(PX_NULL,message,"PainterEngine",MB_OK|MB_ICONERROR);
	/* px_main runs before the Windows platform creates its native window.
	   Posting WM_QUIT here still lets PX_CreateWindow run afterwards, leaving
	   an empty window.  This is a preview-only child process, so terminate it
	   at the bootstrap boundary instead. */
	ExitProcess(1);
#endif
	return PX_NULL;
}

static PX_Object *PX_Object_RealtimePreviewWindowCreate(PX_Runtime *pruntime,PX_Object *parent,const px_char snapshotPath[])
{
	PX_Object *object;
	PX_Object_RealtimePreviewWindow desc,*window;
	PX_IO_Data snapshot;
	PX_REALTIME_PREVIEW_INIT_STAGE stage=PX_REALTIME_PREVIEW_INIT_CREATE_ROOT;
	PX_LIVEEDITOR_BLENDPREVIEW_INSTALL_ERROR installError=PX_LIVEEDITOR_BLENDPREVIEW_INSTALL_OK;
	PX_memset(&desc,0,sizeof(desc));
	PX_memset(&snapshot,0,sizeof(snapshot));
	desc.pruntime=pruntime;
	object=PX_ObjectCreateEx(mp,parent,0,0,0,0,0,0,0,PX_NULL,PX_NULL,PX_NULL,&desc,sizeof(desc));
	if (!object) return PX_Object_RealtimePreviewInitializationFailed(PX_NULL,snapshotPath,stage);
	window=(PX_Object_RealtimePreviewWindow *)object->pObjectDesc[0];
	stage=PX_REALTIME_PREVIEW_INIT_FONT;
	if (!PX_FontModuleInitialize(mp_static,&window->fontmodule)) goto error;
	stage=PX_REALTIME_PREVIEW_INIT_FONT_FILE;
	if (!PX_LoadFontModuleFromFile(&window->fontmodule,"assets/font18.pxf")) goto error;
	stage=PX_REALTIME_PREVIEW_INIT_LANGUAGE;
	if (!PX_JsonInitialize(mp_static,&window->languageJson)) goto error;
	stage=PX_REALTIME_PREVIEW_INIT_LANGUAGE_FILE;
	if (!PX_LoadJsonFromFile(&window->languageJson,"assets/language.json")) goto error;
	stage=PX_REALTIME_PREVIEW_INIT_SNAPSHOT_FILE;
	snapshot=PX_LoadFileToIOData(snapshotPath);
	if (!snapshot.buffer || !snapshot.size) goto error;
	{
		px_int expectedChecksum;
		if (PX_Object_GetRealtimePreviewChecksum(&expectedChecksum)&&
			PX_Object_LiveEditorSnapshotChecksum(snapshot.buffer,(px_int)snapshot.size)!=expectedChecksum)
		{
			goto error;
		}
	}
	stage=PX_REALTIME_PREVIEW_INIT_MODEL_IMPORT;
	/* Import owns the complete framework initialization.  The normal editor
	   importer likewise clears the destination and imports directly; calling
	   Initialize with the dynamic pool first only leaked three vectors and
	   mixed allocator ownership before the static-pool import. */
	PX_memset(&window->liveFramework,0,sizeof(window->liveFramework));
	if (!PX_LiveFrameworkImport(&pruntime->mp_static,&window->liveFramework,snapshot.buffer,snapshot.size))
	{
		PX_FreeIOData(&snapshot);
		PX_memset(&snapshot,0,sizeof(snapshot));
		goto error;
	}
	PX_FreeIOData(&snapshot);
	PX_memset(&snapshot,0,sizeof(snapshot));
	PX_FileDelete(snapshotPath);
	stage=PX_REALTIME_PREVIEW_INIT_POSE_ACCESSOR;
	if (!PX_LiveEditorRealtimePoseAccessorInitializePreview(&window->accessor,&pruntime->mp_dynamic,&window->liveFramework)) goto error;
	stage=PX_REALTIME_PREVIEW_INIT_UI_POOL;
	window->uiMemory=(px_byte *)malloc(PX_REALTIME_PREVIEW_UI_POOL_SIZE);
	if (!window->uiMemory) goto error;
	window->uiPool=MP_Create(window->uiMemory,PX_REALTIME_PREVIEW_UI_POOL_SIZE);
	stage=PX_REALTIME_PREVIEW_INIT_UI_ROOT;
	window->preview=PX_LiveEditorModule_RealtimeBlendPreviewInstall(object,pruntime,&window->uiPool,&window->fontmodule,&window->accessor,&window->languageJson,PX_TRUE,&installError);
	if (!window->preview)
	{
		switch (installError)
		{
		case PX_LIVEEDITOR_BLENDPREVIEW_INSTALL_TITLE: stage=PX_REALTIME_PREVIEW_INIT_UI_TITLE; break;
		case PX_LIVEEDITOR_BLENDPREVIEW_INSTALL_CLOSE_BUTTON: stage=PX_REALTIME_PREVIEW_INIT_UI_CLOSE_BUTTON; break;
		case PX_LIVEEDITOR_BLENDPREVIEW_INSTALL_PANEL: stage=PX_REALTIME_PREVIEW_INIT_UI_PANEL; break;
		case PX_LIVEEDITOR_BLENDPREVIEW_INSTALL_PANEL_HEADER: stage=PX_REALTIME_PREVIEW_INIT_UI_PANEL_HEADER; break;
		case PX_LIVEEDITOR_BLENDPREVIEW_INSTALL_SCROLLAREA: stage=PX_REALTIME_PREVIEW_INIT_UI_SCROLLAREA; break;
		default: stage=PX_REALTIME_PREVIEW_INIT_UI_ROOT; break;
		}
		goto error;
	}
	PX_LiveEditorModule_RealtimeBlendPreviewOpen(window->preview);
	return object;
error:
	if (snapshot.buffer) PX_FreeIOData(&snapshot);
	return PX_Object_RealtimePreviewInitializationFailed(object,snapshotPath,stage);
}

static px_bool PX_Object_GetRealtimePreviewSnapshotPath(px_char path[],px_int pathSize)
{
#ifdef _WIN32
	const px_char *flag="--rt30-preview=",*commandLine=GetCommandLineA(),*begin;
	px_int i=0;
	begin=strstr(commandLine,flag);
	if (!begin) return PX_FALSE;
	begin+=PX_strlen(flag);
	if (*begin=='\"') begin++;
	while (*begin && *begin!=' ' && *begin!='\"' && i<pathSize-1) path[i++]=*begin++;
	path[i]=0;
	return i>0;
#else
	return PX_FALSE;
#endif
}

px_int main()
{
	px_char previewSnapshotPath[260];
	PainterEngine_Initialize(1200, 800);
	if (PX_Object_GetRealtimePreviewSnapshotPath(previewSnapshotPath,sizeof(previewSnapshotPath)))
		PX_Object_RealtimePreviewWindowCreate(PainterEngine_GetRuntime(),root,previewSnapshotPath);
	else
		PX_Object_LiveEditorCreate(PainterEngine_GetRuntime(), root);
	return 1;
}
