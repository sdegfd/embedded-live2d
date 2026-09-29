# Live2D RT30 基线工程 · 交接文档

> 工程位置：`/home/ubuntu/esp/project/live2d_rt30_baseline/`
> 最近更新：2026-07-14
> 目标平台：ESP32-P4（wt99p4c5_s1_board），ESP-IDF 5.5.2

---

## 1. 项目目标

在隔离的 ESP32-P4 基线工程里验证 PainterEngine 新的 **RT30 实时多轴** Live2D 功能。验证模型 `esp.live`（只有实时功能，4 轴，无设置动画）。

验证通过后，**再**把合并后的内核移植回成熟工程 `live2d_demo`（单独任务，本次不在范围内）。

**RT30 = 30 个离散样本/轴，Q15 权重融合，NEUTRAL/TIMELINE/REALTIME30 三种模式互斥，局部变换（平移/旋转/缩放）通过 visual transform 叠加。**

---

## 2. 当前状态（一句话）

RT30 4 轴加载/求值/渲染/模式仲裁**全部跑通**，模型在屏内动起来（眨眼+轻微摇头+表情切换），但帧率只有 **~12fps（~80ms/帧）**，卡顿明显。下一步是性能优化（见 §6）。

---

## 3. 性能现状与瓶颈分析

### 3.1 实测 profile（最近一次烧录日志）

```
fps ≈ 11~14（平均 ~12.5）   cpu0=94%   cpu1=0%（第二核完全空闲）
total ≈ 76~88 ms/帧

render 阶段（=clear+engine）  59000~70000 us
  ├─ clear   6722 us  (CPU memset ARGB, fill=6714 sync=0, clear_mode=cpu)
  ├─ engine  56657 us (live2d_engine_render)
  │    ├─ vm        450 us
  │    ├─ physical  13000 us  (顶点/物理/插值，无像素写)
  │    └─ layer     38000 us  (sort=80, draw=37900)  ← 主要瓶颈
  │         ├─ back_hair   23000~27000 us (51 三角形/42 顶点/tex=1)  ← 单图层占一半
  │         ├─ front_hair  6200 us        (80 三角形/64 顶点/tex=10)
  │         ├─ face        3260 us        (17 三角形)
  │         ├─ cloth       2500 us        (31 三角形)
  │         └─ hairwear     890 us        (26 三角形)
convert（ARGB->RGB565 软件）  15337 us   ← 第二大瓶颈
flush                        1700 us
```

### 3.2 瓶颈本质

**一切开销都是 PSRAM 带宽受限**。整条管线在一个 320×320 的 ARGB8888 缓冲（PSRAM）上反复读写：

| 阶段 | PSRAM 流量/帧 | 耗时 |
|------|--------------|------|
| clear | 写 320×320×4 = 400KB | 6.7ms |
| 光栅化（back_hair 混合像素） | 每像素 纹理读4B + dst读4B + dst写4B = 12B | 24ms（~50k 像素 × ~470ns） |
| convert | 读 400KB + 写 200KB | 15.3ms |

back_hair ~470ns/像素 = ~170 cycle/像素（@360MHz），与"3 次 PSRAM 访问"吻合 → **带宽受限，不是算力受限**。软件微优化（跳透明、行指针）只能省几个 ms，无法质变。

### 3.3 帧预算目标

- 当前 80ms = 12fps
- 砍掉 convert（15ms）+ 光栅化 dst 减半（~10ms）+ clear 减半（3ms）≈ 50ms = **20fps**
- 再砍 physical（13ms）≈ 37ms = **27fps**

---

## 4. 渲染管线架构（当前）

```
main.cpp render loop (CPU0)
  │
  ├─ live2d_renderer_render_frame()
  │     ├─ clear_dirty_rect()          → CPU memset render_argb8888 全清 0
  │     ├─ live2d_engine_render()       → PX_LiveFrameworkRender()
  │     │     ├─ UpdatePhysical         (顶点插值/弹性/sin-cos 缓存)
  │     │     └─ 逐图层 RenderListRasterization → RenderAffinePixelShaderSpan
  │     │           (fastNearestSampling 快路径：定点最近邻 + ARGB alpha 混合，直写 render_argb8888)
  │     └─ convert_to_rgb565()          → 软件逐像素 ARGB8888 -> RGB565，写 frame_rgb565
  │
  └─ sys_display_flush_submit()         → C2M cache 写回 frame_rgb565，提交显示
```

