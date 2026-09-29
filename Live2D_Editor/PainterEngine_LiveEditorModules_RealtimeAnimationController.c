#include "PainterEngine_LiveEditorModules_RealtimeAnimationController.h"
#include "PainterEngine_Resource_smarttranslation_traw.h"
#include "PainterEngine_Resource_globalrotation_traw.h"
#include "PainterEngine_Resource_stretch_traw.h"
#include "PainterEngine_Resource_pointtranslation_traw.h"
#include "PainterEngine_Resource_rotation_traw.h"
#include "PainterEngine_Resource_switchtexture_traw.h"
#include "PainterEngine_Resource_impulse_traw.h"
#include "PainterEngine_Resource_pan_traw.h"

static px_void PX_LiveEditorModule_RealtimeAnimationControllerDisableEditors(PX_LiveEditorModule_RealtimeAnimationController *controller)
{
	PX_LiveEditorModule_RealtimeTransformDisable(controller->transformEditor);
	PX_LiveEditorModule_RealtimeVertexTransformDisable(controller->vertexEditor);
	PX_LiveEditorModule_RealtimeTextureDisable(controller->textureEditor);
	PX_LiveEditorModule_RealtimeImpulseDisable(controller->impulseEditor);
	PX_LiveEditorModule_RealtimePancDisable(controller->pancEditor);
}

static px_bool PX_LiveEditorModule_RealtimeAnimationControllerEnsureEditable(PX_LiveEditorModule_RealtimeAnimationController *controller)
{
	PX_LiveEditorRealtimeAxisAuthoring *axis;
	px_int selected=PX_LiveEditorRealtimePoseAccessorGetSelectedAxis(controller->accessor),defaultSlot,targetSlot=-1;
	axis=PX_LiveEditorRealtimePoseAccessorGetAxis(controller->accessor,selected);
	if (!axis||!axis->authoringAvailable)
	{
		PX_strcpy(controller->hoverTip,"请先创建实时轴",sizeof(controller->hoverTip));
		return PX_FALSE;
	}
	if (PX_LiveEditorRealtimePoseAccessorIsSelectedKeyEditable(controller->accessor)) return PX_TRUE;
	defaultSlot=axis->defaultSampleIndex==0?0:(axis->defaultSampleIndex==PX_LIVE_REALTIME_SAMPLE_COUNT-1?2:1);
	/* Authoring and runtime preview are separate states.  A preview may enter
	   its exact K0/KM/K29 authoring key, but it must never fall back to the
	   previously selected key (or silently choose an endpoint). */
	if (controller->accessor->authoringPoseActive)
	{
		PX_strcpy(controller->hoverTip,"默认关键点不可编辑",sizeof(controller->hoverTip));
		return PX_FALSE;
	}
	if (axis->sampleIndex==0) targetSlot=0;
	else if (axis->sampleIndex==axis->middleKeyIndex) targetSlot=1;
	else if (axis->sampleIndex==PX_LIVE_REALTIME_SAMPLE_COUNT-1) targetSlot=2;
	if (targetSlot<0)
	{
		PX_strcpy(controller->hoverTip,"当前档位不可直接编辑",sizeof(controller->hoverTip));
		return PX_FALSE;
	}
	if (targetSlot==defaultSlot)
	{
		PX_strcpy(controller->hoverTip,"默认关键点不可编辑",sizeof(controller->hoverTip));
		return PX_FALSE;
	}
	if (axis->selectedKeySlot==(px_uchar)targetSlot)
	{
		if (!PX_LiveEditorRealtimePoseAccessorApplySelectedKey(controller->accessor)) return PX_FALSE;
	}
	else if (!PX_LiveEditorRealtimePoseAccessorSelectKeySlot(controller->accessor,targetSlot)) return PX_FALSE;
	return PX_LiveEditorRealtimePoseAccessorIsSelectedKeyEditable(controller->accessor);
}

