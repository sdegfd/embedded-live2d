/**
 * @file PX_LiveFramework.h
 * @brief PainterEngine Live2D 框架内核头文件，定义 Live2D 模型数据结构与 API
 *
 * 本文件是 PainterEngine 移植的 Live2D 渲染框架的核心头文件，
 * 定义了顶点、三角形、纹理、动画、图层等数据结构以及完整的框架 API。
 * 该代码为第三方移植代码，属于 PainterEngine Live2D 渲染框架的一部分。
 */
#ifndef PX_LIVEFRAMEWORK_H
#define PX_LIVEFRAMEWORK_H

#include "../core/PX_Core.h"
#include "PX_LiveRealtime.h"

/* ── 常量定义 ────────────────────────────────── */

#define PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER 256 /**< 支持的最大图层数量 */
#define PX_LIVE_ID_MAX_LEN 32                 /**< ID 字符串最大长度 */
#define PX_LIVE_LAYER_MAX_LINK_NODE 16        /**< 图层最大子节点链接数 */
#define PX_LIVE_VERSION 0x00000001             /**< Live2D 框架数据版本号 */

/* RT30 尾部：附加在 v1 .live 主体之后的小端烘焙实时轴数据（可选） */
#define PX_LIVE_RT30_TRAILER_MAGIC              0x30335452u /* "RT30" */
#define PX_LIVE_RT30_TRAILER_VERSION            1u
#define PX_LIVE_RT30_TRAILER_HEADER_SIZE        24u
#define PX_LIVE_RT30_TRAILER_AXIS_ENTRY_SIZE    68u
#define PX_LIVE_RT30_TRAILER_BINDING_ENTRY_SIZE 16u
/* 动画帧 payload 中局部变换魔数，复用原 reserve 区，payload 体积不变 */
#define PX_LIVE_ANIMATION_LOCAL_TRANSFORM_MAGIC 0x3146544cu /* "LTF1" */

/* ── 动画帧操作码枚举 ──────────────────────────── */

/** 动画帧操作码：定义单个帧中包含的指令类型 */
typedef enum
{
	PX_LIVE_ANIMATION_FRAME_OPCODE_STAMP=0,       /**< 帧操作码：时间戳 */
	PX_LIVE_ANIMATION_FRAME_OPCODE_TRANSLATION,    /**< 帧操作码：位移变换 */
}PX_LIVE_ANIMATION_FRAME_OPCODE;

/* ── 顶点结构体 ────────────────────────────────── */

/** Live2D 模型顶点：包含源位置、当前位置、法线及变换信息 */
typedef struct
{
	px_point	sourcePosition;          /**< 顶点原始位置（模型空间） */
	px_point    currentPosition;          /**< 顶点当前位置（经变换后） */
	px_point	normal;                  /**< 顶点法线方向 */
	px_point	beginTranslation;        /**< 顶点位移起始值 */
	px_point	currentTranslation;      /**< 顶点当前位移值 */
	px_point	endTranslation;          /**< 顶点位移目标值 */
	px_point    velocity;                /**< 弹性运动速度 */
	px_int32	k;                       /**< 弹性系数（0 表示无弹性） */
	px_float32  u,v;                     /**< 纹理 UV 坐标 */
}PX_LiveVertex;

/** 三角形索引结构：三个顶点在顶点数组中的索引 */
typedef struct
{
	px_int index1; /**< 顶点 1 索引 */
	px_int index2; /**< 顶点 2 索引 */
	px_int index3; /**< 顶点 3 索引 */
}PX_LiveTriangle;

/* ── 纹理结构体 ────────────────────────────────── */

/** Live2D 纹理：包含 PainterEngine 纹理对象及在模型中的偏移量 */
typedef struct
{
	px_char id[PX_LIVE_ID_MAX_LEN];   /**< 纹理标识 ID */
	px_texture Texture;                /**< PainterEngine 纹理对象 */
	px_int textureOffsetX;             /**< 纹理在模型中的 X 偏移 */
	px_int textureOffsetY;             /**< 纹理在模型中的 Y 偏移 */
}PX_LiveTexture;

