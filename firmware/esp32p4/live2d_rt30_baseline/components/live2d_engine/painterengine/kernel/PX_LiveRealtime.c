#include "PX_LiveFramework.h"

#define PX_LIVE_REALTIME_MAX_FRACTION_BITS 14

static px_uint32 PX_LiveRealtimeAlign4(px_uint32 value)
{
	return (value+3u)&~3u;
}

static px_void PX_LiveRealtimeMarkDirty(PX_LiveRealtime *realtime)
{
	realtime->dirty=PX_TRUE;
	realtime->stateRevision++;
	if (realtime->stateRevision==0)
	{
		realtime->stateRevision=1;
		realtime->evaluatedRevision=0;
	}
}

px_void PX_LiveRealtimeInvalidateContributions(PX_LiveFramework *plive)
{
	if (!plive)
	{
		return;
	}
	plive->realtime.contributionValid=PX_FALSE;
	PX_memset(plive->realtime.appliedSample,0,sizeof(plive->realtime.appliedSample));
}

static px_void PX_LiveRealtimeFreeAxis(px_memorypool *mp,PX_LiveRealtimeAxis *axis)
{
	if (axis->bindings)
	{
		MP_Free(mp,axis->bindings);
	}
	if (axis->vertexIndices)
	{
		MP_Free(mp,axis->vertexIndices);
	}
	if (axis->samples)
	{
		MP_Free(mp,axis->samples);
	}
	PX_memset(axis,0,sizeof(*axis));
}

static px_uint32 PX_LiveRealtimeAxisStaticBytes(const PX_LiveRealtimeAxis *axis)
{
	return (px_uint32)axis->bindingCount*(px_uint32)sizeof(PX_LiveRealtimeBinding)+
		axis->vertexIndexCount*(px_uint32)sizeof(px_uint16)+axis->sampleBytes;
}

static px_bool PX_LiveRealtimeCopyAxis(px_memorypool *mp,PX_LiveRealtimeAxis *dest,const PX_LiveRealtimeAxis *source)
{
	PX_memset(dest,0,sizeof(*dest));
	*dest=*source;
	dest->bindings=PX_NULL;
	dest->vertexIndices=PX_NULL;
	dest->samples=PX_NULL;

	if (source->bindingCount)
	{
		dest->bindings=(PX_LiveRealtimeBinding *)MP_Malloc(mp,(px_uint)source->bindingCount*sizeof(PX_LiveRealtimeBinding));
		if (!dest->bindings)
		{
			goto _ERROR;
		}
		PX_memcpy(dest->bindings,source->bindings,(px_uint)source->bindingCount*sizeof(PX_LiveRealtimeBinding));
	}
	if (source->vertexIndexCount)
	{
		dest->vertexIndices=(px_uint16 *)MP_Malloc(mp,source->vertexIndexCount*(px_uint)sizeof(px_uint16));
		if (!dest->vertexIndices)
		{
			goto _ERROR;
		}
		PX_memcpy(dest->vertexIndices,source->vertexIndices,source->vertexIndexCount*(px_uint)sizeof(px_uint16));
	}
	if (source->sampleBytes)
	{
		dest->samples=(px_int16 *)MP_Malloc(mp,source->sampleBytes);
		if (!dest->samples)
		{
			goto _ERROR;
		}
		PX_memcpy(dest->samples,source->samples,source->sampleBytes);
	}
	return PX_TRUE;
_ERROR:
	PX_LiveRealtimeFreeAxis(mp,dest);
	return PX_FALSE;
}

px_uint32 PX_LiveRealtimeHashId(const px_char id[])
{
	px_uint32 hash=2166136261u;
	px_int i=0;
	if (!id)
	{
		return 0;
	}
	while (id[i])
	{
		hash^=(px_uchar)id[i];
		hash*=16777619u;
		i++;
	}
	return hash;
}

px_void PX_LiveRealtimeInitialize(PX_LiveRealtime *realtime,px_memorypool *mp)
{
	PX_memset(realtime,0,sizeof(*realtime));
	realtime->mp=mp;
	realtime->stateRevision=1;
	realtime->dirty=PX_TRUE;
}

px_void PX_LiveRealtimeFree(PX_LiveRealtime *realtime)
{
	px_int i;
	px_memorypool *mp;
	if (!realtime)
	{
		return;
	}
	mp=realtime->mp;
	if (mp)
	{
		for (i=0;i<realtime->axisCount;i++)
		{
			PX_LiveRealtimeFreeAxis(mp,&realtime->axes[i]);
		}
		if (realtime->runtimeBlock)
		{
			MP_Free(mp,realtime->runtimeBlock);
		}
	}
	PX_memset(realtime,0,sizeof(*realtime));
}

px_bool PX_LiveRealtimeClone(PX_LiveRealtime *dest,px_memorypool *mp,const PX_LiveRealtime *source)
{
	px_int i;
	PX_LiveRealtimeInitialize(dest,mp);
	dest->coordFractionBits=source->coordFractionBits;
	dest->rotationFractionBits=source->rotationFractionBits;
	dest->stretchFractionBits=source->stretchFractionBits;
	dest->quantizationSet=source->quantizationSet;
	for (i=0;i<source->axisCount;i++)
	{
		if (!PX_LiveRealtimeCopyAxis(mp,&dest->axes[i],&source->axes[i]))
		{
			PX_LiveRealtimeFree(dest);
			return PX_FALSE;
		}
		dest->staticBytes+=PX_LiveRealtimeAxisStaticBytes(&dest->axes[i]);
		dest->axisCount++;
	}
	dest->dirty=PX_TRUE;
	return PX_TRUE;
}

px_bool PX_LiveRealtimePrepareRuntime(PX_LiveFramework *plive)
{
	PX_LiveRealtime *realtime;
	px_uint32 layerCount,totalVertices=0,offsetBytes,layerBytes,vertexBytes,totalBytes;
	px_uint32 *newOffsets;
	px_void *newBlock;
	px_int i;
	if (!plive)
	{
		return PX_FALSE;
	}
	realtime=&plive->realtime;
	layerCount=(px_uint32)plive->layers.size;
	for (i=0;i<plive->layers.size;i++)
	{
		PX_LiveLayer *layer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
		if ((px_uint32)layer->vertices.size>0xffffffffu-totalVertices)
		{
			return PX_FALSE;
		}
		totalVertices+=(px_uint32)layer->vertices.size;
	}

	if (realtime->runtimeBlock&&realtime->runtimeLayerCount==layerCount&&realtime->runtimeVertexCount==totalVertices&&realtime->vertexOffsets)
	{
		px_bool offsetsMatch=PX_TRUE;
		px_uint32 cursor=0;
		for (i=0;i<plive->layers.size;i++)
		{
			PX_LiveLayer *layer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
			if (realtime->vertexOffsets[i]!=cursor)
			{
				offsetsMatch=PX_FALSE;
				break;
			}
			cursor+=(px_uint32)layer->vertices.size;
		}
		if (offsetsMatch&&realtime->vertexOffsets[layerCount]==cursor)
		{
			return PX_TRUE;
		}
	}
	offsetBytes=PX_LiveRealtimeAlign4((layerCount+1u)*(px_uint32)sizeof(px_uint32));
	layerBytes=PX_LiveRealtimeAlign4(layerCount*(px_uint32)sizeof(PX_LiveRealtimeLayerAccumulator));
	vertexBytes=PX_LiveRealtimeAlign4(totalVertices*(px_uint32)sizeof(PX_LiveRealtimeVertexAccumulator));
	if (offsetBytes>0xffffffffu-layerBytes||offsetBytes+layerBytes>0xffffffffu-vertexBytes)
	{
		return PX_FALSE;
	}
	totalBytes=offsetBytes+layerBytes+vertexBytes;
	if (totalBytes>PX_LIVE_REALTIME_RUNTIME_BUDGET_BYTES)
	{
		return PX_FALSE;
	}
	newBlock=totalBytes?MP_Malloc(realtime->mp,totalBytes):PX_NULL;
	if (totalBytes&&!newBlock)
	{
		return PX_FALSE;
	}
	if (newBlock)
	{
		PX_memset(newBlock,0,totalBytes);
	}
	if (realtime->runtimeBlock)
	{
		MP_Free(realtime->mp,realtime->runtimeBlock);
	}
	realtime->runtimeBlock=newBlock;
	realtime->runtimeBytes=totalBytes;
	realtime->runtimeLayerCount=(px_uint16)layerCount;
	realtime->runtimeVertexCount=totalVertices;
	realtime->vertexOffsets=(px_uint32 *)newBlock;
	realtime->layerAccumulators=newBlock?(PX_LiveRealtimeLayerAccumulator *)((px_byte *)newBlock+offsetBytes):PX_NULL;
	realtime->vertexAccumulators=newBlock?(PX_LiveRealtimeVertexAccumulator *)((px_byte *)newBlock+offsetBytes+layerBytes):PX_NULL;
	newOffsets=realtime->vertexOffsets;
	if (newOffsets)
	{
		px_uint32 offset=0;
		for (i=0;i<plive->layers.size;i++)
		{
			PX_LiveLayer *layer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
			newOffsets[i]=offset;
			offset+=(px_uint32)layer->vertices.size;
		}
		newOffsets[layerCount]=offset;
	}
	PX_LiveRealtimeInvalidateContributions(plive);
	PX_LiveRealtimeMarkDirty(realtime);
	return PX_TRUE;
}

