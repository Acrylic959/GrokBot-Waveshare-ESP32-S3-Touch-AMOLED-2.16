# 板型说明 · esp32-s3-touch-amoled-2.16-grokbot

## 硬件映射

| 功能 | GPIO / 参数 |
| --- | --- |
| 屏幕 | CO5300 QSPI AMOLED 480×480，PCLK 40MHz，无 TE 引脚 |
| 触摸 | CST9217，I2C，SDA=15 / SCL=14 |
| IMU | QMI8658，I2C 0x6B（备选 0x6A），SDA=15 / SCL=14 |
| PMU | AXP2101，I2C 0x34 |
| 音频 | ES8311 (DAC) + ES7210 (ADC)，I2S |
| BOOT 键 | GPIO0；单击对话/配网、双击 AEC、三击静音、长按 2000ms 锁屏 |

## 相对上游的改动

完整 diff 见 `port-changes.patch`。摘要：

### 新增板型
- `firmware-src/main/boards/waveshare/esp32-s3-touch-amoled-2.16-grokbot/`
  - `config.h` / `config.json` / `README.md`
  - `esp32-s3-touch-amoled-2.16-grokbot.cc`（主实现：IMU 旋转、锁屏、Wi-Fi 省电）
  - `assets/grokbot.atlas`（360×360 / 28 状态图集，来自上游 fork）

### 修改文件
| 文件 | 改动 |
| --- | --- |
| `main/Kconfig.projbuild` | 注册 `BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_AMOLED_2_16_GROKBOT` |
| `main/CMakeLists.txt` | 板型源码 / 字体 / assets / esp_rom 依赖 |
| `main/display/grokbot_display.cc/.h` | v7 帧率 30fps（音频活跃 15fps / 省电 10fps）、锁屏暂停动画、反向播放解码路径精简、诊断日志 |
| `main/application.cc` | v7 OTA 版本检查前 3 次失败静默（配网 error 降噪） |
| `main/audio/audio_codec.h` | 本板 I2S ISR IRAM、DMA 8×360（120ms）、中断优先级 3 |
| `main/audio/codecs/box_audio_codec.cc` | 应用 I2S 中断优先级配置 |
| `main/audio/audio_service.cc/.h` | 播放队列 2→8、opus 优先级 2→5、audio_output/opus_codec 绑核 core 1 |

## 版本历史

| 版本 | 主要变化 | SHA256（前 8 位） |
| --- | --- | --- |
| v2 | 初版 GrokBot 移植 | `eff52351` |
| v3 | 20fps 节流 + 自动旋转 + 长按锁屏 | `7f8e8dfd` |
| v4 | 旋转 +90° 校准 + I2S 硬件抗断续 | `8054e653` |
| v5 | Wi-Fi modem sleep 动态关闭 | `f97f09d0` |
| v6 | 旋转方向根治（全部正立）+ 音频播放链路第二轮优化 | `f462d950` |
| **v7** | **30 FPS 帧率提升 + BOOT 三击静音 + 旋转黑幕平滑过渡 + 配网 error 降噪 + 代码精简** | **`09407e58`** |

完整 SHA256 见 `releases/SHA256SUMS`。

## 旋转映射说明

`sw_rotate=0`，`lv_display_set_rotation()` 经 `LV_EVENT_RESOLUTION_CHANGED` 由 esp_lvgl_port 更新 CO5300 MADCTL，**整个 framebuffer 一起旋转**（文字与动画方向一致）。

QMI8658 重力四象限映射（同板型已验证）：

```
|ax| > |ay| :  ax > 0 → 0°      ax < 0 → 180°
|ay| >= |ax|:  ay > 0 → 90°     ay < 0 → 270°
|az| > 780mg（平放）→ 不旋转
轴向幅值 < 450mg → 不旋转（防抖：连续 3 次一致才生效）
```

v7 起旋转带平滑过渡：`lv_layer_top()` 上挂全屏黑幕，淡入 130ms → 切换 MADCTL → 淡出 200ms，`rotation_busy_` 标志防止过渡期间重复触发。
