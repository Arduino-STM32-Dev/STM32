// ============================================================
// STM32F103 + DHT11 + TB6612 风扇 —— 树莓派远程控制（STM32 端）
// 链路：树莓派 --MQTT--> ESP32 --UART--> STM32
//   · 每 2 秒串口上报：T:温度,H:湿度,F:风扇(0/1)
//   · 接收串口指令：FAN_ON / FAN_OFF
//   · 保留本地物理按键切换风扇
// 串口：Serial(PA9/PA10) @9600，接 ESP32 的 GPIO17/GPIO16
// ============================================================
#include <DHT.h>

// --- 引脚定义 ---
#define DHTPIN PA0
#define DHTTYPE DHT11
#define AIN1 PB12
#define AIN2 PB13
#define PWMA PA5
#define BUTTON_PIN PA2

DHT dht(DHTPIN, DHTTYPE);

bool isFanOn = false;
int lastButtonState = HIGH;

unsigned long lastDhtReadTime = 0;
unsigned long lastSerialSendTime = 0;
const long dhtInterval = 2000;
const long serialInterval = 2000;
float currentTemp = 0.0;
float currentHumi = 0.0;

void setup() {
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // 初始状态：停止
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  analogWrite(PWMA, 0);

  // 串口：向 ESP32 上报数据、接收指令
  Serial.begin(9600);
  Serial.setTimeout(10);  // 防止 readStringUntil 阻塞导致按键失灵

  dht.begin();
}

void loop() {
  unsigned long currentMillis = millis();

  // 任务1：本地物理按键
  int currentButtonState = digitalRead(BUTTON_PIN);
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    isFanOn = !isFanOn;
    analogWrite(PWMA, isFanOn ? 150 : 0);
    delay(50);
  }
  lastButtonState = currentButtonState;

  // 任务2：读 DHT11（2 秒一次）
  if (currentMillis - lastDhtReadTime >= dhtInterval) {
    lastDhtReadTime = currentMillis;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      currentTemp = t;
      currentHumi = h;
    }
  }

  // 任务3：接收 ESP32 下发的远程指令
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "FAN_ON") {
      isFanOn = true;
      analogWrite(PWMA, 150);
    } else if (cmd == "FAN_OFF") {
      isFanOn = false;
      analogWrite(PWMA, 0);
    }
  }

  // 任务4：向 ESP32 上报（2 秒一次）
  if (currentMillis - lastSerialSendTime >= serialInterval) {
    lastSerialSendTime = currentMillis;
    Serial.print("T:");
    Serial.print(currentTemp);
    Serial.print(",H:");
    Serial.print(currentHumi);
    Serial.print(",F:");
    Serial.println(isFanOn ? 1 : 0);
  }
}
