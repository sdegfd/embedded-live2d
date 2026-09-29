#include "PainterEngine_LiveEditorModules_RealtimePanc.h"
#include "PainterEngine_LiveEditorModules_View.h"

static px_point2D PX_LiveEditorModule_RealtimePancVisualPivot(PX_LiveFramework *plive,PX_LiveLayer *layer)
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

static px_uint32 PX_LiveEditorModule_RealtimePancBindingOffset(PX_LiveEditorRealtimeAxisAuthoring *axis,px_int bindingIndex)
{
	px_uint32 offset=0;
	px_int i;
	for (i=0;i<bindingIndex;i++) offset+=axis->bindings[i].vertexCount;
	return offset;
}

static px_int PX_LiveEditorModule_RealtimePancFindBinding(PX_LiveEditorRealtimeAxisAuthoring *axis,px_int layerIndex)
{
	px_int i;
	for (i=0;i<axis->bindingCount;i++) if (axis->bindings[i].layerIndex==layerIndex) return i;
	return -1;
}

static px_float PX_LiveEditorModule_RealtimePancClamp(px_float value)
{
	if (value<-2047.0f) return -2047.0f;
	if (value>2047.0f) return 2047.0f;
	return value;
}

/*
 * Return the model-space displacement produced by the Panc matrix at one
 * point.  Both vertices and key points must use this exact mapping; applying
 * it only to vertices leaves the hierarchy pivot behind and makes a later
 * local scale/rotation orbit around the stale pivot.
 */
static px_point PX_LiveEditorModule_RealtimePancDisplacement(PX_PancMatrix mat,px_point point)
{
	px_point displacement=PX_POINT(0,0,0);
	if (mat.currentX!=mat.sourceX && point.x>mat.x && point.x<mat.x+mat.width)
	{
		if (point.x<mat.sourceX && mat.sourceX!=mat.x)
		{
			px_float d=point.x-mat.x;
			displacement.x=d*(mat.currentX-mat.x)/(mat.sourceX-mat.x)-d;
		}
		else if (mat.x+mat.width!=mat.sourceX)
		{
			px_float d=mat.x+mat.width-point.x;
			displacement.x=d-d*(mat.x+mat.width-mat.currentX)/(mat.x+mat.width-mat.sourceX);
		}
	}
	if (mat.currentY!=mat.sourceY && point.y>mat.y && point.y<mat.y+mat.height)
	{
		if (point.y<mat.sourceY && mat.sourceY!=mat.y)
		{
			px_float d=point.y-mat.y;
			displacement.y=d*(mat.currentY-mat.y)/(mat.sourceY-mat.y)-d;
		}
		else if (mat.y+mat.height!=mat.sourceY)
		{
			px_float d=mat.y+mat.height-point.y;
			displacement.y=d-d*(mat.y+mat.height-mat.currentY)/(mat.y+mat.height-mat.sourceY);
		}
	}
	return displacement;
}

static px_void PX_LiveEditorModule_RealtimePancReleaseBaseline(PX_LiveEditorModule_RealtimePanc *editor)
{
	if (editor->baseline) MP_Free(editor->accessor->mp,editor->baseline);
	if (editor->baselineTranslations) MP_Free(editor->accessor->mp,editor->baselineTranslations);
	editor->baseline=PX_NULL;
	editor->baselineCount=0;
	editor->baselineTranslations=PX_NULL;
	editor->baselineTranslationCount=0;
}

