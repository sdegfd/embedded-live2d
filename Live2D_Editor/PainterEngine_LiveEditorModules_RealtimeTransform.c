#include "PainterEngine_LiveEditorModules_RealtimeTransform.h"
#include "PainterEngine_LiveEditorModules_View.h"

static px_point2D PX_LiveEditorModule_RealtimeTransformVisualPivot(PX_LiveFramework *plive,PX_LiveLayer *layer)
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

static px_void PX_LiveEditorModule_RealtimeTransformAncestorLinear(PX_LiveFramework *plive,PX_LiveLayer *layer,px_float *rotation,px_float *scale)
{
	PX_LiveLayer *current=PX_LiveFrameworkGetLayerParent(plive,layer);
	*rotation=0;
	*scale=1;
	while(current)
	{
		*rotation+=current->rel_currentLocalRotationAngle;
		*scale*=current->rel_currentLocalScale;
		current=PX_LiveFrameworkGetLayerParent(plive,current);
	}
}

static px_void PX_LiveEditorModule_RealtimeTransformSyncAdapter(PX_LiveEditorModule_RealtimeTransform *editor)
{
	PX_LiveLayer *layer;
	px_point translation;
	px_float rotation,scale,ancestorRotation,ancestorScale;
	px_point2D centerModel,centerScreen,direction,handle;
	if (!editor->adapter->Visible || editor->editingLayer<0) return;
	layer=PX_LiveFrameworkGetLayer(editor->plive,editor->editingLayer);
	if (!layer || !PX_LiveEditorRealtimePoseAccessorGetSelectedLayerTransform(editor->accessor,&translation,&rotation,&scale)) return;
	if (editor->mode==PX_LIVEEDITOR_REALTIME_TRANSFORM_GLOBAL_ROTATION &&
		!PX_LiveEditorRealtimePoseAccessorGetSelectedSubtreeRotation(editor->accessor,&rotation)) return;
	centerModel=PX_LiveEditorModule_RealtimeTransformVisualPivot(editor->plive,layer);
	centerScreen=PX_LiveEditorViewModelToScreen(editor->plive,centerModel);
	PX_LiveEditorModule_RealtimeTransformAncestorLinear(editor->plive,layer,&ancestorRotation,&ancestorScale);
	(void)ancestorScale;
	if (editor->mode==PX_LIVEEDITOR_REALTIME_TRANSFORM_GLOBAL_ROTATION && layer->child_index[0]!=-1)
	{
		PX_LiveLayer *child=PX_LiveFrameworkGetLayer(editor->plive,layer->child_index[0]);
		px_point2D childPivot=child?PX_LiveEditorModule_RealtimeTransformVisualPivot(editor->plive,child):centerModel;
		direction=PX_Point2DSub(PX_LiveEditorViewModelToScreen(editor->plive,childPivot),centerScreen);
		/* TransformAdapter applies the authored rotation after sourceAdaptPoint.
		   Remove that same rotation from the already displayed child direction
		   so the handle is not rotated twice on entry. */
		direction=PX_Point2DRotate(direction,-rotation);
		if (PX_Point2DMod(direction)<1.0f) direction=PX_Point2DRotate(PX_POINT2D(0,-80),layer->rel_currentLocalRotationAngle+ancestorRotation);
	}
	else direction=PX_Point2DRotate(PX_POINT2D(0,-80),layer->rel_currentRotationAngle+ancestorRotation);
	handle=PX_Point2DAdd(centerScreen,direction);
	PX_Object_TransformAdapterResetState(editor->adapter,centerScreen.x,centerScreen.y,handle,rotation,editor->mode==PX_LIVEEDITOR_REALTIME_TRANSFORM_GLOBAL_ROTATION?1:scale);
}

PX_OBJECT_UPDATE_FUNCTION(PX_LiveEditorModule_RealtimeTransformUpdate)
{
	PX_LiveEditorModule_RealtimeTransform *editor=(PX_LiveEditorModule_RealtimeTransform *)pObject->pObjectDesc[0];
	PX_LiveEditorModule_RealtimeTransformSyncAdapter(editor);
}

static px_void PX_LiveEditorModule_RealtimeTransformSelectMode(PX_Object *object)
{
	PX_LiveEditorModule_RealtimeTransform *editor=(PX_LiveEditorModule_RealtimeTransform *)object->pObjectDesc[0];
	editor->editingLayer=-1;
	editor->adapter->Visible=PX_FALSE;
	editor->plive->showKeypoint=PX_TRUE;
	editor->plive->showFocusLayer=PX_FALSE;
	editor->plive->currentEditLayerIndex=-1;
	PX_ObjectReleaseFocus(object);
}

