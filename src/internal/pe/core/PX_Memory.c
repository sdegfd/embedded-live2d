#include "PX_Memory.h"

/** @file @brief PainterEngine 动态内存管理模块，提供动态数组、循环缓冲区、FIFO缓冲区和栈等数据结构的内存操作实现 */

/* ── 基础内存操作 ────────────────────────────── */

/**
 * 初始化内存结构（动态数组）
 * @param mp 所属内存池
 * @param memory 待初始化的内存对象
 */
px_void PX_MemoryInitialize(px_memorypool *mp,px_memory *memory)
{
	PX_memset(memory, 0, sizeof(px_memory));
	memory->mp=mp;
}

/**
 * 在内存缓冲区的指定偏移位置插入数据
 * @param memory 内存对象
 * @param offset 插入位置偏移量
 * @param buffer 数据源指针
 * @param size 数据大小
 * @return 成功返回PX_TRUE
 */
px_bool PX_MemoryInsert(px_memory* memory, px_int offset, const px_void* buffer, px_int size)
{
	/* 先追加数据到末尾，再通过内存移动实现中间插入 */
	if(!PX_MemoryCat(memory, buffer, size)) return PX_FALSE;
	/* 将原数据从插入位置向后移动 */
	PX_memcpy(memory->buffer + offset + size, memory->buffer + offset, memory->usedsize - offset - size);
	/* 将新数据复制到插入位置 */
	PX_memcpy(memory->buffer + offset, buffer, size);
	return PX_TRUE;

}

/**
 * 在内存缓冲区尾部追加数据，空间不足时自动按2的幂扩容
 * @param memory 内存对象
 * @param buffer 要追加的数据
 * @param size 数据大小
 * @return 成功返回PX_TRUE，失败返回PX_FALSE
 */
px_bool PX_MemoryCat(px_memory *memory,const px_void *buffer,px_int size)
{
	px_byte *old;
	px_int length,shl;

	if (size==0)
	{
		return PX_TRUE;
	}

	if (memory->usedsize+size>memory->allocsize)
	{
		shl=0;
		old=memory->buffer;
		length=memory->usedsize+size;
		while ((px_int)(1<<++shl)<=length);
		memory->allocsize=(1<<shl);
		memory->buffer=(px_byte*)MP_Malloc(memory->mp,memory->allocsize);
		if(!memory->buffer) return PX_FALSE;
		if(old)
		PX_memcpy(memory->buffer,old,memory->usedsize);

		PX_memcpy(memory->buffer+memory->usedsize,buffer,size);

		if(old)
		MP_Free(memory->mp,old);
	}
	else
	{
		PX_memcpy(memory->buffer+memory->usedsize,buffer,size);
	}
	memory->usedsize+=size;
	memory->bit_pointer = 0;	/* 重置位指针 */
	return PX_TRUE;
}

/**
 * 在内存缓冲区尾部重复填充指定字节
 * @param memory 内存对象
 * @param code 要重复填充的字节值
 * @param size 填充长度
 * @return 成功返回PX_TRUE
 */
px_bool PX_MemoryCatRepeatByte(px_memory* memory, px_byte code, px_int size)
{
	px_byte* old;
	px_int length, shl;

	if (size == 0)
	{
		return PX_TRUE;
	}

	if (memory->usedsize + size > memory->allocsize)
	{
		shl = 0;
		old = memory->buffer;
		length = memory->usedsize + size;
		while ((px_int)(1 << ++shl) <= length);
		memory->allocsize = (1 << shl);
		memory->buffer = (px_byte*)MP_Malloc(memory->mp, memory->allocsize);
		if (!memory->buffer) return PX_FALSE;
		if (old)
			PX_memcpy(memory->buffer, old, memory->usedsize);

		//PX_memcpy(memory->buffer + memory->usedsize, buffer, size);
		PX_memset(memory->buffer + memory->usedsize, code, size);

		if (old)
			MP_Free(memory->mp, old);
	}
	else
	{
		//PX_memcpy(memory->buffer + memory->usedsize, buffer, size);
		PX_memset(memory->buffer + memory->usedsize, code, size);
	}
	memory->usedsize += size;
	memory->bit_pointer = 0;
	return PX_TRUE;
}