static px_void PX_LiveEditorModule_RealtimePancApplyLayer(PX_LiveEditorModule_RealtimePanc *editor,PX_LiveEditorRealtimeAxisAuthoring *axis,px_int layerIndex,PX_PancMatrix mat,px_point parentDisplacement,px_bool parentAffected)
{
	PX_LiveLayer *layer=PX_LiveFrameworkGetLayer(editor->plive,layerIndex);
	px_int bindingIndex=PX_LiveEditorModule_RealtimePancFindBinding(axis,layerIndex),i;
	px_uint32 offset;
	px_point keyDisplacement,desiredKeyDisplacement,translationDelta,newTranslation,actualLocalTranslation,actualKeyDisplacement;
	if (!layer||bindingIndex<0) return;
	offset=PX_LiveEditorModule_RealtimePancBindingOffset(axis,bindingIndex);
	keyDisplacement=PX_LiveEditorModule_RealtimePancDisplacement(mat,layer->keyPoint);
	/* A Panc vertex delta is consumed before this layer's hierarchy rotation. */
	desiredKeyDisplacement=PX_PointRotate(keyDisplacement,layer->rel_currentRotationAngle);
	translationDelta=desiredKeyDisplacement;
	if (layer->parent_index!=-1)
	{
		PX_LiveLayer *parent=PX_LiveFrameworkGetLayerParent(editor->plive,layer);
		if (!parent) return;
		if (parentAffected) translationDelta=PX_PointSub(desiredKeyDisplacement,parentDisplacement);
		translationDelta=PX_PointRotate(translationDelta,-parent->rel_currentRotationAngle);
	}
	newTranslation.x=PX_LiveEditorModule_RealtimePancClamp(editor->baselineTranslations[bindingIndex].x+translationDelta.x);
	newTranslation.y=PX_LiveEditorModule_RealtimePancClamp(editor->baselineTranslations[bindingIndex].y+translationDelta.y);
	newTranslation.z=0;
	axis->keyBindingPoses[axis->selectedKeySlot][bindingIndex].translation=newTranslation;
	actualLocalTranslation=PX_PointSub(newTranslation,editor->baselineTranslations[bindingIndex]);
	if (layer->parent_index!=-1)
	{
		PX_LiveLayer *parent=PX_LiveFrameworkGetLayerParent(editor->plive,layer);
		actualKeyDisplacement=PX_PointRotate(actualLocalTranslation,parent->rel_currentRotationAngle);
		if (parentAffected) actualKeyDisplacement=PX_PointAdd(actualKeyDisplacement,parentDisplacement);
	}
	else
	{
		actualKeyDisplacement=actualLocalTranslation;
	}
	for (i=0;i<axis->bindings[bindingIndex].vertexCount;i++)
	{
		px_int vertexIndex=axis->bindings[bindingIndex].vertexIndices[i];
		PX_LiveVertex *vertex=PX_VECTORAT(PX_LiveVertex,&layer->vertices,vertexIndex);
		px_point delta=editor->baseline[offset+i];
		px_point vertexDisplacement=PX_LiveEditorModule_RealtimePancDisplacement(mat,vertex->sourcePosition);
		px_point localKeyDisplacement=PX_PointRotate(actualKeyDisplacement,-layer->rel_currentRotationAngle);
		px_point relativeDisplacement=PX_PointSub(vertexDisplacement,localKeyDisplacement);
		delta=PX_PointAdd(delta,relativeDisplacement);
		delta.x=PX_LiveEditorModule_RealtimePancClamp(delta.x);
		delta.y=PX_LiveEditorModule_RealtimePancClamp(delta.y);
		delta.z=0;
		axis->keyVertexDeltas[axis->selectedKeySlot][offset+i]=delta;
	}
	for (i=0;i<PX_COUNTOF(layer->child_index)&&layer->child_index[i]!=-1;i++) PX_LiveEditorModule_RealtimePancApplyLayer(editor,axis,layer->child_index[i],mat,actualKeyDisplacement,PX_TRUE);
}

