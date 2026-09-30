# 🔧 STM32 学习记录（Arduino IDE + STM32duino）

> 从「环境都装不上」到「第一颗 LED 亮起来」。
> 本仓库记录使用 **STM32F103** 学习嵌入式开发的全部代码、接线与踩坑经验，按天持续更新。

[![Platform](https://img.shields.io/badge/platform-STM32F103C8T6-03234B?logo=stmicroelectronics&logoColor=white)](https://www.st.com/)
[![IDE](https://img.shields.io/badge/IDE-Arduino%20IDE%202.x-00979D?logo=arduino&logoColor=white)](https://www.arduino.cc/en/software)
[![Core](https://img.shields.io/badge/core-STM32duino-00599C?logo=cplusplus&logoColor=white)](https://github.com/stm32duino/Arduino_Core_STM32)
[![Programmer](https://img.shields.io/badge/programmer-ST--Link%20V2%20(SWD)-orange)](#-软件环境搭建)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

---

## 📖 项目简介

本仓库是一个 **STM32 从零入门的学习记录库**。

由于**无法顺利下载 STM32CubeMX**（官方安装包需要登录 ST 账号下载，网络受限时极易卡住），本项目改用 **Arduino IDE + STM32duino 核心库**的路线进行开发，使用 **ST-Link V2** 仿真器通过 **SWD** 接口烧录程序。

> 💡 这条路线的好处：不需要重型的 IDE，Arduino 生态的库即装即用，适合快速验证硬件与积累信心；后续需要寄存器级或 HAL 开发时，再平滑切换到 STM32CubeIDE 也不冲突。

**当前进度：[`firmware/Test_LED/`](firmware/Test_LED/Test_LED.ino)** —— 第一天，点亮外部绿色 LED（`PA5`），500 ms 周期闪烁。

---

## 📅 学习进度

| 天数 | 日期 | 实验内容 | 代码 | 笔记 |
| :---: | --- | --- | --- | --- |
| **Day 01** | 2026-10-01 | 环境搭建（Arduino IDE + STM32duino）· ST-Link SWD 烧录 · 点亮外部 LED | [`Test_LED.ino`](firmware/Test_LED/Test_LED.ino) | [📄 Day 01 笔记](docs/DAY01_ENVIRONMENT_AND_BLINK.md) |

---

## 📂 仓库目录结构

```
STM32/
├── README.md                       ← 你正在读的这份文档
├── LICENSE                         ← MIT 许可证
├── .gitignore / .gitattributes
├── firmware/                       ← 按学习顺序排列的 STM32 sketch
│   └── Test_LED/                   ← Day 01：外部 LED（PA5）闪烁
│       └── Test_LED.ino
├── Image_and_video/                ← 演示图片（后续会补充视频）
│   └── day01_green_led_breadboard.jpg  ← Day 01 面包板接线实拍
└── docs/
    └── DAY01_ENVIRONMENT_AND_BLINK.md  ← Day 01：环境搭建、接线与踩坑记录
```

---

## 🛠️ 硬件清单

| 硬件名称 | 数量 | 备注 |
| --- | :---: | --- |
| STM32F103 最小系统板 | 1 | C8T6 / RBT6（BluePill 板型） |
| ST-Link V2 仿真器 | 1 | SWD 烧录，**不能**同时被 STM32CubeProgrammer 占用 |
| 面包板 | 1 | |
| LED 灯 | 1 | 本实验使用绿色 LED，接 `PA5` |
| 杜邦线（母对母） | 若干 | |
| 1KΩ 金属膜电阻 | 1 | ⏳ **待补上**，见 [Day 01 笔记的遗留问题](docs/DAY01_ENVIRONMENT_AND_BLINK.md#-下一步计划) |
| USB Micro 数据线 | 1 | ⏳ **待补上**，用于开发板独立供电 |

---

## 🔌 引脚速查

| 引脚 | 用途 | 电平逻辑 |
| --- | --- | --- |
| `PA5` | 外部 LED（本实验） | **高电平点亮** |
| `PC13` | 板载 LED（红色） | **低电平点亮** ⚠️ 与外接 LED 相反 |
| `PA13` | SWDIO —— 接 ST-Link 的 SWDIO | 烧录用 |
| `PA14` | SWCLK —— 接 ST-Link 的 SWCLK | 烧录用 |
| `GND` | 与 ST-Link、面包板共地 | 必须连接 |
| `3.3V` | 可由 ST-Link 供电（板子已用 USB 供电时可不接） | |

> ⚠️ **最容易踩的坑**：板载 LED（`PC13`）是**低电平点亮**，外接 LED（`PA5`）是**高电平点亮**，换引脚时千万别忘了同步交换 `digitalWrite` 的高低电平。

---

## 🚀 软件环境搭建

1. **安装 Arduino IDE 2.x** —— [下载地址](https://www.arduino.cc/en/software)
2. **添加开发板管理器索引**：`文件` → `首选项` → 「附加开发板管理器网址」填入：
   ```
   https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json
   ```
3. **安装核心包**：`工具` → `开发板` → `开发板管理器`，搜索并安装 **`STM32 MCU based boards`**
4. **安装烧录工具**：安装 [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html)
5. **关键设置**：

   | 设置项 | 取值 |
   | --- | --- |
   | Board | `Generic STM32F1 series` |
   | Board Part Number | `BluePill F103C8` |
   | U(S)ART support | `Enabled (generic 'Serial')` |
   | Upload method | `STM32CubeProgrammer (SWD)` |

6. **上传代码**：打开 [`firmware/Test_LED/Test_LED.ino`](firmware/Test_LED/Test_LED.ino)，点击「上传」

> 🔍 完整的配置截图级步骤、接线照片与报错排查，见 **[docs/DAY01_ENVIRONMENT_AND_BLINK.md](docs/DAY01_ENVIRONMENT_AND_BLINK.md)**。

---

## 🚧 常见报错速查

| 报错 / 现象 | 原因 | 解决 |
| --- | --- | --- |
| `Some indexes could not be updated... read tcp ... wsarecv...` | IDE 走了系统代理，网络受限 | 首选项里改为**无代理**并重启 IDE；或换清华镜像源 |
| `No debug probe detected` | STM32CubeProgrammer 后台运行，**独占了 ST-Link** | 关闭 STM32CubeProgrammer，拔插 ST-Link 后重试上传 |
| 外部 LED 不亮 | 引脚定义成了 `PC13`，或高低电平逻辑写反 | 改成 `#define LED_PIN PA5`，外接 LED 需 `HIGH` 点亮 |
| LED 亮度偏暗 | 未串联限流电阻 | 串上 **1KΩ 电阻**（待办） |

> 详细复盘见 [docs/DAY01_ENVIRONMENT_AND_BLINK.md](docs/DAY01_ENVIRONMENT_AND_BLINK.md#-今日踩坑与解决记录troubleshooting)。

---

## 🎥 演示素材

| 内容 | 文件 |
| --- | --- |
| Day 01 面包板接线实拍（STM32F103C8T6 + 外部绿色 LED） | [day01_green_led_breadboard.jpg](Image_and_video/day01_green_led_breadboard.jpg) |

---

## 🚀 未来计划

- [ ] 补上 **1KΩ 限流电阻**与 **USB Micro 数据线**
- [ ] 按键输入控制 LED（`INPUT_PULLUP` + 消抖）
- [ ] 用 `millis()` 实现**非阻塞闪烁**，为多任务打基础
- [ ] 串口通信（`Serial.print`）与上位机交互
- [ ] PWM 调光（呼吸灯）
- [ ] 定时器中断与外部中断
- [ ] I2C / SPI 外设（OLED、传感器）
- [ ] 迁移到 **STM32CubeIDE + HAL 库**，对比 Arduino 封装与寄存器操作的差异

---

## 🤝 贡献与致谢

- 欢迎提交 Issue 反馈问题，或 PR 补充你的踩坑经验。
- 本项目参考了 [Arduino-Intro](https://github.com/Arduino-STM32-Dev/Arduino-Intro) 的仓库组织方式。

## 📄 许可证

本项目采用 **MIT License**，详见 [LICENSE](LICENSE)。