static px_void PX_LiveEditorModule_RealtimeAnimationControllerSnapshotContext(PX_LiveEditorModule_RealtimeAnimationController *controller)
{
	PX_LiveEditorRealtimeAxisAuthoring *axis;
	controller->activeAxis=PX_LiveEditorRealtimePoseAccessorGetSelectedAxis(controller->accessor);
	axis=PX_LiveEditorRealtimePoseAccessorGetAxis(controller->accessor,controller->activeAxis);
	controller->activeKeySlot=axis?axis->selectedKeySlot:-1;
	controller->editable=PX_LiveEditorRealtimePoseAccessorIsSelectedKeyEditable(controller->accessor);
}

static px_void PX_LiveEditorModule_RealtimeAnimationControllerOnHover(PX_Object *button,PX_Object_Event e,px_void *ptr)
{
	PX_Object *object=(PX_Object *)ptr;
	PX_LiveEditorModule_RealtimeAnimationController *controller=(PX_LiveEditorModule_RealtimeAnimationController *)object->pObjectDesc[0];
	PX_Object_PushButton *push=PX_Object_GetPushButton(button);
	if (e.Event==PX_OBJECT_EVENT_CURSOROUT)
	{
		if (push&&push->Tips&&PX_strequ(controller->hoverTip,push->Tips)) controller->hoverTip[0]=0;
		return;
	}
	if (push&&push->Tips) PX_strcpy(controller->hoverTip,push->Tips,sizeof(controller->hoverTip));
}

static px_void PX_LiveEditorModule_RealtimeAnimationControllerSetFocus(PX_LiveEditorModule_RealtimeAnimationController *controller,PX_LIVEEDITOR_REALTIME_TOOL tool)
{
	controller->focus=tool;
	PX_Object_PushButtonSetBackgroundColor(controller->button_translation,PX_OBJECT_UI_DEFAULT_BACKGROUNDCOLOR);
	PX_Object_PushButtonSetBackgroundColor(controller->button_rotation,PX_OBJECT_UI_DEFAULT_BACKGROUNDCOLOR);
	PX_Object_PushButtonSetBackgroundColor(controller->button_scale,PX_OBJECT_UI_DEFAULT_BACKGROUNDCOLOR);
	PX_Object_PushButtonSetBackgroundColor(controller->button_vertices,PX_OBJECT_UI_DEFAULT_BACKGROUNDCOLOR);
	PX_Object_PushButtonSetBackgroundColor(controller->button_globalrotation,PX_OBJECT_UI_DEFAULT_BACKGROUNDCOLOR);
	PX_Object_PushButtonSetBackgroundColor(controller->button_texture,PX_OBJECT_UI_DEFAULT_BACKGROUNDCOLOR);
	PX_Object_PushButtonSetBackgroundColor(controller->button_impulse,PX_OBJECT_UI_DEFAULT_BACKGROUNDCOLOR);
	PX_Object_PushButtonSetBackgroundColor(controller->button_panc,PX_OBJECT_UI_DEFAULT_BACKGROUNDCOLOR);
	if (tool==PX_LIVEEDITOR_REALTIME_TOOL_TRANSLATION) PX_Object_PushButtonSetBackgroundColor(controller->button_translation,PX_COLOR_FORCECOLOR);
	if (tool==PX_LIVEEDITOR_REALTIME_TOOL_ROTATION) PX_Object_PushButtonSetBackgroundColor(controller->button_rotation,PX_COLOR_FORCECOLOR);
	if (tool==PX_LIVEEDITOR_REALTIME_TOOL_SCALE) PX_Object_PushButtonSetBackgroundColor(controller->button_scale,PX_COLOR_FORCECOLOR);
	if (tool==PX_LIVEEDITOR_REALTIME_TOOL_VERTICES) PX_Object_PushButtonSetBackgroundColor(controller->button_vertices,PX_COLOR_FORCECOLOR);
	if (tool==PX_LIVEEDITOR_REALTIME_TOOL_GLOBAL_ROTATION) PX_Object_PushButtonSetBackgroundColor(controller->button_globalrotation,PX_COLOR_FORCECOLOR);
	if (tool==PX_LIVEEDITOR_REALTIME_TOOL_TEXTURE) PX_Object_PushButtonSetBackgroundColor(controller->button_texture,PX_COLOR_FORCECOLOR);
	if (tool==PX_LIVEEDITOR_REALTIME_TOOL_IMPULSE) PX_Object_PushButtonSetBackgroundColor(controller->button_impulse,PX_COLOR_FORCECOLOR);
	if (tool==PX_LIVEEDITOR_REALTIME_TOOL_PANC) PX_Object_PushButtonSetBackgroundColor(controller->button_panc,PX_COLOR_FORCECOLOR);
}

