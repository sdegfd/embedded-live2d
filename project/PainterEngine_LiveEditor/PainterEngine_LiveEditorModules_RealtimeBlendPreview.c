#include "PainterEngine_LiveEditorModules_RealtimeBlendPreview.h"
#ifdef _WIN32
#include <windows.h>
#endif

#define PX_LIVEEDITOR_BLEND_AXIS_PANEL_WIDTH 360
#define PX_LIVEEDITOR_BLEND_AXIS_PANEL_HEIGHT 250
#define PX_LIVEEDITOR_BLEND_AXIS_ROW_HEIGHT 68
#define PX_LIVEEDITOR_BLEND_AXIS_SCROLL_WIDTH (PX_LIVEEDITOR_BLEND_AXIS_PANEL_WIDTH-16)
#define PX_LIVEEDITOR_BLEND_AXIS_SCROLL_HEIGHT (PX_LIVEEDITOR_BLEND_AXIS_PANEL_HEIGHT-42)
#define PX_LIVEEDITOR_BLEND_AXIS_ROW_WIDTH (PX_LIVEEDITOR_BLEND_AXIS_SCROLL_WIDTH-26)

static px_bool PX_LiveEditorModule_RealtimeBlendPreviewCanUseAxis(const PX_LiveEditorRealtimeAxisAuthoring *axis)
{
	return axis && axis->runtimeHandle!=PX_LIVE_REALTIME_INVALID_HANDLE && axis->baked && !axis->dirty;
}

static px_float PX_LiveEditorModule_RealtimeBlendPreviewMin(px_float a,px_float b)
{
	return a<b?a:b;
}

PX_OBJECT_RENDER_FUNCTION(PX_LiveEditorModule_RealtimeBlendPreviewRender)
{
	PX_LiveEditorModule_RealtimeBlendPreview *preview=(PX_LiveEditorModule_RealtimeBlendPreview *)pObject->pObjectDesc[0];
	PX_LiveFramework *plive=preview->accessor->plive;
	px_float oldScale,scale,availableWidth,availableHeight;
	px_bool oldShowKeypoint,oldShowLinker,oldShowFocus;
	px_bool oldShowMesh[PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER];
	px_int i,layerCount;
	PX_GeoDrawRect(psurface,0,0,(px_int)pObject->Width-1,(px_int)pObject->Height-1,PX_COLOR(255,245,245,245));
	PX_GeoDrawRect(psurface,0,0,(px_int)pObject->Width-1,38,PX_OBJECT_UI_DEFAULT_BACKGROUNDCOLOR);
	PX_GeoDrawBorder(psurface,0,0,(px_int)pObject->Width-1,(px_int)pObject->Height-1,1,PX_OBJECT_UI_DEFAULT_BORDERCOLOR);
	PX_GeoDrawLine(psurface,0,38,(px_int)pObject->Width-1,38,1,PX_OBJECT_UI_DEFAULT_BORDERCOLOR);
	if (!plive || plive->width<=0 || plive->height<=0) return;
	availableWidth=pObject->Width-80;
	availableHeight=pObject->Height-116;
	if (availableWidth<1 || availableHeight<1) return;
	scale=PX_LiveEditorModule_RealtimeBlendPreviewMin(availableWidth/plive->width,availableHeight/plive->height);
	if (scale<0.1f) scale=0.1f;
	if (scale>4.0f) scale=4.0f;
	oldScale=plive->view_scale;
	oldShowKeypoint=plive->showKeypoint;
	oldShowLinker=plive->showlinker;
	oldShowFocus=plive->showFocusLayer;
	layerCount=plive->layers.size;
	if (layerCount>PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER) layerCount=PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER;
	for (i=0;i<layerCount;i++)
	{
		PX_LiveLayer *layer=PX_LiveFrameworkGetLayer(plive,i);
		oldShowMesh[i]=layer?layer->showMesh:PX_FALSE;
		if (layer) layer->showMesh=PX_FALSE;
	}
	plive->view_scale=scale;
	plive->showKeypoint=PX_FALSE;
	plive->showlinker=PX_FALSE;
	plive->showFocusLayer=PX_FALSE;
	PX_LiveFrameworkRenderCurrent(psurface,plive,pObject->Width/2,(pObject->Height+38)/2,PX_ALIGN_CENTER);
	plive->view_scale=oldScale;
	plive->showKeypoint=oldShowKeypoint;
	plive->showlinker=oldShowLinker;
	plive->showFocusLayer=oldShowFocus;
	for (i=0;i<layerCount;i++)
	{
		PX_LiveLayer *layer=PX_LiveFrameworkGetLayer(plive,i);
		if (layer) layer->showMesh=oldShowMesh[i];
	}
}

