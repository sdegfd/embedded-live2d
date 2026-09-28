#ifndef PX_LIVEEDITOR_MODULE_REALTIMEPANC_H
#define PX_LIVEEDITOR_MODULE_REALTIMEPANC_H

#include "PainterEngine_LiveEditorModules_RealtimePoseAccessor.h"

typedef struct
{
	PX_LiveEditorRealtimePoseAccessor *accessor;
	PX_LiveFramework *plive;
	PX_Runtime *pruntime;
	PX_FontModule *fontmodule;
	PX_Object *panc,*bar,*buttonReset,*buttonFinish;
	px_point *baseline;
	px_uint32 baselineCount;
	px_point *baselineTranslations;
	px_uint32 baselineTranslationCount;
	px_int editingLayer;
	PX_PancMatrix modelMatrix;
	px_dword viewRevision;
	px_bool syncing;
}PX_LiveEditorModule_RealtimePanc;

PX_Object *PX_LiveEditorModule_RealtimePancInstall(PX_Object *parent,PX_Runtime *pruntime,PX_FontModule *fm,PX_LiveEditorRealtimePoseAccessor *accessor,PX_Json *language);
px_void PX_LiveEditorModule_RealtimePancEnable(PX_Object *object);
px_void PX_LiveEditorModule_RealtimePancDisable(PX_Object *object);

#endif
