#include "PainterEngine_LiveEditorModules_RealtimeVertexTransform.h"
#include "PainterEngine_LiveEditorModules_View.h"

static px_point2D PX_LiveEditorModule_RealtimeVertexVisualPivot(PX_LiveFramework *plive,PX_LiveLayer *layer)
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

static px_void PX_LiveEditorModule_RealtimeVertexVisualLinear(PX_LiveFramework *plive,PX_LiveLayer *layer,px_float *rotation,px_float *scale)
{
	PX_LiveLayer *current=layer;
	*rotation=0;
	*scale=1;
	while(current)
	{
		*rotation+=current->rel_currentLocalRotationAngle;
		*scale*=current->rel_currentLocalScale;
		current=PX_LiveFrameworkGetLayerParent(plive,current);
	}
}

static px_void PX_LiveEditorModule_RealtimeVertexSelectMode(PX_Object *object)
{
	PX_LiveEditorModule_RealtimeVertexTransform *editor=(PX_LiveEditorModule_RealtimeVertexTransform *)object->pObjectDesc[0];
	PX_LiveLayer *layer=PX_LiveFrameworkGetLayer(editor->plive,editor->editingLayer);
	if (layer) layer->showMesh=PX_FALSE;
	editor->editingLayer=-1;
	editor->editingVertex=-1;
	editor->plive->currentEditLayerIndex=-1;
	editor->plive->currentEditVertexIndex=-1;
	editor->plive->showFocusLayer=PX_FALSE;
	editor->plive->showKeypoint=PX_TRUE;
	PX_ObjectReleaseFocus(object);
}

static px_void PX_LiveEditorModule_RealtimeVertexOnRightDown(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_LiveEditorModule_RealtimeVertexSelectMode(pObject);
}

static px_void PX_LiveEditorModule_RealtimeVertexOnDown(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_LiveEditorModule_RealtimeVertexTransform *editor=(PX_LiveEditorModule_RealtimeVertexTransform *)pObject->pObjectDesc[0];
	px_point2D model=PX_LiveEditorViewScreenToModel(editor->plive,PX_POINT2D(PX_Object_Event_GetCursorX(e),PX_Object_Event_GetCursorY(e)));
	px_int i;
	if (!PX_LiveEditorRealtimePoseAccessorIsSelectedKeyEditable(editor->accessor)) return;
	if (editor->editingLayer>=0)
	{
		PX_LiveLayer *layer=PX_LiveFrameworkGetLayer(editor->plive,editor->editingLayer);
		editor->editingVertex=-1;
		if (!layer) return;
		for (i=0;i<layer->vertices.size;i++)
		{
			PX_LiveVertex *vertex=PX_VECTORAT(PX_LiveVertex,&layer->vertices,i);
			if (PX_isPointInCircle(vertex->currentPosition,PX_POINT(model.x,model.y,0),PX_LiveEditorViewModelMarkerHitRadius(editor->plive,5)))
			{
				editor->editingVertex=i;
				editor->plive->currentEditVertexIndex=i;
				editor->cursorLastX=model.x;
				editor->cursorLastY=model.y;
				return;
			}
		}
		return;
	}
	for (i=0;i<editor->plive->layers.size;i++)
	{
		PX_LiveLayer *layer=PX_LiveFrameworkGetLayer(editor->plive,i);
		px_point2D pivot=PX_LiveEditorModule_RealtimeVertexVisualPivot(editor->plive,layer);
		if (PX_isPoint2DInCircle(pivot,model,PX_LiveEditorViewModelMarkerHitRadius(editor->plive,5)))
		{
			if (!PX_LiveEditorRealtimePoseAccessorSelectLayer(editor->accessor,i)) return;
			editor->editingLayer=i;
			editor->editingVertex=-1;
			editor->plive->currentEditLayerIndex=i;
			editor->plive->showKeypoint=PX_FALSE;
			editor->plive->showFocusLayer=PX_TRUE;
			layer->showMesh=PX_TRUE;
			PX_ObjectSetFocus(pObject);
			return;
		}
	}
}

