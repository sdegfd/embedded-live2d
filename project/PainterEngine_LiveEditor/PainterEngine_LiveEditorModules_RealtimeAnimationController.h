#ifndef PX_LIVEEDITOR_MODULE_REALTIMEANIMATIONCONTROLLER_H
#define PX_LIVEEDITOR_MODULE_REALTIMEANIMATIONCONTROLLER_H

#include "PainterEngine_LiveEditorModules_RealtimeTransform.h"
#include "PainterEngine_LiveEditorModules_RealtimeVertexTransform.h"
#include "PainterEngine_LiveEditorModules_RealtimeTexture.h"
#include "PainterEngine_LiveEditorModules_RealtimeImpulse.h"
#include "PainterEngine_LiveEditorModules_RealtimePanc.h"

typedef enum
{
	PX_LIVEEDITOR_REALTIME_TOOL_TRANSLATION,
	PX_LIVEEDITOR_REALTIME_TOOL_ROTATION,
	PX_LIVEEDITOR_REALTIME_TOOL_SCALE,
	PX_LIVEEDITOR_REALTIME_TOOL_GLOBAL_ROTATION,
	PX_LIVEEDITOR_REALTIME_TOOL_TEXTURE,
	PX_LIVEEDITOR_REALTIME_TOOL_VERTICES,
	PX_LIVEEDITOR_REALTIME_TOOL_IMPULSE,
	PX_LIVEEDITOR_REALTIME_TOOL_PANC
}PX_LIVEEDITOR_REALTIME_TOOL;

typedef struct
{
	PX_Runtime *pruntime;
	PX_LiveEditorRealtimePoseAccessor *accessor;
	PX_Object *button_translation;
	PX_Object *button_rotation;
	PX_Object *button_scale;
	PX_Object *button_vertices;
	PX_Object *button_globalrotation;
	PX_Object *button_texture;
	PX_Object *button_impulse;
	PX_Object *button_panc;
	PX_Object *transformEditor;
	PX_Object *vertexEditor;
	PX_Object *textureEditor;
	PX_Object *impulseEditor;
	PX_Object *pancEditor;
	px_shape shape_translation;
	px_shape shape_rotation;
	px_shape shape_scale;
	px_shape shape_vertices;
	px_shape shape_globalrotation;
	px_shape shape_texture;
	px_shape shape_impulse;
	px_shape shape_panc;
	PX_LIVEEDITOR_REALTIME_TOOL focus;
	px_bool editable;
	px_int activeAxis;
	px_int activeKeySlot;
	px_char hoverTip[64];
}PX_LiveEditorModule_RealtimeAnimationController;

PX_Object *PX_LiveEditorModule_RealtimeAnimationControllerInstall(PX_Object *parent,PX_Runtime *pruntime,PX_FontModule *fm,PX_LiveEditorRealtimePoseAccessor *accessor,PX_Json *language);
px_void PX_LiveEditorModule_RealtimeAnimationControllerEnable(PX_Object *object);
px_void PX_LiveEditorModule_RealtimeAnimationControllerDisable(PX_Object *object);

#endif
