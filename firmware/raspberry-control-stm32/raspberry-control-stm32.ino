#include <DHT.h>

// --- 引脚定义 ---
#define DHTPIN PA0
#define DHTTYPE DHT11
#define AIN1 PB12
#define AIN2 PB13
#define PWMA PA5
#define BUTTON_PIN PA2

DHT dht(DHTPIN, DHTTYPE);

// --- 状态变量 ---
bool isFanOn = false;
int lastButtonState = HIGH;

// --- 时间戳变量 (非阻塞核心) ---
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

  // 初始化串口（向ESP32发送数据，读取ESP32发来的指令）
  Serial.begin(9600);
  Serial.setTimeout(10); // 极其关键！防止串口读取阻塞导致按键失灵
  
  // 初始化DHT11
  dht.begin();
}

void loop() {
  unsigned long currentMillis = millis();

  // ==========================================
  // 任务1：本地物理按键检测
  // ==========================================
  int currentButtonState = digitalRead(BUTTON_PIN);
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    isFanOn = !isFanOn;
    analogWrite(PWMA, isFanOn ? 150 : 0);
    delay(50); // 仅用于按键消抖
  }
  lastButtonState = currentButtonState;

  // ==========================================
  // 任务2：读取DHT11（2秒一次）
  // ==========================================
  if (currentMillis - lastDhtReadTime >= dhtInterval) {
    lastDhtReadTime = currentMillis;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      currentTemp = t;
      currentHumi = h;
    }
  }

  // ==========================================
  // 任务3：接收来自ESP32的远程指令（新增！）
  // ==========================================
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n'); 
    cmd.trim(); // 去除回车换行符和空格
    
    if (cmd == "FAN_ON") {
      isFanOn = true;
      analogWrite(PWMA, 150); // 远程开启低速
    } 
    else if (cmd == "FAN_OFF") {
      isFanOn = false;
      analogWrite(PWMA, 0);   // 远程关闭
    }
  }

  // ==========================================
  // 任务4：通过串口向ESP32发送数据（2秒一次）
  // ==========================================
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