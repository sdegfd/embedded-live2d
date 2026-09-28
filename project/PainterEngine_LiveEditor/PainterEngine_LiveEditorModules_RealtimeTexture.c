#include "PainterEngine_LiveEditorModules_RealtimeTexture.h"
#include "PainterEngine_LiveEditorModules_View.h"

static px_point2D PX_LiveEditorModule_RealtimeTextureVisualPivot(PX_LiveFramework *plive,PX_LiveLayer *layer)
{
	px_point point=layer->currentKeyPoint;
	PX_LiveLayer *current=layer;
	while(current)
	{
		px_point relative=PX_PointSub(point,current->currentKeyPoint);
		px_point translation=PX_PointRotate(current->rel_currentLocalTranslation,current->rel_currentRotationAngle);
		relative=PX_PointRotate(relative,current->rel_currentLocalRotationAngle);
		relative=PX_PointMul(relative,current->rel_currentLocalScale);
		point=PX_PointAdd(PX_PointAdd(relative,current->currentKeyPoint),translation);
		current=PX_LiveFrameworkGetLayerParent(plive,current);
	}
	return PX_POINT2D(point.x,point.y);
}

static px_void PX_LiveEditorModule_RealtimeTextureOnCancel(PX_Object *button,PX_Object_Event e,px_void *ptr)
{
	PX_Object *object=(PX_Object *)ptr;
	PX_LiveEditorModule_RealtimeTexture *editor=(PX_LiveEditorModule_RealtimeTexture *)object->pObjectDesc[0];
	editor->dialog->Visible=PX_FALSE;
	editor->dialog->Enabled=PX_FALSE;
	editor->plive->showKeypoint=PX_TRUE;
	PX_ObjectReleaseFocus(editor->dialog);
}

static px_void PX_LiveEditorModule_RealtimeTextureOnOk(PX_Object *button,PX_Object_Event e,px_void *ptr)
{
	PX_Object *object=(PX_Object *)ptr;
	PX_LiveEditorModule_RealtimeTexture *editor=(PX_LiveEditorModule_RealtimeTexture *)object->pObjectDesc[0];
	const px_char *id=PX_Object_EditGetText(editor->editTexture);
	px_bool clear=PX_strequ(id,"Null")||!id[0];
	px_int index=clear?-1:PX_LiveFrameworkGetLiveTextureIndexById(editor->plive,id);
	if (clear||index>=0) PX_LiveEditorRealtimePoseAccessorSetSelectedLayerTexture(editor->accessor,index);
	PX_LiveEditorModule_RealtimeTextureOnCancel(button,e,ptr);
}

static px_void PX_LiveEditorModule_RealtimeTextureOnRightDown(PX_Object *object,PX_Object_Event e,px_void *ptr)
{
	PX_LiveEditorModule_RealtimeTexture *editor=(PX_LiveEditorModule_RealtimeTexture *)object->pObjectDesc[0];
	editor->dialog->Visible=PX_FALSE;
	editor->dialog->Enabled=PX_FALSE;
	editor->plive->showKeypoint=PX_TRUE;
	editor->plive->showFocusLayer=PX_FALSE;
	editor->plive->currentEditLayerIndex=-1;
}

static px_void PX_LiveEditorModule_RealtimeTextureOnDown(PX_Object *object,PX_Object_Event e,px_void *ptr)
{
	PX_LiveEditorModule_RealtimeTexture *editor=(PX_LiveEditorModule_RealtimeTexture *)object->pObjectDesc[0];
	px_point2D model;
	px_int i;
	if (!PX_LiveEditorRealtimePoseAccessorIsSelectedKeyEditable(editor->accessor)||editor->dialog->Visible) return;
	model=PX_LiveEditorViewScreenToModel(editor->plive,PX_POINT2D(PX_Object_Event_GetCursorX(e),PX_Object_Event_GetCursorY(e)));
	for (i=0;i<editor->plive->layers.size;i++)
	{
		PX_LiveLayer *layer=PX_LiveFrameworkGetLayer(editor->plive,i);
		px_point2D pivot=PX_LiveEditorModule_RealtimeTextureVisualPivot(editor->plive,layer);
		if (PX_isPoint2DInCircle(pivot,model,PX_LiveEditorViewModelMarkerHitRadius(editor->plive,5)))
		{
			px_int textureIndex;
			if (!PX_LiveEditorRealtimePoseAccessorSelectLayer(editor->accessor,i)||!PX_LiveEditorRealtimePoseAccessorGetSelectedLayerTexture(editor->accessor,&textureIndex)) return;
			if (textureIndex>=0&&textureIndex<editor->plive->livetextures.size)
			{
				PX_LiveTexture *texture=PX_VECTORAT(PX_LiveTexture,&editor->plive->livetextures,textureIndex);
				PX_Object_EditSetText(editor->editTexture,texture->id);
			}
			else PX_Object_EditSetText(editor->editTexture,"Null");
			editor->plive->showKeypoint=PX_FALSE;
			editor->plive->showFocusLayer=PX_TRUE;
			editor->dialog->Visible=PX_TRUE;
			editor->dialog->Enabled=PX_TRUE;
			PX_ObjectSetFocus(editor->dialog);
			return;
		}
	}
}

