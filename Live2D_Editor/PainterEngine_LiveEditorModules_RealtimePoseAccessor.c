#include "PainterEngine_LiveEditorModules_RealtimePoseAccessor.h"

static px_void PX_LiveEditorRealtimePoseAccessorFreeAuthoring(PX_LiveEditorRealtimePoseAccessor *accessor,PX_LiveEditorRealtimeAxisAuthoring *axis)
{
	px_int key;
	if (!accessor || !axis || !accessor->mp) return;
	if (axis->bindings) MP_Free(accessor->mp,axis->bindings);
	if (axis->vertexIndices) MP_Free(accessor->mp,axis->vertexIndices);
	for (key=0;key<3;key++)
	{
		if (axis->keyBindingPoses[key]) MP_Free(accessor->mp,axis->keyBindingPoses[key]);
		if (axis->keyVertexDeltas[key]) MP_Free(accessor->mp,axis->keyVertexDeltas[key]);
		axis->keyBindingPoses[key]=PX_NULL;
		axis->keyVertexDeltas[key]=PX_NULL;
	}
	axis->bindings=PX_NULL;
	axis->vertexIndices=PX_NULL;
	axis->bindingCount=0;
	axis->vertexIndexCount=0;
	axis->authoringAvailable=PX_FALSE;
}

static px_void PX_LiveEditorRealtimePoseAccessorFreeAxis(PX_LiveEditorRealtimePoseAccessor *accessor,PX_LiveEditorRealtimeAxisAuthoring *axis)
{
	if (!accessor || !axis || !accessor->mp)
	{
		return;
	}
	PX_LiveEditorRealtimePoseAccessorFreeAuthoring(accessor,axis);
	PX_memset(axis,0,sizeof(*axis));
	axis->runtimeHandle=PX_LIVE_REALTIME_INVALID_HANDLE;
}

static px_int PX_LiveEditorRealtimePoseAccessorDefaultSlot(const PX_LiveEditorRealtimeAxisAuthoring *axis)
{
	if (axis->defaultSampleIndex==0) return 0;
	if (axis->defaultSampleIndex==PX_LIVE_REALTIME_SAMPLE_COUNT-1) return 2;
	return 1;
}

static px_uchar PX_LiveEditorRealtimePoseAccessorSlotSample(const PX_LiveEditorRealtimeAxisAuthoring *axis,px_int keySlot)
{
	if (keySlot<=0) return 0;
	if (keySlot>=2) return PX_LIVE_REALTIME_SAMPLE_COUNT-1;
	return axis->middleKeyIndex;
}

static px_bool PX_LiveEditorRealtimePoseAccessorAllocateAuthoring(PX_LiveEditorRealtimePoseAccessor *accessor,PX_LiveEditorRealtimeAxisAuthoring *axis)
{
	px_int i,key;
	px_uint32 vertexCount=0,vertexOffset=0;
	px_int layerCount=accessor->plive->layers.size;
	if (layerCount<=0 || layerCount>65535)
	{
		return PX_FALSE;
	}
	for (i=0;i<layerCount;i++)
	{
		PX_LiveLayer *layer=PX_LiveFrameworkGetLayer(accessor->plive,i);
		if (!layer || layer->vertices.size>65535 || vertexCount+(px_uint32)layer->vertices.size<vertexCount)
		{
			return PX_FALSE;
		}
		vertexCount+=(px_uint32)layer->vertices.size;
	}
	/* A failed restore may leave allocated-but-disabled dense buffers.  Drop
	   only authoring storage; runtime id/handle and axis settings stay valid. */
	PX_LiveEditorRealtimePoseAccessorFreeAuthoring(accessor,axis);

	axis->bindings=(PX_LiveRealtimeBindingDesc *)MP_Malloc(accessor->mp,sizeof(PX_LiveRealtimeBindingDesc)*layerCount);
	if (vertexCount)
	{
		axis->vertexIndices=(px_uint16 *)MP_Malloc(accessor->mp,sizeof(px_uint16)*vertexCount);
	}
	for (key=0;key<3;key++)
	{
		axis->keyBindingPoses[key]=(PX_LiveRealtimeBindingPose *)MP_Malloc(accessor->mp,sizeof(PX_LiveRealtimeBindingPose)*layerCount);
		if (vertexCount)
		{
			axis->keyVertexDeltas[key]=(px_point *)MP_Malloc(accessor->mp,sizeof(px_point)*vertexCount);
		}
	}
	if (!axis->bindings || (vertexCount&&!axis->vertexIndices))
	{
		PX_LiveEditorRealtimePoseAccessorFreeAuthoring(accessor,axis);
		return PX_FALSE;
	}
	for (key=0;key<3;key++)
	{
		if (!axis->keyBindingPoses[key] || (vertexCount&&!axis->keyVertexDeltas[key]))
		{
			PX_LiveEditorRealtimePoseAccessorFreeAuthoring(accessor,axis);
			return PX_FALSE;
		}
	}

	PX_memset(axis->bindings,0,sizeof(PX_LiveRealtimeBindingDesc)*layerCount);
	for (key=0;key<3;key++)
	{
		PX_memset(axis->keyBindingPoses[key],0,sizeof(PX_LiveRealtimeBindingPose)*layerCount);
		if (vertexCount) PX_memset(axis->keyVertexDeltas[key],0,sizeof(px_point)*vertexCount);
	}
	for (i=0;i<layerCount;i++)
	{
		PX_LiveLayer *layer=PX_LiveFrameworkGetLayer(accessor->plive,i);
		px_int j;
		axis->bindings[i].layerIndex=(px_uint16)i;
		axis->bindings[i].propertyMask=PX_LIVE_REALTIME_PROPERTY_ROTATION|PX_LIVE_REALTIME_PROPERTY_LOCAL_TRANSLATION|PX_LIVE_REALTIME_PROPERTY_LOCAL_ROTATION|PX_LIVE_REALTIME_PROPERTY_LOCAL_SCALE|PX_LIVE_REALTIME_PROPERTY_TEXTURE|PX_LIVE_REALTIME_PROPERTY_IMPULSE;
		axis->bindings[i].vertexCount=(px_uint16)layer->vertices.size;
		if (layer->vertices.size)
		{
			axis->bindings[i].propertyMask|=PX_LIVE_REALTIME_PROPERTY_VERTICES;
			axis->bindings[i].vertexIndices=axis->vertexIndices+vertexOffset;
			for (j=0;j<layer->vertices.size;j++) axis->vertexIndices[vertexOffset+j]=(px_uint16)j;
		}
		for (key=0;key<3;key++)
		{
			axis->keyBindingPoses[key][i].stretch=1;
			axis->keyBindingPoses[key][i].localScale=1;
			axis->keyBindingPoses[key][i].mapTexture=layer->LinkTextureIndex;
			axis->keyBindingPoses[key][i].impulse=PX_POINT(0,0,0);
			axis->keyBindingPoses[key][i].vertexDeltas=layer->vertices.size?axis->keyVertexDeltas[key]+vertexOffset:PX_NULL;
		}
		vertexOffset+=(px_uint32)layer->vertices.size;
	}
	axis->bindingCount=(px_uint16)layerCount;
	axis->vertexIndexCount=vertexCount;
	axis->authoringAvailable=PX_TRUE;
	/* Authoring starts from the neutral model pose.  Never seed a new axis
	   from the currently displayed (possibly multi-axis) composite pose. */
	return PX_TRUE;
}

static px_float PX_LiveEditorRealtimePoseAccessorAbs(px_float value)
{
	return value<0?-value:value;
}

static px_bool PX_LiveEditorRealtimePoseAccessorIsDifferent(px_float a,px_float b)
{
	return PX_LiveEditorRealtimePoseAccessorAbs(a-b)>0.00001f;
}

/* The sidebar previews one selected axis at a time.  Keep the accessor's
   saved sample/weight for every axis, but clear their runtime weights before
   activating the selected axis so a previous neck/eye pose cannot leak into
   the current 30-sample inspection.  The engine's public realtime API still
   supports normal multi-axis blending for the deployed runtime. */
static px_bool PX_LiveEditorRealtimePoseAccessorApplySoloPreview(PX_LiveEditorRealtimePoseAccessor *accessor,PX_LiveEditorRealtimeAxisAuthoring *axis,px_uchar sampleIndex,px_uint16 weightQ15)
{
	if (!accessor || !axis || axis->runtimeHandle==PX_LIVE_REALTIME_INVALID_HANDLE) return PX_FALSE;
	if (!accessor->entered || accessor->authoringPoseActive || accessor->plive->animationMode!=PX_LIVE_MODE_REALTIME30)
	{
		if (!PX_LiveRealtimeEnter(accessor->plive)) return PX_FALSE;
	}
	accessor->entered=PX_TRUE;
	PX_LiveRealtimeResetAll(accessor->plive);
	if (!PX_LiveRealtimeSetAxisSample(accessor->plive,axis->runtimeHandle,sampleIndex,weightQ15)) return PX_FALSE;
	if (!PX_LiveRealtimeUpdate(accessor->plive)) return PX_FALSE;
	accessor->authoringPoseActive=PX_FALSE;
	accessor->blendPreviewActive=PX_FALSE;
	return PX_TRUE;
}