### 画布几何
- 显示：**1024×600**（`BSP_LCD_H/V_RES`）
- 渲染画布 = 模型尺寸 = **320×320**（`choose_render_size` 取 `info.width/height`，clamp 到屏），居中放在 `(352,140)`
- `renderScale = 1.0`（`live2d_engine_set_render_scale`），`LIVE2D_ENGINE_PERF_RENDER_SCALE = 1.0f`
- dirty rect = 整张画布（`live2d_renderer_update_dirty_rect` 固定全清）

### 缓冲区分配（`components/sys/sys_display_buffer/sys_display_buffer.c`）
- `render_argb8888`：`heap_caps_aligned_alloc(64, w*h*4, SPIRAM|8BIT)` — 64 字节对齐，PSRAM
- `frame_rgb565`：`heap_caps_aligned_alloc(64, w*h*2, SPIRAM|8BIT)` — 64 字节对齐，PSRAM
- `dma_bounce`：内部 RAM，DMA 用

---

## 5. PPA 调查结论（重要）

### 现象
独立 canvas 路径下 PPA SRM（ARGB8888->RGB565 转换）会**挂起**：`use_ppa_convert=true` 时 `cpu0=0%`（render 任务阻塞在 PPA 信号量上）、`fps=0`。已在 `main.cpp:314-315` 用 `use_ppa_clear=false; use_ppa_convert=false;` 绕过，改走软件路径。

### 关键发现
1. **PPA 硬件本身没问题**：`components/espressif__esp_lvgl_adapter` 里用 `ppa_do_scale_rotate_mirror`（SRM）+ `ppa_do_fill`（FILL）跑得很正常（`ESP_ERROR_CHECK`，无回退）；`sys_display.c:263` 也注册了 PPA BLEND client。
2. **IDF 自带可工作范例**：`esp-idf/components/esp_driver_ppa/test_apps/main/test_ppa.c:593`（`ppa_srm_performance`）：
   - `scale_x = scale_y = 1.0f`（**不是** NO_SCALE 宏，1.0f 没问题）
   - `heap_caps_aligned_calloc(4, ..., SPIRAM|DMA)` — **只 4 字节对齐**，但带 `MALLOC_CAP_DMA`
   - `out.buffer_size = ALIGN_UP(w*h*bpp/8, 64)`
   - `PPA_TRANS_MODE_BLOCKING`，无显式 `esp_cache_msync`
   - 注意：该范例是 **ARGB->ARGB 同格式**，不是跨格式
3. **`ppa.h:137`**：`out.buffer_size` 要求"external memory align to L1 and L2 cache line size"。lvgl_adapter 用 `LVGL_PORT_PPA_ALIGNMENT=128`。

### 我们的 convert 与可工作模式的差异（疑似挂起原因）
| 项 | 我们（挂起） | IDF 范例/lvgl（正常） |
|----|------------|-------------------|
| 跨格式 | ARGB8888→RGB565 | ARGB→ARGB / RGB565→RGB565 |
| `MALLOC_CAP_DMA` | 否（只有 SPIRAM\|8BIT） | 是（SPIRAM\|DMA） |
| 对齐 | 64 | 4（但带 DMA cap）/ lvgl 128 |
| cache msync | **无 C2M（输入）/ 无 M2C（输出）** | 范例无，但 lvgl 走 draw buf |
| scale | 1.0f | 1.0f |

> 缺 C2M msync 会导致 PPA 读到 CPU cache 里的旧数据 → 输出错（"PPA mismatch" 警告就是这么来的），但这只导致**结果错**，不导致**挂起**。挂起更可能是 `MALLOC_CAP_DMA` 缺失 / 跨格式 / 缓冲不在 2D-DMA 可达区域。

### 风险警告
sys_display 与本渲染器**共享 PPA 硬件**。如果盲目重新启用 PPA SRM 仍然挂起，可能**连带冻住显示**（sys_display 的 BLEND 也用 PPA）。所以：
- **不要**直接 blocking 模式重新启用 PPA 试错。
- 若要试，先用 `PPA_TRANS_MODE_NON_BLOCKING` + `on_trans_done` 回调 + 信号量 + **超时**（如 50ms）包装，超时则永久回退软件路径。这样最坏只是回到当前 12fps，不会冻屏。

---

## 6. 进行中优化：RGB565 直写（接手即做）

**这是下一步要实现的核心优化，已设计完未编码。** 原理：显示本来就是 RGB565，当前却先渲染高精度 ARGB 再转换丢精度；直接把光栅化目标改成 RGB565，**省掉 convert（15ms）+ 光栅化 dst 带宽减半 + clear 减半**，无 PPA/挂起风险。