static px_void PX_LiveEditorModule_RealtimeVertexOnDrag(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_LiveEditorModule_RealtimeVertexTransform *editor=(PX_LiveEditorModule_RealtimeVertexTransform *)pObject->pObjectDesc[0];
	PX_LiveLayer *layer;
	px_point2D model;
	px_point delta,currentDelta;
	px_float visualRotation,visualScale;
	if (editor->editingLayer<0 || editor->editingVertex<0) return;
	layer=PX_LiveFrameworkGetLayer(editor->plive,editor->editingLayer);
	if (!layer || !PX_LiveEditorRealtimePoseAccessorGetSelectedVertexDelta(editor->accessor,editor->editingVertex,&currentDelta)) return;
	model=PX_LiveEditorViewScreenToModel(editor->plive,PX_POINT2D(PX_Object_Event_GetCursorX(e),PX_Object_Event_GetCursorY(e)));
	delta=PX_POINT(model.x-editor->cursorLastX,model.y-editor->cursorLastY,0);
	PX_LiveEditorModule_RealtimeVertexVisualLinear(editor->plive,layer,&visualRotation,&visualScale);
	if (visualScale<0.0001f&&visualScale>-0.0001f) return;
	delta=PX_PointRotate(delta,-visualRotation);
	delta=PX_PointMul(delta,1.0f/visualScale);
	delta=PX_PointRotate(delta,-layer->rel_currentRotationAngle);
	currentDelta=PX_PointAdd(currentDelta,delta);
	editor->cursorLastX=model.x;
	editor->cursorLastY=model.y;
	PX_LiveEditorRealtimePoseAccessorSetSelectedVertexDelta(editor->accessor,editor->editingVertex,currentDelta);
}

PX_Object *PX_LiveEditorModule_RealtimeVertexTransformInstall(PX_Object *parent,PX_Runtime *pruntime,PX_LiveEditorRealtimePoseAccessor *accessor)
{
	PX_Object *object;
	PX_LiveEditorModule_RealtimeVertexTransform desc;
	PX_memset(&desc,0,sizeof(desc));
	desc.accessor=accessor;
	desc.plive=accessor->plive;
	desc.editingLayer=-1;
	desc.editingVertex=-1;
	object=PX_ObjectCreateEx(&pruntime->mp_dynamic,parent,0,0,0,0,0,0,0,PX_NULL,PX_NULL,PX_NULL,&desc,sizeof(desc));
	if (!object) return PX_NULL;
	PX_ObjectRegisterEvent(object,PX_OBJECT_EVENT_CURSORDOWN,PX_LiveEditorModule_RealtimeVertexOnDown,PX_NULL);
	PX_ObjectRegisterEvent(object,PX_OBJECT_EVENT_CURSORRDOWN,PX_LiveEditorModule_RealtimeVertexOnRightDown,PX_NULL);
	PX_ObjectRegisterEvent(object,PX_OBJECT_EVENT_CURSORDRAG,PX_LiveEditorModule_RealtimeVertexOnDrag,PX_NULL);
	object->Enabled=PX_FALSE;
	object->Visible=PX_FALSE;
	return object;
}

px_void PX_LiveEditorModule_RealtimeVertexTransformEnable(PX_Object *object)
{
	object->Enabled=PX_TRUE;
	object->Visible=PX_TRUE;
	PX_LiveEditorModule_RealtimeVertexSelectMode(object);
}

px_void PX_LiveEditorModule_RealtimeVertexTransformDisable(PX_Object *object)
{
	if (!object) return;
	PX_LiveEditorModule_RealtimeVertexSelectMode(object);
	object->Enabled=PX_FALSE;
	object->Visible=PX_FALSE;
}
