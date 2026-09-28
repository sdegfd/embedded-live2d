#ifndef PX_LIVEEDITOR_MODULE_REALTIMECONTROLLER_H
#define PX_LIVEEDITOR_MODULE_REALTIMECONTROLLER_H

#include "PainterEngine_LiveEditorModules_RealtimeAxisEditor.h"
#include "PainterEngine_LiveEditorModules_RealtimeMixer.h"

#define PX_LIVEEDITORMODULE_REALTIME_EVENT_MESSAGE 0x00060120

typedef struct
{
	PX_Runtime *pruntime;
	PX_FontModule *fontmodule;
	PX_Json *language;
	PX_LiveFramework *plive;
	PX_LiveEditorRealtimePoseAccessor accessor;
	PX_Object *button_new;
	PX_Object *button_delete;
	PX_Object *button_reload;
	PX_Object *label_axis;
	PX_Object *axis_selectbar;
	PX_Object *axis_editor;
	PX_Object *mixer;
	PX_Object *messagebox;
	px_bool syncingAxisSelect;
	px_dword seenRevision;
}PX_LiveEditorModule_RealtimeController;

PX_Object *PX_LiveEditorModule_RealtimeControllerInstall(PX_Object *panelWidget,PX_Object *overlayParent,PX_Runtime *pruntime,PX_FontModule *fm,PX_LiveFramework *plive,PX_Json *language);
px_void PX_LiveEditorModule_RealtimeControllerUninstall(PX_Object *object);
px_void PX_LiveEditorModule_RealtimeControllerEnable(PX_Object *object);
px_void PX_LiveEditorModule_RealtimeControllerDisable(PX_Object *object);
px_void PX_LiveEditorModule_RealtimeControllerReload(PX_Object *object);
PX_LiveEditorRealtimePoseAccessor *PX_LiveEditorModule_RealtimeControllerGetPoseAccessor(PX_Object *object);

#endif