### 预期收益
| 阶段 | 当前 | RGB565 直写后 |
|------|------|--------------|
| clear | 6.7ms（400KB ARGB） | ~3.5ms（200KB 565） |
| physical | 13ms | 13ms（不变） |
| layer draw | 38ms（dst 4B） | ~28ms（dst 2B，省 ~33% 带宽） |
| convert | 15.3ms | **0**（直写） |
| flush | 1.7ms | 1.7ms |
| **合计** | ~75ms / 12fps | **~46ms / ~20fps** |

### 视觉影响（可接受）
- 显示终态本就是 RGB565，质量与当前"ARGB 渲染再转 565"基本等价。
- 多层半透明叠加边在 565 空间逐层混合，比 ARGB 一次合成多一点点色阶误差（banding），Live2D 通常可接受。
- "边缘太利"是**最近邻锯齿**问题（vs PC 双线性），与 565 无关，本次不动。

### 实现清单（共 5 个文件）

#### ① `PX_LiveFramework.h`（struct `_PX_LiveFramework`，`renderScale` 后面，约 238 行后）加两个字段：
```c
px_bool renderToRGB565;        /**< 渲染目标直写 RGB565（省 ARGB->RGB565 转换） */
px_uint16 *rgb565Target;       /**< RGB565 目标缓冲（renderToRGB565 为真时有效，stride=psurface->width） */
```
> `px_uint16` 已在 `PX_Typedef.h:215` 定义。`px_color._argb` 字段为 `{r,g,b,a}` 或 `{a,r,g,b}`（取决于 PX_COLOR_FORMAT_RGBA/BGRA 宏，现有代码统一用 `._argb.a/r/g/b` 访问，照搬即可）。

#### ② `PX_LiveFramework.c` `RenderAffinePixelShaderSpan`（245 行起）快路径内加 565 分支
位置：在 fast 路径的 fp 定点设置（`s_fp`/`t_fp`/`s_fp_step`/`t_fp_step` 算完）之后、现有 `if (!blend)` ARGB 循环（345 行）之前，插入：
```c
if (pLiveFramework->renderToRGB565 && pLiveFramework->rgb565Target)
{
    uint16_t *dst565 = (uint16_t *)pLiveFramework->rgb565Target
                       + xstart + (px_int)psurface->width * iy;
    if (!blend) {
        for (ix = xstart; ix < xend; ++ix, ++dst565) {
            tx = s_fp >> 16; ty = t_fp >> 16;
            color = PX_SURFACECOLOR(ptexture, tx, ty);
            px_uchar a = color._argb.a;
            if (a == 0) {
                /* 透明：跳过，dst 已清 0 */
            } else if (a == 0xff) {
                /* 不透明：直转 565 */
                *dst565 = (uint16_t)(((color._argb.r & 0xF8) << 8) |
                                     ((color._argb.g & 0xFC) << 3) |
                                     (color._argb.b >> 3));
            } else {
                /* 半透明：在 565 dst 上混合 */
                uint16_t d = *dst565;
                uint32_t dr = (d >> 11) & 0x1F, dg = (d >> 5) & 0x3F, db = d & 0x1F;
                dr = (dr << 3) | (dr >> 2);      /* 5->8 bit */
                dg = (dg << 2) | (dg >> 4);      /* 6->8 bit */
                db = (db << 3) | (db >> 2);      /* 5->8 bit */
                uint32_t ia = 256 - a;
                uint32_t orr = (a * color._argb.r + ia * dr) >> 8;
                uint32_t ogg = (a * color._argb.g + ia * dg) >> 8;
                uint32_t obb = (a * color._argb.b + ia * db) >> 8;
                *dst565 = (uint16_t)(((orr & 0xF8) << 8) |
                                     ((ogg & 0xFC) << 3) | (obb >> 3));
            }
            s_fp += s_fp_step; t_fp += t_fp_step;
        }
        return;
    }
    /* 带 HDR blend 的 565 路径（运行时不会走到，仅编辑器 focus 模式；为安全补全） */
    for (ix = xstart; ix < xend; ++ix, ++dst565) {
        tx = s_fp >> 16; ty = t_fp >> 16;
        color = PX_SURFACECOLOR(ptexture, tx, ty);
        px_int a = (px_int)(color._argb.a * blend->alpha);
        px_int r = (px_int)(color._argb.r * blend->hdr_R);
        px_int g = (px_int)(color._argb.g * blend->hdr_G);
        px_int b = (px_int)(color._argb.b * blend->hdr_B);
        a = a>255?255:(a<0?0:a); r = r>255?255:(r<0?0:r);
        g = g>255?255:(g<0?0:g); b = b>255?255:(b<0?0:b);
        /* 之后同上 a==0/0xff/半透明 三分支，dst 是 565 */
        ... /* 复用上面的 565 写逻辑 */
        s_fp += s_fp_step; t_fp += t_fp_step;
    }
    return;
}
```
> `uint16_t` 来自 `<stdint.h>`；`PX_LiveFramework.c` 顶部若未 include stdint，加一行 `#include <stdint.h>`。
> **关键**：运行时 RT30 路径（`fastNearestSampling=true`、`currentEditLayerIndex<0`、`showMesh=false`）只走 `!blend` 快路径（render list build 在 1584/1597 行调用，blend 传 `PX_NULL`）。所以只需保证 `!blend` 565 分支正确即可跑通。

