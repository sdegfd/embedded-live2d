#ifndef PX_LIVEEDITOR_MODULE_REALTIMEMIXER_H
#define PX_LIVEEDITOR_MODULE_REALTIMEMIXER_H

#include "PainterEngine_LiveEditorModules_RealtimePoseAccessor.h"

typedef struct
{
	PX_LiveEditorRealtimePoseAccessor *accessor;
	PX_FontModule *fontmodule;
	PX_Json *language;
	PX_Object *label_sample;
	PX_Object *slider_sample;
	PX_Object *label_weight;
	PX_Object *slider_weight;
	PX_Object *button_reset;
	px_bool syncing;
	px_dword seenRevision;
}PX_LiveEditorModule_RealtimeMixer;

PX_Object *PX_LiveEditorModule_RealtimeMixerInstall(PX_Object *parent,PX_Runtime *pruntime,PX_FontModule *fm,PX_LiveEditorRealtimePoseAccessor *accessor,PX_Json *language,px_int x,px_int y);

#endif
