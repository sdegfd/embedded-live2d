#ifndef PX_LIVEEDITOR_MODULE_REALTIMEBLENDPREVIEW_H
#define PX_LIVEEDITOR_MODULE_REALTIMEBLENDPREVIEW_H

#include "PainterEngine_LiveEditorModules_RealtimePoseAccessor.h"

typedef struct
{
	PX_Runtime *pruntime;
	px_memorypool *uiMp;
	PX_FontModule *fontmodule;
	PX_Json *language;
	PX_LiveEditorRealtimePoseAccessor *accessor;
	PX_Object *title;
	PX_Object *button_close;
	PX_Object *axis_panel;
	PX_Object *axis_panel_title;
	PX_Object *active_status;
	PX_Object *axis_scroll;
	PX_Object *button_reset;
	PX_Object *axis_rows[PX_LIVE_REALTIME_MAX_AXES];
	PX_Object *axis_status[PX_LIVE_REALTIME_MAX_AXES];
	PX_Object *sample_sliders[PX_LIVE_REALTIME_MAX_AXES];
	PX_Object *weight_sliders[PX_LIVE_REALTIME_MAX_AXES];
	px_uchar sampleIndices[PX_LIVE_REALTIME_MAX_AXES];
	px_uint16 weightsQ15[PX_LIVE_REALTIME_MAX_AXES];
	px_int rowCount;
	px_bool syncing;
	px_bool restoreEntered;
	px_bool restoreAuthoring;
	px_int restoreAnimationMode;
	px_bool closeProcess;
}PX_LiveEditorModule_RealtimeBlendPreview;

typedef enum
{
	PX_LIVEEDITOR_BLENDPREVIEW_INSTALL_OK,
	PX_LIVEEDITOR_BLENDPREVIEW_INSTALL_ROOT,
	PX_LIVEEDITOR_BLENDPREVIEW_INSTALL_TITLE,
	PX_LIVEEDITOR_BLENDPREVIEW_INSTALL_CLOSE_BUTTON,
	PX_LIVEEDITOR_BLENDPREVIEW_INSTALL_PANEL,
	PX_LIVEEDITOR_BLENDPREVIEW_INSTALL_PANEL_HEADER,
	PX_LIVEEDITOR_BLENDPREVIEW_INSTALL_SCROLLAREA
}PX_LIVEEDITOR_BLENDPREVIEW_INSTALL_ERROR;

PX_Object *PX_LiveEditorModule_RealtimeBlendPreviewInstall(PX_Object *parent,PX_Runtime *pruntime,px_memorypool *uiMp,PX_FontModule *fm,PX_LiveEditorRealtimePoseAccessor *accessor,PX_Json *language,px_bool closeProcess,PX_LIVEEDITOR_BLENDPREVIEW_INSTALL_ERROR *installError);
px_void PX_LiveEditorModule_RealtimeBlendPreviewOpen(PX_Object *object);
px_void PX_LiveEditorModule_RealtimeBlendPreviewClose(PX_Object *object);

#endif
