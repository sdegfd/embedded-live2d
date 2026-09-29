/**
 * @file sys_storage.c
 * @brief 存储子系统实现
 *
 * 本模块实现系统的存储管理功能，包括：
 * - NVS（非易失性存储）初始化
 * - SD卡挂载
 * - 字体文件加载（LVGL FreeType字体）
 * - Live2D模型扫描和加载
 * - 通用文件读写（PSRAM分配）
 */

#include "sys_storage.h"

/* 标准库 */
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>     /* fileno / fsync / mkdir */

/* ESP-IDF / BSP */
#include "bsp/esp-bsp.h"
#include "dirent.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "nvs_flash.h"
#include "sys_sdio.h"

static const char *TAG = "sys_storage";    /**< 日志标签 */

static bool s_ready = false;           /**< 存储子系统是否就绪 */
static char s_mount_point[32];         /**< SD卡挂载点路径 */

typedef enum {
    SYS_FONT_KIND_ADAPTER_FT = 0,
    SYS_FONT_KIND_BINFONT,
} sys_font_kind_t;

typedef struct sys_font_node {
    lv_font_t *font;
    esp_lv_adapter_ft_font_handle_t ft_handle;
    sys_font_kind_t kind;
    char source[SYS_STORAGE_MAX_PATH];
    int size;
    uint16_t refs;
    struct sys_font_node *next;
} sys_font_node_t;

typedef struct {
    int size;
    char path[SYS_STORAGE_MAX_PATH];
    void *data;
    size_t bytes;
    bool probed;
} sys_font_cache_blob_t;

static sys_font_node_t *s_font_nodes;
static sys_font_cache_blob_t s_font_cache_blobs[] = {
    {.size = 16},
};

static void font_cache_probe_all(void);

/* ── 生命周期 ─────────────────────────────────────────── */

/** 初始化存储子系统：NVS初始化和SD卡挂载。 */
esp_err_t sys_storage_init(void)
{
    if (s_ready) {
        return ESP_OK;
    }

    /* NVS初始化 */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS needs erase, erasing...");
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs_flash_erase");
        ret = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "nvs_flash_init");

    /* SD卡挂载 */
    ESP_LOGI(TAG, "Mounting SD card at %s", BSP_SD_MOUNT_POINT);
    ret = bsp_sdcard_mount();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD card mount failed: %s", esp_err_to_name(ret));
        return ret;
    }

    strlcpy(s_mount_point, BSP_SD_MOUNT_POINT, sizeof(s_mount_point));
    s_ready = true;
    ESP_LOGI(TAG, "Storage subsystem ready (NVS + SD at %s)", s_mount_point);

    /* 初始化 SD 异步分块读取服务：所有大文件读串行排队，片间让出，降低 SDMMC/PSRAM
     * 访问与 DSI 帧缓冲刷新同一时间抢带宽造成蓝屏的概率。 */
    esp_err_t sdio_ret = sys_sdio_init();
    if (sdio_ret != ESP_OK) {
        ESP_LOGW(TAG, "sys_sdio_init failed: %s (fallback to direct fread)", esp_err_to_name(sdio_ret));
        /* 不致命：read_file 会在 sys_sdio 不可用时回退直接 fread */
    }

    font_cache_probe_all();

    return ESP_OK;
}

/* ── SD卡 ───────────────────────────────────────────── */

/** 检查存储子系统是否已就绪。 */
bool sys_storage_is_ready(void) { return s_ready; }

/** 获取SD卡挂载点路径。 */
const char *sys_storage_get_mount_point(void) { return s_mount_point; }

/* ── 字体 ──────────────────────────────────────────────── */

static bool build_font_cache_path(int size, char *out, size_t out_size)
{
    static const char *dirs[] = {
        "/sdcard/fonts",
        "/sdcard/game/fonts",
        "/sdcard",
    };

    if (!out || out_size == 0) {
        return false;
    }

    struct stat st;
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); ++i) {
        int n = snprintf(out, out_size, "%s/noto_sans_sc_%d.bin", dirs[i], size);
        if (n <= 0 || n >= (int)out_size) {
            continue;
        }
        if (stat(out, &st) == 0 && st.st_size > 0) {
            return true;
        }
    }

    out[0] = '\0';
    return false;
}