/**
 * 在内存缓冲区尾部追加字符串
 * @param memory 内存对象
 * @param src 以'\0'结尾的源字符串
 * @return 成功返回PX_TRUE
 */
px_bool PX_MemoryCatString(px_memory* memory, const px_char* src)
{
	return PX_MemoryCat(memory, src, PX_strlen(src));
}

/**
 * 释放内存缓冲区，归还内存给内存池
 * @param memory 内存对象
 */
px_void PX_MemoryFree(px_memory *memory)
{
	if (memory->allocsize==0||memory->buffer==PX_NULL)
	{
		return;
	}
	MP_Free(memory->mp,memory->buffer);
	memory->buffer=PX_NULL;
	memory->usedsize=0;
	memory->allocsize=0;
	memory->bit_pointer = 0;
}

/**
 * 在内存缓冲区尾部追加一个字节
 * @param memory 内存对象
 * @param b 要追加的字节
 * @return 成功返回PX_TRUE
 */
px_bool PX_MemoryCatByte(px_memory* memory, px_byte b)
{
	return PX_MemoryCat(memory, &b, 1);
}

/**
 * 获取内存缓冲区的数据指针
 * @param memory 内存对象
 * @return 数据缓冲区指针
 */
px_byte * PX_MemoryData(px_memory *memory)
{
	return memory->buffer;
}

/**
 * 向内存缓冲区追加一个比特位。每8位打包为一个字节存储
 * @param memory 内存对象
 * @param b 要追加的比特值（PX_TRUE或PX_FALSE）
 * @return 成功返回PX_TRUE
 */
px_bool PX_MemoryCatBit(px_memory* memory, px_bool b)
{
	if (memory->bit_pointer==0)
	{
		px_byte bit = (b==1);
		if (!PX_MemoryCat(memory, &bit, 1))
			return PX_FALSE;
		memory->bit_pointer = 1;
	}
	else
	{
		px_byte* plastbyte = memory->buffer + memory->usedsize - 1;
		if(b)
			(*plastbyte) |= (1 << memory->bit_pointer);
		memory->bit_pointer++;
		memory->bit_pointer %= 8;	/* 超过8位则回绕 */
	}
	return PX_TRUE;
}

/**
 * 从字节数组中按小端序读取多个比特并追加到内存缓冲区
 * @param memory 内存对象
 * @param data 字节数组数据源
 * @param bit_count 要读取的比特数
 * @return 成功返回PX_TRUE
 */
px_bool PX_MemoryCatBits(px_memory* memory, px_byte data[], px_int bit_count)
{
	px_int i;
	px_uint bp=0;
	for (i = 0; i < bit_count; i++)
	{
		if (!PX_MemoryCatBit(memory, PX_ReadBitLE(&bp, data)))
			return PX_FALSE;
	}
	return PX_TRUE;
}

/**
 * 对齐位指针到字节边界（将bit_pointer清零）
 * @param memory 内存对象
 */
px_void PX_MemoryAlignBits(px_memory* memory)
{
	if (memory->bit_pointer)
	{
		memory->bit_pointer = 0;
	}

}

/**
 * 分配或重新分配内存缓冲区，之前的数据会被丢弃。以2的幂对齐分配大小
 * @param memory 内存对象
 * @param size 新缓冲区大小（字节），若为0则仅释放
 * @return 成功返回PX_TRUE
 */
px_bool PX_MemoryAlloc(px_memory *memory,px_int size)
{
	PX_MemoryFree(memory);
	memory->allocsize=size;
	memory->usedsize=0;
	memory->bit_pointer = 0;
	if (size==0)
	{
		memory->buffer=PX_NULL;
		return PX_TRUE;
	}
	else
	{
		return (memory->buffer=(px_byte *)MP_Malloc(memory->mp,size))!=0;
	}

}