static px_void PX_LiveEditorRealtimePoseAccessorScanBakedMesh(PX_LiveEditorRealtimePoseAccessor *accessor,PX_LiveEditorRealtimeAxisAuthoring *axis);
static px_bool PX_LiveEditorRealtimePoseAccessorBakeAxis(PX_LiveEditorRealtimePoseAccessor *accessor,PX_LiveEditorRealtimeAxisAuthoring *axis)
{
	PX_LiveRealtimeAxisBakeDesc desc;
	PX_LiveRealtimeBindingDesc *sparseBindings=PX_NULL;
	PX_LiveRealtimeBindingPose *sparsePoses[3]={PX_NULL,PX_NULL,PX_NULL};
	px_uint16 *sparseIndices=PX_NULL;
	px_point *sparseVertices[3]={PX_NULL,PX_NULL,PX_NULL};
	px_int key,newHandle=PX_LIVE_REALTIME_INVALID_HANDLE;
	px_int sourceBinding,sparseBindingCount=0,defaultSlot;
	px_uint32 sourceVertexOffset=0,sparseVertexCount=0;
	px_bool result=PX_FALSE;
	if (!axis->authoringAvailable || !axis->bindings || !axis->bindingCount)
	{
		return PX_FALSE;
	}

	sparseBindings=(PX_LiveRealtimeBindingDesc *)MP_Malloc(accessor->mp,sizeof(PX_LiveRealtimeBindingDesc)*axis->bindingCount);
	sparseIndices=axis->vertexIndexCount?(px_uint16 *)MP_Malloc(accessor->mp,sizeof(px_uint16)*axis->vertexIndexCount):PX_NULL;
	for (key=0;key<3;key++)
	{
		sparsePoses[key]=(PX_LiveRealtimeBindingPose *)MP_Malloc(accessor->mp,sizeof(PX_LiveRealtimeBindingPose)*axis->bindingCount);
		sparseVertices[key]=axis->vertexIndexCount?(px_point *)MP_Malloc(accessor->mp,sizeof(px_point)*axis->vertexIndexCount):PX_NULL;
	}
	if (!sparseBindings || (axis->vertexIndexCount&&!sparseIndices)) goto cleanup;
	for (key=0;key<3;key++) if (!sparsePoses[key] || (axis->vertexIndexCount&&!sparseVertices[key])) goto cleanup;

	defaultSlot=PX_LiveEditorRealtimePoseAccessorDefaultSlot(axis);
	for (sourceBinding=0;sourceBinding<axis->bindingCount;sourceBinding++)
	{
		px_uint16 propertyMask=0;
		px_uint16 activeVertexCount=0;
		px_int j;
		for (key=0;key<3;key++)
		{
			PX_LiveRealtimeBindingPose *pose=&axis->keyBindingPoses[key][sourceBinding];
			PX_LiveRealtimeBindingPose *base=&axis->keyBindingPoses[defaultSlot][sourceBinding];
			if (PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->translation.x,base->translation.x)||PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->translation.y,base->translation.y)) propertyMask|=PX_LIVE_REALTIME_PROPERTY_TRANSLATION;
			if (PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->rotation,base->rotation)) propertyMask|=PX_LIVE_REALTIME_PROPERTY_ROTATION;
			if (PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->stretch,base->stretch)) propertyMask|=PX_LIVE_REALTIME_PROPERTY_STRETCH;
			if (PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->localTranslation.x,base->localTranslation.x)||PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->localTranslation.y,base->localTranslation.y)) propertyMask|=PX_LIVE_REALTIME_PROPERTY_LOCAL_TRANSLATION;
			if (PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->localRotation,base->localRotation)) propertyMask|=PX_LIVE_REALTIME_PROPERTY_LOCAL_ROTATION;
			if (PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->localScale,base->localScale)) propertyMask|=PX_LIVE_REALTIME_PROPERTY_LOCAL_SCALE;
			if (pose->mapTexture!=base->mapTexture) propertyMask|=PX_LIVE_REALTIME_PROPERTY_TEXTURE;
			if (PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->impulse.x,base->impulse.x)||PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->impulse.y,base->impulse.y)) propertyMask|=PX_LIVE_REALTIME_PROPERTY_IMPULSE;
		}
		for (j=0;j<axis->bindings[sourceBinding].vertexCount;j++)
		{
			px_bool active=PX_FALSE;
			for (key=0;key<3;key++)
			{
				px_point value=axis->keyVertexDeltas[key][sourceVertexOffset+j];
				px_point base=axis->keyVertexDeltas[defaultSlot][sourceVertexOffset+j];
				if (PX_LiveEditorRealtimePoseAccessorIsDifferent(value.x,base.x)||PX_LiveEditorRealtimePoseAccessorIsDifferent(value.y,base.y)) active=PX_TRUE;
			}
			if (active)
			{
				sparseIndices[sparseVertexCount+activeVertexCount]=axis->bindings[sourceBinding].vertexIndices[j];
				for (key=0;key<3;key++) sparseVertices[key][sparseVertexCount+activeVertexCount]=axis->keyVertexDeltas[key][sourceVertexOffset+j];
				activeVertexCount++;
			}
		}
		if (activeVertexCount) propertyMask|=PX_LIVE_REALTIME_PROPERTY_VERTICES;
		if (propertyMask)
		{
			PX_LiveRealtimeBindingDesc *binding=&sparseBindings[sparseBindingCount];
			binding->layerIndex=axis->bindings[sourceBinding].layerIndex;
			binding->propertyMask=propertyMask;
			binding->vertexCount=activeVertexCount;
			binding->reserved=0;
			binding->vertexIndices=activeVertexCount?sparseIndices+sparseVertexCount:PX_NULL;
			for (key=0;key<3;key++)
			{
				sparsePoses[key][sparseBindingCount]=axis->keyBindingPoses[key][sourceBinding];
				sparsePoses[key][sparseBindingCount].vertexDeltas=activeVertexCount?sparseVertices[key]+sparseVertexCount:PX_NULL;
			}
			sparseVertexCount+=activeVertexCount;
			sparseBindingCount++;
		}
		sourceVertexOffset+=axis->bindings[sourceBinding].vertexCount;
	}
	if (!sparseBindingCount) goto cleanup;

	PX_memset(&desc,0,sizeof(desc));
	desc.id=axis->id;
	desc.middleKeyIndex=axis->middleKeyIndex;
	desc.defaultSampleIndex=axis->defaultSampleIndex;
	/* All axes share one quantization contract so int32 accumulators can mix directly. */
	desc.coordFractionBits=4;
	desc.rotationFractionBits=6;
	desc.stretchFractionBits=14;
	desc.bindingCount=(px_uint16)sparseBindingCount;
	desc.bindings=sparseBindings;
	for (key=0;key<3;key++) desc.keyPoses[key].bindings=sparsePoses[key];
	if (!PX_LiveRealtimeBakeAxis(accessor->plive,axis->runtimeHandle,&desc,&newHandle))
	{
		goto cleanup;
	}
	axis->runtimeHandle=newHandle;
	axis->baked=PX_TRUE;
	axis->dirty=PX_FALSE;
	if (!PX_LiveRealtimePrepareRuntime(accessor->plive)) goto runtime_failed;
	if (!PX_LiveEditorRealtimePoseAccessorApplySoloPreview(accessor,axis,axis->sampleIndex,axis->weightQ15)) goto runtime_failed;
	result=PX_TRUE;
	goto cleanup;
runtime_failed:
	/* The baked block exists, but it is not safe to present it as a usable
	   preview until runtime preparation and the first evaluation succeed. */
	axis->dirty=PX_TRUE;
	accessor->entered=PX_FALSE;
	accessor->authoringPoseActive=PX_FALSE;
	accessor->blendPreviewActive=PX_FALSE;
	PX_LiveRealtimeLeave(accessor->plive);
cleanup:
	if (sparseBindings) MP_Free(accessor->mp,sparseBindings);
	if (sparseIndices) MP_Free(accessor->mp,sparseIndices);
	for (key=0;key<3;key++)
	{
		if (sparsePoses[key]) MP_Free(accessor->mp,sparsePoses[key]);
		if (sparseVertices[key]) MP_Free(accessor->mp,sparseVertices[key]);
	}
	return result;
}

static px_float PX_LiveEditorRealtimePoseAccessorDecode(px_int16 value,px_uchar fractionBits)
{
	return (px_float)value/(px_float)(1u<<fractionBits);
}

static px_int PX_LiveEditorRealtimePoseAccessorFindBinding(const PX_LiveEditorRealtimeAxisAuthoring *axis,px_int layerIndex)
{
	px_int i;
	if (!axis) return -1;
	for (i=0;i<axis->bindingCount;i++)
	{
		if (axis->bindings[i].layerIndex==layerIndex) return i;
	}
	return -1;
}

static px_uint32 PX_LiveEditorRealtimePoseAccessorGetVertexOffset(const PX_LiveEditorRealtimeAxisAuthoring *axis,px_int bindingIndex)
{
	px_int i;
	px_uint32 offset=0;
	if (!axis || bindingIndex<0 || bindingIndex>=axis->bindingCount) return 0;
	for (i=0;i<bindingIndex;i++) offset+=axis->bindings[i].vertexCount;
	return offset;
}

