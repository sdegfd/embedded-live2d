#include "PainterEngine_LiveEditorModules_Translation.h"
#include "PainterEngine_LiveEditorModules_Display.h"



px_void PX_LiveEditorModule_TranslationOnCursorDown(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_LiveEditorModule_Translation *pTranslation=(PX_LiveEditorModule_Translation *)pObject->pObjectDesc[0];
	px_float x,y,livex,livey,livewidth,liveheight;
	x=PX_Object_Event_GetCursorX(e);
	y=PX_Object_Event_GetCursorY(e);
	/* The zoomed model rectangle covers the toolbar, but those pixels belong to the controls drawn on top.
	   Releasing focus lets this same click continue to the toolbar. */
	if (PX_LiveEditorDisplay_OverUi(pObject,e,(px_float)pTranslation->pruntime->surface_width,(px_float)pTranslation->pruntime->surface_height))
	{
		pTranslation->bSelect=PX_FALSE;
		if (pObject->OnFocus)
		{
			PX_ObjectReleaseFocus(pObject);
		}
		return;
	}
	livex=pTranslation->pLiveFramework->refer_x;
	livey=pTranslation->pLiveFramework->refer_y;
	livewidth=(px_float)pTranslation->pLiveFramework->width;
	liveheight=(px_float)pTranslation->pLiveFramework->height;
	livewidth*=pTranslation->pLiveFramework->view_scale>0?pTranslation->pLiveFramework->view_scale:1.0f;
	liveheight*=pTranslation->pLiveFramework->view_scale>0?pTranslation->pLiveFramework->view_scale:1.0f;

	if(PX_isXYInRegion(x,y,livex,livey,livewidth,liveheight))
	{
		pTranslation->cursor_last_x=x;
		pTranslation->cursor_last_y=y;
		pTranslation->bSelect=PX_TRUE;
		pTranslation->pLiveFramework->showRange=PX_TRUE;
		PX_ObjectSetFocus(pObject);
	}
	else
	{
		pTranslation->bSelect=PX_FALSE;
		pTranslation->pLiveFramework->showRange=PX_FALSE;
		PX_ObjectReleaseFocus(pObject);
	}
}


px_void PX_LiveEditorModule_TranslationOnCursorDrag(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	px_float x,y;
	PX_LiveEditorModule_Translation *pTranslation=(PX_LiveEditorModule_Translation *)pObject->pObjectDesc[0];
	if (!pTranslation->bSelect)
	{
		return;
	}

	x=PX_Object_Event_GetCursorX(e);
	y=PX_Object_Event_GetCursorY(e);

	pTranslation->pLiveFramework->refer_x+=x-pTranslation->cursor_last_x;
	pTranslation->pLiveFramework->refer_y+=y-pTranslation->cursor_last_y;
	pTranslation->pLiveFramework->view_revision++;

	pTranslation->cursor_last_x=x;
	pTranslation->cursor_last_y=y;

}

/* Clicking the model focuses this object, so the display stops receiving the wheel. */
px_void PX_LiveEditorModule_TranslationOnCursorWheel(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_LiveEditorModule_Translation *pTranslation=(PX_LiveEditorModule_Translation *)pObject->pObjectDesc[0];
	if (PX_LiveEditorDisplay_OverUi(pObject,e,(px_float)pTranslation->pruntime->surface_width,(px_float)pTranslation->pruntime->surface_height))
	{
		if (pObject->OnFocus)
		{
			PX_ObjectReleaseFocus(pObject);
		}
		return;
	}
	if (!pObject->OnFocus)
	{
		return;
	}
	PX_LiveEditorDisplay_HandleWheel(pObject,pTranslation->pLiveFramework,pTranslation->pruntime,e);
}


PX_Object * PX_LiveEditorModule_TranslationInstall(PX_Object *pparent,PX_Runtime *pruntime,PX_FontModule *fm,PX_LiveFramework *pLiveFramework,PX_Json *pLanguageJson)
{
	PX_Object *pObject;
	PX_LiveEditorModule_Translation translation,*ptranslation;
	PX_memset(&translation,0,sizeof(translation));
	pObject=PX_ObjectCreateEx(&pruntime->mp_dynamic,pparent,0,0,0,0,0,0,0,PX_NULL,PX_NULL,PX_NULL,&translation,sizeof(translation));
	ptranslation=(PX_LiveEditorModule_Translation *)pObject->pObjectDesc[0];
	ptranslation->pLiveFramework=pLiveFramework;
	ptranslation->pruntime=pruntime;

	PX_ObjectRegisterEvent(pObject,PX_OBJECT_EVENT_CURSORDOWN,PX_LiveEditorModule_TranslationOnCursorDown,PX_NULL);
	PX_ObjectRegisterEvent(pObject,PX_OBJECT_EVENT_CURSORDRAG,PX_LiveEditorModule_TranslationOnCursorDrag,PX_NULL);
	PX_ObjectRegisterEvent(pObject,PX_OBJECT_EVENT_CURSORWHEEL,PX_LiveEditorModule_TranslationOnCursorWheel,PX_NULL);
	return pObject;
}

px_void PX_LiveEditorModule_TranslationUninstall(PX_Object *pObject)
{
	PX_ObjectDelete(pObject);
}

px_void PX_LiveEditorModule_TranslationEnable(PX_Object *pObject)
{
	pObject->Enabled=PX_TRUE;
	pObject->Visible=PX_TRUE;
}

px_void PX_LiveEditorModule_TranslationDisable(PX_Object *pObject)
{
	pObject->Enabled=PX_FALSE;
	pObject->Visible=PX_FALSE;
}