static sys_font_cache_blob_t *font_cache_blob_for_size(int size)
{
    for (size_t i = 0; i < sizeof(s_font_cache_blobs) / sizeof(s_font_cache_blobs[0]); ++i) {
        if (s_font_cache_blobs[i].size == size) {
            return &s_font_cache_blobs[i];
        }
    }
    return NULL;
}

static void font_cache_probe_all(void)
{
    for (size_t i = 0; i < sizeof(s_font_cache_blobs) / sizeof(s_font_cache_blobs[0]); ++i) {
        sys_font_cache_blob_t *blob = &s_font_cache_blobs[i];
        if (build_font_cache_path(blob->size, blob->path, sizeof(blob->path))) {
            struct stat st;
            long bytes = 0;
            if (stat(blob->path, &st) == 0) {
                bytes = (long)st.st_size;
            }
            ESP_LOGI(TAG, "font cache found: %s (%ld bytes)", blob->path, bytes);
        } else {
            ESP_LOGI(TAG, "font cache optional: noto_sans_sc_%d.bin not found on SD", blob->size);
        }
        blob->probed = true;
    }
}

static esp_err_t ensure_font_cache_blob(sys_font_cache_blob_t *blob)
{
    if (!blob) {
        return ESP_ERR_INVALID_ARG;
    }
    if (blob->data && blob->bytes > 0) {
        return ESP_OK;
    }
    if (!blob->path[0] && !build_font_cache_path(blob->size, blob->path, sizeof(blob->path))) {
        return ESP_ERR_NOT_FOUND;
    }

    void *data = NULL;
    size_t bytes = 0;
    esp_err_t ret = sys_storage_read_file(blob->path, &data, &bytes);
    if (ret != ESP_OK) {
        return ret;
    }
    blob->data = data;
    blob->bytes = bytes;
    ESP_LOGI(TAG, "font cache loaded to PSRAM: %s (%u bytes)",
             blob->path, (unsigned)blob->bytes);
    return ESP_OK;
}

static sys_font_node_t *find_font_node(const char *source, int size, sys_font_kind_t kind)
{
    for (sys_font_node_t *node = s_font_nodes; node; node = node->next) {
        if (node->kind == kind && node->size == size && strcmp(node->source, source) == 0) {
            return node;
        }
    }
    return NULL;
}

static lv_font_t *retain_font_node(sys_font_node_t *node)
{
    if (!node) {
        return NULL;
    }
    if (node->refs < UINT16_MAX) {
        node->refs++;
    }
    return node->font;
}

static esp_err_t register_font_node(lv_font_t *font, esp_lv_adapter_ft_font_handle_t ft_handle,
                                    sys_font_kind_t kind, const char *source, int size)
{
    sys_font_node_t *node = calloc(1, sizeof(*node));
    if (!node) {
        return ESP_ERR_NO_MEM;
    }
    node->font = font;
    node->ft_handle = ft_handle;
    node->kind = kind;
    node->size = size;
    node->refs = 1;
    strlcpy(node->source, source, sizeof(node->source));
    node->next = s_font_nodes;
    s_font_nodes = node;
    return ESP_OK;
}

static bool is_default_font_path(const char *path)
{
    return path && strcmp(path, CONFIG_SYS_STORAGE_FONT_PATH) == 0;
}

