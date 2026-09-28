#ifndef PIXELSES_MEMORYPOOL
#define PIXELSES_MEMORYPOOL
/** @file @brief PainterEngine 内存池管理模块头文件，定义内存池结构和分配/释放API */
#include "PX_Typedef.h"

#define __PX_MEMORYPOOL_ALIGN_BYTES (sizeof(void *))
#define PX_MEMORYPOOL_DEBUG_CHECK


typedef enum
{
	PX_MEMORYPOOL_ERROR_OUTOFMEMORY,
	PX_MEMORYPOOL_ERROR_INVALID_ACCESS,
	PX_MEMORYPOOL_ERROR_INVALID_ADDRESS
}PX_MEMORYPOOL_ERROR;

typedef px_void (*PX_MP_ErrorCall)(px_void *ptr,PX_MEMORYPOOL_ERROR);
#define PX_MEMORYPOOL_ERROR_FUNCTION(name) px_void name(px_void *ptr,PX_MEMORYPOOL_ERROR error)
#if defined(PX_DEBUG_MODE) && defined(PX_MEMORYPOOL_DEBUG_CHECK)
typedef struct 
{
	px_void *startAddr;	/**< 分配起始地址 */
	px_void *endAddr;	/**< 分配结束地址 */
	px_dword offset;	/**< 相对内存池起始的偏移 */
}MP_alloc_debug;


typedef struct  
{
	px_dword append;	/**< 魔数标记，用于检测内存越界 */
}MP_Append_data;
#define MP_APPENDDATA_MAGIC 0x31415926	/**< 内存边界魔数 */
#endif

/* ── 内存池结构 ──────────────────────────────── */

typedef struct _memoryPool
{
	px_void *AllocAddr;	/**< 当前分配地址指针 */
	px_void *StartAddr;	/**< 内存区域起始地址 */
	px_void *EndAddr;	/**< 内存区域结束地址 */
	px_uint32 Size;		/**< 内存池总大小 */
	px_uint32 FreeSize;	/**< 剩余空闲大小 */
	px_uint32 nodeCount;	/**< 当前分配的节点数 */
	px_uint32 FreeTableCount;	/**< 空闲碎片表项数 */
	px_uint32 MaxMemoryfragSize;	/**< 最大空闲碎片大小 */
	px_bool   no_catch_error;	/**< 是否禁用内存不足错误捕获 */
	PX_MP_ErrorCall ErrorCall_Ptr;	/**< 错误回调函数指针 */
	px_void* userptr;		/**< 用户自定义数据指针（传递给回调） */
#if defined(PX_DEBUG_MODE) && defined(PX_MEMORYPOOL_DEBUG_CHECK)
	MP_alloc_debug DEBUG_allocdata[256];	/**< 分配记录表（调试用） */
	px_dword DEBUG_allocdata_catch[16];	/**< 捕获列表（调试用） */
	px_bool enable_allocdata_tracert;	/**< 是否启用分配记录跟踪 */
#endif
}px_memorypool;


#if defined(PX_DEBUG_MODE) && defined(PX_MEMORYPOOL_DEBUG_CHECK)
px_void MP_UnreleaseInfo(px_memorypool *mp);
px_void MP_Catch(px_memorypool *mp,px_dword MID);
#define MP_DEBUG_AID(x) MP_UnreleaseInfo(x)
#else
#define MP_DEBUG_AID(x)
#define MP_Catch(x,y)
#endif


/**
 * 创建内存池
 * @param MemoryAddr 内存区域起始地址
 * @param MemorySize 内存池大小
 * @return 内存池结构
 */
px_memorypool	MP_Create	(px_void *MemoryAddr,px_uint MemorySize);
#define PX_MemorypoolCreate		MP_Create
/**
 * 获取指针对应的内存大小
 * @param Pool 内存池
 * @param Ptr 内存指针
 * @return 成功返回大小，否则返回0
 */
px_uint MP_Size(px_memorypool *Pool,px_void *Ptr);

/**
 * 从内存池分配内存
 * @param Pool 内存池
 * @param Size 分配大小
 * @return 成功返回内存起始地址，失败返回NULL
 */
px_void		*MP_Malloc	(px_memorypool *Pool,px_uint Size);
#define		PX_Malloc(t,mp,s) ((t *)MP_Malloc(mp,s))

/**
 * 释放内存回内存池
 * @param Pool 内存池
 * @param pAddress 要释放的内存指针
 */
px_void		MP_Free		(px_memorypool *Pool,px_void *pAddress);
#define     PX_Free		MP_Free
px_void		MP_Release	(px_memorypool *Pool);
px_void     MP_Reset    (px_memorypool *Pool);
px_void		MP_ResetZero(px_memorypool* Pool);
px_void     MP_NoCatchError(px_memorypool* Pool);
px_void     MP_CatchError(px_memorypool* Pool);
/** 添加内存池错误处理 */
px_void MP_ErrorCatch(px_memorypool* Pool, PX_MP_ErrorCall ErrorCall, px_void* ptr);
#endif

