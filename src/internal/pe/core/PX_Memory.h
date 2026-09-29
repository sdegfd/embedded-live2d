#ifndef __PX_MEMORY_H
#define __PX_MEMORY_H

/** @file @brief PainterEngine 动态内存管理模块头文件，定义px_memory、循环缓冲区、FIFO缓冲区和栈的数据结构及API */

#include "PX_MemoryPool.h"

/* ── 动态数组（px_memory） ─────────────────────── */

/** 动态内存结构，提供自动扩容的字节数组 */
typedef struct __PX_memory
{
	px_byte *buffer;			/**< 数据缓冲区指针 */
	px_memorypool *mp;			/**< 所属内存池 */
	px_int  usedsize;			/**< 已使用字节数 */
	px_int  allocsize;			/**< 已分配字节数（按2的幂对齐） */
	px_int  bit_pointer;		/**< 位写入时的当前位偏移（0~7），用于按位操作 */
	px_bool bAsynchronous;		/**< 是否为异步模式 */
	PX_ATOMIC px_bool AsynchronousLock;	/**< 异步自旋锁 */
}px_memory;

/** 初始化内存结构（动态数组） */
px_void PX_MemoryInitialize(px_memorypool *mp,px_memory *memory);
/** 在指定偏移位置插入数据 */
px_bool PX_MemoryInsert(px_memory *memory,px_int offset,const px_void *buffer,px_int size);
/** 清空内存缓冲区（仅重置usedsize，不释放内存） */
px_void PX_MemoryClear(px_memory *memory);
/** 分配或重新分配缓冲区，之前数据被丢弃。若size为0则仅释放 */
px_bool PX_MemoryAlloc(px_memory *memory,px_int size);
/** 调整缓冲区大小（可扩容或缩容），扩容时自动2的幂对齐 */
px_bool PX_MemoryResize(px_memory *memory,px_int size);
/** 在尾部追加数据，空间不足时自动按2的幂扩容 */
px_bool PX_MemoryCat(px_memory *memory,const px_void *buffer,px_int size);
/** 在尾部追加字符串 */
px_bool PX_MemoryCatString(px_memory *memory,const px_char *src);
/** 将数据复制到指定偏移位置，必要时自动扩容 */
px_bool PX_MemoryCopy(px_memory *memory,const px_void *buffer,px_int startoffset,px_int size);
/** 在缓冲区中查找指定数据，返回首次出现地址 */
px_byte *PX_MemoryFind(px_memory *memory,const px_void *buffer,px_int size);
/** 移除[start,end]范围内的数据 */
px_void PX_MemoryRemove(px_memory *memory,px_int start,px_int end);
/** 释放缓冲区内存 */
px_void PX_MemoryFree(px_memory *memory);
/** 在尾部追加一个字节 */
px_bool PX_MemoryCatByte(px_memory *memory,px_byte b);
/** 在尾部重复填充指定字节 */
px_bool PX_MemoryCatRepeatByte(px_memory* memory, px_byte code, px_int size);
/** 获取数据缓冲区指针 */
px_byte *PX_MemoryData(px_memory *memory);
/** 追加一个比特位（每8位打包为一字节） */
px_bool PX_MemoryCatBit(px_memory* memory, px_bool b);
/** 从字节数组按小端序追加多个比特 */
px_bool PX_MemoryCatBits(px_memory* memory, px_byte data[],px_int bit_count);
/** 对齐位指针到字节边界 */
px_void PX_MemoryAlignBits(px_memory* memory);
/** 截断缓冲区到前trimsize字节 */
px_void PX_MemoryLeft(px_memory* memory, px_int trimsize);

/* ── 循环缓冲区（CircularBuffer） ───────────────── */

/** 循环缓冲区结构，用于存储双精度浮点数的环形队列 */
typedef struct
{
	px_memorypool* mp;		/**< 所属内存池 */
	px_double *buffer;		/**< 数据缓冲区 */
	px_int size;			/**< 缓冲区容量 */
	px_int pointer;			/**< 当前指针位置（最新数据位置） */
}PX_CircularBuffer;

