#ifndef PX_QUICKSORT_H
#define PX_QUICKSORT_H

/** @file @brief PainterEngine 快速排序模块头文件 */
#include "PX_Typedef.h"
/** 排序比较字段的数据类型枚举 */
typedef enum
{
	PX_QUICKSORT_REORDER_TYPE_U8,	/**< 无符号8位整数 */
		PX_QUICKSORT_REORDER_TYPE_U16,	/**< 无符号16位整数 */
		PX_QUICKSORT_REORDER_TYPE_U32,	/**< 无符号32位整数 */
		PX_QUICKSORT_REORDER_TYPE_I8,	/**< 有符号8位整数 */
		PX_QUICKSORT_REORDER_TYPE_I16,	/**< 有符号16位整数 */
		PX_QUICKSORT_REORDER_TYPE_I32,	/**< 有符号32位整数 */
		PX_QUICKSORT_REORDER_TYPE_F32,	/**< 32位浮点数 */
		PX_QUICKSORT_REORDER_TYPE_F64,	/**< 64位浮点数 */
}PX_QUICKSORT_REORDER_TYPE;
/** 快速排序原子元素结构 */
typedef struct
{
	px_float weight;	/**< 权重值（排序依据） */
	px_void *pData;		/**< 关联数据指针 */
}PX_QuickSortAtom;
/** 升序快速排序（从小到大） */
px_void PX_Quicksort_MinToMax(px_void* array, px_int size, px_int offset, PX_QUICKSORT_REORDER_TYPE type, px_int left, px_int right);
/** 降序快速排序（从大到小） */
px_void PX_Quicksort_MaxToMin(px_void* array, px_int size, px_int offset, PX_QUICKSORT_REORDER_TYPE type, px_int left, px_int right);
/** 按权重对PX_QuickSortAtom数组升序排序 */
px_void PX_Quicksort_ArrayMinToMax(PX_QuickSortAtom array[], px_int left, px_int right);
/** 按权重对PX_QuickSortAtom数组降序排序 */
px_void PX_Quicksort_ArrayMaxToMin(PX_QuickSortAtom array[], px_int left, px_int right);
#endif