/* ── 动画结构体 ────────────────────────────────── */

/** Live2D 动画：包含一组动画帧数据 */
typedef struct
{
	px_char id[PX_LIVE_ID_MAX_LEN];  /**< 动画标识 ID */
	px_vector framesMemPtr;          /**< 动画帧指针数组（px_void * 类型） */
}PX_LiveAnimation;
/* ── 动画帧结构体 ──────────────────────────────── */

/** 动画帧头部：包含帧标识、数据大小和持续时间 */
typedef struct
{
	px_char frameid[PX_LIVE_ID_MAX_LEN]; /**< 帧标识 ID */
	px_int32 size;                       /**< 帧数据大小（不含头部） */
	px_uint32 duration_ms;               /**< 帧持续时间（毫秒） */
}PX_LiveAnimationFrameHeader;

//////////////////////////////////////////////////////////////////////////

/* ── 动画帧荷载结构体 ───────────────────────────── */

/** 动画帧荷载：存储图层的变换参数（旋转、拉伸、平移、弹性） */
typedef struct
{
	px_point32 translation;                /**< 根图层平移量 */
	px_float32 stretch;                    /**< 子图层拉伸比例 */
	px_float32 rotation;                   /**< 旋转角度 */
	px_int32 mapTexture;                   /**< 映射纹理索引 */
	px_int32 translationVerticesCount;     /**< 顶点位移数量 */
	px_point32 impulse;                    /**< 弹性冲量 */

	px_float panc_x, panc_y, panc_width, panc_height; /**< 扇形变形（PANC）区域 */
	px_float panc_sx,panc_sy;            /**< 扇形变形起始中心 */
	px_float panc_endx, panc_endy;        /**< 扇形变形结束中心 */
	/*
	 * 本图层上的视觉变换：影响自身及后代，但不改变祖先的顶点/关键点/子骨骼长度。
	 * 这些字段复用旧 reserve 区，payload 总大小不变（reserve[32] -> 4 字段 + reserve[26]）。
	 */
	px_point32 localTranslation;           /**< 局部平移（模型空间） */
	px_float32 localRotation;              /**< 局部旋转角度 */
	px_float32 localScaleOffset;           /**< 局部缩放偏移（实际缩放 = 1 + offset） */
	px_dword localTransformMagic;          /**< 局部变换魔数，用于判断字段是否有效 */
	px_dword reserve[26];                  /**< 保留字段 */
}PX_LiveAnimationFramePayload;
/* ── 图层结构体 ────────────────────────────────── */

/** Live2D 模型图层：包含顶点、三角形、父子层级关系及所有变换参数 */
struct _PX_LiveLayer
{
	px_char id[PX_LIVE_ID_MAX_LEN];      /**< 图层标识 ID */

	px_int32 parent_index;                /**< 父图层索引（-1 表示根图层） */
	px_int32 child_index[PX_LIVE_LAYER_MAX_LINK_NODE]; /**< 子图层索引数组 */

	/* ── 旋转变换 ────────────────────────── */
	px_float rotationAngle;               /**< 整体旋转角度 */
	px_float rel_beginRotationAngle;      /**< 旋转插值起始角度 */
	px_float rel_currentRotationAngle;    /**< 旋转插值当前角度 */
	px_float rel_currentRotationSin;      /**< 当前旋转角度的正弦值（缓存） */
	px_float rel_currentRotationCos;      /**< 当前旋转角度的余弦值（缓存） */
	px_bool rel_rotationTrigValid;        /**< 旋转 sin/cos 是否对应当前角度 */
	px_float rel_cachedRotationAngle;     /**< sin/cos 对应的旋转角，可与当前角不同 */
	px_float rel_endRotationAngle;        /**< 旋转插值目标角度 */