PX_OBJECT_RENDER_FUNCTION(PX_LiveEditorModule_RealtimeBlendPreviewPanelRender)
{
	px_float x,y;
	PX_ObjectGetInheritXY(pObject,&x,&y);
	x+=pObject->x;
	y+=pObject->y;
	PX_GeoDrawRect(psurface,(px_int)x,(px_int)y,(px_int)(x+pObject->Width-1),(px_int)(y+pObject->Height-1),PX_OBJECT_UI_DEFAULT_BACKGROUNDCOLOR);
	PX_GeoDrawBorder(psurface,(px_int)x,(px_int)y,(px_int)(x+pObject->Width-1),(px_int)(y+pObject->Height-1),1,PX_OBJECT_UI_DEFAULT_BORDERCOLOR);
}

PX_OBJECT_RENDER_FUNCTION(PX_LiveEditorModule_RealtimeBlendPreviewRowRender)
{
	px_float x,y;
	PX_ObjectGetInheritXY(pObject,&x,&y);
	x+=pObject->x;
	y+=pObject->y;
	PX_GeoDrawRect(psurface,(px_int)x,(px_int)y,(px_int)(x+pObject->Width-1),(px_int)(y+pObject->Height-1),PX_OBJECT_UI_DEFAULT_BACKGROUNDCOLOR);
	PX_GeoDrawBorder(psurface,(px_int)x,(px_int)y,(px_int)(x+pObject->Width-1),(px_int)(y+pObject->Height-1),1,PX_OBJECT_UI_DEFAULT_BORDERCOLOR);
}

static px_void PX_LiveEditorModule_RealtimeBlendPreviewRefreshStatus(PX_Object *object)
{
	PX_LiveEditorModule_RealtimeBlendPreview *preview=(PX_LiveEditorModule_RealtimeBlendPreview *)object->pObjectDesc[0];
	px_int i,activeCount=0;
	px_char text[96];
	for (i=0;i<preview->rowCount;i++)
	{
		PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(preview->accessor,i);
		if (!preview->axis_status[i]) continue;
		if (!PX_LiveEditorModule_RealtimeBlendPreviewCanUseAxis(axis))
		{
			PX_Object_LabelSetText(preview->axis_status[i],PX_JsonGetString(preview->language,"realtime.preview unavailable short"));
			PX_Object_LabelSetTextColor(preview->axis_status[i],PX_COLOR(255,128,0,0));
		}
		else if (preview->weightsQ15[i])
		{
			px_int weightPercent=(preview->weightsQ15[i]*100+PX_LIVE_REALTIME_WEIGHT_ONE_Q15/2)/PX_LIVE_REALTIME_WEIGHT_ONE_Q15;
			PX_sprintf1(text,sizeof(text),"%1%",PX_STRINGFORMAT_INT(weightPercent));
			PX_Object_LabelSetText(preview->axis_status[i],text);
			PX_Object_LabelSetTextColor(preview->axis_status[i],PX_COLOR(255,0,120,0));
			activeCount++;
		}
		else
		{
			PX_Object_LabelSetText(preview->axis_status[i],PX_JsonGetString(preview->language,"realtime.preview inactive"));
			PX_Object_LabelSetTextColor(preview->axis_status[i],PX_OBJECT_UI_DEFAULT_FONTCOLOR);
		}
	}
	PX_sprintf2(text,sizeof(text),"%1:%2",PX_STRINGFORMAT_STRING(PX_JsonGetString(preview->language,"realtime.preview active axes")),PX_STRINGFORMAT_INT(activeCount));
	PX_Object_LabelSetText(preview->active_status,text);
}