static lv_font_t *try_create_cached_binfont(const char *ttf_path, int size)
{
#if LV_USE_FS_MEMFS
    if (!is_default_font_path(ttf_path)) {
        return NULL;
    }
    sys_font_cache_blob_t *blob = font_cache_blob_for_size(size);
    if (!blob) {
        return NULL;
    }
    if (!blob->path[0] && !build_font_cache_path(size, blob->path, sizeof(blob->path))) {
        return NULL;
    }

    sys_font_node_t *existing = find_font_node(blob->path, size, SYS_FONT_KIND_BINFONT);
    if (existing) {
        return retain_font_node(existing);
    }

    if (ensure_font_cache_blob(blob) != ESP_OK) {
        return NULL;
    }
    if (blob->bytes > UINT32_MAX) {
        ESP_LOGE(TAG, "font cache too large: %s (%u bytes)", blob->path, (unsigned)blob->bytes);
        return NULL;
    }

    if (esp_lv_adapter_lock(-1) != ESP_OK) {
        ESP_LOGE(TAG, "lock LVGL failed before binfont create");
        return NULL;
    }
    lv_font_t *font = lv_binfont_create_from_buffer(blob->data, (uint32_t)blob->bytes);
    esp_lv_adapter_unlock();
    if (!font) {
        ESP_LOGE(TAG, "binfont create failed: %s", blob->path);
        return NULL;
    }

    if (register_font_node(font, NULL, SYS_FONT_KIND_BINFONT, blob->path, size) != ESP_OK) {
        if (esp_lv_adapter_lock(-1) == ESP_OK) {
            lv_binfont_destroy(font);
            esp_lv_adapter_unlock();
        }
        return NULL;
    }
    ESP_LOGI(TAG, "Font loaded from PSRAM binfont cache: %s (%d px)", blob->path, size);
    /* LVGL 已从 buffer 里拷贝了全部字形数据（glyph_bitmap/glyph_dsc/cmaps），
     * 原始 .bin 缓存不再需要，释放回收 ~2.8 MB PSRAM。后续同号字体请求
     * 直接走 find_font_node → retain_font_node，不依赖 blob。 */
    heap_caps_free(blob->data);
    blob->data = NULL;
    blob->bytes = 0;
    return font;
#else
    (void)ttf_path;
    (void)size;
    return NULL;
#endif
}

/** 从文件创建字体；优先 SD binfont 缓存，回退 esp_lvgl_adapter FreeType。 */
lv_font_t *sys_storage_font_create(const char *path, int size)
{
    if (!path || size <= 0) {
        ESP_LOGE(TAG, "Invalid font args: path=%s size=%d", path ? path : "NULL", size);
        return NULL;
    }

    lv_font_t *cached = try_create_cached_binfont(path, size);
    if (cached) {
        return cached;
    }

    sys_font_node_t *existing = find_font_node(path, size, SYS_FONT_KIND_ADAPTER_FT);
    if (existing) {
        return retain_font_node(existing);
    }

    const esp_lv_adapter_ft_font_config_t cfg =
        ESP_LV_ADAPTER_FT_FONT_FILE_CONFIG(path, (uint16_t)size,
                                           ESP_LV_ADAPTER_FT_FONT_STYLE_NORMAL);
    esp_lv_adapter_ft_font_handle_t handle = NULL;
    esp_err_t ret = esp_lv_adapter_ft_font_init(&cfg, &handle);
    if (ret != ESP_OK || !handle) {
        ESP_LOGE(TAG, "Failed to load font via esp_lvgl_adapter: %s (%d px): %s",
                 path, size, esp_err_to_name(ret));
        return NULL;
    }

    const lv_font_t *font_const = esp_lv_adapter_ft_font_get(handle);
    if (!font_const) {
        ESP_LOGE(TAG, "Invalid adapter font handle: %s (%d px)", path, size);
        esp_lv_adapter_ft_font_deinit(handle);
        return NULL;
    }

    lv_font_t *font = (lv_font_t *)font_const;
    ret = register_font_node(font, handle, SYS_FONT_KIND_ADAPTER_FT, path, size);
    if (ret != ESP_OK) {
        esp_lv_adapter_ft_font_deinit(handle);
        return NULL;
    }

    ESP_LOGI(TAG, "Font loaded via esp_lvgl_adapter FreeType: %s (%d px)", path, size);
    return font;
}

/** 兼容旧接口：后端由 sys_storage_font_create 决定。 */
lv_font_t *sys_storage_font_create_bitmap(const char *path, int size)
{
    return sys_storage_font_create(path, size);
}

