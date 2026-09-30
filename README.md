# 🔧 STM32 入门教程：零基础点亮第一颗 LED

> **不用 STM32CubeMX，也不用啃寄存器手册。**
> 本教程带你从零把 STM32F103 跑起来 —— 装环境、接线、烧录、排错，每一步都可以照着复现。

[![Platform](https://img.shields.io/badge/platform-STM32F103C8T6-03234B?logo=stmicroelectronics&logoColor=white)](https://www.st.com/)
[![IDE](https://img.shields.io/badge/IDE-Arduino%20IDE%202.x-00979D?logo=arduino&logoColor=white)](https://www.arduino.cc/en/software)
[![Core](https://img.shields.io/badge/core-STM32duino-00599C?logo=cplusplus&logoColor=white)](https://github.com/stm32duino/Arduino_Core_STM32)
[![Programmer](https://img.shields.io/badge/programmer-ST--Link%20V2%20(SWD)-orange)](#-快速开始)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

---

## 📖 这是什么

一份写给**嵌入式新手**的 STM32 实践教程。

STM32CubeMX 的安装包需要登录 ST 账号下载，网络受限时极易卡住，所以本教程改用 **Arduino IDE + STM32duino 核心库**的路线：不装重型 IDE，Arduino 生态的库即装即用，先把硬件跑通、把信心建立起来。等以后需要寄存器级或 HAL 开发时，再平滑切换到 STM32CubeIDE 也完全来得及。

**跟着做完，你会掌握：**

- STM32duino 核心包的安装与开发板参数配置
- 用 ST-Link V2 通过 **SWD** 接口烧录程序
- 外部 LED 的接线，以及「高电平点亮 / 低电平点亮」这个经典坑
- 下载失败、探测不到仿真器、灯不亮等问题的排查思路

---

## 🎬 先看效果

STM32F103C8T6 最小系统板 + 面包板上的外部绿色 LED（`PA5`），500 ms 周期闪烁：

![面包板接线实拍](Image_and_video/day01_green_led_breadboard.jpg)

---

## 🛠️ 硬件清单

| 硬件名称 | 数量 | 说明 |
| --- | :---: | --- |
| STM32F103 最小系统板 | 1 | C8T6 / RBT6（BluePill 板型） |
| ST-Link V2 仿真器 | 1 | SWD 烧录。⚠️ 不能同时被 STM32CubeProgrammer 占用 |
| 面包板 | 1 | |
| LED 灯 | 1 | 教程使用绿色 LED，接 `PA5` |
| 杜邦线（母对母） | 若干 | |
| 1KΩ 电阻 | 1 | 限流用。⚠️ 本教程实测时未接，LED 偏暗，**建议你接上** |
| USB Micro 数据线 | 1 | 可选，用于开发板独立供电 |

---

## 📂 仓库目录结构

```
STM32/
├── README.md                              ← 你正在读的这份文档
├── LICENSE                                ← MIT 许可证
├── .gitignore / .gitattributes
├── firmware/                              ← 按教程顺序排列的 STM32 sketch
│   └── Test_LED/                          ← 教程 01：外部 LED（PA5）闪烁
│       └── Test_LED.ino
├── Image_and_video/                       ← 接线实拍与演示素材
│   └── day01_green_led_breadboard.jpg
└── docs/
    └── 01_ENVIRONMENT_AND_BLINK.md        ← 教程 01：环境搭建、接线与踩坑记录
```

---

## 🚀 快速开始

### 第 1 步：安装 Arduino IDE

下载并安装 [Arduino IDE 2.x](https://www.arduino.cc/en/software)（1.8.x 同样可用）。

### 第 2 步：添加 STM32 开发板索引

`文件` → `首选项` → 「附加开发板管理器网址」填入：

```
https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json
```

> 若提示索引更新失败，多半是系统代理的问题 —— 见 [常见报错速查](#-常见报错速查)。

### 第 3 步：安装核心包

`工具` → `开发板` → `开发板管理器`，搜索并安装 **`STM32 MCU based boards`**（下载体积较大，请耐心等待）。

### 第 4 步：安装烧录工具

安装 [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html)。Arduino IDE 底层依赖它通过 ST-Link 进行 SWD 烧录。

### 第 5 步：配置开发板参数

| 设置项 | 取值 |
| --- | --- |
| Board | `Generic STM32F1 series` |
| Board Part Number | `BluePill F103C8` |
| U(S)ART support | `Enabled (generic 'Serial')` |
| Upload method | `STM32CubeProgrammer (SWD)` |

### 第 6 步：接线并上传

按下面的表把 ST-Link V2 接到板子上，再把 LED 接到 `PA5`：

| ST-Link V2 | STM32 引脚 |
| --- | --- |
| SWDIO | `PA13` |
| SWCLK | `PA14` |
| GND | `GND`（必须接） |
| 3.3V | `3.3V`（板子已用 USB 供电时可不接） |

然后打开 [`firmware/Test_LED/Test_LED.ino`](firmware/Test_LED/Test_LED.ino)，点击「上传」。

🎉 **LED 开始闪烁，说明整条链路已经通了。**

> 🔍 接线照片、逐项报错排查和原理说明，见 **[docs/01_ENVIRONMENT_AND_BLINK.md](docs/01_ENVIRONMENT_AND_BLINK.md)**。

---

## 🔌 引脚速查

| 引脚 | 用途 | 电平逻辑 |
| --- | --- | --- |
| `PA5` | 外部 LED（本教程） | **高电平点亮** |
| `PC13` | 板载 LED（红色） | **低电平点亮** ⚠️ 与外接 LED 相反 |
| `PA13` | SWDIO —— 接 ST-Link 的 SWDIO | 烧录用 |
| `PA14` | SWCLK —— 接 ST-Link 的 SWCLK | 烧录用 |
| `GND` | 与 ST-Link、面包板共地 | 必须连接 |
| `3.3V` | 可由 ST-Link 供电 | 板子已用 USB 供电时可不接 |

> ⚠️ **最容易踩的坑**：板载 LED（`PC13`）是**低电平点亮**，外接 LED（`PA5`）是**高电平点亮**，换引脚时千万别忘了同步交换 `digitalWrite` 的高低电平。

---

## 🚧 常见报错速查

| 报错 / 现象 | 原因 | 解决 |
| --- | --- | --- |
| `Some indexes could not be updated... read tcp ... wsarecv...` | IDE 走了系统代理，网络受限 | 首选项里改为**无代理**并重启 IDE；或换清华镜像源 |
| `No debug probe detected` | STM32CubeProgrammer 后台运行，**独占了 ST-Link** | 关闭 STM32CubeProgrammer，拔插 ST-Link 后重试上传 |
| 外部 LED 不亮 | 引脚定义成了 `PC13`，或高低电平逻辑写反 | 改成 `#define LED_PIN PA5`，外接 LED 需 `HIGH` 点亮 |
| LED 亮度偏暗 | 未串联限流电阻 | 串上 **1KΩ 电阻** |

> 完整复盘（含报错原文与原理）见 [docs/01_ENVIRONMENT_AND_BLINK.md](docs/01_ENVIRONMENT_AND_BLINK.md#-踩坑与解决troubleshooting)。

---

## 📚 教程目录

| 篇 | 内容 | 文档 |
| :---: | --- | --- |
| **01** | 环境搭建 · ST-Link SWD 烧录 · 点亮外部 LED | [📄 环境搭建与点亮第一个 LED](docs/01_ENVIRONMENT_AND_BLINK.md) |

**建议的练习顺序**

- [ ] 用板载 LED（`PC13`）再跑一遍，亲手验证「低电平点亮」
- [ ] 调快 / 调慢闪烁频率，理解 `delay()` 的阻塞行为
- [ ] 加入按键输入（`INPUT_PULLUP` + 软件消抖）控制 LED
- [ ] 用 `millis()` 替换 `delay()`，实现非阻塞闪烁
- [ ] 打开串口（`Serial.print`），把状态打印到电脑上
- [ ] PWM 调光，做一颗呼吸灯
- [ ] 定时器中断与外部中断
- [ ] I2C / SPI 外设（OLED、传感器）
- [ ] 迁移到 **STM32CubeIDE + HAL 库**，对比 Arduino 封装与寄存器操作的差异

---

## 🤝 贡献与致谢

- 欢迎提交 Issue 反馈问题，或 PR 补充你的踩坑经验 —— 让后面的人少走弯路。
- 本仓库的组织方式参考了 [Arduino-Intro](https://github.com/Arduino-STM32-Dev/Arduino-Intro)。

## 📄 许可证

本项目采用 **MIT License**，详见 [LICENSE](LICENSE)。