#### ③ `live2d_engine.h` 加 API 声明
```c
/** 设置 RGB565 直写目标（target=NULL 关闭，回到 ARGB 模式）。必须在 render 前设置。 */
void live2d_engine_set_rgb565_target(live2d_engine_t *engine, uint16_t *target);
```

#### ④ `live2d_engine.c` 实现
```c
void live2d_engine_set_rgb565_target(live2d_engine_t *engine, uint16_t *target)
{
    if (!engine) return;
    engine->live.renderToRGB565 = (target != NULL) ? PX_TRUE : PX_FALSE;
    engine->live.rgb565Target = (px_uint16 *)target;
}
```

#### ⑤ `live2d_renderer.c` `live2d_renderer_render_frame` 改造
- 渲染前：`live2d_engine_set_rgb565_target(engine, buffer->frame_rgb565)`（当 `convert_rgb565` 为真时）。
- clear：565 模式下 memset `frame_rgb565`（2B/像素）而非 `render_argb8888`。新增 `live2d_renderer_clear_dirty_rect_cpu565` 或在现有 clear 里按模式分流。
- convert 阶段：565 模式下**直接跳过**（帧已是 565）。
- 注意 `render_surface`（ARGB）仍需保留并传给 `live2d_engine_render`，因为光栅化用它的 `width/height/limit_*` 做裁剪；只是 `surfaceBuffer`（ARGB）在 565 模式下不会被写（fast 路径已分流到 565）。

### 验证标准
- fps 升到 ~20，convert 耗时变 0，clear 减半。
- back_hair draw 从 ~24ms 降到 ~16ms。
- 模型视觉与之前基本一致（边缘锯齿不变，多层透明边可能略有色阶）。
- 无 task_wdt、无 Guru Meditation。

---

## 7. RT30 移植完成情况（已 DONE）

### 移植策略
**外科式把 PC 的 RT30 合并进 ESP 的 `PX_LiveFramework.{h,c}`**，保留 ESP 所有优化（fastNearestSampling、renderScale、sin/cos 缓存、快速像素着色器、ESP profiling）。**不是**用 PC 版覆盖。`PX_LiveRealtime.{h,c}` + `PX_LiveDeviceFormat.{h,c}` 从 PC 原样拷入（可移植）。

### Header（`PX_LiveFramework.h`）改动
- `#include "PX_LiveRealtime.h"`
- RT30 尾部常量、`PX_LIVE_ANIMATION_MODE` 枚举
- `PX_LiveFramework` 加 `animationMode` + `realtime` 字段
- `PX_LiveLayer` 加局部变换字段：`rel_begin/current/endLocalTranslation/RotationAngle/Scale`
- payload 加 `localTranslation/localRotation/localScaleOffset/localTransformMagic + reserve[26]`
- tagged struct `_PX_LiveFramework`、Update/RenderCurrent/realtime API 声明