/** 销毁或释放一次字体引用。 */
void sys_storage_font_destroy(lv_font_t *font)
{
    if (!font) {
        return;
    }

    sys_font_node_t **pp = &s_font_nodes;
    while (*pp) {
        sys_font_node_t *node = *pp;
        if (node->font != font) {
            pp = &node->next;
            continue;
        }
        if (node->refs > 1) {
            node->refs--;
            return;
        }

        *pp = node->next;
        if (node->kind == SYS_FONT_KIND_BINFONT) {
#if LV_USE_FS_MEMFS
            if (esp_lv_adapter_lock(-1) == ESP_OK) {
                lv_binfont_destroy(node->font);
                esp_lv_adapter_unlock();
            }
#endif
        } else {
            esp_lv_adapter_ft_font_deinit(node->ft_handle);
        }
        free(node);
        return;
    }

    ESP_LOGW(TAG, "font destroy ignored: unmanaged font pointer %p", font);
}

/** 使用 Kconfig 默认路径和字号加载 CJK 字体。 */
lv_font_t *sys_storage_font_get_default(void)
{
    return sys_storage_font_get_default_with_size(CONFIG_SYS_STORAGE_FONT_SIZE);
}

/** 使用 Kconfig 默认路径加载指定字号的 CJK 字体。 */
lv_font_t *sys_storage_font_get_default_with_size(int size)
{
    if (!s_ready) {
        ESP_LOGE(TAG, "Storage not ready, cannot load font");
        return NULL;
    }

    const char *path = CONFIG_SYS_STORAGE_FONT_PATH;
    if (!path || path[0] == '\0') {
        ESP_LOGE(TAG, "Font path not configured");
        return NULL;
    }

    if (is_default_font_path(path)) {
        sys_font_cache_blob_t *blob = font_cache_blob_for_size(size);
        if (blob && (blob->data || blob->path[0] ||
                     build_font_cache_path(size, blob->path, sizeof(blob->path)))) {
            return sys_storage_font_create_bitmap(path, size);
        }
    }

    struct stat st;
    if (stat(path, &st) != 0 || st.st_size <= 0) {
        ESP_LOGE(TAG, "Font file not found: %s", path);
        return NULL;
    }

    return sys_storage_font_create_bitmap(path, size);
}

/* ── Live2D模型 ──────────────────────────────────────── */

/** 检查文件是否为Live2D模型文件（通过魔术字判断）。 */
static bool is_live2d_model(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    char magic[SYS_STORAGE_MODEL_MAGIC_SIZE];
    bool ok = (fread(magic, 1, sizeof(magic), f) == sizeof(magic)) &&
              (memcmp(magic, SYS_STORAGE_MODEL_MAGIC, sizeof(magic)) == 0);
    fclose(f);
    return ok;
}

/** 从挂载点扫描并加载Live2D模型文件（通过魔术字识别）。 */
esp_err_t sys_storage_load_live2d_model(const char *mount_point,
                                         sys_storage_model_data_t *out)
{
    if (!mount_point || !out) return ESP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));

    /* 扫描模型文件 */
    DIR *dir = opendir(mount_point);
    if (!dir) {
        ESP_LOGE(TAG, "Cannot open directory: %s", mount_point);
        return ESP_ERR_NOT_FOUND;
    }

    char found_path[SYS_STORAGE_MAX_PATH] = {0};
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG && entry->d_type != DT_UNKNOWN) continue;

        char full[SYS_STORAGE_MAX_PATH];
        int len = snprintf(full, sizeof(full), "%s/%s", mount_point, entry->d_name);
        if (len < 0 || len >= (int)sizeof(full)) continue;

        if (is_live2d_model(full)) {
            strlcpy(found_path, full, sizeof(found_path));
            break;
        }
    }
    closedir(dir);

    if (!found_path[0]) {
        ESP_LOGE(TAG, "No Live2D model found under %s", mount_point);
        return ESP_ERR_NOT_FOUND;
    }

    /* 读取文件 */
    void *data = NULL;
    size_t size = 0;
    esp_err_t ret = sys_storage_read_file(found_path, &data, &size);
    if (ret != ESP_OK) return ret;

    strlcpy(out->path, found_path, sizeof(out->path));
    out->data = data;
    out->size = size;

    ESP_LOGI(TAG, "Live2D model loaded: %s (%u bytes)", out->path, (unsigned)out->size);
    return ESP_OK;
}

