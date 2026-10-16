# STM32 代码审计报告 · Bug Finder

> **审计对象**：`firmware/PWM-FAN/PWM-FAN.ino`（202 行，红外互锁 + DHT11 温控 + 舵机阀门 + `FAN_PWM` 无级调速）
> **参照对象**：`firmware/edge_compute/edge_compute.ino`（上一版，172 行）、`firmware/servo-control/servo-control.ino`
> **审计方式**：逐行人工审查 + 静态规则扫描（文末附自查命令）
> **审计结论**：**新增的调速功能本身正确，但与既有自动温控存在控制权冲突（P0-1），另有 1 个需实测确认的定时器冲突风险。** 建议先修 P0-1，否则「远程调速」在实际使用中 2 秒后即失效。

---

## 📋 缺陷总表

| 编号 | 等级 | 位置 | 一句话描述 |
| --- | --- | --- | --- |
| **P0-1** | 🔴 致命 | `PWM-FAN.ino:107-123` vs `:149-163` | DHT11 自动温控每 2 秒**覆盖**远程 `FAN_PWM` 设置，手动调速实际上不可用 |
| **P1-1** | 🟠 高危（待实测） | `:56-57` `:74` `:91` `:160` | `Servo` 库与 `analogWrite(PA5)` 可能共用定时器 → PWM/舵机互相干扰 |
| **P1-2** | 🟠 高危 | `:130-164` | 无通信超时保护：ESP32 掉线后 STM32 **永远保持最后状态**（可能全速运转） |
| **P1-3** | 🟠 高危 | `:198` | 上报新增 `Spd:` **插在字段中间**，破坏树莓派既有按位解析 |
| **P1-4** | 🟠 高危 | `:69-77` | 急停只把 PWM 归零，**未拉低 TB6612 的 `STBY`**，未做到硬件级断电 |
| **P2-1** | 🟡 中 | `:151` | `toInt()` 对非法输入返回 0 → `FAN_PWM:abc` 变成「关风扇」 |
| **P2-2** | 🟡 中 | `:130-131` | `readStringUntil` + `setTimeout(10)` 会把**半条指令**当完整指令执行（`FAN_PWM:20` + 迟到的 `0`） |
| **P2-3** | 🟡 中 | `:68-81` | 红外输入无去抖/确认时间窗，抖动时反复打印急停与恢复 |
| **P2-4** | 🟡 中 | `:78-81` | 「Safety Clear.」后**不恢复任何状态、不主动上报**，现场无法判断系统已就绪 |
| **P3-1** | 🔵 低 | `:138` | `FAN_ON` 分支硬编码 `analogWrite(PWMA, 150)`，未使用 `currentFanSpeed` |
| **P3-2** | 🔵 低 | `:92` | 按键处理里 `delay(50)` 阻塞主循环 |
| **P3-3** | 🔵 低 | `:103-105` | DHT11 读取失败时静默沿用旧值，无失败计数/告警 |
| **P3-4** | 🔵 低 | `:47` `:91` `:160` | 未设置 `analogWriteFrequency()`，默认约 1kHz，电机会有可听闻的啸叫 |
| **P3-5** | 🔵 低 | `:192-201` | 上报无序号/时间戳，QoS0 丢包无法察觉（与 ESP32 侧缓存配合时才可追责） |

---

## 🔴 P0-1：自动温控覆盖手动调速（致命，功能层）

**位置**：自动温控 `:107-123`；远程调速 `:149-163`

```cpp
// 任务3：DHT11读取与自动温控（每 2 秒执行一次）
if (!isSafetyAlert) {
  if (currentTemp > 28.0) {
    if (!isFanOn) { isFanOn = true; currentFanSpeed = 150; analogWrite(PWMA, 150); }  // ← 强制 150
    ...
  } else {
    if (isFanOn)  { isFanOn = false; currentFanSpeed = 0; analogWrite(PWMA, 0); }     // ← 强制 0
    ...
  }
}
```

