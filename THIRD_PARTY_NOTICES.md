# 第三方组件与引用声明

本固件移植项目基于多个开源项目。以下逐项列出来源、用途、精确版本/commit 与许可证。控件原文保留，未做删改。

---

## 1. xiaozhi-esp32（固件主体）

- **仓库**：https://github.com/78/xiaozhi-esp32
- **版权**：Copyright (c) 2025 Shenzhen Xinzhi Future Technology Co., Ltd. / Project Contributors
- **许可证**：MIT License
- **用途**：小智 AI 助手固件主体（应用、协议、显示框架、音频服务、资源系统等）

```
MIT License

Copyright (c) 2025 Shenzhen Xinzhi Future Technology Co., Ltd.
Copyright (c) 2025 Project Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

`firmware-src/LICENSE` 为上游原始许可证文件的完整副本。

---

## 2. Ocean-lhy/xiaozhi-esp32 · dev-grokbot 分支（直接代码基线）

- **仓库**：https://github.com/Ocean-lhy/xiaozhi-esp32
- **分支**：`dev-grokbot`
- **精确 commit**：`1e1df1d94bd4a984f899e80b951552f93da5d6ae`
- **commit 说明**：`feat(m5stack): add StopWatch GrokBot animation display`
- **许可证**：继承上游 xiaozhi-esp32 的 MIT License
- **用途**：`firmware-src/` 的直接代码基线；提供 `GrokbotDisplay` / `GrokBotPlayer` 与 `grokbot.atlas` 打包资源。
- **本项目的修改**：新增微雪板型、旋转/锁屏/音频任务调整，完整 diff 见 `docs/port-changes.patch`。

---

## 3. Stopwatch-GrokBot（GrokBot 动画与图集）

- **仓库**：https://github.com/Ocean-lhy/Stopwatch-GrokBot
- **精确 commit**：`9d917f8dcbe48396c74145341a55c8f7b8492081`
- **用途**：GrokBot 动画状态机设计参考；53 状态、400×400、16 级灰度、30FPS 的图集设计。
- **角色来源**：GrokBot 头像解剖 https://grokbot.yin.wiki/index.html
- **许可证**：按其仓库声明；本项目仅参考其概念与打包格式，实际图集来自上游 fork 的 `grokbot.atlas`（360×360 / 28 状态）。

---

## 4. xiaozhi-claude-pixel（像素小怪表情素材）

- **仓库**：https://github.com/xiaomoBoy/xiaozhi-claude-pixel
- **精确 commit**：`239d343a9e4c422018a268a5f0e9f535f186d3c9`
- **版权**：Copyright (c) 2026 xiaomoBoy
- **许可证**：MIT License
- **用途**：早期美化方案的像素小怪表情素材（本项目最终 GrokBot 固件未打包这些素材，但保留致谢）。

```
MIT License

Copyright (c) 2026 xiaomoBoy

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## 5. Tokenmeter（QMI8658 四向旋转映射参考）

- **仓库**：https://github.com/HerbertGao/Tokenmeter
- **参考文件**：`firmware/src/core.cpp` 中 `Orientation::update()` 的重力象限映射
- **用途**：本板型四向自动旋转映射的验证参考。`|ax|>|ay|` 时 `ax>0→0° / ax<0→180°`；否则 `ay>0→90° / ay<0→270°`。
- **许可证**：按其仓库声明。

---

## 6. 框架与组件

| 组件 | 仓库 | 许可证 | 用途 |
| --- | --- | --- | --- |
| ESP-IDF v6.1.1 | https://github.com/espressif/esp-idf | Apache-2.0 | 构建框架 / 驱动 |
| LVGL 9 | https://github.com/lvgl/lvgl | MIT | GUI |
| esp_lvgl_port | Espressif（ESP-IDF 组件） | Apache-2.0 | LVGL 与 LCD/Touch 桥接 |
| esp-wifi-connect | https://github.com/78/esp-wifi-connect | MIT | Wi-Fi 配网与管理 |
| esp-lcd-co5300 | Espressif 组件注册表 | Apache-2.0 | CO5300 QSPI AMOLED 驱动 |
| esp-lcd-touch-cst9217 | Espressif 组件注册表 | Apache-2.0 | CST9217 触摸驱动 |

组件精确版本与哈希见 `firmware-src/dependencies.lock`。

---

## 7. 硬件

- **Waveshare ESP32-S3-Touch-AMOLED-2.16**（https://www.waveshare.com/）：板型硬件设计与示例参考。本项目不复制其专有代码，硬件寄存器配置参考其公开的示例与数据手册。
- **M5Stack StopWatch / C152**：GrokBot 动画原目标硬件，本项目为其动画在另一板型上的移植。

---

## 说明

- 本项目新增代码的许可证见根目录 `LICENSE`（MIT）。
- 若发现任何遗漏的署名、许可文本或错误归属，请提交 Issue，我们将及时更正。
- 本项目与上述任何组织/个人无官方关联，不提供担保。
