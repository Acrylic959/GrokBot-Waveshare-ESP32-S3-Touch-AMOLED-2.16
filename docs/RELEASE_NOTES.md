# GrokBot AMOLED216 v23

预编译固件，适用于 **Waveshare ESP32-S3-Touch-AMOLED-2.16**（ESP32-S3R8 / 16MB Flash / 8MB PSRAM / 480×480 CO5300 AMOLED）。

![宠物页预览](images/pet_ui_v20_rings_preview_v3.png)

v23 修复省电降亮度后点击屏幕崩溃的问题，并按要求把退出省电的方式收敛为「仅 PWR 单击」。界面样式不变。

## v23 变更

用户反馈：v22 在省电模式自动降亮度后点击屏幕会崩溃重启。

**根因**：v22 通过 `esp_lcd_touch_register_interrupt_callback()` 注册了触摸唤醒回调，而该函数内部用 `gpio_isr_handler_add()` 安装回调——它运行在 **GPIO 中断（ISR）上下文**。回调里直接调用 `WakeFromPowerSave()` → `SetPowerSaveMode()`（显示锁 + LVGL API）和 `RestoreBrightness()`（NVS 读写 + esp_timer + 面板 I2C 写），全部是**非 ISR-safe** 操作，省电降亮度后触摸立即触发崩溃。非省电时 `WakeUp()` 只重置空闲计数（极轻量），所以平时点击不崩——这正好解释了「只在省电时崩」。此外该注册还会覆盖 LVGL 自己的触摸中断回调（单槽位），干扰正常事件驱动触摸。

**修复**（按用户后续要求收敛交互）：

- 完全移除触摸中断唤醒回调及配套的静态实例指针；触摸不再唤醒省电。
- 移除 BOOT 单击、Key3 按下中的 `WakeFromPowerSave()`；这两个键不再唤醒省电。
- **只保留 PWR（SYS_OUT / GPIO16）单击** 调用 `WakeFromPowerSave()` 退出省电模式。
- `WakeFromPowerSave()` 仍保留锁屏保护：锁屏时直接返回，保持黑暗直到 BOOT 长按解锁。
- 保留 v22 全部功能（探险 10 事件、字幕去重、角色裁剪渲染、四层运动环、摇一摇头晕）。

**实机验证（2026-11-24）**：刷入 `App version: 2.6.0-grokbot-v23`，`Atlas ready` / `Pet panel ready` / `Touch panel initialized successfully` 全部正常，启动日志无 `assert failed` / `Rebooting`。省电触发后的触摸不再崩溃、PWR 单击唤醒需在电池供电下复现（见下方说明）。

镜像：`grokbot_waveshare_amoled216_v23.bin`（SHA256 `af177357d96260a823dfb6106d3fea04a2b6f27a15e72f521d1e711f167a85e7`）。刷机脚本 `./flash_grokbot_v23.sh`。

---

## v22 变更（历史）

v22 修复三个实机反馈问题，界面样式不变。其中「省电后任意触屏/按键唤醒」的 v22 实现存在上述 ISR 崩溃缺陷，已在 v23 移除并改为仅 PWR 单击。

## 本版本变更

### 1. 探险事件扩充到 10 种（PWR / SYS_OUT 键）

用户反馈：宠物页按 PWR（SYS_OUT / GPIO16）键几乎总是触发同一种探险事件。根因是旧版用 `esp_timer` 毫秒低 5 位做种子（`millis % 5`），连续按容易重复。

- 探险随机结果从 **5 种扩充到 10 种**：找到吃的 / 发现新玩具 / 滚进泥坑 / 跑了很远有点饿 / 交到新朋友 / 追蝴蝶 / 草地打滚 / 偷吃花蜜 / 大雨淋湿 / 秘密基地。
- 随机源改为 `esp_random()` 硬件随机数 ⊕ 递增计数器（Knuth 乘法常量），连续按很少重复。
- 每种事件配独立的有效动画（happy / excited / laughing / sad / scared / curious …）。

### 2. 探险字幕去重 + 删除「探险：」前缀

用户反馈：「滚进泥坑」出现两个「清洁 -12」，且文案带「探险：」前缀。根因：`Explore()` 文案里写了数值（`探险：滚进泥坑 -12清洁`），而 `ShowPetFeedbackDeltas()` 又根据属性差自动生成彩色数值标签「清洁 -12」，同一数值显示两次。

- `Explore()` 现在只返回纯描述（如「滚进泥坑啦」），不再内嵌数值。
- 数值统一由彩色 delta 标签显示一次，颜色对应四层运动环。
- 删除所有「探险：」前缀。

