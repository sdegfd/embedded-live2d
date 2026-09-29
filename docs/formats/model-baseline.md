# 正式模型基线

当前项目唯一正式基准模型是五轴 `esp.live`。

| 副本 | 路径 |
| --- | --- |
| 工作区 | `models/esp.live` |
| 设备 | `/sdcard/esp.live` |

这两份是同一个文件。正式开发、Host 测试、Benchmark、CRC、P4 真机验证都只用它。

## 身份

| 字段 | 值 |
| --- | --- |
| model_path | `models/esp.live` 或 `/sdcard/esp.live` |
| model_sha256 | `1d7f21471dcedee2d205904791df7147c63760269462ad5d7169a97afe386100` |
| model_size | 800796 |
| axis_count | 5（left_eye、right_eye、neck、face、mouth） |
| layer_count | 11 |
| vertex_count | 227 |
| triangle_count | 257 |
| texture_count | 12 |
| canvas | 320×320 |

开测前先算 SHA256。工作区与 SD 不一致时，正式测试立即停止，并报告两边的哈希。不允许改用其他模型继续。

禁止作为正式测试模型：`release.live`、历史四轴 `esp.live`（SHA256 `786b18e342f3a7b3e67042822f138824aaad610d6c46130f58f5c59bbb379b0b`，798660 字节）、其他 `.live`、临时导出的模型、任何替代 fixture。历史四轴只留在旧记录里，不进入 Host/P4 correctness、Benchmark、RT30、光栅 A/B、ROI、Model/Instance 内存或一致性结论。

Git 当前的 `models/esp.live` 已固定为正式五轴文件。旧四轴 blob 仅可从重构前的 Git 历史恢复。

## Host CRC

命令：`./build/host/l2d_test_runtime`。模型即上表。ARGB CRC 是主机软件光栅，不拿来代替 P4 的 RGB565 结论。

| Pose | ARGB CRC | hit | miss |
| --- | --- | --- | --- |
| STATIC | 6793bec7 | 29 | 22 |
| ALL_14_5 | 540d2664 | 29 | 22 |
| ALL_29 | 7a64b4ad | 29 | 22 |
| NECK_15 | 6793bec7 | 29 | 22 |

ALL_14_5 与 ALL_29 包含 mouth。NECK_15 只写 neck。

这一组 CRC 是软件光栅的对照基准，后续 SIMD、定点和其它 backend 都对着它。fast-nearest 内核拆到 `Live2D/src/raster/l2d_raster_nearest.c` 之后，Host 在 `-O2 -fno-strict-aliasing -ffp-contract=off` 下复测，上表不变。P4 的 459 行 RGB565 CRC 是这次拆分和浮点编译选项对齐之前的记录，不能用来判定当前固件。新的设备 CRC 要单独采集，并当作新基线。

## Host 内存

同一轮测得：model pool 预留 2650168 字节、实际使用 837184 字节；每个 instance pool 实际使用 34416 字节。纹理像素、三角形索引、动画帧和 RT30 样本在实例间共享。P4 正确性测试的重载步骤会暂时对同一个 `/sdcard/esp.live` 再加载一次，以检查旧实例仍存在时的行为；它不是第二个正式模型。

## 检查

`python3 Live2D/tools/check_l2d_model_baseline.py` 只核对工作区文件。带 `--device-sha` 或 `--metadata` 时，与设备打印的 SHA256 比较，不一致则退出码为 2。`examples/esp32p4/tools/capture_l2d_profile.py` 在采集前后都会跑这个检查。
