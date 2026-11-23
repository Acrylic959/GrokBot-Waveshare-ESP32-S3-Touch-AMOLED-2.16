# 板型说明 · esp32-s3-touch-amoled-2.16-grokbot

## 硬件映射

| 功能 | GPIO / 参数 |
| --- | --- |
| 屏幕 | CO5300 QSPI AMOLED 480×480，PCLK 40MHz，无 TE 引脚 |
| 触摸 | CST9217，I2C，SDA=15 / SCL=14 |
| IMU | QMI8658，I2C 0x6B（备选 0x6A），SDA=15 / SCL=14 |
| PMU | AXP2101，I2C 0x34 |
| 音频 | ES8311 (DAC) + ES7210 (ADC)，I2S |
| BOOT 键 | GPIO0；单击对话/配网、三击静音、长按 2000ms 锁屏（宠物互动已移到触屏） |
| 触屏 | CST9217；纵向三分区：上=玩耍、中=抚摸、下=投喂（v10 起） |

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
| `main/display/grokbot_display.cc/.h` | v7 帧率 30fps（音频活跃 15fps / 省电 10fps）、锁屏暂停动画、反向播放解码路径精简、诊断日志；v9 idle 时由宠物状态驱动表情 |
| `main/display/pet_state.cc/.h` | v9 新增电子宠物：心情/饱腹/活力三维状态、NVS 持久化、离线衰减、表情建议 |
| `main/application.cc` | v7 OTA 版本检查前 3 次失败静默（配网 error 降噪） |
| `main/audio/audio_codec.h` | 本板 I2S ISR IRAM、DMA 8×360（120ms）、中断优先级 3 |
| `main/audio/codecs/box_audio_codec.cc` | 应用 I2S 中断优先级配置 |
| `main/audio/audio_service.cc/.h` | 播放队列 2→8、opus 优先级 2→5、audio_output/opus_codec 绑核 core 1 |
| `main/boards/common/wifi_board.cc/.h` | v8 联网后固化公共 DNS + 3 秒后重设（防 DHCP 覆盖），根治 `api.tenclass.net` 解析失败 |
| `main/audio/audio_codec.cc/.h` | v8 静音会话级（新增 `SetOutputVolume(volume, persist)`），开机音量默认 70 |
| `main/protocols/*`, `main/mcp_server.*`, `main/ota.*` | v8 从上游 v2.6.0 全量合入，适配 `ProcessPending` / `AddClientCapabilities` / `AddSystemStatus` |
| `CMakeLists.txt` | 版本号（当前 v10：`2.6.0-grokbot-v10`） |
| `sdkconfig` / 板型 `config.json` | v9 曾改 Multinet 自定义唤醒词；v10 回退到 v8 的 AFE WakeNet `wn9_hiesp` |
| 板型 `config.h` | v10 删除 `ROTATE_FADE_*` / `ROTATE_FLIP_*` 动画参数 |
| 板型 `.cc` | v10 删除全部旋转动画、宠物互动改触屏三分区；v9 曾加入宠物按键与物理翻页动画 |
| `main/display/lcd_display.cc/.h` | v8 上游 `SpiLcdDisplay` 构造签名变更适配（移除 `buffer_lines` 参数） |

## 版本历史

| 版本 | 主要变化 | SHA256（前 8 位） |
| --- | --- | --- |
| v2 | 初版 GrokBot 移植 | `eff52351` |
| v3 | 20fps 节流 + 自动旋转 + 长按锁屏 | `7f8e8dfd` |
| v4 | 旋转 +90° 校准 + I2S 硬件抗断续 | `8054e653` |
| v5 | Wi-Fi modem sleep 动态关闭 | `f97f09d0` |
| v6 | 旋转方向根治（全部正立）+ 音频播放链路第二轮优化 | `f462d950` |
| v7 | 30 FPS 帧率提升 + BOOT 三击静音 + 旋转黑幕平滑过渡 + 配网 error 降噪 + 代码精简 | `09407e58` |
| v8 | 升级官方 v2.6.0 基线 + 固化公共 DNS 根治音频断续 + 音量修复 | `bcc53363` |
| v9 | 物理仿真旋转动画 + Multinet 自定义唤醒词「你好小何」+ 电子宠物（BOOT 组合键互动） | `f26f33db` |
| **v10** | **回退 AFE 唤醒词 + 删除所有旋转动画 + 宠物互动改触屏三分区** | **`95f2986e`** |

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

旋转本身是单次硬件 MADCTL 切换。**v10 起直接切换、无过渡动画**：v7 的黑幕淡入淡出与 v9 的物理翻页（黑幕缩放 + 高光 + 贝塞尔缓动）均已在 v10 删除，相关 `lv_anim` / curtain / 缓动参数全部移除。
