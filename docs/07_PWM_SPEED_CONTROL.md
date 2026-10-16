# 07 · ESP32 远程无级调速（FAN_PWM）与 OLED 速度显示

> **本篇目标**：在已有的「红外安全互锁 + DHT11 自动温控 + 舵机阀门 + OLED」基础上，新增 **`FAN_PWM:XXX` 无级调速协议**，让树莓派能把风扇从「开 / 关」调成 **0~255 任意转速**，并在 OLED 与上报数据里体现**当前真实速度**
> **适用环境**：STM32F103C8T6 (BluePill) · Arduino IDE 2.x · STM32duino（Generic STM32F1 series）· ST-Link V2 · ESP32 网关（UART **9600** 8N1）
> **完成标志**：树莓派执行 `mosquitto_pub -h localhost -t "pi/to/esp32" -m "FAN_PWM:200"` → 风扇明显降速（比默认 150 快），OLED 第二行显示 `Fan:ON Spd:200`，串口上报出现 `Spd:200` 字段
> **固件**：[`firmware/PWM-FAN/PWM-FAN.ino`](../firmware/PWM-FAN/PWM-FAN.ino)（202 行）

---

## 📌 为什么需要无级调速

之前的版本只有 `FAN_ON` / `FAN_OFF` 两种状态，自动温控也固定推到 PWM 150。实际部署后立刻遇到两个问题：

| 问题 | 现象 |
| --- | --- |
| **要么太吵、要么没风** | 固定 150 在夜间噪音明显，但调到 100 又吹不动 |
| **状态不透明** | OLED 只显示 `Fan:ON`，树莓派也只知道「开着」，**不知道转多快** |

于是引入一条新协议 + 三处状态同步：

```text
树莓派 ──MQTT("pi/to/esp32", "FAN_PWM:200")──▶ ESP32 ──UART(9600)──▶ STM32
                                                                      │
                                        analogWrite(PWMA, 200) ◀──────┘
                                                                      │
树莓派 ◀──MQTT("home/fan/status", "T:..,H:..,F:1,Spd:200,V:0,S:1")── ESP32
```

---

## 🛠️ 硬件与引脚

| 模块 | 引脚 | 说明 |
| --- | --- | --- |
| TB6612 `PWMA` | `PA5` | **本次调速的关键**：PWM 输入，占空比决定转速 |
| TB6612 `AIN1` / `AIN2` | `PB12` / `PB13` | 方向控制：`HIGH/LOW` = 正转 |
| TB6612 `STBY` | `3V3` | 必须拉高，否则电机不转 |
| DHT11 | `PA0` | 温湿度（自动温控用） |
| 按键 | `PA2` | 本地手动开关（`INPUT_PULLUP`，按下为 LOW） |
| 舵机（阀门） | `PB0` | 90° = 开阀，0° = 关阀 |
| 红外 TCRT5000 | `PB1` | 安全互锁，见 [docs/05](05_EDGE_SAFETY_INTERLOCK.md) |
| ESP32 侧 | `D16`/`D17` ↔ `PA9`/`PA10` | UART 9600 8N1，**必须共地** |

电源注意：TB6612 的 `VM` 接独立电池（本次实测 4×AA / 7.4V 均可），**不要用 STM32 的 3.3V 带电机**。

---

## 💻 第一步：定义 `FAN_PWM:XXX` 协议

在原有指令解析之后追加一段 `startsWith` 分支（`:149-163`）：

```cpp
// 2. 【新增】解析 FAN_PWM:XXX 指令，实现无级调速
if (cmd.startsWith("FAN_PWM:") && !isSafetyAlert) {
  int pwmValue = cmd.substring(8).toInt();      // "FAN_PWM:" 正好 8 个字符

  if (pwmValue < 0)   pwmValue = 0;             // 夹紧到合法区间
  if (pwmValue > 255) pwmValue = 255;

  currentFanSpeed = pwmValue;
  isFanOn = (pwmValue > 0);                     // >0 即视为开启
  analogWrite(PWMA, currentFanSpeed);

  Serial.print("PWM Updated: "); Serial.println(currentFanSpeed);
}
```