static px_void PX_LiveEditorModule_RealtimePancOnChanged(PX_Object *panc,PX_Object_Event e,px_void *ptr)
{
	PX_Object *object=(PX_Object *)ptr;
	PX_LiveEditorModule_RealtimePanc *editor=(PX_LiveEditorModule_RealtimePanc *)object->pObjectDesc[0];
	PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(editor->accessor,PX_LiveEditorRealtimePoseAccessorGetSelectedAxis(editor->accessor));
	PX_PancMatrix mat=PX_Object_PancGetMatrix(editor->panc);
	px_point2D point;
	px_int i;
	if (editor->syncing || !PX_LiveEditorRealtimePoseAccessorIsSelectedKeyEditable(editor->accessor)) return;
	if (!axis||editor->editingLayer<0||!editor->baseline||editor->baselineCount!=axis->vertexIndexCount||
		!editor->baselineTranslations||editor->baselineTranslationCount!=(px_uint32)axis->bindingCount) return;
	point=PX_LiveEditorViewScreenToModel(editor->plive,PX_POINT2D(mat.x,mat.y)); mat.x=point.x; mat.y=point.y;
	point=PX_LiveEditorViewScreenToModel(editor->plive,PX_POINT2D(mat.sourceX,mat.sourceY)); mat.sourceX=point.x; mat.sourceY=point.y;
	point=PX_LiveEditorViewScreenToModel(editor->plive,PX_POINT2D(mat.currentX,mat.currentY)); mat.currentX=point.x; mat.currentY=point.y;
	mat.width/=PX_LiveEditorViewGetScale(editor->plive);
	mat.height/=PX_LiveEditorViewGetScale(editor->plive);
	editor->modelMatrix=mat;
	PX_memcpy(axis->keyVertexDeltas[axis->selectedKeySlot],editor->baseline,sizeof(px_point)*axis->vertexIndexCount);
	for (i=0;i<axis->bindingCount;i++) axis->keyBindingPoses[axis->selectedKeySlot][i].translation=editor->baselineTranslations[i];
	PX_LiveEditorModule_RealtimePancApplyLayer(editor,axis,editor->editingLayer,mat,PX_POINT(0,0,0),PX_FALSE);
	PX_LiveEditorRealtimePoseAccessorCommitChanges(editor->accessor);
}

static px_void PX_LiveEditorModule_RealtimePancSync(PX_LiveEditorModule_RealtimePanc *editor)
{
	PX_Object_Panc *control;
	px_point2D origin,source,current;
	px_float scale;
	if (!editor->panc->Visible||editor->editingLayer<0) return;
	scale=PX_LiveEditorViewGetScale(editor->plive);
	origin=PX_LiveEditorViewModelToScreen(editor->plive,PX_POINT2D(editor->modelMatrix.x,editor->modelMatrix.y));
	source=PX_LiveEditorViewModelToScreen(editor->plive,PX_POINT2D(editor->modelMatrix.sourceX,editor->modelMatrix.sourceY));
	current=PX_LiveEditorViewModelToScreen(editor->plive,PX_POINT2D(editor->modelMatrix.currentX,editor->modelMatrix.currentY));
	editor->syncing=PX_TRUE;
	PX_Object_PancReset(editor->panc,origin.x,origin.y,editor->modelMatrix.width*scale,editor->modelMatrix.height*scale);
	control=PX_ObjectGetDescIndex(PX_Object_Panc,editor->panc,0);
	control->sourceX=source.x;
	control->sourceY=source.y;
	control->currentX=current.x;
	control->currentY=current.y;
	editor->syncing=PX_FALSE;
	editor->viewRevision=editor->plive->view_revision;
}

PX_OBJECT_UPDATE_FUNCTION(PX_LiveEditorModule_RealtimePancUpdate)
{
	PX_LiveEditorModule_RealtimePanc *editor=(PX_LiveEditorModule_RealtimePanc *)pObject->pObjectDesc[0];
	if (editor->viewRevision!=editor->plive->view_revision) PX_LiveEditorModule_RealtimePancSync(editor);
}

static px_void PX_LiveEditorModule_RealtimePancSelectMode(PX_Object *object)
{
	PX_LiveEditorModule_RealtimePanc *editor=(PX_LiveEditorModule_RealtimePanc *)object->pObjectDesc[0];
	PX_LiveEditorModule_RealtimePancReleaseBaseline(editor);
	editor->editingLayer=-1;
	editor->panc->Visible=PX_FALSE;
	editor->bar->Visible=PX_FALSE;
	editor->plive->showKeypoint=PX_TRUE;
	editor->plive->showFocusLayer=PX_FALSE;
	editor->plive->currentEditLayerIndex=-1;
	PX_ObjectReleaseFocus(object);
}

static px_void PX_LiveEditorModule_RealtimePancOnFinish(PX_Object *button,PX_Object_Event e,px_void *ptr)
{
	PX_LiveEditorModule_RealtimePancSelectMode((PX_Object *)ptr);
}

static px_void PX_LiveEditorModule_RealtimePancOnReset(PX_Object *button,PX_Object_Event e,px_void *ptr)
{
	PX_Object *object=(PX_Object *)ptr;
	PX_LiveEditorModule_RealtimePanc *editor=(PX_LiveEditorModule_RealtimePanc *)object->pObjectDesc[0];
	px_float scale=PX_LiveEditorViewGetScale(editor->plive);
	px_point2D origin=PX_LiveEditorViewModelToScreen(editor->plive,PX_POINT2D(0,0));
	PX_Object_PancReset(editor->panc,origin.x,origin.y,editor->plive->width*scale,editor->plive->height*scale);
	PX_LiveEditorModule_RealtimePancOnChanged(editor->panc,e,object);
}

