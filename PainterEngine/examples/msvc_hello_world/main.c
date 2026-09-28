#include "../../PainterEngine.h"

int px_main()
{
	if (!PainterEngine_Initialize(960, 540))
	{
		return 0;
	}

	PainterEngine_SetWindowText("PainterEngine Hello World");
	PainterEngine_SetBackgroundColor(PX_COLOR(255, 246, 248, 252));
	PainterEngine_PrintSetBackgroundColor(PX_COLOR_NONE);
	PainterEngine_PrintSetFontColor(PX_COLOR(255, 32, 48, 72));
	PainterEngine_DrawText(
		PainterEngine_GetSurfaceWidth() / 2,
		PainterEngine_GetSurfaceHeight() / 2,
		"Hello World",
		PX_ALIGN_CENTER,
		PX_COLOR(255, 24, 40, 64));

	return 1;
}