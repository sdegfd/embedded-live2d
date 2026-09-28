#ifndef PX_LIVEEDITOR_MODULE_REALTIMETRANSFORM_H
#define PX_LIVEEDITOR_MODULE_REALTIMETRANSFORM_H

#include "PainterEngine_LiveEditorModules_RealtimePoseAccessor.h"

typedef enum
{
	PX_LIVEEDITOR_REALTIME_TRANSFORM_TRANSLATION,
	PX_LIVEEDITOR_REALTIME_TRANSFORM_ROTATION,
	PX_LIVEEDITOR_REALTIME_TRANSFORM_SCALE,
	PX_LIVEEDITOR_REALTIME_TRANSFORM_GLOBAL_ROTATION
}PX_LIVEEDITOR_REALTIME_TRANSFORM_MODE;

typedef struct
{
	PX_LiveEditorRealtimePoseAccessor *accessor;
	PX_LiveFramework *plive;
	PX_Runtime *pruntime;
	PX_Object *adapter;
	PX_LIVEEDITOR_REALTIME_TRANSFORM_MODE mode;
	px_int editingLayer;
	px_float cursorLastX,cursorLastY;
}PX_LiveEditorModule_RealtimeTransform;

PX_Object *PX_LiveEditorModule_RealtimeTransformInstall(PX_Object *parent,PX_Runtime *pruntime,PX_LiveEditorRealtimePoseAccessor *accessor);
px_void PX_LiveEditorModule_RealtimeTransformEnable(PX_Object *object,PX_LIVEEDITOR_REALTIME_TRANSFORM_MODE mode);
px_void PX_LiveEditorModule_RealtimeTransformDisable(PX_Object *object);

#endif