PX_Object *PX_LiveEditorModule_RealtimeTextureInstall(PX_Object *parent,PX_Runtime *pruntime,PX_FontModule *fm,PX_LiveEditorRealtimePoseAccessor *accessor,PX_Json *language)
{
	PX_LiveEditorModule_RealtimeTexture desc,*editor;
	PX_Object *object;
	PX_memset(&desc,0,sizeof(desc));
	desc.accessor=accessor;
	desc.plive=accessor->plive;
	desc.pruntime=pruntime;
	object=PX_ObjectCreateEx(&pruntime->mp_dynamic,parent,0,0,0,0,0,0,0,PX_NULL,PX_NULL,PX_NULL,&desc,sizeof(desc));
	if (!object) return PX_NULL;
	editor=(PX_LiveEditorModule_RealtimeTexture *)object->pObjectDesc[0];
	editor->dialog=PX_Object_WidgetCreate(&pruntime->mp_dynamic,object,pruntime->surface_width/2-128,pruntime->surface_height/2-64,256,128,"",PX_NULL);
	if (!editor->dialog) { PX_ObjectDelete(object); return PX_NULL; }
	PX_Object_WidgetShowHideCloseButton(editor->dialog,PX_FALSE);
	PX_Object_WidgetSetModel(editor->dialog,PX_TRUE);
	PX_Object_LabelCreate(&pruntime->mp_dynamic,editor->dialog,16,16,64,24,PX_JsonGetString(language,"livetextureeditor.textureid"),fm,PX_OBJECT_UI_DEFAULT_FONTCOLOR);
	editor->editTexture=PX_Object_EditCreate(&pruntime->mp_dynamic,editor->dialog,80,16,160,24,fm);
	PX_Object_EditSetMaxTextLength(editor->editTexture,16);
	editor->buttonOk=PX_Object_PushButtonCreate(&pruntime->mp_dynamic,editor->dialog,128,96,60,24,"Ok",fm);
	editor->buttonCancel=PX_Object_PushButtonCreate(&pruntime->mp_dynamic,editor->dialog,192,96,60,24,"Cancel",fm);
	PX_ObjectRegisterEvent(editor->buttonOk,PX_OBJECT_EVENT_EXECUTE,PX_LiveEditorModule_RealtimeTextureOnOk,object);
	PX_ObjectRegisterEvent(editor->buttonCancel,PX_OBJECT_EVENT_EXECUTE,PX_LiveEditorModule_RealtimeTextureOnCancel,object);
	PX_ObjectRegisterEvent(object,PX_OBJECT_EVENT_CURSORDOWN,PX_LiveEditorModule_RealtimeTextureOnDown,PX_NULL);
	PX_ObjectRegisterEvent(object,PX_OBJECT_EVENT_CURSORRDOWN,PX_LiveEditorModule_RealtimeTextureOnRightDown,PX_NULL);
	editor->dialog->Visible=PX_FALSE;
	editor->dialog->Enabled=PX_FALSE;
	object->Enabled=PX_FALSE;
	object->Visible=PX_FALSE;
	return object;
}

px_void PX_LiveEditorModule_RealtimeTextureEnable(PX_Object *object)
{
	PX_LiveEditorModule_RealtimeTexture *editor;
	if (!object) return;
	editor=(PX_LiveEditorModule_RealtimeTexture *)object->pObjectDesc[0];
	object->Enabled=PX_TRUE;
	object->Visible=PX_TRUE;
	editor->plive->showKeypoint=PX_TRUE;
	editor->plive->showFocusLayer=PX_FALSE;
}

px_void PX_LiveEditorModule_RealtimeTextureDisable(PX_Object *object)
{
	PX_LiveEditorModule_RealtimeTexture *editor;
	if (!object) return;
	editor=(PX_LiveEditorModule_RealtimeTexture *)object->pObjectDesc[0];
	editor->dialog->Visible=PX_FALSE;
	editor->dialog->Enabled=PX_FALSE;
	object->Enabled=PX_FALSE;
	object->Visible=PX_FALSE;
}
