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