static px_bool PX_LiveEditorModule_RealtimeBlendPreviewApply(PX_Object *object)
{
	PX_LiveEditorModule_RealtimeBlendPreview *preview=(PX_LiveEditorModule_RealtimeBlendPreview *)object->pObjectDesc[0];
	return PX_LiveEditorRealtimePoseAccessorApplyBlendPreview(preview->accessor,preview->sampleIndices,preview->weightsQ15,PX_LiveEditorRealtimePoseAccessorGetAxisCount(preview->accessor));
}

static px_int PX_LiveEditorModule_RealtimeBlendPreviewFindSlider(PX_LiveEditorModule_RealtimeBlendPreview *preview,PX_Object *slider,px_bool *isWeight)
{
	px_int i;
	for (i=0;i<preview->rowCount;i++)
	{
		if (preview->sample_sliders[i]==slider)
		{
			*isWeight=PX_FALSE;
			return i;
		}
		if (preview->weight_sliders[i]==slider)
		{
			*isWeight=PX_TRUE;
			return i;
		}
	}
	return -1;
}

static px_void PX_LiveEditorModule_RealtimeBlendPreviewOnValueChanged(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_Object *object=(PX_Object *)ptr;
	PX_LiveEditorModule_RealtimeBlendPreview *preview=(PX_LiveEditorModule_RealtimeBlendPreview *)object->pObjectDesc[0];
	px_bool isWeight=PX_FALSE;
	px_int index;
	if (preview->syncing) return;
	index=PX_LiveEditorModule_RealtimeBlendPreviewFindSlider(preview,pObject,&isWeight);
	if (index<0) return;
	if (isWeight)
	{
		px_int percent=PX_Object_SliderBarGetValue(pObject);
		preview->weightsQ15[index]=(px_uint16)((percent*PX_LIVE_REALTIME_WEIGHT_ONE_Q15+50)/100);
	}
	else
	{
		preview->sampleIndices[index]=(px_uchar)PX_Object_SliderBarGetValue(pObject);
	}
	PX_LiveEditorModule_RealtimeBlendPreviewApply(object);
	PX_LiveEditorModule_RealtimeBlendPreviewRefreshStatus(object);
}