	/* ── 拉伸变换 ────────────────────────── */
	px_float rel_beginStretch;            /**< 拉伸插值起始比例 */
	px_float rel_currentStretch;          /**< 拉伸插值当前比例 */
	px_float rel_endStretch;              /**< 拉伸插值目标比例 */

	/* ── 平移变换 ────────────────────────── */
	px_point rel_beginTranslation;        /**< 平移插值起始值 */
	px_point rel_currentTranslation;      /**< 平移插值当前值 */
	px_point rel_endTranslation;          /**< 平移插值目标值 */

	/* ── 局部视觉变换（RT30/局部变换，相对层级姿态） ── */
	px_point rel_beginLocalTranslation;      /**< 局部平移插值起始值 */
	px_point rel_currentLocalTranslation;    /**< 局部平移插值当前值 */
	px_point rel_endLocalTranslation;        /**< 局部平移插值目标值 */
	px_float rel_beginLocalRotationAngle;    /**< 局部旋转插值起始角度 */
	px_float rel_currentLocalRotationAngle;  /**< 局部旋转插值当前角度 */
	px_float rel_currentLocalRotationSin;    /**< 局部旋转正弦，仅在角度完全相等时复用 */
	px_float rel_currentLocalRotationCos;    /**< 局部旋转余弦，仅在角度完全相等时复用 */
	px_float rel_cachedLocalRotationAngle;   /**< 上述 sin/cos 对应的局部角度 */
	px_bool rel_localRotationTrigValid;      /**< 局部旋转 sin/cos 是否有效 */
	px_float rel_endLocalRotationAngle;      /**< 局部旋转插值目标角度 */
	px_float rel_beginLocalScale;            /**< 局部缩放插值起始值 */
	px_float rel_currentLocalScale;          /**< 局部缩放插值当前值 */
	px_float rel_endLocalScale;              /**< 局部缩放插值目标值 */

	/* ── 弹性冲量 ────────────────────────── */
	px_point rel_impulse;                 /**< 弹性物理冲量 */

	/* ── 关键点 ──────────────────────────── */
	px_point keyPoint;                    /**< 图层原始关键点（中心点） */
	px_point currentKeyPoint;             /**< 图层当前关键点（经变换后） */

	/* ── 顶点与三角形 ─────────────────────── */
	px_vector vertices;                   /**< 顶点数组（PX_LiveVertex 类型） */
	px_vector triangles;                  /**< 三角形数组（PX_Delaunay_Triangle 类型） */

	/* ── 扇形变形（PANC） ─────────────────── */
	px_float panc_x, panc_y, panc_width, panc_height, panc_sx, panc_sy; /**< PANC 参数 */
	px_float panc_beginx, panc_beginy;   /**< PANC 插值起始点 */
	px_float panc_currentx, panc_currenty; /**< PANC 插值当前点 */
	px_float panc_endx, panc_endy;       /**< PANC 插值目标点 */

	px_bool visible;                      /**< 图层可见性 */
	px_bool showMesh;                     /**< 是否显示网格调试线 */
	px_int  LinkTextureIndex;             /**< 绑定的纹理索引 */
	px_int  RenderTextureIndex;           /**< 实际渲染使用的纹理索引 */
};
typedef struct _PX_LiveLayer PX_LiveLayer;

/* ── 像素着色器回调 ───────────────────────────── */

/** 像素着色器函数指针：自定义每个像素的渲染方式 */
typedef px_void (*PX_LiveFramework_PixelShader)(px_surface *psurface,px_int x,px_int y,px_point position,px_float u,px_float v,px_point normal,px_texture *pTexture,PX_TEXTURERENDER_BLEND *blend);

/* ── 框架状态枚举 ─────────────────────────────── */

