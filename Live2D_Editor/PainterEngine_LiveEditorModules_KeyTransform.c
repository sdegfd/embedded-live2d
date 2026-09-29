#include "PainterEngine_LiveEditorModules_KeyTransform.h"
#include "PainterEngine_LiveEditorModules_View.h"

static px_point2D PX_LiveEditorModule_KeyTransformGetVisualPivot(PX_LiveFramework *pLive,PX_LiveLayer *pLayer)
{
	px_point point=pLayer->currentKeyPoint;
	PX_LiveLayer *pCurrent=pLayer;
	while(pCurrent)
	{
		px_point relative=PX_PointSub(point,pCurrent->currentKeyPoint);
		px_point localTranslation=PX_PointRotate(pCurrent->rel_currentLocalTranslation,pCurrent->rel_currentRotationAngle);
		relative=PX_PointRotate(relative,pCurrent->rel_currentLocalRotationAngle);
		relative=PX_PointMul(relative,pCurrent->rel_currentLocalScale);
		point=PX_PointAdd(PX_PointAdd(relative,pCurrent->currentKeyPoint),localTranslation);
		pCurrent=PX_LiveFrameworkGetLayerParent(pLive,pCurrent);
	}
	return PX_POINT2D(point.x,point.y);
}

static px_void PX_LiveEditorModule_KeyTransformGetAncestorVisualLinear(PX_LiveFramework *pLive,PX_LiveLayer *pLayer,px_float *pRotation,px_float *pScale)
{
	PX_LiveLayer *pCurrent=PX_LiveFrameworkGetLayerParent(pLive,pLayer);
	*pRotation=0;
	*pScale=1;
	while(pCurrent)
	{
		*pRotation+=pCurrent->rel_currentLocalRotationAngle;
		*pScale*=pCurrent->rel_currentLocalScale;
		pCurrent=PX_LiveFrameworkGetLayerParent(pLive,pCurrent);
	}
}

static px_void PX_LiveEditorModule_KeyTransformEnsureLocalPayload(PX_LiveAnimationFramePayload *pPayload)
{
	if(pPayload->localTransformMagic!=PX_LIVE_ANIMATION_LOCAL_TRANSFORM_MAGIC)
	{
		pPayload->localTranslation=PX_POINT(0,0,0);
		pPayload->localRotation=0;
		pPayload->localScaleOffset=0;
		pPayload->localTransformMagic=PX_LIVE_ANIMATION_LOCAL_TRANSFORM_MAGIC;
	}
}

static px_void PX_LiveEditorModule_KeyTransformSyncAdapter(PX_LiveEditorModule_KeyTransform *pTransform)
{
	PX_LiveLayer *pLayer;
	PX_LiveAnimationFramePayload *pPayload;
	px_point2D centerModel,centerScreen,handleDirection,handleScreen;
	px_float localRotation=0,localScale=1,ancestorRotation,ancestorScale;
	if(!pTransform->pTranform->Visible) return;
	pLayer=PX_LiveFrameworkGetCurrentEditLiveLayer(pTransform->pLiveFramework);
	pPayload=PX_LiveFrameworkGetCurrentEditAnimationFramePayload(pTransform->pLiveFramework);
	if(!pLayer||!pPayload) return;
	if(pPayload->localTransformMagic==PX_LIVE_ANIMATION_LOCAL_TRANSFORM_MAGIC)
	{
		localRotation=pPayload->localRotation;
		localScale=1+pPayload->localScaleOffset;
	}
	centerModel=PX_LiveEditorModule_KeyTransformGetVisualPivot(pTransform->pLiveFramework,pLayer);
	centerScreen=PX_LiveEditorViewModelToScreen(pTransform->pLiveFramework,centerModel);
	PX_LiveEditorModule_KeyTransformGetAncestorVisualLinear(pTransform->pLiveFramework,pLayer,&ancestorRotation,&ancestorScale);
	(void)ancestorScale;
	/* Fixed screen length; direction inherits hierarchy and ancestor-local rotation. */
	handleDirection=PX_Point2DRotate(PX_POINT2D(0,-80),pLayer->rel_currentRotationAngle+ancestorRotation);
	handleScreen=PX_Point2DAdd(centerScreen,handleDirection);
	PX_Object_TransformAdapterResetState(pTransform->pTranform,centerScreen.x,centerScreen.y,handleScreen,localRotation,localScale);
}

