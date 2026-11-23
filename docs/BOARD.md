# 板型说明 · esp32-s3-touch-amoled-2.16-grokbot

## 硬件映射

| 功能 | GPIO / 参数 |
| --- | --- |
| 屏幕 | CO5300 QSPI AMOLED 480×480，PCLK 40MHz，无 TE 引脚 |
| 触摸 | CST9217，I2C，SDA=15 / SCL=14 |
| IMU | QMI8658，I2C 0x6B（备选 0x6A），SDA=15 / SCL=14 |
| PMU | AXP2101，I2C 0x34 |
| 音频 | ES8311 (DAC) + ES7210 (ADC)，I2S |
| BOOT 键 | GPIO0；单击对话/配网、三击静音、长按 2000ms 锁屏 |
| Key3 | GPIO18（原理图标注 CHIP_PU）；单击进入/退出宠物模式（v14 开机自动检测有效电平，按下沿触发，不再需要双击/长按） |
| SYS_OUT | GPIO16（PWR）；单击宠物页探险/首页静音，且是唯一可退出省电降亮度的按键（v23 起），双击睡觉唤醒，长按退出宠物页 |
| 触屏 | CST9217；首页不响应触摸，v20 宠物模式页四个 76×56 黑白描边 LVGL 按钮（单行居中，间距 8px，下移），点击屏幕中央角色=抚摸，静置 1 分钟=自动睡觉；反馈字幕在等级徽章下方 |

## 相对上游的改动

完整 diff 见 `port-changes.patch`。摘要：

### 新增板型
- `firmware-src/main/boards/waveshare/esp32-s3-touch-amoled-2.16-grokbot/`
  - `config.h` / `config.json` / `README.md`
  - `esp32-s3-touch-amoled-2.16-grokbot.cc`（主实现：IMU 旋转、锁屏、Wi-Fi 省电）
  - `assets/grokbot.atlas`（360×360 / 28 状态图集，来自上游 fork）

### 修改文件
| 文件 | 改动 |
| --- | --- |
| `main/Kconfig.projbuild` | 注册 `BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_AMOLED_2_16_GROKBOT` |
| `main/CMakeLists.txt` | 板型源码 / 字体 / assets / esp_rom 依赖 |
| `main/display/grokbot_display.cc/.h` | v7 帧率 30fps（音频活跃 15fps / 省电 10fps）、锁屏暂停动画、反向播放解码路径精简、诊断日志；v9 idle 时由宠物状态驱动表情；v11 加入待机小动作；v12 独立宠物页；v14 宝可梦风格四边属性环；v15 属性名嵌条、四按钮、点击抚摸（playful）、静置 1 分钟自动睡眠（sleeping）；v16 黑白 UI（角色保持原色）、四条 14px 等宽属性条 + 四个 arc 连成连续圆角矩形环、属性名居中、按钮 92×64 间距 4px；v17 删除 arc，四根 14px 圆角 bar 首尾重叠成环、标签无底色按填充反色、字幕移到等级下方；v18 标签换 14px 字体、竖条文字旋转 90°、反馈字幕上移、横条独占四角不再重叠；v20 改用四个 lv_line 绘制同心圆角运动环（无底轨道、未满圆头/满值闭合、等距圆角）、删除属性名标签、反馈数值按环色显示、按钮下移；v21 角色图集改用子区域 image descriptor（252×286，data 偏移到内容 bbox，保留整帧 stride），emoji_box_ 固定整帧尺寸并把角色 image 用 LV_ALIGN_TOP_LEFT 定位到 bbox 起点，视觉零变化但每帧脏区/合成/送屏面积减 44%
| `main/display/pet_state.cc/.h` | v9 新增电子宠物；v11 重写为完整养成系统：六维状态（心情/饱腹/活力/健康/清洁/乐趣）、等级/成长阶段/亲密度/成就、抚摸防刷分、探险事件、睡眠与永不死亡机制、离线衰减；v22 探险事件扩至 10 种（esp_random+计数器混合种子），文案去掉内嵌数值与"探险："前缀（数值统一由彩色 delta 标签显示，修复重复） |
| `main/application.cc` | v7 OTA 版本检查前 3 次失败静默（配网 error 降噪） |
| `main/audio/audio_codec.h` | 本板 I2S ISR IRAM、DMA 8×360（120ms）、中断优先级 3 |
| `main/audio/codecs/box_audio_codec.cc` | 应用 I2S 中断优先级配置 |
| `main/audio/audio_service.cc/.h` | 播放队列 2→8、opus 优先级 2→5、audio_output/opus_codec 绑核 core 1 |
| `main/boards/common/wifi_board.cc/.h` | v8 联网后固化公共 DNS + 3 秒后重设（防 DHCP 覆盖），根治 `api.tenclass.net` 解析失败 |
| `main/audio/audio_codec.cc/.h` | v8 静音会话级（新增 `SetOutputVolume(volume, persist)`），开机音量默认 70 |
| `main/protocols/*`, `main/mcp_server.*`, `main/ota.*` | v8 从上游 v2.6.0 全量合入，适配 `ProcessPending` / `AddClientCapabilities` / `AddSystemStatus` |
| `CMakeLists.txt` | 版本号（当前 v23：`2.6.0-grokbot-v23`） |
| `sdkconfig` / 板型 `config.json` | v9 曾改 Multinet 自定义唤醒词；v10 起回退到 v8 的 AFE WakeNet `wn9_hiesp`，v11 保持不变 |
| 板型 `config.h` | v10 删除 `ROTATE_FADE_*` / `ROTATE_FLIP_*` 动画参数，v11 保持不变 |
| 板型 `.cc` | v10 删除全部旋转动画、宠物互动改触屏；v11 修复触屏（改轮询输入设备，绕开 LVGL 事件吞噬）、触屏纵向五分区 + 双击探险 + 长按状态页、AXP2101 I2C 探测重试 5 次避免热刷机重启、BOOT 双击投喂；v13 Key3/SYS_OUT 按键；v14 辅助键有效电平开机检测、Key3 按下沿单击切换宠物页、静音改用虚拟单参 `SetOutputVolume()` 真正下发 ES8311 音量 |
| `main/display/lcd_display.cc/.h` | v8 上游 `SpiLcdDisplay` 构造签名变更适配（移除 `buffer_lines` 参数） |

