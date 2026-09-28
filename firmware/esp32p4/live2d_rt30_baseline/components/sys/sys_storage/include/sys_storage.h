/**
 * @file sys_storage.h
 * @brief 系统存储模块 - 统一管理 NVS、SD 卡文件系统、字体和 Live2D 模型文件的加载
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── 生命周期 ─────────────────────────────────────────── */

/** 一次性初始化：初始化 NVS 并挂载 SD 卡。可安全多次调用——后续调用为空操作。 */
esp_err_t sys_storage_init(void);

/* ── SD 卡 ─────────────────────────────────────────────── */

/** 检查 SD 卡是否已就绪。 */
bool sys_storage_is_ready(void);

/** 获取 SD 卡挂载点路径。 */
const char *sys_storage_get_mount_point(void);

/* ── 字体管理 ──────────────────────────────────────────── */

/** 从指定路径加载字体。默认 CJK 字体的 16/20 号优先使用 SD 上的 LVGL binfont 缓存，其他字号走 esp_lvgl_adapter FreeType。 */
lv_font_t *sys_storage_font_create(const char *path, int size);

/** 兼容旧调用：等同于 sys_storage_font_create(path, size)。 */
lv_font_t *sys_storage_font_create_bitmap(const char *path, int size);

/** 销毁字体并释放资源。 */
void sys_storage_font_destroy(lv_font_t *font);

/** 使用 Kconfig 默认路径和字号加载 CJK 字体。
 *  等效于 sys_storage_font_create_bitmap(CONFIG_SYS_STORAGE_FONT_PATH, CONFIG_SYS_STORAGE_FONT_SIZE)。
 *  返回 NULL 表示 SD 卡未挂载或字体文件不存在。 */
lv_font_t *sys_storage_font_get_default(void);

/** 使用 Kconfig 默认路径加载指定字号的 CJK 字体。
 *  返回 NULL 表示 SD 卡未挂载或字体文件不存在。 */
lv_font_t *sys_storage_font_get_default_with_size(int size);

/* ── Live2D 模型管理 ──────────────────────────────────── */

/** Live2D 模型文件的魔数标识。 */
#define SYS_STORAGE_MODEL_MAGIC      "PainterEngineLiveDBinary"
#define SYS_STORAGE_MODEL_MAGIC_SIZE 24   /**< 魔数字节数 */
#define SYS_STORAGE_MAX_PATH         256  /**< 路径最大长度 */

/** Live2D 模型数据结构体。 */
typedef struct {
    void *data;                       /**< 模型二进制数据指针 */
    size_t size;                      /**< 模型数据大小 */
    char path[SYS_STORAGE_MAX_PATH];  /**< 模型文件路径 */
} sys_storage_model_data_t;

/** 在挂载点中扫描第一个 PainterEngine Live2D 模型文件并加载到内存。调用者需使用 sys_storage_free_live2d_model() 释放。 */
esp_err_t sys_storage_load_live2d_model(const char *mount_point,
                                        sys_storage_model_data_t *out);

/** 释放 Live2D 模型数据。 */
void sys_storage_free_live2d_model(sys_storage_model_data_t *model);

/* ── 通用文件 I/O ──────────────────────────────────────── */

/** 将整个文件读取到堆分配的缓冲区中。失败时 *data 设置为 NULL。使用 sys_storage_free_file() 释放。 */
esp_err_t sys_storage_read_file(const char *path, void **data, size_t *size);

/** 释放由 sys_storage_read_file() 分配的文件缓冲区。 */
void sys_storage_free_file(void *data);

/** 检查文件或目录是否存在。不会输出错误日志，适合可选配置文件探测。 */
bool sys_storage_path_exists(const char *path);

/** 递归创建目录，忽略已存在的目录。 */
esp_err_t sys_storage_ensure_dir(const char *path);

/**
 * 将数据写入文件（覆盖写入）。用于持久化用户配置（人设/语言等）到 SD 卡。
 * 调用前自动创建父目录。写入后 fsync 确保落盘，掉电不丢。
 *
 * @param path SD 卡文件路径
 * @param data 待写入数据
 * @param size 数据字节数
 * @return ESP_OK 成功，否则失败
 */
esp_err_t sys_storage_write_file(const char *path, const void *data, size_t size);

/**
 * 向文件末尾追加数据。用于 JSONL 记忆/session 日志。
 * 调用前自动创建父目录。写入后 fsync 确保落盘。
 */
esp_err_t sys_storage_append_file(const char *path, const void *data, size_t size);

#ifdef __cplusplus
}
#endif