static px_void PX_LiveEditorModule_RealtimeBlendPreviewBuildRows(PX_Object *object)
{
	PX_LiveEditorModule_RealtimeBlendPreview *preview=(PX_LiveEditorModule_RealtimeBlendPreview *)object->pObjectDesc[0];
	PX_Object *content;
	px_int i,axisCount=PX_LiveEditorRealtimePoseAccessorGetAxisCount(preview->accessor);
	preview->syncing=PX_TRUE;
	PX_Object_ScrollAreaClear(preview->axis_scroll);
	PX_memset(preview->axis_rows,0,sizeof(preview->axis_rows));
	PX_memset(preview->axis_status,0,sizeof(preview->axis_status));
	PX_memset(preview->sample_sliders,0,sizeof(preview->sample_sliders));
	PX_memset(preview->weight_sliders,0,sizeof(preview->weight_sliders));
	preview->rowCount=axisCount;
	content=PX_Object_ScrollAreaGetIncludedObjects(preview->axis_scroll);
	if (!axisCount)
	{
		PX_Object *label=PX_Object_LabelCreate(preview->uiMp,content,8,12,360,24,PX_JsonGetString(preview->language,"realtime.preview no axes"),preview->fontmodule,PX_COLOR(255,128,0,0));
		if (label) PX_Object_LabelSetAlign(label,PX_ALIGN_CENTER);
	}
	for (i=0;i<axisCount;i++)
	{
		PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(preview->accessor,i);
		PX_Object *row=PX_ObjectCreate(preview->uiMp,content,2,(px_float)(i*(PX_LIVEEDITOR_BLEND_AXIS_ROW_HEIGHT+4)),0,PX_LIVEEDITOR_BLEND_AXIS_ROW_WIDTH,PX_LIVEEDITOR_BLEND_AXIS_ROW_HEIGHT,0);
		PX_Object *name,*sampleLabel,*weightLabel;
		px_bool canUse=PX_LiveEditorModule_RealtimeBlendPreviewCanUseAxis(axis);
		if (!row) break;
		PX_ObjectSetRenderFunction(row,PX_LiveEditorModule_RealtimeBlendPreviewRowRender,0);
		preview->axis_rows[i]=row;
		name=PX_Object_LabelCreate(preview->uiMp,row,6,2,196,18,axis?axis->id:"",preview->fontmodule,PX_OBJECT_UI_DEFAULT_FONTCOLOR);
		if (name) PX_Object_LabelSetAlign(name,PX_ALIGN_LEFTMID);
		preview->axis_status[i]=PX_Object_LabelCreate(preview->uiMp,row,208,2,102,18,"",preview->fontmodule,PX_OBJECT_UI_DEFAULT_FONTCOLOR);
		sampleLabel=PX_Object_LabelCreate(preview->uiMp,row,6,24,40,18,PX_JsonGetString(preview->language,"realtime.sample"),preview->fontmodule,PX_OBJECT_UI_DEFAULT_FONTCOLOR);
		if (sampleLabel) PX_Object_LabelSetAlign(sampleLabel,PX_ALIGN_RIGHTMID);
		preview->sample_sliders[i]=PX_Object_SliderBarCreate(preview->uiMp,row,50,24,260,18,PX_OBJECT_SLIDERBAR_TYPE_HORIZONTAL,PX_OBJECT_SLIDERBAR_STYLE_BOX);
		if (!preview->sample_sliders[i]) break;
		PX_Object_SliderBarSetRange(preview->sample_sliders[i],0,PX_LIVE_REALTIME_SAMPLE_COUNT-1);
		PX_Object_SliderBarSetShowValue(preview->sample_sliders[i],PX_TRUE,PX_OBJECT_UI_DEFAULT_FONTCOLOR);
		PX_Object_SliderBarSetValue(preview->sample_sliders[i],preview->sampleIndices[i]);
		preview->sample_sliders[i]->Enabled=canUse;
		weightLabel=PX_Object_LabelCreate(preview->uiMp,row,6,46,40,18,PX_JsonGetString(preview->language,"realtime.weight"),preview->fontmodule,PX_OBJECT_UI_DEFAULT_FONTCOLOR);
		if (weightLabel) PX_Object_LabelSetAlign(weightLabel,PX_ALIGN_RIGHTMID);
		preview->weight_sliders[i]=PX_Object_SliderBarCreate(preview->uiMp,row,50,46,260,18,PX_OBJECT_SLIDERBAR_TYPE_HORIZONTAL,PX_OBJECT_SLIDERBAR_STYLE_BOX);
		if (!preview->weight_sliders[i]) break;
		PX_Object_SliderBarSetRange(preview->weight_sliders[i],0,100);
		PX_Object_SliderBarSetShowValue(preview->weight_sliders[i],PX_TRUE,PX_OBJECT_UI_DEFAULT_FONTCOLOR);
		PX_Object_SliderBarSetValue(preview->weight_sliders[i],(preview->weightsQ15[i]*100+PX_LIVE_REALTIME_WEIGHT_ONE_Q15/2)/PX_LIVE_REALTIME_WEIGHT_ONE_Q15);
		preview->weight_sliders[i]->Enabled=canUse;
		PX_ObjectRegisterEvent(preview->sample_sliders[i],PX_OBJECT_EVENT_VALUECHANGED,PX_LiveEditorModule_RealtimeBlendPreviewOnValueChanged,object);
		PX_ObjectRegisterEvent(preview->weight_sliders[i],PX_OBJECT_EVENT_VALUECHANGED,PX_LiveEditorModule_RealtimeBlendPreviewOnValueChanged,object);
	}
	preview->rowCount=i;
	PX_Object_ScrollAreaUpdateRange(preview->axis_scroll);
	preview->syncing=PX_FALSE;
	PX_LiveEditorModule_RealtimeBlendPreviewRefreshStatus(object);
}

