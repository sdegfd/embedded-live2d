#include "PainterEngine_LiveEditorModules_RealtimeController.h"

PX_OBJECT_FREE_FUNCTION(PX_LiveEditorModule_RealtimeControllerFree)
{
	PX_LiveEditorModule_RealtimeController *controller=(PX_LiveEditorModule_RealtimeController *)pObject->pObjectDesc[0];
	PX_LiveEditorRealtimePoseAccessorFree(&controller->accessor);
}

static px_void PX_LiveEditorModule_RealtimeControllerRefresh(PX_Object *object)
{
	PX_LiveEditorModule_RealtimeController *controller=(PX_LiveEditorModule_RealtimeController *)object->pObjectDesc[0];
	px_int i,selected=PX_LiveEditorRealtimePoseAccessorGetSelectedAxis(&controller->accessor);
	controller->syncingAxisSelect=PX_TRUE;
	PX_Object_SelectBarClear(controller->axis_selectbar);
	for (i=0;i<PX_LiveEditorRealtimePoseAccessorGetAxisCount(&controller->accessor);i++)
	{
		PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(&controller->accessor,i);
		if (axis) PX_Object_SelectBarAddItem(controller->axis_selectbar,axis->id);
	}
	if (selected>=0) PX_Object_SelectBarSetCurrentIndex(controller->axis_selectbar,selected);
	controller->axis_selectbar->Enabled=PX_LiveEditorRealtimePoseAccessorGetAxisCount(&controller->accessor)>0;
	controller->syncingAxisSelect=PX_FALSE;
	controller->button_delete->Enabled=selected>=0;
}

static px_void PX_LiveEditorModule_RealtimeControllerOnAxisChanged(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_Object *controllerObject=(PX_Object *)ptr;
	PX_LiveEditorModule_RealtimeController *controller=(PX_LiveEditorModule_RealtimeController *)controllerObject->pObjectDesc[0];
	if (!controller->syncingAxisSelect)
	{
		PX_LiveEditorRealtimePoseAccessorSelectAxis(&controller->accessor,PX_Object_SelectBarGetCurrentIndex(controller->axis_selectbar));
	}
}

static px_void PX_LiveEditorModule_RealtimeControllerOnAxisIdConfirm(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_Object *controllerObject=(PX_Object *)ptr;
	PX_LiveEditorModule_RealtimeController *controller=(PX_LiveEditorModule_RealtimeController *)controllerObject->pObjectDesc[0];
	const px_char *id=PX_Object_MessageBoxGetInput(pObject);
	if (!PX_LiveEditorRealtimePoseAccessorCreateAxis(&controller->accessor,id))
	{
		PX_Object_MessageBoxAlertOk(controller->messagebox,PX_JsonGetString(controller->language,"realtime.create failed"),PX_NULL,PX_NULL);
	}
}

static px_void PX_LiveEditorModule_RealtimeControllerOnNew(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_Object *controllerObject=(PX_Object *)ptr;
	PX_LiveEditorModule_RealtimeController *controller=(PX_LiveEditorModule_RealtimeController *)controllerObject->pObjectDesc[0];
	PX_Object_MessageBoxInputBox(controller->messagebox,PX_JsonGetString(controller->language,"realtime.axis id"),PX_LiveEditorModule_RealtimeControllerOnAxisIdConfirm,controllerObject,PX_NULL,PX_NULL);
}

static px_void PX_LiveEditorModule_RealtimeControllerOnDelete(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_Object *controllerObject=(PX_Object *)ptr;
	PX_LiveEditorModule_RealtimeController *controller=(PX_LiveEditorModule_RealtimeController *)controllerObject->pObjectDesc[0];
	if (!PX_LiveEditorRealtimePoseAccessorDeleteSelectedAxis(&controller->accessor))
	{
		PX_Object_MessageBoxAlertOk(controller->messagebox,PX_JsonGetString(controller->language,"realtime.delete failed"),PX_NULL,PX_NULL);
	}
}

static px_void PX_LiveEditorModule_RealtimeControllerOnReload(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_LiveEditorModule_RealtimeControllerReload((PX_Object *)ptr);
}

