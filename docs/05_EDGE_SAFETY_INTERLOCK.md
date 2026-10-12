# STM32 进阶 05：边缘安全互锁 —— 红外急停 + 舵机阀门

> **本篇目标**：给执行终端加上「本地最高优先级安全逻辑」—— 人体一靠近，**毫秒级切断电机与舵机动力**，并把这个状态上报给上位机
> **适用环境**：STM32F103C8T6（BluePill）· Arduino IDE 2.x · STM32duino 核心库 · ST-Link V2
> **完成标志**：手靠近红外传感器 → 风扇立刻停、阀门立刻关、OLED 弹出 `!! SAFETY ALERT !!`、串口上报 `S:0`
> ⚠️ **本篇最后会解释：为什么安全逻辑必须写在「端」上，而不能交给云端 —— 这是本篇最重要的一条经验**

---

## 📌 为什么安全逻辑必须放在「端」上

上一版系统已经能「树莓派下发指令 → 有 2 秒心跳上报」，但它有一个致命短板：**所有决策都在链路的另一端**。

假设人手已经伸进风扇：

| 方案 | 链路 | 实际延迟 |
| --- | --- | --- |
| 云端判断 | 传感器 → STM32 → ESP32 → WiFi → MQTT → 树莓派判断 → MQTT → WiFi → ESP32 → UART → STM32 | **几百毫秒起，网络一抖就变几秒**，断网直接失效 |
| **边缘判断（本篇）** | 传感器 → STM32 的一个 `if` → 切断 PWM | **微秒 ~ 毫秒级，与网络完全无关** |

工业现场的共识是：**安全回路必须能在没有网络的情况下独立生效**。所以本篇把红外急停放进 `loop()` 的**第一个任务**，它的优先级高于按键、高于温控、高于远程指令。

---

## 🛠️ 硬件准备

| 硬件 | 数量 | 说明 |
| --- | :---: | --- |
| STM32F103 最小系统板 | 1 | C8T6（BluePill 板型） |
| ST-Link V2 仿真器 | 1 | 烧录用 |
| **TCRT5000 红外避障传感器模块** | 1 | 本篇核心，数字输出 `DO` 接 `PB1` |
| **SG90 舵机（模拟阀门）** | 1 | 信号线接 `PB0` |
| TB6612FNG 电机驱动模块 | 1 | 驱动风扇，沿用 04 篇接法 |
| 130 直流电机 + 扇叶 | 1 | 被保护的对象 |
| DHT11 温湿度模块 | 1 | 数据线接 `PA0` |
| SSD1306 OLED（I2C 128×64） | 1 | 现场状态显示 |
| 两脚常开按键 | 1 | 接 `PA2`，手动开关风扇 |
| 外部 5V 电源 | 1 | ⚠️ 舵机 + 电机必须**独立供电**，与 STM32 **共地** |
| 面包板 / 杜邦线 | 若干 | |

---

## 🔌 硬件接线

### 控制侧（STM32 ↔ 各模块）

| STM32 引脚 | 接到 | 作用 |
| --- | --- | --- |
| `PB1` | TCRT5000 的 `DO` | **红外避障输入**（本篇核心） |
| `PB0` | SG90 舵机 信号线（橙） | 阀门开（90°）/ 关（0°） |
| `PB12` | TB6612 `AIN1` | 电机方向 |
| `PB13` | TB6612 `AIN2` | 电机方向 |
| `PA5` | TB6612 `PWMA` | 电机转速（PWM） |
| `PA0` | DHT11 `DATA` | 温湿度 |
| `PA2` | 按键一端（另一端接 `GND`） | `INPUT_PULLUP`，按下 = `LOW` |
| `PB6` / `PB7` | OLED `SCL` / `SDA` | 硬件 I2C（`U8G2_..._HW_I2C`） |
| `PA9` / `PA10` | ESP32 `D16` / `D17` | UART **9600**, 8N1 ↔ 网关 |

### 动力侧（务必分开供电）

| 模块 | 供电 | 备注 |
| --- | --- | --- |
| TCRT5000 | `3.3V` / `5V` + `GND` | 模块自带比较器，`DO` 输出数字电平 |
| SG90 舵机 | **独立 5V** | 堵转电流可达 1A，直接从板子取电会复位 |
| TB6612 `VM` / 电机 | **独立 5V** | 见 04 篇踩坑 |
| TB6612 `VCC` / `STBY` | `3.3V` | `STBY` 必须拉高，否则电机不动 |

> ⚠️ **所有 GND 必须连在一起**（STM32 / 舵机电源 / 电机电源 / ESP32），这是 04 篇用血换来的结论。

---

## 💻 第一步：先把红外传感器读通

TCRT5000 模块上有两个输出：`AO`（模拟量）和 `DO`（数字量，带电位器可调阈值）。避障场景用 `DO` 就够了：

