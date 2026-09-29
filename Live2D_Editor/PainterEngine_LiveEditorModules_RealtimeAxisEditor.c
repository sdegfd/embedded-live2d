#include "PainterEngine_LiveEditorModules_RealtimeAxisEditor.h"

static px_void PX_LiveEditorModule_RealtimeAxisEditorAlert(PX_LiveEditorModule_RealtimeAxisEditor *editor,const px_char text[])
{
	PX_Object_MessageBoxAlertOk(editor->messagebox,text,PX_NULL,PX_NULL);
}

static px_int PX_LiveEditorModule_RealtimeAxisEditorGetDefaultSlot(const PX_LiveEditorRealtimeAxisAuthoring *axis)
{
	if (!axis || axis->defaultSampleIndex==0) return 0;
	if (axis->defaultSampleIndex==PX_LIVE_REALTIME_SAMPLE_COUNT-1) return 2;
	return 1;
}

static px_void PX_LiveEditorModule_RealtimeAxisEditorOnMiddle(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_Object *editorObject=(PX_Object *)ptr;
	PX_LiveEditorModule_RealtimeAxisEditor *editor=(PX_LiveEditorModule_RealtimeAxisEditor *)editorObject->pObjectDesc[0];
	if (!editor->syncing) PX_LiveEditorRealtimePoseAccessorSetMiddle(editor->accessor,PX_Object_SliderBarGetValue(editor->slider_middle));
}

static px_void PX_LiveEditorModule_RealtimeAxisEditorOnKey(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_Object *editorObject=(PX_Object *)ptr;
	PX_LiveEditorModule_RealtimeAxisEditor *editor=(PX_LiveEditorModule_RealtimeAxisEditor *)editorObject->pObjectDesc[0];
	PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(editor->accessor,PX_LiveEditorRealtimePoseAccessorGetSelectedAxis(editor->accessor));
	px_int key;
	for (key=0;key<3;key++) if (pObject==editor->button_key[key]) break;
	if (key<3)
	{
		if (axis && axis->selectedKeySlot==key) PX_LiveEditorRealtimePoseAccessorApplySelectedKey(editor->accessor);
		else PX_LiveEditorRealtimePoseAccessorSelectKeySlot(editor->accessor,key);
	}
}

static px_void PX_LiveEditorModule_RealtimeAxisEditorOnDefault(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_Object *editorObject=(PX_Object *)ptr;
	PX_LiveEditorModule_RealtimeAxisEditor *editor=(PX_LiveEditorModule_RealtimeAxisEditor *)editorObject->pObjectDesc[0];
	PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(editor->accessor,PX_LiveEditorRealtimePoseAccessorGetSelectedAxis(editor->accessor));
	px_int offset,current;
	if (!axis) return;
	current=PX_LiveEditorModule_RealtimeAxisEditorGetDefaultSlot(axis);
	for (offset=1;offset<3;offset++) if (PX_LiveEditorRealtimePoseAccessorSetDefaultKeySlot(editor->accessor,(current+offset)%3)) return;
	PX_LiveEditorModule_RealtimeAxisEditorAlert(editor,PX_JsonGetString(editor->language,"realtime.default nonneutral"));
}

static px_void PX_LiveEditorModule_RealtimeAxisEditorOnCopyDefault(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_Object *editorObject=(PX_Object *)ptr;
	PX_LiveEditorModule_RealtimeAxisEditor *editor=(PX_LiveEditorModule_RealtimeAxisEditor *)editorObject->pObjectDesc[0];
	if (!PX_LiveEditorRealtimePoseAccessorCopyDefaultToSelectedKey(editor->accessor))
		PX_LiveEditorModule_RealtimeAxisEditorAlert(editor,PX_JsonGetString(editor->language,"realtime.authoring unavailable"));
}

static px_void PX_LiveEditorModule_RealtimeAxisEditorOnBake(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_Object *editorObject=(PX_Object *)ptr;
	PX_LiveEditorModule_RealtimeAxisEditor *editor=(PX_LiveEditorModule_RealtimeAxisEditor *)editorObject->pObjectDesc[0];
	if (!PX_LiveEditorRealtimePoseAccessorBakeSelectedAxis(editor->accessor))
		PX_LiveEditorModule_RealtimeAxisEditorAlert(editor,PX_JsonGetString(editor->language,"realtime.bake failed"));
}