PX_OBJECT_UPDATE_FUNCTION(PX_LiveEditorModule_RealtimeControllerUpdate)
{
	PX_LiveEditorModule_RealtimeController *controller=(PX_LiveEditorModule_RealtimeController *)pObject->pObjectDesc[0];
	if (controller->seenRevision!=controller->accessor.revision)
	{
		controller->seenRevision=controller->accessor.revision;
		PX_LiveEditorModule_RealtimeControllerRefresh(pObject);
	}
}

PX_OBJECT_RENDER_FUNCTION(PX_LiveEditorModule_RealtimeControllerRender)
{
	px_float inheritX,inheritY,x,y;
	PX_ObjectGetInheritXY(pObject,&inheritX,&inheritY);
	x=pObject->x+inheritX;
	y=pObject->y+inheritY;
	PX_GeoDrawRect(psurface,(px_int)x,(px_int)y,(px_int)(x+pObject->Width-1),(px_int)(y+pObject->Height-1),PX_OBJECT_UI_DEFAULT_BACKGROUNDCOLOR);
}

PX_Object *PX_LiveEditorModule_RealtimeControllerInstall(PX_Object *panelWidget,PX_Object *overlayParent,PX_Runtime *pruntime,PX_FontModule *fm,PX_LiveFramework *plive,PX_Json *language)
{
	PX_Object *object;
	PX_LiveEditorModule_RealtimeController desc,*controller;
	px_int contentHeight;
	if (!panelWidget || !overlayParent) return PX_NULL;
	contentHeight=PX_Object_WidgetGetRenderTargetHeight(panelWidget)-24;
	if (contentHeight<246) return PX_NULL;
	PX_memset(&desc,0,sizeof(desc));
	desc.pruntime=pruntime;
	desc.fontmodule=fm;
	desc.language=language;
	desc.plive=plive;
	desc.seenRevision=(px_dword)-1;
	object=PX_ObjectCreateEx(&pruntime->mp_dynamic,PX_Object_WidgetGetRoot(panelWidget),0,24,0,256,(px_float)contentHeight,0,0,PX_LiveEditorModule_RealtimeControllerUpdate,PX_LiveEditorModule_RealtimeControllerRender,PX_LiveEditorModule_RealtimeControllerFree,&desc,sizeof(desc));
	if (!object) return PX_NULL;
	controller=(PX_LiveEditorModule_RealtimeController *)object->pObjectDesc[0];
	if (!PX_LiveEditorRealtimePoseAccessorInitialize(&controller->accessor,&pruntime->mp_dynamic,plive))
	{
		PX_ObjectDelete(object);
		return PX_NULL;
	}
	controller->messagebox=PX_Object_MessageBoxCreate(&pruntime->mp_dynamic,overlayParent,fm);
	if (!controller->messagebox)
	{
		PX_ObjectDelete(object);
		return PX_NULL;
	}
	controller->button_new=PX_Object_PushButtonCreate(&pruntime->mp_dynamic,object,4,4,76,24,PX_JsonGetString(language,"realtime.new axis"),fm);
	controller->button_delete=PX_Object_PushButtonCreate(&pruntime->mp_dynamic,object,82,4,76,24,PX_JsonGetString(language,"realtime.delete axis"),fm);
	controller->button_reload=PX_Object_PushButtonCreate(&pruntime->mp_dynamic,object,160,4,92,24,PX_JsonGetString(language,"realtime.reload"),fm);
	PX_ObjectRegisterEvent(controller->button_new,PX_OBJECT_EVENT_EXECUTE,PX_LiveEditorModule_RealtimeControllerOnNew,object);
	PX_ObjectRegisterEvent(controller->button_delete,PX_OBJECT_EVENT_EXECUTE,PX_LiveEditorModule_RealtimeControllerOnDelete,object);
	PX_ObjectRegisterEvent(controller->button_reload,PX_OBJECT_EVENT_EXECUTE,PX_LiveEditorModule_RealtimeControllerOnReload,object);
	controller->axis_editor=PX_LiveEditorModule_RealtimeAxisEditorInstall(object,pruntime,fm,&controller->accessor,language,controller->messagebox,0,60);
	controller->mixer=PX_LiveEditorModule_RealtimeMixerInstall(object,pruntime,fm,&controller->accessor,language,0,176);
	if (!controller->axis_editor || !controller->mixer)
	{
		PX_Object *messagebox=controller->messagebox;
		PX_ObjectDelete(object);
		PX_ObjectDelete(messagebox);
		return PX_NULL;
	}
	controller->label_axis=PX_Object_LabelCreate(&pruntime->mp_dynamic,object,6,34,34,20,PX_JsonGetString(language,"realtime.axis"),fm,PX_OBJECT_UI_DEFAULT_FONTCOLOR);
	/* Create the selector after its submodules so the expanded item list is
	   rendered above both the axis editor and mixer. */
	controller->axis_selectbar=PX_Object_SelectBarCreate(&pruntime->mp_dynamic,object,42,32,208,24,fm);
	PX_Object_SelectBarSetMaxDisplayCount(controller->axis_selectbar,7);
	PX_ObjectRegisterEvent(controller->axis_selectbar,PX_OBJECT_EVENT_VALUECHANGED,PX_LiveEditorModule_RealtimeControllerOnAxisChanged,object);
	object->Enabled=PX_FALSE;
	object->Visible=PX_FALSE;
	return object;
}

