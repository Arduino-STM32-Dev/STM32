# STM32 入门 04：TB6612 驱动直流电机与多档位风扇

> **本篇目标**：用 TB6612FNG 驱动 130 直流电机，再用一个按键循环切换 0~3 档风速
> **适用环境**：STM32F103C8T6（BluePill）· Arduino IDE 2.x · STM32duino 核心库 · ST-Link V2
> **完成标志**：按一下按键，风扇按 **停 → 低速 → 中速 → 高速 → 停** 循环
> ⚠️ **本篇最后会解释：为什么电机必须用「独立电源」—— 这是本篇最重要的一条经验**

---

## 📌 为什么不能直接用 IO 口驱动电机

STM32 一个 IO 口的输出电流只有 **20 mA 量级**，而 130 直流电机转起来就要几百 mA，启动瞬间的**浪涌电流**可能接近 1A。

直接拿 IO 口去驱动电机的后果：

- 轻则电机不转、单片机复位；
- 重则**烧掉 IO 口**。

所以中间必须要有一个「功率放大 + 方向控制」的器件 —— 常用的是 **H 桥驱动芯片**。本篇用的是 **TB6612FNG**：

| 参数 | 值 |
| --- | --- |
| 通道数 | 双 H 桥（本篇只用 A 通道） |
| 连续电流 | 约 1.2 A / 通道 |
| 峰值电流 | 约 3.2 A |
| 电机电压 `VM` | 最高 15 V |
| 逻辑电压 `VCC` | 2.7 ~ 5.5 V（可直接用 3.3V） |

它把「**方向**」和「**速度**」拆成两组引脚来控制，思路非常清晰：

- `AIN1` / `AIN2` → 决定**方向**（正转 / 反转 / 停 / 刹车）
- `PWMA` → 决定**速度**（PWM 占空比，和 02 篇的调光是一回事）

---

## 🛠️ 硬件准备

| 硬件 | 数量 | 说明 |
| --- | :---: | --- |
| STM32F103 最小系统板 | 1 | C8T6 / RBT6（BluePill 板型） |
| ST-Link V2 仿真器 | 1 | 烧录用 |
| **TB6612FNG 电机驱动模块** | 1 | 本篇核心 |
| **130 直流电机 + 扇叶** | 1 | 输出执行器 |
| 两脚常开按键 | 1 | 接 `PA2`，切换档位 |
| **外部 5V 电源** | 1 | 电池盒 / 充电宝。⚠️ **强烈建议**，见踩坑 2 |
| 面包板 | 1 | |
| 杜邦线（母对母） | 若干 | ⚠️ 质量参差，见踩坑 1 |
| USB Micro 数据线 | 1 | 给 STM32 供电 |

---

## 🔌 硬件接线

接线分**控制侧**和**动力侧**两部分，务必都接对。

### 控制侧：STM32 → TB6612

| STM32 引脚 | TB6612 引脚 | 作用 |
| --- | --- | --- |
| `PB12` | `AIN1` | A 通道方向 1 |
| `PB13` | `AIN2` | A 通道方向 2 |
| `PA5` | `PWMA` | A 通道调速（PWM） |
| `3.3V` | `VCC` | 逻辑供电 |
| `3.3V` | `STBY` | **使能驱动** ⚠️ 必须接高电平 |
| `GND` | `GND` | 共地 |

### 动力侧：TB6612 → 电机 / 电源

| TB6612 引脚 | 接到 | 说明 |
| --- | --- | --- |
| `AO1` / `AO2` | 直流电机两极 | **不分正负**，转向反了就对调这两根 |
| `VM` | 外部 5V 电源 **正极** | 电机电源输入 |
| `GND` | 外部电源 **负极**，同时接 STM32 `GND` | ⚠️ **必须共地** |
| `STBY` | `3.3V` | 待机控制，不接高电平电机纹丝不动 |

### 按键

| 按键一端 | 另一端 |
| --- | --- |
| `PA2` | `GND` |

`INPUT_PULLUP` 内部上拉，松开 `HIGH`，按下 `LOW`。

### 方向真值表

| `AIN1` | `AIN2` | 电机状态 |
| :---: | :---: | --- |
| 0 | 0 | 停止（滑行） |
| 1 | 0 | **正转** |
| 0 | 1 | 反转 |
| 1 | 1 | 刹车（短路制动） |

### 接线实拍

![TB6612 驱动风扇接线实拍](../Image_and_video/Fan-control.jpg)

---

## 💻 第一步：先让电机转起来

