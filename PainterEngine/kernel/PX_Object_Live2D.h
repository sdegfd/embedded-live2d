#ifndef PX_OBJECT_LIVE2D_H
#define PX_OBJECT_LIVE2D_H
#include "PX_Object.h"
#include "PX_LiveFramework.h"

typedef struct 
{
	PX_Live live;
}PX_Object_Live2D;

PX_Object* PX_Object_Live2DAttachObject(PX_Object* pObject, px_int attachIndex, px_int x, px_int y, PX_LiveFramework* pLiveFramework);
PX_Object* PX_Object_Live2DCreate(px_memorypool* mp, PX_Object* Parent, px_int x, px_int y, PX_LiveFramework *pLiveFramework);
px_void PX_Object_Live2DPlayAnimation(PX_Object *pObject,px_char *name);
px_void PX_Object_Live2DPlayAnimationRandom(PX_Object* pObject);
px_void PX_Object_Live2DPlayAnimationIndex(PX_Object* pObject, px_int index);
px_bool PX_Object_Live2DEnterRealtime30(PX_Object *pObject);
px_void PX_Object_Live2DLeaveRealtime30(PX_Object *pObject);
px_int PX_Object_Live2DFindRealtimeAxis(PX_Object *pObject,const px_char id[]);
px_bool PX_Object_Live2DSetRealtimeAxisSample(PX_Object *pObject,px_int axisHandle,px_uchar sampleIndex,px_uint16 weightQ15);
px_bool PX_Object_Live2DSetRealtimeAxisNormalizedQ15(PX_Object *pObject,px_int axisHandle,px_uint16 normalizedQ15,px_uint16 weightQ15);
#endif