/** Live2D 框架运行状态 */
typedef enum
{
	PX_LIVEFRAMEWORK_STATUS_STOP,           /**< 停止状态 */
	PX_LIVEFRAMEWORK_STATUS_PLAYING,        /**< 播放状态 */
}PX_LIVEFRAMEWORK_STATUS;

/** 动画模式：Timeline 与 RT30 实时模式互斥，不能同时写模型姿态 */
typedef enum
{
	PX_LIVE_MODE_NEUTRAL,      /**< 中性：无 Timeline 也无 RT30 写入 */
	PX_LIVE_MODE_TIMELINE,     /**< Timeline 时间轴动画驱动 */
	PX_LIVE_MODE_REALTIME30,   /**< RT30 实时多轴驱动 */
}PX_LIVE_ANIMATION_MODE;

/* ── 主框架结构体 ─────────────────────────────── */

/** Live2D 框架主控结构：管理图层、纹理、动画和虚拟机的完整状态 */
/** Single-frame kernel timings in microseconds; zero when skipped. */
typedef struct
{
	px_dword poseUs;       /* RT30 evaluator, or timeline VM */
	px_dword physicalUs;   /* physical/vertex update */
	px_dword keypointUs;   /* fine diagnostic only */
	px_dword visualTransformUs;
	px_dword stretchUs;
	px_dword vertexTransformUs;
	px_dword uvUpdateUs;
	px_dword sortUs;       /* draw-list construction and sorting */
	px_dword drawUs;       /* layer rasterization */
} PX_LiveFrameworkFrameProfile;

/* Diagnostic workload counters; enabled only in L2D_PROFILE_DETAIL builds. */
typedef struct {
    px_uint32 triangles, fragments, samplerCalls, fourTapAlphaZero;
    px_uint32 alphaZero, alphaOpaque, alphaMixed;
} PX_LiveFrameworkLayerWork;
typedef struct {
    PX_LiveFrameworkLayerWork layers[PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER];
    px_uint32 layerCount, coveredPixels, maxOverdraw;
    px_uint64 fragmentOverdrawSum;
    px_uint32 scanlines, spans, spanPixels, maxSpanLength;
    px_uint32 sampleInBounds, sampleOutOfBounds;
    px_uint32 fullyInBoundsSpans, partialOrOobSpans;
} PX_LiveFrameworkDetailFrame;
/* Screen-space bounds from the exact vertices submitted to rasterization. */
typedef struct {
    px_float min_x, min_y, max_x, max_y;
    px_bool valid, unsafe;
} PX_LiveGeometryBounds;
typedef struct _PX_LiveFramework PX_LiveFramework;
#ifdef __cplusplus
extern "C" {
#endif
px_void PX_LiveFrameworkDetailBeginFrame(px_uchar *overdraw, px_int width, px_int height);
px_void PX_LiveFrameworkDetailGetFrame(PX_LiveFrameworkDetailFrame *out);
px_void PX_LiveFrameworkGetGeometryBounds(const PX_LiveFramework *plive,
                                          PX_LiveGeometryBounds *out);

#if L2D_CFG_PROFILE_VISUAL
typedef struct {
	px_dword physical_ran;
	px_dword history_valid;
	px_dword layers;
	px_dword get_visual_calls;
	px_dword point_rotate_calls;
	px_dword sin_angle_calls;
	px_dword cos_angle_calls;
	px_dword sind_calls;
	px_dword sind_transform;
	px_dword sind_final;
	px_dword sind_set_rotation;
	px_dword sind_other;
	px_dword unique_rotation_angles;
	px_dword unique_local_rotation_angles;
	px_dword unique_point_rotate_angles;
	px_dword point_rotate_angle_samples;
	px_dword point_rotate_angle_overflow;
	px_dword ancestor_visits;
	px_dword unique_ancestors;
	px_dword depth_sum;
	px_dword max_depth;
	px_dword rotation_changed;
	px_dword local_rotation_changed;
	px_dword scale_changed;
	px_dword local_translation_changed;
	px_dword hierarchy_translation_changed;
	px_dword keypoint_changed;
	px_dword parent_visual_changed;
	px_dword traverse_us;
	px_dword local_translation_rotate_us;
	px_dword relative_rotate_us;
	px_dword final_sincos_us;
	px_dword diag_pose_us;
	px_dword diag_physical_us;
} PX_LiveVisualDiagFrame;

typedef struct {
	px_int index;
	px_int parent;
	px_int depth;
	px_int children;
	px_dword rotation_bits;
	px_dword local_rotation_bits;
	px_dword scale_bits;
	px_dword local_tx_bits;
	px_dword local_ty_bits;
	px_dword key_x_bits;
	px_dword key_y_bits;
	px_char id[PX_LIVE_ID_MAX_LEN];
} PX_LiveVisualDiagLayer;

void PX_LiveFrameworkVisualDiagInvalidate(void);
void PX_LiveFrameworkVisualDiagCopy(PX_LiveFramework *plive, PX_LiveVisualDiagFrame *out);
int PX_LiveFrameworkVisualDiagTopology(PX_LiveFramework *plive, PX_LiveVisualDiagLayer *out, int capacity);
#endif
#ifdef __cplusplus
}
#endif

