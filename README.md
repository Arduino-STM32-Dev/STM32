# 🔧 STM32 入门教程：从点亮第一颗 LED 到驱动直流电机

> **不用 STM32CubeMX，也不用啃寄存器手册。**
> 本教程带你从零把 STM32F103 跑起来 —— 装环境、接线、烧录、排错，再一路做呼吸灯、蜂鸣器、多档位风扇，最后接入无线控制链路，做出一个带**红外安全互锁**的边缘执行终端，每一步都可以照着复现。

[![Platform](https://img.shields.io/badge/platform-STM32F103C8T6-03234B?logo=stmicroelectronics&logoColor=white)](https://www.st.com/)
[![IDE](https://img.shields.io/badge/IDE-Arduino%20IDE%202.x-00979D?logo=arduino&logoColor=white)](https://www.arduino.cc/en/software)
[![Core](https://img.shields.io/badge/core-STM32duino-00599C?logo=cplusplus&logoColor=white)](https://github.com/stm32duino/Arduino_Core_STM32)
[![Programmer](https://img.shields.io/badge/programmer-ST--Link%20V2%20(SWD)-orange)](#-快速开始)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

---

## 📖 这是什么

一份写给**嵌入式新手**的 STM32 实践教程，目前包含 5 篇，沿着「数字输出 → PWM → 输入 → 功率驱动 → 边缘安全互锁」这条最自然的主线往前走。

`firmware/` 里另外放了四份**面向真实项目**的固件，是「树莓派 → ESP32 → STM32」三级控制链路中的**终端层**实现，见 [🔗 物联网联调](#-物联网联调接收-esp32-的无线指令)：

- [`pi_control_fan_dht11`](firmware/pi_control_fan_dht11/pi_control_fan_dht11.ino)：**简明联调版** —— DHT11 + 风扇 + 按键，上报 `T:/H:/F:`，对应 [附录 06](docs/06_PI_FAN_DHT11.md)。- [`raspberry-control-stm32`](firmware/raspberry-control-stm32/raspberry-control-stm32.ino)：**第一步联调** —— 通过 `Serial` 接收 ESP32 转发的指令，控制板载 LED，先用最小代价把链路跑通。
- [`servo-control`](firmware/servo-control/servo-control.ino)：DHT11 + TB6612 + 舵机 + OLED 的完整外设版本，本地自动温控。
- [`edge_compute`](firmware/edge_compute/edge_compute.ino)：**当前主力固件** —— 在上一版基础上加入 **TCRT5000 红外安全互锁**，危险时本地强制急停，并锁死远程的「开启类」指令。

STM32CubeMX 的安装包需要登录 ST 账号下载，网络受限时极易卡住，所以本教程改用 **Arduino IDE + STM32duino 核心库**的路线：不装重型 IDE，Arduino 生态的库即装即用，先把硬件跑通、把信心建立起来。等以后需要寄存器级或 HAL 开发时，再平滑切换到 STM32CubeIDE 也完全来得及。

**跟着做完，你会掌握：**

- STM32duino 核心包的安装与开发板参数配置，用 ST-Link V2 通过 **SWD** 接口烧录
- 外部 LED 的接线，以及「高电平点亮 / 低电平点亮」这个经典坑
- **PWM 与占空比** —— 用 `analogWrite()` 做呼吸灯、调速电机
- **按键输入**：内部上拉、边沿检测、软件消抖
- **有源 / 无源蜂鸣器**的区别与驱动方式
- 用 **TB6612FNG** 驱动直流电机，写一个多档位状态机
- 用 **DHT11** 采集温湿度、**舵机**做阀门、**U8g2** 驱动 OLED，并用 `millis()` 把 6 个任务组织成非阻塞结构
- **边缘安全互锁**：用 TCRT5000 红外光电开关做最高优先级任务，危险时本地强制急停，并锁死远程的开启类指令
- 硬件排错的基本功：**短接法**查杜邦线、从板载 LED 判断**欠压复位**、**独立供电 + 共地**
- 用 **`Serial` + `readStringUntil('\n')`** 和 ESP32 做 UART 联调，接入 MQTT 无线控制链路

---

## 🎬 先看效果

| 01 · 点亮外部 LED | 04 · 多档位风扇 |
| :---: | :---: |
| ![面包板接线实拍](Image_and_video/green_led_breadboard.jpg) | ![TB6612 驱动风扇实拍](Image_and_video/Fan-control.jpg) |

---

## 🛠️ 硬件清单

| 硬件名称 | 数量 | 说明 |
| --- | :---: | --- |
| STM32F103 最小系统板 | 1 | C8T6 / RBT6（BluePill 板型） |
| ST-Link V2 仿真器 | 1 | SWD 烧录。⚠️ 不能同时被 STM32CubeProgrammer 占用 |
| TB6612FNG 电机驱动模块 | 1 | 教程 04 用，双 H 桥 |
| 130 直流电机 + 扇叶 | 1 | 教程 04 用 |
| TCRT5000 红外光电开关模块 | 1 | 教程 05 用，遮挡触发（输出 `LOW`），是**安全互锁的输入源** |
| SG90 舵机（180°） | 1 | 教程 05 用，模拟阀门开 / 关（`90°` / `0°`） |
| DHT11 温湿度模块 | 1 | 教程 05 用，三针模块自带 4.7K 上拉，接 `PA0` |
| 0.96" OLED 显示屏（SSD1306, I2C） | 1 | 教程 05 用，**硬件 I2C**，地址通常为 `0x3C` |
| 有源蜂鸣器模块 | 1 | 教程 03 用（教程用的是 MH-FMD 三针模块） |
| 两脚常开按键 | 1 | 教程 02 / 03 / 04 用，接 `INPUT_PULLUP` |
| 面包板 | 1 | |
| LED 灯 | 1 | 教程使用绿色 LED，接 `PA5` |
| 杜邦线（母对母） | 若干 | ⚠️ 质量参差，内部断线是「玄学故障」头号嫌疑人 |
| 1KΩ 电阻 | 1 | 限流用。⚠️ 教程 01 实测时未接，LED 偏暗，**建议你接上** |
| USB Micro 数据线 | 1 | 给开发板供电 |
| 外部 5V 电源（电池盒 / 充电宝） | 1 | 教程 04 / 05 用。⚠️ 驱动电机和舵机**强烈建议**配一个，并与 STM32 **共地** |

---

## 📂 仓库目录结构

```
STM32/
├── README.md                                  ← 你正在读的这份文档
├── LICENSE                                    ← MIT 许可证
├── .gitignore / .gitattributes
├── firmware/                                  ← 按教程顺序排列的 STM32 sketch
│   ├── Test_LED/                              ← 教程 01：外部 LED（PA5）闪烁
│   │   └── Test_LED.ino
│   ├── PWM_LED/                               ← 教程 02：PWM 呼吸灯 + 按键开关
│   │   └── PWM_LED.ino
│   ├── Buzzer_test/                           ← 教程 03：按键控制有源蜂鸣器
│   │   └── Buzzer_test.ino
│   ├── use_button_control_fan/                ← 教程 04-1：按住按键电机就转（先验证硬件）
│   │   └── use_button_control_fan.ino
│   ├── MultiSpeed_Fan/                        ← 教程 04-2：按键循环切换 0~3 档风速
│   │   └── MultiSpeed_Fan.ino
│   ├── pi_control_fan_dht11/                  ← 联调 01（简明版）：DHT11 + 风扇 + 按键，仅上报 T:/H:/F:
│   │   └── pi_control_fan_dht11.ino
│   ├── raspberry-control-stm32/               ← 联调 02：Serial 接收 ESP32 指令，控制板载 LED
│   │   └── raspberry-control-stm32.ino
│   ├── servo-control/                         ← 联调 03：DHT11 + TB6612 + 舵机 + OLED，本地自动温控
│   │   └── servo-control.ino
│   └── edge_compute/                          ← 教程 05（主力）：红外安全互锁 + 远程指令 + 状态上报
│       └── edge_compute.ino
├── Image_and_video/                           ← 接线实拍与演示素材
│   ├── green_led_breadboard.jpg
│   ├── Buzzer_test.jpg
│   └── Fan-control.jpg
└── docs/
    ├── 01_ENVIRONMENT_AND_BLINK.md            ← 教程 01：环境搭建、接线与排错
    ├── 02_PWM_AND_BREATHING_LED.md            ← 教程 02：PWM 调光与呼吸灯
    ├── 03_BUTTON_AND_BUZZER.md                ← 教程 03：按键控制有源蜂鸣器
    ├── 04_DC_MOTOR_MULTI_SPEED_FAN.md         ← 教程 04：TB6612 驱动电机与多档位风扇
    ├── 05_EDGE_SAFETY_INTERLOCK.md            ← 教程 05：红外安全互锁与边缘计算
    └── 06_PI_FAN_DHT11.md                     ← 附录 06：DHT11 + 风扇（树莓派远程控制 · 简明版）
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

到这里「工具链」就搭好了，后面 02 / 03 / 04 / 05 篇可以直接在 Arduino IDE 里换个 sketch 上传，环境不需要再动。

> 🔍 接线照片、逐项报错排查和原理说明，见 **[docs/01_ENVIRONMENT_AND_BLINK.md](docs/01_ENVIRONMENT_AND_BLINK.md)**。

---

## 🔗 物联网联调：接收 ESP32 的无线指令

当 STM32 从「面包板上的练习」变成「无线控制系统的执行终端」时，它不再靠按键输入，而是通过 **UART** 接收 ESP32 经 MQTT 转发的指令，并把传感器数据回传上去。

```text
[树莓派 5 · MQTT Broker]       订阅 home/fan/status
      ⬆ WiFi / MQTT  主题 pi/to/esp32
[ESP32 · 无线网关]  firmware/cache_control（断网缓存 + 非阻塞队列）
      ⬇ UART 9600, 8N1
[STM32 · 执行终端]  firmware/edge_compute（红外安全互锁）
```

**接线（注意 TX / RX 交叉）：**

| ESP32 | STM32 | 说明 |
| --- | --- | --- |
| `D17` (TX2) | `PA10` (RX) | 指令下发 |
| `D16` (RX2) | `PA9` (TX) | 状态回传 |
| `GND` | `GND` | **共地（必须）** |

> ⚠️ ESP32 与 STM32 **各自独立供电**，请勿把 VCC 直接互连。

**跑通步骤：**

1. 烧录 [`firmware/edge_compute/edge_compute.ino`](firmware/edge_compute/edge_compute.ino) 到 STM32（想先跑最小链路，也可以先用 [`raspberry-control-stm32`](firmware/raspberry-control-stm32/raspberry-control-stm32.ino)）。
2. 按上表接好三根线，ESP32 烧录网关固件 —— 见 [esp32 仓库](https://github.com/Arduino-STM32-Dev/esp32)。
3. 在树莓派终端下发指令：

```bash
mosquitto_pub -h localhost -t "pi/to/esp32" -m "FAN_ON"       # 开风扇（PWM 150）
mosquitto_pub -h localhost -t "pi/to/esp32" -m "FAN_OFF"      # 关风扇
mosquitto_pub -h localhost -t "pi/to/esp32" -m "VALVE_OPEN"   # 开阀门（舵机 90°）
mosquitto_pub -h localhost -t "pi/to/esp32" -m "VALVE_CLOSE"  # 关阀门（舵机 0°）
mosquitto_sub -h localhost -t "home/fan/status" -v            # 看回传状态
```

**指令对照：**

| 指令 | STM32 动作 | 安全锁 |
| --- | --- | --- |
| `FAN_ON` | `analogWrite(PWMA, 150)`，风扇转动 | ⚠️ 警报时**被忽略** |
| `FAN_OFF` | `analogWrite(PWMA, 0)`，风扇停止 | ✅ 永远执行 |
| `VALVE_OPEN` | `valveServo.write(90)`，阀门打开 | ⚠️ 警报时**被忽略** |
| `VALVE_CLOSE` | `valveServo.write(0)`，阀门关闭 | ✅ 永远执行 |

**回传格式（每 2 秒一行）：**

```text
T:25.00,H:60.00,F:1,V:0,S:1
```

| 字段 | 含义 |
| --- | --- |
| `T:` | 温度（℃） |
| `H:` | 湿度（%） |
| `F:` | 风扇状态，`1` 开 / `0` 关 |
| `V:` | 阀门状态，`1` 开 / `0` 关 |
| `S:` | **安全状态，`1` 安全 / `0` 警报** |

> 🛡️ **本地安全优先**：`edge_compute` 把红外检测放在 `loop()` 的**任务 1**。一旦遮挡触发，它不等云端指令，直接 `analogWrite(PWMA, 0)` + `valveServo.write(0)` 并置位 `isSafetyAlert`；此后所有**开启类**动作（本地按键、自动温控、远程 `FAN_ON` / `VALVE_OPEN`）全部被锁死，而**关闭类**指令始终有效 —— 也就是「**危险只能让系统停下来，不能让系统动起来**」。详见 [教程 05](docs/05_EDGE_SAFETY_INTERLOCK.md)。

> ⚠️ **换行符约定**：STM32 用 `readStringUntil('\n')` 解析，所以 ESP32 必须用 `println()` 发送；STM32 回传同样用 `println()`，ESP32 才能按行读到。两端成对使用，否则会收到空串。

---

## 🔌 引脚速查

### 各篇教程用到的引脚

| 引脚 | 用于 | 用途 | 电平逻辑 |
| --- | :---: | --- | --- |
| `PA5` | 01 | 外部 LED | **高电平点亮** |
| `PA5` | 02 | PWM 调光输出 | `analogWrite()` 0~255 |
| `PA5` | 03 | 蜂鸣器模块 `I/O` | 见 [03 篇触发极性说明](docs/03_BUTTON_AND_BUZZER.md) |
| `PA5` | 04 | TB6612 的 `PWMA` | `analogWrite()` 0~255 |
| `PA1` | 02 / 03 | 按键（`INPUT_PULLUP`） | 按下 = `LOW` |
| `PA2` | 04 / 05 | 按键（`INPUT_PULLUP`） | 按下 = `LOW` |
| `PB12` | 04 / 05 | TB6612 的 `AIN1` | 方向控制 |
| `PB13` | 04 / 05 | TB6612 的 `AIN2` | 方向控制 |
| `PA0` | 05 | DHT11 数据脚 | 单总线，模块自带 4.7K 上拉 |
| `PB0` | 05 | 舵机信号（模拟阀门） | `0°` 关 / `90°` 开 |
| `PB1` | 05 | TCRT5000 红外输出 | **遮挡 = `LOW` → 触发急停** |

> ⚠️ **`PA5` 在 4 篇里接了 4 种不同的外设**，换实验时记得**同时改接线和代码**，别让旧接线留在面包板上。

### 固定用途的引脚

| 引脚 | 用途 | 说明 |
| --- | --- | --- |
| `PA13` | SWDIO | 接 ST-Link 的 SWDIO，烧录用 |
| `PA14` | SWCLK | 接 ST-Link 的 SWCLK，烧录用 |
| `PA9` | `Serial` TX | 接 ESP32 的 `D16` (RX2)，**状态回传**，9600 8N1 |
| `PA10` | `Serial` RX | 接 ESP32 的 `D17` (TX2)，**接收指令**，9600 8N1 |
| `PB6` / `PB7` | OLED SCL / SDA | **硬件 I2C** 默认引脚（`U8g2 ... _HW_I2C`），地址 `0x3C` |
| `PC13` | 板载 LED（红色） | **低电平点亮** ⚠️ 与外接 LED 相反；**闪一下 = 复位** |
| `GND` | 共地 | 与 ST-Link、面包板、**外部电源**都必须连在一起 |
| `3.3V` / `5V` | 供电 | 板子已用 USB 供电时可不接 ST-Link 的 3.3V |

> ⚠️ **最容易踩的坑**：板载 LED（`PC13`）是**低电平点亮**，外接 LED（`PA5`）是**高电平点亮**，换引脚时千万别忘了同步交换 `digitalWrite` 的高低电平。

---

## 🚧 常见报错速查

| 报错 / 现象 | 原因 | 解决 |
| --- | --- | --- |
| `Some indexes could not be updated... read tcp ... wsarecv...` | IDE 走了系统代理，网络受限 | 首选项里改为**无代理**并重启 IDE；或换清华镜像源 |
| `No debug probe detected` | STM32CubeProgrammer 后台运行，**独占了 ST-Link** | 关闭 STM32CubeProgrammer，拔插 ST-Link 后重试上传 |
| 外部 LED 不亮 | 引脚定义成了 `PC13`，或高低电平逻辑写反 | 改成 `#define LED_PIN PA5`，外接 LED 需 `HIGH` 点亮 |
| LED 亮度偏暗 | 未串联限流电阻 | 串上 **1KΩ 电阻** |
| `analogWrite()` 只有全亮 / 全灭 | 该引脚不支持硬件 PWM | 换一个支持 PWM 的引脚 |
| 蜂鸣器上电就一直响 | 模块是**低电平触发**，代码电平写反 | 对调 `digitalWrite` 的 `HIGH` / `LOW` |
| 蜂鸣器声音太小 | 有源蜂鸣器音量由硬件决定；防尘贴纸挡住了发声孔 | 撕掉顶部 `REMOVE SEAL AFTER WASHING` 贴纸；或改用 5V 供电 |
| 按键怎么按都没反应 | **杜邦线内部断路**（外皮完好，肉眼看不出来） | 把按键信号脚**直接短接到 GND** 测试；换一根线 |
| 电机 / 舵机一动作，STM32 就复位（板载红灯闪一下） | USB 供电能力不足，电机或舵机启动浪涌把 5V 拉低 | 给电机、舵机接**独立 5V 电源**，并与 STM32 **共地** |
| 红外没遮挡却一直急停（`S:0`） | TCRT5000 触发极性 / 灵敏度与假设相反，或供电不稳 | 代码用 `irState == LOW` 判定遮挡，实测相反就改成 `HIGH`；用模块上电位器调灵敏度；红外模块单独 3.3V 供电 |
| 警报解除后风扇 / 阀门不自动恢复 | 这是**设计要求**：急停只负责关闭，不负责恢复 | 障碍物移开后需重新下发 `FAN_ON` / `VALVE_OPEN`，或按本地按键 |
| 警报期间远程 `FAN_ON` 没反应 | **安全锁生效**（预期行为） | 想看互锁是否真的触发，观察串口是否打印 `EMERGENCY_STOP: Object detected!` |
| DHT11 读到 `nan` / OLED 温度一直是 0 | 上电 1~2 秒内首次读取会失败；或上拉 / 接线问题 | 代码已用 `isnan()` 过滤，等 2 秒再看；宽量程改 DHT22；检查 `PA0` 接线 |
| OLED 全黑不显示 | I2C 地址不是 `0x3C`，或 SCL / SDA 接错 | 跑一段 I2C 扫描程序确认地址；F103 默认 `PB6`(SCL) / `PB7`(SDA) |
| 舵机上电抖动 / 直接转到底 | 舵机 5V 供电不足或与 STM32 未共地；初始化前收到随机电平 | 舵机单独 5V 供电并**共地**；`setup()` 里已 `valveServo.write(0)` 归位 |
| 电机纹丝不动 | TB6612 的 `STBY` 没接高电平；或 `AIN1`/`AIN2` 都是低 | `STBY` 接 `3.3V`；按真值表给 `AIN1`/`AIN2` 一个高一个低 |

> 完整复盘（含报错原文、排查过程与原理）见各篇文档的「🚧 踩坑与排错」章节。

---

## 📚 教程目录

| 篇 | 内容 | 文档 |
| :---: | --- | --- |
| **01** | 环境搭建 · ST-Link SWD 烧录 · 点亮外部 LED | [📄 环境搭建与点亮第一个 LED](docs/01_ENVIRONMENT_AND_BLINK.md) |
| **02** | PWM 原理 · `analogWrite()` · 呼吸灯 · 按键开关 | [📄 PWM 调光与呼吸灯](docs/02_PWM_AND_BREATHING_LED.md) |
| **03** | 有源 / 无源蜂鸣器 · 边沿检测 · 按键开关 | [📄 按键控制有源蜂鸣器](docs/03_BUTTON_AND_BUZZER.md) |
| **04** | TB6612FNG · H 桥 · 直流电机 · 多档位状态机 · 供电排错 | [📄 TB6612 驱动直流电机与多档位风扇](docs/04_DC_MOTOR_MULTI_SPEED_FAN.md) |
| **05** | TCRT5000 红外 · 边缘计算 · **安全互锁** · 任务优先级 · 远程指令与状态上报 | [📄 红外安全互锁与边缘计算](docs/05_EDGE_SAFETY_INTERLOCK.md) |
| **06** | （简明版）DHT11 + 风扇 · 树莓派远程控制最小实现 | [📄 STM32 端 DHT11 + 风扇](docs/06_PI_FAN_DHT11.md) |

**建议的练习顺序**

- [ ] 跑通 01，用板载 LED（`PC13`）再验证一遍「低电平点亮」
- [ ] 调快 / 调慢闪烁频率，理解 `delay()` 的阻塞行为
- [ ] 做 02 的呼吸灯，把 `delay(5)` 改成 `15` / `2` 观察速度变化
- [ ] 把 02 的按键改成「三个档位亮度循环」
- [ ] 做 03 的蜂鸣器开关，再换成**无源蜂鸣器**用 PWM 奏出音调
- [ ] 做 04：先用「按住就转」验证硬件，再实现多档位风扇
- [ ] 给 04 的电机加**软启动**（`analogWrite` 从 0 缓慢升到目标档位）
- [ ] 用 `millis()` 替换所有 `delay()`，实现非阻塞的多档位风扇
- [ ] 接 14 号（NTC）热敏电阻，让风扇**根据温度自动调速**（05 篇已用 DHT11 实现自动温控）
- [ ] 做 05：先只测红外急停，再接上 `FAN_ON` / `VALVE_OPEN` 远程指令，验证**安全锁**是否生效
- [ ] 把 05 的警报接入蜂鸣器或继电器，做成「**本地报警 + 云端通知**」的双通道告警
- [ ] 打开串口（`Serial.print`），把温度、档位打印到电脑上
- [ ] 定时器中断与外部中断
- [ ] I2C / SPI 外设（OLED、传感器）
- [ ] 迁移到 **STM32CubeIDE + HAL 库**，对比 Arduino 封装与寄存器操作的差异

---

## 🤝 贡献与致谢

- 欢迎提交 Issue 反馈问题，或 PR 补充你的踩坑经验 —— 让后面的人少走弯路。
- 本仓库的组织方式参考了 [Arduino-Intro](https://github.com/Arduino-STM32-Dev/Arduino-Intro)。

## 📄 许可证

本项目采用 **MIT License**，详见 [LICENSE](LICENSE)。