static px_void PX_LiveEditorModule_RealtimeBlendPreviewOnReset(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_Object *object=(PX_Object *)ptr;
	PX_LiveEditorModule_RealtimeBlendPreview *preview=(PX_LiveEditorModule_RealtimeBlendPreview *)object->pObjectDesc[0];
	px_int i;
	preview->syncing=PX_TRUE;
	for (i=0;i<preview->rowCount;i++)
	{
		PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(preview->accessor,i);
		preview->sampleIndices[i]=axis?axis->defaultSampleIndex:0;
		preview->weightsQ15[i]=PX_LiveEditorModule_RealtimeBlendPreviewCanUseAxis(axis)?PX_LIVE_REALTIME_WEIGHT_ONE_Q15:0;
		if (preview->sample_sliders[i]) PX_Object_SliderBarSetValue(preview->sample_sliders[i],preview->sampleIndices[i]);
		if (preview->weight_sliders[i]) PX_Object_SliderBarSetValue(preview->weight_sliders[i],preview->weightsQ15[i]?100:0);
	}
	preview->syncing=PX_FALSE;
	PX_LiveEditorModule_RealtimeBlendPreviewApply(object);
	PX_LiveEditorModule_RealtimeBlendPreviewRefreshStatus(object);
}

static px_void PX_LiveEditorModule_RealtimeBlendPreviewOnClose(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_LiveEditorModule_RealtimeBlendPreviewClose((PX_Object *)ptr);
}

PX_OBJECT_UPDATE_FUNCTION(PX_LiveEditorModule_RealtimeBlendPreviewUpdate)
{
	PX_LiveEditorModule_RealtimeBlendPreview *preview=(PX_LiveEditorModule_RealtimeBlendPreview *)pObject->pObjectDesc[0];
	/* RenderCurrent intentionally does not advance model state.  The preview
	   therefore owns the RT30/physical update step before drawing. */
	PX_LiveFrameworkUpdate(preview->accessor->plive,elapsed);
	pObject->Width=(px_float)preview->pruntime->surface_width;
	pObject->Height=(px_float)preview->pruntime->surface_height;
	preview->button_close->x=pObject->Width-94;
	preview->axis_panel->x=16;
	preview->axis_panel->y=pObject->Height-PX_LIVEEDITOR_BLEND_AXIS_PANEL_HEIGHT-16;
}