static px_void PX_LiveEditorModule_RealtimeAnimationControllerEnableFocusedEditor(PX_LiveEditorModule_RealtimeAnimationController *controller)
{
	if (controller->focus==PX_LIVEEDITOR_REALTIME_TOOL_VERTICES)
	{
		PX_LiveEditorModule_RealtimeVertexTransformEnable(controller->vertexEditor);
	}
	else if (controller->focus==PX_LIVEEDITOR_REALTIME_TOOL_TEXTURE) PX_LiveEditorModule_RealtimeTextureEnable(controller->textureEditor);
	else if (controller->focus==PX_LIVEEDITOR_REALTIME_TOOL_IMPULSE) PX_LiveEditorModule_RealtimeImpulseEnable(controller->impulseEditor);
	else if (controller->focus==PX_LIVEEDITOR_REALTIME_TOOL_PANC) PX_LiveEditorModule_RealtimePancEnable(controller->pancEditor);
	else
	{
		PX_LIVEEDITOR_REALTIME_TRANSFORM_MODE mode=PX_LIVEEDITOR_REALTIME_TRANSFORM_TRANSLATION;
		if (controller->focus==PX_LIVEEDITOR_REALTIME_TOOL_ROTATION) mode=PX_LIVEEDITOR_REALTIME_TRANSFORM_ROTATION;
		if (controller->focus==PX_LIVEEDITOR_REALTIME_TOOL_SCALE) mode=PX_LIVEEDITOR_REALTIME_TRANSFORM_SCALE;
		if (controller->focus==PX_LIVEEDITOR_REALTIME_TOOL_GLOBAL_ROTATION) mode=PX_LIVEEDITOR_REALTIME_TRANSFORM_GLOBAL_ROTATION;
		PX_LiveEditorModule_RealtimeTransformEnable(controller->transformEditor,mode);
	}
}

