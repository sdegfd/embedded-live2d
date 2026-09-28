/** @file PX_Vector.c
 *  @brief PainterEngine 动态数组（向量）容器实现
 *  提供类似于std::vector的动态数组功能，支持自动扩容、插入、删除等操作。
 *  所有元素按节点大小(nodeSize)进行内存管理。
 */

#include "PX_Vector.h"

/**
 * 初始化动态数组
 * @param mp 内存池指针
 * @param vec 向量对象指针
 * @param nodeSize 单个元素的大小（字节）
 * @param init_size 初始容量
 * @return 初始化成功返回PX_TRUE，失败返回PX_FALSE
 */
px_bool PX_VectorInitialize(px_memorypool *mp,px_vector *vec,px_int nodeSize,px_int init_size)
{
	vec->allocsize=init_size;  /**< 分配容量 */
	vec->nodesize=nodeSize;   /**< 元素大小 */
	vec->size=0;              /**< 当前元素个数 */
	vec->mp=mp;               /**< 内存池 */

	/* 初始容量为0时，延迟分配内存 */
	if (init_size==0)
	{
		vec->data=PX_NULL;
		return PX_TRUE;
	}

	if(!(vec->data=MP_Malloc(mp,init_size*nodeSize)))
		return PX_FALSE;

	return PX_TRUE;
}

/**
 * 设置指定索引位置的元素
 * 若索引超出当前容量，则自动扩容
 * @param vec 向量对象指针
 * @param index 目标索引
 * @param data 要写入的元素数据指针
 * @return 操作成功返回PX_TRUE，失败返回PX_FALSE
 */
px_bool PX_VectorSet(px_vector *vec,px_uint index,px_void *data)
{
	px_void *old;
	px_uint allocsize=1;
	/* 索引超出容量时自动扩容（倍增策略） */
	if (index>(px_uint)vec->allocsize-1)
	{
		while(allocsize<=(px_uint)index) allocsize<<=1;

		old=vec->data;
		vec->data=MP_Malloc(vec->mp,allocsize*vec->nodesize);
		if (vec->data==PX_NULL)
		{
			return PX_FALSE;
		}
		PX_memcpy(vec->data,old,vec->allocsize*vec->nodesize);
		vec->allocsize=allocsize;
		MP_Free(vec->mp,old);
	}
	/* 更新实际元素计数 */
	if (index>(px_uint)vec->size-1)
	{
		vec->size=index+1;
	}
	PX_memcpy((px_byte *)vec->data+index*vec->nodesize,data,vec->nodesize);
	return PX_TRUE;
}

/**
 * 在指定位置前插入元素
 * @param vec 向量对象指针
 * @param insert_before_index 插入位置索引
 * @param data 要插入的元素数据指针
 * @return 操作成功返回PX_TRUE，失败返回PX_FALSE
 */
px_bool PX_VectorInsert(px_vector* vec, px_int insert_before_index, px_void* data)
{
	/* 检查索引有效性 */
	if (insert_before_index < 0 || insert_before_index > vec->size)
	{
		return PX_FALSE;
	}
	/* 先在尾部压入一个空位 */
	if (!PX_VectorPushback(vec, PX_NULL))
	{
		return PX_FALSE;
	}
	/* 将插入点后的元素后移 */
	if (insert_before_index < vec->size - 1)
	{
		PX_memcpy((px_byte*)vec->data + (insert_before_index + 1) * vec->nodesize, (px_byte*)vec->data + insert_before_index * vec->nodesize, (vec->size - insert_before_index - 1) * vec->nodesize);
	}
	/* 写入新元素 */
	PX_memcpy((px_byte*)vec->data + insert_before_index * vec->nodesize, data, vec->nodesize);
	return PX_TRUE;
}

/**
 * 在向量尾部添加元素
 * 若容量不足则自动扩容（翻倍）
 * @param vec 向量对象指针
 * @param data 要添加的元素数据指针，为NULL时填充零
 * @return 操作成功返回PX_TRUE，失败返回PX_FALSE
 */
