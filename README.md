# GrokBot · Waveshare ESP32-S3-Touch-AMOLED-2.16

把 **GrokBot 动画角色** 移植到微雪 `ESP32-S3-Touch-AMOLED-2.16`（小智 AI 语音助手固件）上的第三方固件。带四向自动旋转、触屏互动的电子宠物、AFE 唤醒词、BOOT 按键快捷操作与音频断续优化，开箱即刷。

第一次来？先看 **[项目介绍](INTRODUCTION.md)** —— 这个文件讲清楚它是什么、为什么做、能做什么；本文件则是刷机与构建的技术手册。

> 本仓库是**移植与增强项目**，不是上述任一上游项目的官方仓库。所有原始代码的版权归各自作者所有，详见 [引用与致谢](#引用与致谢) 与 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

> 想要一个直接可刷的固件镜像？去 [Releases](https://github.com/Acrylic959/grokbot-amoled216/releases/tag/v12) 下载 `grokbot_waveshare_amoled216_v12.bin`，或按下方「刷机」步骤用分分区镜像保留 Wi-Fi 配网。

## 功能

- **基于官方 v2.6.0 基线**（v8 起）：整体合入上游 78/xiaozhi-esp32 v2.6.0 的协议层 / MCP / OTA / 网络管理最新代码，并适配其破坏性 API 变更。
- **AFE 唤醒词**（v10 起恢复）：使用官方 WakeNet `wn9_hiesp` 模型（v9 的 Multinet 自定义词已按反馈移除）。
- **完整养成电子宠物**（v11，参考 DigiCat / TamaPetchi / 小鲸）：**六维数值**（心情 / 饱腹 / 活力 / 健康 / 清洁 / 乐趣）+ 等级与经验 + **成长阶段**（宝宝→幼体→成体）+ **睡眠系统**（闲置 5 分钟自动入睡、精力耗尽自动打盹、醒来恢复）+ **探险随机事件** + **抚摸防刷分** + **待机小动作**（伸懒腰 / 东张西望 / 哼歌）+ **7 项成就**。宠物永不死亡，长期无照顾会自动复活到可玩基线。状态自动驱动 GrokBot 表情，NVS 持久化 + 离线衰减。
- **独立电子宠物模式**（v12）：首页保持纯净、不响应触摸；BOOT 双击进入专属宠物页面——顶部六维属性条 + 底部六个交互按钮，中间镂空露出**同一个** GrokBot 形象，动画全程连续不重建。点击按钮会播放对应动画（投喂→开心、玩耍→兴奋、抚摸→俏皮、清洁→害羞、探险→惊喜、睡觉→入睡）。语音唤醒助手时宠物页渐隐退回主界面。
- **旋转直接切换，无动画**（v10 起）：IMU 检测到的方向变化直接切换 MADCTL，不再有黑幕 / 翻页 / 淡入淡出过渡。
- **GrokBot 动画显示**：28 状态 / 360×360 图集，随小智状态（idle / listening / speaking / notifying …）切换表情；v7 起 30 FPS 满帧播放。
- **四向自动旋转**：板载 QMI8658 六轴 IMU，重力方向判定，状态栏 / 字幕 / 动画整体正立。
- **BOOT 按键**：单击对话/配网、双击投喂、三击静音、长按 2 秒锁屏。触屏提供全部宠物玩法，BOOT 只留最常用的投喂快捷键。静音为会话级（不写入 NVS）。
- **音频流畅度优化**：I2S ISR/控制函数入 IRAM、DMA 缓冲加深、播放队列 480ms、解码/输出任务独立核绑定、Wi-Fi 省电动态关闭。
- **DNS 固化根治音频断续**（v8）：联网后固定公共 DNS（223.5.5.5 / 119.29.29.29 / 8.8.8.8）并在 DHCP 完成后重设，解决 `api.tenclass.net` 解析失败导致的 TTS/MQTT 断流。
- **自动配网保留**：刷机不清除 NVS，已配网设备开机自动重连，无需重新配网；宠物状态也保存在同一 NVS 中。

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

刷机脚本只覆盖 bootloader / 分区表 / app / assets，**保留 0x9000 的 NVS**（Wi-Fi 凭据、设备绑定 token 与宠物状态）。首次刷入或全新板子会自动进入配网热点 `Xiaozhi-XXXX`（`http://192.168.4.1`）。

### 2. 与电子宠物互动

**首页**（v12）保持简洁：只显示状态栏、字幕和 GrokBot 动画，**不响应触摸**。所有宠物玩法都在独立页面里。

**进入 / 退出宠物页**：

| 操作 | 效果 |
| --- | --- |
| BOOT 双击 | 进入电子宠物模式（渐显；若宠物睡着会自动唤醒） |
| 语音唤醒助手 | 宠物页渐隐，自动回到主界面 |
| BOOT 双击（宠物页内） | 退出宠物模式 |

**宠物页界面**：顶部为标题（小何 Lv.X 成长阶段 · 互动次数）与六条实时属性条（心情 / 饱腹 / 活力 / 健康 / 清洁 / 乐趣），底部六个按钮，中间是持续流畅播放的 GrokBot 形象。

| 按钮 | 效果 | 触发动画 |
| --- | --- | --- |
| 投喂 | 饱腹 +25、心情 +8 | happy 开心 |
| 玩耍 | 心情 +20、乐趣 +25，消耗活力/饱腹 | excited 兴奋 |
| 抚摸 | 心情 +4（连续 3 次进入防刷分冷却） | playful 俏皮 |
| 清洁 | 清洁 +30、心情 +6 | shy 害羞 |
| 探险 | 随机事件（找到食物 / 新玩具 / 泥坑 / 交朋友等） | surprised 惊喜 |
| 睡觉 / 唤醒 | 切换睡眠状态，睡眠时精力快速恢复 | sleeping / waking |

> 动画形象是**同一个 LVGL 图像对象**，切换页面只淡入淡出覆盖层、不销毁重建，因此进宠物页、点按钮、唤醒返回的全过程中动画连贯流畅。

**BOOT 按键总览**：

| 操作 | 效果 |
| --- | --- |
| 单击 | 对话 / 配网 |
| 双击 | 进入 / 退出电子宠物模式 |
| 三击 | 静音 / 恢复（会话级，不写 NVS） |
| 长按 2 秒 | 锁屏 / 解锁 |

**养成规则**：六维数值每分钟衰减；健康仅在某项告急时下降、各项良好时回升。每 10 次互动升 1 级；累计 30 分钟通电或 30 次互动进入成体。闲置 5 分钟或活力耗尽会自动入睡；宠物永不死亡（长期无照顾复活到可玩基线）。数据持久化在 NVS，离线期间补偿衰减。

### 3. 从源码构建

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

### 4. 恢复原厂

见 [`tools/restore.sh`](tools/restore.sh)（需自行提供原厂备份镜像）。

## 目录结构

```
.
├── firmware-src/                # 小智固件源码（基于官方 v2.6.0 + 全部 GrokBot 定制）
│   └── main/boards/waveshare/esp32-s3-touch-amoled-2.16-grokbot/   # 本移植的板型代码
├── releases/                    # 预编译固件
│   ├── grokbot_waveshare_amoled216_v12.bin  # v12 整片合并镜像（推荐）
│   ├── grokbot_waveshare_amoled216_v11.bin  # v11 镜像（回滚用）
│   ├── grokbot_waveshare_amoled216_v10.bin  # v10 镜像（回滚用）
│   ├── grokbot_waveshare_amoled216_v9.bin   # v9 镜像（回滚用）
│   ├── grokbot_waveshare_amoled216_v8.bin   # v8 镜像（回滚用）
│   ├── grokbot_waveshare_amoled216_v7.bin   # v7 镜像（回滚用）
│   ├── grokbot_waveshare_amoled216_v6.bin   # v6 镜像（回滚用）
│   ├── SHA256SUMS                            # 各版本校验和
│   └── parts/                                # 分区组件（供保留 NVS 的分分区刷机）
├── tools/
│   ├── flash.sh                 # 保留 NVS 的刷机脚本
│   └── restore.sh               # 原厂恢复辅助脚本
├── docs/
│   ├── port-changes.patch       # 相对上游 fork commit 的移植改动（v1–v7 主体）
│   ├── BOARD.md                 # 板型说明与版本历史
│   └── RELEASE_NOTES.md         # v12 发布说明
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
| [DigiCat](https://github.com/aquascape123/digicat) | aquascape123 | 养成玩法设计参考（四维需求 / 睡眠 / 抚摸防刷分 / 永不死亡） | 见其仓库 |
| [ESP32-TamaPetchi](https://github.com/CyberXcyborg/ESP32-TamaPetchi) | CyberXcyborg | 养成玩法设计参考（状态追踪 / 成就 / 多阶段心情） | 见其仓库 |
| [ESP32-C6 Desk Pet](https://github.com/thefreelight/esp32_pet/) | thefreelight | 玩法设计参考（探索事件 / 小游戏 / 多种宠物） | 见其仓库 |
| [小鲸 · DeepSeek 鲸鱼娘宠物](https://github.com/jiashuxue/ai-passport-whale-pet) | jiashuxue | 同固件生态养成参考（自动入睡 / 待机小动作 / 三种抚摸反应 / 状态页） | MIT |
| [ESP-IDF](https://github.com/espressif/esp-idf) | Espressif Systems | 构建框架（v6.1.1） | Apache-2.0 |
| [LVGL](https://github.com/lvgl/lvgl) | LVGL K.K. | GUI 框架 | MIT |
| [esp-wifi-connect](https://github.com/78/esp-wifi-connect) | 78 | Wi-Fi 配网与管理组件 | MIT |

GrokBot 角色本身来自 [GrokBot 头像解剖](https://grokbot.yin.wiki/index.html)。

如果本项目中遗漏了任何应有的署名或许可声明，请开 Issue，我会立即补上。

## 免责声明

本项目为**个人移植实验作品**，与微雪电子、M5Stack、xAI、GrokBot 原作者及小智官方均无关联，不提供任何担保。刷机有风险，操作前请自行备份原厂固件。

## 许可证

本仓库中由本移植新增的代码（板型配置、旋转/锁屏逻辑、音频任务调整、刷机脚本、文档）以 [MIT License](LICENSE) 发布。上游代码的许可证以其各自仓库声明为准，见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