/** 释放Live2D模型数据。 */
void sys_storage_free_live2d_model(sys_storage_model_data_t *model)
{
    if (model) {
        sys_storage_free_file(model->data);
        memset(model, 0, sizeof(*model));
    }
}

/* ── 通用文件I/O ──────────────────────────────────── */

/** 读取文件的全部内容到PSRAM分配的缓冲区中。
 *  优先走 sys_sdio 串行分片读取；sys_sdio 不可用时回退直接 fread。 */
esp_err_t sys_storage_read_file(const char *path, void **data, size_t *size)
{
    if (!path || !data || !size) return ESP_ERR_INVALID_ARG;
    *data = NULL;
    *size = 0;

    struct stat st;
    if (stat(path, &st) != 0) {
        ESP_LOGE(TAG, "stat failed: %s", path);
        return ESP_ERR_NOT_FOUND;
    }
    if (st.st_size <= 0) {
        ESP_LOGE(TAG, "Empty or invalid file: %s", path);
        return ESP_ERR_INVALID_SIZE;
    }

    /* 目标缓冲仍在 PSRAM（调用方语义不变，返回 PSRAM 指针，调用方 free）。
     * 额外 +1 字节并写 '\0' 终止符：许多调用方（cJSON 解析、字符串处理）按文本访问，
     * 需要 NUL 结尾。分配恰好 st_size 会让调用方的 buf[size]='\0' 越界踩坏堆元数据。 */
    void *buf = heap_caps_malloc((size_t)st.st_size + 1,
                                  MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        ESP_LOGE(TAG, "malloc(%lld) failed for %s", (long long)st.st_size, path);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = sys_sdio_read_file(path, buf, (size_t)st.st_size, size);
    if (ret == ESP_OK) {
        ((uint8_t *)buf)[*size] = 0;   /* NUL 终止符（不越界，缓冲多 1 字节） */
        *data = buf;
        return ESP_OK;
    }
    if (ret != ESP_ERR_INVALID_STATE) {
        /* sys_sdio 已初始化但读取失败（非“未初始化”）：不回退，直接报错避免静默错误 */
        ESP_LOGE(TAG, "sys_sdio_read_file failed: %s (%s)", path, esp_err_to_name(ret));
        heap_caps_free(buf);
        return ret;
    }

    /* 回退：sys_sdio 未初始化，走直接 fread（旧路径） */
    ESP_LOGW(TAG, "sys_sdio unavailable, fallback direct fread: %s", path);
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "fopen failed: %s", path);
        heap_caps_free(buf);
        return ESP_ERR_NOT_FOUND;
    }
    size_t rd = fread(buf, 1, (size_t)st.st_size, f);
    fclose(f);
    if (rd != (size_t)st.st_size) {
        ESP_LOGE(TAG, "Short read: %s (%u/%lld)", path, (unsigned)rd, (long long)st.st_size);
        heap_caps_free(buf);
        return ESP_FAIL;
    }
    ((uint8_t *)buf)[rd] = 0;          /* NUL 终止符 */
    *data = buf;
    *size = rd;
    return ESP_OK;
}

/** 释放由sys_storage_read_file分配的文件数据缓冲区。 */
void sys_storage_free_file(void *data)
{
    if (data) heap_caps_free(data);
}

/* ── 文件写入 ──────────────────────────────────────── */

bool sys_storage_path_exists(const char *path)
{
    if (!path || !path[0]) {
        return false;
    }

    struct stat st;
    return stat(path, &st) == 0;
}

esp_err_t sys_storage_ensure_dir(const char *path)
{
    if (!path || !path[0]) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_ready) {
        ESP_LOGE(TAG, "storage not ready, cannot mkdir %s", path);
        return ESP_ERR_INVALID_STATE;
    }

    char buf[SYS_STORAGE_MAX_PATH];
    int n = snprintf(buf, sizeof(buf), "%s", path);
    if (n <= 0 || n >= (int)sizeof(buf)) {
        return ESP_ERR_INVALID_SIZE;
    }

    for (char *p = buf + 1; *p; p++) {
        if (*p != '/') {
            continue;
        }
        *p = '\0';
        (void)mkdir(buf, 0777);
        *p = '/';
    }

    (void)mkdir(buf, 0777);
    struct stat st;
    if (stat(buf, &st) != 0 || !S_ISDIR(st.st_mode)) {
        ESP_LOGE(TAG, "mkdir failed: %s", buf);
        return ESP_FAIL;
    }
    return ESP_OK;
}