```cpp
#define IR_PIN PB1

void setup() {
  pinMode(IR_PIN, INPUT);   // 模块已自带上下拉，这里用普通 INPUT
}

void loop() {
  int irState = digitalRead(IR_PIN);
  // 有遮挡（手靠近）= LOW；无障碍 = HIGH
  Serial.println(irState == LOW ? "BLOCKED" : "CLEAR");
}
```

**先单独验证这一步**：打开串口监视器，用手在传感器前晃，看输出是否跟着翻转。

> ⚠️ 不同批次模块的极性可能相反（有的遮挡输出 `HIGH`）。**以你的模块实测为准**，如果反了，把后面的 `LOW` / `HIGH` 对调即可。

---

## 💻 第二步：写一个「最高优先级」的急停

急停要的不是「关一次」，而是**进入警报状态并锁住**。所以用 `isSafetyAlert` 这个状态变量，并且只在**边沿**上执行动作（避免每圈循环都重复关一次、刷屏）：

```cpp
bool isSafetyAlert = false;   // 红外安全警报状态

// ===== 任务1：边缘计算最高优先级 · 红外安全逻辑 =====
int irState = digitalRead(IR_PIN);

if (irState == LOW && !isSafetyAlert) {
  isSafetyAlert = true;
  // 立即执行本地安全动作：强制关闭一切
  isFanOn = false;
  isValveOpen = false;
  analogWrite(PWMA, 0);       // 电机动力归零
  valveServo.write(0);        // 舵机回位（关阀）
  Serial.println("EMERGENCY_STOP: Object detected!");
}
else if (irState == HIGH && isSafetyAlert) {
  isSafetyAlert = false;      // 障碍物移除，解除警报
  Serial.println("Safety Clear.");
}
```

这段代码里有两个刻意为之的细节：

1. **`analogWrite(PWMA, 0)` 而不是 `digitalWrite`**：直接把 PWM 占空比清零，绕开所有上层状态机，**不判断也不依赖任何人**；
2. **`delay()` 一个都没有**：急停路径上不能有任何阻塞，否则「急停」本身就是延迟。

---

## 💻 第三步：给每条控制路径都加上安全锁

这是本篇**最容易写错、也最值得抄走**的一节。

急停只关一次是不够的：温控逻辑每 2 秒跑一次，如果它只看温度，就会在急停之后**把风扇重新打开**。所以每一条「会打开设备」的路径，都必须加同一把锁 `!isSafetyAlert`：

| 路径 | 加锁方式 | 说明 |
| --- | --- | --- |
| 本地按键开风扇 | `currentButtonState == LOW && lastButtonState == HIGH && !isSafetyAlert` | 警报中按键无效 |
| 自动温控（>28℃ 开风扇/阀门） | `if (!isSafetyAlert) { ...温控逻辑... }` | **整段包起来**，否则每 2 秒把风扇抢回去 |
| 远程 `FAN_ON` | `if (cmd == "FAN_ON" && !isSafetyAlert)` | 云端不能绕过本地安全 |
| 远程 `VALVE_OPEN` | `if (cmd == "VALVE_OPEN" && !isSafetyAlert)` | 同上 |
| 远程 `FAN_OFF` / `VALVE_CLOSE` | **不加锁** | 关闭类指令永远允许（安全方向放行） |

```cpp
// 远程指令：开类动作加锁，关类动作放行
if (cmd == "FAN_ON"  && !isSafetyAlert) { isFanOn = true;  analogWrite(PWMA, 150); }
if (cmd == "FAN_OFF")                   { isFanOn = false; analogWrite(PWMA, 0);   }
if (cmd == "VALVE_OPEN"  && !isSafetyAlert) { isValveOpen = true;  valveServo.write(90); }
if (cmd == "VALVE_CLOSE")                   { isValveOpen = false; valveServo.write(0);  }
```

> 🔑 **一句话原则：安全状态只锁「开」，不锁「关」。** 这样即使警报误触发，系统也始终能被关到安全态。

---

## 💻 第四步：把安全状态显示出来、报上去

现场必须**一眼看出**设备处于什么状态，所以 OLED 和串口上报都要带上安全位：

```cpp
// OLED 第三行：正常 / 警报
if (isSafetyAlert) u8g2.print("!! SAFETY ALERT !!");
else               u8g2.print("Status: Normal");
```

串口上报格式在原来的 `T/H/F/V` 后面加了 **`S`** 字段：

```cpp
Serial.print("T:"); Serial.print(currentTemp);
Serial.print(",H:"); Serial.print(currentHumi);
Serial.print(",F:"); Serial.print(isFanOn ? 1 : 0);
Serial.print(",V:"); Serial.print(isValveOpen ? 1 : 0);
Serial.print(",S:"); Serial.println(isSafetyAlert ? 0 : 1);  // S:0 = 警报，S:1 = 安全
```

| 字段 | 含义 | 取值 |
| --- | --- | --- |
| `T` | 温度 | 例如 `25.00` |
| `H` | 湿度 | 例如 `60.00` |
| `F` | 风扇 | `1` 开 / `0` 关 |
| `V` | 阀门（舵机） | `1` 开 / `0` 关 |
| **`S`** | **安全状态** | **`1` 安全 / `0` 警报** |

