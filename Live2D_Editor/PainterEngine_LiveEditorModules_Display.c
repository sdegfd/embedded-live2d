#include "PainterEngine_LiveEditorModules_Display.h"
#include "PainterEngine_LiveEditorModules_View.h"

PX_OBJECT_UPDATE_FUNCTION(PX_LiveEditorModule_DisplayUpdate)
{
	PX_LiveEditorModule_Display *pdisplay=(PX_LiveEditorModule_Display *)pObject->pObjectDesc[0];

}


PX_OBJECT_RENDER_FUNCTION(PX_LiveEditorModule_DisplayRender)
{
	PX_LiveEditorModule_Display *pdisplay=(PX_LiveEditorModule_Display *)pObject->pObjectDesc[0];
	if(pdisplay->pLiveFramework->view_scale<=0)
	{
		pdisplay->pLiveFramework->view_scale=1.0f;
		pdisplay->pLiveFramework->view_revision++;
	}
	PX_LiveFrameworkRenderRefer(psurface,pdisplay->pLiveFramework,PX_ALIGN_LEFTTOP,elapsed);

	pdisplay->elapsed+=elapsed;
	pdisplay->_tRenderFrames++;

	if (pdisplay->elapsed>1000)
	{
		pdisplay->fps=pdisplay->_tRenderFrames/(pdisplay->elapsed/1000.0f);
		pdisplay->elapsed=0;
		pdisplay->_tRenderFrames=0;
	}

	if (pdisplay->showFPS)
	{
		px_char content[32];
		PX_sprintf1(content,sizeof(content),"FPS:%1.2",PX_STRINGFORMAT_FLOAT(pdisplay->fps));
		if (pdisplay->fps<20)
		{
			PX_FontModuleDrawText(psurface,pdisplay->fm,psurface->width-5,psurface->height-5,PX_ALIGN_RIGHTBOTTOM,content,PX_COLOR(255,255,0,0));
		}
		else if(pdisplay->fps<36)
		{
			PX_FontModuleDrawText(psurface,pdisplay->fm,psurface->width-5,psurface->height-5,PX_ALIGN_RIGHTBOTTOM,content,PX_COLOR(255,255,255,0));
		}
		else
		{
			PX_FontModuleDrawText(psurface,pdisplay->fm,psurface->width-5,psurface->height-5,PX_ALIGN_RIGHTBOTTOM,content,PX_COLOR(255,0,255,0));
		}
	}

	if (pdisplay->showHelpline)
	{
		//horizontal
		PX_GeoDrawLine(psurface,0,(px_int)pdisplay->helpPoint.y,(px_int)psurface->width-1,(px_int)pdisplay->helpPoint.y,1,PX_COLOR(128,255,0,0));
		//vertical
		PX_GeoDrawLine(psurface,(px_int)pdisplay->helpPoint.x,0,(px_int)pdisplay->helpPoint.x,(px_int)psurface->height-1,1,PX_COLOR(128,255,0,0));
	}
}

px_void PX_LiveEditorModule_DisplayOnCursorWheel(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_LiveEditorModule_Display *pdisplay=(PX_LiveEditorModule_Display *)pObject->pObjectDesc[0];
	px_float oldScale=pdisplay->pLiveFramework->view_scale;
	px_float wheel=PX_Object_Event_GetCursorZ(e);
	if(wheel==0) return;
	if(oldScale<=0)
	{
		oldScale=1.0f;
		pdisplay->pLiveFramework->view_scale=oldScale;
	}
	px_float newScale=oldScale*(wheel>0?1.1f:1.0f/1.1f);
	px_float x=PX_Object_Event_GetCursorX(e),y=PX_Object_Event_GetCursorY(e);
	if(newScale<0.25f)newScale=0.25f;
	if(newScale>8.0f)newScale=8.0f;
	pdisplay->pLiveFramework->refer_x=x-(x-pdisplay->pLiveFramework->refer_x)*newScale/oldScale;
	pdisplay->pLiveFramework->refer_y=y-(y-pdisplay->pLiveFramework->refer_y)*newScale/oldScale;
	pdisplay->pLiveFramework->view_scale=newScale;
	pdisplay->pLiveFramework->view_revision++;
}