static px_void PX_LiveEditorModule_RealtimeAnimationControllerOnTool(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_Object *controllerObject=(PX_Object *)ptr;
	PX_LiveEditorModule_RealtimeAnimationController *controller=(PX_LiveEditorModule_RealtimeAnimationController *)controllerObject->pObjectDesc[0];
	if (!PX_LiveEditorModule_RealtimeAnimationControllerEnsureEditable(controller)) return;
	PX_LiveEditorModule_RealtimeAnimationControllerDisableEditors(controller);
	if (pObject==controller->button_translation)
	{
		PX_LiveEditorModule_RealtimeTransformEnable(controller->transformEditor,PX_LIVEEDITOR_REALTIME_TRANSFORM_TRANSLATION);
		PX_LiveEditorModule_RealtimeAnimationControllerSetFocus(controller,PX_LIVEEDITOR_REALTIME_TOOL_TRANSLATION);
	}
	else if (pObject==controller->button_rotation)
	{
		PX_LiveEditorModule_RealtimeTransformEnable(controller->transformEditor,PX_LIVEEDITOR_REALTIME_TRANSFORM_ROTATION);
		PX_LiveEditorModule_RealtimeAnimationControllerSetFocus(controller,PX_LIVEEDITOR_REALTIME_TOOL_ROTATION);
	}
	else if (pObject==controller->button_scale)
	{
		PX_LiveEditorModule_RealtimeTransformEnable(controller->transformEditor,PX_LIVEEDITOR_REALTIME_TRANSFORM_SCALE);
		PX_LiveEditorModule_RealtimeAnimationControllerSetFocus(controller,PX_LIVEEDITOR_REALTIME_TOOL_SCALE);
	}
	else if (pObject==controller->button_globalrotation)
	{
		PX_LiveEditorModule_RealtimeTransformEnable(controller->transformEditor,PX_LIVEEDITOR_REALTIME_TRANSFORM_GLOBAL_ROTATION);
		PX_LiveEditorModule_RealtimeAnimationControllerSetFocus(controller,PX_LIVEEDITOR_REALTIME_TOOL_GLOBAL_ROTATION);
	}
	else if (pObject==controller->button_texture)
	{
		PX_LiveEditorModule_RealtimeTextureEnable(controller->textureEditor);
		PX_LiveEditorModule_RealtimeAnimationControllerSetFocus(controller,PX_LIVEEDITOR_REALTIME_TOOL_TEXTURE);
	}
	else if (pObject==controller->button_vertices)
	{
		PX_LiveEditorModule_RealtimeVertexTransformEnable(controller->vertexEditor);
		PX_LiveEditorModule_RealtimeAnimationControllerSetFocus(controller,PX_LIVEEDITOR_REALTIME_TOOL_VERTICES);
	}
	else if (pObject==controller->button_impulse)
	{
		PX_LiveEditorModule_RealtimeImpulseEnable(controller->impulseEditor);
		PX_LiveEditorModule_RealtimeAnimationControllerSetFocus(controller,PX_LIVEEDITOR_REALTIME_TOOL_IMPULSE);
	}
	else
	{
		PX_LiveEditorModule_RealtimePancEnable(controller->pancEditor);
		PX_LiveEditorModule_RealtimeAnimationControllerSetFocus(controller,PX_LIVEEDITOR_REALTIME_TOOL_PANC);
	}
	PX_LiveEditorModule_RealtimeAnimationControllerSnapshotContext(controller);
}

PX_OBJECT_UPDATE_FUNCTION(PX_LiveEditorModule_RealtimeAnimationControllerUpdate)
{
	PX_LiveEditorModule_RealtimeAnimationController *controller=(PX_LiveEditorModule_RealtimeAnimationController *)pObject->pObjectDesc[0];
	PX_LiveEditorRealtimeAxisAuthoring *axis;
	px_float targetX=controller->pruntime->surface_width/2-pObject->Width/2;
	px_float targetY=pObject->Enabled?controller->pruntime->surface_height-pObject->Height-1:controller->pruntime->surface_height+1.0f;
	px_int activeAxis=PX_LiveEditorRealtimePoseAccessorGetSelectedAxis(controller->accessor),activeKeySlot;
	px_bool editable=PX_LiveEditorRealtimePoseAccessorIsSelectedKeyEditable(controller->accessor);
	axis=PX_LiveEditorRealtimePoseAccessorGetAxis(controller->accessor,activeAxis);
	activeKeySlot=axis?axis->selectedKeySlot:-1;
	pObject->x=targetX;
	pObject->y=targetY;
	/* Every editor owns transient selection/anchor/baseline state.  Reset it
	   whenever the authoring axis or key changes, even if both keys happen to
	   be editable, so no tool can carry K29 state into KM/K0 or another axis. */
	if (controller->editable!=editable || controller->activeAxis!=activeAxis || controller->activeKeySlot!=activeKeySlot)
	{
		PX_LiveEditorModule_RealtimeAnimationControllerDisableEditors(controller);
		if (editable) PX_LiveEditorModule_RealtimeAnimationControllerEnableFocusedEditor(controller);
		controller->editable=editable;
		controller->activeAxis=activeAxis;
		controller->activeKeySlot=activeKeySlot;
	}
	/* Keep buttons enabled outside authoring so hover help and the exact
	   preview-key handoff can receive events. */
	controller->button_translation->Enabled=PX_TRUE;
	controller->button_rotation->Enabled=PX_TRUE;
	controller->button_scale->Enabled=PX_TRUE;
	controller->button_vertices->Enabled=PX_TRUE;
	controller->button_globalrotation->Enabled=PX_TRUE;
	controller->button_texture->Enabled=PX_TRUE;
	controller->button_impulse->Enabled=PX_TRUE;
	controller->button_panc->Enabled=PX_TRUE;
}