PX_OBJECT_UPDATE_FUNCTION(PX_LiveEditorModule_KeyTransformUpdate)
{
	PX_LiveEditorModule_KeyTransform *pTransform=(PX_LiveEditorModule_KeyTransform *)pObject->pObjectDesc[0];
	PX_LiveEditorModule_KeyTransformSyncAdapter(pTransform);
}

static px_void PX_LiveEditorModule_KeyTransformEnterEditMode(PX_Object *pObject,px_int layerIndex)
{
	PX_LiveEditorModule_KeyTransform *pDesc=(PX_LiveEditorModule_KeyTransform *)pObject->pObjectDesc[0];
	pDesc->pLiveFramework->showKeypoint=PX_FALSE;
	pDesc->pLiveFramework->showlinker=PX_FALSE;
	pDesc->pLiveFramework->showRange=PX_FALSE;
	pDesc->pLiveFramework->showFocusLayer=PX_FALSE;
	pDesc->pLiveFramework->currentEditLayerIndex=layerIndex;
	PX_ObjectSetFocus(pObject);
}

static px_void PX_LiveEditorModule_KeyTransformEnterSelectMode(PX_Object *pObject)
{
	PX_LiveEditorModule_KeyTransform *pDesc=(PX_LiveEditorModule_KeyTransform *)pObject->pObjectDesc[0];
	pDesc->pLiveFramework->showKeypoint=PX_TRUE;
	pDesc->pLiveFramework->showlinker=PX_FALSE;
	pDesc->pLiveFramework->showRange=PX_FALSE;
	pDesc->pLiveFramework->showFocusLayer=PX_FALSE;
	pDesc->pLiveFramework->currentEditLayerIndex=-1;
	pDesc->pTranform->Visible=PX_FALSE;
	PX_ObjectReleaseFocus(pObject);
}

px_void PX_LiveEditorModule_KeyTransformOnTransformChanged(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_Object *pDescObject=(PX_Object *)ptr;
	PX_LiveEditorModule_KeyTransform *pDesc=(PX_LiveEditorModule_KeyTransform *)pDescObject->pObjectDesc[0];
	PX_LiveAnimationFramePayload *pPayload=(PX_LiveAnimationFramePayload *)PX_LiveFrameworkGetCurrentEditAnimationFramePayload(pDesc->pLiveFramework);
	if(!pPayload) return;
	PX_LiveEditorModule_KeyTransformEnsureLocalPayload(pPayload);
	if(pDesc->mode==PX_LIVEEDITORMODULE_KEYTRANSFORM_MODE_ALL||pDesc->mode==PX_LIVEEDITORMODULE_KEYTRANSFORM_MODE_ROTATION)
	{
		pPayload->localRotation=PX_Object_TransformAdapterGetRotation(pDesc->pTranform);
	}
	if(pDesc->mode==PX_LIVEEDITORMODULE_KEYTRANSFORM_MODE_ALL||pDesc->mode==PX_LIVEEDITORMODULE_KEYTRANSFORM_MODE_STRETCH)
	{
		pPayload->localScaleOffset=PX_Object_TransformAdapterGetStretch(pDesc->pTranform)-1;
	}

	PX_LiveFrameworkRunCurrentEditFrame(pDesc->pLiveFramework);
}

px_void PX_LiveEditorModule_KeyTransformOnCursorRDown(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_LiveEditorModule_KeyTransform *pTransform=(PX_LiveEditorModule_KeyTransform *)pObject->pObjectDesc[0];
	pTransform->pLiveFramework->currentEditLayerIndex=-1;
	pTransform->pTranform->Visible=PX_FALSE;;
	PX_LiveEditorModule_KeyTransformEnterSelectMode(pObject);
}

