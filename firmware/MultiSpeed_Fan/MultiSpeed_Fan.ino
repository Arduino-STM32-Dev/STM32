// 多档位风扇：按一下按键，档位循环切换
//   0 档（停） -> 1 档（低速） -> 2 档（中速） -> 3 档（高速） -> 回到 0 档
//
// 接线：
//   PB12 -> AIN1        PB13 -> AIN2        PA5 -> PWMA
//   PA2  -> 按键（另一端接 GND，启用内部上拉）
//   TB6612 的 STBY 接 3.3V，VM 接外部 5V 电源，外部电源 GND 与 STM32 GND 共地

#define AIN1 PB12
#define AIN2 PB13
#define PWMA PA5
#define BUTTON_PIN PA2

int fanLevel = 0; // 0~3 档
int lastButtonState = HIGH;

void setup() {
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // 启用内部上拉
  setFanSpeed(0);                    // 上电先停住
}

void loop() {
  // 只在「按下的那一瞬间」切换档位，按住不放不会连续跳档
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
  digitalWrite(AIN1, HIGH); // AIN1=HIGH / AIN2=LOW -> 正转
  digitalWrite(AIN2, LOW);
  switch (level) {
    case 0: analogWrite(PWMA, 0);   break; // 停
    case 1: analogWrite(PWMA, 90);  break; // 低速
    case 2: analogWrite(PWMA, 180); break; // 中速
    case 3: analogWrite(PWMA, 255); break; // 高速
  }
}
