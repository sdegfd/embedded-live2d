#ifndef PX_LIVEEDITOR_MODULE_REALTIMEAXISEDITOR_H
#define PX_LIVEEDITOR_MODULE_REALTIMEAXISEDITOR_H

#include "PainterEngine_LiveEditorModules_RealtimePoseAccessor.h"

typedef struct
{
	PX_LiveEditorRealtimePoseAccessor *accessor;
	PX_FontModule *fontmodule;
	PX_Json *language;
	PX_Object *label_middle;
	PX_Object *slider_middle;
	PX_Object *button_default;
	PX_Object *button_key[3];
	PX_Object *button_copydefault;
	PX_Object *button_bake;
	PX_Object *label_status;
	PX_Object *messagebox;
	px_bool syncing;
	px_dword seenRevision;
}PX_LiveEditorModule_RealtimeAxisEditor;

PX_Object *PX_LiveEditorModule_RealtimeAxisEditorInstall(PX_Object *parent,PX_Runtime *pruntime,PX_FontModule *fm,PX_LiveEditorRealtimePoseAccessor *accessor,PX_Json *language,PX_Object *messagebox,px_int x,px_int y);

#endif