px_void PX_LiveEditorModule_RealtimeControllerUninstall(PX_Object *object)
{
	PX_LiveEditorModule_RealtimeController *controller;
	PX_Object *messagebox;
	if (!object) return;
	controller=(PX_LiveEditorModule_RealtimeController *)object->pObjectDesc[0];
	messagebox=controller->messagebox;
	PX_ObjectDelete(object);
	if (messagebox) PX_ObjectDelete(messagebox);
}

px_void PX_LiveEditorModule_RealtimeControllerEnable(PX_Object *object)
{
	PX_LiveEditorModule_RealtimeController *controller;
	if (!object) return;
	controller=(PX_LiveEditorModule_RealtimeController *)object->pObjectDesc[0];
	object->Enabled=PX_TRUE;
	object->Visible=PX_TRUE;
	controller->plive->showKeypoint=PX_TRUE;
	controller->plive->showFocusLayer=PX_LiveEditorRealtimePoseAccessorGetSelectedLayer(&controller->accessor)>=0;
	if (PX_LiveEditorRealtimePoseAccessorGetSelectedAxis(&controller->accessor)>=0)
		PX_LiveEditorRealtimePoseAccessorApplySelectedKey(&controller->accessor);
	else
		PX_LiveEditorRealtimePoseAccessorEnter(&controller->accessor);
}

px_void PX_LiveEditorModule_RealtimeControllerDisable(PX_Object *object)
{
	PX_LiveEditorModule_RealtimeController *controller;
	if (!object) return;
	controller=(PX_LiveEditorModule_RealtimeController *)object->pObjectDesc[0];
	PX_LiveEditorRealtimePoseAccessorLeave(&controller->accessor);
	object->Enabled=PX_FALSE;
	object->Visible=PX_FALSE;
}

px_void PX_LiveEditorModule_RealtimeControllerReload(PX_Object *object)
{
	PX_LiveEditorModule_RealtimeController *controller;
	px_bool wasEnabled;
	if (!object) return;
	controller=(PX_LiveEditorModule_RealtimeController *)object->pObjectDesc[0];
	wasEnabled=object->Enabled;
	PX_LiveEditorRealtimePoseAccessorLeave(&controller->accessor);
	PX_LiveEditorRealtimePoseAccessorReload(&controller->accessor);
	if (wasEnabled)
	{
		if (PX_LiveEditorRealtimePoseAccessorGetSelectedAxis(&controller->accessor)>=0)
			PX_LiveEditorRealtimePoseAccessorApplySelectedKey(&controller->accessor);
		else
			PX_LiveEditorRealtimePoseAccessorEnter(&controller->accessor);
	}
}

PX_LiveEditorRealtimePoseAccessor *PX_LiveEditorModule_RealtimeControllerGetPoseAccessor(PX_Object *object)
{
	if (!object) return PX_NULL;
	return &((PX_LiveEditorModule_RealtimeController *)object->pObjectDesc[0])->accessor;
}
