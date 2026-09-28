#ifndef PX_LIVEEDITOR_MODULE_REALTIMETEXTURE_H
#define PX_LIVEEDITOR_MODULE_REALTIMETEXTURE_H

#include "PainterEngine_LiveEditorModules_RealtimePoseAccessor.h"

typedef struct
{
	PX_LiveEditorRealtimePoseAccessor *accessor;
	PX_LiveFramework *plive;
	PX_Runtime *pruntime;
	PX_Object *dialog,*editTexture,*buttonOk,*buttonCancel;
}PX_LiveEditorModule_RealtimeTexture;

PX_Object *PX_LiveEditorModule_RealtimeTextureInstall(PX_Object *parent,PX_Runtime *pruntime,PX_FontModule *fm,PX_LiveEditorRealtimePoseAccessor *accessor,PX_Json *language);
px_void PX_LiveEditorModule_RealtimeTextureEnable(PX_Object *object);
px_void PX_LiveEditorModule_RealtimeTextureDisable(PX_Object *object);

#endif
