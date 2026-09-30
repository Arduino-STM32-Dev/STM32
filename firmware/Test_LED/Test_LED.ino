/*
 * Test_LED.ino —— STM32 入门第一天：点亮第一个 LED
 *
 * 开发板 : STM32F103C8T6 (BluePill) 最小系统板
 * 核心库 : STM32duino (STM32 MCU based boards)
 * 烧录器 : ST-Link V2 —— Upload method 选择 STM32CubeProgrammer (SWD)
 *
 * 接线 :
 *   外部 LED —— PA5 接 LED 长脚(正极)，LED 短脚(负极) 经面包板 GND 轨接 STM32 GND
 *   板载 LED —— PC13（低电平点亮，与外部 LED 的极性正好相反）
 */

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