PX_OBJECT_RENDER_FUNCTION(PX_LiveEditorModule_RealtimeAnimationControllerRender)
{
	PX_GeoDrawRect(psurface,(px_int)pObject->x,(px_int)pObject->y,(px_int)(pObject->x+pObject->Width-1),(px_int)(pObject->y+pObject->Height-1),PX_OBJECT_UI_DEFAULT_BACKGROUNDCOLOR);
	PX_GeoDrawBorder(psurface,(px_int)pObject->x,(px_int)pObject->y,(px_int)(pObject->x+pObject->Width-1),(px_int)(pObject->y+pObject->Height-1),1,PX_OBJECT_UI_DEFAULT_BORDERCOLOR);
	{
		PX_LiveEditorModule_RealtimeAnimationController *controller=(PX_LiveEditorModule_RealtimeAnimationController *)pObject->pObjectDesc[0];
		/* This toolbar draws an immediate shared label; suppress the PushButton
		   one-second duplicate while retaining each button's tip text. */
		PX_Object_GetPushButton(controller->button_translation)->bCursorShowTipsElapsed=0;
		PX_Object_GetPushButton(controller->button_rotation)->bCursorShowTipsElapsed=0;
		PX_Object_GetPushButton(controller->button_scale)->bCursorShowTipsElapsed=0;
		PX_Object_GetPushButton(controller->button_globalrotation)->bCursorShowTipsElapsed=0;
		PX_Object_GetPushButton(controller->button_texture)->bCursorShowTipsElapsed=0;
		PX_Object_GetPushButton(controller->button_vertices)->bCursorShowTipsElapsed=0;
		PX_Object_GetPushButton(controller->button_impulse)->bCursorShowTipsElapsed=0;
		PX_Object_GetPushButton(controller->button_panc)->bCursorShowTipsElapsed=0;
		if (controller->hoverTip[0])
		{
			px_int w,h,cx=(px_int)(pObject->x+pObject->Width/2),bottom=(px_int)pObject->y-3;
			PX_FontModuleTextGetRenderWidthHeight(PX_Object_GetPushButton(controller->button_translation)->fontModule,controller->hoverTip,&w,&h);
			PX_GeoDrawRect(psurface,cx-w/2-4,bottom-h-4,cx+w/2+4,bottom,PX_COLOR_BACKGROUNDCOLOR);
			PX_GeoDrawBorder(psurface,cx-w/2-4,bottom-h-4,cx+w/2+4,bottom,1,PX_COLOR_BORDERCOLOR);
			PX_FontModuleDrawText(psurface,PX_Object_GetPushButton(controller->button_translation)->fontModule,cx,bottom-2,PX_ALIGN_MIDBOTTOM,controller->hoverTip,PX_OBJECT_UI_DEFAULT_FONTCOLOR);
		}
	}
}

PX_OBJECT_FREE_FUNCTION(PX_LiveEditorModule_RealtimeAnimationControllerFree)
{
	PX_LiveEditorModule_RealtimeAnimationController *controller=(PX_LiveEditorModule_RealtimeAnimationController *)pObject->pObjectDesc[0];
	PX_ShapeFree(&controller->shape_translation);
	PX_ShapeFree(&controller->shape_rotation);
	PX_ShapeFree(&controller->shape_scale);
	PX_ShapeFree(&controller->shape_vertices);
	PX_ShapeFree(&controller->shape_globalrotation);
	PX_ShapeFree(&controller->shape_texture);
	PX_ShapeFree(&controller->shape_impulse);
	PX_ShapeFree(&controller->shape_panc);
}