### `.c`（`PX_LiveFramework.c`）改动
- `#include "PX_LiveDeviceFormat.h"`（修 `PX_LiveDeviceReader` 未知类型）
- Import：`plive->animationMode=NEUTRAL; PX_LiveRealtimeInitialize(&plive->realtime,mp);`（**修 NULL pool 崩溃**）
- UpdatePhysical：**始终运行** `UpdateLayerInterpolation`（**修 sin/cos 缓存不刷新导致渲染卡死**）
- 光栅化 `RenderListRasterization` 开头加大坐标守卫（`>100000.f` 跳过，**修异常量化顶点导致 y 循环空转卡死**）
- UpdateLayerVertices 弹性：`k=(mode==REALTIME30)?0:plv->k;`（**修 RT30 下图层漂移**）
- visual transform：`PX_LiveFramework_GetLayerVisualTransform` + `PX_PointRotate/Add/Mul`（从 PC 原样移植）
- 尾部解析器 `PX_LiveFrameworkImportRealtimeTrailer`（从 PC 原样移植）
- realtime 包装：`PX_LiveEnterRealtime30` 等薄封装
- Free/Reset/Play/PlayAnimation 里加 RT30 init/free/leave/reset 调用

### 历次修复记录（按出现顺序）
| 症状 | 根因 | 修复 |
|------|------|------|
| set-target 默认 esp32 | — | `idf.py set-target esp32p4` |
| `PX_LiveDeviceReader` 未知类型 | 缺 include | 加 `#include "PX_LiveDeviceFormat.h"` |
| main.cpp 聚合初始化顺序 | C++ 设计符顺序 | `.task_stack_size` 在 `.task_priority` 前 |
| main.cpp 取右值地址 | — | `make_display_config()` 存局部变量 |
| bootloader 太大（0x6070>0x6000） | INFO 日志 | `CONFIG_BOOTLOADER_LOG_LEVEL_NONE=y` |
| Guru Meditation in `PX_LiveRealtimePrepareRuntime`（MTVAL=0x1c） | Import 的 `PX_memset` 清零了 `realtime.mp` | Import 里重新 `PX_LiveRealtimeInitialize` |
| 渲染任务 watchdog（`RenderAffinePixelShaderSpan`） | (1) 跳过插值致 sin/cos 缓存失效 (2) 光栅化 y 循环不裁剪 | (1) 始终运行插值 (2) 大坐标守卫 |
| cpu0=0% fps=0 | 独立 canvas PPA SRM 挂起 | PPA 绕过（软件 clear+convert） |
| 图层漂移（"后发的图层会一直漂移"） | 弹性物理（distance²·dt）在大帧 dt 失稳 | RT30 模式 `k=0` 关弹性 |
| task_wdt（IDLE0 饿死） | 渲染 56ms>33ms 不让出 | 超时时 `vTaskDelay(1)` |
| 12fps 回退 + back_hair 20ms | 关弹性后暴露离屏顶点（demo 把轴推到极端采样） | 收窄轴范围（neck 15±4，face [15,12,15,18]） |

---

## 8. 关键文件清单

```
live2d_rt30_baseline/
├─ main/main.cpp                        demo 主程序：渲染循环、轴采样计算、PPA 绕过(314-315)
├─ sdcard/release.live                  = esp.live 模型（798660 字节，4 轴实时，无动画）
├─ components/
│  ├─ live2d_engine/
│  │  ├─ include/live2d_engine.h        引擎 API（含 RT30 接口）
│  │  ├─ live2d_engine.c                引擎封装；load() 日志轴的量化位宽
│  │  ├─ CMakeLists.txt                 SRCS 含 PX_LiveRealtime.c + PX_LiveDeviceFormat.c
│  │  └─ painterengine/kernel/
│  │     ├─ PX_LiveFramework.{h,c}      ★ 合并后的内核（RT30 + ESP 优化）
│  │     ├─ PX_LiveRealtime.{h,c}       RT30 求值器（从 PC 原样）
│  │     └─ PX_LiveDeviceFormat.{h,c}   设备格式（从 PC 原样）
│  ├─ live2d_renderer/
│  │  ├─ include/live2d_renderer.h      渲染器结构（PPA 句柄、统计字段）
│  │  └─ live2d_renderer.c              render_frame / clear / convert / PPA
│  ├─ sys/sys_display_buffer/           缓冲分配（64 对齐 PSRAM）
│  ├─ sys/sys_display/                  显示 + PPA BLEND overlay
│  └─ wt99p4c5_s1_board/                BSP（LCD 1024×600）
├─ sdkconfig / sdkconfig.defaults       CONFIG_BOOTLOADER_LOG_LEVEL_NONE=y 等
└─ README.md
```

---

## 9. 关键技术参数