static px_void PX_LiveEditorModule_RealtimeTransformOnChanged(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_Object *editorObject=(PX_Object *)ptr;
	PX_LiveEditorModule_RealtimeTransform *editor=(PX_LiveEditorModule_RealtimeTransform *)editorObject->pObjectDesc[0];
	px_point translation;
	px_float rotation,scale;
	if (!PX_LiveEditorRealtimePoseAccessorGetSelectedLayerTransform(editor->accessor,&translation,&rotation,&scale)) return;
	if (editor->mode==PX_LIVEEDITOR_REALTIME_TRANSFORM_GLOBAL_ROTATION)
	{
		PX_LiveEditorRealtimePoseAccessorSetSelectedSubtreeRotation(editor->accessor,PX_Object_TransformAdapterGetRotation(editor->adapter));
		return;
	}
	if (editor->mode==PX_LIVEEDITOR_REALTIME_TRANSFORM_ROTATION) rotation=PX_Object_TransformAdapterGetRotation(editor->adapter);
	if (editor->mode==PX_LIVEEDITOR_REALTIME_TRANSFORM_SCALE) scale=PX_Object_TransformAdapterGetStretch(editor->adapter);
	PX_LiveEditorRealtimePoseAccessorSetSelectedLayerTransform(editor->accessor,translation.x,translation.y,rotation,scale);
}

static px_void PX_LiveEditorModule_RealtimeTransformOnRightDown(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_LiveEditorModule_RealtimeTransformSelectMode(pObject);
}

static px_void PX_LiveEditorModule_RealtimeTransformOnDown(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_LiveEditorModule_RealtimeTransform *editor=(PX_LiveEditorModule_RealtimeTransform *)pObject->pObjectDesc[0];
	px_float x=PX_Object_Event_GetCursorX(e),y=PX_Object_Event_GetCursorY(e);
	px_point2D model=PX_LiveEditorViewScreenToModel(editor->plive,PX_POINT2D(x,y));
	px_int i;
	if (!PX_LiveEditorRealtimePoseAccessorIsSelectedKeyEditable(editor->accessor)) return;
	if (editor->editingLayer>=0)
	{
		if (editor->mode==PX_LIVEEDITOR_REALTIME_TRANSFORM_TRANSLATION)
		{
			editor->cursorLastX=model.x;
			editor->cursorLastY=model.y;
		}
		return;
	}
	for (i=0;i<editor->plive->layers.size;i++)
	{
		PX_LiveLayer *layer=PX_LiveFrameworkGetLayer(editor->plive,i);
		px_point2D pivot=PX_LiveEditorModule_RealtimeTransformVisualPivot(editor->plive,layer);
		if (PX_isPoint2DInCircle(pivot,model,PX_LiveEditorViewModelMarkerHitRadius(editor->plive,5)))
		{
			if (!PX_LiveEditorRealtimePoseAccessorSelectLayer(editor->accessor,i)) return;
			editor->editingLayer=i;
			editor->cursorLastX=model.x;
			editor->cursorLastY=model.y;
			editor->plive->showKeypoint=PX_FALSE;
			editor->plive->showFocusLayer=PX_FALSE;
			editor->adapter->Visible=editor->mode!=PX_LIVEEDITOR_REALTIME_TRANSFORM_TRANSLATION;
			PX_ObjectSetFocus(pObject);
			PX_LiveEditorModule_RealtimeTransformSyncAdapter(editor);
			return;
		}
	}
}