static px_float PX_LiveRealtimeUnwrapNear(px_float reference,px_float value)
{
	while (value-reference>180.0f)
	{
		value-=360.0f;
	}
	while (value-reference<-180.0f)
	{
		value+=360.0f;
	}
	return value;
}

static px_bool PX_LiveRealtimeQuantize(px_float value,px_uchar fractionBits,px_int16 *output)
{
	px_float scaled=value*(px_float)(1u<<fractionBits);
	px_int32 rounded;
	if (scaled>32767.0f||scaled<-32768.0f)
	{
		return PX_FALSE;
	}
	rounded=(px_int32)(scaled>=0?scaled+0.5f:scaled-0.5f);
	if (rounded>32767||rounded<-32768)
	{
		return PX_FALSE;
	}
	*output=(px_int16)rounded;
	return PX_TRUE;
}

static px_bool PX_LiveRealtimeWriteQuantized(px_byte **cursor,px_float value,px_uchar fractionBits)
{
	px_int16 quantized;
	if (!PX_LiveRealtimeQuantize(value,fractionBits,&quantized))
	{
		return PX_FALSE;
	}
	PX_memcpy(*cursor,&quantized,sizeof(quantized));
	*cursor+=sizeof(quantized);
	return PX_TRUE;
}

static px_bool PX_LiveRealtimeWriteInt16(px_byte **cursor,px_int value)
{
	px_int16 stored;
	if (value<-32768||value>32767) return PX_FALSE;
	stored=(px_int16)value;
	PX_memcpy(*cursor,&stored,sizeof(stored));
	*cursor+=sizeof(stored);
	return PX_TRUE;
}

static px_float PX_LiveRealtimeInterpolate(px_float a,px_float b,px_float t)
{
	return a+(b-a)*t;
}

static px_bool PX_LiveRealtimeBuildAxis(PX_LiveFramework *plive,const PX_LiveRealtimeAxisBakeDesc *desc,PX_LiveRealtimeAxis *axis)
{
	px_uint32 stride=0,totalVertexIndices=0;
	px_uint32 vertexCursor=0;
	px_int i,sample;
	px_int defaultKey;
	px_int idLength=0;
	if (!desc||!desc->id||!desc->id[0]||!desc->bindings||!desc->bindingCount)
	{
		return PX_FALSE;
	}
	while (idLength<PX_LIVE_REALTIME_AXIS_ID_MAX_LEN&&desc->id[idLength])
	{
		idLength++;
	}
	if (idLength>=PX_LIVE_REALTIME_AXIS_ID_MAX_LEN)
	{
		/* Hashing a longer string while retaining a truncated id is ambiguous. */
		return PX_FALSE;
	}
	if (desc->middleKeyIndex<1||desc->middleKeyIndex>28)
	{
		return PX_FALSE;
	}
	if (desc->defaultSampleIndex!=0&&desc->defaultSampleIndex!=desc->middleKeyIndex&&desc->defaultSampleIndex!=29)
	{
		return PX_FALSE;
	}
	if (desc->coordFractionBits>PX_LIVE_REALTIME_MAX_FRACTION_BITS||
		desc->rotationFractionBits>PX_LIVE_REALTIME_MAX_FRACTION_BITS||
		desc->stretchFractionBits>PX_LIVE_REALTIME_MAX_FRACTION_BITS)
	{
		return PX_FALSE;
	}
	for (i=0;i<3;i++)
	{
		if (!desc->keyPoses[i].bindings)
		{
			return PX_FALSE;
		}
	}
	PX_memset(axis,0,sizeof(*axis));
	for (i=0;i<desc->bindingCount;i++)
	{
		const PX_LiveRealtimeBindingDesc *binding=&desc->bindings[i];
		PX_LiveLayer *layer;
		px_uint32 bindingBytes=0;
		px_int v;
		if (binding->layerIndex>=(px_uint16)plive->layers.size||!binding->propertyMask||
			(binding->propertyMask&~PX_LIVE_REALTIME_PROPERTY_ALL))
		{
			return PX_FALSE;
		}
		layer=PX_VECTORAT(PX_LiveLayer,&plive->layers,binding->layerIndex);
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_TRANSLATION) bindingBytes+=2u*sizeof(px_int16);
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_ROTATION) bindingBytes+=sizeof(px_int16);
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_STRETCH) bindingBytes+=sizeof(px_int16);
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_TRANSLATION) bindingBytes+=2u*sizeof(px_int16);
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_ROTATION) bindingBytes+=sizeof(px_int16);
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_SCALE) bindingBytes+=sizeof(px_int16);
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_TEXTURE)
		{
			for (v=0;v<3;v++)
			{
				px_int texture=desc->keyPoses[v].bindings[i].mapTexture;
				if (texture<-1||texture>=plive->livetextures.size||texture>32767) return PX_FALSE;
			}
			bindingBytes+=sizeof(px_int16);
		}
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_IMPULSE) bindingBytes+=2u*sizeof(px_int16);
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_VERTICES)
		{
			if (binding->vertexCount&&!binding->vertexIndices)
			{
				return PX_FALSE;
			}
			for (v=0;v<binding->vertexCount;v++)
			{
				if (binding->vertexIndices[v]>=(px_uint16)layer->vertices.size)
				{
					return PX_FALSE;
				}
			}
			bindingBytes+=(px_uint32)binding->vertexCount*2u*sizeof(px_int16);
			if (totalVertexIndices>0xffffffffu-binding->vertexCount)
			{
				return PX_FALSE;
			}
			totalVertexIndices+=binding->vertexCount;
			for (v=0;v<3;v++)
			{
				if (binding->vertexCount&&!desc->keyPoses[v].bindings[i].vertexDeltas)
				{
					return PX_FALSE;
				}
			}
		}
		if (stride>0xffffffffu-bindingBytes)
		{
			return PX_FALSE;
		}
		stride+=bindingBytes;
	}
	stride=PX_LiveRealtimeAlign4(stride);
	if (!stride||stride>0xffffffffu/PX_LIVE_REALTIME_SAMPLE_COUNT)
	{
		return PX_FALSE;
	}
	axis->bindings=(PX_LiveRealtimeBinding *)MP_Malloc(plive->mp,(px_uint)desc->bindingCount*sizeof(PX_LiveRealtimeBinding));
	if (!axis->bindings)
	{
		goto _ERROR;
	}
	if (totalVertexIndices)
	{
		axis->vertexIndices=(px_uint16 *)MP_Malloc(plive->mp,totalVertexIndices*(px_uint)sizeof(px_uint16));
		if (!axis->vertexIndices)
		{
			goto _ERROR;
		}
	}
	axis->sampleBytes=stride*PX_LIVE_REALTIME_SAMPLE_COUNT;
	axis->samples=(px_int16 *)MP_Malloc(plive->mp,axis->sampleBytes);
	if (!axis->samples)
	{
		goto _ERROR;
	}
	PX_memset(axis->samples,0,axis->sampleBytes);
	axis->bindingCount=desc->bindingCount;
	axis->vertexIndexCount=totalVertexIndices;
	axis->sampleStride=stride;
	axis->middleKeyIndex=desc->middleKeyIndex;
	axis->defaultSampleIndex=desc->defaultSampleIndex;
	axis->sampleIndex=desc->defaultSampleIndex;
	axis->continuousActive=PX_FALSE;
	axis->continuousPosition=0;
	axis->textureHysteresis=0;
	axis->coordFractionBits=desc->coordFractionBits;
	axis->rotationFractionBits=desc->rotationFractionBits;
	axis->stretchFractionBits=desc->stretchFractionBits;
	axis->weightQ15=0;
	axis->valid=PX_TRUE;
	axis->idHash=desc->idHash?desc->idHash:PX_LiveRealtimeHashId(desc->id);
	for (i=0;i<PX_LIVE_REALTIME_AXIS_ID_MAX_LEN-1&&desc->id[i];i++)
	{
		axis->id[i]=desc->id[i];
	}
	axis->id[i]=0;

	stride=0;
	for (i=0;i<desc->bindingCount;i++)
	{
		const PX_LiveRealtimeBindingDesc *source=&desc->bindings[i];
		PX_LiveRealtimeBinding *target=&axis->bindings[i];
		px_uint32 bindingBytes=0;
		PX_memset(target,0,sizeof(*target));
		target->layerIndex=source->layerIndex;
		target->propertyMask=source->propertyMask;
		target->vertexCount=(source->propertyMask&PX_LIVE_REALTIME_PROPERTY_VERTICES)?source->vertexCount:0;
		target->vertexIndexOffset=vertexCursor;
		target->sampleOffset=stride;
		if (target->vertexCount)
		{
			PX_memcpy(axis->vertexIndices+vertexCursor,source->vertexIndices,(px_uint)target->vertexCount*sizeof(px_uint16));
			vertexCursor+=target->vertexCount;
		}
		if (target->propertyMask&PX_LIVE_REALTIME_PROPERTY_TRANSLATION) bindingBytes+=2u*sizeof(px_int16);
		if (target->propertyMask&PX_LIVE_REALTIME_PROPERTY_ROTATION) bindingBytes+=sizeof(px_int16);
		if (target->propertyMask&PX_LIVE_REALTIME_PROPERTY_STRETCH) bindingBytes+=sizeof(px_int16);
		if (target->propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_TRANSLATION) bindingBytes+=2u*sizeof(px_int16);
		if (target->propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_ROTATION) bindingBytes+=sizeof(px_int16);
		if (target->propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_SCALE) bindingBytes+=sizeof(px_int16);
		if (target->propertyMask&PX_LIVE_REALTIME_PROPERTY_TEXTURE) bindingBytes+=sizeof(px_int16);
		if (target->propertyMask&PX_LIVE_REALTIME_PROPERTY_IMPULSE) bindingBytes+=2u*sizeof(px_int16);
		if (target->propertyMask&PX_LIVE_REALTIME_PROPERTY_VERTICES) bindingBytes+=(px_uint32)target->vertexCount*2u*sizeof(px_int16);
		stride+=bindingBytes;
	}

	defaultKey=desc->defaultSampleIndex==0?0:(desc->defaultSampleIndex==desc->middleKeyIndex?1:2);
	for (sample=0;sample<PX_LIVE_REALTIME_SAMPLE_COUNT;sample++)
	{
		px_int keyA,keyB;
		px_float t;
		if (sample<=desc->middleKeyIndex)
		{
			keyA=0;
			keyB=1;
			t=(px_float)sample/(px_float)desc->middleKeyIndex;
		}
		else
		{
			keyA=1;
			keyB=2;
			t=(px_float)(sample-desc->middleKeyIndex)/(px_float)(29-desc->middleKeyIndex);
		}
		for (i=0;i<desc->bindingCount;i++)
		{
			const PX_LiveRealtimeBinding *binding=&axis->bindings[i];
			const PX_LiveRealtimeBindingPose *a=&desc->keyPoses[keyA].bindings[i];
			const PX_LiveRealtimeBindingPose *b=&desc->keyPoses[keyB].bindings[i];
			const PX_LiveRealtimeBindingPose *neutral=&desc->keyPoses[defaultKey].bindings[i];
			px_byte *cursor=(px_byte *)axis->samples+(px_uint32)sample*axis->sampleStride+binding->sampleOffset;
			if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_TRANSLATION)
			{
				if (!PX_LiveRealtimeWriteQuantized(&cursor,PX_LiveRealtimeInterpolate(a->translation.x,b->translation.x,t)-neutral->translation.x,desc->coordFractionBits)||
					!PX_LiveRealtimeWriteQuantized(&cursor,PX_LiveRealtimeInterpolate(a->translation.y,b->translation.y,t)-neutral->translation.y,desc->coordFractionBits))
				{
					goto _ERROR;
				}
			}
			if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_ROTATION)
			{
				px_float r0=desc->keyPoses[0].bindings[i].rotation;
				px_float r1=PX_LiveRealtimeUnwrapNear(r0,desc->keyPoses[1].bindings[i].rotation);
				px_float r2=PX_LiveRealtimeUnwrapNear(r1,desc->keyPoses[2].bindings[i].rotation);
				px_float ra=keyA==0?r0:r1;
				px_float rb=keyB==1?r1:r2;
				px_float rn=defaultKey==0?r0:(defaultKey==1?r1:r2);
				if (!PX_LiveRealtimeWriteQuantized(&cursor,PX_LiveRealtimeInterpolate(ra,rb,t)-rn,desc->rotationFractionBits))
				{
					goto _ERROR;
				}
			}
			if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_STRETCH)
			{
				if (!PX_LiveRealtimeWriteQuantized(&cursor,PX_LiveRealtimeInterpolate(a->stretch,b->stretch,t)-neutral->stretch,desc->stretchFractionBits))
				{
					goto _ERROR;
				}
			}
			if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_TRANSLATION)
			{
				if (!PX_LiveRealtimeWriteQuantized(&cursor,PX_LiveRealtimeInterpolate(a->localTranslation.x,b->localTranslation.x,t)-neutral->localTranslation.x,desc->coordFractionBits)||
					!PX_LiveRealtimeWriteQuantized(&cursor,PX_LiveRealtimeInterpolate(a->localTranslation.y,b->localTranslation.y,t)-neutral->localTranslation.y,desc->coordFractionBits))
				{
					goto _ERROR;
				}
			}
			if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_ROTATION)
			{
				px_float r0=desc->keyPoses[0].bindings[i].localRotation;
				px_float r1=PX_LiveRealtimeUnwrapNear(r0,desc->keyPoses[1].bindings[i].localRotation);
				px_float r2=PX_LiveRealtimeUnwrapNear(r1,desc->keyPoses[2].bindings[i].localRotation);
				px_float ra=keyA==0?r0:r1;
				px_float rb=keyB==1?r1:r2;
				px_float rn=defaultKey==0?r0:(defaultKey==1?r1:r2);
				if (!PX_LiveRealtimeWriteQuantized(&cursor,PX_LiveRealtimeInterpolate(ra,rb,t)-rn,desc->rotationFractionBits))
				{
					goto _ERROR;
				}
			}
			if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_SCALE)
			{
				if (!PX_LiveRealtimeWriteQuantized(&cursor,PX_LiveRealtimeInterpolate(a->localScale,b->localScale,t)-neutral->localScale,desc->stretchFractionBits))
				{
					goto _ERROR;
				}
			}
			if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_TEXTURE)
			{
				/* Texture ids are categorical.  Bake a deterministic nearest-key step;
				   runtime never interpolates adjacent parameter samples. */
				if (!PX_LiveRealtimeWriteInt16(&cursor,t<0.5f?a->mapTexture:b->mapTexture)) goto _ERROR;
			}
			if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_IMPULSE)
			{
				if (!PX_LiveRealtimeWriteQuantized(&cursor,PX_LiveRealtimeInterpolate(a->impulse.x,b->impulse.x,t)-neutral->impulse.x,desc->coordFractionBits)||
					!PX_LiveRealtimeWriteQuantized(&cursor,PX_LiveRealtimeInterpolate(a->impulse.y,b->impulse.y,t)-neutral->impulse.y,desc->coordFractionBits)) goto _ERROR;
			}
			if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_VERTICES)
			{
				px_int v;
				for (v=0;v<binding->vertexCount;v++)
				{
					if (!PX_LiveRealtimeWriteQuantized(&cursor,PX_LiveRealtimeInterpolate(a->vertexDeltas[v].x,b->vertexDeltas[v].x,t)-neutral->vertexDeltas[v].x,desc->coordFractionBits)||
						!PX_LiveRealtimeWriteQuantized(&cursor,PX_LiveRealtimeInterpolate(a->vertexDeltas[v].y,b->vertexDeltas[v].y,t)-neutral->vertexDeltas[v].y,desc->coordFractionBits))
					{
						goto _ERROR;
					}
				}
			}
		}
	}
	return PX_TRUE;
