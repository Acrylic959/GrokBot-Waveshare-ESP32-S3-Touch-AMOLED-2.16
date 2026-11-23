# GrokBot AMOLED216 v21

预编译固件，适用于 **Waveshare ESP32-S3-Touch-AMOLED-2.16**（ESP32-S3R8 / 16MB Flash / 8MB PSRAM / 480×480 CO5300 AMOLED）。

![v20 宠物页预览（v21 视觉完全一致）](images/pet_ui_v20_rings_preview_v3.png)

v21 是**纯性能优化版本**：把角色图集只渲染到实际有内容的区域，界面样式、颜色、玩法与 v20 完全一致（视觉零变化）。v20 的详细改动见下方「历史」。

## 本版本变更：方向 3 — 静态 UI 分层 / 只渲染动态内容

GrokBot 的 360×360 图集里，每一帧的可见角色都落在固定的内容边界内，边界之外全黑。逐帧解码统计全部 28 个 clip 的所有帧，得到全局内容 bbox：

```text
x[54..305] y[26..311]  →  252×286
面积 252×286 / 360×360 = 55.6%  （比整帧少 44%）
```

改动：

- `image_descs_[i]` 从整帧 360×360 改为**子区域 descriptor**：`header.w/h = 252×286`，data 指针偏移到 `(y=26, x=54)`，但 `stride` 仍保留整帧的 720 字节（同一块 buffer，不拷贝像素）。
- `emoji_box_` 固定为整帧 360×360，角色 image 用 `LV_ALIGN_TOP_LEFT` 定位到 `(54, 26)`，因此屏幕上角色位置、大小与 v20 完全相同。
- LVGL 9 的 `lv_image_set_src()` 会 invalidate 整个 image 对象；对象从 360×360 缩到 252×286 后，**每帧的脏区、软件合成、QSPI 送屏面积减少约 44%**。
- 已核对 LVGL 9 解码器路径：`CONFIG_LV_DRAW_BUF_STRIDE_ALIGN=1` 使 stride 重排不触发，bin decoder 直接沿用自定义 stride 与 data 偏移，裁剪安全。

### 性能预期（基于 v20 实机基线）

v20 稳态日志（`laughing`）：`fps≈20，render≈49ms = decode≈36ms + present≈13ms`。

- 方向 3 只作用于 `present`（合成 + 送屏），**预计 present 下降**；`decode` 是解码/展开图集，不受影响。
- 由于 decode 占约 73%，整体 fps 提升幅度取决于 present 中合成/送屏的占比。

### 实机验证（2026-11-24 已刷入）

启动正常：`App version: 2.6.0-grokbot-v21`、`Atlas ready: 360x360`、`Pet panel ready`、无 `assert failed` / `Rebooting`，NVS 保留、Wi-Fi 自动重连、MQTT 连通。视觉与 v20 逐像素一致。

性能 A/B（抓取 `Animation fps` 日志，首页无宠物 UI 叠加、同状态 `notifying`、同时间点约 7 秒，最干净对照）：

| 指标 | v20 | v21 | 变化 |
| --- | --- | --- | --- |
| present | 6994 us | 3174 us | **-55%** |
| render | 32267 us | 23368 us | **-28%** |
| decode | 25190 us | 20115 us | -20%（波动，非裁剪贡献） |

- 稳态 `laughing`（首页）fps 从 v20 约 19–20 提升到 v21 约 22。
- 宠物页交互密集时（投喂/玩耍/连续摸角色）present 仍在 16–24ms、fps 18–22，因为角色脏区与底部按钮、环边 strip 重叠——这是 v20 已知的宠物页叠加瓶颈，方向 3 不解决它。
- decode 是主要瓶颈且不受裁剪影响，所以整体 fps 提升有限（notifying 单点 fps 未变，render 降 28% 但被 30fps 目标与调度掩盖）。

## 保留的玩法（与 v20 一致）

| 操作 | 效果 | 动画 |
| --- | --- | --- |
| Key3（GPIO18）单击 | 进入 / 退出宠物页 | — |
| 投喂 / 玩耍 / 清洁 / 探险 | 对应数值变化 | `happy` / `excited` / `shy` / `surprised` |
| 点击屏幕中央角色 | 抚摸（防刷分冷却） | `playful` |
| 宠物页静置 1 分钟 | 自动入睡 | `sleeping` |
| 摇一摇设备 | 头晕动画，按 `confused` clip 实际长度完整播放（首页 / 宠物页均生效） | `confused` |
| 语音唤醒助手 | 宠物页立即隐藏，回到主界面 | — |

## 刷机

```bash
pip3 install esptool pyserial
./tools/flash.sh /dev/cu.usbmodem2101                       # 保留 NVS（推荐）
./flash_grokbot_v21.sh /dev/cu.usbmodem2101                 # 工作区内专用脚本
./flash_grokbot_v21.sh /dev/cu.usbmodem2101 AUTO            # 跳过确认
```

整片镜像 `grokbot_waveshare_amoled216_v21.bin`（SHA256 `b98184bf95596b6a74977c46e425de2169e180dae07f96ca8ca0f6afc776353f`）可直烧 0x0（会清 NVS，适合全新板子）。

## 回滚

`./flash_grokbot_v20.sh`（工作区内，保留 NVS）。`releases/` 保留 v6–v21（跳过未发布的 v19）全部镜像与分分区组件。

## 历史

### v20 — 四层运动环宠物页

从四条属性条改为四层同心圆角矩形运动环（心情青 `#00E5FF` / 健康粉 `#FF5C8A` / 饱腹琥珀 `#FFC857` / 清洁绿 `#4ADE80`），无底轨道、未满圆头/满值闭合、等距圆角、环上无属性名；互动数值按对应环色显示，反馈字幕紧贴等级徽章下方。按钮 `76×56` 下移为中间角色留白。同期加入摇一摇头晕（完整播放 clip）、删除 BOOT 三击静音，并修复进页时的日志锁死锁与按键任务栈溢出。性能上把四环预光栅化为一张 ARGB8888 位图、再拆成上下左右四条边 image，减少透明层合成。

## 引用

玩法与界面设计参考 DigiCat、ESP32-TamaPetchi、ESP32-C6 Desk Pet、小鲸鲸鱼娘宠物等开源项目，详见 [THIRD_PARTY_NOTICES.md](https://github.com/Acrylic959/grokbot-amoled216/blob/master/THIRD_PARTY_NOTICES.md)。本项目是第三方移植增强，非上游官方仓库。
