#ifndef PX_LIVEEDITOR_MODULE_REALTIMEIMPULSE_H
#define PX_LIVEEDITOR_MODULE_REALTIMEIMPULSE_H

#include "PainterEngine_LiveEditorModules_RealtimePoseAccessor.h"

typedef struct
{
	PX_LiveEditorRealtimePoseAccessor *accessor;
	PX_LiveFramework *plive;
	PX_Runtime *pruntime;
	px_int editingLayer;
	px_point2D anchorModel,cursorModel;
}PX_LiveEditorModule_RealtimeImpulse;

PX_Object *PX_LiveEditorModule_RealtimeImpulseInstall(PX_Object *parent,PX_Runtime *pruntime,PX_LiveEditorRealtimePoseAccessor *accessor);
px_void PX_LiveEditorModule_RealtimeImpulseEnable(PX_Object *object);
px_void PX_LiveEditorModule_RealtimeImpulseDisable(PX_Object *object);

#endif
