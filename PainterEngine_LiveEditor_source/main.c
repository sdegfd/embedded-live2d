#include "PainterEngine.h"

#include "PainterEngine_LiveEditorModules_Menu.h"
#include "PainterEngine_LiveEditorModules_CreateProject.h"
#include "PainterEngine_LiveEditorModules_ImportImage.h"
#include "PainterEngine_LiveEditorModules_LiveController.h"
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
	PX_Object* module_importlive;
	PX_Object* module_exportlive;
	PX_Object* module_livecontroller;
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
	PX_Object_LiveEditorSwitchEditMode(pApp);
	PX_LiveEditorModule_CreateProjectDisable(pApp->module_createproject);
}



px_void PX_Object_LiveEditorOnScheduleEvent_LoadImage(PX_Object_LiveEditor* pApp, PX_Object_Event e, px_void* ptr)
{
	PX_LiveEditorModule_ImportImageEnable(pApp->module_importimage);
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

	if (pController->state == PX_LIVEEDITORMODULE_LIVECONTROLLER_STATE_ANIMATION)
	{
		PX_LiveEditorModule_AnimationControllerEnable(pApp->module_animationcontroller);
		PX_LiveEditorModule_FrameControllerEnable(pApp->module_framecontroller);
		PX_LiveEditorModule_TranslationDisable(pApp->module_translation);

	}
	else
	{
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
	PX_LiveEditorModule_KeyTransformEnable(pApp->module_keytransform, PX_LIVEEDITORMODULE_KEYTRANSFORM_MODE_ALL);
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
	case PX_LIVEEDITORMODULE_IMPORTLIVE_EVENT_MESSAGE:
	case PX_LIVEEDITORMODULE_IMPORTIMAGE_EVENT_MESSAGE:
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
	case PX_LIVEEDITORMODULE_IMPORTIMAGE_EVENT_EXIT:
	{
		PX_Object_LiveEditorOnScheduleEvent_ImportImage_exit(pApp, e, ptr);
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

	if ((pApp->module_importlive = PX_LiveEditorModule_ImportLiveInstall(pObject, pruntime, &pApp->fontmodule, &pApp->liveFramework, &pApp->languageJson)) == PX_NULL)
		goto _ERROR;
	PX_LiveEditorModule_ImportLiveDisable(pApp->module_importlive);

	if ((pApp->module_exportlive = PX_LiveEditorModule_ExportLiveInstall(pObject, pruntime, &pApp->fontmodule, &pApp->liveFramework, &pApp->languageJson)) == PX_NULL)
		goto _ERROR;
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

px_int main()
{
	PainterEngine_Initialize(1200, 800);
	PX_Object_LiveEditorCreate(PainterEngine_GetRuntime(), root);
	return 1;
}