px_bool PX_VectorPushback(px_vector *vec,px_void *data)
{
	px_void *old;

	/* 容量充足时直接写入 */
	if (vec->size<vec->allocsize)
	{
		if (data)
		{
			PX_memcpy((px_byte*)vec->data + vec->size * vec->nodesize, data, vec->nodesize);
		}
		else
		{
			PX_memset((px_byte*)vec->data + vec->size * vec->nodesize, 0, vec->nodesize);
		}
		vec->size++;
	}
	/* 容量不足，扩容为原来的2倍（或初始化为2） */
	else
	{
		px_int allocSize=vec->allocsize;
		if (allocSize==0)
		{
			allocSize=2;
		}
		else
		{
			allocSize=vec->allocsize*2;
		}

		old=vec->data;
		vec->data=MP_Malloc(vec->mp,allocSize*vec->nodesize);
		if (vec->data==PX_NULL)
		{
			return PX_FALSE;
		}
		PX_memcpy(vec->data,old,vec->allocsize*vec->nodesize);
		vec->allocsize=allocSize;

		if (data)
		{
			PX_memcpy((px_byte*)vec->data + vec->size * vec->nodesize, data, vec->nodesize);
		}
		else
		{
			PX_memset((px_byte*)vec->data + vec->size * vec->nodesize, 0, vec->nodesize);
		}
		vec->size++;
		if(old)
		MP_Free(vec->mp,old);
	}
	return PX_TRUE;
}

/**
 * 对向量按权重从大到小重新排序
 * @param vec 向量对象指针
 * @param weight_offset 权重字段在元素结构体中的偏移量
 * @param type 排序数据类型（int/float/double）
 */
px_void PX_VectorReorder_MaxToMin(px_vector* vec, px_int weight_offset, PX_QUICKSORT_REORDER_TYPE type)
{
	if (vec->size > 0)
	{
		PX_Quicksort_MaxToMin(vec->data, vec->nodesize, weight_offset, type, 0, vec->size - 1);
	}
}

/**
 * 对向量按权重从小到大重新排序
 * @param vec 向量对象指针
 * @param weight_offset 权重字段在元素结构体中的偏移量
 * @param type 排序数据类型
 */
px_void PX_VectorReorder_MinToMax(px_vector* vec, px_int weight_offset, PX_QUICKSORT_REORDER_TYPE type)
{
	if (vec->size > 0)
	{
		PX_Quicksort_MinToMax(vec->data, vec->nodesize, weight_offset, type, 0, vec->size - 1);
	}
}

/**
 * 将元素压入向量并移动到指定索引位置
 * @param vec 向量对象指针
 * @param data 要添加的元素数据指针
 * @param index 目标索引
 * @return 操作成功返回PX_TRUE，失败返回PX_FALSE
 */
px_bool PX_VectorPushTo(px_vector *vec,px_void *data,px_int index)
{
	if(!PX_VectorPushback(vec,data)) return PX_FALSE;
	if (index<0||index>=vec->size)
	{
		return PX_TRUE;
	}
#ifndef PX_DEBUG_MODE
	if (index<0)
	{
		PX_ASSERT();
		return PX_FALSE;
	}
#endif
	/* 将新元素移到指定索引位置 */
	PX_memcpy((px_byte *)vec->data+(index+1)*vec->nodesize,(px_byte *)vec->data+(index)*vec->nodesize,(vec->size-index-1)*vec->nodesize);
	PX_memcpy((px_byte *)vec->data+(index)*vec->nodesize,data,vec->nodesize);
	return PX_TRUE;
}

/**
 * 删除指定索引位置的元素
 * @param vec 向量对象指针
 * @param index 要删除的索引
 * @return 操作成功返回PX_TRUE，失败返回PX_FALSE
 */
px_bool PX_VectorErase(px_vector *vec,px_int index)
{
	px_int i;
	if (index<0||index>=vec->size)
	{
		PX_ERROR("Vector Erase Error!");
		return PX_FALSE;
	}
	/* 将后续元素前移覆盖目标元素 */
	for (i=index;i<vec->size-1;i++)
	{
		PX_memcpy((px_byte *)vec->data+i*vec->nodesize,(px_byte *)vec->data+(i+1)*vec->nodesize,vec->nodesize);
	}
	vec->size--;
	return PX_TRUE;
}

/**
 * 弹出向量尾部元素（仅减少计数，不释放内存）
 * @param vec 向量对象指针
 * @return 操作成功返回PX_TRUE，向量为空时返回PX_FALSE
 */