```cpp
// 任务4：远程调速
if (cmd.startsWith("FAN_PWM:") && !isSafetyAlert) {
  currentFanSpeed = pwmValue;
  isFanOn = (pwmValue > 0);
  analogWrite(PWMA, currentFanSpeed);        // ← 设置完，最多 2 秒后被上面覆盖
}
```

**症状**：

- 树莓派下发 `FAN_PWM:200`，OLED 短暂显示 `Spd:200`，**一两秒后自己变回 150（温度 >28℃）或 0（温度 ≤28℃）**；
- 上位机 UI 上表现为「设置成功 → 状态回跳」，被误判成通信问题；
- 用户设的 255 全速散热，会因为温度恰好 ≤28℃ 而被**直接关停**，散热需求被忽略。

**根因**：固件是「**最后写入者获胜**」的单一状态机，自动温控与远程指令共享 `isFanOn` / `currentFanSpeed` 两个变量，没有任何「谁在控制」的概念。自动任务每 2 秒无条件执行一次，等于**每 2 秒抢一次控制权**。

**复现**：

```bash
mosquitto_pub -h localhost -t "pi/to/esp32" -m "FAN_PWM:255"
# 观察 OLED：Spd:255 → （≤2s 后）Spd:150 或 Fan:OFF Spd:0
```

**修复**：引入显式控制模式，自动温控只在 `AUTO` 模式下动风扇。

```cpp
enum ControlMode { MODE_AUTO, MODE_MANUAL };
ControlMode controlMode = MODE_AUTO;
unsigned long lastRemoteCmdTime = 0;

// 任务3：温控只在 AUTO 模式下接管
if (currentMillis - lastDhtReadTime >= dhtInterval) {
  lastDhtReadTime = currentMillis;
  float t = dht.readTemperature(), h = dht.readHumidity();
  if (!isnan(t) && !isnan(h)) {
    currentTemp = t; currentHumi = h;
    if (!isSafetyAlert && controlMode == MODE_AUTO) {   // ← 唯一的改动
      /* 原有自动逻辑保持不变 */
    }
  }
}

// 任务4：收到任何远程调速指令 → 切到 MANUAL
if (cmd.startsWith("FAN_PWM:") && !isSafetyAlert) {
  ...
  controlMode = MODE_MANUAL;
  lastRemoteCmdTime = currentMillis;
}

// 可选：手动模式静默 10 分钟无指令 → 回落自动
if (controlMode == MODE_MANUAL && currentMillis - lastRemoteCmdTime > 600000UL) {
  controlMode = MODE_AUTO;
  Serial.println("Manual timeout, back to AUTO.");
}
```

**验证**：下发 `FAN_PWM:200` 后持续观察 30 秒，OLED 的 `Spd:200` **不得**跳变；OLED 可另加一行显示 `Mode:MAN`/`Mode:AUTO` 便于现场判断。

---

## 🟠 P1-1：`Servo` 与 `analogWrite(PA5)` 的定时器冲突风险（需实测确认）

**位置**：`valveServo.attach(SERVO_PIN);`（`:56`）、`valveServo.write(0/90)`（`:57` `:75` `:114` `:121` `:145` `:146`）、`analogWrite(PWMA, ...)`（`:47` `:74` `:91` `:112` `:119` `:138` `:143` `:160`）

**风险**：STM32duino 的 `Servo` 库使用**硬件定时器**产生 50Hz 舵机脉冲；而在 STM32F103 上 `analogWrite(PA5)` 走 **TIM2_CH1**。若舵机库恰好分配到同一个定时器，两者会争用比较寄存器：典型表现是 **调 PWM 时舵机抽动，或动舵机时风扇转速跳变**。

**确认方法**（5 分钟）：

1. 单独跑：`analogWrite(PA5, 128)` 固定不动，读 `PA5` 电压应稳定 ≈1.65V（万用表/示波器）；
2. 再 `valveServo.write(90)`，读 `PA5` 电压：若明显偏移或跳动，即存在定时器冲突；
3. 也可打印 `TIM2->PSC` / `TIM2->ARR` 在 `attach()` 前后是否被改写。

**缓解方案**：