px_void PX_LiveEditorModule_DisplayOnCursorClick(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_LiveEditorModule_Display *pdisplay=(PX_LiveEditorModule_Display *)pObject->pObjectDesc[0];
	if (pdisplay->showHelpline)
	{
		pdisplay->helpPoint.x=PX_Object_Event_GetCursorX(e);
		pdisplay->helpPoint.y=PX_Object_Event_GetCursorY(e);
	}
}


PX_Object* PX_LiveEditorModule_DisplayInstall(PX_Object *pparent,PX_Runtime *pruntime,PX_FontModule *fm,PX_LiveFramework *pLiveFramework,PX_Json *pLanguageJson)
{
	PX_Object *pObject;
	PX_LiveEditorModule_Display display,*pdisplay;
	PX_memset(&display,0,sizeof(display));
	pObject=PX_ObjectCreateEx(&pruntime->mp_dynamic,pparent,0,0,0,0,0,0,0,PX_LiveEditorModule_DisplayUpdate,PX_LiveEditorModule_DisplayRender,PX_NULL,&display,sizeof(display));
	pdisplay=(PX_LiveEditorModule_Display *)pObject->pObjectDesc[0];
	pdisplay->pLiveFramework=pLiveFramework;
	pdisplay->pruntime=pruntime;
	pdisplay->fm=fm;
	pdisplay->pLanguageJson=pLanguageJson;
	pdisplay->showFPS=PX_TRUE;
	pdisplay->showHelpline=PX_FALSE;
	PX_ObjectRegisterEvent(pObject,PX_OBJECT_EVENT_CURSORCLICK,PX_LiveEditorModule_DisplayOnCursorClick,PX_NULL);
	PX_ObjectRegisterEvent(pObject,PX_OBJECT_EVENT_CURSORWHEEL,PX_LiveEditorModule_DisplayOnCursorWheel,PX_NULL);
	return pObject;
}

px_void PX_LiveEditorModule_DisplayUninstall(PX_Object *pObject)
{
	PX_ObjectDelete(pObject);
}

px_void PX_LiveEditorModule_DisplayShowFPS(PX_Object *pObject)
{
	PX_LiveEditorModule_Display *pdisplay=(PX_LiveEditorModule_Display *)pObject->pObjectDesc[0];
	pdisplay->showFPS=!pdisplay->showFPS;
}

px_void PX_LiveEditorModule_DisplayShowHelperLine(PX_Object *pObject)
{
	PX_LiveEditorModule_Display *pdisplay=(PX_LiveEditorModule_Display *)pObject->pObjectDesc[0];
	pdisplay->showHelpline=!pdisplay->showHelpline;
}

px_void PX_LiveEditorModule_DisplayResetPosition(PX_Object *pObject)
{
	PX_LiveEditorModule_Display *pdisplay=(PX_LiveEditorModule_Display *)pObject->pObjectDesc[0];
	px_float scale=pdisplay->pLiveFramework->view_scale;
	if(scale<=0) scale=1.0f;
	pdisplay->pLiveFramework->refer_x=(px_float)(pdisplay->pruntime->surface_width-pdisplay->pLiveFramework->width*scale)/2;
	pdisplay->pLiveFramework->refer_y=(px_float)(pdisplay->pruntime->surface_height-pdisplay->pLiveFramework->height*scale)/2;
	pdisplay->pLiveFramework->view_revision++;
}

px_void PX_LiveEditorModule_DisplayEnable(PX_Object *pObject)
{
	pObject->Enabled=PX_TRUE;
	pObject->Visible=PX_TRUE;
}

px_void PX_LiveEditorModule_DisplayDisable(PX_Object *pObject)
{
	pObject->Enabled=PX_FALSE;
	pObject->Visible=PX_FALSE;
}