| 写法 | 说明 |
| --- | --- |
| `cmd.substring(8)` | `"FAN_PWM:"` 长度为 8（`F A N _ P W M :`），从下标 8 开始就是数字 |
| `.toInt()` | 自带到数字为止的解析：`"200abc"` → `200`；**全非法输入返回 0**（见 [docs/08](08_BUG_REPORT.md) 第 4 条） |
| `pwmValue > 255` 夹紧 | `analogWrite` 在 STM32duino 上按 0~255 映射占空比，超范围会被截断，显式夹紧更清晰 |
| `!isSafetyAlert` | **安全锁**：红外报警期间任何「开启类」指令一律忽略；`FAN_OFF`/`VALVE_CLOSE` 不受限制 |

---

## 💻 第二步：把「速度」提升为一等状态

新增全局变量 `currentFanSpeed`（`:27`），并让**所有**改转速的路径都同步它：

```cpp
int currentFanSpeed = 0;      // 当前风扇速度：OLED 显示 + 上报 + 状态恢复都靠它
```

| 触发路径 | 行号 | 同步动作 |
| --- | --- | --- |
| 上电初始化 | `:47` | `analogWrite(PWMA, 0)` + `currentFanSpeed = 0` |
| 本地按键 | `:88-91` | `currentFanSpeed = isFanOn ? 150 : 0`（按键默认 150） |
| 自动温控（>28℃） | `:109-113` | `currentFanSpeed = 150`（自动模式固定 150） |
| 自动温控（≤28℃） | `:116-120` | `currentFanSpeed = 0` |
| 红外急停 | `:73-75` | `currentFanSpeed = 0` + `analogWrite(PWMA, 0)` + 关阀 |
| 远程 `FAN_ON` | `:135-139` | `currentFanSpeed = 150` |
| 远程 `FAN_OFF` | `:140-144` | `currentFanSpeed = 0` |
| **远程 `FAN_PWM:XXX`** | `:149-163` | `currentFanSpeed = pwmValue` |

> ⚠️ 注意「按键 / 自动温控 / `FAN_ON` 都会把速度重置为 150」。这意味着**手动调速与自动温控会互相抢夺控制权**——这是当前固件的一个真实设计缺陷，详见 [docs/08](08_BUG_REPORT.md) 第 1 条。

---

## 💻 第三步：OLED 显示真实转速

原来第二行只显示 `Fan:ON`，现在补上速度（`:174-177`）：

```cpp
u8g2.setCursor(0, 30);
u8g2.print("Fan:"); u8g2.print(isFanOn ? "ON " : "OFF");
u8g2.print("Spd:"); u8g2.print(currentFanSpeed);
```

屏幕四行布局：

```text
Temp: 30.3 C          ← y=15
Fan:ON Spd:200        ← y=30   （新增速度）
Valve:CLOSE           ← y=45
Status: Normal        ← y=60   （报警时变成 !! SAFETY ALERT !!）
```

`Spd:0` 且 `Fan:OFF` = 完全停转；`Fan:ON Spd:255` = 全速。

---

## 💻 第四步：上报格式新增 `Spd` 字段

`:192-201`：

```cpp
if (currentMillis - lastSerialSendTime >= serialInterval) {   // 每 2 秒
  lastSerialSendTime = currentMillis;
  Serial.print("T:");   Serial.print(currentTemp);
  Serial.print(",H:");  Serial.print(currentHumi);
  Serial.print(",F:");  Serial.print(isFanOn ? 1 : 0);
  Serial.print(",Spd:"); Serial.print(currentFanSpeed);        // ← 新增
  Serial.print(",V:");  Serial.print(isValveOpen ? 1 : 0);
  Serial.print(",S:");  Serial.println(isSafetyAlert ? 0 : 1);
}
```

上报格式变化：

| 版本 | 格式 |
| --- | --- |
| 旧 | `T:25.00,H:60.00,F:1,V:0,S:1` |
| **新** | `T:30.30,H:46.00,F:1,`**`Spd:200`**`,V:0,S:1` |