static px_void PX_LiveEditorModule_RealtimePancOnRightDown(PX_Object *object,PX_Object_Event e,px_void *ptr)
{
	PX_LiveEditorModule_RealtimePancSelectMode(object);
}

static px_void PX_LiveEditorModule_RealtimePancOnDown(PX_Object *object,PX_Object_Event e,px_void *ptr)
{
	PX_LiveEditorModule_RealtimePanc *editor=(PX_LiveEditorModule_RealtimePanc *)object->pObjectDesc[0];
	PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(editor->accessor,PX_LiveEditorRealtimePoseAccessorGetSelectedAxis(editor->accessor));
	px_point2D model;
	px_int i;
	px_float scale;
	px_point2D origin;
	if (editor->editingLayer>=0||!axis||!PX_LiveEditorRealtimePoseAccessorIsSelectedKeyEditable(editor->accessor)) return;
	model=PX_LiveEditorViewScreenToModel(editor->plive,PX_POINT2D(PX_Object_Event_GetCursorX(e),PX_Object_Event_GetCursorY(e)));
	for (i=0;i<editor->plive->layers.size;i++)
	{
		PX_LiveLayer *layer=PX_LiveFrameworkGetLayer(editor->plive,i);
		if (PX_isPoint2DInCircle(PX_LiveEditorModule_RealtimePancVisualPivot(editor->plive,layer),model,PX_LiveEditorViewModelMarkerHitRadius(editor->plive,5)))
		{
			px_int bindingIndex;
			if (!PX_LiveEditorRealtimePoseAccessorSelectLayer(editor->accessor,i)) return;
			editor->baseline=axis->vertexIndexCount?(px_point *)MP_Malloc(editor->accessor->mp,sizeof(px_point)*axis->vertexIndexCount):PX_NULL;
			editor->baselineTranslations=axis->bindingCount?(px_point *)MP_Malloc(editor->accessor->mp,sizeof(px_point)*axis->bindingCount):PX_NULL;
			if ((axis->vertexIndexCount&&!editor->baseline)||(axis->bindingCount&&!editor->baselineTranslations))
			{
				PX_LiveEditorModule_RealtimePancReleaseBaseline(editor);
				return;
			}
			editor->baselineCount=axis->vertexIndexCount;
			editor->baselineTranslationCount=axis->bindingCount;
			if (axis->vertexIndexCount) PX_memcpy(editor->baseline,axis->keyVertexDeltas[axis->selectedKeySlot],sizeof(px_point)*axis->vertexIndexCount);
			for (bindingIndex=0;bindingIndex<axis->bindingCount;bindingIndex++) editor->baselineTranslations[bindingIndex]=axis->keyBindingPoses[axis->selectedKeySlot][bindingIndex].translation;
			editor->editingLayer=i;
			editor->plive->showKeypoint=PX_FALSE;
			editor->plive->showFocusLayer=PX_TRUE;
			scale=PX_LiveEditorViewGetScale(editor->plive);
			origin=PX_LiveEditorViewModelToScreen(editor->plive,PX_POINT2D(0,0));
			PX_Object_PancReset(editor->panc,origin.x,origin.y,editor->plive->width*scale,editor->plive->height*scale);
			editor->modelMatrix=PX_Object_PancGetMatrix(editor->panc);
			{
				px_point2D p=PX_LiveEditorViewScreenToModel(editor->plive,PX_POINT2D(editor->modelMatrix.x,editor->modelMatrix.y));
				editor->modelMatrix.x=p.x; editor->modelMatrix.y=p.y;
				p=PX_LiveEditorViewScreenToModel(editor->plive,PX_POINT2D(editor->modelMatrix.sourceX,editor->modelMatrix.sourceY));
				editor->modelMatrix.sourceX=p.x; editor->modelMatrix.sourceY=p.y;
				p=PX_LiveEditorViewScreenToModel(editor->plive,PX_POINT2D(editor->modelMatrix.currentX,editor->modelMatrix.currentY));
				editor->modelMatrix.currentX=p.x; editor->modelMatrix.currentY=p.y;
				editor->modelMatrix.width/=scale; editor->modelMatrix.height/=scale;
			}
			editor->viewRevision=editor->plive->view_revision;
			editor->panc->Visible=PX_TRUE;
			editor->bar->Visible=PX_TRUE;
			PX_ObjectSetFocus(object);
			return;
		}
	}
}

