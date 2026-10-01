# STM32 入门 02：PWM 调光与呼吸灯

> **本篇目标**：理解 PWM 与「占空比」，用 `analogWrite()` 做一颗呼吸灯，并用按键控制它的开关
> **适用环境**：STM32F103C8T6（BluePill）· Arduino IDE 2.x · STM32duino 核心库 · ST-Link V2
> **完成标志**：上电后 LED 熄灭；按一下按键，LED 开始「渐亮 → 渐暗」循环；再按一下，熄灭

---

## 📌 从「闪烁」到「调光」

01 篇里 LED 只有**亮**和**灭**两种状态 —— 因为 `digitalWrite()` 只能让引脚输出 0V 或 3.3V。

可我们想要「半亮」，怎么办？数字引脚本身给不出 1.65V，但可以**让它在 0V 和 3.3V 之间飞快地切换**：

- 一半时间给高、一半时间给低 → 亮和灭交替得太快，人眼跟不上，看到的就是**半亮**；
- 高电平占的时间越多，看起来就越亮。

这就是 **PWM（脉冲宽度调制）**。其中「高电平占一个周期的比例」叫**占空比**：

| 占空比 | 效果 |
| :---: | --- |
| 0% | 全灭 |
| 25% | 微亮 |
| 50% | 半亮 |
| 100% | 全亮 |

Arduino 把这件事封装成了一个函数：

```cpp
analogWrite(pin, value);   // value 取 0~255
```

占空比 = `value / 255`。写 `0` 就是全灭，写 `255` 就是全亮，写 `127` 约等于半亮。

> ⚠️ **不是所有引脚都能输出 PWM**。如果你写 `analogWrite()` 后发现亮度只有「全亮 / 全灭」两种跳变，多半是碰上了一个不支持 PWM 的引脚，换一个脚试试。
> `analogWrite()` 输出的是一路**固定频率**的方波（STM32duino 上大约在 1 kHz 量级，具体随核心版本而定），我们改变的只是它的**占空比**。

---

## 🛠️ 硬件准备

在 01 篇的基础上，多一个按键：

| 硬件 | 数量 | 说明 |
| --- | :---: | --- |
| STM32F103 最小系统板 | 1 | C8T6 / RBT6（BluePill 板型） |
| ST-Link V2 仿真器 | 1 | 烧录用 |
| 面包板 | 1 | |
| LED 灯 | 1 | 本篇用绿色 LED，接 `PA5` |
| **两脚常开按键** | 1 | 接 `PA1`，用来开关呼吸灯 |
| 1KΩ 电阻 | 1 | 限流，**建议接上** |
| 杜邦线（母对母） | 若干 | |
| USB Micro 数据线 | 1 | 可选，给开发板独立供电 |

---

## 🔌 硬件接线

### LED 电路（沿用 01 篇）

```
PA5 ──[1KΩ]──►│── 面包板 GND 轨 ── STM32 GND
            绿色 LED
```

- `PA5` → 电阻 → LED **长脚（正极）**
- LED **短脚（负极）** → 面包板 GND 轨 → STM32 的 `GND`

### 按键电路

```
PA1 ──┬── 按键 ── GND
      └── （引脚内部上拉，松开时读到 HIGH）
```

- 按键一端 → `PA1`，另一端 → `GND`
- **不需要外接上拉电阻**：代码里用 `INPUT_PULLUP` 打开了 STM32 芯片**内部**的上拉电阻

| 按键状态 | `PA1` 读到 |
| --- | --- |
| 松开 | `HIGH`（被内部上拉拉到 3.3V） |
| 按下 | `LOW`（被直接拉到 GND） |

> 💡 两脚按键**没有极性**，正接反接都能用。它本质上就是一个「按下去就接通」的开关。

---

## 💻 核心代码

代码位于 [`firmware/PWM_LED/PWM_LED.ino`](../firmware/PWM_LED/PWM_LED.ino)：

```cpp
#define LED_PIN PA5
#define BUTTON_PIN PA1

bool isBreathing = false; // 记录呼吸灯是否开启
int lastButtonState = HIGH;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  // --- 第一部分：按键检测逻辑 ---
  int currentButtonState = digitalRead(BUTTON_PIN);
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    isBreathing = !isBreathing; // 切换开关状态
    if (!isBreathing) {
      digitalWrite(LED_PIN, LOW); // 关闭时立刻熄灭
    }
    delay(50); // 消抖
  }
  lastButtonState = currentButtonState;

  // --- 第二部分：呼吸灯执行逻辑 ---
  if (isBreathing) {
    // 渐亮
    for (int brightness = 0; brightness <= 255; brightness++) {
      analogWrite(LED_PIN, brightness);
      delay(5); // 这里延时小一点，呼吸快一些
      // 如果中途按下了按键，立刻退出这个循环，提高响应速度
      if (digitalRead(BUTTON_PIN) == LOW) break; 
    }
    // 渐暗
    for (int brightness = 255; brightness >= 0; brightness--) {
      analogWrite(LED_PIN, brightness);
      delay(5);
      if (digitalRead(BUTTON_PIN) == LOW) break;
    }
  }
}
```