_ERROR:
	PX_LiveRealtimeFreeAxis(plive->mp,axis);
	return PX_FALSE;
}

px_bool PX_LiveRealtimeBakeAxis(PX_LiveFramework *plive,px_int replaceHandle,const PX_LiveRealtimeAxisBakeDesc *desc,px_int *outHandle)
{
	PX_LiveRealtimeAxis newAxis;
	PX_LiveRealtime *realtime;
	px_uint32 prospectiveStaticBytes;
	px_int existing;
	if (!plive||!desc)
	{
		return PX_FALSE;
	}
	realtime=&plive->realtime;
	if (replaceHandle!=PX_LIVE_REALTIME_INVALID_HANDLE&&(replaceHandle<0||replaceHandle>=realtime->axisCount))
	{
		return PX_FALSE;
	}
	if (replaceHandle==PX_LIVE_REALTIME_INVALID_HANDLE&&realtime->axisCount>=PX_LIVE_REALTIME_MAX_AXES)
	{
		return PX_FALSE;
	}
	if (realtime->quantizationSet)
	{
		px_bool replacingOnlyAxis=(replaceHandle>=0&&realtime->axisCount==1);
		if (!replacingOnlyAxis&&(desc->coordFractionBits!=realtime->coordFractionBits||
			desc->rotationFractionBits!=realtime->rotationFractionBits||
			desc->stretchFractionBits!=realtime->stretchFractionBits))
		{
			return PX_FALSE;
		}
	}
	existing=PX_LiveRealtimeFindAxisByHash(plive,desc->idHash?desc->idHash:PX_LiveRealtimeHashId(desc->id));
	if (existing>=0&&existing!=replaceHandle)
	{
		return PX_FALSE;
	}
	if (!PX_LiveRealtimeBuildAxis(plive,desc,&newAxis))
	{
		return PX_FALSE;
	}
	prospectiveStaticBytes=realtime->staticBytes+PX_LiveRealtimeAxisStaticBytes(&newAxis);
	if (replaceHandle>=0)
	{
		prospectiveStaticBytes-=PX_LiveRealtimeAxisStaticBytes(&realtime->axes[replaceHandle]);
	}
	if (prospectiveStaticBytes>PX_LIVE_REALTIME_STATIC_BUDGET_BYTES)
	{
		PX_LiveRealtimeFreeAxis(plive->mp,&newAxis);
		return PX_FALSE;
	}
	if (!PX_LiveRealtimePrepareRuntime(plive))
	{
		PX_LiveRealtimeFreeAxis(plive->mp,&newAxis);
		return PX_FALSE;
	}
	if (replaceHandle>=0)
	{
		realtime->staticBytes-=PX_LiveRealtimeAxisStaticBytes(&realtime->axes[replaceHandle]);
		PX_LiveRealtimeFreeAxis(plive->mp,&realtime->axes[replaceHandle]);
		realtime->axes[replaceHandle]=newAxis;
		if (outHandle) *outHandle=replaceHandle;
	}
	else
	{
		realtime->axes[realtime->axisCount]=newAxis;
		if (outHandle) *outHandle=realtime->axisCount;
		realtime->axisCount++;
	}
	realtime->staticBytes+=PX_LiveRealtimeAxisStaticBytes(&newAxis);
	realtime->coordFractionBits=desc->coordFractionBits;
	realtime->rotationFractionBits=desc->rotationFractionBits;
	realtime->stretchFractionBits=desc->stretchFractionBits;
	realtime->quantizationSet=PX_TRUE;
	PX_LiveRealtimeInvalidateContributions(plive);
	PX_LiveRealtimeMarkDirty(realtime);
	return PX_TRUE;
}