static px_void PX_LiveEditorModule_RealtimeTransformOnDrag(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_LiveEditorModule_RealtimeTransform *editor=(PX_LiveEditorModule_RealtimeTransform *)pObject->pObjectDesc[0];
	PX_LiveLayer *layer;
	px_point translation,delta;
	px_float rotation,scale,ancestorRotation,ancestorScale;
	px_point2D model;
	if (editor->mode!=PX_LIVEEDITOR_REALTIME_TRANSFORM_TRANSLATION || editor->editingLayer<0) return;
	layer=PX_LiveFrameworkGetLayer(editor->plive,editor->editingLayer);
	if (!layer || !PX_LiveEditorRealtimePoseAccessorGetSelectedLayerTransform(editor->accessor,&translation,&rotation,&scale)) return;
	model=PX_LiveEditorViewScreenToModel(editor->plive,PX_POINT2D(PX_Object_Event_GetCursorX(e),PX_Object_Event_GetCursorY(e)));
	delta=PX_POINT(model.x-editor->cursorLastX,model.y-editor->cursorLastY,0);
	PX_LiveEditorModule_RealtimeTransformAncestorLinear(editor->plive,layer,&ancestorRotation,&ancestorScale);
	if (ancestorScale<0.0001f&&ancestorScale>-0.0001f) return;
	delta=PX_PointRotate(delta,-(layer->rel_currentRotationAngle+ancestorRotation));
	delta=PX_PointMul(delta,1.0f/ancestorScale);
	editor->cursorLastX=model.x;
	editor->cursorLastY=model.y;
	PX_LiveEditorRealtimePoseAccessorSetSelectedLayerTransform(editor->accessor,translation.x+delta.x,translation.y+delta.y,rotation,scale);
}

PX_Object *PX_LiveEditorModule_RealtimeTransformInstall(PX_Object *parent,PX_Runtime *pruntime,PX_LiveEditorRealtimePoseAccessor *accessor)
{
	PX_Object *object;
	PX_LiveEditorModule_RealtimeTransform desc,*editor;
	PX_memset(&desc,0,sizeof(desc));
	desc.accessor=accessor;
	desc.plive=accessor->plive;
	desc.pruntime=pruntime;
	desc.editingLayer=-1;
	object=PX_ObjectCreateEx(&pruntime->mp_dynamic,parent,0,0,0,0,0,0,0,PX_LiveEditorModule_RealtimeTransformUpdate,PX_NULL,PX_NULL,&desc,sizeof(desc));
	if (!object) return PX_NULL;
	editor=(PX_LiveEditorModule_RealtimeTransform *)object->pObjectDesc[0];
	editor->adapter=PX_Object_TransformAdapterCreate(&pruntime->mp_dynamic,object,0,0,PX_POINT2D(0,1));
	if (!editor->adapter) { PX_ObjectDelete(object); return PX_NULL; }
	editor->adapter->Visible=PX_FALSE;
	PX_ObjectRegisterEvent(editor->adapter,PX_OBJECT_EVENT_VALUECHANGED,PX_LiveEditorModule_RealtimeTransformOnChanged,object);
	PX_ObjectRegisterEvent(object,PX_OBJECT_EVENT_CURSORDOWN,PX_LiveEditorModule_RealtimeTransformOnDown,PX_NULL);
	PX_ObjectRegisterEvent(object,PX_OBJECT_EVENT_CURSORRDOWN,PX_LiveEditorModule_RealtimeTransformOnRightDown,PX_NULL);
	PX_ObjectRegisterEvent(object,PX_OBJECT_EVENT_CURSORDRAG,PX_LiveEditorModule_RealtimeTransformOnDrag,PX_NULL);
	object->Enabled=PX_FALSE;
	object->Visible=PX_FALSE;
	return object;
}

px_void PX_LiveEditorModule_RealtimeTransformEnable(PX_Object *object,PX_LIVEEDITOR_REALTIME_TRANSFORM_MODE mode)
{
	PX_LiveEditorModule_RealtimeTransform *editor=(PX_LiveEditorModule_RealtimeTransform *)object->pObjectDesc[0];
	editor->mode=mode;
	PX_Object_TransformAdapterSetMode(editor->adapter,(mode==PX_LIVEEDITOR_REALTIME_TRANSFORM_ROTATION||mode==PX_LIVEEDITOR_REALTIME_TRANSFORM_GLOBAL_ROTATION)?PX_OBJECT_TRANSFORMADAPTER_MODE_ROTATION:PX_OBJECT_TRANSFORMADAPTER_MODE_STRETCH);
	object->Enabled=PX_TRUE;
	object->Visible=PX_TRUE;
	PX_LiveEditorModule_RealtimeTransformSelectMode(object);
}

px_void PX_LiveEditorModule_RealtimeTransformDisable(PX_Object *object)
{
	PX_LiveEditorModule_RealtimeTransform *editor;
	if (!object) return;
	editor=(PX_LiveEditorModule_RealtimeTransform *)object->pObjectDesc[0];
	editor->adapter->Visible=PX_FALSE;
	editor->editingLayer=-1;
	object->Enabled=PX_FALSE;
	object->Visible=PX_FALSE;
}
