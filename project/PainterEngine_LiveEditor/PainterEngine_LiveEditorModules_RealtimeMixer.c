#include "PainterEngine_LiveEditorModules_RealtimeMixer.h"

static px_void PX_LiveEditorModule_RealtimeMixerApply(PX_LiveEditorModule_RealtimeMixer *mixer)
{
	px_int sample=PX_Object_SliderBarGetValue(mixer->slider_sample);
	px_int weightPercent=PX_Object_SliderBarGetValue(mixer->slider_weight);
	px_uint16 weightQ15=(px_uint16)((weightPercent*PX_LIVE_REALTIME_WEIGHT_ONE_Q15+50)/100);
	PX_LiveEditorRealtimePoseAccessorSetPreview(mixer->accessor,sample,weightQ15);
}

static px_void PX_LiveEditorModule_RealtimeMixerOnValueChanged(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_Object *mixerObject=(PX_Object *)ptr;
	PX_LiveEditorModule_RealtimeMixer *mixer=(PX_LiveEditorModule_RealtimeMixer *)mixerObject->pObjectDesc[0];
	if (!mixer->syncing) PX_LiveEditorModule_RealtimeMixerApply(mixer);
}

static px_void PX_LiveEditorModule_RealtimeMixerOnReset(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_Object *mixerObject=(PX_Object *)ptr;
	PX_LiveEditorModule_RealtimeMixer *mixer=(PX_LiveEditorModule_RealtimeMixer *)mixerObject->pObjectDesc[0];
	PX_LiveEditorRealtimePoseAccessorResetPreview(mixer->accessor);
}

PX_OBJECT_UPDATE_FUNCTION(PX_LiveEditorModule_RealtimeMixerUpdate)
{
	PX_LiveEditorModule_RealtimeMixer *mixer=(PX_LiveEditorModule_RealtimeMixer *)pObject->pObjectDesc[0];
	PX_LiveEditorRealtimeAxisAuthoring *axis;
	px_bool canPreview;
	if (mixer->seenRevision==mixer->accessor->revision) return;
	mixer->seenRevision=mixer->accessor->revision;
	axis=PX_LiveEditorRealtimePoseAccessorGetAxis(mixer->accessor,PX_LiveEditorRealtimePoseAccessorGetSelectedAxis(mixer->accessor));
	canPreview=axis && !mixer->accessor->authoringPoseActive && axis->runtimeHandle!=PX_LIVE_REALTIME_INVALID_HANDLE && axis->baked && !axis->dirty;
	mixer->syncing=PX_TRUE;
	if (!canPreview)
	{
		mixer->slider_sample->Enabled=PX_FALSE;
		mixer->slider_weight->Enabled=PX_FALSE;
		mixer->button_reset->Enabled=PX_FALSE;
	}
	else
	{
		px_int weightPercent=(axis->weightQ15*100+PX_LIVE_REALTIME_WEIGHT_ONE_Q15/2)/PX_LIVE_REALTIME_WEIGHT_ONE_Q15;
		mixer->slider_sample->Enabled=PX_TRUE;
		mixer->slider_weight->Enabled=PX_TRUE;
		mixer->button_reset->Enabled=PX_TRUE;
		PX_Object_SliderBarSetValue(mixer->slider_sample,axis->sampleIndex);
		PX_Object_SliderBarSetValue(mixer->slider_weight,weightPercent);
	}
	mixer->syncing=PX_FALSE;
}

PX_OBJECT_RENDER_FUNCTION(PX_LiveEditorModule_RealtimeMixerRender)
{
	px_float x,y;
	PX_ObjectGetInheritXY(pObject,&x,&y);
	x+=pObject->x;
	y+=pObject->y;
	PX_GeoDrawRect(psurface,(px_int)x,(px_int)y,(px_int)(x+pObject->Width-1),(px_int)(y+pObject->Height-1),PX_OBJECT_UI_DEFAULT_BACKGROUNDCOLOR);
	PX_GeoDrawBorder(psurface,(px_int)x,(px_int)y,(px_int)(x+pObject->Width-1),(px_int)(y+pObject->Height-1),1,PX_OBJECT_UI_DEFAULT_BORDERCOLOR);
}

PX_Object *PX_LiveEditorModule_RealtimeMixerInstall(PX_Object *parent,PX_Runtime *pruntime,PX_FontModule *fm,PX_LiveEditorRealtimePoseAccessor *accessor,PX_Json *language,px_int x,px_int y)
{
	PX_Object *object;
	PX_LiveEditorModule_RealtimeMixer desc,*mixer;
	PX_memset(&desc,0,sizeof(desc));
	desc.accessor=accessor;
	desc.fontmodule=fm;
	desc.language=language;
	desc.seenRevision=(px_dword)-1;
	object=PX_ObjectCreateEx(&pruntime->mp_dynamic,parent,(px_float)x,(px_float)y,0,256,58,0,0,PX_LiveEditorModule_RealtimeMixerUpdate,PX_LiveEditorModule_RealtimeMixerRender,PX_NULL,&desc,sizeof(desc));
	if (!object) return PX_NULL;
	mixer=(PX_LiveEditorModule_RealtimeMixer *)object->pObjectDesc[0];
	mixer->label_sample=PX_Object_LabelCreate(&pruntime->mp_dynamic,object,6,4,42,20,PX_JsonGetString(language,"realtime.sample"),fm,PX_OBJECT_UI_DEFAULT_FONTCOLOR);
	mixer->slider_sample=PX_Object_SliderBarCreate(&pruntime->mp_dynamic,object,50,4,200,20,PX_OBJECT_SLIDERBAR_TYPE_HORIZONTAL,PX_OBJECT_SLIDERBAR_STYLE_BOX);
	PX_Object_SliderBarSetRange(mixer->slider_sample,0,PX_LIVE_REALTIME_SAMPLE_COUNT-1);
	PX_Object_SliderBarSetShowValue(mixer->slider_sample,PX_TRUE,PX_OBJECT_UI_DEFAULT_FONTCOLOR);
	PX_ObjectRegisterEvent(mixer->slider_sample,PX_OBJECT_EVENT_VALUECHANGED,PX_LiveEditorModule_RealtimeMixerOnValueChanged,object);
	mixer->label_weight=PX_Object_LabelCreate(&pruntime->mp_dynamic,object,6,31,42,22,PX_JsonGetString(language,"realtime.weight"),fm,PX_OBJECT_UI_DEFAULT_FONTCOLOR);
	mixer->slider_weight=PX_Object_SliderBarCreate(&pruntime->mp_dynamic,object,50,30,148,22,PX_OBJECT_SLIDERBAR_TYPE_HORIZONTAL,PX_OBJECT_SLIDERBAR_STYLE_BOX);
	PX_Object_SliderBarSetRange(mixer->slider_weight,0,100);
	PX_Object_SliderBarSetShowValue(mixer->slider_weight,PX_TRUE,PX_OBJECT_UI_DEFAULT_FONTCOLOR);
	PX_ObjectRegisterEvent(mixer->slider_weight,PX_OBJECT_EVENT_VALUECHANGED,PX_LiveEditorModule_RealtimeMixerOnValueChanged,object);
	mixer->button_reset=PX_Object_PushButtonCreate(&pruntime->mp_dynamic,object,202,29,48,22,PX_JsonGetString(language,"realtime.reset"),fm);
	PX_ObjectRegisterEvent(mixer->button_reset,PX_OBJECT_EVENT_EXECUTE,PX_LiveEditorModule_RealtimeMixerOnReset,object);
	return object;
}