static px_bool PX_LiveRealtimeValidateBakedAxis(PX_LiveFramework *plive,const PX_LiveRealtimeAxis *source)
{
	px_uint32 expectedSampleOffset=0,expectedVertexOffset=0;
	px_int i,idLength=0;
	if (!source||!source->idHash||!source->bindings||!source->samples||!source->bindingCount)
	{
		return PX_FALSE;
	}
	while (idLength<PX_LIVE_REALTIME_AXIS_ID_MAX_LEN&&source->id[idLength])
	{
		idLength++;
	}
	if (idLength>=PX_LIVE_REALTIME_AXIS_ID_MAX_LEN)
	{
		return PX_FALSE;
	}
	if (idLength&&PX_LiveRealtimeHashId(source->id)!=source->idHash)
	{
		return PX_FALSE;
	}
	if (source->middleKeyIndex<1||source->middleKeyIndex>28||
		(source->defaultSampleIndex!=0&&source->defaultSampleIndex!=source->middleKeyIndex&&source->defaultSampleIndex!=29)||
		source->sampleIndex>=PX_LIVE_REALTIME_SAMPLE_COUNT||
		source->weightQ15>PX_LIVE_REALTIME_WEIGHT_ONE_Q15||
		source->coordFractionBits>PX_LIVE_REALTIME_MAX_FRACTION_BITS||
		source->rotationFractionBits>PX_LIVE_REALTIME_MAX_FRACTION_BITS||
		source->stretchFractionBits>PX_LIVE_REALTIME_MAX_FRACTION_BITS)
	{
		return PX_FALSE;
	}
	if (!source->sampleStride||source->sampleStride>0xffffffffu/PX_LIVE_REALTIME_SAMPLE_COUNT||
		source->sampleBytes!=source->sampleStride*PX_LIVE_REALTIME_SAMPLE_COUNT)
	{
		return PX_FALSE;
	}
	for (i=0;i<source->bindingCount;i++)
	{
		const PX_LiveRealtimeBinding *binding=&source->bindings[i];
		PX_LiveLayer *layer;
		px_uint32 bindingBytes=0;
		px_int v;
		if (binding->layerIndex>=(px_uint16)plive->layers.size||!binding->propertyMask||
			(binding->propertyMask&~PX_LIVE_REALTIME_PROPERTY_ALL)||binding->sampleOffset!=expectedSampleOffset||
			binding->vertexIndexOffset!=expectedVertexOffset)
		{
			return PX_FALSE;
		}
		layer=PX_VECTORAT(PX_LiveLayer,&plive->layers,binding->layerIndex);
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_TRANSLATION) bindingBytes+=2u*sizeof(px_int16);
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_ROTATION) bindingBytes+=sizeof(px_int16);
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_STRETCH) bindingBytes+=sizeof(px_int16);
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_TRANSLATION) bindingBytes+=2u*sizeof(px_int16);
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_ROTATION) bindingBytes+=sizeof(px_int16);
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_SCALE) bindingBytes+=sizeof(px_int16);
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_TEXTURE) bindingBytes+=sizeof(px_int16);
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_IMPULSE) bindingBytes+=2u*sizeof(px_int16);
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_VERTICES)
		{
			if (binding->vertexCount&&!source->vertexIndices)
			{
				return PX_FALSE;
			}
			if (expectedVertexOffset>source->vertexIndexCount||
				binding->vertexCount>source->vertexIndexCount-expectedVertexOffset)
			{
				return PX_FALSE;
			}
			for (v=0;v<binding->vertexCount;v++)
			{
				if (source->vertexIndices[expectedVertexOffset+v]>=(px_uint16)layer->vertices.size)
				{
					return PX_FALSE;
				}
			}
			bindingBytes+=(px_uint32)binding->vertexCount*2u*sizeof(px_int16);
			expectedVertexOffset+=binding->vertexCount;
		}
		else if (binding->vertexCount)
		{
			return PX_FALSE;
		}
		if (expectedSampleOffset>source->sampleStride||bindingBytes>source->sampleStride-expectedSampleOffset)
		{
			return PX_FALSE;
		}
		expectedSampleOffset+=bindingBytes;
	}
	if (expectedVertexOffset!=source->vertexIndexCount||PX_LiveRealtimeAlign4(expectedSampleOffset)!=source->sampleStride)
	{
		return PX_FALSE;
	}
	return PX_TRUE;
}

px_bool PX_LiveRealtimeInstallBakedAxis(PX_LiveFramework *plive,px_int replaceHandle,const PX_LiveRealtimeAxis *source,px_int *outHandle)
{
	PX_LiveRealtime *realtime;
	PX_LiveRealtimeAxis copy;
	px_int existing;
	px_uint32 prospectiveStaticBytes;
	if (!plive||!PX_LiveRealtimeValidateBakedAxis(plive,source))
	{
		return PX_FALSE;
	}
	realtime=&plive->realtime;
	if (replaceHandle!=PX_LIVE_REALTIME_INVALID_HANDLE&&(replaceHandle<0||replaceHandle>=realtime->axisCount))
	{
		return PX_FALSE;
	}
	if (replaceHandle==PX_LIVE_REALTIME_INVALID_HANDLE&&realtime->axisCount>=PX_LIVE_REALTIME_MAX_AXES)
	{
		return PX_FALSE;
	}
	if (realtime->quantizationSet)
	{
		px_bool replacingOnlyAxis=(replaceHandle>=0&&realtime->axisCount==1);
		if (!replacingOnlyAxis&&(source->coordFractionBits!=realtime->coordFractionBits||
			source->rotationFractionBits!=realtime->rotationFractionBits||
			source->stretchFractionBits!=realtime->stretchFractionBits))
		{
			return PX_FALSE;
		}
	}
	existing=PX_LiveRealtimeFindAxisByHash(plive,source->idHash);
	if (existing>=0&&existing!=replaceHandle)
	{
		return PX_FALSE;
	}
	if (!PX_LiveRealtimeCopyAxis(plive->mp,&copy,source))
	{
		return PX_FALSE;
	}
	prospectiveStaticBytes=realtime->staticBytes+PX_LiveRealtimeAxisStaticBytes(&copy);
	if (replaceHandle>=0)
	{
		prospectiveStaticBytes-=PX_LiveRealtimeAxisStaticBytes(&realtime->axes[replaceHandle]);
	}
	if (prospectiveStaticBytes>PX_LIVE_REALTIME_STATIC_BUDGET_BYTES)
	{
		PX_LiveRealtimeFreeAxis(plive->mp,&copy);
		return PX_FALSE;
	}
	copy.valid=PX_TRUE;
	copy.continuousActive=PX_FALSE;
	copy.textureHysteresis=0;
	if (!PX_LiveRealtimePrepareRuntime(plive))
	{
		PX_LiveRealtimeFreeAxis(plive->mp,&copy);
		return PX_FALSE;
	}
	if (replaceHandle>=0)
	{
		realtime->staticBytes-=PX_LiveRealtimeAxisStaticBytes(&realtime->axes[replaceHandle]);
		PX_LiveRealtimeFreeAxis(plive->mp,&realtime->axes[replaceHandle]);
		realtime->axes[replaceHandle]=copy;
		if (outHandle) *outHandle=replaceHandle;
	}
	else
	{
		realtime->axes[realtime->axisCount]=copy;
		if (outHandle) *outHandle=realtime->axisCount;
		realtime->axisCount++;
	}
	realtime->staticBytes+=PX_LiveRealtimeAxisStaticBytes(&copy);
	realtime->coordFractionBits=copy.coordFractionBits;
	realtime->rotationFractionBits=copy.rotationFractionBits;
	realtime->stretchFractionBits=copy.stretchFractionBits;
	realtime->quantizationSet=PX_TRUE;
	PX_LiveRealtimeInvalidateContributions(plive);
	PX_LiveRealtimeMarkDirty(realtime);
	return PX_TRUE;
}

px_bool PX_LiveRealtimeDeleteAxis(PX_LiveFramework *plive,px_int axisHandle)
{
	PX_LiveRealtime *realtime;
	px_int i;
	if (!plive)
	{
		return PX_FALSE;
	}
	realtime=&plive->realtime;
	if (axisHandle<0||axisHandle>=realtime->axisCount)
	{
		return PX_FALSE;
	}
	realtime->staticBytes-=PX_LiveRealtimeAxisStaticBytes(&realtime->axes[axisHandle]);
	PX_LiveRealtimeFreeAxis(plive->mp,&realtime->axes[axisHandle]);
	for (i=axisHandle;i<realtime->axisCount-1;i++)
	{
		realtime->axes[i]=realtime->axes[i+1];
	}
	realtime->axisCount--;
	PX_memset(&realtime->axes[realtime->axisCount],0,sizeof(realtime->axes[0]));
	PX_LiveRealtimeInvalidateContributions(plive);
	if (!realtime->axisCount)
	{
		realtime->quantizationSet=PX_FALSE;
		realtime->coordFractionBits=0;
		realtime->rotationFractionBits=0;
		realtime->stretchFractionBits=0;
	}
	PX_LiveRealtimeMarkDirty(realtime);
	return PX_TRUE;
}

