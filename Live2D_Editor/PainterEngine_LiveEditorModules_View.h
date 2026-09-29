#ifndef PX_LIVEEDITORMODULES_VIEW_H
#define PX_LIVEEDITORMODULES_VIEW_H

#include "runtime/PainterEngine_Runtime.h"

/*
 * The live framework stores every vertex, key point and animation payload in
 * model space.  refer_x/refer_y are the screen-space origin of that model and
 * view_scale is the only scale between the two spaces.  Keep all editor tools
 * on these helpers so rendering and hit testing always use inverse transforms.
 */
static px_float PX_LiveEditorViewGetScale(const PX_LiveFramework *plive)
{
	return plive->view_scale>0?plive->view_scale:1.0f;
}

static px_point2D PX_LiveEditorViewModelToScreen(const PX_LiveFramework *plive,px_point2D point)
{
	px_float scale=PX_LiveEditorViewGetScale(plive);
	return PX_POINT2D(plive->refer_x+point.x*scale,plive->refer_y+point.y*scale);
}

static px_point2D PX_LiveEditorViewScreenToModel(const PX_LiveFramework *plive,px_point2D point)
{
	px_float scale=PX_LiveEditorViewGetScale(plive);
	return PX_POINT2D((point.x-plive->refer_x)/scale,(point.y-plive->refer_y)/scale);
}

static px_float PX_LiveEditorViewScreenToModelLength(const PX_LiveFramework *plive,px_float length)
{
	return length/PX_LiveEditorViewGetScale(plive);
}

static px_float PX_LiveEditorViewModelToScreenLength(const PX_LiveFramework *plive,px_float length)
{
	return length*PX_LiveEditorViewGetScale(plive);
}

/* Core markers are drawn after projection at a fixed screen radius. */
static px_float PX_LiveEditorViewModelMarkerHitRadius(const PX_LiveFramework *plive,px_float modelRadius)
{
	return PX_LiveEditorViewScreenToModelLength(plive,modelRadius>6?modelRadius:6);
}

#endif
