# GrokBot AMOLED216 v8

预编译固件，适用于 **Waveshare ESP32-S3-Touch-AMOLED-2.16**（ESP32-S3R8 / 16MB Flash / 8MB PSRAM / 480×480 CO5300 AMOLED）。

本次是**基线大版本升级**：从上游小智 `v2.5.3-dev-grokbot` 整体切换到官方 **78/xiaozhi-esp32 v2.6.0**（177 个提交），并重新移植全部 GrokBot 定制与音频优化。

## 本版本新增

- **升级到官方 v2.6.0 基线**：全量合入上游协议层 / MCP / OTA / 设备状态机 / 网络管理等最新代码；适配上游的破坏性 API 变更（`Protocol::ProcessPending()` 接口、`Protocol::AddClientCapabilities()`、`Board::AddSystemStatus()`、`SpiLcdDisplay` 构造签名变更）。启动版本号显示 `2.6.0-grokbot-v8`。
- **固化公共 DNS，根治音频断续**（核心修复）：设备此前对 `api.tenclass.net` 的 DNS 查询持续失败（`getaddrinfo()` 返回 `EAI_AGAIN`），导致 TTS 音频流与 MQTT 云端链路无法正常建立，表现为"声音卡顿"。v8 在 Wi-Fi 连接后把 DNS 固定为 `223.5.5.5`（阿里）/ `119.29.29.29`（腾讯）/ `8.8.8.8`，并在 DHCP 租约完全 settles 后（3 秒）重设一次，防止 DHCP 下发的 DNS 覆盖。实测读回 `readback=223.5.5.5`，域名成功解析为 `47.112.110.152`，`MQTT Connected` + `Activation done`，云端音频链路完全打通。
- **音量体验修复**：三击静音改为会话级，不再把音量 0 写入 NVS；开机若读到静音遗留的 0，恢复为舒适音量 70（此前会退回近乎无声的 10，容易被误认为卡顿）。
- **保留 v7 全部能力**：30 FPS 动画、BOOT 三击静音、旋转平滑过渡、配网 error 降噪。

## 关于"最新固件"的重要说明

对比上游 v2.6.0 后确认：**上游音频核心代码相对本固件没有任何卡顿修复**——上游仍是 2 帧播放队列、优先级 4、不绑核，而本固件早已采用 8 帧队列（480ms）、优先级 6、audio/opus 绑核 core 1、120ms DMA 等更激进的抗断续优化。因此本次升级**保留**了这些定制优化（未被上游覆盖），音频卡顿的真正根因是设备侧 DNS，已在本版本根治。

## 继承 v1–v7 的全部能力

- 四向自动旋转（QMI8658，状态栏/字幕/动画整体正立）+ 旋转黑幕淡入淡出过渡
- BOOT 单击对话 / 双击 AEC / 三击静音 / 长按 2 秒锁屏
- 动画常态 30 FPS、音频活跃 15 FPS、锁屏暂停动画
- 音频链路全套优化（I2S ISR 入 IRAM、DMA 加深、播放队列 480ms、任务绑核提优先级、Wi-Fi 省电动态关闭）
- 刷机保留 NVS（Wi-Fi 凭据与设备绑定 token），开机自动重连

## 刷机

```bash
pip3 install esptool pyserial
./tools/flash.sh                       # 默认 /dev/cu.usbmodem2101，保留 Wi-Fi 配网
./tools/flash.sh /dev/cu.usbmodem2101  # 指定串口
```

脚本写入 bootloader / 分区表 / ota_data / app / assets，**保留 0x9000 起的 NVS**。全新板子首次开机会进入配网热点 `Xiaozhi-XXXX`（`http://192.168.4.1`）。

## 回滚

如需回到升级前版本：`./flash_grokbot_v7.sh`（工作区内，保留 NVS）。发布目录内保留 v7、v6 镜像可回滚。

## 文件说明

| 文件 | 用途 |
| --- | --- |
| `grokbot_waveshare_amoled216_v8.bin` | v8 整片 16MB 合并镜像（推荐，基于 v2.6.0） |
| `grokbot_waveshare_amoled216_v7.bin` | v7 镜像（回滚用） |
| `grokbot_waveshare_amoled216_v6.bin` | v6 镜像（回滚用） |
| `parts/*.bin` | 分分区镜像，供 `tools/flash.sh` 保留 NVS 刷机 |
| `SHA256SUMS` | 全部镜像校验和 |

## 引用

详见仓库 [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md)。本项目是第三方移植增强，非上游官方仓库。上游小智固件来自 [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) v2.6.0。