一行完整示例：

```text
T:25.00,H:60.00,F:0,V:0,S:1
```

> 💡 这条数据会被 ESP32 原样发布到 MQTT 主题 `home/fan/status`，树莓派侧只要按 `,` 和 `:` 切分就能还原全部状态。

---

## ✅ 验证：你应该看到什么

1. **上电**：串口打印 `STM32 System Booted with Edge Safety...`，OLED 显示温度、`Fan:OFF Valve:CLOSE`、`Status: Normal`；
2. **正常上报**（每 2 秒一行）：`T:25.00,H:60.00,F:0,V:0,S:1`；
3. **手靠近红外**：风扇停、舵机回 0°、OLED 第三行变 `!! SAFETY ALERT !!`、串口打印 `EMERGENCY_STOP: Object detected!`，随后上报变成 `...,S:0`；
4. **警报中按按键 / 远程下发 `FAN_ON`**：风扇**不应转起来**（这条最能说明互锁生效）；
5. **手移开**：串口打印 `Safety Clear.`，上报恢复 `S:1`，此时温控/按键/远程才重新生效。

---

## 🚧 踩坑与排错（本篇重点）

### 1. 急停之后，风扇又自己转起来了

**现象**：手靠近时风扇停，几秒后（没移开手）又转。

**原因**：急停只执行了一次「关」，而温控任务每 2 秒判断一次温度，发现 `>28℃` 就又把风扇打开了 —— 两个任务的优先级没拉开。

**解决**：把温控整段用 `if (!isSafetyAlert) { ... }` 包起来；远程 `FAN_ON` / `VALVE_OPEN` 也加同一把锁。**凡是「打开」的动作都要问一句：现在安全吗？**

### 2. 传感器「一碰就报、一松就好」，但手指停在前面时状态乱跳

**原因**：TCRT5000 的阈值电位器调得太灵敏，或者传感器离反射面太远导致输出在临界值抖动。

**解决**：调模块上的电位器，让「有遮挡」和「无遮挡」两种情况的串口读数**明显分开**；必要时再加 2~3 次连续采样一致才认账（软件消抖）。

### 3. 舵机一转，STM32 就复位（板载红灯闪一下）

**原因**：和 04 篇的电机是同一个坑 —— SG90 启动/堵转电流可达 1A，从板子的 5V 取电会把电压拉低触发欠压复位。

**解决**：舵机接**独立 5V 电源**，并与 STM32 **共地**；舵机信号线仍接 `PB0`。`valveServo.write()` 之后不要用 `delay()` 等它转完。

### 4. OLED 与舵机同时工作时屏幕闪烁 / 花屏

**原因**：两者共用 5V 且线太长，I2C 时序被电源波动干扰。

**解决**：OLED 用 `3.3V` 供电更稳；缩短 I2C 走线；屏幕刷新保持 500 ms 一次，别放进 `loop()` 每圈都刷。

### 5. 红外一直报「安全」，怎么挡都不触发

**原因**：`DO` 接到了错误的引脚，或传感器模块没供电（TCRT5000 需要 VCC，不是无源器件）。

**解决**：先单独跑「第一步」的最小代码验证 `PB1` 读数；确认模块 VCC/GND 接好，并测量 `DO` 在遮挡时的电平变化。

---

## 🔎 原理小结：为什么「本地互锁」比「云端下发关闭」快

| 环节 | 延迟量级 | 断网时 |
| --- | --- | --- |
| STM32 `digitalRead` + `analogWrite` | **微秒 ~ 毫秒** | ✅ 照常工作 |
| MQTT 一次往返（同局域网） | 10 ~ 100 ms | ❌ 直接失效 |
| 经树莓派上层判断后再回传 | 100 ms ~ 数秒 | ❌ 直接失效 |

结论：**越靠近危险的判断越要放在端侧**。云端负责「记录、可视化、策略下发」，端侧负责「即时反应」——这就是「云-边-端」架构里 `边` 的价值所在。

---

## 📈 继续往下

- [ ] 把红外换成**测距**（VL53L0X / HC-SR04），按距离分级降速而不是直接急停
- [ ] 给急停加**软启动恢复**：`analogWrite` 从 0 缓慢升到 150，避免恢复瞬间电流冲击
- [ ] 把 `S` 字段做成**报警次数统计**，掉电保存到 Flash
- [ ] 用**外部中断**（`attachInterrupt`）响应红外，替代 `loop()` 轮询，进一步压低延迟
- [ ] 在树莓派侧做**告警弹窗 / 记录**，把每次 `S:0` 存进数据库

## 🔗 参考链接

- [Arduino Core STM32（STM32duino）](https://github.com/stm32duino/Arduino_Core_STM32)
- [U8g2 图形库](https://github.com/olikraus/u8g2)
- [Servo 库（Arduino）](https://www.arduino.cc/reference/en/libraries/servo/)
- 配套仓库：[esp32（无线网关 · 断网缓存）](https://github.com/Arduino-STM32-Dev/esp32)