PX_Object *PX_LiveEditorModule_RealtimeBlendPreviewInstall(PX_Object *parent,PX_Runtime *pruntime,px_memorypool *uiMp,PX_FontModule *fm,PX_LiveEditorRealtimePoseAccessor *accessor,PX_Json *language,px_bool closeProcess,PX_LIVEEDITOR_BLENDPREVIEW_INSTALL_ERROR *installError)
{
	PX_Object *object;
	PX_LiveEditorModule_RealtimeBlendPreview desc,*preview;
	if (installError) *installError=PX_LIVEEDITOR_BLENDPREVIEW_INSTALL_ROOT;
	if (!parent || !pruntime || !uiMp || !fm || !accessor || !language) return PX_NULL;
	PX_memset(&desc,0,sizeof(desc));
	desc.pruntime=pruntime;
	desc.uiMp=uiMp;
	desc.fontmodule=fm;
	desc.language=language;
	desc.accessor=accessor;
	desc.closeProcess=closeProcess;
	object=PX_ObjectCreateEx(uiMp,parent,0,0,0,(px_float)pruntime->surface_width,(px_float)pruntime->surface_height,0,0,PX_LiveEditorModule_RealtimeBlendPreviewUpdate,PX_LiveEditorModule_RealtimeBlendPreviewRender,PX_NULL,&desc,sizeof(desc));
	if (!object) return PX_NULL;
	preview=(PX_LiveEditorModule_RealtimeBlendPreview *)object->pObjectDesc[0];
	if (installError) *installError=PX_LIVEEDITOR_BLENDPREVIEW_INSTALL_TITLE;
	preview->title=PX_Object_LabelCreate(uiMp,object,14,5,360,28,PX_JsonGetString(language,"realtime.blend preview"),fm,PX_OBJECT_UI_DEFAULT_FONTCOLOR);
	if (!preview->title) { PX_ObjectDelete(object); return PX_NULL; }
	PX_Object_LabelSetAlign(preview->title,PX_ALIGN_LEFTMID);
	if (installError) *installError=PX_LIVEEDITOR_BLENDPREVIEW_INSTALL_CLOSE_BUTTON;
	preview->button_close=PX_Object_PushButtonCreate(uiMp,object,pruntime->surface_width-94,5,80,28,PX_JsonGetString(language,"realtime.preview finish"),fm);
	if (!preview->button_close) { PX_ObjectDelete(object); return PX_NULL; }
	if (installError) *installError=PX_LIVEEDITOR_BLENDPREVIEW_INSTALL_PANEL;
	preview->axis_panel=PX_ObjectCreate(uiMp,object,16,(px_float)(pruntime->surface_height-PX_LIVEEDITOR_BLEND_AXIS_PANEL_HEIGHT-16),0,PX_LIVEEDITOR_BLEND_AXIS_PANEL_WIDTH,PX_LIVEEDITOR_BLEND_AXIS_PANEL_HEIGHT,0);
	if (!preview->axis_panel) { PX_ObjectDelete(object); return PX_NULL; }
	PX_ObjectSetRenderFunction(preview->axis_panel,PX_LiveEditorModule_RealtimeBlendPreviewPanelRender,0);
	if (installError) *installError=PX_LIVEEDITOR_BLENDPREVIEW_INSTALL_PANEL_HEADER;
	preview->axis_panel_title=PX_Object_LabelCreate(uiMp,preview->axis_panel,8,5,86,24,PX_JsonGetString(language,"realtime.preview axis list"),fm,PX_OBJECT_UI_DEFAULT_FONTCOLOR);
	if (!preview->axis_panel_title) { PX_ObjectDelete(object); return PX_NULL; }
	PX_Object_LabelSetAlign(preview->axis_panel_title,PX_ALIGN_LEFTMID);
	preview->active_status=PX_Object_LabelCreate(uiMp,preview->axis_panel,98,5,156,24,"",fm,PX_OBJECT_UI_DEFAULT_FONTCOLOR);
	preview->button_reset=PX_Object_PushButtonCreate(uiMp,preview->axis_panel,262,4,90,26,PX_JsonGetString(language,"realtime.preview reset all"),fm);
	if (!preview->active_status || !preview->button_reset) { PX_ObjectDelete(object); return PX_NULL; }
	if (installError) *installError=PX_LIVEEDITOR_BLENDPREVIEW_INSTALL_SCROLLAREA;
	preview->axis_scroll=PX_Object_ScrollAreaCreate(uiMp,preview->axis_panel,8,34,PX_LIVEEDITOR_BLEND_AXIS_SCROLL_WIDTH,PX_LIVEEDITOR_BLEND_AXIS_SCROLL_HEIGHT);
	if (!preview->axis_scroll) { PX_ObjectDelete(object); return PX_NULL; }
	PX_Object_ScrollAreaSetBkColor(preview->axis_scroll,PX_OBJECT_UI_DEFAULT_BACKGROUNDCOLOR);
	PX_Object_ScrollAreaSetBorder(preview->axis_scroll,PX_TRUE);
	PX_Object_ScrollAreaSetBorderColor(preview->axis_scroll,PX_OBJECT_UI_DEFAULT_BORDERCOLOR);
	PX_ObjectRegisterEvent(preview->button_close,PX_OBJECT_EVENT_EXECUTE,PX_LiveEditorModule_RealtimeBlendPreviewOnClose,object);
	PX_ObjectRegisterEvent(preview->button_reset,PX_OBJECT_EVENT_EXECUTE,PX_LiveEditorModule_RealtimeBlendPreviewOnReset,object);
	object->Enabled=PX_FALSE;
	object->Visible=PX_FALSE;
	if (installError) *installError=PX_LIVEEDITOR_BLENDPREVIEW_INSTALL_OK;
	return object;
}