/**
 * 调整内存缓冲区大小，扩容时自动按2的幂对齐
 * @param memory 内存对象
 * @param size 目标大小
 * @return 成功返回PX_TRUE
 */
px_bool PX_MemoryResize(px_memory *memory,px_int size)
{
	if (size==0)
	{
		PX_MemoryFree(memory);
		return PX_TRUE;
	}
	else
	{
		if (size<memory->allocsize)
		{
			memory->usedsize = size;
			return PX_TRUE;
		}
		else
		{
			px_byte* old;
			px_int length, shl;
			shl = 0;
			old = memory->buffer;
			length =size;
			while ((px_int)(1 << ++shl) <= length);
			memory->allocsize = (1 << shl);
			memory->buffer = (px_byte*)MP_Malloc(memory->mp, memory->allocsize);
			if (!memory->buffer) return PX_FALSE;
			if (old)
			{
				PX_memcpy(memory->buffer, old, memory->usedsize);
				MP_Free(memory->mp, old);
			}
			memory->usedsize = size;

			return PX_TRUE;
		}
	}
	return PX_FALSE;;
}

/**
 * 在内存缓冲区中查找指定数据
 * @param memory 内存对象
 * @param buffer 要查找的数据
 * @param size 数据大小
 * @return 找到返回数据地址指针，未找到返回PX_NULL
 */
px_byte *PX_MemoryFind(px_memory *memory,const px_void *buffer,px_int size)
{
	px_int offest;
	if (memory->usedsize<size)
	{
		return PX_NULL;
	}
	for (offest=0;offest<memory->usedsize-size+1;offest++)
	{
		if (PX_memequ(memory->buffer+offest,buffer,size))
		{
			return (memory->buffer+offest);
		}
	}
	return PX_NULL;
}

/**
 * 从内存缓冲区中移除指定范围内的数据
 * @param memory 内存对象
 * @param start 起始偏移（包含）
 * @param end 结束偏移（包含），若start>end则自动交换
 */
px_void PX_MemoryRemove(px_memory *memory,px_int start,px_int end)
{
	if (start>end)
	{
		px_int t = end;
		end = start;
		start = t;
	}
	if (start<0)
	{
		PX_ASSERT();
		return;
	}
	if (end >= memory->usedsize)
	{
		PX_ASSERT();
		return;
	}
	PX_memcpy(memory->buffer+start,memory->buffer+end+1,memory->usedsize-end-1);
	memory->usedsize += (start - end - 1);
	memory->bit_pointer = 0;
}

/**
 * 清空内存缓冲区（仅重置使用量，不释放内存）
 * @param memory 内存对象
 */
px_void PX_MemoryClear(px_memory *memory)
{
	memory->usedsize=0;
	memory->bit_pointer = 0;
}

/**
 * 截断内存缓冲区到指定大小（保留前trimsize字节）
 * @param memory 内存对象
 * @param trimsize 目标截断大小
 */
px_void PX_MemoryLeft(px_memory* memory,px_int trimsize)
{
	if (trimsize>=memory->usedsize)
	{
		return;
	}
	memory->usedsize = trimsize;
	memory->bit_pointer = 0;
}

/**
 * 将数据复制到内存缓冲区的指定偏移位置
 * @param memory 内存对象
 * @param buffer 源数据指针
 * @param startoffset 目标偏移
 * @param size 数据大小
 * @return 成功返回PX_TRUE
 */
px_bool PX_MemoryCopy(px_memory *memory,const px_void *buffer,px_int startoffset,px_int size)
{
	px_byte *old;
	px_int length,shl;

	if (startoffset+size>memory->allocsize)
	{
		shl=0;
		old=memory->buffer;
		length=startoffset+size;
		while ((px_int)(1<<++shl)<=length);
		memory->allocsize=(1<<shl);
		memory->buffer=(px_byte*)MP_Malloc(memory->mp,memory->allocsize);
		if (!memory->buffer)
		{
			MP_Free(memory->mp,old);
			return PX_FALSE;
		}
		if(old)
			PX_memcpy(memory->buffer,old,memory->usedsize);

		PX_memcpy(memory->buffer+startoffset,buffer,size);

		if(old)
			MP_Free(memory->mp,old);

		memory->usedsize=startoffset+size;
	}
	else
	{
		if (startoffset+size>memory->usedsize)
		{
			memory->usedsize=startoffset+size;
		}

		PX_memcpy(memory->buffer+startoffset,buffer,size);

	}
	return PX_TRUE;
}