static px_bool PX_LiveEditorRealtimePoseAccessorRestoreAuthoring(PX_LiveEditorRealtimePoseAccessor *accessor,PX_LiveEditorRealtimeAxisAuthoring *axis,const PX_LiveRealtimeAxis *runtimeAxis)
{
	px_int i,key;
	px_uchar keySamples[3];
	px_int savedHandle;
	px_char savedId[PX_LIVE_REALTIME_AXIS_ID_MAX_LEN];
	if (!runtimeAxis || !runtimeAxis->bindingCount) return PX_FALSE;
	/* Runtime data is sparse.  Expand it back to a dense editor-only pose so
	   every layer remains selectable after loading an existing .live file. */
	savedHandle=axis->runtimeHandle;
	PX_strcpy(savedId,axis->id,sizeof(savedId));
	if (!PX_LiveEditorRealtimePoseAccessorAllocateAuthoring(accessor,axis))
	{
		/* AllocateAuthoring releases partial allocations on failure.  Preserve
		   the runtime identity so the axis can still be previewed/deleted. */
		axis->runtimeHandle=savedHandle;
		PX_strcpy(axis->id,savedId,sizeof(axis->id));
		axis->middleKeyIndex=runtimeAxis->middleKeyIndex;
		axis->defaultSampleIndex=runtimeAxis->defaultSampleIndex;
		axis->selectedKeySlot=(px_uchar)PX_LiveEditorRealtimePoseAccessorDefaultSlot(axis);
		axis->sampleIndex=runtimeAxis->sampleIndex;
		axis->weightQ15=runtimeAxis->weightQ15;
		axis->baked=PX_TRUE;
		return PX_FALSE;
	}
	keySamples[0]=0;
	keySamples[1]=runtimeAxis->middleKeyIndex;
	keySamples[2]=PX_LIVE_REALTIME_SAMPLE_COUNT-1;
	for (i=0;i<runtimeAxis->bindingCount;i++)
	{
		const PX_LiveRealtimeBinding *sourceBinding=&runtimeAxis->bindings[i];
		px_int targetBinding=PX_LiveEditorRealtimePoseAccessorFindBinding(axis,sourceBinding->layerIndex);
		px_uint32 targetVertexOffset;
		px_int j;
		if (targetBinding<0) return PX_FALSE;
		targetVertexOffset=PX_LiveEditorRealtimePoseAccessorGetVertexOffset(axis,targetBinding);
		for (key=0;key<3;key++)
		{
			const px_byte *sample=(const px_byte *)runtimeAxis->samples+(px_uint32)keySamples[key]*runtimeAxis->sampleStride+sourceBinding->sampleOffset;
			const px_int16 *values=(const px_int16 *)sample;
			PX_LiveRealtimeBindingPose *pose=&axis->keyBindingPoses[key][targetBinding];
			px_int valueIndex=0;
			if (sourceBinding->propertyMask&PX_LIVE_REALTIME_PROPERTY_TRANSLATION)
			{
				pose->translation.x=PX_LiveEditorRealtimePoseAccessorDecode(values[valueIndex++],runtimeAxis->coordFractionBits);
				pose->translation.y=PX_LiveEditorRealtimePoseAccessorDecode(values[valueIndex++],runtimeAxis->coordFractionBits);
			}
			if (sourceBinding->propertyMask&PX_LIVE_REALTIME_PROPERTY_ROTATION) pose->rotation=PX_LiveEditorRealtimePoseAccessorDecode(values[valueIndex++],runtimeAxis->rotationFractionBits);
			if (sourceBinding->propertyMask&PX_LIVE_REALTIME_PROPERTY_STRETCH) pose->stretch=1+PX_LiveEditorRealtimePoseAccessorDecode(values[valueIndex++],runtimeAxis->stretchFractionBits);
			if (sourceBinding->propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_TRANSLATION)
			{
				pose->localTranslation.x=PX_LiveEditorRealtimePoseAccessorDecode(values[valueIndex++],runtimeAxis->coordFractionBits);
				pose->localTranslation.y=PX_LiveEditorRealtimePoseAccessorDecode(values[valueIndex++],runtimeAxis->coordFractionBits);
			}
			if (sourceBinding->propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_ROTATION) pose->localRotation=PX_LiveEditorRealtimePoseAccessorDecode(values[valueIndex++],runtimeAxis->rotationFractionBits);
			if (sourceBinding->propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_SCALE) pose->localScale=1+PX_LiveEditorRealtimePoseAccessorDecode(values[valueIndex++],runtimeAxis->stretchFractionBits);
			if (sourceBinding->propertyMask&PX_LIVE_REALTIME_PROPERTY_TEXTURE) pose->mapTexture=values[valueIndex++];
			if (sourceBinding->propertyMask&PX_LIVE_REALTIME_PROPERTY_IMPULSE)
			{
				pose->impulse.x=PX_LiveEditorRealtimePoseAccessorDecode(values[valueIndex++],runtimeAxis->coordFractionBits);
				pose->impulse.y=PX_LiveEditorRealtimePoseAccessorDecode(values[valueIndex++],runtimeAxis->coordFractionBits);
			}
			for (j=0;j<sourceBinding->vertexCount;j++)
			{
				px_uint16 localVertex=runtimeAxis->vertexIndices[sourceBinding->vertexIndexOffset+j];
				px_point delta=PX_POINT(0,0,0);
				if (localVertex>=axis->bindings[targetBinding].vertexCount) return PX_FALSE;
				if (sourceBinding->propertyMask&PX_LIVE_REALTIME_PROPERTY_VERTICES)
				{
					delta.x=PX_LiveEditorRealtimePoseAccessorDecode(values[valueIndex++],runtimeAxis->coordFractionBits);
					delta.y=PX_LiveEditorRealtimePoseAccessorDecode(values[valueIndex++],runtimeAxis->coordFractionBits);
				}
				axis->keyVertexDeltas[key][targetVertexOffset+localVertex]=delta;
			}
		}
	}
	axis->authoringAvailable=PX_TRUE;
	return PX_TRUE;
}

static px_void PX_LiveEditorRealtimePoseAccessorReloadInternal(PX_LiveEditorRealtimePoseAccessor *accessor,px_bool restoreAuthoring)
{
	px_int i,count;
	if (!accessor || !accessor->plive) return;
	for (i=0;i<accessor->axisCount;i++) PX_LiveEditorRealtimePoseAccessorFreeAxis(accessor,&accessor->axes[i]);
	accessor->axisCount=0;
	accessor->selectedAxis=-1;
	accessor->selectedLayer=-1;
	accessor->entered=PX_FALSE;
	accessor->authoringPoseActive=PX_FALSE;
	accessor->blendPreviewActive=PX_FALSE;
	count=PX_LiveRealtimeGetAxisCount(accessor->plive);
	if (count>PX_LIVE_REALTIME_MAX_AXES) count=PX_LIVE_REALTIME_MAX_AXES;
	for (i=0;i<count;i++)
	{
		const PX_LiveRealtimeAxis *runtimeAxis=PX_LiveRealtimeGetAxisConst(accessor->plive,i);
		PX_LiveEditorRealtimeAxisAuthoring *axis=&accessor->axes[accessor->axisCount++];
		PX_memset(axis,0,sizeof(*axis));
		axis->runtimeHandle=i;
		if (runtimeAxis)
		{
			PX_strcpy(axis->id,runtimeAxis->id,sizeof(axis->id));
			axis->middleKeyIndex=runtimeAxis->middleKeyIndex;
			axis->defaultSampleIndex=runtimeAxis->defaultSampleIndex;
			axis->sampleIndex=runtimeAxis->sampleIndex;
			axis->weightQ15=runtimeAxis->weightQ15;
			axis->selectedKeySlot=(px_uchar)PX_LiveEditorRealtimePoseAccessorDefaultSlot(axis);
			axis->bindingCount=runtimeAxis->bindingCount;
			axis->vertexIndexCount=runtimeAxis->vertexIndexCount;
			axis->baked=PX_TRUE;
			/* A standalone blend preview only reads baked runtime samples.  Dense
			   three-key authoring buffers can be many times larger than the sparse
			   axis data and must not compete with preview UI allocations. */
			if (restoreAuthoring && !PX_LiveEditorRealtimePoseAccessorRestoreAuthoring(accessor,axis,runtimeAxis)) axis->authoringAvailable=PX_FALSE;
		}
	}
	if (accessor->axisCount) accessor->selectedAxis=0;
	if (accessor->plive->layers.size)
	{
		px_int editLayer=accessor->plive->currentEditLayerIndex;
		accessor->selectedLayer=(editLayer>=0&&editLayer<accessor->plive->layers.size)?editLayer:0;
	}
	accessor->revision++;
}

static px_bool PX_LiveEditorRealtimePoseAccessorInitializeInternal(PX_LiveEditorRealtimePoseAccessor *accessor,px_memorypool *mp,PX_LiveFramework *plive,px_bool restoreAuthoring)
{
	if (!accessor || !mp || !plive) return PX_FALSE;
	PX_memset(accessor,0,sizeof(*accessor));
	accessor->mp=mp;
	accessor->plive=plive;
	accessor->selectedAxis=-1;
	accessor->selectedLayer=-1;
	PX_LiveEditorRealtimePoseAccessorReloadInternal(accessor,restoreAuthoring);
	return PX_TRUE;
}

px_bool PX_LiveEditorRealtimePoseAccessorInitialize(PX_LiveEditorRealtimePoseAccessor *accessor,px_memorypool *mp,PX_LiveFramework *plive)
{
	return PX_LiveEditorRealtimePoseAccessorInitializeInternal(accessor,mp,plive,PX_TRUE);
}

px_bool PX_LiveEditorRealtimePoseAccessorInitializePreview(PX_LiveEditorRealtimePoseAccessor *accessor,px_memorypool *mp,PX_LiveFramework *plive)
{
	return PX_LiveEditorRealtimePoseAccessorInitializeInternal(accessor,mp,plive,PX_FALSE);
}

px_void PX_LiveEditorRealtimePoseAccessorFree(PX_LiveEditorRealtimePoseAccessor *accessor)
{
	px_int i;
	if (!accessor) return;
	for (i=0;i<accessor->axisCount;i++) PX_LiveEditorRealtimePoseAccessorFreeAxis(accessor,&accessor->axes[i]);
	PX_memset(accessor,0,sizeof(*accessor));
	accessor->selectedAxis=-1;
	accessor->selectedLayer=-1;
}

px_void PX_LiveEditorRealtimePoseAccessorReload(PX_LiveEditorRealtimePoseAccessor *accessor)
{
	PX_LiveEditorRealtimePoseAccessorReloadInternal(accessor,PX_TRUE);
}

px_int PX_LiveEditorRealtimePoseAccessorGetAxisCount(const PX_LiveEditorRealtimePoseAccessor *accessor)
{
	return accessor?accessor->axisCount:0;
}

PX_LiveEditorRealtimeAxisAuthoring *PX_LiveEditorRealtimePoseAccessorGetAxis(PX_LiveEditorRealtimePoseAccessor *accessor,px_int axisIndex)
{
	if (!accessor || axisIndex<0 || axisIndex>=accessor->axisCount) return PX_NULL;
	return &accessor->axes[axisIndex];
}

const PX_LiveEditorRealtimeAxisAuthoring *PX_LiveEditorRealtimePoseAccessorGetAxisConst(const PX_LiveEditorRealtimePoseAccessor *accessor,px_int axisIndex)
{
	if (!accessor || axisIndex<0 || axisIndex>=accessor->axisCount) return PX_NULL;
	return &accessor->axes[axisIndex];
}