| 项 | 值 |
|----|----|
| 模型 | `esp.live`，4 轴（left_eye/right_eye/neck/face），无设置动画 |
| 量化位宽 | coord=Q4, rotation=Q6, stretch=Q14（已验证） |
| `px_point32` | = `px_point` = {x,y,z} = 12 字节（payload 大小必须保留） |
| 轴 sample 范围 | 0~29（30 个离散样本） |
| 轴 weight | Q15，0~32767（WEIGHT_FULL_Q15 = 全权重） |
| 画布 | 320×320（模型尺寸），居中于 1024×600 |
| renderScale | 1.0（不可降，用户要求不缩小模型） |
| 内存 | 32MB PSRAM @200MHz，16MB flash；psram free ~13.4MB |
| 当前轴驱动 | neck 15±4 正弦(0.8s)，face [15,12,15,18] 每 5s 一档，眼 0↔29 眨眼(4s) |

---

## 10. 已知问题与后续优化路线

### 当前已知问题
1. **帧率低（~12fps）** — 见 §6，下一步做 RGB565 直写。
2. **边缘太利（最近邻锯齿）** — vs PC 双线性。ESP 用最近邻换速度。可选：快速 float 双线性着色器（~4× 最近邻开销，~13fps），或 renderScale<1.0+双线性（复杂，且用户不让缩小模型）。**优先级低于帧率。**
3. **PPA SRM convert 挂起** — 见 §5，短期不碰，RGB565 直写后 convert 步骤直接消失，PPA convert 自然作废。

### 优化路线（按收益排序）
1. **RGB565 直写**（§6）→ ~20fps。无风险，先做。
2. **physical 13ms 优化** → 顶点插值/sin-cos。RT30 只 4 轴变，多数图层或许不必全量重插值。引擎级改动，需调研 `UpdatePhysical`/`UpdateLayerInterpolation`。→ 再省 10ms，~25fps。
3. **PPA FILL 清屏**（565 模式下清 frame_rgb565）→ 若 PPA FILL 不挂（比 SRM 简单），省 ~3ms。需非阻塞+超时包装验证。
4. **CPU1 利用**（cpu1=0%）→ 把 clear 或下一帧 physical 放 CPU1 并行。复杂，收益有限（render 仍是瓶颈）。
5. **双线性抗锯齿** → 解决"边缘太利"，但掉帧。等帧率上去再权衡。

### 移植回 live2d_demo（最终，单独任务）
验证 OK 后，把合并后的 `PX_LiveFramework.{h,c}` + `PX_LiveRealtime.*` + `PX_LiveDeviceFormat.*` + realtime API 移植回成熟工程。注意 live2d_demo 用 overlay+PPA BLEND 路径（不走 SRM），PPA 在那边是工作的。

---

## 11. 构建烧录监控流程

```bash
cd /home/ubuntu/esp/project/live2d_rt30_baseline
. /home/ubuntu/esp/esp-idf/export.sh
idf.py build                  # 我（助手）负责编译
idf.py -p /dev/ttyACM0 flash monitor   # 用户负责烧录+监控
```

### 关键日志 tag
- `l2d_baseline`：`perf window=N fps=X eye=Y neck=Z face=W ... render=A convert=B flush=C total=D cpu0=E`
- `PX_LiveFramework`：`Profile avg: vm=.. physical=.. layer=..(sort=.. draw=..)` + `Layer topN: index=.. id='..' avg=..us triangles=.. vertices=.. texture=..`
- `live2d_renderer`：`Rendered Live2D frame=N ... avg_render=.. avg_clear=..(fill=.. sync=..) avg_engine=.. avg_convert=.. clear_mode=cpu/ppa`
- `sys_monitor`：`cpu0=..% cpu1=..% fps=.. mem int=.. psram=.. stack ..`

### 判读
- `convert` 应随 RGB565 直写降到 0。
- `back_hair` avg 应从 ~24ms 降到 ~16ms。
- `clear_mode=cpu` 且 clear fill 应减半。
- `cpu1` 仍 0%（本优化不用 CPU1）。
- 出现 `task_wdt` / `Guru Meditation` = 回归。

---

## 12. 工作约定

- **助手负责编译；用户负责烧录和监控**（"你只需要负责编译，烧录和监控我来"）。
- 改动后给出"本轮改了什么 + 预期 + 验证标准"，等用户烧录反馈。
- 不缩小模型（用户明确要求），renderScale 保持 1.0。
- 优先提高帧率（用户当前最关心），边缘锯齿问题次之。

### 相关记忆
- `~/.claude/projects/-home-ubuntu-esp-project-live2d-demo/memory/live2d-rt30-baseline.md`
```
该记忆记录了基线工程的目标与状态，与本交接文档互补。
```