/* ── 循环缓冲区（CircularBuffer） ───────────────── */

/**
 * 初始化循环缓冲区
 * @param mp 内存池
 * @param pcbuffer 循环缓冲区对象
 * @param size 缓冲区容量
 * @return 成功返回PX_TRUE
 */
px_bool PX_CircularBufferInitialize(px_memorypool* mp, PX_CircularBuffer* pcbuffer, px_int size)
{
	PX_memset(pcbuffer, 0, sizeof(PX_CircularBuffer));
	if (size==0)
	{
		PX_ASSERT();
	}
	pcbuffer->buffer = (px_double *)MP_Malloc(mp, sizeof(px_double) * size);
	if (!pcbuffer)
	{
		return PX_FALSE;
	}
	PX_memset(pcbuffer->buffer, 0, sizeof(px_double) * size);
	pcbuffer->mp = mp;
	pcbuffer->pointer = 0;
	pcbuffer->size = size;
	return PX_TRUE;
}

/** 向循环缓冲区头部推入一个值 */
px_void PX_CircularBufferPush(PX_CircularBuffer* pcbuffer, px_double v)
{
	/* 指针向前移动（回绕），覆盖最旧的数据 */
	pcbuffer->pointer--;
	if (pcbuffer->pointer<0)
	{
		pcbuffer->pointer = pcbuffer->size - 1;
	}
	pcbuffer->buffer[pcbuffer->pointer] = v;
}

/** 在循环缓冲区的指定位置累加一个值 */
px_void PX_CircularBufferAdd(PX_CircularBuffer* pcbuffer, px_int pos, px_double v)
{
	pos = pos % pcbuffer->size;
	if (pos < 0) { pos += pcbuffer->size; }
	pcbuffer->buffer[pos] += v;
}

/** 设置循环缓冲区指定位置的值 */
px_void PX_CircularBufferSet(PX_CircularBuffer* pcbuffer, px_int pos, px_double v)
{
	pos = pos % pcbuffer->size;
	if (pos < 0) { pos += pcbuffer->size; }
	pcbuffer->buffer[pos] = v;
}

/** 获取循环缓冲区指定位置的值 */
px_double  PX_CircularBufferGet(PX_CircularBuffer* pcbuffer, px_int pos)
{
	pos = pos % pcbuffer->size;
	if (pos < 0) { pos += pcbuffer->size; }
	return pcbuffer->buffer[pos];
}

/** 将循环缓冲区所有数据清零 */
px_void PX_CircularBufferZeroClear(PX_CircularBuffer* pcbuffer)
{
	PX_memset(pcbuffer->buffer, 0, sizeof(px_double) * pcbuffer->size);
}

/** 释放循环缓冲区 */
px_void PX_CircularBufferFree(PX_CircularBuffer* pcbuffer)
{
	if (pcbuffer->mp)
	{
		MP_Free(pcbuffer->mp, pcbuffer->buffer);
		PX_memset(pcbuffer, 0, sizeof(PX_CircularBuffer));
	}

}

/** 获取循环缓冲区中延迟pos位置的值（相对当前指针偏移） */
px_double PX_CircularBufferDelay(PX_CircularBuffer* pcbuffer, px_int pos)
{
	return pcbuffer->buffer[(pcbuffer->pointer + pos) %pcbuffer->size];
}

/* ── FIFO缓冲区（先进先出） ───────────────────── */

/**
 * 初始化FIFO缓冲区（先进先出队列），基于px_memory实现
 * @param mp 内存池
 * @param pfifo FIFO缓冲区对象
 * @param bAsynchronous 是否为异步模式（启用自旋锁保护）
 */