px_int PX_LiveEditorRealtimePoseAccessorGetSelectedAxis(const PX_LiveEditorRealtimePoseAccessor *accessor)
{
	return accessor?accessor->selectedAxis:-1;
}

static px_void PX_LiveEditorRealtimePoseAccessorResetKeyToNeutral(PX_LiveEditorRealtimePoseAccessor *accessor,PX_LiveEditorRealtimeAxisAuthoring *axis,px_int keySlot)
{
	px_int i;
	if (!axis || !axis->authoringAvailable || keySlot<0 || keySlot>2) return;
	for (i=0;i<axis->bindingCount;i++)
	{
		PX_LiveRealtimeBindingPose *pose=&axis->keyBindingPoses[keySlot][i];
		pose->translation=PX_POINT(0,0,0);
		pose->rotation=0;
		pose->stretch=1;
		pose->localTranslation=PX_POINT(0,0,0);
		pose->localRotation=0;
		pose->localScale=1;
		pose->mapTexture=PX_LiveFrameworkGetLayer(accessor->plive,axis->bindings[i].layerIndex)->LinkTextureIndex;
		pose->impulse=PX_POINT(0,0,0);
	}
	if (axis->vertexIndexCount)
	{
		PX_memset(axis->keyVertexDeltas[keySlot],0,sizeof(px_point)*axis->vertexIndexCount);
	}
}

static px_bool PX_LiveEditorRealtimePoseAccessorIsKeyNeutral(PX_LiveEditorRealtimePoseAccessor *accessor,const PX_LiveEditorRealtimeAxisAuthoring *axis,px_int keySlot)
{
	px_int i;
	if (!axis || !axis->authoringAvailable || keySlot<0 || keySlot>2 || !axis->keyBindingPoses[keySlot]) return PX_FALSE;
	for (i=0;i<axis->bindingCount;i++)
	{
		const PX_LiveRealtimeBindingPose *pose=&axis->keyBindingPoses[keySlot][i];
		PX_LiveLayer *layer=PX_LiveFrameworkGetLayer(accessor->plive,axis->bindings[i].layerIndex);
		if (!layer) return PX_FALSE;
		if (PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->translation.x,0)||
			PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->translation.y,0)||
			PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->rotation,0)||
			PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->stretch,1)||
			PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->localTranslation.x,0)||
			PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->localTranslation.y,0)||
			PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->localRotation,0)||
			PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->localScale,1)||
			pose->mapTexture!=layer->LinkTextureIndex||
			PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->impulse.x,0)||
			PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->impulse.y,0)) return PX_FALSE;
	}
	for (i=0;i<(px_int)axis->vertexIndexCount;i++)
	{
		px_point delta=axis->keyVertexDeltas[keySlot][i];
		if (PX_LiveEditorRealtimePoseAccessorIsDifferent(delta.x,0)||PX_LiveEditorRealtimePoseAccessorIsDifferent(delta.y,0)) return PX_FALSE;
	}
	return PX_TRUE;
}

static px_bool PX_LiveEditorRealtimePoseAccessorValidateKeyTopology(PX_LiveEditorRealtimePoseAccessor *accessor,PX_LiveEditorRealtimeAxisAuthoring *axis,px_int keySlot)
{
	px_int i;
	px_uint32 vertexOffset=0;
	if (!accessor || !axis || !axis->bindings || keySlot<0 || keySlot>2 || !axis->keyBindingPoses[keySlot]) return PX_FALSE;
	if (axis->vertexIndexCount && !axis->keyVertexDeltas[keySlot]) return PX_FALSE;
	for (i=0;i<axis->bindingCount;i++)
	{
		PX_LiveRealtimeBindingDesc *binding=&axis->bindings[i];
		PX_LiveLayer *layer=PX_LiveFrameworkGetLayer(accessor->plive,binding->layerIndex);
		px_int j;
		if (!layer || (binding->vertexCount&&!binding->vertexIndices)) return PX_FALSE;
		if (vertexOffset+binding->vertexCount<vertexOffset || vertexOffset+binding->vertexCount>axis->vertexIndexCount) return PX_FALSE;
		for (j=0;j<binding->vertexCount;j++)
		{
			px_int vertexIndex=binding->vertexIndices[j];
			if (vertexIndex<0 || vertexIndex>=layer->vertices.size) return PX_FALSE;
		}
		vertexOffset+=binding->vertexCount;
	}
	return vertexOffset==axis->vertexIndexCount;
}

px_bool PX_LiveEditorRealtimePoseAccessorApplySelectedKey(PX_LiveEditorRealtimePoseAccessor *accessor)
{
	PX_LiveEditorRealtimeAxisAuthoring *axis;
	px_int i,keySlot;
	px_uint32 vertexOffset=0;
	px_bool wasAuthoring;
	if (!accessor || !accessor->plive) return PX_FALSE;
	wasAuthoring=accessor->authoringPoseActive;
	axis=PX_LiveEditorRealtimePoseAccessorGetAxis(accessor,accessor->selectedAxis);
	if (!axis || !axis->authoringAvailable) return PX_FALSE;
	keySlot=axis->selectedKeySlot;
	/* Validate the whole dense pose before resetting the framework.  This
	   prevents an invalid topology from leaving a half-applied hierarchy. */
	if (!PX_LiveEditorRealtimePoseAccessorValidateKeyTopology(accessor,axis,keySlot)) return PX_FALSE;
	if (!PX_LiveRealtimeEnter(accessor->plive)) return PX_FALSE;
	accessor->entered=PX_TRUE;
	for (i=0;i<axis->bindingCount;i++)
	{
		PX_LiveRealtimeBindingDesc *binding=&axis->bindings[i];
		PX_LiveRealtimeBindingPose *pose=&axis->keyBindingPoses[keySlot][i];
		PX_LiveLayer *layer=PX_LiveFrameworkGetLayer(accessor->plive,binding->layerIndex);
		px_int j;
		if (!layer) return PX_FALSE;
		layer->rel_currentTranslation=pose->translation;
		layer->rel_beginTranslation=pose->translation;
		layer->rel_endTranslation=pose->translation;
		layer->rel_currentRotationAngle=pose->rotation;
		layer->rel_beginRotationAngle=pose->rotation;
		layer->rel_endRotationAngle=pose->rotation;
		layer->rel_currentStretch=pose->stretch;
		layer->rel_beginStretch=pose->stretch;
		layer->rel_endStretch=pose->stretch;
		layer->rel_currentLocalTranslation=pose->localTranslation;
		layer->rel_beginLocalTranslation=pose->localTranslation;
		layer->rel_endLocalTranslation=pose->localTranslation;
		layer->rel_currentLocalRotationAngle=pose->localRotation;
		layer->rel_beginLocalRotationAngle=pose->localRotation;
		layer->rel_endLocalRotationAngle=pose->localRotation;
		layer->rel_currentLocalScale=pose->localScale;
		layer->rel_beginLocalScale=pose->localScale;
		layer->rel_endLocalScale=pose->localScale;
		layer->RenderTextureIndex=pose->mapTexture;
		layer->rel_impulse=pose->impulse;
		for (j=0;j<binding->vertexCount;j++)
		{
			px_int vertexIndex=binding->vertexIndices[j];
			px_point delta=axis->keyVertexDeltas[keySlot][vertexOffset+j];
			PX_LiveVertex *vertex;
			if (vertexIndex<0 || vertexIndex>=layer->vertices.size) return PX_FALSE;
			vertex=PX_VECTORAT(PX_LiveVertex,&layer->vertices,vertexIndex);
			vertex->currentTranslation=delta;
			vertex->beginTranslation=delta;
			vertex->endTranslation=delta;
		}
		vertexOffset+=binding->vertexCount;
	}
	if (accessor->selectedLayer>=0 && accessor->selectedLayer<accessor->plive->layers.size)
	{
		accessor->plive->currentEditLayerIndex=accessor->selectedLayer;
		accessor->plive->showFocusLayer=PX_TRUE;
		accessor->plive->showKeypoint=PX_TRUE;
	}
	PX_LiveFrameworkUpdate(accessor->plive,0);
	accessor->authoringPoseActive=PX_TRUE;
	accessor->blendPreviewActive=PX_FALSE;
	if (!wasAuthoring) accessor->revision++;
	return PX_TRUE;
}

px_bool PX_LiveEditorRealtimePoseAccessorSelectAxis(PX_LiveEditorRealtimePoseAccessor *accessor,px_int axisIndex)
{
	px_int oldAxis;
	if (!accessor || axisIndex<0 || axisIndex>=accessor->axisCount) return PX_FALSE;
	if (accessor->selectedAxis!=axisIndex)
	{
		oldAxis=accessor->selectedAxis;
		accessor->selectedAxis=axisIndex;
		if (!PX_LiveEditorRealtimePoseAccessorApplySelectedKey(accessor))
		{
			accessor->selectedAxis=oldAxis;
			if (oldAxis>=0) PX_LiveEditorRealtimePoseAccessorApplySelectedKey(accessor);
			accessor->revision++;
			return PX_FALSE;
		}
		accessor->revision++;
	}
	return PX_TRUE;
}

px_int PX_LiveEditorRealtimePoseAccessorGetSelectedLayer(const PX_LiveEditorRealtimePoseAccessor *accessor)
{
	return accessor?accessor->selectedLayer:-1;
}

px_bool PX_LiveEditorRealtimePoseAccessorSelectLayer(PX_LiveEditorRealtimePoseAccessor *accessor,px_int layerIndex)
{
	px_int oldLayer;
	if (!accessor || !accessor->plive || layerIndex<0 || layerIndex>=accessor->plive->layers.size) return PX_FALSE;
	if (accessor->selectedLayer!=layerIndex)
	{
		oldLayer=accessor->selectedLayer;
		accessor->selectedLayer=layerIndex;
		accessor->plive->currentEditLayerIndex=layerIndex;
		accessor->plive->showFocusLayer=PX_TRUE;
		accessor->plive->showKeypoint=PX_TRUE;
		if (accessor->selectedAxis>=0 && accessor->authoringPoseActive && !PX_LiveEditorRealtimePoseAccessorApplySelectedKey(accessor))
		{
			accessor->selectedLayer=oldLayer;
			accessor->plive->currentEditLayerIndex=oldLayer;
			if (oldLayer>=0) PX_LiveEditorRealtimePoseAccessorApplySelectedKey(accessor);
			accessor->revision++;
			return PX_FALSE;
		}
		accessor->revision++;
	}
	return PX_TRUE;
}