px_void PX_LiveRealtimeClearAxes(PX_LiveFramework *plive)
{
	PX_LiveRealtime *realtime;
	px_int i;
	if (!plive)
	{
		return;
	}
	realtime=&plive->realtime;
	for (i=0;i<realtime->axisCount;i++)
	{
		PX_LiveRealtimeFreeAxis(plive->mp,&realtime->axes[i]);
	}
	realtime->axisCount=0;
	realtime->staticBytes=0;
	realtime->quantizationSet=PX_FALSE;
	realtime->coordFractionBits=0;
	realtime->rotationFractionBits=0;
	realtime->stretchFractionBits=0;
	PX_LiveRealtimeInvalidateContributions(plive);
	PX_LiveRealtimeMarkDirty(realtime);
}

px_int PX_LiveRealtimeGetAxisCount(const PX_LiveFramework *plive)
{
	return plive?(px_int)plive->realtime.axisCount:0;
}

PX_LiveRealtimeAxis *PX_LiveRealtimeGetAxis(PX_LiveFramework *plive,px_int axisHandle)
{
	if (!plive||axisHandle<0||axisHandle>=plive->realtime.axisCount)
	{
		return PX_NULL;
	}
	return &plive->realtime.axes[axisHandle];
}

const PX_LiveRealtimeAxis *PX_LiveRealtimeGetAxisConst(const PX_LiveFramework *plive,px_int axisHandle)
{
	if (!plive||axisHandle<0||axisHandle>=plive->realtime.axisCount)
	{
		return PX_NULL;
	}
	return &plive->realtime.axes[axisHandle];
}

px_int PX_LiveRealtimeFindAxisByHash(const PX_LiveFramework *plive,px_uint32 idHash)
{
	px_int i;
	if (!plive||!idHash)
	{
		return PX_LIVE_REALTIME_INVALID_HANDLE;
	}
	for (i=0;i<plive->realtime.axisCount;i++)
	{
		if (plive->realtime.axes[i].idHash==idHash)
		{
			return i;
		}
	}
	return PX_LIVE_REALTIME_INVALID_HANDLE;
}

px_int PX_LiveRealtimeFindAxisById(const PX_LiveFramework *plive,const px_char id[])
{
	px_int handle=PX_LiveRealtimeFindAxisByHash(plive,PX_LiveRealtimeHashId(id));
	px_int i;
	if (handle<0||!id)
	{
		return PX_LIVE_REALTIME_INVALID_HANDLE;
	}
	for (i=0;i<PX_LIVE_REALTIME_AXIS_ID_MAX_LEN;i++)
	{
		if (plive->realtime.axes[handle].id[i]!=id[i])
		{
			return PX_LIVE_REALTIME_INVALID_HANDLE;
		}
		if (!id[i])
		{
			return handle;
		}
	}
	return PX_LIVE_REALTIME_INVALID_HANDLE;
}

px_bool PX_LiveRealtimeEnter(PX_LiveFramework *plive)
{
	if (!plive||!PX_LiveRealtimePrepareRuntime(plive))
	{
		return PX_FALSE;
	}
	PX_LiveFrameworkReset(plive);
	plive->animationMode=PX_LIVE_MODE_REALTIME30;
	PX_LiveRealtimeMarkDirty(&plive->realtime);
	return PX_LiveRealtimeUpdate(plive);
}

px_void PX_LiveRealtimeLeave(PX_LiveFramework *plive)
{
	if (!plive)
	{
		return;
	}
	PX_LiveFrameworkReset(plive);
}

px_bool PX_LiveRealtimeSetAxisSample(PX_LiveFramework *plive,px_int axisHandle,px_uchar sampleIndex,px_uint16 weightQ15)
{
	PX_LiveRealtimeAxis *axis=PX_LiveRealtimeGetAxis(plive,axisHandle);
	if (!axis||!axis->valid||sampleIndex>=PX_LIVE_REALTIME_SAMPLE_COUNT)
	{
		return PX_FALSE;
	}
	if (weightQ15>PX_LIVE_REALTIME_WEIGHT_ONE_Q15)
	{
		weightQ15=PX_LIVE_REALTIME_WEIGHT_ONE_Q15;
	}
	if (axis->sampleIndex!=sampleIndex||axis->weightQ15!=weightQ15||axis->continuousActive)
	{
		axis->sampleIndex=sampleIndex;
		axis->weightQ15=weightQ15;
		axis->continuousActive=PX_FALSE;
		axis->textureHysteresis=0;
		PX_LiveRealtimeMarkDirty(&plive->realtime);
	}
	return PX_TRUE;
}

px_bool PX_LiveRealtimeSetAxisPosition(PX_LiveFramework *plive,px_int axisHandle,px_float position,px_uint16 weightQ15)
{
	PX_LiveRealtimeAxis *axis=PX_LiveRealtimeGetAxis(plive,axisHandle);
	px_uchar sampleIndex;
	if (!axis||!axis->valid||position!=position)
	{
		return PX_FALSE;
	}
	if (position<0)
	{
		position=0;
	}
	if (position>(px_float)(PX_LIVE_REALTIME_SAMPLE_COUNT-1))
	{
		position=(px_float)(PX_LIVE_REALTIME_SAMPLE_COUNT-1);
	}
	if (weightQ15>PX_LIVE_REALTIME_WEIGHT_ONE_Q15)
	{
		weightQ15=PX_LIVE_REALTIME_WEIGHT_ONE_Q15;
	}
	sampleIndex=(px_uchar)position;
	if (sampleIndex>=PX_LIVE_REALTIME_SAMPLE_COUNT)
	{
		sampleIndex=PX_LIVE_REALTIME_SAMPLE_COUNT-1;
	}
	if (!axis->continuousActive||axis->continuousPosition!=position||axis->sampleIndex!=sampleIndex||axis->weightQ15!=weightQ15)
	{
		axis->continuousActive=PX_TRUE;
		axis->continuousPosition=position;
		axis->sampleIndex=sampleIndex;
		axis->weightQ15=weightQ15;
		PX_LiveRealtimeMarkDirty(&plive->realtime);
	}
	return PX_TRUE;
}

px_float PX_LiveRealtimeTransitionEvaluate(const PX_LiveRealtimeTransition *transition,px_dword nowMs)
{
	px_float t;
	px_dword elapsed;
	if (!transition)
	{
		return 0;
	}
	if (transition->durationMs==0||nowMs<=transition->startTimeMs)
	{
		return nowMs<=transition->startTimeMs?transition->startValue:transition->targetValue;
	}
	elapsed=nowMs-transition->startTimeMs;
	if (elapsed>=transition->durationMs)
	{
		return transition->targetValue;
	}
	t=(px_float)elapsed/(px_float)transition->durationMs;
	if (transition->smoothstep)
	{
		t=t*t*(3.0f-2.0f*t);
	}
	return transition->startValue+(transition->targetValue-transition->startValue)*t;
}

px_bool PX_LiveRealtimeSetAxisNormalizedQ15(PX_LiveFramework *plive,px_int axisHandle,px_uint16 normalizedQ15,px_uint16 weightQ15)
{
	px_uint32 index;
	if (normalizedQ15>PX_LIVE_REALTIME_WEIGHT_ONE_Q15)
	{
		normalizedQ15=PX_LIVE_REALTIME_WEIGHT_ONE_Q15;
	}
	index=((px_uint32)normalizedQ15*29u+(PX_LIVE_REALTIME_WEIGHT_ONE_Q15/2u))/PX_LIVE_REALTIME_WEIGHT_ONE_Q15;
	return PX_LiveRealtimeSetAxisSample(plive,axisHandle,(px_uchar)index,weightQ15);
}

px_bool PX_LiveRealtimeResetAxis(PX_LiveFramework *plive,px_int axisHandle)
{
	PX_LiveRealtimeAxis *axis=PX_LiveRealtimeGetAxis(plive,axisHandle);
	if (!axis)
	{
		return PX_FALSE;
	}
	return PX_LiveRealtimeSetAxisSample(plive,axisHandle,axis->defaultSampleIndex,0);
}

px_void PX_LiveRealtimeResetAll(PX_LiveFramework *plive)
{
	px_int i;
	px_bool changed=PX_FALSE;
	if (!plive)
	{
		return;
	}
#if CONFIG_L2D_PROFILE_VISUAL
	PX_LiveFrameworkVisualDiagInvalidate();
#endif
	for (i=0;i<plive->layers.size;i++)
	{
		PX_LiveLayer *layer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
		layer->rel_rotationTrigValid=PX_FALSE;
		layer->rel_localRotationTrigValid=PX_FALSE;
	}
	for (i=0;i<plive->realtime.axisCount;i++)
	{
		PX_LiveRealtimeAxis *axis=&plive->realtime.axes[i];
		if (axis->sampleIndex!=axis->defaultSampleIndex||axis->weightQ15!=0)
		{
			axis->sampleIndex=axis->defaultSampleIndex;
			axis->weightQ15=0;
			changed=PX_TRUE;
		}
	}
	if (changed)
	{
		PX_LiveRealtimeMarkDirty(&plive->realtime);
	}
}

static px_int32 PX_LiveRealtimeReadInt16(const px_byte **cursor)
{
	px_int16 value;
	PX_memcpy(&value,*cursor,sizeof(value));
	*cursor+=sizeof(value);
	return value;
}

static px_int32 PX_LiveRealtimeWeightValue(px_int32 value,px_uint16 weightQ15)
{
	px_int32 product=value*(px_int32)weightQ15;
	if (product>=0)
	{
		product+=16384;
	}
	else
	{
		product-=16384;
	}
	return product/32768;
}