px_void PX_FifoBufferInitialize(px_memorypool* mp, px_fifobuffer* pfifo, px_bool bAsynchronous)
{
	PX_memset(pfifo, 0, sizeof(px_fifobuffer));
	pfifo->AsynchronousLock = PX_FALSE;
	PX_MemoryInitialize(mp, pfifo);
}

/**
 * 从FIFO缓冲区头部弹出一条数据
 * @param pfifo FIFO缓冲区
 * @param data 接收数据的缓冲区（可为PX_NULL仅获取大小）
 * @param size 接收缓冲区大小
 * @return 实际弹出数据的字节数，失败返回0
 */
px_int PX_FifoBufferPop(px_fifobuffer* pfifo, px_void* data, px_int size)
{
	if (pfifo->usedsize)
	{
		px_int rsize;
		if (pfifo->bAsynchronous)
		{
			while (pfifo->AsynchronousLock);
			pfifo->AsynchronousLock = PX_TRUE;
		}
		rsize = *(px_int*)pfifo->buffer;
		if (rsize>pfifo->usedsize-(px_int)sizeof(px_int))
		{
			PX_ASSERT();//fifo error
			if (pfifo->bAsynchronous)
				pfifo->AsynchronousLock = PX_FALSE;
			return 0;
		}
		
		if (data)
		{
			if (size < rsize)
			{
				if (pfifo->bAsynchronous)
					pfifo->AsynchronousLock = PX_FALSE;
				return 0;
			}
			PX_memcpy(data, pfifo->buffer + sizeof(px_int), rsize);
		}
			
		PX_MemoryRemove(pfifo, 0, rsize + sizeof(px_int) - 1);
		if (pfifo->bAsynchronous)
		{
			pfifo->AsynchronousLock = PX_FALSE;
		}
		return rsize;
	}

	return 0;

}

/**
 * 获取FIFO缓冲区中数据包数量
 * @param pfifo FIFO缓冲区
 * @return 数据包个数
 */
px_int PX_FifoGetCount(px_fifobuffer* pfifo)
{
	px_int count = 0;
	px_int offset = 0;
	while (offset < pfifo->usedsize)
	{
		px_int size = *(px_int*)(pfifo->buffer + offset);
		offset += size + sizeof(px_int);
		count++;
	}
	return count;
}

/**
 * 向FIFO缓冲区尾部推入一条数据（格式：大小 + 数据）
 * @param pfifo FIFO缓冲区
 * @param data 要推入的数据
 * @param size 数据大小
 * @return 成功返回PX_TRUE
 */
px_bool PX_FifoBufferPush(px_fifobuffer* pfifo, px_void* data, px_int size)
{
	px_int wsize = size;
	if (wsize<0)
	{
		PX_ASSERT();
	}
	if (wsize==0)
	{
		return PX_TRUE;
	}
	if (pfifo->bAsynchronous)
	{
		while (pfifo->AsynchronousLock);
		pfifo->AsynchronousLock = PX_TRUE;
	}
	if (PX_MemoryCat(pfifo, &wsize, sizeof(wsize)))
	{
		if (PX_MemoryCat(pfifo,data,size))
		{
			if (pfifo->bAsynchronous)
			{
				pfifo->AsynchronousLock = PX_FALSE;
			}
			return PX_TRUE;
		}
	}
	if (pfifo->bAsynchronous)
	{
		pfifo->AsynchronousLock = PX_FALSE;
	}
	return PX_FALSE;

}

/**
 * 向FIFO缓冲区尾部仅推入一个长度标记（无实际数据）
 * @param pfifo FIFO缓冲区
 * @param size 长度值
 * @return 成功返回PX_TRUE
 */
px_bool PX_FifoBufferPushSize(px_fifobuffer* pfifo,px_dword size)
{
	if (pfifo->bAsynchronous)
	{
		while (pfifo->AsynchronousLock);
		pfifo->AsynchronousLock = PX_TRUE;
	}

	if (PX_MemoryCat(pfifo, &size, sizeof(size)))
	{
		if (pfifo->bAsynchronous)
		{
			pfifo->AsynchronousLock = PX_FALSE;
		}
		return PX_TRUE;
	}
	if (pfifo->bAsynchronous)
	{
		pfifo->AsynchronousLock = PX_FALSE;
	}
	return PX_FALSE;
}