px_void PX_LiveEditorModule_KeyTransformOnCursorDown(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_LiveEditorModule_KeyTransform *pTransform=(PX_LiveEditorModule_KeyTransform *)pObject->pObjectDesc[0];
	px_float x,y;
	px_int i;

	if (pTransform->pLiveFramework->currentEditLayerIndex>=0&&pTransform->pLiveFramework->currentEditLayerIndex<pTransform->pLiveFramework->layers.size)
	{
		if(pTransform->mode==PX_LIVEEDITORMODULE_KEYTRANSFORM_MODE_ALL||pTransform->mode==PX_LIVEEDITORMODULE_KEYTRANSFORM_MODE_TRANSLATION)
		{
			x=PX_Object_Event_GetCursorX(e);
			y=PX_Object_Event_GetCursorY(e);
			{ px_point2D point=PX_LiveEditorViewScreenToModel(pTransform->pLiveFramework,PX_POINT2D(x,y)); x=point.x; y=point.y; }
			pTransform->cursor_last_x=x;
			pTransform->cursor_last_y=y;
		}
		return;
	}

	x=PX_Object_Event_GetCursorX(e);
	y=PX_Object_Event_GetCursorY(e);

	{ px_point2D point=PX_LiveEditorViewScreenToModel(pTransform->pLiveFramework,PX_POINT2D(x,y)); x=point.x; y=point.y; }

	for (i=0;i<pTransform->pLiveFramework->layers.size;i++)
	{
		PX_LiveLayer *pLayer;
		px_point2D layerCurrentPt;
		pLayer=PX_VECTORAT(PX_LiveLayer,&pTransform->pLiveFramework->layers,i);
		layerCurrentPt=PX_LiveEditorModule_KeyTransformGetVisualPivot(pTransform->pLiveFramework,pLayer);
		if(PX_isPoint2DInCircle(layerCurrentPt,PX_POINT2D(x,y),PX_LiveEditorViewModelMarkerHitRadius(pTransform->pLiveFramework,5)))
		{
			pTransform->cursor_last_x=x;
			pTransform->cursor_last_y=y;
			if (!PX_LiveFrameworkGetCurrentEditAnimationFramePayloadIndex(pTransform->pLiveFramework,i))
			{
				return;
			}
			pTransform->pTranform->Visible=pTransform->mode!=PX_LIVEEDITORMODULE_KEYTRANSFORM_MODE_TRANSLATION;
			PX_LiveEditorModule_KeyTransformEnterEditMode(pObject,i);
			PX_LiveEditorModule_KeyTransformSyncAdapter(pTransform);
			return;
		}
	}
	
	
}

px_void PX_LiveEditorModule_KeyTransformOnCursorDrag(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	px_float x,y;
	PX_LiveEditorModule_KeyTransform *pDesc=(PX_LiveEditorModule_KeyTransform *)pObject->pObjectDesc[0];
	PX_LiveLayer *pLayer=PX_LiveFrameworkGetCurrentEditLiveLayer(pDesc->pLiveFramework);
	if (pLayer)
	{
		PX_LiveAnimationFramePayload *pPayload=(PX_LiveAnimationFramePayload *)PX_LiveFrameworkGetCurrentEditAnimationFramePayload(pDesc->pLiveFramework);

		if (pPayload&&(pDesc->mode==PX_LIVEEDITORMODULE_KEYTRANSFORM_MODE_ALL||pDesc->mode==PX_LIVEEDITORMODULE_KEYTRANSFORM_MODE_TRANSLATION))
		{
			px_float translate_x,translate_y;
			px_float ancestorRotation,ancestorScale;
			px_point localDelta;
			x=PX_Object_Event_GetCursorX(e);
			y=PX_Object_Event_GetCursorY(e);

			{ px_point2D point=PX_LiveEditorViewScreenToModel(pDesc->pLiveFramework,PX_POINT2D(x,y)); x=point.x; y=point.y; }

			translate_x=x-pDesc->cursor_last_x;
			translate_y=y-pDesc->cursor_last_y;

			/* Invert hierarchy plus ancestor visual rotation/scale before storing. */
			PX_LiveEditorModule_KeyTransformEnsureLocalPayload(pPayload);
			PX_LiveEditorModule_KeyTransformGetAncestorVisualLinear(pDesc->pLiveFramework,pLayer,&ancestorRotation,&ancestorScale);
			if(ancestorScale<0.0001f&&ancestorScale>-0.0001f)
			{
				return;
			}
			localDelta=PX_PointRotate(PX_POINT(translate_x,translate_y,0),-(pLayer->rel_currentRotationAngle+ancestorRotation));
			localDelta=PX_PointMul(localDelta,1.0f/ancestorScale);
			pPayload->localTranslation.x+=localDelta.x;
			pPayload->localTranslation.y+=localDelta.y;
			pPayload->localTranslation.z=0;
			
			pDesc->cursor_last_x=x;
			pDesc->cursor_last_y=y;

			PX_LiveFrameworkRunCurrentEditFrame(pDesc->pLiveFramework);
		}
	}
}

