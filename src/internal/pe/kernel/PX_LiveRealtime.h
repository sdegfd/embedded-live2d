#ifndef PX_LIVEREALTIME_H
#define PX_LIVEREALTIME_H

#include "../core/PX_Core.h"

/*
 * RT30 is a parameter-driven pose evaluator.  It deliberately has no clock,
 * instruction pointer or dependency on the desktop editor.  All coordinates
 * stored here are model/layer-local values; screen transforms never enter the
 * RT30 data path.
 */
#define PX_LIVE_REALTIME_SAMPLE_COUNT       30
#define PX_LIVE_REALTIME_MAX_AXES           32
#define PX_LIVE_REALTIME_AXIS_ID_MAX_LEN    32
#define PX_LIVE_REALTIME_WEIGHT_ONE_Q15     32767
#define PX_LIVE_REALTIME_INVALID_HANDLE     (-1)
#define PX_LIVE_REALTIME_STATIC_BUDGET_BYTES  (640u*1024u)
#define PX_LIVE_REALTIME_RUNTIME_BUDGET_BYTES (128u*1024u)

typedef enum
{
	PX_LIVE_REALTIME_PROPERTY_TRANSLATION = 1 << 0,
	PX_LIVE_REALTIME_PROPERTY_ROTATION    = 1 << 1,
	PX_LIVE_REALTIME_PROPERTY_STRETCH     = 1 << 2,
	PX_LIVE_REALTIME_PROPERTY_VERTICES    = 1 << 3,
	PX_LIVE_REALTIME_PROPERTY_LOCAL_TRANSLATION = 1 << 4,
	PX_LIVE_REALTIME_PROPERTY_LOCAL_ROTATION    = 1 << 5,
	PX_LIVE_REALTIME_PROPERTY_LOCAL_SCALE       = 1 << 6,
	PX_LIVE_REALTIME_PROPERTY_TEXTURE            = 1 << 7,
	PX_LIVE_REALTIME_PROPERTY_IMPULSE            = 1 << 8,
	PX_LIVE_REALTIME_PROPERTY_ALL         = (1 << 9) - 1
}PX_LIVE_REALTIME_PROPERTY;

/* Bake-only binding description. vertexIndices is read, but never retained. */
typedef struct
{
	px_uint16 layerIndex;
	px_uint16 propertyMask;
	px_uint16 vertexCount;
	px_uint16 reserved;
	const px_uint16 *vertexIndices;
}PX_LiveRealtimeBindingDesc;

/*
 * One binding in one authoring key pose.  translation and vertexDeltas are
 * model-space values, rotation is in PainterEngine degrees, and stretch is a
 * multiplier (normally 1 at the neutral key).  Bake subtracts the selected
 * default key, so the stored default sample is always a zero delta.
 */
typedef struct
{
	px_point translation;
	px_float rotation;
	px_float stretch;
	px_point localTranslation;
	px_float localRotation;
	px_float localScale;
	px_int mapTexture;
	px_point impulse;
	const px_point *vertexDeltas;
}PX_LiveRealtimeBindingPose;

typedef struct
{
	/* bindingCount entries, in the same order as PX_LiveRealtimeBindingDesc. */
	const PX_LiveRealtimeBindingPose *bindings;
}PX_LiveRealtimeKeyPose;

typedef struct
{
	const px_char *id;
	/* Zero asks BakeAxis to compute a stable FNV-1a hash from id. */
	px_uint32 idHash;
	px_uchar middleKeyIndex;  /* 1..28 */
	/* Must be 0, middleKeyIndex, or 29. */
	px_uchar defaultSampleIndex;
	px_uchar coordFractionBits;
	px_uchar rotationFractionBits;
	px_uchar stretchFractionBits;
	px_uchar reserved[3];
	px_uint16 bindingCount;
	const PX_LiveRealtimeBindingDesc *bindings;
	/* K0, KM and K29 respectively. */
	PX_LiveRealtimeKeyPose keyPoses[3];
}PX_LiveRealtimeAxisBakeDesc;