/**
 * 向FIFO缓冲区尾部推入原始数据（不带长度标记，需配合PushSize使用）
 * @param pfifo FIFO缓冲区
 * @param data 数据指针
 * @param datasize 数据大小
 * @return 成功返回PX_TRUE
 */
px_bool PX_FifoBufferPushData(px_fifobuffer* pfifo,px_void *data, px_dword datasize)
{
	if (pfifo->bAsynchronous)
	{
		while (pfifo->AsynchronousLock);
		pfifo->AsynchronousLock = PX_TRUE;
	}

	if (PX_MemoryCat(pfifo, data, datasize))
	{
		if (pfifo->bAsynchronous)
		{
			pfifo->AsynchronousLock = PX_FALSE;
		}
		return PX_TRUE;
	}
	if (pfifo->bAsynchronous)
	{
		pfifo->AsynchronousLock = PX_FALSE;
	}
	return PX_FALSE;
}

/**
 * 获取FIFO缓冲区头部数据的长度
 * @param pfifo FIFO缓冲区
 * @return 头部数据长度，无数据时返回0
 */
px_int PX_FifoBufferGetPopDataSize(px_fifobuffer* pfifo)
{
	if (pfifo->usedsize>sizeof(px_int))
	{
		while (pfifo->AsynchronousLock);
		return *(px_int*)pfifo->buffer;
	}
	return 0;
}

/** 获取FIFO缓冲区头部数据指针（不弹出） */
px_void* PX_FifoBufferGetPopData(px_fifobuffer* pfifo)
{
	if (pfifo->usedsize>sizeof(px_int))
	{
		return pfifo->buffer + sizeof(px_int);
	}
	return PX_NULL;
}

/** 检查FIFO缓冲区是否为空 */
px_bool PX_FifoBufferIsEmpty(px_fifobuffer* pfifo)
{
	return PX_FifoBufferGetPopDataSize(pfifo) == 0;
}

/** 清空FIFO缓冲区 */
px_void PX_FifoBufferClear(px_fifobuffer* pfifo)
{
	if(pfifo->bAsynchronous)
	{
		while (pfifo->AsynchronousLock);
		pfifo->AsynchronousLock = PX_TRUE;
	}
	PX_MemoryClear(pfifo);
	if (pfifo->bAsynchronous)
	{
		pfifo->AsynchronousLock = PX_FALSE;
	}
}

px_void PX_FifoBufferFree(px_fifobuffer* pfifo)
{
	while (pfifo->AsynchronousLock);
	PX_MemoryFree(pfifo);
}

/* ── 栈（后进先出） ──────────────────────────── */

/**
 * 初始化栈结构
 * @param mp 内存池
 * @param pstack 栈对象
 * @param bAsynchronous 是否为异步模式
 */
px_void PX_StackInitialize(px_memorypool* mp, px_stack* pstack, px_bool bAsynchronous)
{
	PX_memset(pstack, 0, sizeof(px_stack));
	pstack->bAsynchronous = bAsynchronous;
	PX_MemoryInitialize(mp, pstack);
}

/**
 * 从栈顶弹出一条数据
 * @param pstack 栈对象
 * @param data 接收数据的缓冲区（可为PX_NULL仅获取大小）
 * @param size 接收缓冲区大小
 * @return 实际弹出数据的字节数，失败返回0
 */