typedef struct _PX_LiveFramework
{
	px_memorypool *mp;                       /**< 内存池指针 */

	/* ── 属性 ────────────────────────────── */
	px_char id[PX_LIVE_ID_MAX_LEN];         /**< Live2D 框架标识 ID */

	/* ── 编辑器控制器 ─────────────────────── */
	px_int currentEditLayerIndex;            /**< 当前编辑的图层索引 */
	px_int currentEditAnimationIndex;        /**< 当前编辑的动画索引 */
	px_int currentEditFrameIndex;            /**< 当前编辑的帧索引 */
	px_int currentEditVertexIndex;           /**< 当前编辑的顶点索引 */

	/* ── 渲染相关 ────────────────────────── */
	px_vector layers;                        /**< 图层对象数组（PX_LiveLayer 类型） */
	px_vector livetextures;                  /**< 纹理对象数组（PX_LiveTexture 类型） */
	px_vector liveAnimations;                /**< 动画对象数组（PX_LiveAnimation 类型） */
	px_int width;                            /**< 模型画布宽度 */
	px_int height;                           /**< 模型画布高度 */
	px_bool showRange;                       /**< 是否显示范围边框 */
	px_bool showKeypoint;                    /**< 是否显示关键点 */
	px_bool showlinker;                      /**< 是否显示图层链接线 */
	px_bool showFocusLayer;                  /**< 是否高亮当前编辑图层 */
	px_bool showRootHelperLine;              /**< 是否显示根图层辅助线 */
	px_float refer_x,refer_y;               /**< 参考坐标偏移 */
	PX_LIVEFRAMEWORK_STATUS status;          /**< 框架运行状态 */
	/* Timeline 与 RT30 互斥的姿态写入模式 */
	PX_LIVE_ANIMATION_MODE animationMode;    /**< 动画模式（NEUTRAL/TIMELINE/REALTIME30） */
	PX_LiveFramework_PixelShader pixelShader; /**< 自定义像素着色器（为空则使用默认） */
	px_bool fastNearestSampling;             /**< 是否启用快速最近邻采样 */
	px_float renderScale;                    /**< 渲染缩放比例 */
	PX_LiveRealtime realtime;                /**< RT30 实时多轴求值器 */
	PX_LiveFrameworkFrameProfile frameProfile; /**< Last frame phase timings */
	PX_LiveGeometryBounds geometryBounds;      /**< Last raster frame geometry bounds */
	px_uint32 meshPoseRevision;             /**< Revision already applied to vertices */

	/* ── 动画虚拟机 ───────────────────────── */
	px_int32 reg_duration;                   /**< 当前帧总时长（ms） */
	px_int32 reg_ip;                         /**< 指令指针（当前帧索引） */
	px_int32 reg_elapsed;                    /**< 已播放时间（ms） */
	px_int32 reg_animation;                  /**< 当前播放动画索引 */
	px_int32 reg_bp;                         /**< 断点寄存器 */
	px_dword trigCacheHit;                  /**< Hits for the last update on this instance */
	px_dword trigCacheMiss;                 /**< Misses for the last update on this instance */
	/* Instance clones alias model-owned texture pixels, triangle indices,
	 * animation payloads, and baked RT30 tables. Free must not release them. */
	px_bool sharesImmutablePayloads;
}PX_LiveFramework;



