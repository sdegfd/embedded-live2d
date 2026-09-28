#ifndef PX_LIVEEDITOR_MODULE_REALTIMEVERTEXTRANSFORM_H
#define PX_LIVEEDITOR_MODULE_REALTIMEVERTEXTRANSFORM_H

#include "PainterEngine_LiveEditorModules_RealtimePoseAccessor.h"

typedef struct
{
	PX_LiveEditorRealtimePoseAccessor *accessor;
	PX_LiveFramework *plive;
	px_int editingLayer;
	px_int editingVertex;
	px_float cursorLastX,cursorLastY;
}PX_LiveEditorModule_RealtimeVertexTransform;

PX_Object *PX_LiveEditorModule_RealtimeVertexTransformInstall(PX_Object *parent,PX_Runtime *pruntime,PX_LiveEditorRealtimePoseAccessor *accessor);
px_void PX_LiveEditorModule_RealtimeVertexTransformEnable(PX_Object *object);
px_void PX_LiveEditorModule_RealtimeVertexTransformDisable(PX_Object *object);

#endif