/* Compact runtime binding. Data in every sample is ordered by property mask. */
typedef struct
{
	px_uint16 layerIndex;
	px_uint16 propertyMask;
	px_uint16 vertexCount;
	px_uint16 reserved;
	px_uint32 vertexIndexOffset;
	px_uint32 sampleOffset;
}PX_LiveRealtimeBinding;

typedef struct
{
	px_char id[PX_LIVE_REALTIME_AXIS_ID_MAX_LEN];
	px_uint32 idHash;
	px_uchar middleKeyIndex;
	px_uchar defaultSampleIndex;
	px_uchar sampleIndex;
	px_uchar coordFractionBits;
	px_uchar rotationFractionBits;
	px_uchar stretchFractionBits;
	px_uchar valid;
	px_uchar reserved0;
	px_uint16 weightQ15;
	px_uint16 bindingCount;
	px_uint16 reserved1;
	px_uint32 vertexIndexCount;
	px_uint32 sampleStride;
	px_uint32 sampleBytes;
	/* continuousActive is off for the integer sample API. Position is q in [0,29]. */
	px_bool continuousActive;
	px_float continuousPosition;
	px_uchar textureHysteresis;
	PX_LiveRealtimeBinding *bindings;
	px_uint16 *vertexIndices;
	px_int16 *samples;
}PX_LiveRealtimeAxis;

typedef struct
{
	px_int32 translationX;
	px_int32 translationY;
	px_int32 rotation;
	px_int32 stretch;
	px_int32 localTranslationX;
	px_int32 localTranslationY;
	px_int32 localRotation;
	px_int32 localScale;
	px_int32 impulseX;
	px_int32 impulseY;
	px_int32 textureIndex;
	px_uint16 textureWeightQ15;
	px_uint16 textureAxisIndex;
}PX_LiveRealtimeLayerAccumulator;

typedef struct
{
	px_int32 x;
	px_int32 y;
}PX_LiveRealtimeVertexAccumulator;

typedef struct
{
	px_uint32 staticBytes;
	px_uint32 runtimeBytes;
	px_uint32 totalBytes;
	px_uint32 layerCount;
	px_uint32 vertexCount;
	px_uint32 activeAxisCount;
	px_uint32 selectedSampleBytes;
}PX_LiveRealtimeMemoryStats;

/* Last parameters actually accumulated for one axis. Not stored in the file. */
typedef struct
{
	px_bool applied;
	px_bool continuousActive;
	px_float continuousPosition;
	px_uchar sampleIndex;
	px_uint16 weightQ15;
}PX_LiveRealtimeAppliedSample;

typedef struct
{
	px_memorypool *mp;
	px_uint16 axisCount;
	px_uint16 runtimeLayerCount;
	/* All axes in one instance share accumulator Q formats. */
	px_uchar coordFractionBits;
	px_uchar rotationFractionBits;
	px_uchar stretchFractionBits;
	px_uchar quantizationSet;
	px_uint32 runtimeVertexCount;
	px_uint32 runtimeBytes;
	px_uint32 staticBytes;
	px_uint32 stateRevision;
	px_uint32 evaluatedRevision;
	px_bool dirty;
	/* False after a topology or baked-axis change; the next update rebuilds. */
	px_bool contributionValid;
	px_void *runtimeBlock;
	px_uint32 *vertexOffsets; /* runtimeLayerCount + 1 */
	PX_LiveRealtimeLayerAccumulator *layerAccumulators;
	PX_LiveRealtimeVertexAccumulator *vertexAccumulators;
	PX_LiveRealtimeAxis axes[PX_LIVE_REALTIME_MAX_AXES];
	PX_LiveRealtimeAppliedSample appliedSample[PX_LIVE_REALTIME_MAX_AXES];
}PX_LiveRealtime;

struct _PX_LiveFramework;
typedef struct _PX_LiveFramework PX_LiveFramework;

