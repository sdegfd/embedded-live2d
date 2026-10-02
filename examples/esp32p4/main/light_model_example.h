#pragma once

/**
 * @brief 启动 light.live 的 P4 正确性和帧率/CPU 测试任务。
 *
 * Returns:
 * - 无返回值；任务创建失败时记录错误。
 *
 * 备注：调用一次；测试结束后保持 13 轴动画循环。
 */
void l2d_light_example_start(void);
