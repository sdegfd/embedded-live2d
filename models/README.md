# 示例模型

这里只保存原始 PainterEngine `.live`（线性 RGBA 纹理）和 `live-inspect` 输出的 JSON。模型身份、尺寸、网格和轴名见 [manifest.json](manifest.json)。加载时纹理转为运行时的线性 BGRA，文件内容保持不变。

| 模型 | 文件字节数 | 画布 | 图层 / 纹理 | 顶点 / 三角形 | Timeline / RT30 轴 |
| --- | ---: | --- | --- | --- | --- |
| `esp.live` | 800,796 | 320×320 | 11 / 12 | 227 / 257 | 1 / 5 |
| `light.live` | 1,248,800 | 480×640 | 14 / 14 | 459 / 534 | 0 / 13 |

`light.live` 的 `head_raw` 轴名沿用模型里的拼写。眼睛、嘴巴的 0 和 29 样本均回到张开形态，15 样本用来检查中间形态。不要仅凭两个端点相同就认为轴没有动作。

```sh
python3 Live2D/tools/check_l2d_model_baseline.py --model esp
python3 Live2D/tools/check_l2d_model_baseline.py --model light
```

命令从仓库根目录执行；会核对大小和 SHA256。示例资源允许随仓库再分发，具体范围见 [LICENSE](LICENSE)。