PX_Object *PX_LiveEditorModule_RealtimeAnimationControllerInstall(PX_Object *parent,PX_Runtime *pruntime,PX_FontModule *fm,PX_LiveEditorRealtimePoseAccessor *accessor,PX_Json *language)
{
	PX_Object *object;
	PX_LiveEditorModule_RealtimeAnimationController desc,*controller;
	PX_memset(&desc,0,sizeof(desc));
	desc.pruntime=pruntime;
	desc.accessor=accessor;
	object=PX_ObjectCreateEx(&pruntime->mp_dynamic,parent,0,(px_float)pruntime->surface_height+1,0,257,33,0,0,PX_LiveEditorModule_RealtimeAnimationControllerUpdate,PX_LiveEditorModule_RealtimeAnimationControllerRender,PX_LiveEditorModule_RealtimeAnimationControllerFree,&desc,sizeof(desc));
	if (!object) return PX_NULL;
	controller=(PX_LiveEditorModule_RealtimeAnimationController *)object->pObjectDesc[0];
	controller->activeAxis=-1;
	controller->activeKeySlot=-1;
	if (!PX_ShapeCreateFromMemory(&pruntime->mp_static,(px_void *)smarttranslation_traw,sizeof(smarttranslation_traw),&controller->shape_translation) ||
		!PX_ShapeCreateFromMemory(&pruntime->mp_static,(px_void *)globalrotation_traw,sizeof(globalrotation_traw),&controller->shape_rotation) ||
		!PX_ShapeCreateFromMemory(&pruntime->mp_static,(px_void *)stretch_traw,sizeof(stretch_traw),&controller->shape_scale) ||
		!PX_ShapeCreateFromMemory(&pruntime->mp_static,(px_void *)pointtranslation_traw,sizeof(pointtranslation_traw),&controller->shape_vertices) ||
		!PX_ShapeCreateFromMemory(&pruntime->mp_static,(px_void *)rotation_traw,sizeof(rotation_traw),&controller->shape_globalrotation) ||
		!PX_ShapeCreateFromMemory(&pruntime->mp_static,(px_void *)switchtexture_traw,sizeof(switchtexture_traw),&controller->shape_texture) ||
		!PX_ShapeCreateFromMemory(&pruntime->mp_static,(px_void *)impulse_traw,sizeof(impulse_traw),&controller->shape_impulse) ||
		!PX_ShapeCreateFromMemory(&pruntime->mp_static,(px_void *)pan_traw,sizeof(pan_traw),&controller->shape_panc))
	{
		PX_ObjectDelete(object);
		return PX_NULL;
	}
	controller->button_translation=PX_Object_PushButtonCreate(&pruntime->mp_dynamic,object,1,0,32,32,"",fm);
	controller->button_rotation=PX_Object_PushButtonCreate(&pruntime->mp_dynamic,object,33,0,32,32,"",fm);
	controller->button_scale=PX_Object_PushButtonCreate(&pruntime->mp_dynamic,object,65,0,32,32,"",fm);
	controller->button_globalrotation=PX_Object_PushButtonCreate(&pruntime->mp_dynamic,object,97,0,32,32,"",fm);
	controller->button_texture=PX_Object_PushButtonCreate(&pruntime->mp_dynamic,object,129,0,32,32,"",fm);
	controller->button_vertices=PX_Object_PushButtonCreate(&pruntime->mp_dynamic,object,161,0,32,32,"",fm);
	controller->button_impulse=PX_Object_PushButtonCreate(&pruntime->mp_dynamic,object,193,0,32,32,"",fm);
	controller->button_panc=PX_Object_PushButtonCreate(&pruntime->mp_dynamic,object,225,0,32,32,"",fm);
	PX_Object_PushButtonSetShape(controller->button_translation,&controller->shape_translation);
	PX_Object_PushButtonSetShape(controller->button_rotation,&controller->shape_rotation);
	PX_Object_PushButtonSetShape(controller->button_scale,&controller->shape_scale);
	PX_Object_PushButtonSetShape(controller->button_vertices,&controller->shape_vertices);
	PX_Object_PushButtonSetShape(controller->button_globalrotation,&controller->shape_globalrotation);
	PX_Object_PushButtonSetShape(controller->button_texture,&controller->shape_texture);
	PX_Object_PushButtonSetShape(controller->button_impulse,&controller->shape_impulse);
	PX_Object_PushButtonSetShape(controller->button_panc,&controller->shape_panc);
	PX_Object_PushButtonSetTips(controller->button_translation,PX_JsonGetString(language,"tips.smarttranslation"));
	PX_Object_PushButtonSetTips(controller->button_rotation,PX_JsonGetString(language,"tips.rotation"));
	PX_Object_PushButtonSetTips(controller->button_scale,PX_JsonGetString(language,"tips.stretch"));
	PX_Object_PushButtonSetTips(controller->button_vertices,PX_JsonGetString(language,"tips.verticeseditor"));
	PX_Object_PushButtonSetTips(controller->button_globalrotation,PX_JsonGetString(language,"tips.globalrotation"));
	PX_Object_PushButtonSetTips(controller->button_texture,PX_JsonGetString(language,"tips.switchtexture"));
	PX_Object_PushButtonSetTips(controller->button_impulse,PX_JsonGetString(language,"tips.impulse"));
	PX_Object_PushButtonSetTips(controller->button_panc,PX_JsonGetString(language,"tips.pan"));
	PX_ObjectRegisterEvent(controller->button_translation,PX_OBJECT_EVENT_EXECUTE,PX_LiveEditorModule_RealtimeAnimationControllerOnTool,object);
	PX_ObjectRegisterEvent(controller->button_rotation,PX_OBJECT_EVENT_EXECUTE,PX_LiveEditorModule_RealtimeAnimationControllerOnTool,object);
	PX_ObjectRegisterEvent(controller->button_scale,PX_OBJECT_EVENT_EXECUTE,PX_LiveEditorModule_RealtimeAnimationControllerOnTool,object);
	PX_ObjectRegisterEvent(controller->button_vertices,PX_OBJECT_EVENT_EXECUTE,PX_LiveEditorModule_RealtimeAnimationControllerOnTool,object);
	PX_ObjectRegisterEvent(controller->button_globalrotation,PX_OBJECT_EVENT_EXECUTE,PX_LiveEditorModule_RealtimeAnimationControllerOnTool,object);
	PX_ObjectRegisterEvent(controller->button_texture,PX_OBJECT_EVENT_EXECUTE,PX_LiveEditorModule_RealtimeAnimationControllerOnTool,object);
	PX_ObjectRegisterEvent(controller->button_impulse,PX_OBJECT_EVENT_EXECUTE,PX_LiveEditorModule_RealtimeAnimationControllerOnTool,object);
	PX_ObjectRegisterEvent(controller->button_panc,PX_OBJECT_EVENT_EXECUTE,PX_LiveEditorModule_RealtimeAnimationControllerOnTool,object);
	PX_ObjectRegisterEvent(controller->button_translation,PX_OBJECT_EVENT_CURSOROVER,PX_LiveEditorModule_RealtimeAnimationControllerOnHover,object);
	PX_ObjectRegisterEvent(controller->button_rotation,PX_OBJECT_EVENT_CURSOROVER,PX_LiveEditorModule_RealtimeAnimationControllerOnHover,object);
	PX_ObjectRegisterEvent(controller->button_scale,PX_OBJECT_EVENT_CURSOROVER,PX_LiveEditorModule_RealtimeAnimationControllerOnHover,object);
	PX_ObjectRegisterEvent(controller->button_globalrotation,PX_OBJECT_EVENT_CURSOROVER,PX_LiveEditorModule_RealtimeAnimationControllerOnHover,object);
	PX_ObjectRegisterEvent(controller->button_texture,PX_OBJECT_EVENT_CURSOROVER,PX_LiveEditorModule_RealtimeAnimationControllerOnHover,object);
	PX_ObjectRegisterEvent(controller->button_vertices,PX_OBJECT_EVENT_CURSOROVER,PX_LiveEditorModule_RealtimeAnimationControllerOnHover,object);
	PX_ObjectRegisterEvent(controller->button_impulse,PX_OBJECT_EVENT_CURSOROVER,PX_LiveEditorModule_RealtimeAnimationControllerOnHover,object);
	PX_ObjectRegisterEvent(controller->button_panc,PX_OBJECT_EVENT_CURSOROVER,PX_LiveEditorModule_RealtimeAnimationControllerOnHover,object);
	PX_ObjectRegisterEvent(controller->button_translation,PX_OBJECT_EVENT_CURSOROUT,PX_LiveEditorModule_RealtimeAnimationControllerOnHover,object);
	PX_ObjectRegisterEvent(controller->button_rotation,PX_OBJECT_EVENT_CURSOROUT,PX_LiveEditorModule_RealtimeAnimationControllerOnHover,object);
	PX_ObjectRegisterEvent(controller->button_scale,PX_OBJECT_EVENT_CURSOROUT,PX_LiveEditorModule_RealtimeAnimationControllerOnHover,object);
	PX_ObjectRegisterEvent(controller->button_globalrotation,PX_OBJECT_EVENT_CURSOROUT,PX_LiveEditorModule_RealtimeAnimationControllerOnHover,object);
	PX_ObjectRegisterEvent(controller->button_texture,PX_OBJECT_EVENT_CURSOROUT,PX_LiveEditorModule_RealtimeAnimationControllerOnHover,object);
	PX_ObjectRegisterEvent(controller->button_vertices,PX_OBJECT_EVENT_CURSOROUT,PX_LiveEditorModule_RealtimeAnimationControllerOnHover,object);
	PX_ObjectRegisterEvent(controller->button_impulse,PX_OBJECT_EVENT_CURSOROUT,PX_LiveEditorModule_RealtimeAnimationControllerOnHover,object);
	PX_ObjectRegisterEvent(controller->button_panc,PX_OBJECT_EVENT_CURSOROUT,PX_LiveEditorModule_RealtimeAnimationControllerOnHover,object);
	controller->transformEditor=PX_LiveEditorModule_RealtimeTransformInstall(parent,pruntime,accessor);
	controller->vertexEditor=PX_LiveEditorModule_RealtimeVertexTransformInstall(parent,pruntime,accessor);
	controller->textureEditor=PX_LiveEditorModule_RealtimeTextureInstall(parent,pruntime,fm,accessor,language);
	controller->impulseEditor=PX_LiveEditorModule_RealtimeImpulseInstall(parent,pruntime,accessor);
	controller->pancEditor=PX_LiveEditorModule_RealtimePancInstall(parent,pruntime,fm,accessor,language);
	if (!controller->transformEditor || !controller->vertexEditor || !controller->textureEditor || !controller->impulseEditor || !controller->pancEditor)
	{
		PX_ObjectDelete(object);
		return PX_NULL;
	}
	object->Enabled=PX_FALSE;
	object->Visible=PX_FALSE;
	return object;
}

