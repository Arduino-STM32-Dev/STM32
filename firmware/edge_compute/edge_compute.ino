#include <DHT.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Servo.h>

// --- 引脚定义 ---
#define DHTPIN PA0
#define DHTTYPE DHT11
#define AIN1 PB12
#define AIN2 PB13
#define PWMA PA5
#define BUTTON_PIN PA2
#define SERVO_PIN PB0
#define IR_PIN PB1         // 红外传感器引脚

DHT dht(DHTPIN, DHTTYPE);
Servo valveServo; // 舵机对象
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// --- 状态变量 ---
bool isFanOn = false;
bool isValveOpen = false;
bool isSafetyAlert = false; // 红外安全警报状态
int lastButtonState = HIGH;

// --- 时间戳变量 (非阻塞核心) ---
unsigned long lastDhtReadTime = 0;
unsigned long lastSerialSendTime = 0;
unsigned long lastOledUpdateTime = 0;
const long dhtInterval = 2000;    
const long serialInterval = 2000; 
const long oledInterval = 500; 
float currentTemp = 0.0;
float currentHumi = 0.0;

void setup() {
  // 1. 引脚初始化
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(IR_PIN, INPUT);     // 红外传感器输入

  // 2. 初始状态：全关
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  analogWrite(PWMA, 0); 

  // 3. 串口初始化（与ESP32通信，设置10ms超时防阻塞）
  Serial.begin(9600);
  Serial.setTimeout(10); 

  // 4. 传感器与显示初始化
  dht.begin();
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB08_tr); 
  
  // 5. 舵机初始化
  valveServo.attach(SERVO_PIN);
  valveServo.write(0);
  
  // 6. 开机自检
  Serial.println("STM32 System Booted with Edge Safety...");
}

void loop() {
  unsigned long currentMillis = millis();

  // ==========================================
  // 任务1：【边缘计算最高优先级】红外安全逻辑
  // ==========================================
  int irState = digitalRead(IR_PIN);
  // 假设有遮挡时输出低电平（请根据实际传感器现象调整逻辑）
  if (irState == LOW && !isSafetyAlert) {
    isSafetyAlert = true;
    // 立即执行本地安全动作：强制关闭一切
    isFanOn = false;
    isValveOpen = false;
    analogWrite(PWMA, 0);
    valveServo.write(0);
    Serial.println("EMERGENCY_STOP: Object detected!");
  } 
  else if (irState == HIGH && isSafetyAlert) {
    isSafetyAlert = false; // 障碍物移除，解除警报
    Serial.println("Safety Clear.");
  }

  // ==========================================
  // 任务2：本地物理按键检测（加上安全锁）
  // ==========================================
  int currentButtonState = digitalRead(BUTTON_PIN);
  if (currentButtonState == LOW && lastButtonState == HIGH && !isSafetyAlert) { 
    isFanOn = !isFanOn;
    analogWrite(PWMA, isFanOn ? 150 : 0); 
    delay(50); // 按键消抖
  }
  lastButtonState = currentButtonState;

  // ==========================================
  // 任务3：读取DHT11与自动温控（加上安全锁）
  // ==========================================
  if (currentMillis - lastDhtReadTime >= dhtInterval) {
    lastDhtReadTime = currentMillis;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      currentTemp = t;
      currentHumi = h;
      
      if (!isSafetyAlert) { // 只有安全状态下才执行温控逻辑
        if (currentTemp > 28.0) {
          if (!isFanOn) { isFanOn = true; analogWrite(PWMA, 150); }
          if (!isValveOpen) { isValveOpen = true; valveServo.write(90); }
        } else {
          if (isFanOn) { isFanOn = false; analogWrite(PWMA, 0); }
          if (isValveOpen) { isValveOpen = false; valveServo.write(0); }
        }
      }
    }
  }

  // ==========================================
  // 任务4：接收来自ESP32的远程指令（安全优先）
  // ==========================================
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n'); 
    cmd.trim(); 
    
    if (cmd == "FAN_ON" && !isSafetyAlert) { isFanOn = true; analogWrite(PWMA, 150); }
    if (cmd == "FAN_OFF") { isFanOn = false; analogWrite(PWMA, 0); }
    if (cmd == "VALVE_OPEN" && !isSafetyAlert) { isValveOpen = true; valveServo.write(90); }
    if (cmd == "VALVE_CLOSE") { isValveOpen = false; valveServo.write(0); }
  }

  // ==========================================
  // 任务5：OLED 显示更新（加上安全警告界面）
  // ==========================================
  if (currentMillis - lastOledUpdateTime >= oledInterval) {
    lastOledUpdateTime = currentMillis;
    u8g2.clearBuffer();         
    
    // 第一行：温度
    u8g2.setCursor(0, 15);      
    u8g2.print("Temp: "); u8g2.print(currentTemp); u8g2.print(" C");
    
    // 第二行：风扇和阀门状态
    u8g2.setCursor(0, 30);
    u8g2.print("Fan:"); u8g2.print(isFanOn ? "ON " : "OFF");
    u8g2.print(" Valve:"); u8g2.print(isValveOpen ? "OPEN" : "CLOSE");
    
    // 第三行：安全状态（正常或警报）
    u8g2.setCursor(0, 45);
    if (isSafetyAlert) {
      u8g2.print("!! SAFETY ALERT !!");
    } else {
      u8g2.print("Status: Normal");
    }
    u8g2.sendBuffer();          
  }

  // ==========================================
  // 任务6：串口向ESP32上报数据（加入了 S 状态）
  // ==========================================
  if (currentMillis - lastSerialSendTime >= serialInterval) {
    lastSerialSendTime = currentMillis;
    Serial.print("T:"); Serial.print(currentTemp);
    Serial.print(",H:"); Serial.print(currentHumi);
    Serial.print(",F:"); Serial.print(isFanOn ? 1 : 0);
    Serial.print(",V:"); Serial.print(isValveOpen ? 1 : 0);
    Serial.print(",S:"); Serial.println(isSafetyAlert ? 0 : 1); // S:0为警报，S:1为安全
  }
}