### 3. 省电降亮度后触屏 / 按键唤醒

用户反馈：省电模式自动降亮度后无法用触屏或按键恢复。根因：触摸输入与大部分按键路径没有调用 `PowerSaveTimer::WakeUp()`，只有 BOOT 解锁和系统状态切换才会恢复。

- 注册触摸中断回调 `TouchWakeupCb`，任意触屏 → `WakeFromPowerSave()`。
- BOOT 单击、Key3 按下、SYS_OUT 单击均调用 `WakeFromPowerSave()`。
- `WakeFromPowerSave()` 在锁屏时直接返回（锁屏保持黑暗，直到 BOOT 长按解锁），否则 `WakeUp()` → `ExitSleepMode()` → `RestoreBrightness()`。
- 已唤醒时 `WakeUp()` 只是重置空闲倒计时，开销可忽略。

### 4. 探险各事件配独立动画（附带修复）

实机验证时发现所有探险动画都固定为 `surprised`：`SuggestEmotion()` 里 `case 5` 硬编码，忽略了 `Explore()` 给每个事件设的 emotion。v22 让 `MarkAction()` 保存传入 emotion，探险分支返回它，现在不同事件配不同动画（happy / excited / sad / scared / laughing …）。

## 实机验证（2026-11-24）

- 刷入正常：`App version: 2.6.0-grokbot-v22`、`Atlas ready 360x360`、`Pet panel ready`，无 `assert failed` / `Rebooting`。
- **探险多样化已确认**：日志 `logs/grokbot_v22_interaction.log` 中连续 13 次 SYS_OUT 单击触发了 10 种不同事件（找吃的 / 新玩具 / 滚进泥坑 / 跑远有点饿 / 交到新朋友 / 追蝴蝶 / 草地打滚 / 偷吃花蜜 / 大雨淋湿 / 秘密基地），文案为纯描述、无重复数值。
- **省电唤醒**：代码路径已实现并编译通过（触摸中断回调 + BOOT/Key3/SYS_OUT 三键）；但插 USB 供电时 `battery_discharging_=false` 会禁用省电定时器，无法在当前连接状态复现，需拔掉 USB 用电池静置 60 秒后触摸验证。

## 性能与视觉

v22 继承 v21 的角色图集子区域裁剪（252×286 / 360×360，每帧送屏面积减约 44%），视觉零变化。v21 实机 A/B（首页 notifying 同状态）present 6994→3174us（-55%）。

## 刷机

```bash
pip3 install esptool pyserial
./tools/flash.sh /dev/cu.usbmodem2101                       # 保留 NVS（推荐）
./flash_grokbot_v23.sh /dev/cu.usbmodem2101                 # 工作区内专用脚本
./flash_grokbot_v23.sh /dev/cu.usbmodem2101 AUTO            # 跳过确认
```

整片镜像 `grokbot_waveshare_amoled216_v23.bin`（SHA256 `af177357d96260a823dfb6106d3fea04a2b6f27a15e72f521d1e711f167a85e7`）可直烧 0x0（会清 NVS，适合全新板子）。

## 回滚

`./flash_grokbot_v22.sh`（工作区内，保留 NVS）。`releases/` 保留 v6–v23（跳过未发布的 v19）全部镜像与分分区组件。

## 历史

### v21 — 静态 UI 分层 / 只渲染动态内容

角色图集从整帧 360×360 裁到内容 bbox 252×286（x=54,y=26），保留整帧 stride 与 data 偏移，视觉零变化。实机 A/B：首页 notifying present -55%、render -28%；decode 是主瓶颈且不受影响，整机 fps 提升有限。

### v20 — 四层运动环宠物页

四层同心圆角矩形运动环（心情青 / 健康粉 / 饱腹琥珀 / 清洁绿），无底轨道、未满圆头/满值闭合；互动数值按环色显示。加入摇一摇头晕（完整播放 clip）、删除 BOOT 三击静音，修复进页死锁与栈溢出。环预光栅化后拆成四条边 image 减少合成。

## 引用

玩法与界面设计参考 DigiCat、ESP32-TamaPetchi、ESP32-C6 Desk Pet、小鲸鲸鱼娘宠物等开源项目，详见 [THIRD_PARTY_NOTICES.md](https://github.com/Acrylic959/grokbot-amoled216/blob/master/THIRD_PARTY_NOTICES.md)。本项目是第三方移植增强，非上游官方仓库。
