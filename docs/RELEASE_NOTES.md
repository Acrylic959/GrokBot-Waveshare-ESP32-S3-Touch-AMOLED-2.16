# GrokBot AMOLED216 v12

预编译固件，适用于 **Waveshare ESP32-S3-Touch-AMOLED-2.16**（ESP32-S3R8 / 16MB Flash / 8MB PSRAM / 480×480 CO5300 AMOLED）。

本版本按实际使用反馈重做交互架构：v11 把宠物互动做成首页触屏分区，但点击只弹文字、看不到动画变化，被评价为"文字游戏"。v12 把宠物玩法收进**独立页面**，让每个按钮真正驱动一个不同的动画。

## 本版本新增 / 变更

### 1. 独立电子宠物模式页

- **首页取消触摸交互**：首页只保留状态栏、字幕和 GrokBot 动画，触摸不再触发任何宠物动作。
- **BOOT 双击进入宠物页**：顶部是标题（小何 Lv.X · 成长阶段 · 互动次数）与六条实时属性条（心情/饱腹/活力/健康/清洁/乐趣）；底部是六个交互按钮；中间完全镂空，露出正在播放的 GrokBot 形象。
- **进页自动唤醒**：若宠物处于睡眠状态，进入宠物页时自动唤醒，保证每个按钮立即可用（v11 中睡着时所有动作都会被拒绝，看起来像失灵）。
- **语音唤醒渐隐返回**：检测到 `LISTENING` 状态时宠物页淡出、主界面恢复，全程无需额外按键。

### 2. 按钮真正触发不同动画（核心修复）

v11 每次点击都会调用全局 `ShowNotification()`，使动画选择器在数秒内强制返回 `notifying`，把动作动画完全盖住——这就是"只有文字、没有动画"的根因。v12：

- 动作反馈改为宠物页内的局部文字，**不再抢占全局动画通道**；
- 宠物模式下的动画完全由 `PetState::SuggestEmotion()` 的动作状态驱动。

实机自检验证（6 个按钮依次点击后的动画切换）：

| 按钮 | 动画 | 日志证据 |
| --- | --- | --- |
| 投喂 | `happy` | `Pet action 0 -> 投喂成功` → `Animation -> happy` |
| 玩耍 | `excited` | `Pet action 1 -> 玩得开心` → `Animation -> excited` |
| 抚摸 | `playful` | `Pet action 2 -> 舒服~` → `Animation -> playful` |
| 清洁 | `shy` | `Pet action 3 -> 洗得香香的` → `Animation -> shy` |
| 探险 | `surprised` | `Pet action 4 -> 探险：发现新玩具` → `Animation -> surprised` |
| 睡觉 | `sleeping` | `Pet action 5 -> 晚安，Zzz` → `Animation -> sleeping` |

### 3. 动画全程连贯

GrokBot 形象是**同一个 LVGL 图像对象**，由独立动画任务持续解码渲染。切换页面只在它上层淡入/淡出一个透明覆盖面板，从不销毁或重建图像对象；进宠物页、点按钮、语音唤醒返回的全过程中动画不中断、不跳帧。

### 4. BOOT 按键

| 操作 | 效果 |
| --- | --- |
| 单击 | 对话 / 配网 |
| 双击 | 进入 / 退出电子宠物模式 |
| 三击 | 静音 / 恢复（会话级，不写 NVS） |
| 长按 2 秒 | 锁屏 / 解锁 |

## 继承 v1–v11 的能力

- 完整养成系统：六维数值 + 等级 + 成长阶段 + 睡眠 + 探险事件 + 抚摸防刷分 + 待机小动作 + 7 项成就，NVS 持久化与离线衰减，宠物永不死亡
- v2.6.0 官方基线、AFE WakeNet `wn9_hiesp` 唤醒词
- 四向自动旋转（直接切换、无动画）、GrokBot 30 FPS 动画
- 音频链路全套优化 + 公共 DNS 固化
- AXP2101 热刷机稳定性修复
- 刷机保留 NVS（Wi-Fi 凭据、设备 token、宠物状态），自动配网

## 验证记录（实机）

- `App version: 2.6.0-grokbot-v12`；`Init AXP2101` 正常，无重启循环
- `Pet panel ready (stats + 6 buttons, shared animation)`；`Electronic pet ready: ...`
- 六按钮自检：`Animation -> happy / excited / playful / shy / surprised / sleeping` 全部命中
- `Pet mode ON / OFF` 淡入淡出正常，动画对象未重建
- `Model 0: wn9_hiesp`；`Static DNS ... readback=223.5.5.5`；`MQTT: Connected`；`Activation done`
- 无 `AP started`（NVS 保留、自动连 Wi-Fi），无 E 级错误

## 刷机

```
pip3 install esptool pyserial
./flash_grokbot_v12.sh /dev/cu.usbmodem2101        # 保留 NVS（推荐）
```

整片镜像 `grokbot_waveshare_amoled216_v12.bin` 可直接烧录到 0x0（会清 NVS，适合全新板子）。分分区脚本保留 NVS，适合已有配网/宠物数据的设备。

## 回滚

如需回到上一版本：`./flash_grokbot_v11.sh`（工作区内，保留 NVS）。发布目录内保留 v6–v12 镜像可回滚。

## 引用

完整养成玩法设计参考 DigiCat、ESP32-TamaPetchi、ESP32-C6 Desk Pet、小鲸鲸鱼娘宠物等开源项目，详见仓库 [THIRD_PARTY_NOTICES.md](https://github.com/Acrylic959/grokbot-amoled216/blob/master/THIRD_PARTY_NOTICES.md) 与 [README](https://github.com/Acrylic959/grokbot-amoled216/blob/master/README.md)。本项目是第三方移植增强，非上游官方仓库。上游小智固件来自 [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) v2.6.0。