### 代码里三个新东西

| 新东西 | 作用 |
| --- | --- |
| `analogWrite(LED_PIN, brightness)` | 输出 PWM，`brightness` 从 0 到 255 就是由暗到亮 |
| `pinMode(BUTTON_PIN, INPUT_PULLUP)` | 启用**内部上拉**，按键可以直接一头接引脚、一头接 GND |
| `currentButtonState == LOW && lastButtonState == HIGH` | **边沿检测** —— 只在「按下的那一瞬间」触发一次 |

**想动手改参数？**

| 位置 | 含义 | 可以怎么改 |
| --- | --- | --- |
| `delay(5)` | 每一级亮度停留 5 ms | 改大呼吸变慢（如 `15`），改小变快（如 `2`） |
| `brightness <= 255` | 渐亮到最亮为止 | 改成 `<= 127` 就只呼吸到半亮 |
| `bool isBreathing = false` | 上电默认关闭 | 改成 `true` 让呼吸灯上电就启动 |

---

## ✅ 验证：你应该看到什么

1. Arduino IDE 底部提示 `上传成功`；
2. 上电后 **LED 是灭的**；
3. 按一下按键 → LED 开始**由暗到亮、再由亮到暗**地循环；
4. 再按一下 → LED 熄灭。

---

## 🚧 踩坑与提醒

### 1. 按一下，灯却切换了好几次

- **原因**：机械按键在按下和松开的瞬间，触点会「弹跳」几下 —— 几毫秒内电平反复横跳，程序会把它当成按了好多次。这叫**抖动**。
- **解决**：代码里的 `delay(50)` 就是**软件消抖**：检测到按下后先等 50 ms，把这段抖动期跳过去再继续。

### 2. 呼吸灯「卡顿」，按键要按很久才响应

- **原因**：渐亮 / 渐暗的两个 `for` 循环一共要跑 2 × 256 × 5 ms ≈ **2.6 秒**，这段时间里 `delay()` 把 CPU 完全堵住了，程序根本没空去读按键。
- **本篇的折中**：在两个 `for` 循环里各加了一句 `if (digitalRead(BUTTON_PIN) == LOW) break;`，一旦发现按键被按下就立刻跳出，用**主动让路**的方式换回一点响应速度。
- **真正的解法**：用 `millis()` 记录时间、把「渐亮渐暗」拆成状态机，彻底消灭 `delay()`，让按键在任何时刻都能立刻响应。这是后面一篇要练的内容。

### 3. 亮度只有「全亮」和「全灭」两档

- **原因**：这个引脚不支持硬件 PWM，`analogWrite()` 退化成了 `digitalWrite()`。
- **解决**：换一个支持 PWM 的引脚。

---

## 🔎 原理小结：为什么两个 `for` 循环就是「呼吸」

- 「渐亮」：`brightness` 从 0 涨到 255，占空比由 0% 升到 100%，人眼看到的就是光慢慢变亮；
- 「渐暗」：再从 255 掉回 0，光慢慢变暗；
- 一个完整的呼吸周期 ≈ 2 × 256 × 5 ms ≈ **2.6 秒**。

而按键部分用的是**边沿触发**：只有「上一轮是 `HIGH`、这一轮是 `LOW`」这个**变化**才被认作一次按下。如果直接写成 `if (digitalRead(BUTTON_PIN) == LOW)`，那按住不放时 `loop()` 每轮都会翻转一次状态，灯会疯狂闪烁 —— 这也正是 03 篇里按键开关的核心技巧。

---

## 🚀 继续往下

- [ ] 把 `delay(5)` 改成 `15` 或 `2`，观察呼吸速度的变化
- [ ] 用 `millis()` 重写，让按键在任何时刻都能立刻响应
- [ ] 改成「三个档位亮度循环」：暗 → 中 → 亮 → 暗
- [ ] 用 PWM 驱动**无源蜂鸣器**，奏出不同音调 → 见 [03 篇](03_BUTTON_AND_BUZZER.md)
- [ ] 用 PWM 给**直流电机**调速 → 见 [04 篇](04_DC_MOTOR_MULTI_SPEED_FAN.md)

---

## 🔗 参考链接

- [Arduino `analogWrite()` 文档](https://www.arduino.cc/reference/en/language/functions/analog-io/analogwrite/)
- [STM32duino 官方文档](https://github.com/stm32duino/Arduino_Core_STM32)