PX_OBJECT_UPDATE_FUNCTION(PX_LiveEditorModule_RealtimeAxisEditorUpdate)
{
	PX_LiveEditorModule_RealtimeAxisEditor *editor=(PX_LiveEditorModule_RealtimeAxisEditor *)pObject->pObjectDesc[0];
	PX_LiveEditorRealtimeAxisAuthoring *axis;
	px_char text[128];
	px_int key,defaultSlot=0;
	px_bool isDefault=PX_FALSE;
	if (editor->seenRevision==editor->accessor->revision) return;
	editor->seenRevision=editor->accessor->revision;
	axis=PX_LiveEditorRealtimePoseAccessorGetAxis(editor->accessor,PX_LiveEditorRealtimePoseAccessorGetSelectedAxis(editor->accessor));
	editor->syncing=PX_TRUE;
	if (!axis)
	{
		PX_Object_LabelSetText(editor->label_status,PX_JsonGetString(editor->language,"realtime.no axis"));
		editor->slider_middle->Enabled=PX_FALSE;
		for (key=0;key<3;key++) editor->button_key[key]->Enabled=PX_FALSE;
		editor->button_default->Enabled=PX_FALSE;
		editor->button_copydefault->Enabled=PX_FALSE;
		editor->button_bake->Enabled=PX_FALSE;
		editor->syncing=PX_FALSE;
		return;
	}
	defaultSlot=PX_LiveEditorModule_RealtimeAxisEditorGetDefaultSlot(axis);
	isDefault=axis->selectedKeySlot==defaultSlot;
	editor->slider_middle->Enabled=PX_TRUE;
	PX_Object_SliderBarSetValue(editor->slider_middle,axis->middleKeyIndex);
	for (key=0;key<3;key++)
	{
		editor->button_key[key]->Enabled=PX_TRUE;
		PX_Object_PushButtonSetBackgroundColor(editor->button_key[key],PX_OBJECT_UI_DEFAULT_BACKGROUNDCOLOR);
	}
	if (editor->accessor->authoringPoseActive && axis->selectedKeySlot<3) PX_Object_PushButtonSetBackgroundColor(editor->button_key[axis->selectedKeySlot],PX_COLOR_FORCECOLOR);
	editor->button_default->Enabled=PX_TRUE;
	editor->button_copydefault->Enabled=editor->accessor->authoringPoseActive&&axis->authoringAvailable&&!isDefault;
	editor->button_bake->Enabled=axis->authoringAvailable;
	PX_sprintf1(text,sizeof(text),"KM:%1",PX_STRINGFORMAT_INT(axis->middleKeyIndex));
	PX_Object_PushButtonSetText(editor->button_key[1],text);
	PX_sprintf1(text,sizeof(text),"%1:K0",PX_STRINGFORMAT_STRING(PX_JsonGetString(editor->language,"realtime.default")));
	if (defaultSlot==1) PX_sprintf1(text,sizeof(text),"%1:KM",PX_STRINGFORMAT_STRING(PX_JsonGetString(editor->language,"realtime.default")));
	if (defaultSlot==2) PX_sprintf1(text,sizeof(text),"%1:K29",PX_STRINGFORMAT_STRING(PX_JsonGetString(editor->language,"realtime.default")));
	PX_Object_PushButtonSetText(editor->button_default,text);
	PX_Object_LabelSetTextColor(editor->label_status,axis->dirty?PX_COLOR(255,128,0,0):PX_OBJECT_UI_DEFAULT_FONTCOLOR);
	if (!axis->authoringAvailable) PX_Object_LabelSetText(editor->label_status,PX_JsonGetString(editor->language,"realtime.baked only"));
	else if (!editor->accessor->authoringPoseActive && axis->baked && !axis->dirty)
	{
		PX_sprintf2(text,sizeof(text),"%1:%2",PX_STRINGFORMAT_STRING(PX_JsonGetString(editor->language,"realtime.preview")),PX_STRINGFORMAT_INT(axis->sampleIndex));
		PX_Object_LabelSetText(editor->label_status,text);
	}
	else if (isDefault) PX_Object_LabelSetText(editor->label_status,PX_JsonGetString(editor->language,"realtime.default neutral"));
	else if (axis->dirty) PX_Object_LabelSetText(editor->label_status,PX_JsonGetString(editor->language,"realtime.bake stale"));
	else if (axis->meshDegenerate||axis->meshFlipped||axis->meshUvOutside)
	{
		PX_sprintf2(text,sizeof(text),"%1 %2",PX_STRINGFORMAT_STRING(PX_JsonGetString(editor->language,"realtime.mesh defects")),PX_STRINGFORMAT_INT(axis->meshDegenerate+axis->meshFlipped+axis->meshUvOutside));
		PX_Object_LabelSetText(editor->label_status,text);
	}
	else PX_Object_LabelSetText(editor->label_status,PX_JsonGetString(editor->language,"realtime.baked"));
	editor->syncing=PX_FALSE;
}