> ⚠️ **字段插入在中间（F 与 V 之间），不是追加在末尾**。树莓派侧若用**固定下标**解析（`split(',')[3]` 当 V），会把 `Spd` 当成阀门状态。请改成**按键名解析**（`key:value` 拆开存字典），或统一升级为 JSON。见 [docs/08](08_BUG_REPORT.md) 第 6 条。

---

## ✅ 验证步骤

**1. 烧录与自检**

```text
STM32 System Booted with PWM Support...     ← 上电打印
```

OLED 显示 `Temp: .. C` / `Fan:OFF Spd:0` / `Valve:CLOSE` / `Status: Normal`。

**2. 基础指令回归（不破坏旧功能）**

```bash
mosquitto_pub -h localhost -t "pi/to/esp32" -m "FAN_ON"      # OLED: Fan:ON Spd:150
mosquitto_pub -h localhost -t "pi/to/esp32" -m "FAN_OFF"     # OLED: Fan:OFF Spd:0
mosquitto_pub -h localhost -t "pi/to/esp32" -m "VALVE_OPEN"  # OLED: Valve:OPEN
mosquitto_pub -h localhost -t "pi/to/esp32" -m "VALVE_CLOSE" # OLED: Valve:CLOSE
```

**3. 无级调速（本次核心）**

```bash
mosquitto_pub -h localhost -t "pi/to/esp32" -m "FAN_PWM:80"    # 低速、安静
mosquitto_pub -h localhost -t "pi/to/esp32" -m "FAN_PWM:150"   # 中速
mosquitto_pub -h localhost -t "pi/to/esp32" -m "FAN_PWM:255"   # 全速
mosquitto_pub -h localhost -t "pi/to/esp32" -m "FAN_PWM:0"     # 停转
```

STM32 串口应打印 `PWM Updated: 80`，OLED 的 `Spd:` 同步变化，且树莓派侧能收到 `Spd:` 字段：

```bash
mosquitto_sub -h localhost -t "home/fan/status" -v
# home/fan/status T:30.30,H:46.00,F:1,Spd:200,V:0,S:1
```

**4. 安全互锁仍然有效（必测）**

```bash
mosquitto_pub -h localhost -t "pi/to/esp32" -m "FAN_PWM:255"   # 先全速
# 用手挡住红外传感器 →
```

预期：立刻 `EMERGENCY_STOP: Object detected!`，风扇停转、阀门关闭、OLED 显示 `!! SAFETY ALERT !!`；

```bash
mosquitto_pub -h localhost -t "pi/to/esp32" -m "FAN_PWM:255"   # 警报期间下发 → 被忽略
mosquitto_pub -h localhost -t "pi/to/esp32" -m "FAN_OFF"       # 关闭类指令 → 仍执行
```

---

## 🚧 踩坑与排错

### 1. 树莓派显示「已设为 200」，可风扇 2 秒后又变了

**原因**：DHT11 自动温控每 2 秒跑一次（`:99-125`），只要 `!isSafetyAlert` 就按温度把速度**强制改写**成 150 或 0 —— 手动设置的 `FAN_PWM` 会被覆盖。
**结论**：这不是通信问题，是**控制权冲突**。临时验证请先把温度控制在 28℃ 以下并把自动逻辑注释掉；正式修复方案（引入 `AUTO` / `MANUAL` 模式）见 [docs/08](08_BUG_REPORT.md) 第 1 条。

### 2. 调 `FAN_PWM:255` 也没觉得更快

**排查顺序**：

1. 万用表量 `PA5` 对地电压：PWM 200/255 时应约 `3.3V × 占空比`（255 → ≈3.3V，128 → ≈1.65V）；
2. 检查 TB6612 的 `STBY` 是否接 3.3V —— 悬空时输出全关，风扇根本不转；
3. 检查 `VM`（电机电源）是否与 STM32 **共地**；
4. 确认电机能承受 255 的电流，USB 供电带不动电机会导致复位（表现为「一调高就重启」）。

### 3. 舵机接上之后风扇调速变得不稳定