px_bool PX_LiveEditorRealtimePoseAccessorGetSelectedLayerTransform(PX_LiveEditorRealtimePoseAccessor *accessor,px_point *translation,px_float *rotation,px_float *stretch)
{
	PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(accessor,accessor?accessor->selectedAxis:-1);
	px_int bindingIndex;
	PX_LiveRealtimeBindingPose *pose;
	if (!PX_LiveEditorRealtimePoseAccessorIsSelectedKeyEditable(accessor) || !axis || accessor->selectedLayer<0) return PX_FALSE;
	bindingIndex=PX_LiveEditorRealtimePoseAccessorFindBinding(axis,accessor->selectedLayer);
	if (bindingIndex<0) return PX_FALSE;
	pose=&axis->keyBindingPoses[axis->selectedKeySlot][bindingIndex];
	if (translation) *translation=pose->localTranslation;
	if (rotation) *rotation=pose->localRotation;
	if (stretch) *stretch=pose->localScale;
	return PX_TRUE;
}

static px_float PX_LiveEditorRealtimePoseAccessorClamp(px_float value,px_float minimum,px_float maximum)
{
	if (value<minimum) return minimum;
	if (value>maximum) return maximum;
	return value;
}

px_bool PX_LiveEditorRealtimePoseAccessorSetSelectedLayerTransform(PX_LiveEditorRealtimePoseAccessor *accessor,px_float translationX,px_float translationY,px_float rotation,px_float stretch)
{
	PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(accessor,accessor?accessor->selectedAxis:-1);
	PX_LiveRealtimeBindingPose *pose;
	px_int bindingIndex;
	px_bool changed;
	if (!PX_LiveEditorRealtimePoseAccessorIsSelectedKeyEditable(accessor) || !axis || accessor->selectedLayer<0) return PX_FALSE;
	bindingIndex=PX_LiveEditorRealtimePoseAccessorFindBinding(axis,accessor->selectedLayer);
	if (bindingIndex<0) return PX_FALSE;
	translationX=PX_LiveEditorRealtimePoseAccessorClamp(translationX,-2047.0f,2047.0f);
	translationY=PX_LiveEditorRealtimePoseAccessorClamp(translationY,-2047.0f,2047.0f);
	rotation=PX_LiveEditorRealtimePoseAccessorClamp(rotation,-360.0f,360.0f);
	stretch=PX_LiveEditorRealtimePoseAccessorClamp(stretch,0.05f,2.9f);
	pose=&axis->keyBindingPoses[axis->selectedKeySlot][bindingIndex];
	changed=PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->localTranslation.x,translationX)||
		PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->localTranslation.y,translationY)||
		PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->localRotation,rotation)||
		PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->localScale,stretch);
	if (!changed) return PX_TRUE;
	pose->localTranslation=PX_POINT(translationX,translationY,0);
	pose->localRotation=rotation;
	pose->localScale=stretch;
	axis->dirty=PX_TRUE;
	accessor->revision++;
	return PX_LiveEditorRealtimePoseAccessorApplySelectedKey(accessor);
}

px_bool PX_LiveEditorRealtimePoseAccessorResetSelectedLayerTransform(PX_LiveEditorRealtimePoseAccessor *accessor)
{
	return PX_LiveEditorRealtimePoseAccessorSetSelectedLayerTransform(accessor,0,0,0,1);
}

px_bool PX_LiveEditorRealtimePoseAccessorIsSelectedKeyEditable(PX_LiveEditorRealtimePoseAccessor *accessor)
{
	PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(accessor,accessor?accessor->selectedAxis:-1);
	return accessor&&accessor->authoringPoseActive&&axis&&axis->authoringAvailable&&axis->selectedKeySlot!=PX_LiveEditorRealtimePoseAccessorDefaultSlot(axis);
}

px_bool PX_LiveEditorRealtimePoseAccessorGetSelectedVertexDelta(PX_LiveEditorRealtimePoseAccessor *accessor,px_int vertexIndex,px_point *delta)
{
	PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(accessor,accessor?accessor->selectedAxis:-1);
	px_int bindingIndex,j;
	px_uint32 vertexOffset;
	if (!PX_LiveEditorRealtimePoseAccessorIsSelectedKeyEditable(accessor) || !axis || accessor->selectedLayer<0 || !delta) return PX_FALSE;
	bindingIndex=PX_LiveEditorRealtimePoseAccessorFindBinding(axis,accessor->selectedLayer);
	if (bindingIndex<0) return PX_FALSE;
	vertexOffset=PX_LiveEditorRealtimePoseAccessorGetVertexOffset(axis,bindingIndex);
	for (j=0;j<axis->bindings[bindingIndex].vertexCount;j++)
	{
		if (axis->bindings[bindingIndex].vertexIndices[j]==vertexIndex)
		{
			*delta=axis->keyVertexDeltas[axis->selectedKeySlot][vertexOffset+j];
			return PX_TRUE;
		}
	}
	return PX_FALSE;
}

px_bool PX_LiveEditorRealtimePoseAccessorSetSelectedVertexDelta(PX_LiveEditorRealtimePoseAccessor *accessor,px_int vertexIndex,px_point delta)
{
	PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(accessor,accessor?accessor->selectedAxis:-1);
	px_int bindingIndex,j;
	px_uint32 vertexOffset;
	if (!PX_LiveEditorRealtimePoseAccessorIsSelectedKeyEditable(accessor) || !axis || accessor->selectedLayer<0) return PX_FALSE;
	bindingIndex=PX_LiveEditorRealtimePoseAccessorFindBinding(axis,accessor->selectedLayer);
	if (bindingIndex<0) return PX_FALSE;
	vertexOffset=PX_LiveEditorRealtimePoseAccessorGetVertexOffset(axis,bindingIndex);
	delta.x=PX_LiveEditorRealtimePoseAccessorClamp(delta.x,-2047.0f,2047.0f);
	delta.y=PX_LiveEditorRealtimePoseAccessorClamp(delta.y,-2047.0f,2047.0f);
	delta.z=0;
	for (j=0;j<axis->bindings[bindingIndex].vertexCount;j++)
	{
		if (axis->bindings[bindingIndex].vertexIndices[j]==vertexIndex)
		{
			axis->keyVertexDeltas[axis->selectedKeySlot][vertexOffset+j]=delta;
			axis->dirty=PX_TRUE;
			accessor->revision++;
			return PX_LiveEditorRealtimePoseAccessorApplySelectedKey(accessor);
		}
	}
	return PX_FALSE;
}

static px_bool PX_LiveEditorRealtimePoseAccessorCanRotateBranch(PX_LiveEditorRealtimePoseAccessor *accessor,PX_LiveEditorRealtimeAxisAuthoring *axis,px_int layerIndex,px_float delta)
{
	PX_LiveLayer *layer=PX_LiveFrameworkGetLayer(accessor->plive,layerIndex);
	px_int bindingIndex,i;
	if (!layer) return PX_FALSE;
	bindingIndex=PX_LiveEditorRealtimePoseAccessorFindBinding(axis,layerIndex);
	if (bindingIndex<0) return PX_FALSE;
	if (axis->keyBindingPoses[axis->selectedKeySlot][bindingIndex].rotation+delta<-360.0f||
		axis->keyBindingPoses[axis->selectedKeySlot][bindingIndex].rotation+delta>360.0f) return PX_FALSE;
	for (i=0;i<PX_COUNTOF(layer->child_index)&&layer->child_index[i]!=-1;i++)
	{
		if (!PX_LiveEditorRealtimePoseAccessorCanRotateBranch(accessor,axis,layer->child_index[i],delta)) return PX_FALSE;
	}
	return PX_TRUE;
}

static px_void PX_LiveEditorRealtimePoseAccessorRotateBranch(PX_LiveEditorRealtimePoseAccessor *accessor,PX_LiveEditorRealtimeAxisAuthoring *axis,px_int layerIndex,px_float delta)
{
	PX_LiveLayer *layer=PX_LiveFrameworkGetLayer(accessor->plive,layerIndex);
	px_int bindingIndex=PX_LiveEditorRealtimePoseAccessorFindBinding(axis,layerIndex),i;
	axis->keyBindingPoses[axis->selectedKeySlot][bindingIndex].rotation+=delta;
	for (i=0;i<PX_COUNTOF(layer->child_index)&&layer->child_index[i]!=-1;i++)
	{
		PX_LiveEditorRealtimePoseAccessorRotateBranch(accessor,axis,layer->child_index[i],delta);
	}
}

px_bool PX_LiveEditorRealtimePoseAccessorGetSelectedSubtreeRotation(PX_LiveEditorRealtimePoseAccessor *accessor,px_float *rotation)
{
	PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(accessor,accessor?accessor->selectedAxis:-1);
	px_int bindingIndex;
	if (!PX_LiveEditorRealtimePoseAccessorIsSelectedKeyEditable(accessor)||!axis||!rotation||accessor->selectedLayer<0) return PX_FALSE;
	bindingIndex=PX_LiveEditorRealtimePoseAccessorFindBinding(axis,accessor->selectedLayer);
	if (bindingIndex<0) return PX_FALSE;
	*rotation=axis->keyBindingPoses[axis->selectedKeySlot][bindingIndex].rotation;
	return PX_TRUE;
}

