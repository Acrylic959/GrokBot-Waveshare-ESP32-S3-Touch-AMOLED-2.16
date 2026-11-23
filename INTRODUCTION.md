# 项目介绍 · GrokBot for Waveshare AMOLED216

> 一块会「看方向、认主人”的 AI 桌面宠物屏幕——把 M5Stack StopWatch 上大火的 GrokBot 动画，搬进微雪 2.16 寸 AMOLED 小智 AI 助手里。

## 这是什么

这是一份**第三方移植固件**，让微雪 `ESP32-S3-Touch-AMOLED-2.16` 开发板（一块 2.16 寸方形 AMOLED 触摸屏）运行 GrokBot 动画，并作为小智 AI 语音助手的可视化形象：

- 你说话时，它会用 28 种表情回应——聆听、思考、说话、提醒……
- 你把屏幕横过来、竖过来、倒过来，画面、字幕、状态栏**始终正立**；
- 长按 BOOT 键 2 秒即可锁屏，再长按解锁；
- 音频经过底层优化，对话更连贯，不再断断续续；
- 刷机不清除配网信息，开机即连，开箱即用。

它不是任何官方固件——是站在多个开源项目肩膀上的一次个人移植实验。所有引用与许可证都写在 [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) 里。

## 为什么做

M5Stack 的 StopWatch 上，GrokBot 这块「像素小怪物」屏幕很受欢迎，但它的固件只跑在 M5Stack 自家硬件上。微雪这块 2.16 寸 AMOLED 屏幕更大更清晰（480×480），却缺少这样的动态形象。

于是把两者接起来：以 `xiaozhi-esp32` 的 `dev-grokbot` 分支为代码基线，新建微雪板型，让 GrokBot 动画在小智固件里跑起来——再顺手解决这块板子实际使用中暴露的三个问题：**旋转方向反、声音卡顿、刷机后要重新配网**。

## 它能做什么

| 特性 | 说明 |
| --- | --- |
| 🎭 **GrokBot 动画** | 28 状态 / 360×360 灰度图集，跟随小智状态实时切换；常态 20FPS，对话时自动降到 8FPS 让出资源给音频 |
| 🔄 **四向自动旋转** | 板载 QMI8658 六轴 IMU 感知重力，横竖屏自动切换，状态栏 / 字幕 / 动画整体正立 |
| 🔒 **按键锁屏** | BOOT 长按 2 秒：暂停动画、熄屏、禁用触摸；再长按恢复 |
| 🔊 **音频优化** | I2S 中断入 IRAM、DMA 缓冲加深、播放队列 120ms→480ms、解码/输出任务绑核、Wi-Fi 省电在播放时关闭 |
| 📶 **保留配网** | 刷机只覆盖程序分区，保留 NVS 中的 Wi-Fi 凭据与设备 token，重启自动重连 |

## 硬件要求

- **开发板**：Waveshare ESP32-S3-Touch-AMOLED-2.16
- **主控**：ESP32-S3R8，16MB Flash / 8MB PSRAM
- **屏幕**：480×480 CO5300 QSPI AMOLED
- **触摸**：CST9217 / CST9220
- **IMU**：QMI8658（I2C，SDA=15 / SCL=14）
- **音频**：ES8311 输出 + ES7210 双麦输入
- **电源管理**：AXP2101

## 最快上手

```bash
pip3 install esptool pyserial
python3 tools/reconstruct-assets.py   # 还原仓库中的二进制文件
./tools/flash.sh                       # 刷机（保留 Wi-Fi 配网）
```

全新板子首次开机会进入配网热点 `Xiaozhi-XXXX`（`http://192.168.4.1`）。详细步骤见 [`README.md`](README.md)。

## 项目结构一览

```
firmware-src/   完整固件源码（基于 Ocean-lhy/xiaozhi-esp32 dev-grokbot）
releases/       预编译固件（整片镜像 + 分分区组件 + 校验和）
tools/          刷机 / 恢复 / 二进制还原脚本
docs/           板型说明、移植补丁、发布说明
encoded-assets/ 二进制文件的 base64 文本块
```

## 关于署名

本项目是**移植与增强**，不是上游官方仓库。它复用了：

- `78/xiaozhi-esp32`（MIT）固件主体
- `Ocean-lhy/xiaozhi-esp32` dev-grokbot 分支（GrokBot 显示）
- `Ocean-lhy/Stopwatch-GrokBot`（动画图集设计）
- `HerbertGao/Tokenmeter`（同板型旋转映射参考）
- 以及 ESP-IDF / LVGL 等框架

完整清单、精确 commit、许可证原文见 [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)。若发现遗漏署名，欢迎开 Issue。

## 免责声明

个人实验作品，与微雪电子、M5Stack、小智官方、GrokBot 原作者均无官方关联，不提供任何担保。刷机有风险，操作前请自行备份原厂固件。本项目新增代码以 MIT 发布（见 [`LICENSE`](LICENSE)）。
