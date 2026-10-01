#define BUZZER_PIN PA5
#define BUTTON_PIN PA1

bool buzzerState = false; // 记录蜂鸣器状态（初始关闭）
int lastButtonState = HIGH;

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(BUZZER_PIN, LOW);
}

void loop() {
  int currentButtonState = digitalRead(BUTTON_PIN);
  
  // 检测“按下的那一瞬间”
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    buzzerState = !buzzerState; // 翻转开关状态
    
    if (buzzerState) {
      digitalWrite(BUZZER_PIN, HIGH); // 开启
    } else {
      digitalWrite(BUZZER_PIN, LOW);  // 关闭
    }
    delay(50); // 消抖
  }
  
  lastButtonState = currentButtonState;
}