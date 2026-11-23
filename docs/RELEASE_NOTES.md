# GrokBot AMOLED216 v11

预编译固件，适用于 **Waveshare ESP32-S3-Touch-AMOLED-2.16**（ESP32-S3R8 / 16MB Flash / 8MB PSRAM / 480×480 CO5300 AMOLED）。

本版本两大主题：**修复触屏互动失效** + **把电子宠物升级为完整养成系统**（参考 DigiCat / ESP32-TamaPetchi / ESP32-C6 Desk Pet / 小鲸鲸鱼娘等开源桌宠）。

## 本版本新增 / 变更

### 1. 修复触屏互动（关键）
v10 把点击回调注册在 `lv_screen_active()` 上，但 LVGL 事件默认不向父级冒泡，点击落在角色图片 / 状态栏等子控件时被吞掉，导致触屏互动完全无效。v11 改为**独立任务轮询 `lv_indev_get_state()` + `lv_indev_get_point()`**，直接读按压边沿与逻辑坐标，彻底绕开事件系统。实机日志确认 `Pet touch at (x,y)` 与对应互动一一触发。

### 2. 完整养成系统
- **六维数值**：心情 / 饱腹 / 活力 / 健康 / 清洁 / 乐趣，各自独立衰减；健康仅在某项告急时下降、各项良好时回升。
- **等级与成长**：每 10 次互动升 1 级；累计 30 分钟通电或 30 次互动进入成体（宝宝→幼体→成体）。
- **睡眠系统**：闲置 5 分钟或活力耗尽自动入睡，睡眠时精力快速恢复；醒来有唤醒表情。
- **探险事件**：双击触发 5 种随机事件（找到食物 / 新玩具 / 泥坑 / 跑远 / 交朋友），各有数值奖励或惩罚。
- **抚摸防刷分**：3 秒内连摸 3 次后进入冷却，避免刷数值。
- **待机小动作**：需求满足且闲置 90 秒后，轮流做伸懒腰 / 东张西望 / 哼歌（映射 GrokBot 表情）。
- **7 项成就**：首次投喂 / 玩耍 / 清洁、探险家、Lv.5、成体、50 次互动。
- **永不死亡**：长期无照顾后数值触底会在开机时复活到可玩基线，不会卡在昏睡状态。
- 全部状态 NVS 持久化，离线时间补偿衰减。

### 3. 互动方式（key 键 + 触屏并存）
- **触屏五分区**（纵向均分）：顶部=玩耍、20–40%=抚摸、40–60%=清洁、60–80%=投喂、底部=睡觉/唤醒。
- **触屏手势**：双击=探险、长按 1.2 秒=状态页。
- **BOOT 键**：单击对话、双击投喂、三击静音、长按锁屏。
- 分区按逻辑坐标判定，屏幕旋转后依然正确；锁屏时触屏自动禁用。

### 4. 稳定性
修复热刷机后 AXP2101 PMU 偶发 I2C NACK 导致 `ESP_ERROR_CHECK` 无限重启的问题：构造 PMU 前先探测 0x34 地址、失败则复位 I2C 总线并重试 5 次；仍失败则跳过 PMU 初始化（电源键禁用）而非崩溃。

## 继承 v1–v10 的能力

- v2.6.0 官方基线、AFE WakeNet `wn9_hiesp` 唤醒词
- 四向自动旋转（直接切换、无动画）、GrokBot 30 FPS 动画
- 音频链路全套优化 + 公共 DNS 固化
- 刷机保留 NVS（Wi-Fi 凭据、设备 token、宠物状态），自动配网

## 验证记录（实机）

- `App version: 2.6.0-grokbot-v11`；`Init AXP2101` 正常，无重启循环
- `Electronic pet ready: 心96 饱99 力25 健91 洁87 趣100`（含离线衰减与复活基线）
- `Pet touch ready (play/caress/clean/feed/sleep, double-tap=explore)`
- 触屏实测：`Pet touch at (173,238)`→Cleaned、`(143,136)`→Petted、`(121,471)`→Sleeping，五分区正确触发
- `Achievement: grown up`、`Level up -> 4` 养成系统正常
- `Model 0: wn9_hiesp`；`Static DNS ... readback=223.5.5.5`；`MQTT: Connected`；`Activation done`
- 自动连 Wi-Fi，无 `AP started`，无 E 级错误

## 刷机

```bash
pip3 install esptool pyserial
./tools/flash.sh                       # 默认 /dev/cu.usbmodem2101，保留 Wi-Fi 配网
./tools/flash.sh /dev/cu.usbmodem2101  # 指定串口
```

脚本写入 bootloader / 分区表 / app / assets，**保留 0x9000 起的 NVS**。全新板子首次开机会进入配网热点 `Xiaozhi-XXXX`（`http://192.168.4.1`）。

## 回滚

如需回到上一版本：`./flash_grokbot_v10.sh`（工作区内，保留 NVS）。发布目录内保留 v6–v11 镜像可回滚。

## 文件说明

| 文件 | 用途 |
| --- | --- |
| `grokbot_waveshare_amoled216_v11.bin` | v11 整片 16MB 合并镜像（推荐） |
| `grokbot_waveshare_amoled216_v10.bin` | v10 镜像（回滚用） |
| `grokbot_waveshare_amoled216_v9.bin` | v9 镜像（回滚用） |
| `grokbot_waveshare_amoled216_v8.bin` | v8 镜像（回滚用） |
| `grokbot_waveshare_amoled216_v7.bin` | v7 镜像（回滚用） |
| `grokbot_waveshare_amoled216_v6.bin` | v6 镜像（回滚用） |
| `parts/*.bin` | 分分区镜像，供 `tools/flash.sh` 保留 NVS 刷机 |
| `SHA256SUMS` | 全部镜像校验和 |

## 引用

完整养成玩法设计参考 DigiCat、ESP32-TamaPetchi、ESP32-C6 Desk Pet、小鲸鲸鱼娘宠物等开源项目，详见仓库 [THIRD_PARTY_NOTICES.md](https://github.com/Acrylic959/grokbot-amoled216/blob/master/THIRD_PARTY_NOTICES.md) 与 [README](https://github.com/Acrylic959/grokbot-amoled216/blob/master/README.md)。本项目是第三方移植增强，非上游官方仓库。上游小智固件来自 [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) v2.6.0。
