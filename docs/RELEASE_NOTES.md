# GrokBot AMOLED216 v13

预编译固件，适用于 **Waveshare ESP32-S3-Touch-AMOLED-2.16**（ESP32-S3R8 / 16MB Flash / 8MB PSRAM / 480×480 CO5300 AMOLED）。

v12 的宠物页有 three 个实机问题：属性面板挡住角色脸、只有 3 个按钮可见、语音唤醒没有渐隐。v13 逐项修复，并按你提供的原理图按键定义重新分配交互。

## 本版本修复 / 变更

### 1. 宠物页 UI 重构（不再挡画面、按钮更大更全）

- **属性条移到左右两侧**：v12 顶部 176px 高的面板遮住了角色上半身。v13 把 6 条属性条改成左右各 3 条 slim 条（128×50，半透明），**中间完全镂空**，GrokBot 全脸可见。
- **六个大按钮单行排布**：v12 用两行按钮且 Y 偏移为正值，第二行被推到屏幕外（只剩 3 个可见）。v13 改为底部单行 6 个 **66×66** 大按钮，X 居中分布在 [60,420] 安全区内，全部可见、触控面积更大。
- **标题紧凑化**：缩为顶部小胶囊，不再占用画面中央。

### 2. 按原理图按键定义重构交互

| 按键 | GPIO | 首页 | 宠物页 |
| --- | --- | --- | --- |
| BOOT | GPIO0 | 单击对话/配网、三击静音、长按锁屏 | 同左 |
| Key3（CHIP_PU） | GPIO18 | 单击进入宠物模式 | 单击/长按退出宠物模式 |
| SYS_OUT | GPIO16 | 单击静音 | 单击探险、双击睡觉/唤醒、长按退出 |

BOOT 上不再绑定双击（避免误触），进出宠物模式交给专用 Key3。辅助按键设为高电平有效，并加 5 秒开机保护，避免上电抖动误触锁屏。

### 3. 修复语音唤醒渐隐

**根因**：刷机复位后 BOOT/GPIO 引脚抖动触发了"长按锁屏"，`SetPetMode()` 开头的 `screen_locked_` 检查把渐隐直接拦截了——所以唤醒时宠物页不消失。

**修复**：
- 动画循环直接轮询 `requested_status_`，一旦是 `listening` 就强制退出宠物模式（不依赖 `SetStatus` 调用时机）；
- 辅助按键长按加开机 5 秒保护 + SYS_OUT 长按改为退出宠物模式（不再锁屏），消除误锁屏来源。

自检验证（随后已移除自检代码）：

```
=== V13 SELFTEST BEGIN ===
Pet mode ON (overlay fade-in, character stays live)
Pet action 0 -> 投喂成功
SELFTEST: simulate LISTENING status
Pet mode OFF (overlay fade-out, character stays live)
Voice wake-up: pet panel fading out to home
Animation -> listening
```

### 4. 继承 v12 的能力

- 六个按钮各驱动一种不同动画（happy / excited / playful / shy / surprised / sleeping）
- 进宠物模式自动唤醒睡着的宠物；动作反馈在面板内显示，不抢占全局动画
- 中间 GrokBot 形象始终是同一个对象，切页只淡入淡出覆盖层，动画连贯
- v2.6.0 官方基线、AFE `wn9_hiesp` 唤醒词、四向旋转、30 FPS、DNS 固化、AXP2101 稳定性、保留 NVS

## 验证记录（实机）

- `App version: 2.6.0-grokbot-v13`；`Pet panel ready`；`Electronic pet ready`
- 自检：进入宠物模式 → 投喂 → 模拟 LISTENING → `Pet mode OFF` + `Voice wake-up: ... fading out` → `Animation -> listening`
- 开机日志无 `Screen locked`（启动保护生效），无 `AP started`（自动配网），无 E 级错误
- `Static DNS ... readback=223.5.5.5`；`MQTT: Connected`；`Model 0: wn9_hiesp`

## 刷机

```
pip3 install esptool pyserial
./flash_grokbot_v13.sh /dev/cu.usbmodem2101        # 保留 NVS（推荐）
```

整片镜像 `grokbot_waveshare_amoled216_v13.bin` 可直烧 0x0（会清 NVS，适合全新板子）。

## 回滚

`./flash_grokbot_v12.sh`（工作区内，保留 NVS）。发布目录保留 v6–v13 镜像。

## 引用

玩法设计参考 DigiCat、ESP32-TamaPetchi、ESP32-C6 Desk Pet、小鲸鲸鱼娘宠物等开源项目，详见 [THIRD_PARTY_NOTICES.md](https://github.com/Acrylic959/grokbot-amoled216/blob/master/THIRD_PARTY_NOTICES.md)。本项目是第三方移植增强，非上游官方仓库。