px_int PX_StackPop(px_stack* pstack, px_void* data, px_int size)
{
	if (pstack->usedsize>sizeof(px_int))
	{
		px_int rsize;
		if (pstack->bAsynchronous)
		{
			while (pstack->AsynchronousLock);
			pstack->AsynchronousLock = PX_TRUE;
		}
		rsize = *(px_int*)(pstack->buffer + pstack->usedsize - sizeof(px_int));
		if (rsize > pstack->usedsize - (px_int)sizeof(px_int))
		{
			PX_ASSERT();//stack error
			if (pstack->bAsynchronous)
				pstack->AsynchronousLock = PX_FALSE;
			return 0;
		}
		
		if (data&&size>0)
		{
			if (size < rsize)
			{
				if (pstack->bAsynchronous)
					pstack->AsynchronousLock = PX_FALSE;
				return 0;
			}
			PX_memcpy(data, pstack->buffer + pstack->usedsize - sizeof(px_int) - rsize, rsize);
		}
			
		PX_MemoryRemove(pstack, pstack->usedsize - sizeof(px_int) - rsize, pstack->usedsize - 1);
		if (pstack->bAsynchronous)
		{
			pstack->AsynchronousLock = PX_FALSE;
		}
		return rsize;

	}
	return 0;
}

/**
 * 向栈顶推入一条数据（格式：数据 + 大小标记）
 * @param pstack 栈对象
 * @param data 要推入的数据
 * @param size 数据大小
 * @return 成功返回PX_TRUE
 */
px_bool PX_StackPush(px_stack* pstack, px_void* data, px_int size)
{
	px_int wsize = size;
	if (wsize < 0)
	{
		PX_ASSERT();
	}
	if (wsize == 0)
	{
		return PX_TRUE;
	}
	if (pstack->bAsynchronous)
	{
		while (pstack->AsynchronousLock);
		pstack->AsynchronousLock = PX_TRUE;
	}
	
	if (PX_MemoryCat(pstack, data, size))
	{
		if (PX_MemoryCat(pstack, &wsize, sizeof(wsize)))
		{
			if (pstack->bAsynchronous)
			{
				pstack->AsynchronousLock = PX_FALSE;
			}
			return PX_TRUE;
		}
	}
	if (pstack->bAsynchronous)
	{
		pstack->AsynchronousLock = PX_FALSE;
	}
	return PX_FALSE;
}

/** 获取栈顶数据的大小（不弹出） */
px_int PX_StackGetPopSize(px_stack* pstack)
{
	if (pstack->usedsize > sizeof(px_int))
	{
		if(pstack->bAsynchronous)
		while(pstack->AsynchronousLock);
		return *(px_int*)(pstack->buffer + pstack->usedsize - sizeof(px_int));
	}
	return 0;
	
}

/** 获取栈中数据包的总数 */
px_int PX_StackGetCount(px_stack* pstack)
{
	px_int count = 0;
	px_int offset = pstack->usedsize;
	while (offset >= sizeof(px_int))
	{
		px_int size = *(px_int*)(pstack->buffer + offset - sizeof(px_int));
		offset -= size + sizeof(px_int);
		count++;
	}
	return count;
}

/** 获取栈顶数据指针（不弹出） */
px_void* PX_StackGetPopData(px_stack* pstack)
{
	if (pstack->usedsize > sizeof(px_int))
	{
		px_int datasize = *(px_int*)(pstack->buffer + pstack->usedsize - sizeof(px_int));
		return pstack->buffer + pstack->usedsize - sizeof(px_int) - datasize;
	}
	return PX_NULL;
}

/** 获取栈底数据指针（最早推入的数据） */
const px_byte * PX_StackGetBottomData(px_stack* pstack)
{
	if (pstack->usedsize > sizeof(px_int))
	{
		return pstack->buffer;
	}
	return PX_NULL;
}

/** 移除栈底（最早推入）的数据包 */
px_void PX_StackRemoveBottom(px_stack* pstack)
{
	if (pstack->usedsize > sizeof(px_int))
	{
		px_int offset = pstack->usedsize;
		px_int datasize=0;
		while (offset>0)
		{
			datasize = *(px_int*)(pstack->buffer + pstack->usedsize - sizeof(px_int));
			offset -= datasize + sizeof(px_int);
		}
		PX_MemoryRemove(pstack, 0, datasize + sizeof(px_int) - 1);
	}
}

/** 释放栈 */
px_void PX_StackFree(px_stack* pstack)
{
	if (pstack->bAsynchronous)
		while (pstack->AsynchronousLock);
	PX_MemoryFree(pstack);
}