/** 递归创建路径中的目录（忽略已存在的目录）。 */
static void ensure_parent_dirs(const char *path)
{
    if (!path || !path[0]) {
        return;
    }

    char buf[SYS_STORAGE_MAX_PATH];
    int n = snprintf(buf, sizeof(buf), "%s", path);
    if (n <= 0 || n >= (int)sizeof(buf)) {
        return;
    }

    /* 从第一个 '/' 之后逐级创建，跳过根目录。SD 卡路径形如 /sdcard/a/b/c.json */
    for (char *p = buf + 1; *p; p++) {
        if (*p != '/') {
            continue;
        }
        *p = '\0';                 /* 截断到当前目录层级 */
        (void)mkdir(buf, 0777);     /* 已存在时返回 -1 errno=EEXIST，忽略 */
        *p = '/';                   /* 恢复分隔符，继续下一级 */
    }
}

esp_err_t sys_storage_write_file(const char *path, const void *data, size_t size)
{
    if (!path || (!data && size > 0)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_ready) {
        ESP_LOGE(TAG, "storage not ready, cannot write %s", path);
        return ESP_ERR_INVALID_STATE;
    }

    ensure_parent_dirs(path);

    /* tmp 写入 + rename，避免写入中途掉电导致配置文件损坏。
     * FATFS 同目录 rename 是原子的（目录项一次性更新）。 */
    char tmp_path[SYS_STORAGE_MAX_PATH];
    int n = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    if (n <= 0 || n >= (int)sizeof(tmp_path)) {
        strlcpy(tmp_path, path, sizeof(tmp_path));  /* 路径过长 → 直接写目标（降级） */
    }

    FILE *f = fopen(tmp_path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "fopen(wb) fail: %s", tmp_path);
        return ESP_FAIL;
    }

    size_t wr = (size > 0) ? fwrite(data, 1, size, f) : 0;
    if (wr != size) {
        ESP_LOGE(TAG, "short write %s (%u/%u)", tmp_path, (unsigned)wr, (unsigned)size);
        fclose(f);
        return ESP_FAIL;
    }
    fflush(f);
    int fd = fileno(f);
    if (fd >= 0) {
        fsync(fd);                 /* 确保数据落盘，掉电不丢 */
    }
    fclose(f);

    if (strcmp(tmp_path, path) != 0) {
        /* FATFS f_rename 在目标已存在时会失败（FR_EXIST），先 unlink 目标。
         * tmp 文件已是完整内容并 fsync，删除旧目标后 rename 即可原子替换。 */
        (void)unlink(path);
        if (rename(tmp_path, path) != 0) {
            ESP_LOGE(TAG, "rename %s -> %s fail", tmp_path, path);
            /* rename 失败：tmp 文件已是完整内容，但目标未更新 → 报错让调用方知晓 */
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "write ok: %s (%u bytes)", path, (unsigned)size);
    return ESP_OK;
}

esp_err_t sys_storage_append_file(const char *path, const void *data, size_t size)
{
    if (!path || (!data && size > 0)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_ready) {
        ESP_LOGE(TAG, "storage not ready, cannot append %s", path);
        return ESP_ERR_INVALID_STATE;
    }

    ensure_parent_dirs(path);

    FILE *f = fopen(path, "ab");
    if (!f) {
        ESP_LOGE(TAG, "fopen(ab) fail: %s", path);
        return ESP_FAIL;
    }

    size_t wr = (size > 0) ? fwrite(data, 1, size, f) : 0;
    if (wr != size) {
        ESP_LOGE(TAG, "short append %s (%u/%u)", path, (unsigned)wr, (unsigned)size);
        fclose(f);
        return ESP_FAIL;
    }
    fflush(f);
    int fd = fileno(f);
    if (fd >= 0) {
        fsync(fd);
    }
    fclose(f);

    ESP_LOGI(TAG, "append ok: %s (%u bytes)", path, (unsigned)size);
    return ESP_OK;
}