px_void PX_LiveEditorModule_RealtimeBlendPreviewOpen(PX_Object *object)
{
	PX_LiveEditorModule_RealtimeBlendPreview *preview;
	px_int i,axisCount;
	if (!object || object->Enabled) return;
	preview=(PX_LiveEditorModule_RealtimeBlendPreview *)object->pObjectDesc[0];
	preview->restoreEntered=preview->accessor->entered;
	preview->restoreAuthoring=preview->accessor->authoringPoseActive;
	preview->restoreAnimationMode=preview->accessor->plive->animationMode;
	axisCount=PX_LiveEditorRealtimePoseAccessorGetAxisCount(preview->accessor);
	for (i=0;i<axisCount;i++)
	{
		PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(preview->accessor,i);
		preview->sampleIndices[i]=axis?axis->defaultSampleIndex:0;
		preview->weightsQ15[i]=PX_LiveEditorModule_RealtimeBlendPreviewCanUseAxis(axis)?PX_LIVE_REALTIME_WEIGHT_ONE_Q15:0;
	}
	object->Enabled=PX_TRUE;
	object->Visible=PX_TRUE;
	PX_ObjectSetFocus(object);
	PX_LiveEditorModule_RealtimeBlendPreviewBuildRows(object);
	/* The preview shell and neutral model must remain visible even if the
	   first RT30 evaluation fails.  Hiding the whole object until Apply
	   succeeded turned every post-import runtime error into a blank window. */
	if (!PX_LiveEditorModule_RealtimeBlendPreviewApply(object))
	{
		PX_LiveEditorRealtimePoseAccessorLeave(preview->accessor);
		PX_Object_LabelSetText(preview->active_status,PX_JsonGetString(preview->language,"realtime.preview initialize failed"));
		PX_Object_LabelSetTextColor(preview->active_status,PX_COLOR(255,160,0,0));
	}
}

px_void PX_LiveEditorModule_RealtimeBlendPreviewClose(PX_Object *object)
{
	PX_LiveEditorModule_RealtimeBlendPreview *preview;
	if (!object || !object->Enabled) return;
	preview=(PX_LiveEditorModule_RealtimeBlendPreview *)object->pObjectDesc[0];
	if (preview->closeProcess)
	{
#ifdef _WIN32
		/* UI events run on PainterEngine's render thread.  PostQuitMessage
		   would target that thread instead of the native window thread and
		   could leave the preview process alive. */
		ExitProcess(0);
#endif
		return;
	}
	if (preview->restoreAuthoring)
	{
		if (!PX_LiveEditorRealtimePoseAccessorApplySelectedKey(preview->accessor)) return;
	}
	else if (preview->restoreEntered)
	{
		PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(preview->accessor,PX_LiveEditorRealtimePoseAccessorGetSelectedAxis(preview->accessor));
		if (axis && axis->runtimeHandle!=PX_LIVE_REALTIME_INVALID_HANDLE && axis->baked && !axis->dirty)
		{
			if (!PX_LiveEditorRealtimePoseAccessorSetPreview(preview->accessor,axis->sampleIndex,axis->weightQ15)) return;
		}
		else PX_LiveEditorRealtimePoseAccessorLeave(preview->accessor);
	}
	else
	{
		PX_LiveEditorRealtimePoseAccessorLeave(preview->accessor);
		if (preview->restoreAnimationMode==PX_LIVE_MODE_TIMELINE) PX_LiveFrameworkRunCurrentEditFrame(preview->accessor->plive);
	}
	object->Enabled=PX_FALSE;
	object->Visible=PX_FALSE;
	PX_ObjectReleaseFocus(object);
}
