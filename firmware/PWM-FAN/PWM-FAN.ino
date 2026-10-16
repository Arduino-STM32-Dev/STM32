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
Servo valveServo;
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// --- 状态变量 ---
bool isFanOn = false;
bool isValveOpen = false;
bool isSafetyAlert = false;
int lastButtonState = HIGH;

// 记录当前的风扇速度（用于状态显示和恢复）
int currentFanSpeed = 0; 

unsigned long lastDhtReadTime = 0;
unsigned long lastSerialSendTime = 0;
unsigned long lastOledUpdateTime = 0;
const long dhtInterval = 2000;    
const long serialInterval = 2000; 
const long oledInterval = 500; 
float currentTemp = 0.0;
float currentHumi = 0.0;

void setup() {
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(IR_PIN, INPUT);

  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  analogWrite(PWMA, 0); 

  Serial.begin(9600);
  Serial.setTimeout(10); 
  
  dht.begin();
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB08_tr); 
  
  valveServo.attach(SERVO_PIN);
  valveServo.write(0);
  
  Serial.println("STM32 System Booted with PWM Support...");
}

void loop() {
  unsigned long currentMillis = millis();

  // ==========================================
  // 任务1：红外安全逻辑（最高优先级）
  // ==========================================
  int irState = digitalRead(IR_PIN);
  if (irState == LOW && !isSafetyAlert) {
    isSafetyAlert = true;
    isFanOn = false;
    isValveOpen = false;
    currentFanSpeed = 0;
    analogWrite(PWMA, 0);
    valveServo.write(0);
    Serial.println("EMERGENCY_STOP: Object detected!");
  } 
  else if (irState == HIGH && isSafetyAlert) {
    isSafetyAlert = false; 
    Serial.println("Safety Clear.");
  }

  // ==========================================
  // 任务2：本地物理按键（安全锁）
  // ==========================================
  int currentButtonState = digitalRead(BUTTON_PIN);
  if (currentButtonState == LOW && lastButtonState == HIGH && !isSafetyAlert) { 
    isFanOn = !isFanOn;
    // 按键开启时，默认以 150 速度运行
    currentFanSpeed = isFanOn ? 150 : 0;
    analogWrite(PWMA, currentFanSpeed); 
    delay(50); 
  }
  lastButtonState = currentButtonState;

  // ==========================================
  // 任务3：DHT11读取与自动温控
  // ==========================================
  if (currentMillis - lastDhtReadTime >= dhtInterval) {
    lastDhtReadTime = currentMillis;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      currentTemp = t;
      currentHumi = h;
      
      if (!isSafetyAlert) { 
        if (currentTemp > 28.0) {
          if (!isFanOn) { 
            isFanOn = true; 
            currentFanSpeed = 150; // 自动模式固定150
            analogWrite(PWMA, currentFanSpeed); 
          }
          if (!isValveOpen) { isValveOpen = true; valveServo.write(90); }
        } else {
          if (isFanOn) { 
            isFanOn = false; 
            currentFanSpeed = 0;
            analogWrite(PWMA, 0); 
          }
          if (isValveOpen) { isValveOpen = false; valveServo.write(0); }
        }
      }
    }
  }

  // ==========================================
  // 任务4：【关键更新】接收来自ESP32的远程指令
  // ==========================================
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n'); 
    cmd.trim(); 
    
    // 1. 兼容之前的固定指令
    if (cmd == "FAN_ON" && !isSafetyAlert) { 
      isFanOn = true; 
      currentFanSpeed = 150;
      analogWrite(PWMA, 150); 
    }
    if (cmd == "FAN_OFF") { 
      isFanOn = false; 
      currentFanSpeed = 0;
      analogWrite(PWMA, 0); 
    }
    if (cmd == "VALVE_OPEN" && !isSafetyAlert) { isValveOpen = true; valveServo.write(90); }
    if (cmd == "VALVE_CLOSE") { isValveOpen = false; valveServo.write(0); }
    
    // 2. 【新增】解析 FAN_PWM:XXX 指令，实现无级调速
    if (cmd.startsWith("FAN_PWM:") && !isSafetyAlert) {
      // 提取冒号后面的数字
      int pwmValue = cmd.substring(8).toInt();
      
      // 限制 PWM 值在合法的 0~255 之间
      if (pwmValue < 0) pwmValue = 0;
      if (pwmValue > 255) pwmValue = 255;
      
      // 更新状态并驱动电机
      currentFanSpeed = pwmValue;
      isFanOn = (pwmValue > 0); // 如果PWM大于0，风扇视为开启状态
      analogWrite(PWMA, currentFanSpeed);
      
      Serial.print("PWM Updated: "); Serial.println(currentFanSpeed);
    }
  }

  // ==========================================
  // 任务5：OLED 显示
  // ==========================================
  if (currentMillis - lastOledUpdateTime >= oledInterval) {
    lastOledUpdateTime = currentMillis;
    u8g2.clearBuffer();         
    u8g2.setCursor(0, 15);      
    u8g2.print("Temp: "); u8g2.print(currentTemp); u8g2.print(" C");
    u8g2.setCursor(0, 30);
    // 显示风扇具体速度，不再是简单的ON/OFF
    u8g2.print("Fan:"); u8g2.print(isFanOn ? "ON " : "OFF");
    u8g2.print("Spd:"); u8g2.print(currentFanSpeed); 
    u8g2.setCursor(0, 45);
    u8g2.print("Valve:"); u8g2.print(isValveOpen ? "OPEN" : "CLOSE");
    u8g2.setCursor(0, 60);
    if (isSafetyAlert) {
      u8g2.print("!! SAFETY ALERT !!");
    } else {
      u8g2.print("Status: Normal");
    }
    u8g2.sendBuffer();          
  }

  // ==========================================
  // 任务6：上报数据（将速度加入上传格式）
  // ==========================================
  if (currentMillis - lastSerialSendTime >= serialInterval) {
    lastSerialSendTime = currentMillis;
    Serial.print("T:"); Serial.print(currentTemp);
    Serial.print(",H:"); Serial.print(currentHumi);
    Serial.print(",F:"); Serial.print(isFanOn ? 1 : 0);
    // 新增 Spd 字段，让树莓派知道当前具体速度
    Serial.print(",Spd:"); Serial.print(currentFanSpeed); 
    Serial.print(",V:"); Serial.print(isValveOpen ? 1 : 0);
    Serial.print(",S:"); Serial.println(isSafetyAlert ? 0 : 1); 
  }
}