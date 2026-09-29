/** @file @brief 日志输出、断言宏与函数声明 */

#ifndef __PX_LOG_H
#define __PX_LOG_H

#ifdef PX_DEBUG_MODE
/** 如果条件为真则触发断言 */
#define PX_ASSERTIF(x) do{if(!(x))break; PX_ASSERT();}while (1);
/** 如果条件为真则触发断言并附带日志 */
#define PX_ASSERTIFX(x,log) do{if(!(x))break; PX_ASSERT();}while (1);
/** 无条件触发断言并附带日志 */
#define PX_ASSERTX(log) PX_ASSERT()
#else
#define PX_ASSERTIF(x)          /* 空实现：非调试模式不执行任何操作 */
#define PX_ASSERTIFX(x,log)     /* 空实现：非调试模式不执行任何操作 */
#define PX_ASSERTX(x)           /* 空实现：非调试模式不执行任何操作 */
#endif

/** 标准输出打印宏，映射到 printf */
#define PX_printf printf

/** 日志输出函数声明 */
px_void PX_LOG(const px_char fmt[]);
/** 错误处理函数声明 */
px_void PX_ERROR(const px_char fmt[]);
/** 获取日志内容函数声明 */
px_char *PX_GETLOG(void);
/** 断言函数声明 */
px_void PX_ASSERT(void);
/** 空操作函数声明 */
px_void PX_NOP(px_void);
#endif
