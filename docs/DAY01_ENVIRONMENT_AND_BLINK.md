# STM32 入门第一天：环境搭建与点亮第一个 LED

> **日期**：2026-10-01
> **目标**：跑通 STM32F103 的开发与烧录链路，让第一颗外部 LED 闪起来
> **结果**：✅ 成功 —— 外部绿色 LED（`PA5`）以 500 ms 周期闪烁
> **关键词**：`STM32F103C8T6` · `Arduino IDE` · `STM32duino` · `ST-Link V2` · `SWD`

---

## 📌 项目简介

本项目是 STM32 学习的第一个基础实验。

由于**无法顺利下载 STM32CubeMX**（官方安装包需要登录 ST 账号下载，网络受限时极易卡住），我们选择改走 **Arduino IDE + STM32duino 核心库**的路线：

- **开发板**：STM32F103C8T6 最小系统板（BluePill）
- **开发环境**：Arduino IDE 2.x + `STM32 MCU based boards` 核心包
- **烧录方式**：ST-Link V2 仿真器，通过 **SWD** 接口烧录（Upload method 选 `STM32CubeProgrammer (SWD)`）

通过 ST-Link V2 仿真器进行程序烧录，最终成功实现了对外部 LED 的控制。本记录汇总今天的环境配置流程、硬件接线、代码以及踩坑经验。

---

## 🛠️ 硬件准备

| 硬件 | 数量 | 状态 | 备注 |
| --- | :---: | :---: | --- |
| STM32F103 最小系统板 | 1 | ✅ 已就绪 | C8T6 / RBT6 |
| ST-Link V2 仿真器 | 1 | ✅ 已就绪 | SWD 烧录 |
| 面包板 | 1 | ✅ 已就绪 | |
| 绿色 LED 灯 | 1 | ✅ 已就绪 | 本次实验对象 |
| 杜邦线（母对母） | 若干 | ✅ 已就绪 | |
| **1KΩ 金属膜电阻** | 1 | ⏳ **待添加** | 限流用，见文末「遗留问题」 |
| **USB Micro 数据线** | 1 | ⏳ **待添加** | 给开发板独立供电 |

---

## 💻 软件环境配置

1. **安装 Arduino IDE (2.x)**。
2. **配置开发板管理器**：`文件` → `首选项` → 「附加开发板管理器网址」中填入 STM32 官方索引地址：
   ```
   https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json
   ```
   > 注：若遇网络代理问题，可改用**清华镜像源**。
3. **安装核心包**：`工具` → `开发板` → `开发板管理器`，搜索并安装 `STM32 MCU based boards`。
4. **安装烧录工具**：安装 `STM32CubeProgrammer`，Arduino IDE 底层依赖它通过 ST-Link 进行 SWD 烧录。
5. **IDE 关键设置**：

| 设置项 | 取值 |
| --- | --- |
| Board | `Generic STM32F1 series` |
| Board Part Number | `BluePill F103C8` |
| U(S)ART support | `Enabled (generic 'Serial')` |
| Upload method | `STM32CubeProgrammer (SWD)` |

---

## 🔌 硬件接线

### ST-Link V2 ↔ STM32（SWD）

| ST-Link V2 | STM32 引脚 | 说明 |
| --- | --- | --- |
| SWDIO | `PA13` (SWDIO) | 数据线 |
| SWCLK | `PA14` (SWCLK) | 时钟线 |
| GND | `GND` | 共地，**必须接** |
| 3.3V | `3.3V` | 若板子已通过 Micro USB 供电，此线可不接 |

### LED 电路

```
PA5 ──►│── 面包板 GND 轨 ── STM32 GND
     绿色 LED
```

- `PA5` → 绿色 LED **长脚（正极）**
- LED **短脚（负极）** → 面包板 GND 轨 → STM32 的 `GND`

> ⚠️ **注意**：目前为了测试硬件，暂时**未串联 1KΩ 限流电阻**，导致 LED 亮度较暗，后续需补上。

📷 **实拍接线**（STM32F103C8T6 + 面包板外部绿色 LED）：

![面包板接线实拍](../Image_and_video/day01_green_led_breadboard.jpg)

---

