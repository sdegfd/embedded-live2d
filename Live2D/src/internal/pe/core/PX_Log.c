/** @file @brief 日志输出与断言工具函数实现 */

#include "PX_Typedef.h"


/** 断言：调试模式下触发除零中断并进入死循环 */
px_void PX_ASSERT(void)
{
#ifdef PX_DEBUG_MODE
	PX_ATOMIC px_int i = 0;
	i =i / i;
	//return;
#endif
	while (1);
}

/** 错误处理：调用断言后进入死循环 */
px_void PX_ERROR(const px_char fmt[])
{
	PX_ASSERT();
}


/** 获取日志缓冲区内容（空实现，返回空字符串） */
px_char * PX_GETLOG(void)
{
	return (px_char *)"";
}

/** 日志输出函数（空实现） */
px_void PX_LOG(const px_char fmt[])
{
	int errcode;
	errcode = 0;
}

/** 空操作 */
px_void PX_NOP()
{

}