**原因（待实测确认）**：STM32duino 的 `Servo` 库会占用一个定时器产生 50Hz 舵机信号，而 `analogWrite(PA5)` 在 F103 上走 **TIM2_CH1**。若两者落在同一定时器，会出现**PWM 频率/占空比互相干扰**（风扇抖动、舵机抽动）。
**建议**：把舵机换到另一个定时器的通道（如 `PA0`→TIM2_CH1、`PA1`→TIM2_CH2、`PB6`→TIM4_CH1 需自行确认资源表），或改用软件 PWM 舵机库；上电后先用示波器/万用表确认两路互不影响。

### 4. 发 `FAN_PWM:abc` 风扇直接停了

**原因**：`String::toInt()` 对非法输入返回 `0`（`:151`），于是「调速」变成了「关风扇」。
**临时规避**：树莓派侧保证只发数字。**根治**：解析前先校验字符串全为数字。见 [docs/08](08_BUG_REPORT.md) 第 4 条。

### 5. 报警期间发 `FAN_PWM` 没反应

**原因**：`&& !isSafetyAlert` 是**故意**加的安全锁（`:149`），报警期间所有「开启类」指令都被忽略，只有 `FAN_OFF` / `VALVE_CLOSE` 等关闭类指令可执行。这是设计特性，不是 bug。

### 6. 上报解析错位，阀门状态忽开忽关

**原因**：新增的 `Spd:` 字段插在 `F` 与 `V` 之间，按下标解析会错位。
**解决**：树莓派侧改按键名解析（推荐）：

```python
def parse(payload: str) -> dict:
    return {k: v for k, v in (kv.split(':', 1) for kv in payload.split(',') if ':' in kv)}
# {'T': '30.30', 'H': '46.00', 'F': '1', 'Spd': '200', 'V': '0', 'S': '1'}
```

---

## 🔎 原理小结：为什么 PWM 能调速

TB6612 内部的 H 桥按 `PWMA` 引脚上的**方波占空比**决定平均输出电压：

```text
占空比 100% (255)  ──────────────  平均 ≈ 3.3V  →  全速
占空比  59% (150)  ────┐  ┌────    平均 ≈ 1.9V  →  中速（默认）
占空比  31% ( 80)  ─┐ ┌─┐ ┌───    平均 ≈ 1.0V  →  低速（安静）
占空比   0% (  0)  ──────────────  平均 ≈ 0V    →  停转
```

- **默认 150 的来历**：实测在 7.4V 电池下，150 已能让 12V 小风扇稳定起转又不吵；
- `AIN1=HIGH, AIN2=LOW` 固定正转，方向不变，**只调速度**；
- 数值 0~255 是 Arduino `analogWrite` 的分辨率（8 位），STM32duino 会映射到定时器比较寄存器。

**一句话总结：开关量控制的是「转不转」，PWM 控制的是「多大力气转」；一旦引入调速，「当前速度」就必须变成一个贯穿驱动、显示、上报的状态量。**

---

## 🚀 继续往下

- [ ] 引入 `ControlMode { AUTO, MANUAL }`，解决自动温控与手动调速抢控制权
- [ ] 用 PID 按温差动态调 PWM，而不是 150 一档到底
- [ ] 上报改为 JSON：`{"t":30.3,"h":46,"fan":1,"spd":200,"valve":0,"safe":1}`
- [ ] 增加 `FAN_RAMP` 指令，按斜率平滑升速（避免启动冲击电流）
- [ ] 通信超时保护：5 秒收不到 ESP32 指令就回落到安全状态
- [ ] OLED 加一个简单的转速柱状图

## 🔗 参考链接

- [TB6612FNG 数据手册](https://www.sparkfun.com/datasheets/Robotics/TB6612FNG.pdf)
- [STM32duino analogWrite 文档](https://github.com/stm32duino/Arduino_Core_STM32/wiki/API#analogwrite)
- [docs/05 边缘安全互锁（红外急停）](05_EDGE_SAFETY_INTERLOCK.md)
- 本篇缺陷清单：[docs/08 代码审计报告](08_BUG_REPORT.md)
- 配套仓库：[esp32（无线网关 · W25Q64 断电缓存）](https://github.com/Arduino-STM32-Dev/esp32)