别一上来就写复杂逻辑。先用「按住按键就转、松开就停」把**硬件链路**验证通：

代码位于 [`firmware/use_button_control_fan/use_button_control_fan.ino`](../firmware/use_button_control_fan/use_button_control_fan.ino)：

```cpp
#define AIN1 PB12
#define AIN2 PB13
#define PWMA PA5
#define BUTTON_PIN PA2  // 换到 PA2 引脚了！

void setup() {
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // 启用内部上拉
}

void loop() {
  // 读取PA2的状态
  if (digitalRead(BUTTON_PIN) == LOW) {
    // 如果按键被按下（接了GND）
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);
    analogWrite(PWMA, 10); // 用低速运转测试，避免电流过大导致复位
  } else {
    // 按键松开
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, LOW);
    analogWrite(PWMA, 0);   // 停转
  }
  delay(10);
}
```

> 💡 注意这里的 `analogWrite(PWMA, 10)` —— 故意只给了一个**很低的速度**。这是为了在验证阶段尽量压小电流，避免电机一启动就把单片机带复位（原因见踩坑 2）。

---

## 💻 第二步：多档位风扇（状态机）

硬件链路通了，再引入「档位」的概念。

代码位于 [`firmware/MultiSpeed_Fan/MultiSpeed_Fan.ino`](../firmware/MultiSpeed_Fan/MultiSpeed_Fan.ino)：

```cpp
#define AIN1 PB12
#define AIN2 PB13
#define PWMA PA5
#define BUTTON_PIN PA2

int fanLevel = 0; // 0~3档
int lastButtonState = HIGH;

void setup() {
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  setFanSpeed(0);
}

void loop() {
  int currentButtonState = digitalRead(BUTTON_PIN);
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    fanLevel++;
    if (fanLevel > 3) fanLevel = 0;
    setFanSpeed(fanLevel);
    delay(50); // 软件消抖
  }
  lastButtonState = currentButtonState;
}

void setFanSpeed(int level) {
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  switch (level) {
    case 0: analogWrite(PWMA, 0);   break;
    case 1: analogWrite(PWMA, 90);  break;
    case 2: analogWrite(PWMA, 180); break;
    case 3: analogWrite(PWMA, 255); break;
  }
}
```

### 档位表

| `fanLevel` | `analogWrite(PWMA, ...)` | 占空比 | 效果 |
| :---: | :---: | :---: | --- |
| 0 | 0 | 0% | 停 |
| 1 | 90 | 约 35% | 低速 |
| 2 | 180 | 约 71% | 中速 |
| 3 | 255 | 100% | 高速 |

### 代码怎么读

- `fanLevel++` 之后紧跟 `if (fanLevel > 3) fanLevel = 0;` —— 这就是一个最简单的**状态机**：状态在 0/1/2/3 之间循环，不会越界；
- 按键依旧用 02、03 篇的**边沿检测**写法，保证「按住不放也只切一档」；
- 真正改动电机的地方全部收进 `setFanSpeed()` 一个函数里，以后想加速度曲线、加软启动，只改这一处。

**想动手改参数？**

| 位置 | 含义 | 可以怎么改 |
| --- | :--- | --- |
| `analogWrite(PWMA, 90)` 等三处 | 各档速度 | 调成 `60 / 130 / 200`，档位手感更细腻 |
| `if (fanLevel > 3)` | 最高档位 | 改成 `> 4` 并补一个 `case 4`，就能做四档 |
| `delay(50)` | 消抖延时 | 更严谨的写法是把它**挪到读取之后**再判断电平，当前写法只是延迟了循环 |

---

## ✅ 验证：你应该看到什么

1. Arduino IDE 底部提示 `上传成功`；
2. 上电后风扇**不转**（0 档）；
3. 按一下按键 → 低速转；再按 → 中速；再按 → 高速；再按 → 停，如此循环；
4. 全程 STM32 板载红灯**不闪**。

只要第 4 条不满足（红灯闪了一下），就说明复位了 —— 直接跳到下面的踩坑 2。

---

## 🚧 踩坑与排错（本篇重点）

### 1. 换了一根杜邦线之后，按键彻底失灵

- **现象**：更换按键接线后，电机和按键**都没有反应**了。
- **排查**：用「**短接法**」—— 把按键的信号脚直接短接到 `GND`（相当于人工按下按键），依然毫无反应。这就把嫌疑从「按键坏了」转移到了「导线不通」上。
- **解决**：换一根全新的杜邦线，恢复正常。
- **原因**：杜邦线内部是几根极细的金属丝，反复插拔、弯折后**内部断裂但外皮完好**，肉眼看不出任何异常。

