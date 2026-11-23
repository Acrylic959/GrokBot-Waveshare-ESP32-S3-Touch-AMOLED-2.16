# GrokBot AMOLED216 v20

预编译固件，适用于 **Waveshare ESP32-S3-Touch-AMOLED-2.16**（ESP32-S3R8 / 16MB Flash / 8MB PSRAM / 480×480 CO5300 AMOLED）。

![v20 宠物页预览](images/pet_ui_v20_rings_preview_v3.png)

按最终确认的预览重做宠物页：从四条属性条改为**四层同心圆角矩形运动环**，环上不再写属性名，互动反馈的数值按对应环颜色显示。v19 为中间尝试版本，未发布。

## 本版本变更

### 1. 四层同心圆角矩形运动环

用四个 `lv_line` 对象绘制完整闭环，从外到内分别是：

| 环 | 属性 | 颜色 | 边界 / 圆角 |
| --- | --- | --- | --- |
| 1 | 心情 | 青色 `#00E5FF` | 16..464 / r30 |
| 2 | 健康 | 粉色 `#FF5C8A` | 28..452 / r25 |
| 3 | 饱腹 | 琥珀 `#FFC857` | 40..440 / r20 |
| 4 | 清洁 | 绿色 `#4ADE80` | 52..428 / r15 |

- 线宽统一 10px，圆角半径按 5px 递减，因此直线段和圆角处的间距一致。
- 没有深色底轨道；未填充处直接露出黑屏。
- 进度从顶部中点顺时针增长；未满时用圆头（`lv_obj_set_style_line_rounded`），**达到 99 以上时闭合为完整一圈**并去掉圆头，避免满状态看上去仍有一道缺口。
- 环上不再显示任何属性名称。
- 折线点按约 14px 步长采样（上限 160 点），保证满环绘制仍维持流畅帧率。

### 2. 反馈字幕与彩色数值

等级徽章上移到 `y=66` 且**去掉了边框与底色**（只留文字），反馈字幕下移到 `y=112` 与等级错开、不再重叠。互动后按动作前后的数值差生成彩色片段：

- 前缀为白色结果文字，例如「好吃！」。
- 每个变化的属性显示为对应环色，例如「饱腹 +25」（琥珀）、「心情 +8」（青）。
- 反馈显示 4 秒后自动隐藏，若助手字幕有内容则恢复显示助手字幕。

### 3. 布局调整

按钮改为 `76×56`、间距 8px，整体下移到 `y=348`，与最内层环完全分开。等级徽章、反馈字幕上移贴近内环，中间区域留给 GrokBot 角色。

### 4. 稳定性修复

- 首次实机抓日志时发现：进入宠物页时 `SetPetMode()` 在持有显示互斥锁的状态下打日志，动画任务又可能在持有日志锁时等待显示锁，触发 ESP-IDF 日志锁超时断言并重启。v20 把 `PetState::Wake()` 与所有 `ESP_LOGI()` 移出显示锁作用域。
- 复现后发现按键回调任务栈较小，而折线点/长度数组原本是函数内栈变量，进宠物页时栈溢出。改为受显示锁串行保护的静态缓存后，进页不再崩溃重启。
- 环的折线采样步长从 6px 放宽到 14px（点数上限 320→160），避免绘制拖慢到 4 FPS。
- 实机反馈去掉了宠物页的淡入淡出：进入 / 退出改为立即显示 / 隐藏覆盖层，消除切换时的过渡延迟。

## 保留的玩法

| 操作 | 效果 | 动画 |
| --- | --- | --- |
| Key3（GPIO18）单击 | 进入 / 退出宠物页 | — |
| 投喂 / 玩耍 / 清洁 / 探险 | 对应数值变化 | `happy` / `excited` / `shy` / `surprised` |
| 点击屏幕中央角色 | 抚摸（防刷分冷却） | `playful` |
| 宠物页静置 1 分钟 | 自动入睡 | `sleeping` |
| 语音唤醒助手 | 宠物页立即隐藏，回到主界面 | — |

## 实机验证记录

启动日志（`xiaozhi/logs/grokbot_boot_v20.log`）：

```text
I (106) app_init: App version:      2.6.0-grokbot-v20
I (240) WsAmoled216Grokbot: Button GPIO18 idle=1 -> active-low
I (260) WsAmoled216Grokbot: Button GPIO16 idle=0 -> active-high
I (1868) WsAmoled216Grokbot: Electronic pet ready: 心100 饱81 力92 健97 洁48 趣90
I (1955) GrokbotDisplay: Pet panel ready (4 concentric rounded rings, coloured deltas, subtitle under level)
I (4587) WifiBoard: Static DNS applied ... readback=223.5.5.5 (err 0/0/0 rb=0)
I (6255) MQTT: Connected to endpoint
```

- 无 `AP started` → NVS 保留，自动配网成功。
- 无 `assert failed` / `Rebooting` / E 级错误。
- 后续按实机照片修正了满环闭合、等级边框去除、反馈字幕与等级错开，并去掉淡入淡出；最终版已重新刷入并通过 Key3 进页验证：无 `assert failed` / `Rebooting`。
- 环的视觉细节与彩色反馈请按上方预览图在实机确认。

## 刷机

```bash
pip3 install esptool pyserial
./tools/flash.sh /dev/cu.usbmodem2101                       # 保留 NVS（推荐）
./flash_grokbot_v20.sh /dev/cu.usbmodem2101                 # 工作区内专用脚本
./flash_grokbot_v20.sh /dev/cu.usbmodem2101 AUTO            # 跳过确认
```

整片镜像 `grokbot_waveshare_amoled216_v20.bin`（SHA256 `9d51ac547fe4e3c921976361f30cab09cc32bc9a7ffad342eeee6c0d07ba2709`）可直烧 0x0（会清 NVS，适合全新板子）。

## 回滚

`./flash_grokbot_v18.sh`（工作区内，保留 NVS）。`releases/` 保留 v6–v20（跳过未发布的 v19）全部镜像与分分区组件。

## 引用

玩法与界面设计参考 DigiCat、ESP32-TamaPetchi、ESP32-C6 Desk Pet、小鲸鲸鱼娘宠物等开源项目，详见 [THIRD_PARTY_NOTICES.md](https://github.com/Acrylic959/grokbot-amoled216/blob/master/THIRD_PARTY_NOTICES.md)。本项目是第三方移植增强，非上游官方仓库。
