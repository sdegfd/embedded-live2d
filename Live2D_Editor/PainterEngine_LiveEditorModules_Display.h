#ifndef PX_LIVEFRAMEWORKMODULES_DISPLAY_H
#define PX_LIVEFRAMEWORKMODULES_DISPLAY_H

#include "runtime/PainterEngine_Runtime.h"
#include "platform/modules/px_file.h"

typedef struct
{
	PX_LiveFramework *pLiveFramework;
	PX_Runtime *pruntime;
	PX_Json *pLanguageJson;
	PX_FontModule *fm;
	px_bool showFPS;
	px_bool showHelpline;
	px_point2D helpPoint;
	px_float fps;
	px_int  _tRenderFrames;
	px_dword elapsed;
	px_texture modelCache;
	px_bool modelCacheReady;
	px_bool pictureValid;
	px_dword pictureSignature;
}PX_LiveEditorModule_Display;

PX_Object* PX_LiveEditorModule_DisplayInstall(PX_Object *pparent,PX_Runtime *pruntime,PX_FontModule *fm,PX_LiveFramework *pLiveFramework,PX_Json *pLanguageJson);
px_void PX_LiveEditorModule_DisplayUninstall(PX_Object *pObject);

px_void PX_LiveEditorModule_DisplayShowFPS(PX_Object *pObject);
px_void PX_LiveEditorModule_DisplayShowHelperLine(PX_Object *pObject);
px_void PX_LiveEditorModule_DisplayResetPosition(PX_Object *pObject);
px_void PX_LiveEditorModule_DisplayEnable(PX_Object *pObject);
px_void PX_LiveEditorModule_DisplayDisable(PX_Object *pObject);
/* True when the cursor is on a visible editor control. self is excluded; its parent is the editor root. */
px_bool PX_LiveEditorDisplay_OverUi(PX_Object *self,PX_Object_Event e,px_float surfaceW,px_float surfaceH);
/* Zoom while the model selection box is visible. self is excluded from the UI hit test; its parent is the editor root. */
px_void PX_LiveEditorDisplay_HandleWheel(PX_Object *self,PX_LiveFramework *live,PX_Runtime *runtime,PX_Object_Event e);
#endif