> 💡 **经验**：嵌入式开发里杜邦线内部断裂极其常见，是「玄学故障」的头号嫌疑人。当某个输入莫名其妙没反应时，**第一件事就是把它直接短接到 GND 试试** —— 能区分是「外设/线的问题」还是「代码/引脚的问题」。

### 2. 一按按键，STM32 就复位（本篇最核心的坑）

- **现象**：不按键时电机转得好好的；**一旦按下按键（启动电机），STM32 毫无反应，电机也不转**。
- **排查过程**（这个思路很值得记住）：
  1. **拔掉电机**，只用板载 LED（`PC13`）测试按键输入 —— 确认 `PA2` 引脚和按键逻辑**完全正常**。这一步就排除了「代码写错 / 引脚选错」。
  2. 把电机接回去，按下按键的**瞬间，能观察到 STM32 板载红灯闪了一下**。
- **原因**：STM32 的 Micro USB 供电能力有限（约 **500 mA**）。电机启动瞬间的**浪涌电流极大**，把 5V 电压瞬间拉低，STM32 因为**欠压自动复位**——代码从 `setup()` 重新执行了一遍，自然维持不住电机的运转。
- **解决**：**引入外部独立电源**。
  - 电池盒的正极 → TB6612 的 `VM`，负极 → TB6612 的 `GND`；
  - **必须把外部电源的 `GND` 与 STM32 的 `GND` 连在一起（共地）**，否则控制信号没有共同的参考电平，电机照样不转；
  - 让 STM32 继续用 USB 供电，电机吃电池的电 —— 彻底隔离干扰。

> 💡 **两个通用经验**：
> ① **板载红灯闪一下 = 复位**。这是判断「是不是欠压复位」最快的方法，比接串口打印还直接。
> ② **凡是「一动大功率负载就死机」的现象，先怀疑供电，再怀疑代码。** 控制板和功率器件分电源、共地，是嵌入式里最基本的做法。

### 3. 电机完全不转 / 只会朝一个方向转

按顺序检查：

1. **`STBY` 有没有接 3.3V** —— 这是最容易漏的一根线，不接高电平 TB6612 一直处于待机，电机纹丝不动；
2. **`AIN1` / `AIN2` 是否一个高一个低** —— 两个都低就是「停止」，真值表见上；
3. **`VM` 上有没有电** —— 万用表量一下外部电源；
4. **外部电源和 STM32 是否共地**；
5. 转向反了：把电机的两根线 `AO1` / `AO2` **对调**即可。

---

## 🔎 原理小结：为什么会「复位」，又为什么要「共地」

**复位**：STM32 内部有一个欠压检测电路。当供电电压掉到阈值以下，芯片会强制复位，让程序从头开始跑 —— 这是保护机制，不是芯片坏了。电机启动时的浪涌电流在电源内阻上产生了一个瞬时压降，把 5V 拉到了阈值以下，于是芯片重启。**供电能力越弱（USB 只有 500mA），这个现象越明显。**

**共地**：电路的「电压」永远是两点之间的**电位差**。STM32 输出的 `HIGH` = 3.3V 是**相对于它自己的 GND** 而言的。如果两块板子的 GND 不连在一起，TB6612 就无从判断 STM32 给的信号到底是高还是低 —— 电流也没有回路可走。所以**只要有两套电源，就必须把 GND 接在一起**。

---

## 🚀 继续往下

- [ ] 接入外部 5V 电池盒，彻底验证大功率驱动下的稳定性
- [ ] **软启动**：用 `analogWrite()` 从 0 逐渐升到目标档位，避免瞬间浪涌电流冲击
- [ ] 用 `millis()` 替换 `delay()`，做非阻塞的多档位风扇
- [ ] 接入 14 号（NTC）热敏电阻读取环境温度，让风扇**根据温度自动调速**
- [ ] 加一块 LCD / OLED，把当前温度、档位显示出来
- [ ] 接上**无源蜂鸣器**，按键时给一声提示音 → 见 [03 篇](03_BUTTON_AND_BUZZER.md)

---

## 🔗 参考链接

- [TB6612FNG 数据手册](https://www.toshiba-semiconductors.com/)
- [Arduino `analogWrite()` 文档](https://www.arduino.cc/reference/en/language/functions/analog-io/analogwrite/)
- [STM32duino 官方文档](https://github.com/stm32duino/Arduino_Core_STM32)
