#include "PainterEngine_LiveEditorModules_RealtimeImpulse.h"
#include "PainterEngine_LiveEditorModules_View.h"

static px_point2D PX_LiveEditorModule_RealtimeImpulseVisualPivot(PX_LiveFramework *plive,PX_LiveLayer *layer)
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

static px_void PX_LiveEditorModule_RealtimeImpulseSelectMode(PX_Object *object)
{
	PX_LiveEditorModule_RealtimeImpulse *editor=(PX_LiveEditorModule_RealtimeImpulse *)object->pObjectDesc[0];
	editor->editingLayer=-1;
	editor->plive->currentEditLayerIndex=-1;
	editor->plive->showKeypoint=PX_TRUE;
	editor->plive->showFocusLayer=PX_FALSE;
	PX_ObjectReleaseFocus(object);
}

static px_void PX_LiveEditorModule_RealtimeImpulseOnRightDown(PX_Object *object,PX_Object_Event e,px_void *ptr)
{
	PX_LiveEditorModule_RealtimeImpulseSelectMode(object);
}

static px_void PX_LiveEditorModule_RealtimeImpulseOnMove(PX_Object *object,PX_Object_Event e,px_void *ptr)
{
	PX_LiveEditorModule_RealtimeImpulse *editor=(PX_LiveEditorModule_RealtimeImpulse *)object->pObjectDesc[0];
	if (editor->editingLayer<0) return;
	editor->cursorModel=PX_LiveEditorViewScreenToModel(editor->plive,PX_POINT2D(PX_Object_Event_GetCursorX(e),PX_Object_Event_GetCursorY(e)));
}

static px_void PX_LiveEditorModule_RealtimeImpulseOnDown(PX_Object *object,PX_Object_Event e,px_void *ptr)
{
	PX_LiveEditorModule_RealtimeImpulse *editor=(PX_LiveEditorModule_RealtimeImpulse *)object->pObjectDesc[0];
	px_point2D model=PX_LiveEditorViewScreenToModel(editor->plive,PX_POINT2D(PX_Object_Event_GetCursorX(e),PX_Object_Event_GetCursorY(e)));
	px_int i;
	if (!PX_LiveEditorRealtimePoseAccessorIsSelectedKeyEditable(editor->accessor)) return;
	if (editor->editingLayer>=0)
	{
		PX_LiveEditorRealtimePoseAccessorSetSelectedLayerImpulse(editor->accessor,PX_POINT(model.x-editor->anchorModel.x,model.y-editor->anchorModel.y,0));
		PX_LiveEditorModule_RealtimeImpulseSelectMode(object);
		return;
	}
	for (i=0;i<editor->plive->layers.size;i++)
	{
		PX_LiveLayer *layer=PX_LiveFrameworkGetLayer(editor->plive,i);
		px_point2D pivot=PX_LiveEditorModule_RealtimeImpulseVisualPivot(editor->plive,layer);
		if (PX_isPoint2DInCircle(pivot,model,PX_LiveEditorViewModelMarkerHitRadius(editor->plive,5)))
		{
			if (!PX_LiveEditorRealtimePoseAccessorSelectLayer(editor->accessor,i)) return;
			editor->editingLayer=i;
			editor->anchorModel=pivot;
			editor->cursorModel=model;
			editor->plive->showKeypoint=PX_FALSE;
			editor->plive->showFocusLayer=PX_TRUE;
			PX_ObjectSetFocus(object);
			return;
		}
	}
}

PX_OBJECT_RENDER_FUNCTION(PX_LiveEditorModule_RealtimeImpulseRender)
{
	PX_LiveEditorModule_RealtimeImpulse *editor=(PX_LiveEditorModule_RealtimeImpulse *)pObject->pObjectDesc[0];
	if (editor->editingLayer>=0)
	{
		PX_GeoDrawArrow(psurface,PX_LiveEditorViewModelToScreen(editor->plive,editor->anchorModel),PX_LiveEditorViewModelToScreen(editor->plive,editor->cursorModel),1,PX_COLOR(255,0,0,255));
	}
	else
	{
		px_int oldLayer=PX_LiveEditorRealtimePoseAccessorGetSelectedLayer(editor->accessor),i;
		for (i=0;i<editor->plive->layers.size;i++)
		{
			px_point impulse;
			PX_LiveLayer *layer=PX_LiveFrameworkGetLayer(editor->plive,i);
			px_point2D pivot;
			editor->accessor->selectedLayer=i;
			if (!PX_LiveEditorRealtimePoseAccessorGetSelectedLayerImpulse(editor->accessor,&impulse)||(impulse.x==0&&impulse.y==0)) continue;
			pivot=PX_LiveEditorModule_RealtimeImpulseVisualPivot(editor->plive,layer);
			PX_GeoDrawArrow(psurface,PX_LiveEditorViewModelToScreen(editor->plive,pivot),PX_LiveEditorViewModelToScreen(editor->plive,PX_POINT2D(pivot.x+impulse.x,pivot.y+impulse.y)),1,PX_COLOR(255,0,0,255));
		}
		editor->accessor->selectedLayer=oldLayer;
	}
}

PX_Object *PX_LiveEditorModule_RealtimeImpulseInstall(PX_Object *parent,PX_Runtime *pruntime,PX_LiveEditorRealtimePoseAccessor *accessor)
{
	PX_LiveEditorModule_RealtimeImpulse desc;
	PX_Object *object;
	PX_memset(&desc,0,sizeof(desc));
	desc.accessor=accessor;
	desc.plive=accessor->plive;
	desc.pruntime=pruntime;
	desc.editingLayer=-1;
	object=PX_ObjectCreateEx(&pruntime->mp_dynamic,parent,0,0,0,0,0,0,0,PX_NULL,PX_LiveEditorModule_RealtimeImpulseRender,PX_NULL,&desc,sizeof(desc));
	if (!object) return PX_NULL;
	PX_ObjectRegisterEvent(object,PX_OBJECT_EVENT_CURSORDOWN,PX_LiveEditorModule_RealtimeImpulseOnDown,PX_NULL);
	PX_ObjectRegisterEvent(object,PX_OBJECT_EVENT_CURSORRDOWN,PX_LiveEditorModule_RealtimeImpulseOnRightDown,PX_NULL);
	PX_ObjectRegisterEvent(object,PX_OBJECT_EVENT_CURSORMOVE,PX_LiveEditorModule_RealtimeImpulseOnMove,PX_NULL);
	object->Enabled=PX_FALSE;
	object->Visible=PX_FALSE;
	return object;
}

px_void PX_LiveEditorModule_RealtimeImpulseEnable(PX_Object *object)
{
	if (!object) return;
	object->Enabled=PX_TRUE;
	object->Visible=PX_TRUE;
	PX_LiveEditorModule_RealtimeImpulseSelectMode(object);
}

px_void PX_LiveEditorModule_RealtimeImpulseDisable(PX_Object *object)
{
	PX_LiveEditorModule_RealtimeImpulse *editor;
	if (!object) return;
	editor=(PX_LiveEditorModule_RealtimeImpulse *)object->pObjectDesc[0];
	editor->editingLayer=-1;
	object->Enabled=PX_FALSE;
	object->Visible=PX_FALSE;
}
