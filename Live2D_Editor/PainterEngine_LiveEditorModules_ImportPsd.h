#ifndef PX_LIVEEDITOR_IMPORTPSD_H
#define PX_LIVEEDITOR_IMPORTPSD_H

#include "runtime/PainterEngine_Runtime.h"
#include "platform/modules/px_file.h"

#define PX_LIVEEDITORMODULE_IMPORTPSD_EVENT_MESSAGE 0x00040010
#define PX_LIVEEDITORMODULE_IMPORTPSD_EVENT_EXIT 0x00040011

typedef struct
{
	PX_LiveFramework *pLiveFramework;
	PX_Runtime *pruntime;
	PX_FontModule *fontmodule;
	PX_Json *pLanguageJson;
} PX_LiveEditorModule_ImportPsd;

PX_Object *PX_LiveEditorModule_ImportPsdInstall(PX_Object *parent, PX_Runtime *pruntime, PX_FontModule *fm, PX_LiveFramework *pLiveFramework, PX_Json *pLanguageJson);
px_void PX_LiveEditorModule_ImportPsdUninstall(PX_Object *pObject);
px_void PX_LiveEditorModule_ImportPsdEnable(PX_Object *pObject);
px_void PX_LiveEditorModule_ImportPsdDisable(PX_Object *pObject);

#endif