/* ── 框架生命周期函数 ──────────────────────────── */

/** 初始化 Live2D 框架 */
px_bool PX_LiveFrameworkInitialize(px_memorypool *mp,PX_LiveFramework *plive,px_int width,px_int height);
/** 开始播放当前动画 */
px_void PX_LiveFrameworkPlay(PX_LiveFramework *plive);
/** 暂停播放 */
px_void PX_LiveFrameworkPause(PX_LiveFramework *plive);
/** 重置框架状态（回退到初始帧） */
px_void PX_LiveFrameworkReset(PX_LiveFramework *plive);
/** 停止播放（等价于重置） */
px_void PX_LiveFrameworkStop(PX_LiveFramework *plive);

/* ── 更新/渲染拆分（模式仲裁） ─────────────────── */

/** 按 animationMode 推进一帧状态：TIMELINE 走 VM，REALTIME30 走 RT30 求值，NEUTRAL 仅松弛。
 *  RenderCurrent 不推进状态；两者分离便于实时模式下参数与帧率解耦。 */
px_void PX_LiveFrameworkUpdate(PX_LiveFramework *plive,px_dword elapsed);
px_void PX_LiveFrameworkGetTrigCacheFrame(const PX_LiveFramework *plive, px_dword *hit, px_dword *miss);
/** 仅渲染当前姿态，不推进动画状态。保留 ESP 的 renderScale/快速采样/sin-cos 缓存路径。 */
px_void PX_LiveFrameworkRenderCurrent(px_surface *psurface,PX_LiveFramework *plive,px_int x,px_int y,PX_ALIGN refPoint);

/* ── 渲染函数 ─────────────────────────────────── */

/** 渲染一帧到目标表面 */
px_void PX_LiveFrameworkRender(px_surface *psurface,PX_LiveFramework *plive,px_int x,px_int y,PX_ALIGN refPoint,px_dword elapsed);
/** 使用参考坐标偏移渲染 */
px_void PX_LiveFrameworkRenderRefer(px_surface *psurface,PX_LiveFramework *plive,PX_ALIGN refPoint,px_dword elapsed);

/* ── 动画控制 ─────────────────────────────────── */

/** 播放指定索引的动画 */
px_bool PX_LiveFrameworkPlayAnimation(PX_LiveFramework *plive,px_int index);
/** 按名称播放动画 */
px_bool PX_LiveFrameworkPlayAnimationByName(PX_LiveFramework *plive,const px_char name[]);

/* ── 图层管理 ─────────────────────────────────── */

/** 获取图层的父图层 */
PX_LiveLayer *PX_LiveFrameworkGetLayerParent(PX_LiveFramework *plive,PX_LiveLayer *pLayer);
/** 获取图层的子图层 */
PX_LiveLayer *PX_LiveFrameworkGetLayerChild(PX_LiveFramework *plive,PX_LiveLayer *pLayer,px_int childIndex);
/** 通过索引获取图层 */
PX_LiveLayer *PX_LiveFrameworkGetLayer(PX_LiveFramework *plive,px_int index);

/* ── UV 坐标更新 ───────────────────────────────── */