PX_Object * PX_LiveEditorModule_KeyTransformInstall(PX_Object *pparent,PX_Runtime *pruntime,PX_FontModule *fm,PX_LiveFramework *pLiveFramework,PX_Json *pLanguageJson)
{
	PX_Object *pObject;
	PX_LiveEditorModule_KeyTransform Transform,*pTransform;
	PX_memset(&Transform,0,sizeof(Transform));
	pObject=PX_ObjectCreateEx(&pruntime->mp_dynamic,pparent,0,0,0,0,0,0,0,PX_LiveEditorModule_KeyTransformUpdate,PX_NULL,PX_NULL,&Transform,sizeof(Transform));
	pTransform=(PX_LiveEditorModule_KeyTransform *)pObject->pObjectDesc[0];
	pTransform->pLiveFramework=pLiveFramework;
	pTransform->pruntime=pruntime;
	pTransform->pLiveFramework->currentEditLayerIndex=-1;
	pTransform->pTranform=PX_Object_TransformAdapterCreate(&pruntime->mp_dynamic,pObject,0,0,PX_POINT2D(0,1));
	pTransform->pTranform->Visible=PX_FALSE;

	pObject->OnLostFocusReleaseEvent=PX_TRUE;
	PX_ObjectRegisterEvent(pTransform->pTranform,PX_OBJECT_EVENT_VALUECHANGED,PX_LiveEditorModule_KeyTransformOnTransformChanged,pObject);
	PX_ObjectRegisterEvent(pObject,PX_OBJECT_EVENT_CURSORDOWN,PX_LiveEditorModule_KeyTransformOnCursorDown,PX_NULL);
	PX_ObjectRegisterEvent(pObject,PX_OBJECT_EVENT_CURSORRDOWN,PX_LiveEditorModule_KeyTransformOnCursorRDown,PX_NULL);
	PX_ObjectRegisterEvent(pObject,PX_OBJECT_EVENT_CURSORDRAG,PX_LiveEditorModule_KeyTransformOnCursorDrag,PX_NULL);
	return pObject;
}

px_void PX_LiveEditorModule_KeyTransformUninstall(PX_Object *pObject)
{
	PX_ObjectDelete(pObject);
}

px_void PX_LiveEditorModule_KeyTransformEnable(PX_Object *pObject,PX_LIVEEDITORMODULE_KEYTRANSFORM_MODE mode)
{
	PX_LiveEditorModule_KeyTransform *pTransform=(PX_LiveEditorModule_KeyTransform *)pObject->pObjectDesc[0];
	pObject->Enabled=PX_TRUE;
	pObject->Visible=PX_TRUE;
	pTransform->pLiveFramework->showRootHelperLine=PX_TRUE;
	pTransform->pLiveFramework->currentEditLayerIndex=-1;
	pTransform->pLiveFramework->showlinker=PX_FALSE;
	pTransform->pLiveFramework->showKeypoint=PX_TRUE;
	pTransform->pLiveFramework->showFocusLayer=PX_FALSE;
	pTransform->mode=mode;
	pTransform->pTranform->Visible=PX_FALSE;

	switch(mode)
	{
	case PX_LIVEEDITORMODULE_KEYTRANSFORM_MODE_ALL:
		PX_Object_TransformAdapterSetMode(pTransform->pTranform,PX_OBJECT_TRANSFORMADAPTER_MODE_ANY);
		break;
	case PX_LIVEEDITORMODULE_KEYTRANSFORM_MODE_TRANSLATION:
		PX_Object_TransformAdapterSetMode(pTransform->pTranform,PX_OBJECT_TRANSFORMADAPTER_MODE_ANY);
		break;
	case PX_LIVEEDITORMODULE_KEYTRANSFORM_MODE_ROTATION:
		PX_Object_TransformAdapterSetMode(pTransform->pTranform,PX_OBJECT_TRANSFORMADAPTER_MODE_ROTATION);
		break;
	case PX_LIVEEDITORMODULE_KEYTRANSFORM_MODE_STRETCH:
		PX_Object_TransformAdapterSetMode(pTransform->pTranform,PX_OBJECT_TRANSFORMADAPTER_MODE_STRETCH);
		break;
	}
	
}

px_void PX_LiveEditorModule_KeyTransformDisable(PX_Object *pObject)
{
	PX_LiveEditorModule_KeyTransform *pTransform=(PX_LiveEditorModule_KeyTransform *)pObject->pObjectDesc[0];
	pObject->Enabled=PX_FALSE;
	pObject->Visible=PX_FALSE;
	
}
