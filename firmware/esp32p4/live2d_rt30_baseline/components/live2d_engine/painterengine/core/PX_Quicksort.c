#include "PX_Quicksort.h"

/** @file @brief PainterEngine 快速排序模块，支持多种数据类型的排序和按权重重排 */

/* ── 内部辅助函数 ────────────────────────────── */

/** 交换数组中两个元素的位置（按size字节交换） */
static void PX_Quicksort_swap(px_byte *array,px_int size, px_int left, px_int right)
{
	
	px_dword* pdw1, * pdw2;
	px_byte* pbyte1, * pbyte2;
	px_int i;
	pdw1 = (px_dword*)(array + left * size);
	pdw2 = (px_dword*)(array + right * size);
	pbyte1 = array + left*size+(size>>2)*4;
	pbyte2 = array + right*size+(size&3);
	
	for (i = 0; i < (size>>2); i++)
	{
		px_dword dwtemp = pdw1[i];
		pdw1[i] = pdw2[i];
		pdw2[i] = dwtemp;
	}
	for (i = 0; i < (size & 3); i++)
	{
		px_byte  btemp;
		btemp = pbyte1[i];
		pbyte1[i] = pbyte2[i];
		pbyte2[i] = btemp;
	}
}

/** 获取数组中指定索引元素的权重值（根据类型转换） */
static px_double PX_Quicksort_GetWeight(px_byte* array, px_int size, px_int offset, PX_QUICKSORT_REORDER_TYPE type, px_int index)
{
	
	switch (type)
	{
	case PX_QUICKSORT_REORDER_TYPE_U8:
		return (px_double)(*(px_byte*)(array + index * size+ offset));
	case PX_QUICKSORT_REORDER_TYPE_U16:
		return (px_double)(*(px_uint16*)(array + index * size + offset));
	case PX_QUICKSORT_REORDER_TYPE_U32:
		return (px_double)(*(px_uint32*)(array + index * size + offset));
	case PX_QUICKSORT_REORDER_TYPE_I8:
		return (px_double)(*(px_char*)(array + index * size + offset));
	case PX_QUICKSORT_REORDER_TYPE_I16:
		return (px_double)(*(px_int16*)(array + index * size + offset));
	case PX_QUICKSORT_REORDER_TYPE_I32:
		return (px_double)(*(px_int32*)(array + index * size + offset));
	case PX_QUICKSORT_REORDER_TYPE_F32:
		return (px_double)(*(px_float*)(array + index * size + offset));
	case PX_QUICKSORT_REORDER_TYPE_F64:
		return (px_double)(*(px_double*)(array + index * size + offset));
	default:
		return 0;
	}
}

/** 升序分区函数（Lomuto分区方案） */
static px_int PX_Quicksort_partition_l(px_byte *array, px_int size,px_int offset, PX_QUICKSORT_REORDER_TYPE type, px_int left, px_int right, px_int pivot_index)
{
	px_double pivot_value = PX_Quicksort_GetWeight(array, size, offset, type, pivot_index);
	px_int store_index = left;
	px_int i;

	PX_Quicksort_swap(array, size, pivot_index, right);
	for (i = left; i < right; i++)
	{
		px_double cweight = PX_Quicksort_GetWeight(array, size, offset, type, i);
		if (cweight < pivot_value)
		{
			PX_Quicksort_swap(array,size, i, store_index);
			++store_index;
		}
	}
	PX_Quicksort_swap(array, size, store_index, right);
	return store_index;
}

/** 降序分区函数 */
static px_int PX_Quicksort_partition_m(px_byte *array, px_int size, px_int offset, PX_QUICKSORT_REORDER_TYPE type, px_int left, px_int right, px_int pivot_index)
{
	px_double pivot_value = PX_Quicksort_GetWeight(array, size, offset, type, pivot_index);
	px_int store_index = left;
	px_int i;

	PX_Quicksort_swap(array, size, pivot_index, right);
	for (i = left; i < right; i++)
	{
		px_double cweight = PX_Quicksort_GetWeight(array, size, offset, type, i);
		if (cweight > pivot_value)
		{
			PX_Quicksort_swap(array, size, i, store_index);
			++store_index;
		}
	}
	PX_Quicksort_swap(array, size, store_index, right);
	return store_index;
}

/* ── 排序API ─────────────────────────────────── */

/**
 * 对数组进行升序快速排序（从小到大）
 * @param array 数组指针
 * @param size 每个元素的大小
 * @param offset 比较字段在元素内的偏移
 * @param type 比较字段的数据类型
 * @param left 排序起始索引
 * @param right 排序结束索引
 */
px_void PX_Quicksort_MinToMax(px_void *array,px_int size,px_int offset, PX_QUICKSORT_REORDER_TYPE type, px_int left, px_int right)
{
	px_int pivot_index = left;
	px_int pivot_new_index;

	while (right > left)
	{
		pivot_new_index = PX_Quicksort_partition_l((px_byte *)array, size, offset, type, left, right, pivot_index);
		PX_Quicksort_MinToMax(array,size,offset,type, left, pivot_new_index - 1);
		pivot_index = left = pivot_new_index + 1;

	}
}

/** 对PX_QuickSortAtom数组按weight字段升序排序 */
px_void PX_Quicksort_ArrayMinToMax(PX_QuickSortAtom array[], px_int left, px_int right)
{
	PX_Quicksort_MinToMax(array, sizeof(PX_QuickSortAtom), PX_STRUCT_OFFSET(PX_QuickSortAtom, weight),PX_QUICKSORT_REORDER_TYPE_F32, left, right);
}

/**
 * 对数组进行降序快速排序（从大到小）
 * @param array 数组指针
 * @param size 每个元素的大小
 * @param offset 比较字段在元素内的偏移
 * @param type 比较字段的数据类型
 * @param left 排序起始索引
 * @param right 排序结束索引
 */
px_void PX_Quicksort_MaxToMin(px_void* array, px_int size, px_int offset, PX_QUICKSORT_REORDER_TYPE type, px_int left, px_int right)
{
	px_int pivot_index = left;
	px_int pivot_new_index;
	while (right > left)
	{
		pivot_new_index = PX_Quicksort_partition_m((px_byte *)array, size, offset, type, left, right, pivot_index);
		PX_Quicksort_MaxToMin(array,size,offset,type,left, pivot_new_index - 1);
		pivot_index = left = pivot_new_index + 1;
	}
}

/** 对PX_QuickSortAtom数组按weight字段降序排序 */
px_void PX_Quicksort_ArrayMaxToMin(PX_QuickSortAtom array[], px_int left, px_int right)
{
	PX_Quicksort_MaxToMin(array, sizeof(PX_QuickSortAtom), PX_STRUCT_OFFSET(PX_QuickSortAtom, weight), PX_QUICKSORT_REORDER_TYPE_F32, left, right);
}