px_bool PX_LiveEditorRealtimePoseAccessorSetSelectedSubtreeRotation(PX_LiveEditorRealtimePoseAccessor *accessor,px_float rotation)
{
	PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(accessor,accessor?accessor->selectedAxis:-1);
	px_float current,delta;
	if (!PX_LiveEditorRealtimePoseAccessorIsSelectedKeyEditable(accessor)||!axis||accessor->selectedLayer<0) return PX_FALSE;
	rotation=PX_LiveEditorRealtimePoseAccessorClamp(rotation,-360.0f,360.0f);
	if (!PX_LiveEditorRealtimePoseAccessorGetSelectedSubtreeRotation(accessor,&current)) return PX_FALSE;
	delta=rotation-current;
	if (!PX_LiveEditorRealtimePoseAccessorIsDifferent(delta,0)) return PX_TRUE;
	if (!PX_LiveEditorRealtimePoseAccessorCanRotateBranch(accessor,axis,accessor->selectedLayer,delta)) return PX_FALSE;
	PX_LiveEditorRealtimePoseAccessorRotateBranch(accessor,axis,accessor->selectedLayer,delta);
	axis->dirty=PX_TRUE;
	accessor->revision++;
	return PX_LiveEditorRealtimePoseAccessorApplySelectedKey(accessor);
}

px_bool PX_LiveEditorRealtimePoseAccessorGetSelectedLayerTexture(PX_LiveEditorRealtimePoseAccessor *accessor,px_int *textureIndex)
{
	PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(accessor,accessor?accessor->selectedAxis:-1);
	px_int bindingIndex;
	if (!PX_LiveEditorRealtimePoseAccessorIsSelectedKeyEditable(accessor)||!axis||!textureIndex||accessor->selectedLayer<0) return PX_FALSE;
	bindingIndex=PX_LiveEditorRealtimePoseAccessorFindBinding(axis,accessor->selectedLayer);
	if (bindingIndex<0) return PX_FALSE;
	*textureIndex=axis->keyBindingPoses[axis->selectedKeySlot][bindingIndex].mapTexture;
	return PX_TRUE;
}

px_bool PX_LiveEditorRealtimePoseAccessorSetSelectedLayerTexture(PX_LiveEditorRealtimePoseAccessor *accessor,px_int textureIndex)
{
	PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(accessor,accessor?accessor->selectedAxis:-1);
	px_int bindingIndex;
	PX_LiveRealtimeBindingPose *pose;
	if (!PX_LiveEditorRealtimePoseAccessorIsSelectedKeyEditable(accessor)||!axis||accessor->selectedLayer<0) return PX_FALSE;
	if (textureIndex<-1||textureIndex>=accessor->plive->livetextures.size) return PX_FALSE;
	bindingIndex=PX_LiveEditorRealtimePoseAccessorFindBinding(axis,accessor->selectedLayer);
	if (bindingIndex<0) return PX_FALSE;
	pose=&axis->keyBindingPoses[axis->selectedKeySlot][bindingIndex];
	if (pose->mapTexture==textureIndex) return PX_TRUE;
	pose->mapTexture=textureIndex;
	axis->dirty=PX_TRUE;
	accessor->revision++;
	return PX_LiveEditorRealtimePoseAccessorApplySelectedKey(accessor);
}

px_bool PX_LiveEditorRealtimePoseAccessorGetSelectedLayerImpulse(PX_LiveEditorRealtimePoseAccessor *accessor,px_point *impulse)
{
	PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(accessor,accessor?accessor->selectedAxis:-1);
	px_int bindingIndex;
	if (!PX_LiveEditorRealtimePoseAccessorIsSelectedKeyEditable(accessor)||!axis||!impulse||accessor->selectedLayer<0) return PX_FALSE;
	bindingIndex=PX_LiveEditorRealtimePoseAccessorFindBinding(axis,accessor->selectedLayer);
	if (bindingIndex<0) return PX_FALSE;
	*impulse=axis->keyBindingPoses[axis->selectedKeySlot][bindingIndex].impulse;
	return PX_TRUE;
}

px_bool PX_LiveEditorRealtimePoseAccessorSetSelectedLayerImpulse(PX_LiveEditorRealtimePoseAccessor *accessor,px_point impulse)
{
	PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(accessor,accessor?accessor->selectedAxis:-1);
	px_int bindingIndex;
	PX_LiveRealtimeBindingPose *pose;
	if (!PX_LiveEditorRealtimePoseAccessorIsSelectedKeyEditable(accessor)||!axis||accessor->selectedLayer<0) return PX_FALSE;
	bindingIndex=PX_LiveEditorRealtimePoseAccessorFindBinding(axis,accessor->selectedLayer);
	if (bindingIndex<0) return PX_FALSE;
	impulse.x=PX_LiveEditorRealtimePoseAccessorClamp(impulse.x,-2047.0f,2047.0f);
	impulse.y=PX_LiveEditorRealtimePoseAccessorClamp(impulse.y,-2047.0f,2047.0f);
	impulse.z=0;
	pose=&axis->keyBindingPoses[axis->selectedKeySlot][bindingIndex];
	if (!PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->impulse.x,impulse.x)&&!PX_LiveEditorRealtimePoseAccessorIsDifferent(pose->impulse.y,impulse.y)) return PX_TRUE;
	pose->impulse=impulse;
	axis->dirty=PX_TRUE;
	accessor->revision++;
	return PX_LiveEditorRealtimePoseAccessorApplySelectedKey(accessor);
}

px_bool PX_LiveEditorRealtimePoseAccessorCommitChanges(PX_LiveEditorRealtimePoseAccessor *accessor)
{
	PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(accessor,accessor?accessor->selectedAxis:-1);
	if (!PX_LiveEditorRealtimePoseAccessorIsSelectedKeyEditable(accessor)||!axis) return PX_FALSE;
	axis->dirty=PX_TRUE;
	accessor->revision++;
	return PX_LiveEditorRealtimePoseAccessorApplySelectedKey(accessor);
}

px_bool PX_LiveEditorRealtimePoseAccessorCreateAxis(PX_LiveEditorRealtimePoseAccessor *accessor,const px_char id[])
{
	PX_LiveEditorRealtimeAxisAuthoring *axis;
	px_int i;
	if (!accessor || !id || !id[0] || PX_strlen(id)>=PX_LIVE_REALTIME_AXIS_ID_MAX_LEN || accessor->axisCount>=PX_LIVE_REALTIME_MAX_AXES) return PX_FALSE;
	if (PX_LiveRealtimeFindAxisById(accessor->plive,id)!=PX_LIVE_REALTIME_INVALID_HANDLE) return PX_FALSE;
	for (i=0;i<accessor->axisCount;i++) if (PX_strequ(accessor->axes[i].id,id)) return PX_FALSE;
	axis=&accessor->axes[accessor->axisCount];
	PX_memset(axis,0,sizeof(*axis));
	axis->runtimeHandle=PX_LIVE_REALTIME_INVALID_HANDLE;
	PX_strcpy(axis->id,id,sizeof(axis->id));
	axis->middleKeyIndex=15;
	axis->defaultSampleIndex=15;
	axis->selectedKeySlot=1;
	axis->sampleIndex=15;
	axis->weightQ15=PX_LIVE_REALTIME_WEIGHT_ONE_Q15;
	if (!PX_LiveEditorRealtimePoseAccessorAllocateAuthoring(accessor,axis))
	{
		PX_LiveEditorRealtimePoseAccessorFreeAxis(accessor,axis);
		return PX_FALSE;
	}
	accessor->selectedAxis=accessor->axisCount;
	accessor->axisCount++;
	axis->dirty=PX_TRUE;
	axis->baked=PX_FALSE;
	accessor->revision++;
	PX_LiveEditorRealtimePoseAccessorApplySelectedKey(accessor);
	return PX_TRUE;
}

px_bool PX_LiveEditorRealtimePoseAccessorDeleteSelectedAxis(PX_LiveEditorRealtimePoseAccessor *accessor)
{
	px_int index,deletedHandle,i;
	if (!accessor || accessor->selectedAxis<0 || accessor->selectedAxis>=accessor->axisCount) return PX_FALSE;
	index=accessor->selectedAxis;
	deletedHandle=accessor->axes[index].runtimeHandle;
	if (deletedHandle!=PX_LIVE_REALTIME_INVALID_HANDLE && !PX_LiveRealtimeDeleteAxis(accessor->plive,deletedHandle)) return PX_FALSE;
	PX_LiveEditorRealtimePoseAccessorFreeAxis(accessor,&accessor->axes[index]);
	for (i=index;i<accessor->axisCount-1;i++) accessor->axes[i]=accessor->axes[i+1];
	PX_memset(&accessor->axes[accessor->axisCount-1],0,sizeof(accessor->axes[0]));
	accessor->axisCount--;
	if (deletedHandle!=PX_LIVE_REALTIME_INVALID_HANDLE)
	{
		for (i=0;i<accessor->axisCount;i++) if (accessor->axes[i].runtimeHandle>deletedHandle) accessor->axes[i].runtimeHandle--;
	}
	if (!accessor->axisCount) accessor->selectedAxis=-1;
	else if (index>=accessor->axisCount) accessor->selectedAxis=accessor->axisCount-1;
	accessor->revision++;
	if (accessor->selectedAxis>=0) PX_LiveEditorRealtimePoseAccessorApplySelectedKey(accessor);
	else
	{
		PX_LiveRealtimeEnter(accessor->plive);
		accessor->entered=PX_TRUE;
		accessor->authoringPoseActive=PX_FALSE;
		PX_LiveFrameworkUpdate(accessor->plive,0);
	}
	return PX_TRUE;
}

px_bool PX_LiveEditorRealtimePoseAccessorSetMiddle(PX_LiveEditorRealtimePoseAccessor *accessor,px_int middleKeyIndex)
{
	PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(accessor,accessor?accessor->selectedAxis:-1);
	px_uchar oldMiddle;
	if (!axis) return PX_FALSE;
	if (middleKeyIndex<1) middleKeyIndex=1;
	if (middleKeyIndex>28) middleKeyIndex=28;
	oldMiddle=axis->middleKeyIndex;
	if (oldMiddle==(px_uchar)middleKeyIndex) return PX_TRUE;
	axis->middleKeyIndex=(px_uchar)middleKeyIndex;
	if (axis->defaultSampleIndex==oldMiddle) axis->defaultSampleIndex=(px_uchar)middleKeyIndex;
	if (axis->sampleIndex==oldMiddle) axis->sampleIndex=(px_uchar)middleKeyIndex;
	axis->dirty=PX_TRUE;
	accessor->revision++;
	return PX_TRUE;
}