px_bool PX_VectorPop(px_vector *vec)
{
	if (vec->size==0)
	{
		return PX_FALSE;
	}
	vec->size--;
	return PX_TRUE;
}

/**
 * 获取向量尾部元素（不弹出）
 * @param vec 向量对象指针
 * @param data 输出缓冲区指针
 * @return 操作成功返回PX_TRUE，向量为空时返回PX_FALSE
 */
px_bool PX_VectorLastTo(px_vector* vec, px_void* data)
{
	if (vec->size == 0)
	{
		return PX_FALSE;
	}
	PX_memcpy(data, (px_byte*)vec->data + (vec->size - 1) * vec->nodesize, vec->nodesize);
	return PX_TRUE;
}

/**
 * 弹出向量尾部元素并复制到输出缓冲区
 * @param vec 向量对象指针
 * @param data 输出缓冲区指针
 * @return 操作成功返回PX_TRUE，向量为空时返回PX_FALSE
 */
px_bool PX_VectorPopTo(px_vector* vec, px_void* data)
{
	if (vec->size==0)
	{
		return PX_FALSE;
	}
	PX_memcpy(data,(px_byte *)vec->data+(vec->size-1)*vec->nodesize, vec->nodesize);
	vec->size--;
	return PX_TRUE;
}

/**
 * 从尾部弹出N个元素
 * @param vec 向量对象指针
 * @param N 要弹出的元素数量
 * @return 操作成功返回PX_TRUE，元素不足时返回PX_FALSE
 */
px_bool PX_VectorPopN(px_vector* vec,px_int N)
{
	if (N == 0)
		return PX_TRUE;
	if (vec->size < N)
	{
		return PX_FALSE;
	}
	vec->size-=N;
	return PX_TRUE;
}

/**
 * 复制向量内容到目标向量
 * @param destvec 目标向量指针
 * @param resvec 源向量指针
 * @return 操作成功返回PX_TRUE，节点大小不匹配时返回PX_FALSE
 */
px_bool PX_VectorCopy(px_vector *destvec,px_vector *resvec)
{
	if (destvec->nodesize!=resvec->nodesize)
	{
		return PX_FALSE;
	}
	/* 目标容量不足时先扩容 */
	if (destvec->allocsize<resvec->allocsize)
	{
		PX_VectorResize(destvec,resvec->allocsize);
	}
	PX_memcpy(destvec->data,resvec->data,resvec->nodesize*resvec->size);
	destvec->size=resvec->size;
	return PX_TRUE;
}

/**
 * 释放向量内存
 * @param vec 向量对象指针
 */
px_void PX_VectorFree(px_vector *vec)
{
	if((vec->data)!=PX_NULL)
		MP_Free(vec->mp,vec->data);
}

/**
 * 调整向量容量并重置元素计数
 * @param vec 向量对象指针
 * @param size 新的容量
 * @return 操作成功返回PX_TRUE，失败返回PX_FALSE
 */
px_bool PX_VectorResize(px_vector *vec,px_int size)
{
	if (size!=vec->size)
	{
		PX_VectorFree(vec);
		if(!PX_VectorInitialize(vec->mp,vec,vec->nodesize,size))return PX_FALSE;
		PX_memset(vec->data,0,vec->nodesize*vec->allocsize);
		vec->size=size;
		return PX_TRUE;
	}
	return PX_FALSE;
}

/**
 * 检查索引是否在向量有效范围内
 * @param vec 向量对象指针
 * @param index 要检查的索引
 * @return 有效返回PX_TRUE，无效返回PX_FALSE
 */
px_bool PX_VectorCheckIndex(px_vector *vec,px_int index)
{
	if (index<0||index>=vec->size)
	{
		return PX_FALSE;
	}
	return PX_TRUE;
}

/**
 * 设置向量的实际元素计数（不改变已分配内存）
 * @param vec 向量对象指针
 * @param size 新的元素计数
 * @return 操作成功返回PX_TRUE，参数无效时返回PX_FALSE
 */
px_bool PX_VectorAllocSize(px_vector *vec,px_int size)
{
	if (size<0||size<vec->size)
	{
		return PX_FALSE;
	}
	vec->size=size;
	return PX_TRUE;
}