/** 初始化循环缓冲区 */
px_bool PX_CircularBufferInitialize(px_memorypool* mp, PX_CircularBuffer* pcbuffer, px_int size);
/** 向缓冲区头部推入一个值（覆盖最旧值） */
px_void PX_CircularBufferPush(PX_CircularBuffer* pcbuffer, px_double v);
/** 在指定位置累加一个值 */
px_void PX_CircularBufferAdd(PX_CircularBuffer* pcbuffer, px_int pos,px_double v);
/** 设置指定位置的值 */
px_void PX_CircularBufferSet(PX_CircularBuffer* pcbuffer, px_int pos,px_double v);
/** 获取指定位置的值 */
px_double PX_CircularBufferGet(PX_CircularBuffer* pcbuffer, px_int pos);
/** 将所有数据清零 */
px_void PX_CircularBufferZeroClear(PX_CircularBuffer* pcbuffer);
/** 释放循环缓冲区 */
px_void PX_CircularBufferFree(PX_CircularBuffer* pcbuffer);
/** 获取相对当前指针偏移pos位置的值（延迟读取） */
px_double PX_CircularBufferDelay(PX_CircularBuffer* pcbuffer, px_int pos);

/* ── FIFO缓冲区（先进先出队列） ──────────────────── */

/** FIFO缓冲区（基于px_memory实现），数据格式为[大小|数据] */
typedef px_memory px_fifobuffer;
/** 初始化FIFO缓冲区 */
px_void PX_FifoBufferInitialize(px_memorypool* mp, px_fifobuffer* pfifo, px_bool bAsynchronous);
/** 从头部弹出一条数据 */
px_int PX_FifoBufferPop(px_fifobuffer* pfifo, px_void* data, px_int size);
/** 向尾部推入一条数据（格式：大小 + 数据） */
px_bool PX_FifoBufferPush(px_fifobuffer* pfifo, px_void* data, px_int size);
/** 获取头部数据的大小 */
px_int PX_FifoBufferGetPopDataSize(px_fifobuffer* pfifo);
/** 获取头部数据指针（不弹出） */
px_void* PX_FifoBufferGetPopData(px_fifobuffer* pfifo);
/** 检查FIFO缓冲区是否为空 */
px_bool PX_FifoBufferIsEmpty(px_fifobuffer* pfifo);
/** 清空FIFO缓冲区 */
px_void PX_FifoBufferClear(px_fifobuffer* pfifo);
/** 释放FIFO缓冲区 */
px_void PX_FifoBufferFree(px_fifobuffer* pfifo);
/** 向尾部仅推入长度标记（无实际数据） */
px_bool PX_FifoBufferPushSize(px_fifobuffer* pfifo, px_dword size);
/** 向尾部推入原始数据（不带长度标记，需配合PushSize使用） */
px_bool PX_FifoBufferPushData(px_fifobuffer* pfifo, px_void* data, px_dword datasize);
/** 获取FIFO缓冲区中数据包的数量 */
px_int PX_FifoGetCount(px_fifobuffer* pfifo);

/* ── 栈（后进先出） ──────────────────────────────── */

/** 栈结构（基于px_memory实现），数据格式为[数据|大小] */
typedef px_memory px_stack;
/** 初始化栈 */
px_void PX_StackInitialize(px_memorypool* mp, px_stack* pstack, px_bool bAsynchronous);
/** 从栈顶弹出一条数据 */
px_int PX_StackPop(px_stack* pstack, px_void* data, px_int size);
/** 向栈顶推入一条数据 */
px_bool PX_StackPush(px_stack* pstack, px_void* data, px_int size);
/** 获取栈顶数据的大小 */
px_int PX_StackGetPopSize(px_stack* pstack);
/** 获取栈中数据包总数 */
px_int PX_StackGetCount(px_stack* pstack);
/** 获取栈顶数据指针 */
px_void* PX_StackGetPopData(px_stack* pstack);
/** 释放栈 */
px_void PX_StackFree(px_stack* pstack);
/** 获取栈底数据指针（最早推入的数据） */
const px_byte* PX_StackGetBottomData(px_stack* pstack);
/** 移除栈底的数据包 */
px_void PX_StackRemoveBottom(px_stack* pstack);
#endif