PX_OBJECT_FREE_FUNCTION(PX_LiveEditorModule_RealtimePancFree)
{
	PX_LiveEditorModule_RealtimePanc *editor=(PX_LiveEditorModule_RealtimePanc *)pObject->pObjectDesc[0];
	PX_LiveEditorModule_RealtimePancReleaseBaseline(editor);
}

PX_Object *PX_LiveEditorModule_RealtimePancInstall(PX_Object *parent,PX_Runtime *pruntime,PX_FontModule *fm,PX_LiveEditorRealtimePoseAccessor *accessor,PX_Json *language)
{
	PX_LiveEditorModule_RealtimePanc desc,*editor;
	PX_Object *object;
	PX_memset(&desc,0,sizeof(desc));
	desc.accessor=accessor;
	desc.plive=accessor->plive;
	desc.pruntime=pruntime;
	desc.fontmodule=fm;
	desc.editingLayer=-1;
	object=PX_ObjectCreateEx(&pruntime->mp_dynamic,parent,0,0,0,0,0,0,0,PX_LiveEditorModule_RealtimePancUpdate,PX_NULL,PX_LiveEditorModule_RealtimePancFree,&desc,sizeof(desc));
	if (!object) return PX_NULL;
	editor=(PX_LiveEditorModule_RealtimePanc *)object->pObjectDesc[0];
	editor->panc=PX_Object_PancCreate(&pruntime->mp_dynamic,object,0,0,(px_float)editor->plive->width,(px_float)editor->plive->height);
	editor->bar=PX_ObjectCreate(&pruntime->mp_dynamic,object,(px_float)pruntime->surface_width/2-85,8,0,170,38,0);
	if (!editor->panc||!editor->bar) { PX_ObjectDelete(object); return PX_NULL; }
	editor->buttonReset=PX_Object_PushButtonCreate(&pruntime->mp_dynamic,editor->bar,1,3,82,32,PX_JsonGetString(language,"panc.reset"),fm);
	editor->buttonFinish=PX_Object_PushButtonCreate(&pruntime->mp_dynamic,editor->bar,86,3,82,32,PX_JsonGetString(language,"panc.finish"),fm);
	PX_ObjectRegisterEvent(editor->panc,PX_OBJECT_EVENT_VALUECHANGED,PX_LiveEditorModule_RealtimePancOnChanged,object);
	PX_ObjectRegisterEvent(editor->buttonReset,PX_OBJECT_EVENT_EXECUTE,PX_LiveEditorModule_RealtimePancOnReset,object);
	PX_ObjectRegisterEvent(editor->buttonFinish,PX_OBJECT_EVENT_EXECUTE,PX_LiveEditorModule_RealtimePancOnFinish,object);
	PX_ObjectRegisterEvent(object,PX_OBJECT_EVENT_CURSORDOWN,PX_LiveEditorModule_RealtimePancOnDown,PX_NULL);
	PX_ObjectRegisterEvent(object,PX_OBJECT_EVENT_CURSORRDOWN,PX_LiveEditorModule_RealtimePancOnRightDown,PX_NULL);
	editor->panc->Visible=PX_FALSE;
	editor->bar->Visible=PX_FALSE;
	object->Enabled=PX_FALSE;
	object->Visible=PX_FALSE;
	return object;
}

px_void PX_LiveEditorModule_RealtimePancEnable(PX_Object *object)
{
	if (!object) return;
	object->Enabled=PX_TRUE;
	object->Visible=PX_TRUE;
	PX_LiveEditorModule_RealtimePancSelectMode(object);
}

px_void PX_LiveEditorModule_RealtimePancDisable(PX_Object *object)
{
	if (!object) return;
	PX_LiveEditorModule_RealtimePancSelectMode(object);
	object->Enabled=PX_FALSE;
	object->Visible=PX_FALSE;
}
