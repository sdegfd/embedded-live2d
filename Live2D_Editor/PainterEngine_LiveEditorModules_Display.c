#include "PainterEngine_LiveEditorModules_Display.h"
#include "PainterEngine_LiveEditorModules_View.h"
#include "kernel/PX_Object_Menu.h"
#include <stdlib.h>

PX_OBJECT_FREE_FUNCTION(PX_LiveEditorModule_DisplayFree);

static px_void PX_LiveEditorDisplay_FreeCache(PX_LiveEditorModule_Display *pdisplay)
{
	if (pdisplay->modelCache.surfaceBuffer)
	{
		free(pdisplay->modelCache.surfaceBuffer);
	}
	PX_memset(&pdisplay->modelCache,0,sizeof(pdisplay->modelCache));
	pdisplay->modelCacheReady=PX_FALSE;
	pdisplay->pictureValid=PX_FALSE;
}

static px_bool PX_LiveEditorDisplay_EnsureCache(PX_LiveEditorModule_Display *pdisplay)
{
	PX_LiveFramework *live=pdisplay->pLiveFramework;
	px_int width=live->width;
	px_int height=live->height;
	px_int bytes;
	px_color *buffer;
	if (width<=0||height<=0||width>4096||height>4096)
	{
		PX_LiveEditorDisplay_FreeCache(pdisplay);
		return PX_FALSE;
	}
	if (pdisplay->modelCacheReady&&pdisplay->modelCache.width==width&&pdisplay->modelCache.height==height)
	{
		return PX_TRUE;
	}
	PX_LiveEditorDisplay_FreeCache(pdisplay);
	bytes=width*height*(px_int)sizeof(px_color);
	buffer=(px_color *)malloc((size_t)bytes);
	if (!buffer)
	{
		return PX_FALSE;
	}
	PX_memset(buffer,0,bytes);
	pdisplay->modelCache.surfaceBuffer=buffer;
	pdisplay->modelCache.width=width;
	pdisplay->modelCache.height=height;
	pdisplay->modelCache.MP=PX_NULL;
	pdisplay->modelCache.limit_left=0;
	pdisplay->modelCache.limit_top=0;
	pdisplay->modelCache.limit_right=width-1;
	pdisplay->modelCache.limit_bottom=height-1;
	pdisplay->modelCacheReady=PX_TRUE;
	return PX_TRUE;
}

static px_dword PX_LiveEditorDisplay_PictureSignature(PX_LiveFramework *live)
{
	px_dword sig=2166136261u;
	px_int i,v;
	sig=sig*16777619u+(px_dword)live->width;
	sig=sig*16777619u+(px_dword)live->height;
	sig=sig*16777619u+(px_dword)live->showFocusLayer;
	sig=sig*16777619u+(px_dword)live->currentEditLayerIndex;
	sig=sig*16777619u+(px_dword)live->layers.size;
	for (i=0;i<live->layers.size;i++)
	{
		PX_LiveLayer *layer=PX_VECTORAT(PX_LiveLayer,&live->layers,i);
		PX_LiveTexture *tex=PX_LiveFrameworkGetLiveTexture(live,layer->RenderTextureIndex);
		sig=sig*16777619u+(px_dword)layer->visible;
		sig=sig*16777619u+(px_dword)layer->RenderTextureIndex;
		sig=sig*16777619u+(px_dword)layer->vertices.size;
		if (tex&&tex->Texture.surfaceBuffer)
		{
			px_uint64 bits=(px_uint64)(px_void *)tex->Texture.surfaceBuffer;
			sig=sig*16777619u+(px_dword)tex->Texture.width;
			sig=sig*16777619u+(px_dword)tex->Texture.height;
			sig=sig*16777619u+(px_dword)tex->textureOffsetX;
			sig=sig*16777619u+(px_dword)tex->textureOffsetY;
			sig=sig*16777619u+(px_dword)bits;
			sig=sig*16777619u+(px_dword)(bits>>32);
		}
		for (v=0;v<layer->vertices.size;v++)
		{
			PX_LiveVertex *vert=PX_VECTORAT(PX_LiveVertex,&layer->vertices,v);
			sig=sig*16777619u+(px_dword)(vert->currentPosition.x*16.0f);
			sig=sig*16777619u+(px_dword)(vert->currentPosition.y*16.0f);
		}
	}
	return sig;
}

