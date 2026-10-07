# 05 · STM32 端：DHT11 + 风扇（树莓派远程控制）

> 本仓库（Arduino-STM32-Dev/STM32）的固件说明。
> 完整链路：**树莓派 → MQTT → ESP32 → UART → STM32**。ESP32 端见 esp32 仓库的 esp32-fan-gateway.md。

## 一、功能

- DHT11 采集温度 / 湿度（PA0）
- TB6612 驱动直流风扇（AIN1=PB12，AIN2=PB13，PWMA=PA5）
- 本地物理按键（PA2）切换风扇开 / 关
- 每 2 秒通过串口上报：T:温度,H:湿度,F:风扇(0/1)
- 接收串口远程指令：FAN_ON / FAN_OFF

## 二、接线

| STM32 | 连接 |
| :--- | :--- |
| PA0 | DHT11 DATA |
| PA2 | 按键（另一端接 GND） |
| PB12 / PB13 | TB6612 AIN1 / AIN2 |
| PA5 | TB6612 PWMA |
| PA9 (TX1) | ESP32 GPIO16 (RX2) |
| PA10 (RX1) | ESP32 GPIO17 (TX2) |
| GND | 与 ESP32、电源共地 |

## 三、串口协议（9600）

上报（每 2 秒一行）：

    T:26.10,H:32.00,F:0

指令（树莓派经 ESP32 下发）：

    FAN_ON
    FAN_OFF

## 四、使用

1. Arduino IDE 选择 STM32F103C8T6，用 ST-Link 烧录本文件
2. 接线完成后，串口每 2 秒输出一行温湿度与风扇状态
3. 树莓派侧（网页看板 / 语音助手）即可读取数据、下发开/关风扇指令