static px_int32 PX_LiveRealtimeTakeSample(const px_byte **cursorA,const px_byte **cursorB,px_float t,px_uint16 weightQ15)
{
	px_int32 a=PX_LiveRealtimeReadInt16(cursorA);
	px_int32 rounded;
	px_float mixed;
	if (!cursorB||t<=0)
	{
		return PX_LiveRealtimeWeightValue(a,weightQ15);
	}
	mixed=(1.0f-t)*(px_float)a+t*(px_float)PX_LiveRealtimeReadInt16(cursorB);
	rounded=(px_int32)(mixed>=0?mixed+0.5f:mixed-0.5f);
	return PX_LiveRealtimeWeightValue(rounded,weightQ15);
}

static px_void PX_LiveRealtimeResolveSample(px_bool continuousActive,px_float continuousPosition,px_uchar sampleIndex,px_uchar *outSample,px_float *outT)
{
	px_float q;
	px_uchar sampleA;
	px_float sampleT=0;
	if (continuousActive)
	{
		q=continuousPosition;
		if (q<0) q=0;
		if (q>(px_float)(PX_LIVE_REALTIME_SAMPLE_COUNT-1)) q=(px_float)(PX_LIVE_REALTIME_SAMPLE_COUNT-1);
		sampleA=(px_uchar)q;
		sampleT=q-(px_float)sampleA;
		if (sampleA>=PX_LIVE_REALTIME_SAMPLE_COUNT-1)
		{
			sampleA=PX_LIVE_REALTIME_SAMPLE_COUNT-1;
			sampleT=0;
		}
	}
	else
	{
		sampleA=sampleIndex;
		if (sampleA>=PX_LIVE_REALTIME_SAMPLE_COUNT)
		{
			sampleA=PX_LIVE_REALTIME_SAMPLE_COUNT-1;
		}
	}
	*outSample=sampleA;
	*outT=sampleT;
}

static px_bool PX_LiveRealtimeTextureChooseHigh(const PX_LiveRealtimeAxis *axis,px_float sampleT)
{
	if (axis->textureHysteresis==1&&sampleT<0.58f) return PX_FALSE;
	if (axis->textureHysteresis==2&&sampleT>0.42f) return PX_TRUE;
	return sampleT>=0.5f;
}

static px_void PX_LiveRealtimeCommitHysteresis(PX_LiveRealtimeAxis *axis,px_float sampleT,px_bool chooseHigh)
{
	if (sampleT>0) axis->textureHysteresis=(px_uchar)(chooseHigh?2:1);
	else axis->textureHysteresis=0;
}

static px_bool PX_LiveRealtimeAppliedMatches(const PX_LiveRealtimeAppliedSample *applied,const PX_LiveRealtimeAxis *axis)
{
	if (!applied->applied||applied->continuousActive!=axis->continuousActive||
		applied->sampleIndex!=axis->sampleIndex||applied->weightQ15!=axis->weightQ15)
	{
		return PX_FALSE;
	}
	if (axis->continuousActive&&applied->continuousPosition!=axis->continuousPosition)
	{
		return PX_FALSE;
	}
	return PX_TRUE;
}

static px_bool PX_LiveRealtimeAxisHasTexture(const PX_LiveRealtimeAxis *axis)
{
	px_int bindingIndex;
	if (!axis->bindings) return PX_FALSE;
	for (bindingIndex=0;bindingIndex<axis->bindingCount;bindingIndex++)
	{
		if (axis->bindings[bindingIndex].propertyMask&PX_LIVE_REALTIME_PROPERTY_TEXTURE)
		{
			return PX_TRUE;
		}
	}
	return PX_FALSE;
}

/*
 * sign is +1 or -1. doAdditive updates translation/rotation/stretch/vertices.
 * doTexture updates the discrete texture winner. Both paths consume the sample
 * cursor in property-mask order so either mode stays aligned.
 */
static px_bool PX_LiveRealtimeAccumulateAxis(PX_LiveFramework *plive,px_int axisIndex,px_uchar sampleA,px_float sampleT,px_uint16 weightQ15,px_int sign,px_bool doAdditive,px_bool doTexture,px_bool updateHysteresis,px_bool *layerDirty)
{
	PX_LiveRealtime *realtime=&plive->realtime;
	PX_LiveRealtimeAxis *axis=&realtime->axes[axisIndex];
	const px_byte *sampleBase;
	const px_byte *sampleBaseB=PX_NULL;
	px_bool textureChooseHigh;
	px_int bindingIndex;
	if (!axis->valid||!weightQ15||sampleA>=PX_LIVE_REALTIME_SAMPLE_COUNT)
	{
		return PX_TRUE;
	}
	textureChooseHigh=PX_LiveRealtimeTextureChooseHigh(axis,sampleT);
	if (updateHysteresis)
	{
		PX_LiveRealtimeCommitHysteresis(axis,sampleT,textureChooseHigh);
	}
	sampleBase=(const px_byte *)axis->samples+(px_uint32)sampleA*axis->sampleStride;
	if (sampleT>0)
	{
		sampleBaseB=(const px_byte *)axis->samples+(px_uint32)(sampleA+1u)*axis->sampleStride;
	}
	for (bindingIndex=0;bindingIndex<axis->bindingCount;bindingIndex++)
	{
		const PX_LiveRealtimeBinding *binding=&axis->bindings[bindingIndex];
		const px_byte *cursor=sampleBase+binding->sampleOffset;
		const px_byte *cursorB=sampleBaseB?sampleBaseB+binding->sampleOffset:PX_NULL;
		PX_LiveRealtimeLayerAccumulator *layerAccumulator;
		if (binding->layerIndex>=realtime->runtimeLayerCount)
		{
			return PX_FALSE;
		}
		layerAccumulator=&realtime->layerAccumulators[binding->layerIndex];
		if (layerDirty&&binding->layerIndex<PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER)
		{
			layerDirty[binding->layerIndex]=PX_TRUE;
		}
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_TRANSLATION)
		{
			px_int32 x=PX_LiveRealtimeTakeSample(&cursor,cursorB?&cursorB:PX_NULL,sampleT,weightQ15);
			px_int32 y=PX_LiveRealtimeTakeSample(&cursor,cursorB?&cursorB:PX_NULL,sampleT,weightQ15);
			if (doAdditive)
			{
				layerAccumulator->translationX+=sign*x;
				layerAccumulator->translationY+=sign*y;
			}
		}
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_ROTATION)
		{
			px_int32 value=PX_LiveRealtimeTakeSample(&cursor,cursorB?&cursorB:PX_NULL,sampleT,weightQ15);
			if (doAdditive) layerAccumulator->rotation+=sign*value;
		}
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_STRETCH)
		{
			px_int32 value=PX_LiveRealtimeTakeSample(&cursor,cursorB?&cursorB:PX_NULL,sampleT,weightQ15);
			if (doAdditive) layerAccumulator->stretch+=sign*value;
		}
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_TRANSLATION)
		{
			px_int32 x=PX_LiveRealtimeTakeSample(&cursor,cursorB?&cursorB:PX_NULL,sampleT,weightQ15);
			px_int32 y=PX_LiveRealtimeTakeSample(&cursor,cursorB?&cursorB:PX_NULL,sampleT,weightQ15);
			if (doAdditive)
			{
				layerAccumulator->localTranslationX+=sign*x;
				layerAccumulator->localTranslationY+=sign*y;
			}
		}
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_ROTATION)
		{
			px_int32 value=PX_LiveRealtimeTakeSample(&cursor,cursorB?&cursorB:PX_NULL,sampleT,weightQ15);
			if (doAdditive) layerAccumulator->localRotation+=sign*value;
		}
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_SCALE)
		{
			px_int32 value=PX_LiveRealtimeTakeSample(&cursor,cursorB?&cursorB:PX_NULL,sampleT,weightQ15);
			if (doAdditive) layerAccumulator->localScale+=sign*value;
		}
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_TEXTURE)
		{
			px_int32 textureIndex=PX_LiveRealtimeReadInt16(&cursor);
			px_int32 textureOther=textureIndex;
			if (cursorB)
			{
				textureOther=PX_LiveRealtimeReadInt16(&cursorB);
			}
			if (doTexture)
			{
				if (textureChooseHigh) textureIndex=textureOther;
				if (textureIndex<-1||textureIndex>=plive->livetextures.size) return PX_FALSE;
				if (axis->weightQ15>layerAccumulator->textureWeightQ15)
				{
					layerAccumulator->textureIndex=textureIndex;
					layerAccumulator->textureWeightQ15=axis->weightQ15;
					layerAccumulator->textureAxisIndex=(px_uint16)axisIndex;
				}
			}
		}
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_IMPULSE)
		{
			px_int32 x=PX_LiveRealtimeTakeSample(&cursor,cursorB?&cursorB:PX_NULL,sampleT,weightQ15);
			px_int32 y=PX_LiveRealtimeTakeSample(&cursor,cursorB?&cursorB:PX_NULL,sampleT,weightQ15);
			if (doAdditive)
			{
				layerAccumulator->impulseX+=sign*x;
				layerAccumulator->impulseY+=sign*y;
			}
		}
		if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_VERTICES)
		{
			px_int v;
			px_uint32 layerStart=realtime->vertexOffsets[binding->layerIndex];
			px_uint32 layerEnd=realtime->vertexOffsets[binding->layerIndex+1];
			for (v=0;v<binding->vertexCount;v++)
			{
				px_uint32 localVertex=axis->vertexIndices[binding->vertexIndexOffset+v];
				px_uint32 flatVertex=layerStart+localVertex;
				px_int32 x,y;
				if (flatVertex>=layerEnd)
				{
					return PX_FALSE;
				}
				x=PX_LiveRealtimeTakeSample(&cursor,cursorB?&cursorB:PX_NULL,sampleT,weightQ15);
				y=PX_LiveRealtimeTakeSample(&cursor,cursorB?&cursorB:PX_NULL,sampleT,weightQ15);
				if (doAdditive)
				{
					realtime->vertexAccumulators[flatVertex].x+=sign*x;
					realtime->vertexAccumulators[flatVertex].y+=sign*y;
				}
			}
		}
	}
	return PX_TRUE;
}

