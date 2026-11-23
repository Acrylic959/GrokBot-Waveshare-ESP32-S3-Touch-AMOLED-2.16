# GrokBot AMOLED216 v14

预编译固件，适用于 **Waveshare ESP32-S3-Touch-AMOLED-2.16**（ESP32-S3R8 / 16MB Flash / 8MB PSRAM / 480×480 CO5300 AMOLED）。

v13 留下三个实机问题：Key3 要「双击并长按」才出宠物页、松开又消失；静音只显示提示但仍有声音；宠物页信息太多、太像仪表盘。v14 逐项修复，并把宠物页重绘成宝可梦风格。

## 本版本修复 / 变更

### 1. Key3 单击切换宠物页（根因：有效电平配置错误）

- 开机时用内部上拉采样 GPIO18 / GPIO16 的空闲电平，自动推断有效电平：
  - 空闲为低 → 按键按下拉高 → `active-high`
  - 空闲为高 → 按键按下拉低 → `active-low`
- 实机日志确认：
  ```text
  I (239) WsAmoled216Grokbot: Button GPIO18 idle=1 -> active-low
  I (259) WsAmoled216Grokbot: Button GPIO16 idle=0 -> active-high
  ```
  v13 把 Key3 固定配成 `active-high`，而实际硬件是 `active-low`，所以一次物理点击被解释成「释放+按下」一对边沿，表现为需要双击/长按。
- Key3 改为监听**按下沿**（`OnPressDown`）切换宠物模式，不再绑定 `OnClick` / `OnLongPress`。一次正常单击即进入，松开后状态保持；再单击退出。

### 2. 静音真正生效（根因：调用了不下发硬件的重载）

`AudioCodec` 有两个重载：

```cpp
virtual void SetOutputVolume(int volume);          // BoxAudioCodec 覆盖：写 ES8311 硬件 + NVS
virtual void SetOutputVolume(int volume, bool persist);  // 基类：只改内存 + 可选 NVS
```

v13 的 `ToggleMute()` 调用了 `SetOutputVolume(0, false)`，只改了 `output_volume_`，**从未调用到 `BoxAudioCodec::SetOutputVolume()`**，所以界面显示 Muted，扬声器却仍在出声。

v14 改为调用单参虚函数：

```cpp
codec->SetOutputVolume(0);      // 静音：硬件音量 0
codec->SetOutputVolume(restored); // 恢复：硬件音量回到原值
```

日志判定：`Muted (hardware volume 0, saved volume ...)` + `Set output volume to 0 (persist=1)`。若上次关机时留在静音状态，`AudioCodec::Start()` 会把读到的 0 自动恢复为 70，避免重启后近乎无声。

### 3. 宝可梦风格宠物页重绘

- **顶部**：只显示 `Lv.X · 体型 成长阶段`（如 `Lv.12 · 体型 成体`）。删掉名字「小何」和互动次数。
- **四条属性条组成完整环**：
  - 心情 → 上边（青色，360×12）
  - 健康 → 右边（粉色，12×330，竖向）
  - 饱腹 → 下边（琥珀色，360×12）
  - 清洁 → 左边（绿色，12×330，竖向）
  - 四角用小圆角连接点收口，视觉上形成一个闭环。
- **只保留这四项**：活力 / 乐趣仍在后台养成逻辑里参与计算，但不再占用屏幕上四个位置。
- **操作按钮**：六个 84×52 大圆角按钮按 3×2 网格居中排布，彩色描边区分类型；比 v13 的 66×66 单行更宽松，触控区更大。
- **角色动画不中断**：中间仍是同一个 `emoji_image_`，切页只淡入淡出覆盖层；六个按钮继续分别触发 happy / excited / playful / shy / surprised / sleeping。
- 语音唤醒仍会强制宠物页渐隐回主界面。

## 实机验证记录

启动日志（`xiaozhi/logs/grokbot_boot_v14.log`）：

```text
I (106) app_init: App version:      2.6.0-grokbot-v14
I (239) WsAmoled216Grokbot: Button GPIO18 idle=1 -> active-low
I (259) WsAmoled216Grokbot: Button GPIO16 idle=0 -> active-high
I (1867) WsAmoled216Grokbot: Electronic pet ready: 心87 饱59 力73 健100 洁87 趣30
I (1958) GrokbotDisplay: Pet panel ready (Lv+body badge, 4 edge bars, 6 action buttons)
I (4585) WifiBoard: Static DNS applied ... readback=223.5.5.5 (err 0/0/0)
I (5949) MQTT: Connected to endpoint
I (7596) WifiBoard: Static DNS applied ... readback=223.5.5.5 (err 0/0/0)
```

- 无 `AP started` → NVS 保留，自动配网成功。
- 无 `ESP_ERROR_CHECK` / `Rebooting` / `abort` / E 级错误。
- Key3 与物理静音需按键实测；代码路径与有效电平已由日志确认。

## 刷机

```bash
pip3 install esptool pyserial
./flash_grokbot_v14.sh /dev/cu.usbmodem2101        # 保留 NVS（推荐）
./flash_grokbot_v14.sh /dev/cu.usbmodem2101 AUTO   # 跳过确认
```

整片镜像 `grokbot_waveshare_amoled216_v14.bin`（SHA256 `783161184cfad17746ea4cd5b2295addfa79d2d1567197c43f066bf6620c3ae0`）可直烧 0x0（会清 NVS，适合全新板子）。

## 回滚

`./flash_grokbot_v13.sh`（工作区内，保留 NVS）。`releases/` 保留 v6–v14 全部镜像与分分区组件。

## 引用

玩法与界面设计参考 DigiCat、ESP32-TamaPetchi、ESP32-C6 Desk Pet、小鲸鲸鱼娘宠物等开源项目，详见 [THIRD_PARTY_NOTICES.md](https://github.com/Acrylic959/grokbot-amoled216/blob/master/THIRD_PARTY_NOTICES.md)。本项目是第三方移植增强，非上游官方仓库。
