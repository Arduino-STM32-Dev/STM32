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