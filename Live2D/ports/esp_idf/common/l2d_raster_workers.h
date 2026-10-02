#ifndef L2D_RASTER_WORKERS_H
#define L2D_RASTER_WORKERS_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief 在渲染开始前创建常驻工作任务及同步对象。
 * Returns: true：可用或当前配置关闭；false：分配失败，已回收部分资源。
 * 备注：串行执行生命周期操作；成功后取得一个引用，调用方停止渲染后对应 shutdown。
 */
bool l2d_raster_workers_init(void);
/** @brief 释放本调用方引用，最后一个引用回收常驻任务；本调用方须已停止渲染。 */
void l2d_raster_workers_shutdown(void);
#ifdef __cplusplus
}
#endif
#endif
