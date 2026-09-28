#ifndef PX_LIVEFRAMEWORKMODULES_EXPORTLIVE_H
#define PX_LIVEFRAMEWORKMODULES_EXPORTLIVE_H

#include "runtime/PainterEngine_Runtime.h"
#include "platform/modules/px_file.h"
#include "PainterEngine_LiveEditorModules_RealtimePoseAccessor.h"


#define  PX_LIVEEDITORMODULE_EXPORTLIVE_EVENT_MESSAGE 0x00100000


typedef struct
{
	PX_LiveFramework *pLiveFramework;
	PX_Runtime *pruntime;
	PX_Object *explorer;
	PX_FontModule *fontmodule;
	PX_Json *pLanguageJson;
	PX_LiveEditorRealtimePoseAccessor *poseAccessor;
}PX_LiveEditorModule_ExportLive;

PX_Object * PX_LiveEditorModule_ExportLiveInstall(PX_Object *parent,PX_Runtime *pruntime,PX_FontModule *fm,PX_LiveFramework *pLiveFramework,PX_Json *pLanguageJson);
px_void PX_LiveEditorModule_ExportLiveUninstall(PX_Object *pObject);

px_void PX_LiveEditorModule_ExportLiveSetPoseAccessor(PX_Object *pObject,PX_LiveEditorRealtimePoseAccessor *accessor);
px_void PX_LiveEditorModule_ExportLiveEnable(PX_Object *pObject);
px_void PX_LiveEditorModule_ExportLiveDisable(PX_Object *pObject);
#endif   