static px_void PX_LiveEditorDisplay_Present(px_surface *dst,px_texture *src,px_float originX,px_float originY,px_float scale)
{
	px_int x0,y0,newW,newH;
	px_int left,top,right,bottom,dy;
	px_int clipLeft,clipTop,clipRight,clipBottom;
	if (scale<=0)
	{
		scale=1.0f;
	}
	x0=(px_int)originX;
	y0=(px_int)originY;
	newW=(px_int)(src->width*scale);
	newH=(px_int)(src->height*scale);
	if (newW<1) newW=1;
	if (newH<1) newH=1;
	clipLeft=dst->limit_left;
	clipTop=dst->limit_top;
	clipRight=dst->limit_right;
	clipBottom=dst->limit_bottom;
	if (clipRight<clipLeft||clipBottom<clipTop)
	{
		clipLeft=0;
		clipTop=0;
		clipRight=dst->width-1;
		clipBottom=dst->height-1;
	}
	left=x0;
	top=y0;
	right=x0+newW;
	bottom=y0+newH;
	if (left<clipLeft) left=clipLeft;
	if (top<clipTop) top=clipTop;
	if (right>clipRight+1) right=clipRight+1;
	if (bottom>clipBottom+1) bottom=clipBottom+1;
	if (left>=right||top>=bottom)
	{
		return;
	}
	if (newW==src->width&&newH==src->height)
	{
		for (dy=top;dy<bottom;dy++)
		{
			PX_memcpy(dst->surfaceBuffer+dy*dst->width+left,src->surfaceBuffer+(dy-y0)*src->width+(left-x0),(right-left)*(px_int)sizeof(px_color));
		}
		return;
	}
	for (dy=top;dy<bottom;dy++)
	{
		px_int sy=(px_int)(((px_int64)(dy-y0)*src->height)/newH);
		px_color *srow;
		px_color *drow;
		px_int dx;
		px_int64 acc;
		px_int64 step;
		if (sy<0) sy=0;
		if (sy>=src->height) sy=src->height-1;
		srow=src->surfaceBuffer+sy*src->width;
		drow=dst->surfaceBuffer+dy*dst->width;
		step=((px_int64)src->width<<16)/newW;
		acc=(px_int64)(left-x0)*step;
		for (dx=left;dx<right;dx++)
		{
			px_int sx=(px_int)(acc>>16);
			acc+=step;
			if ((unsigned)sx>=(unsigned)src->width)
			{
				continue;
			}
			drow[dx]=srow[sx];
		}
	}
}

static px_bool PX_LiveEditorDisplay_OverOpenMenu(PX_Object_Menu_Item *item,px_float x,px_float y)
{
	px_list_node *node;
	if (item->width>0&&item->height>0&&PX_isPointXYInRect(x,y,(px_float)item->x,(px_float)item->y,(px_float)item->width,(px_float)item->height))
	{
		return PX_TRUE;
	}
	if (!item->Activated)
	{
		return PX_FALSE;
	}
	for (node=PX_ListNodeAt(&item->Items,0);node;node=PX_ListNodeNext(node))
	{
		if (PX_LiveEditorDisplay_OverOpenMenu(PX_LIST_NODETDATA(PX_Object_Menu_Item,node),x,y))
		{
			return PX_TRUE;
		}
	}
	return PX_FALSE;
}

static px_bool PX_LiveEditorDisplay_OverMenu(PX_Object *menu,px_float x,px_float y)
{
	PX_Object_Menu_Item *root=PX_Object_MenuGetRootItem(menu);
	px_list_node *node;
	for (node=PX_ListNodeAt(&root->Items,0);node;node=PX_ListNodeNext(node))
	{
		if (PX_LiveEditorDisplay_OverOpenMenu(PX_LIST_NODETDATA(PX_Object_Menu_Item,node),x,y))
		{
			return PX_TRUE;
		}
	}
	return PX_FALSE;
}