px_void PX_LiveEditorModule_RealtimeAnimationControllerEnable(PX_Object *object)
{
	PX_LiveEditorModule_RealtimeAnimationController *controller;
	if (!object) return;
	controller=(PX_LiveEditorModule_RealtimeAnimationController *)object->pObjectDesc[0];
	object->Enabled=PX_TRUE;
	object->Visible=PX_TRUE;
	PX_LiveEditorModule_RealtimeAnimationControllerSnapshotContext(controller);
	PX_LiveEditorModule_RealtimeAnimationControllerDisableEditors(controller);
	if (controller->editable) PX_LiveEditorModule_RealtimeTransformEnable(controller->transformEditor,PX_LIVEEDITOR_REALTIME_TRANSFORM_TRANSLATION);
	PX_LiveEditorModule_RealtimeAnimationControllerSetFocus(controller,PX_LIVEEDITOR_REALTIME_TOOL_TRANSLATION);
}

px_void PX_LiveEditorModule_RealtimeAnimationControllerDisable(PX_Object *object)
{
	PX_LiveEditorModule_RealtimeAnimationController *controller;
	if (!object) return;
	controller=(PX_LiveEditorModule_RealtimeAnimationController *)object->pObjectDesc[0];
	PX_LiveEditorModule_RealtimeAnimationControllerDisableEditors(controller);
	object->Enabled=PX_FALSE;
	object->Visible=PX_FALSE;
}
