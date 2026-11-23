# GrokBot AMOLED216 v7

预编译固件，适用于 **Waveshare ESP32-S3-Touch-AMOLED-2.16**（ESP32-S3R8 / 16MB Flash / 8MB PSRAM / 480×480 CO5300 AMOLED）。

## 本版本新增

- **动画帧率提升**：常态 20 → **30 FPS**（与 atlas 原生帧率一致，实测稳定 28.5 FPS）；音频活跃 8 → 15 FPS、省电 5 → 10 FPS。实测 render ≈ 34ms/帧、decode ≈ 15ms。动画任务固定在 core 0，音频解码/输出固定在 core 1，提帧不抢占音频。
- **BOOT 三击静音**：三击切换静音/恢复，自动记住静音前音量并写入 NVS（重启后仍生效），状态栏显示静音图标。
  - 按键总览：单击 = 对话开关 / 配网，双击 = AEC 切换（需编译时开启），**三击 = 静音**，长按 2 秒 = 锁屏。
- **旋转平滑过渡**：屏幕旋转时先淡入全屏黑幕 → 切换硬件旋转（MADCTL）→ 淡出，避免硬切闪烁（淡入 130ms / 淡出 200ms）。
- **配网 error 降噪**：OTA 版本检查前 3 次失败改为静默重试（仅日志，不再每次弹全屏错误提示），连续失败才提示用户。刚连 Wi-Fi 时 DNS 常未就绪（`getaddrinfo` 返回 `EAI_AGAIN`），此前会每次重试都弹错误。
- **代码精简**：合并反向播放的解码路径，去除冗余分支。

## 继承 v1–v6 的全部能力

- 四向自动旋转（QMI8658，状态栏/字幕/动画整体正立）
- BOOT 长按锁屏、音频链路全套优化（I2S ISR 入 IRAM、DMA 加深、播放队列 480ms、任务绑核、Wi-Fi 省电动态关闭）
- 刷机保留 NVS（Wi-Fi 凭据与设备绑定 token），开机自动重连

## 刷机

```bash
pip3 install esptool pyserial
./tools/flash.sh                       # 默认 /dev/cu.usbmodem2101，保留 Wi-Fi 配网
./tools/flash.sh /dev/cu.usbmodem2101  # 指定串口
```

脚本写入 bootloader / 分区表 / ota_data / app / assets，**保留 0x9000 起的 NVS**。全新板子首次开机会进入配网热点 `Xiaozhi-XXXX`（`http://192.168.4.1`）。

## 文件说明

| 文件 | 用途 |
| --- | --- |
| `grokbot_waveshare_amoled216_v7.bin` | v7 整片 16MB 合并镜像（推荐） |
| `grokbot_waveshare_amoled216_v6.bin` | 上一版本镜像（回滚用） |
| `parts/*.bin` | 分分区镜像，供 `tools/flash.sh` 保留 NVS 刷机 |
| `SHA256SUMS` | 全部镜像校验和 |

## 引用

详见仓库 [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md)。本项目是第三方移植增强，非上游官方仓库。