static px_bool PX_LiveRealtimeVoteTextures(PX_LiveFramework *plive)
{
	PX_LiveRealtime *realtime=&plive->realtime;
	px_int axisIndex,layerIndex;
	for (layerIndex=0;layerIndex<plive->layers.size;layerIndex++)
	{
		PX_LiveRealtimeLayerAccumulator *acc=&realtime->layerAccumulators[layerIndex];
		acc->textureIndex=0;
		acc->textureWeightQ15=0;
		acc->textureAxisIndex=0;
	}
	for (axisIndex=0;axisIndex<realtime->axisCount;axisIndex++)
	{
		PX_LiveRealtimeAxis *axis=&realtime->axes[axisIndex];
		px_uchar sampleA;
		px_float sampleT;
		if (!axis->valid||!axis->weightQ15||!PX_LiveRealtimeAxisHasTexture(axis))
		{
			continue;
		}
		PX_LiveRealtimeResolveSample(axis->continuousActive,axis->continuousPosition,axis->sampleIndex,&sampleA,&sampleT);
		if (!PX_LiveRealtimeAccumulateAxis(plive,axisIndex,sampleA,sampleT,axis->weightQ15,1,PX_FALSE,PX_TRUE,PX_FALSE,PX_NULL))
		{
			return PX_FALSE;
		}
	}
	return PX_TRUE;
}

static px_bool PX_LiveRealtimePreferIncremental(const PX_LiveRealtime *realtime)
{
	px_uint32 fullBytes=0;
	px_uint32 changedBytes=0;
	px_int axisIndex;
	for (axisIndex=0;axisIndex<realtime->axisCount;axisIndex++)
	{
		const PX_LiveRealtimeAxis *axis=&realtime->axes[axisIndex];
		const PX_LiveRealtimeAppliedSample *applied=&realtime->appliedSample[axisIndex];
		if (!axis->valid)
		{
			continue;
		}
		if (axis->weightQ15)
		{
			fullBytes+=axis->sampleStride;
		}
		if (!PX_LiveRealtimeAppliedMatches(applied,axis))
		{
			if (applied->applied&&applied->weightQ15)
			{
				changedBytes+=axis->sampleStride;
			}
			if (axis->weightQ15)
			{
				changedBytes+=axis->sampleStride;
			}
		}
	}
	return changedBytes>0&&changedBytes<fullBytes;
}

static px_bool PX_LiveRealtimeUpdateIncremental(PX_LiveFramework *plive,px_bool *layerDirty,px_bool *writeAll)
{
	PX_LiveRealtime *realtime=&plive->realtime;
	px_int axisIndex;
	px_bool textureDirty=PX_FALSE;
	for (axisIndex=0;axisIndex<realtime->axisCount;axisIndex++)
	{
		PX_LiveRealtimeAxis *axis=&realtime->axes[axisIndex];
		PX_LiveRealtimeAppliedSample *applied=&realtime->appliedSample[axisIndex];
		px_uchar sampleA;
		px_float sampleT;
		if (axis->valid&&PX_LiveRealtimeAppliedMatches(applied,axis))
		{
			continue;
		}
		if (applied->applied&&applied->weightQ15)
		{
			PX_LiveRealtimeResolveSample(applied->continuousActive,applied->continuousPosition,applied->sampleIndex,&sampleA,&sampleT);
			if (!PX_LiveRealtimeAccumulateAxis(plive,axisIndex,sampleA,sampleT,applied->weightQ15,-1,PX_TRUE,PX_FALSE,PX_FALSE,layerDirty))
			{
				return PX_FALSE;
			}
		}
		if (axis->valid&&axis->weightQ15)
		{
			PX_LiveRealtimeResolveSample(axis->continuousActive,axis->continuousPosition,axis->sampleIndex,&sampleA,&sampleT);
			if (!PX_LiveRealtimeAccumulateAxis(plive,axisIndex,sampleA,sampleT,axis->weightQ15,1,PX_TRUE,PX_FALSE,PX_TRUE,layerDirty))
			{
				return PX_FALSE;
			}
		}
		if (PX_LiveRealtimeAxisHasTexture(axis))
		{
			textureDirty=PX_TRUE;
		}
	}
	if (textureDirty)
	{
		if (!PX_LiveRealtimeVoteTextures(plive))
		{
			return PX_FALSE;
		}
		*writeAll=PX_TRUE;
	}
	return PX_TRUE;
}

static px_void PX_LiveRealtimeSnapshotApplied(PX_LiveRealtime *realtime)
{
	px_int axisIndex;
	for (axisIndex=0;axisIndex<realtime->axisCount;axisIndex++)
	{
		PX_LiveRealtimeAxis *axis=&realtime->axes[axisIndex];
		PX_LiveRealtimeAppliedSample *applied=&realtime->appliedSample[axisIndex];
		applied->applied=axis->valid?PX_TRUE:PX_FALSE;
		applied->continuousActive=axis->continuousActive;
		applied->continuousPosition=axis->continuousPosition;
		applied->sampleIndex=axis->sampleIndex;
		applied->weightQ15=axis->weightQ15;
	}
	for (;axisIndex<PX_LIVE_REALTIME_MAX_AXES;axisIndex++)
	{
		realtime->appliedSample[axisIndex].applied=PX_FALSE;
	}
	realtime->contributionValid=PX_TRUE;
}