px_uint32 PX_LiveRealtimeHashId(const px_char id[]);
px_void PX_LiveRealtimeInitialize(PX_LiveRealtime *realtime,px_memorypool *mp);
px_void PX_LiveRealtimeFree(PX_LiveRealtime *realtime);
px_bool PX_LiveRealtimeClone(PX_LiveRealtime *dest,px_memorypool *mp,const PX_LiveRealtime *source);

/* Allocates/rebuilds accumulators during load/edit only; Update never allocates. */
px_bool PX_LiveRealtimePrepareRuntime(PX_LiveFramework *plive);

/* replaceHandle=-1 appends; otherwise the existing handle is atomically replaced. */
px_bool PX_LiveRealtimeBakeAxis(PX_LiveFramework *plive,px_int replaceHandle,const PX_LiveRealtimeAxisBakeDesc *desc,px_int *outHandle);
/* Loader path for an already baked sample-major axis; validates then deep-copies. */
px_bool PX_LiveRealtimeInstallBakedAxis(PX_LiveFramework *plive,px_int replaceHandle,const PX_LiveRealtimeAxis *source,px_int *outHandle);
px_bool PX_LiveRealtimeDeleteAxis(PX_LiveFramework *plive,px_int axisHandle);
px_void PX_LiveRealtimeClearAxes(PX_LiveFramework *plive);

px_int PX_LiveRealtimeGetAxisCount(const PX_LiveFramework *plive);
PX_LiveRealtimeAxis *PX_LiveRealtimeGetAxis(PX_LiveFramework *plive,px_int axisHandle);
const PX_LiveRealtimeAxis *PX_LiveRealtimeGetAxisConst(const PX_LiveFramework *plive,px_int axisHandle);
px_int PX_LiveRealtimeFindAxisByHash(const PX_LiveFramework *plive,px_uint32 idHash);
px_int PX_LiveRealtimeFindAxisById(const PX_LiveFramework *plive,const px_char id[]);

px_bool PX_LiveRealtimeEnter(PX_LiveFramework *plive);
px_void PX_LiveRealtimeLeave(PX_LiveFramework *plive);
px_bool PX_LiveRealtimeSetAxisSample(PX_LiveFramework *plive,px_int axisHandle,px_uchar sampleIndex,px_uint16 weightQ15);
/* q is continuous in [0,29]. Integer samples stay available through SetAxisSample. */
px_bool PX_LiveRealtimeSetAxisPosition(PX_LiveFramework *plive,px_int axisHandle,px_float position,px_uint16 weightQ15);
px_bool PX_LiveRealtimeSetAxisNormalizedQ15(PX_LiveFramework *plive,px_int axisHandle,px_uint16 normalizedQ15,px_uint16 weightQ15);
px_bool PX_LiveRealtimeResetAxis(PX_LiveFramework *plive,px_int axisHandle);
px_void PX_LiveRealtimeResetAll(PX_LiveFramework *plive);

/*
 * Integer samples read one block. A continuous position blends the two
 * neighboring samples after bake, then applies the axis weight. Texture
 * indices stay discrete. Duration of PX_LiveRealtimeTransition is wall time,
 * not a frame count.
 */
typedef struct
{
	px_float startValue;
	px_float targetValue;
	px_dword startTimeMs;
	px_dword durationMs;
	px_bool smoothstep;
}PX_LiveRealtimeTransition;

px_float PX_LiveRealtimeTransitionEvaluate(const PX_LiveRealtimeTransition *transition,px_dword nowMs);
px_bool PX_LiveRealtimeUpdate(PX_LiveFramework *plive);
/* Drop cached axis contributions. The next update rebuilds from samples. */
px_void PX_LiveRealtimeInvalidateContributions(PX_LiveFramework *plive);
px_void PX_LiveRealtimeGetMemoryStats(const PX_LiveFramework *plive,PX_LiveRealtimeMemoryStats *stats);

#endif
