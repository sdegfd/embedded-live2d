/** @file PX_Vector.h
 *  @brief PainterEngine 动态数组（向量）容器头文件
 *  提供类似于 std::vector 的动态数组功能，支持自动扩容和元素访问。
 */

#ifndef __PX_VECTOR_H
#define __PX_VECTOR_H
#include "PX_MemoryPool.h"
#include "PX_Quicksort.h"

/**
 * @brief 动态数组结构体
 * 管理一块连续内存，支持按元素大小进行存取操作。
 */
typedef struct __px_vector
{
	px_void *data;        /**< 数据缓冲区指针 */
	px_int nodesize;      /**< 单个元素大小（字节） */
	px_int size;          /**< 当前元素个数 */
	px_int allocsize;     /**< 已分配容量（元素个数） */
	px_memorypool *mp;    /**< 内存池指针 */
}px_vector;

px_bool PX_VectorInitialize(px_memorypool *mp,px_vector *vec,px_int nodeSize,px_int init_size);
px_bool PX_VectorSet(px_vector *vec,px_uint index,px_void *data);
px_bool PX_VectorAllocSize(px_vector *vec,px_int size);
px_bool PX_VectorPushback(px_vector *vec,px_void *data);
px_bool PX_VectorInsert(px_vector* vec, px_int insert_before_index, px_void* data);
px_bool PX_VectorPushTo(px_vector *vec,px_void *data,px_int index);
px_bool PX_VectorErase(px_vector *vec,px_int index);
px_bool PX_VectorPop(px_vector *vec);
px_bool PX_VectorPopTo(px_vector* vec, px_void* data);
px_bool PX_VectorLastTo(px_vector* vec, px_void* data);
px_bool PX_VectorPopN(px_vector* vec, px_int N);
px_bool PX_VectorCopy(px_vector *destvec,px_vector *resvec);
px_void PX_VectorFree(px_vector *vec);
px_bool PX_VectorResize(px_vector *vec,px_int size);
px_bool PX_VectorCheckIndex(px_vector *vec,px_int index);
px_void PX_VectorReorder_MaxToMin(px_vector* vec, px_int weight_offset, PX_QUICKSORT_REORDER_TYPE type);
px_void PX_VectorReorder_MinToMax(px_vector* vec, px_int weight_offset, PX_QUICKSORT_REORDER_TYPE type);

/** 获取向量当前元素个数（宏） */
#define PX_VectorSize(x) ((x)->size)

#ifdef PX_DEBUG_MODE
/** 按索引获取向量元素指针（调试模式带类型检查） */
#define PX_VECTORAT(t,vec,i) (sizeof(t)==(vec)->nodesize?((t *)((px_byte *)((vec)->data)+(vec)->nodesize*(i))):PX_NULL)
#else
/** 按索引获取向量元素指针 */
#define PX_VECTORAT(t,vec,i) ((t *)((px_byte *)((vec)->data)+(vec)->nodesize*(i)))
#endif

/** 获取向量最后一个元素的指针 */
#define PX_VECTORLAST(t,vec) PX_VECTORAT(t,vec,(vec)->size-1)

/** 清空向量（仅重置计数，不释放内存） */
#define PX_VectorClear(vec) ((vec)->size=0)

#endif