px_bool PX_LiveEditorRealtimePoseAccessorSetDefaultKeySlot(PX_LiveEditorRealtimePoseAccessor *accessor,px_int keySlot)
{
	PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(accessor,accessor?accessor->selectedAxis:-1);
	px_uchar sample;
	if (!axis || keySlot<0 || keySlot>2) return PX_FALSE;
	sample=PX_LiveEditorRealtimePoseAccessorSlotSample(axis,keySlot);
	if (axis->defaultSampleIndex!=sample)
	{
		/* Changing the reference after authoring would silently rebase every
		   sample.  Require the target key to be neutral instead. */
		if (!PX_LiveEditorRealtimePoseAccessorIsKeyNeutral(accessor,axis,keySlot)) return PX_FALSE;
		axis->defaultSampleIndex=sample;
		/* The serialized RT30 chunk stores deltas from the default sample, so
		   its authoring key must remain the neutral model pose. */
		PX_LiveEditorRealtimePoseAccessorResetKeyToNeutral(accessor,axis,keySlot);
		axis->dirty=PX_TRUE;
		accessor->revision++;
		PX_LiveEditorRealtimePoseAccessorApplySelectedKey(accessor);
	}
	return PX_TRUE;
}

px_bool PX_LiveEditorRealtimePoseAccessorSelectKeySlot(PX_LiveEditorRealtimePoseAccessor *accessor,px_int keySlot)
{
	PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(accessor,accessor?accessor->selectedAxis:-1);
	px_uchar oldKeySlot;
	if (!axis || keySlot<0 || keySlot>2) return PX_FALSE;
	if (axis->selectedKeySlot!=(px_uchar)keySlot)
	{
		oldKeySlot=axis->selectedKeySlot;
		axis->selectedKeySlot=(px_uchar)keySlot;
		if (!PX_LiveEditorRealtimePoseAccessorApplySelectedKey(accessor))
		{
			axis->selectedKeySlot=oldKeySlot;
			PX_LiveEditorRealtimePoseAccessorApplySelectedKey(accessor);
			accessor->revision++;
			return PX_FALSE;
		}
		accessor->revision++;
	}
	return PX_TRUE;
}

px_bool PX_LiveEditorRealtimePoseAccessorCaptureSelectedKey(PX_LiveEditorRealtimePoseAccessor *accessor)
{
	PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(accessor,accessor?accessor->selectedAxis:-1);
	PX_LiveRealtimeBindingPose *pose;
	PX_LiveLayer *layer;
	px_int bindingIndex,j;
	px_uint32 vertexOffset;
	if (!PX_LiveEditorRealtimePoseAccessorIsSelectedKeyEditable(accessor) || !axis || accessor->selectedLayer<0) return PX_FALSE;
	bindingIndex=PX_LiveEditorRealtimePoseAccessorFindBinding(axis,accessor->selectedLayer);
	layer=PX_LiveFrameworkGetLayer(accessor->plive,accessor->selectedLayer);
	if (bindingIndex<0 || !layer) return PX_FALSE;
	pose=&axis->keyBindingPoses[axis->selectedKeySlot][bindingIndex];
	pose->translation=layer->rel_currentTranslation;
	pose->rotation=layer->rel_currentRotationAngle;
	pose->stretch=layer->rel_currentStretch;
	pose->localTranslation=layer->rel_currentLocalTranslation;
	pose->localRotation=layer->rel_currentLocalRotationAngle;
	pose->localScale=layer->rel_currentLocalScale;
	pose->mapTexture=layer->RenderTextureIndex;
	pose->impulse=layer->rel_impulse;
	vertexOffset=PX_LiveEditorRealtimePoseAccessorGetVertexOffset(axis,bindingIndex);
	for (j=0;j<axis->bindings[bindingIndex].vertexCount;j++)
	{
		px_int vertexIndex=axis->bindings[bindingIndex].vertexIndices[j];
		PX_LiveVertex *vertex;
		if (vertexIndex<0 || vertexIndex>=layer->vertices.size) return PX_FALSE;
		vertex=PX_VECTORAT(PX_LiveVertex,&layer->vertices,vertexIndex);
		axis->keyVertexDeltas[axis->selectedKeySlot][vertexOffset+j]=vertex->currentTranslation;
	}
	axis->dirty=PX_TRUE;
	accessor->revision++;
	return PX_TRUE;
}

px_bool PX_LiveEditorRealtimePoseAccessorCopyDefaultToSelectedKey(PX_LiveEditorRealtimePoseAccessor *accessor)
{
	PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(accessor,accessor?accessor->selectedAxis:-1);
	px_int src,dst,i;
	px_uint32 vertexOffset=0;
	if (!PX_LiveEditorRealtimePoseAccessorIsSelectedKeyEditable(accessor) || !axis) return PX_FALSE;
	src=PX_LiveEditorRealtimePoseAccessorDefaultSlot(axis);
	dst=axis->selectedKeySlot;
	for (i=0;i<axis->bindingCount;i++)
	{
		px_int j;
		axis->keyBindingPoses[dst][i].translation=axis->keyBindingPoses[src][i].translation;
		axis->keyBindingPoses[dst][i].rotation=axis->keyBindingPoses[src][i].rotation;
		axis->keyBindingPoses[dst][i].stretch=axis->keyBindingPoses[src][i].stretch;
		axis->keyBindingPoses[dst][i].localTranslation=axis->keyBindingPoses[src][i].localTranslation;
		axis->keyBindingPoses[dst][i].localRotation=axis->keyBindingPoses[src][i].localRotation;
		axis->keyBindingPoses[dst][i].localScale=axis->keyBindingPoses[src][i].localScale;
		axis->keyBindingPoses[dst][i].mapTexture=axis->keyBindingPoses[src][i].mapTexture;
		axis->keyBindingPoses[dst][i].impulse=axis->keyBindingPoses[src][i].impulse;
		for (j=0;j<axis->bindings[i].vertexCount;j++) axis->keyVertexDeltas[dst][vertexOffset+j]=axis->keyVertexDeltas[src][vertexOffset+j];
		vertexOffset+=axis->bindings[i].vertexCount;
	}
	axis->dirty=PX_TRUE;
	accessor->revision++;
	return PX_LiveEditorRealtimePoseAccessorApplySelectedKey(accessor);
}

px_bool PX_LiveEditorRealtimePoseAccessorBakeSelectedAxis(PX_LiveEditorRealtimePoseAccessor *accessor)
{
	PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(accessor,accessor?accessor->selectedAxis:-1);
	if (axis) PX_LiveEditorRealtimePoseAccessorResetKeyToNeutral(accessor,axis,PX_LiveEditorRealtimePoseAccessorDefaultSlot(axis));
	if (!axis || !PX_LiveEditorRealtimePoseAccessorBakeAxis(accessor,axis)) return PX_FALSE;
	PX_LiveEditorRealtimePoseAccessorScanBakedMesh(accessor,axis);
	accessor->revision++;
	return PX_TRUE;
}

static px_void PX_LiveEditorRealtimePoseAccessorAddDefects(PX_LiveMeshDefects *total,const PX_LiveMeshDefects *more)
{
	total->degenerate+=more->degenerate;
	total->flipped+=more->flipped;
	total->uvOutside+=more->uvOutside;
}

static px_void PX_LiveEditorRealtimePoseAccessorScanBakedMesh(PX_LiveEditorRealtimePoseAccessor *accessor,PX_LiveEditorRealtimeAxisAuthoring *axis)
{
	px_uchar savedSample;
	px_uint16 savedWeight;
	px_uchar samples[3];
	px_int i;
	PX_LiveMeshDefects total,one;
	if (!accessor||!axis||axis->runtimeHandle==PX_LIVE_REALTIME_INVALID_HANDLE)
	{
		return;
	}
	savedSample=axis->sampleIndex;
	savedWeight=axis->weightQ15;
	samples[0]=0;
	samples[1]=axis->middleKeyIndex;
	samples[2]=PX_LIVE_REALTIME_SAMPLE_COUNT-1;
	PX_memset(&total,0,sizeof(total));
	for (i=0;i<3;i++)
	{
		if (!PX_LiveEditorRealtimePoseAccessorApplySoloPreview(accessor,axis,samples[i],PX_LIVE_REALTIME_WEIGHT_ONE_Q15))
		{
			continue;
		}
		PX_LiveFrameworkUpdate(accessor->plive,0);
		PX_LiveFrameworkCountMeshDefects(accessor->plive,&one);
		PX_LiveEditorRealtimePoseAccessorAddDefects(&total,&one);
	}
	for (i=0;i<accessor->axisCount;i++)
	{
		PX_LiveEditorRealtimeAxisAuthoring *other=&accessor->axes[i];
		px_uchar sampleIndices[PX_LIVE_REALTIME_MAX_AXES];
		px_uint16 weights[PX_LIVE_REALTIME_MAX_AXES];
		px_int axisIndex,selectedIndex;
		if (other==axis||!other->baked||other->dirty||other->runtimeHandle==PX_LIVE_REALTIME_INVALID_HANDLE)
		{
			continue;
		}
		selectedIndex=-1;
		for (axisIndex=0;axisIndex<accessor->axisCount;axisIndex++)
		{
			sampleIndices[axisIndex]=accessor->axes[axisIndex].defaultSampleIndex;
			weights[axisIndex]=0;
			if (&accessor->axes[axisIndex]==axis)
			{
				selectedIndex=axisIndex;
			}
		}
		if (selectedIndex<0)
		{
			break;
		}
		sampleIndices[selectedIndex]=0;
		weights[selectedIndex]=PX_LIVE_REALTIME_WEIGHT_ONE_Q15;
		sampleIndices[i]=PX_LIVE_REALTIME_SAMPLE_COUNT-1;
		weights[i]=PX_LIVE_REALTIME_WEIGHT_ONE_Q15;
		if (PX_LiveEditorRealtimePoseAccessorApplyBlendPreview(accessor,sampleIndices,weights,accessor->axisCount))
		{
			PX_LiveFrameworkUpdate(accessor->plive,0);
			PX_LiveFrameworkCountMeshDefects(accessor->plive,&one);
			PX_LiveEditorRealtimePoseAccessorAddDefects(&total,&one);
		}
		break;
	}
	PX_LiveEditorRealtimePoseAccessorApplySoloPreview(accessor,axis,savedSample,savedWeight);
	axis->meshDegenerate=total.degenerate;
	axis->meshFlipped=total.flipped;
	axis->meshUvOutside=total.uvOutside;
}

