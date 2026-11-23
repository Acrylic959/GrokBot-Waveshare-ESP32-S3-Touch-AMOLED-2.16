# GrokBot AMOLED216 v6

预编译固件，适用于 **Waveshare ESP32-S3-Touch-AMOLED-2.16**（ESP32-S3R8 / 16MB Flash / 8MB PSRAM / 480×480 CO5300 AMOLED）。

## 本版本修复

- **四向自动旋转**：QMI8658 重力映射改为与同板型参考项目一致，状态栏、字幕、动画整体正立。
- **音频流畅度**：播放队列 2 → 8（120ms → 480ms 缓冲）、opus 解码优先级 2 → 5、`audio_output` / `opus_codec` 绑定 core 1。
- **前序修复继承**：I2S ISR/控制函数入 IRAM、DMA 缓冲加深、I2S 中断优先级提升、Wi-Fi modem sleep 在音频活跃时关闭。
- **保留配网**：刷机脚本不擦除 NVS，已配网设备开机自动重连。

## 刷机

```bash
pip3 install esptool pyserial
./tools/flash.sh                      # 默认 /dev/cu.usbmodem2101
./tools/flash.sh /dev/cu.usbmodem2101  # 指定串口
```

脚本写入 bootloader / 分区表 / ota_data / app / assets，**保留 0x9000 起的 NVS**（Wi-Fi 凭据与设备 token）。全新板子首次开机会进入配网热点 `Xiaozhi-XXXX`（`http://192.168.4.1`）。

## 文件说明

| 文件 | 用途 |
| --- | --- |
| `grokbot_waveshare_amoled216_v6.bin` | 整片 16MB 合并镜像（适合整片烧录，会清 NVS） |
| `parts/*.bin` | 分分区镜像，供 `tools/flash.sh` 保留 NVS 刷机 |
| `SHA256SUMS` | 全部镜像校验和 |

> 源码仓库中的二进制文件以 base64 文本块形式存放在 `encoded-assets/`，运行 `python3 tools/reconstruct-assets.py` 可还原；本 Release 的资产为还原后的原始二进制。

## 引用

详见仓库 [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md)。本项目是第三方移植增强，非上游官方仓库。