/** 更新指定图层的渲染顶点 UV 坐标 */
px_void PX_LiveFrameworkUpdateLayerRenderVerticesUV(PX_LiveFramework *plive,PX_LiveLayer *pLayer);

/* ── 动画数据管理 ──────────────────────────────── */


/* ── 纹理管理 ─────────────────────────────────── */


/* ── 删除操作 ─────────────────────────────────── */


/* ── 图层链接管理 ──────────────────────────────── */


/* ── 删除操作（按索引） ─────────────────────────── */

/** 通过索引删除图层 */
px_void PX_LiveFrameworkDeleteLayer(PX_LiveFramework *plive,px_int index);
/** 通过索引删除纹理 */
px_void PX_LiveFrameworkDeleteLiveTexture(PX_LiveFramework *plive,px_int index);
/** 通过索引删除动画 */
px_void PX_LiveFrameworkDeleteLiveAnimation(PX_LiveFramework *plive,px_int index);
/** 释放框架所有资源 */
px_void PX_LiveFrameworkFree(PX_LiveFramework *plive);


/* ── 导入导出 ─────────────────────────────────── */

/** 从内存缓冲区导入 Live2D 框架数据 */
px_bool PX_LiveFrameworkImport(px_memorypool *mp,PX_LiveFramework *plive,px_void *buffer,px_int size);


/* ── PX_Live 镜像 API ─────────────────────────── */

/**
 * PX_Live 是 PX_LiveFramework 的别名镜像类型。
 * 提供简化后的 API，便于独立运行模型实例。
 */
typedef PX_LiveFramework PX_Live;
/** 从现有框架创建 Live 实例（深拷贝图层和顶点数据） */
px_bool PX_LiveCreate(px_memorypool *mp,PX_LiveFramework *pLiveFramework,PX_Live *pLive);
/** 释放 Live 实例资源 */
px_void PX_LiveFree(PX_Live *pLive);

/** 播放动画 */
px_void PX_LivePlay(PX_Live*plive);
/** 获取动画数量 */
px_int PX_LiveGetAnimationCount(PX_Live *plive);
/** 通过索引播放动画 */
px_bool PX_LivePlayAnimation(PX_Live *plive,px_int index);
/** 通过名称播放动画 */
px_bool PX_LivePlayAnimationByName(PX_Live *plive,const px_char name[]);
/** 暂停播放 */
px_void PX_LivePause(PX_Live *plive);
/** 重置状态 */
px_void PX_LiveReset(PX_Live *plive);
/** 停止播放 */
px_void PX_LiveStop(PX_Live *plive);

/* ── RT30 实时多轴接口（薄封装 PX_LiveRealtime.*） ── */
/** 进入 RT30 实时模式：先 Reset Timeline 状态，再切到 REALTIME30。 */
px_bool PX_LiveEnterRealtime30(PX_Live *plive);
/** 退出 RT30 实时模式：恢复 NEUTRAL 并复位实时轴。 */
px_void PX_LiveLeaveRealtime30(PX_Live *plive);
/** 设置某轴选中样本与 Q15 权重（sample 0..29，weight 0..32767）。 */
px_bool PX_LiveSetRealtimeAxisSample(PX_Live *plive,px_int axisHandle,px_uchar sampleIndex,px_uint16 weightQ15);
/** 设置某轴归一化 Q15 值（映射到样本区间）与权重。 */
px_bool PX_LiveSetRealtimeAxisNormalizedQ15(PX_Live *plive,px_int axisHandle,px_uint16 normalizedQ15,px_uint16 weightQ15);
/** 按字符串 ID 查找实时轴，返回整数 handle（未找到返回 -1）。 */
px_int PX_LiveFindRealtimeAxisById(const PX_Live *plive,const px_char id[]);

/** 渲染一帧 */
px_void PX_LiveRender(px_surface *psurface,PX_Live *plive,px_int x,px_int y,PX_ALIGN refPoint,px_dword elapsed);


#endif
