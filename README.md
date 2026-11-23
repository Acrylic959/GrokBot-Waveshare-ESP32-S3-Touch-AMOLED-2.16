# GrokBot · Waveshare ESP32-S3-Touch-AMOLED-2.16

把 **GrokBot 动画角色** 移植到微雪 `ESP32-S3-Touch-AMOLED-2.16`（小智 AI 语音助手固件）上的第三方固件。带四向自动旋转、BOOT 长按锁屏、音频断续优化，开箱即刷。

第一次来？先看 **[项目介绍](INTRODUCTION.md)** —— 这个文件讲清楚它是什么、为什么做、能做什么；本文件则是刷机与构建的技术手册。

> 本仓库是**移植与增强项目**，不是上述任一上游项目的官方仓库。所有原始代码的版权归各自作者所有，详见 [引用与致谢](#引用与致谢) 与 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

> 想要一个直接可刷的固件镜像？去 [Releases](../../releases/tag/v6) 下载 `grokbot_waveshare_amoled216_v6.bin`，或按下方「刷机」步骤用分分区镜像保留 Wi-Fi 配网。

## 功能

- **GrokBot 动画显示**：28 状态 / 360×360 图集，随小智状态（idle / listening / speaking / notifying …）切换表情；v7 起 30 FPS 满帧播放。
- **四向自动旋转**：板载 QMI8658 六轴 IMU，重力方向判定，状态栏 / 字幕 / 动画整体正立；v7 起旋转带黑幕淡入淡出的平滑过渡。
- **BOOT 按键操作**：单击对话/配网、双击 AEC（可选）、**三击静音**、长按 2 秒锁屏。静音音量写入 NVS，重启保留。
- **音频流畅度优化**：I2S ISR/控制函数入 IRAM、DMA 缓冲加深、播放队列 480ms、解码/输出任务独立核绑定、Wi-Fi 省电动态关闭。
- **自动配网保留**：刷机不清除 NVS，已配网设备开机自动重连，无需重新配网。

## 硬件要求

| 项目 | 参数 |
| --- | --- |
| 开发板 | Waveshare ESP32-S3-Touch-AMOLED-2.16 |
| 主控 | ESP32-S3R8（16MB Flash / 8MB PSRAM） |
| 屏幕 | 480×480 CO5300 QSPI AMOLED |
| 触摸 | CST9217 / CST9220（I2C） |
| IMU | QMI8658（I2C SDA=15 / SCL=14） |
| 音频 | ES8311 输出 + ES7210 双麦输入 |
| 电源管理 | AXP2101 |

串口：`/dev/cu.usbmodem2101`（macOS）/ `/dev/ttyUSB0`（Linux）。

## 快速开始

### 1. 刷机（使用预编译固件）

```bash
# macOS（默认串口 /dev/cu.usbmodem2101）
./tools/flash.sh

# 指定串口
./tools/flash.sh /dev/cu.usbmodem2101
```

需要 `python3` + `esptool` + `pyserial`：

```bash
pip3 install esptool pyserial
```

刷机脚本只覆盖 bootloader / 分区表 / app / assets，**保留 0x9000 的 NVS**（Wi-Fi 凭据与设备绑定 token）。首次刷入或全新板子会自动进入配网热点 `Xiaozhi-XXXX`（`http://192.168.4.1`）。

### 2. 从源码构建

```bash
# 1) 安装 ESP-IDF v6.1.x 并加载环境
source ~/esp/esp-idf-v6.1/export.sh

# 2) 拉取组件依赖（首次）
cd firmware-src
idf.py set-target esp32s3

# 3) 选择板型：CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_AMOLED_2_16_GROKBOT
idf.py menuconfig

# 4) 编译
idf.py build
```

板型定义位于 `firmware-src/main/boards/waveshare/esp32-s3-touch-amoled-2.16-grokbot/`。移植相对上游的精确改动见 [`docs/port-changes.patch`](docs/port-changes.patch)。

> 构建提示：若 `idf.py build` 因 `python` / `python3` 路径不一致报错，可用 `cmake --build build -- -j8` 绕过；此方式不会自动刷新 `merged-binary.bin`，需手动用 `esptool merge-bin` 合并（参数见 `firmware-src/build/flash_args`）。

### 3. 恢复原厂

见 [`tools/restore.sh`](tools/restore.sh)（需自行提供原厂备份镜像）。

## 目录结构

```
.
├── firmware-src/                # 小智固件源码（基于 Ocean-lhy/xiaozhi-esp32 dev-grokbot）
│   └── main/boards/waveshare/esp32-s3-touch-amoled-2.16-grokbot/   # 本移植的板型代码
├── releases/                    # 预编译固件
│   ├── grokbot_waveshare_amoled216_v7.bin   # v7 整片合并镜像（推荐）
│   ├── grokbot_waveshare_amoled216_v6.bin   # v6 镜像（回滚用）
│   ├── SHA256SUMS                            # 各版本校验和
│   └── parts/                                # 分区组件（供保留 NVS 的分分区刷机）
├── tools/
│   ├── flash.sh                 # 保留 NVS 的刷机脚本
│   └── restore.sh               # 原厂恢复辅助脚本
├── docs/
│   ├── port-changes.patch       # 相对上游 fork commit 的完整改动
│   ├── BOARD.md                 # 板型说明
│   └── RELEASE_NOTES.md         # v6 发布说明
├── .github/workflows/release.yml # 自动还原二进制并发布 Release
├── INTRODUCTION.md              # 项目介绍（面向访客）
├── THIRD_PARTY_NOTICES.md       # 第三方来源、commit、许可证全文引用
└── LICENSE                      # MIT（仅覆盖本仓库新增的移植代码）
```

## 引用与致谢

本项目站在以下开源项目肩上，按字母顺序列出（非穷尽，完整清单见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)）：