## 💻 核心代码

代码位于 [`firmware/Test_LED/Test_LED.ino`](../firmware/Test_LED/Test_LED.ino)：

```cpp
// 方法一：测试板载 LED（无需外接元件，推荐先测试这个）
//#define LED_PIN PC13  // 大多数 STM32F103 最小系统板的板载 LED 在 PC13，低电平点亮

// 方法二：测试接在面包板上的外部 LED（本次实验使用，高电平点亮）
#define LED_PIN PA5

void setup() {
  // 初始化 LED 引脚为输出模式
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_PIN, HIGH);  // 点亮 LED（外部 LED 为高电平点亮）
  delay(500);                   // 等待 500 毫秒
  digitalWrite(LED_PIN, LOW);   // 熄灭 LED
  delay(500);                   // 等待 500 毫秒
}
```

**参数说明**

| 位置 | 含义 | 可调范围 |
| --- | --- | --- |
| `#define LED_PIN PA5` | 外部 LED 接在 `PA5` | 改成 `PC13` 可改测板载 LED（此时需交换高低电平） |
| `delay(500)` | 亮 / 灭各持续 500 ms | 改小（如 `100`）闪得更快，改大（如 `1000`）闪得更慢 |

---

## 🚧 今日踩坑与解决记录（Troubleshooting）

### 1. Arduino IDE 开发板索引下载失败（网络代理报错）

- **报错现象**：
  ```
  Some indexes could not be updated... read tcp 192.168.0.115... wsarecv...
  ```
- **原因**：IDE 设置了系统代理，或网络受限。
- **解决**：在首选项中**关闭代理**，使用「无代理」并重启 IDE；或更换**清华大学镜像源**。

### 2. 上传报错 `No debug probe detected`

- **报错现象**：点击上传后报 `No debug probe detected`。
- **原因**：`STM32CubeProgrammer` 软件正在后台运行，**独占占用了 ST-Link 硬件资源**。
- **解决**：关闭 STM32CubeProgrammer 软件，**拔插 ST-Link 释放资源**，再返回 Arduino IDE 点击上传。

### 3. 绿灯不亮

- **原因 1**：代码中 `LED_PIN` 默认定义成了 `PC13`（板载红灯），实际外部 LED 接的是 `PA5`。
- **原因 2**：STM32 的**板载 LED** 是 `LOW` 点亮，而**外部 LED** 需要 `HIGH` 才是点亮。
- **解决**：修改代码 `#define LED_PIN PA5`，并**交换 `digitalWrite` 的高低电平逻辑**。

> 💡 **一句话总结**：STM32 的「板载 LED」和「外接 LED」极性相反 ——
> `PC13` 板载红灯**低电平点亮**，`PA5` 外接 LED**高电平点亮**。换引脚时别忘了同步改逻辑。

---

## ✅ 今日产出

| 项目 | 状态 |
| --- | --- |
| 开发板核心包（STM32duino）安装完成 | ✅ |
| ST-Link V2 (SWD) 烧录链路打通 | ✅ |
| 外部 LED 成功闪烁（`PA5`，500 ms 周期） | ✅ |
| 代码归档到 [`firmware/Test_LED/`](../firmware/Test_LED/Test_LED.ino) | ✅ |

---

## 🚀 下一步计划

- [ ] 补上 **1KΩ 限流电阻**，解决 LED 亮度偏暗的问题
- [ ] 补一根 **USB Micro 数据线**，让开发板可独立供电（脱离 ST-Link 的 3.3V）
- [ ] 尝试**板载 LED（PC13）**版本，验证「低电平点亮」的结论
- [ ] 加入**按键输入**，实现「按一下切换亮灭」
- [ ] 用 `millis()` 替换 `delay()`，实现**非阻塞闪烁**（为后续多任务打基础）
- [ ] 打通**串口输出**（`Serial.print`），方便后续调试与日志

---

## 🔗 参考链接

- [STM32duino 官方文档](https://github.com/stm32duino/Arduino_Core_STM32)
- [STM32 开发板管理器索引文件](https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json)
- [STM32CubeProgrammer 下载页](https://www.st.com/en/development-tools/stm32cubeprog.html)