px_bool PX_LiveEditorRealtimePoseAccessorEnter(PX_LiveEditorRealtimePoseAccessor *accessor)
{
	px_int i;
	px_bool result;
	if (!accessor || !PX_LiveRealtimeEnter(accessor->plive)) return PX_FALSE;
	accessor->entered=PX_TRUE;
	for (i=0;i<accessor->axisCount;i++)
	{
		PX_LiveEditorRealtimeAxisAuthoring *axis=&accessor->axes[i];
		if (axis->runtimeHandle!=PX_LIVE_REALTIME_INVALID_HANDLE) PX_LiveRealtimeSetAxisSample(accessor->plive,axis->runtimeHandle,axis->sampleIndex,axis->weightQ15);
	}
	result=PX_LiveRealtimeUpdate(accessor->plive);
	if (result)
	{
		accessor->authoringPoseActive=PX_FALSE;
		accessor->blendPreviewActive=PX_FALSE;
	}
	return result;
}

px_void PX_LiveEditorRealtimePoseAccessorLeave(PX_LiveEditorRealtimePoseAccessor *accessor)
{
	if (accessor)
	{
		PX_LiveRealtimeLeave(accessor->plive);
		accessor->entered=PX_FALSE;
		accessor->authoringPoseActive=PX_FALSE;
		accessor->blendPreviewActive=PX_FALSE;
	}
}

px_void PX_LiveEditorEvaluationGuardSuspend(PX_LiveFramework *plive,PX_LiveEditorRealtimePoseAccessor *accessor,PX_LiveEditorEvaluationGuard *guard)
{
	if (!guard) return;
	PX_memset(guard,0,sizeof(*guard));
	if (!plive) return;
	guard->animationIndex=plive->currentEditAnimationIndex;
	guard->frameIndex=plive->currentEditFrameIndex;
	if (accessor && (accessor->entered || accessor->authoringPoseActive || accessor->blendPreviewActive))
	{
		guard->suspended=PX_TRUE;
		guard->resumeAuthoring=accessor->authoringPoseActive && accessor->selectedAxis>=0;
		PX_LiveEditorRealtimePoseAccessorLeave(accessor);
		return;
	}
	if (plive->animationMode!=PX_LIVE_MODE_NEUTRAL || plive->status==PX_LIVEFRAMEWORK_STATUS_PLAYING)
	{
		guard->suspended=PX_TRUE;
		guard->resumeFrame=plive->animationMode==PX_LIVE_MODE_TIMELINE && guard->frameIndex>=0;
		PX_LiveFrameworkStop(plive);
	}
}

px_void PX_LiveEditorEvaluationGuardResume(PX_LiveFramework *plive,PX_LiveEditorRealtimePoseAccessor *accessor,const PX_LiveEditorEvaluationGuard *guard)
{
	if (!plive || !guard || !guard->suspended) return;
	if (guard->resumeAuthoring && accessor)
	{
		PX_LiveEditorRealtimePoseAccessorApplySelectedKey(accessor);
	}
	else if (guard->resumeFrame)
	{
		plive->currentEditAnimationIndex=guard->animationIndex;
		plive->currentEditFrameIndex=guard->frameIndex;
		PX_LiveFrameworkRunCurrentEditFrame(plive);
	}
}

px_bool PX_LiveEditorRealtimePoseAccessorSetPreview(PX_LiveEditorRealtimePoseAccessor *accessor,px_int sampleIndex,px_uint16 weightQ15)
{
	PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(accessor,accessor?accessor->selectedAxis:-1);
	if (!axis || axis->runtimeHandle==PX_LIVE_REALTIME_INVALID_HANDLE) return PX_FALSE;
	if (sampleIndex<0) sampleIndex=0;
	if (sampleIndex>=PX_LIVE_REALTIME_SAMPLE_COUNT) sampleIndex=PX_LIVE_REALTIME_SAMPLE_COUNT-1;
	if (weightQ15>PX_LIVE_REALTIME_WEIGHT_ONE_Q15) weightQ15=PX_LIVE_REALTIME_WEIGHT_ONE_Q15;
	if (!PX_LiveEditorRealtimePoseAccessorApplySoloPreview(accessor,axis,(px_uchar)sampleIndex,weightQ15)) return PX_FALSE;
	axis->sampleIndex=(px_uchar)sampleIndex;
	axis->weightQ15=weightQ15;
	accessor->revision++;
	return PX_TRUE;
}

px_bool PX_LiveEditorRealtimePoseAccessorResetPreview(PX_LiveEditorRealtimePoseAccessor *accessor)
{
	PX_LiveEditorRealtimeAxisAuthoring *axis=PX_LiveEditorRealtimePoseAccessorGetAxis(accessor,accessor?accessor->selectedAxis:-1);
	if (!axis || axis->runtimeHandle==PX_LIVE_REALTIME_INVALID_HANDLE) return PX_FALSE;
	if (!PX_LiveEditorRealtimePoseAccessorApplySoloPreview(accessor,axis,axis->defaultSampleIndex,0)) return PX_FALSE;
	axis->sampleIndex=axis->defaultSampleIndex;
	axis->weightQ15=0;
	accessor->revision++;
	return PX_TRUE;
}

px_bool PX_LiveEditorRealtimePoseAccessorApplyBlendPreview(PX_LiveEditorRealtimePoseAccessor *accessor,const px_uchar sampleIndices[],const px_uint16 weightsQ15[],px_int axisCount)
{
	px_int i;
	px_bool wasBlendPreview;
	if (!accessor || !accessor->plive || !sampleIndices || !weightsQ15 || axisCount!=accessor->axisCount) return PX_FALSE;
	for (i=0;i<axisCount;i++)
	{
		PX_LiveEditorRealtimeAxisAuthoring *axis=&accessor->axes[i];
		if (sampleIndices[i]>=PX_LIVE_REALTIME_SAMPLE_COUNT || weightsQ15[i]>PX_LIVE_REALTIME_WEIGHT_ONE_Q15) return PX_FALSE;
		if (weightsQ15[i] && (axis->runtimeHandle==PX_LIVE_REALTIME_INVALID_HANDLE || !axis->baked || axis->dirty)) return PX_FALSE;
	}
	wasBlendPreview=accessor->blendPreviewActive;
	if (!accessor->entered || accessor->authoringPoseActive || accessor->plive->animationMode!=PX_LIVE_MODE_REALTIME30)
	{
		if (!PX_LiveRealtimeEnter(accessor->plive)) return PX_FALSE;
	}
	accessor->entered=PX_TRUE;
	PX_LiveRealtimeResetAll(accessor->plive);
	for (i=0;i<axisCount;i++)
	{
		PX_LiveEditorRealtimeAxisAuthoring *axis=&accessor->axes[i];
		if (weightsQ15[i] && !PX_LiveRealtimeSetAxisSample(accessor->plive,axis->runtimeHandle,sampleIndices[i],weightsQ15[i]))
		{
			PX_LiveRealtimeResetAll(accessor->plive);
			PX_LiveRealtimeUpdate(accessor->plive);
			return PX_FALSE;
		}
	}
	if (!PX_LiveRealtimeUpdate(accessor->plive)) return PX_FALSE;
	accessor->authoringPoseActive=PX_FALSE;
	accessor->blendPreviewActive=PX_TRUE;
	if (!wasBlendPreview) accessor->revision++;
	return PX_TRUE;
}

px_bool PX_LiveEditorRealtimePoseAccessorEndBlendPreview(PX_LiveEditorRealtimePoseAccessor *accessor)
{
	px_bool wasBlendPreview;
	PX_LiveEditorRealtimeAxisAuthoring *axis;
	if (!accessor || !accessor->plive) return PX_FALSE;
	if (!accessor->blendPreviewActive) return PX_TRUE;
	wasBlendPreview=accessor->blendPreviewActive;
	axis=PX_LiveEditorRealtimePoseAccessorGetAxis(accessor,accessor->selectedAxis);
	if (axis && axis->authoringAvailable)
	{
		return PX_LiveEditorRealtimePoseAccessorApplySelectedKey(accessor);
	}
	if (!PX_LiveRealtimeEnter(accessor->plive)) return PX_FALSE;
	accessor->entered=PX_TRUE;
	PX_LiveRealtimeResetAll(accessor->plive);
	if (!PX_LiveRealtimeUpdate(accessor->plive)) return PX_FALSE;
	accessor->authoringPoseActive=PX_FALSE;
	accessor->blendPreviewActive=PX_FALSE;
	if (wasBlendPreview) accessor->revision++;
	return PX_TRUE;
}

px_void PX_LiveEditorRealtimePoseAccessorGetMemoryStats(PX_LiveEditorRealtimePoseAccessor *accessor,PX_LiveRealtimeMemoryStats *stats)
{
	if (!stats) return;
	PX_memset(stats,0,sizeof(*stats));
	if (accessor) PX_LiveRealtimeGetMemoryStats(accessor->plive,stats);
}

px_bool PX_LiveEditorRealtimePoseAccessorHasUnbakedEdits(const PX_LiveEditorRealtimePoseAccessor *accessor)
{
	px_int i;
	if (!accessor)
	{
		return PX_FALSE;
	}
	for (i = 0; i < accessor->axisCount; i++)
	{
		const PX_LiveEditorRealtimeAxisAuthoring *axis = &accessor->axes[i];
		if (axis->dirty || !axis->baked)
		{
			return PX_TRUE;
		}
	}
	return PX_FALSE;
}