## 版本历史

| 版本 | 主要变化 | SHA256（前 8 位） |
| --- | --- | --- |
| v2 | 初版 GrokBot 移植 | `eff52351` |
| v3 | 20fps 节流 + 自动旋转 + 长按锁屏 | `7f8e8dfd` |
| v4 | 旋转 +90° 校准 + I2S 硬件抗断续 | `8054e653` |
| v5 | Wi-Fi modem sleep 动态关闭 | `f97f09d0` |
| v6 | 旋转方向根治（全部正立）+ 音频播放链路第二轮优化 | `f462d950` |
| v7 | 30 FPS 帧率提升 + BOOT 三击静音 + 旋转黑幕平滑过渡 + 配网 error 降噪 + 代码精简 | `09407e58` |
| v8 | 升级官方 v2.6.0 基线 + 固化公共 DNS 根治音频断续 + 音量修复 | `bcc53363` |
| v9 | 物理仿真旋转动画 + Multinet 自定义唤醒词「你好小何」+ 电子宠物（BOOT 组合键互动） | `f26f33db` |
| v10 | 回退 AFE 唤醒词 + 删除所有旋转动画 + 宠物互动改触屏三分区 | `95f2986e` |
| v11 | 修复触屏互动（输入设备轮询）+ 完整养成宠物系统 + AXP2101 热刷机稳定性修复 | `5efaba44` |
| v12 | 首页取消触摸 + 独立宠物模式页（属性条+六按钮）+ 按钮驱动差异化动画 + 语音唤醒渐隐 + 进页自动唤醒 | `9f77b010` |
| v13 | 属性条改左右侧不挡角色 + 六按钮单行大尺寸修复裁切 + Key3/SYS_OUT 按键 + 修复唤醒渐隐被误锁屏拦截 | `008dcb1e` |
| v14 | Key3 有效电平自动检测与单击切换 + 静音真正下发硬件音量 + 宝可梦风格宠物页（等级/体型、四边环形属性条、3×2 按钮） | `78316118` |
| v15 | 属性名嵌入属性条 + 按钮精简为四个 + 点击角色抚摸（playful）+ 静置 1 分钟自动睡眠（sleeping）+ 属性环贴边 | `f4cefafe` |
| v16 | 黑白极简 UI（角色保持原色）+ 四条 14px 等宽属性条与四角 arc 连成完整圆角矩形环 + 属性名居中 + 按钮 92×64 间距 4px | `bbed6038` |
| v17 | 四根圆角属性条首尾重叠成环（无 arc/圆点）+ 标签无底色按填充反色 + 字幕移到等级下方 | `3756aad3` |
| v18 | 属性条 14px 小字 + 竖条文字旋转 90° + 互动反馈上移 + 四角横条独占不重叠 | `1ba374bc` |
| **v20** | 四层运动环 + 彩色反馈 + 摇一摇头晕 + 环位图四条带优化 + 删 BOOT 三击静音 + 修复进页死锁 | `9d51ac54` |
| v21 | 方向3：角色图集子区域裁剪（252×286 / 360×360），每帧脏区与送屏面积减 44%，视觉零变化 | `b98184bf` |
| v22 | 探险事件扩至10种+硬件随机源+各自动画 + 探险字幕去重并删"探险："前缀 + 省电降亮度后触屏/按键唤醒（该唤醒实现存在 ISR 崩溃） | `8ebb1200` |
| **v23** | **修复省电降亮度后点击屏幕崩溃（移除触摸 ISR 唤醒）+ 退出省电收敛为仅 PWR 单击，界面不变** | **`af177357`** |

完整 SHA256 见 `releases/SHA256SUMS`。

## 旋转映射说明

`sw_rotate=0`，`lv_display_set_rotation()` 经 `LV_EVENT_RESOLUTION_CHANGED` 由 esp_lvgl_port 更新 CO5300 MADCTL，**整个 framebuffer 一起旋转**（文字与动画方向一致）。

QMI8658 重力四象限映射（同板型已验证）：

```
|ax| > |ay| :  ax > 0 → 0°      ax < 0 → 180°
|ay| >= |ax|:  ay > 0 → 90°     ay < 0 → 270°
|az| > 780mg（平放）→ 不旋转
轴向幅值 < 450mg → 不旋转（防抖：连续 3 次一致才生效）
```

旋转本身是单次硬件 MADCTL 切换。**v10 起（含 v11）直接切换、无过渡动画**：v7 的黑幕淡入淡出与 v9 的物理翻页（黑幕缩放 + 高光 + 贝塞尔缓动）均已在 v10 删除，相关 `lv_anim` / curtain / 缓动参数全部移除。