- 把舵机换到不冲突的通道（如 `PB6`/`PB7` 的 TIM4 通道，需按具体 core 版本核对定时器分配表）；
- 或改用 `Servo` 之外的软件 PWM 实现（精度略低，但完全不占定时器）。

**注意**：本条为**待验证风险**，不是已确认缺陷。请先按上述步骤实测，再决定是否改硬件。

---

## 🟠 P1-2：没有通信超时保护，断线后保持「最后状态」

**位置**：`if (Serial.available() > 0)`（`:130`）——**只在有数据时**才处理指令，没有任何「多久没收到指令」的判断；全固件无 `watchdog`。

**症状**：ESP32 断电/掉网/**MQTT Broker 挂掉**时，STM32 完全无感知：

- 若断线前是 `FAN_PWM:255`，风扇会**一直全速转下去**；
- 阀门保持在开位，温控继续按本地规则跑；
- 上位机界面显示「离线」，但现场设备仍在动作。

**修复**：加通信看门狗，超时回落安全状态。

```cpp
unsigned long lastCmdTime = 0;
const unsigned long CMD_TIMEOUT = 5000;      // 5 秒无指令

// 收到指令时：lastCmdTime = currentMillis;

if (currentMillis - lastCmdTime > CMD_TIMEOUT && !isSafetyAlert) {
  if (isFanOn || isValveOpen) {
    Serial.println("⚠️ Link timeout, falling back to SAFE state.");
    isFanOn = false; currentFanSpeed = 0; analogWrite(PWMA, 0);
    isValveOpen = false; valveServo.write(0);
    controlMode = MODE_AUTO;                 // 交给本地温控
  }
  lastCmdTime = currentMillis;               // 防止反复打印
}
```

**验证**：发 `FAN_PWM:255` 后拔掉 ESP32 电源，5 秒内串口应打印超时告警并停转。

---

## 🟠 P1-3：上报字段插在中间，破坏既有解析

**位置**：`:192-201`

```cpp
Serial.print(",F:");   Serial.print(isFanOn ? 1 : 0);
Serial.print(",Spd:"); Serial.print(currentFanSpeed);   // ← 插在 F 与 V 之间
Serial.print(",V:");   Serial.print(isValveOpen ? 1 : 0);
```

| 版本 | 载荷 | `split(',')[3]` 的含义 |
| --- | --- | --- |
| 旧 | `T:..,H:..,F:1,V:0,S:1` | `V:0`（阀门） |
| **新** | `T:..,H:..,F:1,Spd:200,V:0,S:1` | **`Spd:200`（速度）** ← 语义漂移 |

**症状**：树莓派/Node-RED 面板上「阀门状态」在 0/200 之间乱跳，或直接解析异常。

**修复（二选一，推荐 A）**：

**A. 上位机改为按键名解析**（向前兼容所有未来字段）：

```python
def parse(payload: str) -> dict:
    return {k: v for k, v in (kv.split(':', 1) for kv in payload.split(',') if ':' in kv)}
```

**B. 固件升级为 JSON**（同时解决 P3-5）：

```cpp
Serial.printf("{\"t\":%.1f,\"h\":%.1f,\"fan\":%d,\"spd\":%d,\"valve\":%d,\"safe\":%d}\n",
              currentTemp, currentHumi, isFanOn ? 1 : 0, currentFanSpeed,
              isValveOpen ? 1 : 0, isSafetyAlert ? 0 : 1);
```

> ⚠️ 无论选哪种，**ESP32 侧的 `std::queue` / W25Q64 缓存只是转发字符串**，不受格式影响；受影响的是树莓派解析层。

---

## 🟠 P1-4：急停没有做到硬件级断电

**位置**：`if (irState == LOW && !isSafetyAlert)`（`:69-77`）

```cpp
isFanOn = false;
isValveOpen = false;
currentFanSpeed = 0;
analogWrite(PWMA, 0);
valveServo.write(0);
```

**问题**：`analogWrite(PWMA, 0)` 只是让 H 桥的占空比为 0。TB6612 在「PWM=0 且 `AIN1`/`AIN2` 保持 `HIGH`/`LOW`」时仍处于**使能状态**（部分接线方式下是短路刹车或持续导通），**并没有物理切断电机驱动**。工业场景下的急停应当**同时拉低 `STBY`**，让驱动芯片彻底失能。

**修复**：

```cpp
#define STBY_PIN PB14        // 按实际接线调整；接线为 3V3 时需改接到 GPIO

pinMode(STBY_PIN, OUTPUT);
digitalWrite(STBY_PIN, HIGH);                 // 正常运行使能

// 急停分支内追加：
digitalWrite(STBY_PIN, LOW);                  // 物理级失能
digitalWrite(AIN1, LOW); digitalWrite(AIN2, LOW);

// Safety Clear 时恢复：
digitalWrite(STBY_PIN, HIGH);
```

**若 `STBY` 目前直接接 3V3**（无 GPIO 控制），至少要意识到：**当前急停是「软停」**，请勿把它当作安全回路（functional safety）认证手段，真实急停必须切断电机电源。

---

## 🟡 中优先级项

| 编号 | 位置 | 问题 | 修复 |
| --- | --- | --- | --- |
| **P2-1** | `:151` | `cmd.substring(8).toInt()`：`"FAN_PWM:abc"` → `0` → 风扇被关；`"FAN_PWM:-5"` → 夹到 0（可接受）；`"FAN_PWM:1e9"` → 夹到 255 | 解析前校验全数字：`bool ok = s.length() && all_of(s.begin(), s.end(), ::isdigit);` 校验失败直接 `return`（保留原速度） |
| **P2-2** | `:130-131` | `readStringUntil('\n')` 配合 `setTimeout(10)`：数据分片到达时（如 `FAN_PWM:20` 到这儿超时、`0` 晚 10ms 到）会把半条指令当完整指令执行 → 速度变 20 而非 200 | 改用固定接收缓冲 + 行完整性校验（必须以 `\n` 结尾且长度 ≤ 32），或把超时提到 50ms 并校验 `cmd` 是否符合白名单格式 |
| **P2-3** | `:68-81` | 红外无消抖。TCRT5000 在临界距离/反光地面会产生毫秒级抖动，导致 `EMERGENCY_STOP` 与 `Safety Clear.` 交替刷屏，风扇反复启停 | 加 30~50ms 确认窗：连续 N 次采样一致才切换 `isSafetyAlert`；并加最小保持时间（如 1s）防抖 |
| **P2-4** | `:78-81` | `Safety Clear.` 只打印一句串口日志：不恢复任何状态、不主动向树莓派推送「已恢复」，也不清零报警计数 | 清除时主动上报一条状态（或让 2 秒周期的 `S:` 字段承担此职），并在 OLED 上给出明确提示；建议同时记录报警次数用于统计 |

---

## 🔵 低优先级项（P3）

| 编号 | 位置 | 说明与建议 |
| --- | --- | --- |
| P3-1 | `:138` | `FAN_ON` 分支直接写 `analogWrite(PWMA, 150)`，而同分支上文已设 `currentFanSpeed = 150`。建议统一为 `analogWrite(PWMA, currentFanSpeed)`，将来改默认速度只需改一处。 |
| P3-2 | `:92` | 按键处理里的 `delay(50)` 是遗留消抖写法，会阻塞红外监控与串口接收 50ms。建议改为 `millis()` 时间戳消抖。 |
| P3-3 | `:103-105` | `if (!isnan(t) && !isnan(h))` 失败时**静默跳过**。DHT11 读失败较常见（尤其长线/无上拉），建议 `dhtFailCount++` 并在连续失败 5 次后上报错误字段、OLED 显示 `DHT ERR`。 |
| P3-4 | `:47` `:91` `:160` | 未调用 `analogWriteFrequency(PWMA, 20000)` 之类设置。默认 PWM 频率落在可听范围，电机会有啸叫；MB102/TB6612 通常能接受 20kHz 以上，风扇噪音可明显降低（注意别超过驱动芯片与电机的响应能力）。 |
| P3-5 | `:192-201` | 上报无序号、无时间戳，MQTT QoS0 下丢包不可知。建议载荷加 `Seq:` 或 `Ms:`，配合 ESP32 侧的 W25Q64 缓存即可精确统计丢包率。 |

---

## 🔧 建议修复顺序

| 优先级 | 动作 | 影响面 |
| --- | --- | --- |
| 1 | 引入 `ControlMode`，自动温控让位手动（P0-1） | 让「远程调速」真正可用 |
| 2 | 加通信超时回落安全状态（P1-2） | 防断线后设备失控 |
| 3 | 树莓派改按键名解析 / 固件改 JSON（P1-3） | 恢复面板显示正确性 |
| 4 | 实测 Servo 与 PWM 定时器冲突（P1-1） | 决定是否需要改硬件接线 |
| 5 | 急停拉低 `STBY`（P1-4） | 提升到硬件级停机 |
| 6 | `toInt` 校验 + 行完整性校验（P2-1、P2-2） | 防误指令 |
| 7 | 红外消抖（P2-3） | 消除误触发刷屏 |

---

## 🧰 自查清单（提交前跑一遍）

```bash
# 1. 自动温控是否仍会无条件覆盖手动速度
grep -n "currentFanSpeed = 150" firmware/PWM-FAN/PWM-FAN.ino

# 2. 是否存在控制模式标识
grep -n "MODE_AUTO\|MODE_MANUAL" firmware/PWM-FAN/PWM-FAN.ino

# 3. 是否有通信超时保护
grep -n "lastCmdTime\|CMD_TIMEOUT" firmware/PWM-FAN/PWM-FAN.ino

# 4. 急停是否拉低 STBY
grep -n "STBY" firmware/PWM-FAN/PWM-FAN.ino

# 5. 所有 analogWrite 是否都用 currentFanSpeed
grep -n "analogWrite(PWMA" firmware/PWM-FAN/PWM-FAN.ino

# 6. 上报字段是否含 Spd（并确认上位机已按名解析）
grep -n 'Spd:' firmware/PWM-FAN/PWM-FAN.ino

# 7. 敏感信息是否已清理
#    ⚠️ 这里故意不写真实字符串，请把你自己的 SSID / 密码 / IP 关键词填进去再跑
grep -rn -e "<你的WiFi名称>" -e "<你的WiFi密码>" -e "192.168." firmware/ docs/ *.md
```

| 期望结果 | 说明 |
| --- | --- |
| 第 1 条：仅在 `controlMode == MODE_AUTO` 分支内 | 否则手动调速被覆盖 |
| 第 3 条：有 | 否则断线后设备失控 |
| 第 5 条：`analogWrite(PWMA, currentFanSpeed)` 为主 | 避免硬编码不一致 |
| 第 7 条：无输出 | 凭证不得入库 |

---

## 📎 附：关键代码位置对照

| 功能 | 行号 |
| --- | --- |
| 引脚定义（DHTPIN PA0 / AIN1 PB12 / AIN2 PB13 / PWMA PA5 / BUTTON PA2 / SERVO PB0 / IR PB1） | `:7-14` |
| 状态变量（`isFanOn` / `isValveOpen` / `isSafetyAlert` / `currentFanSpeed`） | `:21-27` |
| 定时常量（DHT 2000 / 上报 2000 / OLED 500） | `:32-34` |
| `setup()`：引脚、串口 9600、舵机归零 | `:38-60` |
| 任务1 红外急停（最高优先级） | `:68-81` |
| 任务2 本地按键（150 默认速度） | `:86-94` |
| 任务3 DHT11 自动温控（28.0℃ 阈值） | `:99-125` |
| 任务4 远程指令 + `FAN_PWM` 调速 | `:130-164` |
| 任务5 OLED 显示（含 `Spd:`） | `:169-187` |
| 任务6 上报（`T,H,F,Spd,V,S`） | `:192-201` |

> 相关教程：[docs/07 ESP32 远程无级调速（FAN_PWM）](07_PWM_SPEED_CONTROL.md) · [docs/05 边缘安全互锁](05_EDGE_SAFETY_INTERLOCK.md)
> 姊妹报告：[ESP32 代码审计报告](https://github.com/Arduino-STM32-Dev/esp32/blob/main/docs/03_BUG_REPORT.md)
