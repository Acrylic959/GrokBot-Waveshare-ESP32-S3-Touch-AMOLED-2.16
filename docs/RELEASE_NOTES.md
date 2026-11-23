# GrokBot AMOLED216 v10

预编译固件，适用于 **Waveshare ESP32-S3-Touch-AMOLED-2.16**（ESP32-S3R8 / 16MB Flash / 8MB PSRAM / 480×480 CO5300 AMOLED）。

本版本按实际使用反馈，对 v9 做了三项「做减法」调整：恢复稳定唤醒词、去掉花哨的旋转动画、把宠物互动从 BOOT 按键移到触屏。

## 本版本新增 / 变更

- **回退唤醒词到 v8 的 AFE 方案**：移除 v9 的 Multinet 自定义唤醒词 `ni hao xiao he`（显示「小何」），恢复官方 WakeNet `wn9_hiesp` 模型（唤醒词「小智小智 / Hi,ESP」）。assets 随之从 7.45 MB 降回 5.54 MB，启动日志确认 `Set WakeNet Model: wn9_hiesp`、`AFE Pipeline: ... -> WakeNet(wn9_hiesp)`，且不再出现 `CustomWakeWord`。
- **删除所有旋转过渡动画**：移除 v9 的物理翻页（黑幕缩放 + 高光边缘 + 贝塞尔缓动）以及更早的黑幕淡入淡出。IMU 检测到方向变化后直接切换 MADCTL，画面即时旋转，无动画。相关 `lv_anim` / curtain / 缓动代码与 `ROTATE_FADE_*`、`ROTATE_FLIP_*` 参数全部清除。
- **电子宠物互动改为触屏**：取消 BOOT 双击投喂 / 四击玩耍；改为触屏按纵向三分区——上方=玩耍、中部=抚摸、下方=投喂。BOOT 键精简为单击对话、三击静音、长按 2 秒锁屏三个动作。分区按 LVGL 逻辑坐标判定，旋转后依然正确。锁屏时触屏互动自动禁用。

## 继承 v1–v9 的全部能力

- 电子宠物：心情 / 饱腹 / 活力三维状态，NVS 持久化 + 离线时间补偿衰减，状态自动驱动 GrokBot 表情
- 基于官方 v2.6.0 基线（协议层 / MCP / OTA / 设备状态机）
- 四向自动旋转（QMI8658，状态栏/字幕/动画整体正立）
- BOOT 单击对话 / 三击静音 / 长按锁屏
- 动画常态 30 FPS、音频活跃 15 FPS、锁屏暂停动画
- 音频链路全套优化（I2S ISR 入 IRAM、DMA 加深、播放队列 480ms、任务绑核提优先级、Wi-Fi 省电动态关闭）
- 公共 DNS 固化（223.5.5.5 / 119.29.29.29 / 8.8.8.8），根治 `api.tenclass.net` 解析失败导致的音频断续
- 刷机保留 NVS（Wi-Fi 凭据、设备绑定 token、宠物状态），开机自动重连

## 验证记录（实机）

- `App version: 2.6.0-grokbot-v10`
- `Electronic pet ready: ...` + `Pet touch interaction ready (top=play, middle=pet, bottom=feed)`
- `Model 0: wn9_hiesp` / `Set WakeNet Model: wn9_hiesp` / `WakeNet(wn9_hiesp)`，无 `CustomWakeWord`
- `Static DNS applied ... readback=223.5.5.5`、`MQTT: Connected to endpoint`、`Activation done`
- 自动连接 Wi-Fi `K3`（192.168.107.90），无 `AP started`；启动日志无 E 级错误

## 刷机

```bash
pip3 install esptool pyserial
./tools/flash.sh                       # 默认 /dev/cu.usbmodem2101，保留 Wi-Fi 配网
./tools/flash.sh /dev/cu.usbmodem2101  # 指定串口
```

脚本写入 bootloader / 分区表 / app / assets，**保留 0x9000 起的 NVS**。全新板子首次开机会进入配网热点 `Xiaozhi-XXXX`（`http://192.168.4.1`）。

## 回滚

如需回到上一版本：`./flash_grokbot_v9.sh`（工作区内，保留 NVS）。发布目录内保留 v6–v9 镜像可回滚。

## 文件说明

| 文件 | 用途 |
| --- | --- |
| `grokbot_waveshare_amoled216_v10.bin` | v10 整片 16MB 合并镜像（推荐） |
| `grokbot_waveshare_amoled216_v9.bin` | v9 镜像（回滚用，含自定义唤醒词与旋转动画） |
| `grokbot_waveshare_amoled216_v8.bin` | v8 镜像（回滚用） |
| `grokbot_waveshare_amoled216_v7.bin` | v7 镜像（回滚用） |
| `grokbot_waveshare_amoled216_v6.bin` | v6 镜像（回滚用） |
| `parts/*.bin` | 分分区镜像，供 `tools/flash.sh` 保留 NVS 刷机 |
| `SHA256SUMS` | 全部镜像校验和 |

## 引用

详见仓库 [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md)。本项目是第三方移植增强，非上游官方仓库。上游小智固件来自 [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) v2.6.0。