| 项目 | 作者 / 组织 | 用途 | 许可证 |
| --- | --- | --- | --- |
| [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) | Shenzhen Xinzhi Future Technology / 78 | 小智 AI 助手固件主体 | MIT |
| [Ocean-lhy/xiaozhi-esp32 (dev-grokbot)](https://github.com/Ocean-lhy/xiaozhi-esp32/tree/dev-grokbot) | Ocean-lhy | GrokBot 显示分支（fork commit `1e1df1d94bd4a984f899e80b951552f93da5d6ae`） | MIT（继承上游） |
| [Stopwatch-GrokBot](https://github.com/Ocean-lhy/Stopwatch-GrokBot) | Ocean-lhy | GrokBot 动画状态机 / 图集来源（commit `9d917f8dcbe48396c74145341a55c8f7b8492081`） | 见其仓库 |
| [xiaozhi-claude-pixel](https://github.com/xiaomoBoy/xiaozhi-claude-pixel) | xiaomoBoy | 像素小怪表情素材（commit `239d343a9e4c422018a268a5f0e9f535f186d3c9`） | MIT |
| [Tokenmeter](https://github.com/HerbertGao/Tokenmeter) | HerbertGao | QMI8658 四向旋转映射参考（同板型验证） | 见其仓库 |
| [ESP-IDF](https://github.com/espressif/esp-idf) | Espressif Systems | 构建框架（v6.1.1） | Apache-2.0 |
| [LVGL](https://github.com/lvgl/lvgl) | LVGL K.K. | GUI 框架 | MIT |
| [esp-wifi-connect](https://github.com/78/esp-wifi-connect) | 78 | Wi-Fi 配网与管理组件 | MIT |

GrokBot 角色本身来自 [GrokBot 头像解剖](https://grokbot.yin.wiki/index.html)。

如果本项目中遗漏了任何应有的署名或许可声明，请开 Issue，我会立即补上。

## 免责声明

本项目为**个人移植实验作品**，与微雪电子、M5Stack、xAI、GrokBot 原作者及小智官方均无关联，不提供任何担保。刷机有风险，操作前请自行备份原厂固件。

## 许可证

本仓库中由本移植新增的代码（板型配置、旋转/锁屏逻辑、音频任务调整、刷机脚本、文档）以 [MIT License](LICENSE) 发布。上游代码的许可证以其各自仓库声明为准，见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