px_bool PX_LiveRealtimeUpdate(PX_LiveFramework *plive)
{
	PX_LiveRealtime *realtime;
	px_int axisIndex,layerIndex;
	px_float coordScale,rotationScale,stretchScale;
	px_bool writeAll;
	px_bool layerDirty[PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER];
	if (!plive||plive->animationMode!=PX_LIVE_MODE_REALTIME30)
	{
		return PX_FALSE;
	}
	realtime=&plive->realtime;
	if (realtime->runtimeLayerCount!=(px_uint16)plive->layers.size||
		(realtime->runtimeLayerCount&&!realtime->runtimeBlock))
	{
		/* Runtime topology must be prepared before entering the frame loop. */
		return PX_FALSE;
	}
	for (layerIndex=0;layerIndex<plive->layers.size;layerIndex++)
	{
		PX_LiveLayer *layer=PX_VECTORAT(PX_LiveLayer,&plive->layers,layerIndex);
		if (realtime->vertexOffsets[layerIndex+1]-realtime->vertexOffsets[layerIndex]!=(px_uint32)layer->vertices.size)
		{
			/* Editing topology requires an explicit PrepareRuntime before frames resume. */
			return PX_FALSE;
		}
	}
	if (realtime->contributionValid&&!realtime->dirty&&realtime->evaluatedRevision==realtime->stateRevision)
	{
		return PX_TRUE;
	}
	writeAll=!realtime->contributionValid||!PX_LiveRealtimePreferIncremental(realtime);
	PX_memset(layerDirty,0,sizeof(layerDirty));
	if (!writeAll)
	{
		if (!PX_LiveRealtimeUpdateIncremental(plive,layerDirty,&writeAll))
		{
			realtime->contributionValid=PX_FALSE;
			return PX_FALSE;
		}
		if (plive->layers.size>PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER)
		{
			writeAll=PX_TRUE;
		}
	}
	else
	{
	if (realtime->runtimeLayerCount)
	{
		PX_memset(realtime->layerAccumulators,0,(px_uint)realtime->runtimeLayerCount*sizeof(PX_LiveRealtimeLayerAccumulator));
	}
	if (realtime->runtimeVertexCount)
	{
		PX_memset(realtime->vertexAccumulators,0,realtime->runtimeVertexCount*(px_uint)sizeof(PX_LiveRealtimeVertexAccumulator));
	}
	for (axisIndex=0;axisIndex<realtime->axisCount;axisIndex++)
	{
		PX_LiveRealtimeAxis *axis=&realtime->axes[axisIndex];
		const px_byte *sampleBase;
		const px_byte *sampleBaseB=PX_NULL;
		px_float sampleT=0;
		px_uchar sampleA;
		px_bool textureChooseHigh=PX_FALSE;
		px_int bindingIndex;
		if (!axis->valid||!axis->weightQ15)
		{
			continue;
		}
		sampleA=axis->sampleIndex;
		if (axis->continuousActive)
		{
			px_float q=axis->continuousPosition;
			if (q<0) q=0;
			if (q>(px_float)(PX_LIVE_REALTIME_SAMPLE_COUNT-1)) q=(px_float)(PX_LIVE_REALTIME_SAMPLE_COUNT-1);
			sampleA=(px_uchar)q;
			sampleT=q-(px_float)sampleA;
			if (sampleA>=PX_LIVE_REALTIME_SAMPLE_COUNT-1)
			{
				sampleA=PX_LIVE_REALTIME_SAMPLE_COUNT-1;
				sampleT=0;
			}
		}
		if (sampleA>=PX_LIVE_REALTIME_SAMPLE_COUNT)
		{
			continue;
		}
		if (axis->textureHysteresis==1&&sampleT<0.58f) textureChooseHigh=PX_FALSE;
		else if (axis->textureHysteresis==2&&sampleT>0.42f) textureChooseHigh=PX_TRUE;
		else textureChooseHigh=sampleT>=0.5f;
		if (sampleT>0) axis->textureHysteresis=textureChooseHigh?2:1;
		else axis->textureHysteresis=0;
		sampleBase=(const px_byte *)axis->samples+(px_uint32)sampleA*axis->sampleStride;
		if (sampleT>0)
		{
			sampleBaseB=(const px_byte *)axis->samples+(px_uint32)(sampleA+1u)*axis->sampleStride;
		}
		for (bindingIndex=0;bindingIndex<axis->bindingCount;bindingIndex++)
		{
			const PX_LiveRealtimeBinding *binding=&axis->bindings[bindingIndex];
			const px_byte *cursor=sampleBase+binding->sampleOffset;
			const px_byte *cursorB=sampleBaseB?sampleBaseB+binding->sampleOffset:PX_NULL;
			PX_LiveRealtimeLayerAccumulator *layerAccumulator;
			if (binding->layerIndex>=realtime->runtimeLayerCount)
			{
				return PX_FALSE;
			}
			layerAccumulator=&realtime->layerAccumulators[binding->layerIndex];
			if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_TRANSLATION)
			{
				layerAccumulator->translationX+=PX_LiveRealtimeTakeSample(&cursor,cursorB?&cursorB:PX_NULL,sampleT,axis->weightQ15);
				layerAccumulator->translationY+=PX_LiveRealtimeTakeSample(&cursor,cursorB?&cursorB:PX_NULL,sampleT,axis->weightQ15);
			}
			if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_ROTATION)
			{
				layerAccumulator->rotation+=PX_LiveRealtimeTakeSample(&cursor,cursorB?&cursorB:PX_NULL,sampleT,axis->weightQ15);
			}
			if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_STRETCH)
			{
				layerAccumulator->stretch+=PX_LiveRealtimeTakeSample(&cursor,cursorB?&cursorB:PX_NULL,sampleT,axis->weightQ15);
			}
			if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_TRANSLATION)
			{
				layerAccumulator->localTranslationX+=PX_LiveRealtimeTakeSample(&cursor,cursorB?&cursorB:PX_NULL,sampleT,axis->weightQ15);
				layerAccumulator->localTranslationY+=PX_LiveRealtimeTakeSample(&cursor,cursorB?&cursorB:PX_NULL,sampleT,axis->weightQ15);
			}
			if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_ROTATION)
			{
				layerAccumulator->localRotation+=PX_LiveRealtimeTakeSample(&cursor,cursorB?&cursorB:PX_NULL,sampleT,axis->weightQ15);
			}
			if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_SCALE)
			{
				layerAccumulator->localScale+=PX_LiveRealtimeTakeSample(&cursor,cursorB?&cursorB:PX_NULL,sampleT,axis->weightQ15);
			}
			if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_TEXTURE)
			{
				px_int32 textureIndex=PX_LiveRealtimeReadInt16(&cursor);
				px_int32 textureOther=textureIndex;
				if (cursorB)
				{
					textureOther=PX_LiveRealtimeReadInt16(&cursorB);
				}
				if (textureChooseHigh)
				{
					textureIndex=textureOther;
				}
				if (textureIndex<-1||textureIndex>=plive->livetextures.size) return PX_FALSE;
				if (axis->weightQ15>layerAccumulator->textureWeightQ15)
				{
					layerAccumulator->textureIndex=textureIndex;
					layerAccumulator->textureWeightQ15=axis->weightQ15;
					layerAccumulator->textureAxisIndex=(px_uint16)axisIndex;
				}
			}
			if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_IMPULSE)
			{
				layerAccumulator->impulseX+=PX_LiveRealtimeTakeSample(&cursor,cursorB?&cursorB:PX_NULL,sampleT,axis->weightQ15);
				layerAccumulator->impulseY+=PX_LiveRealtimeTakeSample(&cursor,cursorB?&cursorB:PX_NULL,sampleT,axis->weightQ15);
			}
			if (binding->propertyMask&PX_LIVE_REALTIME_PROPERTY_VERTICES)
			{
				px_int v;
				px_uint32 layerStart=realtime->vertexOffsets[binding->layerIndex];
				px_uint32 layerEnd=realtime->vertexOffsets[binding->layerIndex+1];
				for (v=0;v<binding->vertexCount;v++)
				{
					px_uint32 localVertex=axis->vertexIndices[binding->vertexIndexOffset+v];
					px_uint32 flatVertex=layerStart+localVertex;
					if (flatVertex>=layerEnd)
					{
						return PX_FALSE;
					}
					realtime->vertexAccumulators[flatVertex].x+=PX_LiveRealtimeTakeSample(&cursor,cursorB?&cursorB:PX_NULL,sampleT,axis->weightQ15);
					realtime->vertexAccumulators[flatVertex].y+=PX_LiveRealtimeTakeSample(&cursor,cursorB?&cursorB:PX_NULL,sampleT,axis->weightQ15);
				}
			}
		}
	}
	}
	coordScale=1.0f/(px_float)(1u<<realtime->coordFractionBits);
	rotationScale=1.0f/(px_float)(1u<<realtime->rotationFractionBits);
	stretchScale=1.0f/(px_float)(1u<<realtime->stretchFractionBits);
	for (layerIndex=0;layerIndex<plive->layers.size;layerIndex++)
	{
		PX_LiveLayer *layer=PX_VECTORAT(PX_LiveLayer,&plive->layers,layerIndex);
		PX_LiveRealtimeLayerAccumulator *acc=&realtime->layerAccumulators[layerIndex];
		px_uint32 vertexStart=realtime->vertexOffsets[layerIndex];
		px_int v;
		if (!writeAll&&(layerIndex>=PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER||!layerDirty[layerIndex]))
		{
			continue;
		}
		layer->rel_currentTranslation=PX_POINT(acc->translationX*coordScale,acc->translationY*coordScale,0);
		layer->rel_beginTranslation=layer->rel_currentTranslation;
		layer->rel_endTranslation=layer->rel_currentTranslation;
		layer->rel_currentRotationAngle=acc->rotation*rotationScale;
		layer->rel_beginRotationAngle=layer->rel_currentRotationAngle;
		layer->rel_endRotationAngle=layer->rel_currentRotationAngle;
		layer->rel_currentStretch=1.0f+acc->stretch*stretchScale;
		layer->rel_beginStretch=layer->rel_currentStretch;
		layer->rel_endStretch=layer->rel_currentStretch;
		layer->rel_currentLocalTranslation=PX_POINT(acc->localTranslationX*coordScale,acc->localTranslationY*coordScale,0);
		layer->rel_beginLocalTranslation=layer->rel_currentLocalTranslation;
		layer->rel_endLocalTranslation=layer->rel_currentLocalTranslation;
		layer->rel_currentLocalRotationAngle=acc->localRotation*rotationScale;
		layer->rel_beginLocalRotationAngle=layer->rel_currentLocalRotationAngle;
		layer->rel_endLocalRotationAngle=layer->rel_currentLocalRotationAngle;
		layer->rel_currentLocalScale=1.0f+acc->localScale*stretchScale;
		layer->rel_beginLocalScale=layer->rel_currentLocalScale;
		layer->rel_endLocalScale=layer->rel_currentLocalScale;
		layer->RenderTextureIndex=acc->textureWeightQ15?acc->textureIndex:layer->LinkTextureIndex;
		layer->rel_impulse=PX_POINT(acc->impulseX*coordScale,acc->impulseY*coordScale,0);
		for (v=0;v<layer->vertices.size;v++)
		{
			PX_LiveVertex *vertex=PX_VECTORAT(PX_LiveVertex,&layer->vertices,v);
			PX_LiveRealtimeVertexAccumulator *vertexAcc=&realtime->vertexAccumulators[vertexStart+v];
			vertex->currentTranslation=PX_POINT(vertexAcc->x*coordScale,vertexAcc->y*coordScale,0);
			vertex->beginTranslation=vertex->currentTranslation;
			vertex->endTranslation=vertex->currentTranslation;
		}
	}
	PX_LiveRealtimeSnapshotApplied(realtime);
	realtime->dirty=PX_FALSE;
	realtime->evaluatedRevision=realtime->stateRevision;
	return PX_TRUE;
}

px_void PX_LiveRealtimeGetMemoryStats(const PX_LiveFramework *plive,PX_LiveRealtimeMemoryStats *stats)
{
	px_int i;
	if (!stats)
	{
		return;
	}
	PX_memset(stats,0,sizeof(*stats));
	if (!plive)
	{
		return;
	}
	stats->staticBytes=plive->realtime.staticBytes;
	stats->runtimeBytes=plive->realtime.runtimeBytes;
	stats->totalBytes=stats->staticBytes+stats->runtimeBytes;
	stats->layerCount=plive->realtime.runtimeLayerCount;
	stats->vertexCount=plive->realtime.runtimeVertexCount;
	for (i=0;i<plive->realtime.axisCount;i++)
	{
		const PX_LiveRealtimeAxis *axis=&plive->realtime.axes[i];
		if (axis->valid&&axis->weightQ15)
		{
			stats->activeAxisCount++;
			stats->selectedSampleBytes+=axis->sampleStride;
		}
	}
}