px_bool PX_LiveEditorDisplay_OverUi(PX_Object *display,PX_Object_Event e,px_float surfaceW,px_float surfaceH)
{
	PX_Object *root=display->pParent;
	PX_Object *stack[256];
	px_bool shown[256];
	px_int topIndex=0;
	px_float x=PX_Object_Event_GetCursorX(e);
	px_float y=PX_Object_Event_GetCursorY(e);
	if (!root)
	{
		return PX_FALSE;
	}
	stack[0]=root;
	shown[0]=PX_TRUE;
	while (topIndex>=0)
	{
		PX_Object *current=stack[topIndex];
		PX_Object *child;
		px_bool visible=shown[topIndex]&&current->Visible;
		topIndex--;
		if (visible&&PX_ObjectCheckType(current,PX_OBJECT_TYPE_MENU)&&PX_LiveEditorDisplay_OverMenu(current,x,y))
		{
			return PX_TRUE;
		}
		if (current!=display&&visible&&current->Width>1&&current->Height>1)
		{
			px_rect rect=PX_ObjectGetRect(current);
			px_bool coversWindow=rect.width>=surfaceW-1&&rect.height>=surfaceH-1;
			if (!coversWindow&&PX_isPointXYInRect(x,y,rect.x,rect.y,rect.width,rect.height))
			{
				return PX_TRUE;
			}
		}
		if (!visible)
		{
			continue;
		}
		for (child=current->pChilds;child;child=child->pNextBrother)
		{
			if (topIndex+1>=(px_int)(sizeof(stack)/sizeof(stack[0])))
			{
				return PX_TRUE;
			}
			topIndex++;
			stack[topIndex]=child;
			shown[topIndex]=visible;
		}
	}
	return PX_FALSE;
}

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
	if (PX_LiveEditorDisplay_EnsureCache(pdisplay))
	{
		px_dword signature;
		px_float savedScale=pdisplay->pLiveFramework->view_scale;
		PX_LiveFrameworkUpdate(pdisplay->pLiveFramework,elapsed);
		signature=PX_LiveEditorDisplay_PictureSignature(pdisplay->pLiveFramework);
		if (!pdisplay->pictureValid||signature!=pdisplay->pictureSignature)
		{
			/* Draw the model once at its own resolution. Zoom is only the view. */
			pdisplay->pLiveFramework->view_scale=1.0f;
			PX_SurfaceClearAll(&pdisplay->modelCache,PX_COLOR_BACKGROUNDCOLOR);
			PX_LiveFrameworkRenderLayers(&pdisplay->modelCache,pdisplay->pLiveFramework,0,0,PX_ALIGN_LEFTTOP);
			pdisplay->pLiveFramework->view_scale=savedScale;
			pdisplay->pictureSignature=signature;
			pdisplay->pictureValid=PX_TRUE;
		}
		PX_LiveEditorDisplay_Present(psurface,&pdisplay->modelCache,pdisplay->pLiveFramework->refer_x,pdisplay->pLiveFramework->refer_y,savedScale);
		PX_LiveFrameworkRenderOverlay(psurface,pdisplay->pLiveFramework,pdisplay->pLiveFramework->refer_x,pdisplay->pLiveFramework->refer_y,PX_ALIGN_LEFTTOP);
	}
	else
	{
		PX_LiveFrameworkRenderRefer(psurface,pdisplay->pLiveFramework,PX_ALIGN_LEFTTOP,elapsed);
	}

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

px_void PX_LiveEditorDisplay_HandleWheel(PX_Object *self,PX_LiveFramework *live,PX_Runtime *runtime,PX_Object_Event e)
{
	px_float oldScale,newScale,x,y;
	px_float wheel=PX_Object_Event_GetCursorZ(e);
	if(wheel==0||!live->showRange) return;
	if (PX_LiveEditorDisplay_OverUi(self,e,(px_float)runtime->surface_width,(px_float)runtime->surface_height))
	{
		return;
	}
	oldScale=live->view_scale;
	if(oldScale<=0) oldScale=1.0f;
	newScale=oldScale*(wheel>0?1.1f:1.0f/1.1f);
	if(newScale<0.25f)newScale=0.25f;
	if(newScale>8.0f)newScale=8.0f;
	x=PX_Object_Event_GetCursorX(e);
	y=PX_Object_Event_GetCursorY(e);
	live->refer_x=x-(x-live->refer_x)*newScale/oldScale;
	live->refer_y=y-(y-live->refer_y)*newScale/oldScale;
	live->view_scale=newScale;
	live->view_revision++;
}

px_void PX_LiveEditorModule_DisplayOnCursorWheel(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_LiveEditorModule_Display *pdisplay=(PX_LiveEditorModule_Display *)pObject->pObjectDesc[0];
	PX_LiveEditorDisplay_HandleWheel(pObject,pdisplay->pLiveFramework,pdisplay->pruntime,e);
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
	pObject=PX_ObjectCreateEx(&pruntime->mp_dynamic,pparent,0,0,0,0,0,0,0,PX_LiveEditorModule_DisplayUpdate,PX_LiveEditorModule_DisplayRender,PX_LiveEditorModule_DisplayFree,&display,sizeof(display));
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

PX_OBJECT_FREE_FUNCTION(PX_LiveEditorModule_DisplayFree)
{
	PX_LiveEditorModule_Display *pdisplay=(PX_LiveEditorModule_Display *)pObject->pObjectDesc[0];
	if (pdisplay)
	{
		PX_LiveEditorDisplay_FreeCache(pdisplay);
	}
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