PX_OBJECT_RENDER_FUNCTION(PX_LiveEditorModule_RealtimeAxisEditorRender)
{
	px_float x,y;
	PX_ObjectGetInheritXY(pObject,&x,&y);
	x+=pObject->x; y+=pObject->y;
	PX_GeoDrawRect(psurface,(px_int)x,(px_int)y,(px_int)(x+pObject->Width-1),(px_int)(y+pObject->Height-1),PX_OBJECT_UI_DEFAULT_BACKGROUNDCOLOR);
	PX_GeoDrawBorder(psurface,(px_int)x,(px_int)y,(px_int)(x+pObject->Width-1),(px_int)(y+pObject->Height-1),1,PX_OBJECT_UI_DEFAULT_BORDERCOLOR);
}

PX_Object *PX_LiveEditorModule_RealtimeAxisEditorInstall(PX_Object *parent,PX_Runtime *pruntime,PX_FontModule *fm,PX_LiveEditorRealtimePoseAccessor *accessor,PX_Json *language,PX_Object *messagebox,px_int x,px_int y)
{
	PX_Object *object;
	PX_LiveEditorModule_RealtimeAxisEditor desc,*editor;
	PX_memset(&desc,0,sizeof(desc));
	desc.accessor=accessor; desc.fontmodule=fm; desc.language=language; desc.messagebox=messagebox; desc.seenRevision=(px_dword)-1;
	object=PX_ObjectCreateEx(&pruntime->mp_dynamic,parent,(px_float)x,(px_float)y,0,256,112,0,0,PX_LiveEditorModule_RealtimeAxisEditorUpdate,PX_LiveEditorModule_RealtimeAxisEditorRender,PX_NULL,&desc,sizeof(desc));
	if (!object) return PX_NULL;
	editor=(PX_LiveEditorModule_RealtimeAxisEditor *)object->pObjectDesc[0];
	editor->label_middle=PX_Object_LabelCreate(&pruntime->mp_dynamic,object,6,5,34,22,PX_JsonGetString(language,"realtime.middle"),fm,PX_OBJECT_UI_DEFAULT_FONTCOLOR);
	editor->slider_middle=PX_Object_SliderBarCreate(&pruntime->mp_dynamic,object,42,4,124,22,PX_OBJECT_SLIDERBAR_TYPE_HORIZONTAL,PX_OBJECT_SLIDERBAR_STYLE_BOX);
	PX_Object_SliderBarSetRange(editor->slider_middle,1,28);
	PX_Object_SliderBarSetShowValue(editor->slider_middle,PX_TRUE,PX_OBJECT_UI_DEFAULT_FONTCOLOR);
	PX_ObjectRegisterEvent(editor->slider_middle,PX_OBJECT_EVENT_VALUECHANGED,PX_LiveEditorModule_RealtimeAxisEditorOnMiddle,object);
	editor->button_default=PX_Object_PushButtonCreate(&pruntime->mp_dynamic,object,170,4,80,22,"",fm);
	PX_ObjectRegisterEvent(editor->button_default,PX_OBJECT_EVENT_EXECUTE,PX_LiveEditorModule_RealtimeAxisEditorOnDefault,object);
	editor->button_key[0]=PX_Object_PushButtonCreate(&pruntime->mp_dynamic,object,6,30,77,22,"K0",fm);
	editor->button_key[1]=PX_Object_PushButtonCreate(&pruntime->mp_dynamic,object,87,30,77,22,"KM",fm);
	editor->button_key[2]=PX_Object_PushButtonCreate(&pruntime->mp_dynamic,object,168,30,82,22,"K29",fm);
	for (x=0;x<3;x++) PX_ObjectRegisterEvent(editor->button_key[x],PX_OBJECT_EVENT_EXECUTE,PX_LiveEditorModule_RealtimeAxisEditorOnKey,object);
	editor->button_copydefault=PX_Object_PushButtonCreate(&pruntime->mp_dynamic,object,6,56,118,22,PX_JsonGetString(language,"realtime.copy"),fm);
	editor->button_bake=PX_Object_PushButtonCreate(&pruntime->mp_dynamic,object,128,56,122,22,PX_JsonGetString(language,"realtime.bake"),fm);
	PX_ObjectRegisterEvent(editor->button_copydefault,PX_OBJECT_EVENT_EXECUTE,PX_LiveEditorModule_RealtimeAxisEditorOnCopyDefault,object);
	PX_ObjectRegisterEvent(editor->button_bake,PX_OBJECT_EVENT_EXECUTE,PX_LiveEditorModule_RealtimeAxisEditorOnBake,object);
	editor->label_status=PX_Object_LabelCreate(&pruntime->mp_dynamic,object,6,84,244,18,"",fm,PX_OBJECT_UI_DEFAULT_FONTCOLOR);
	return object;
}
