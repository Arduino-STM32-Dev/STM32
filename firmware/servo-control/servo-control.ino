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

// --- 初始化对象 ---
DHT dht(DHTPIN, DHTTYPE);
Servo valveServo; // 舵机对象
// 初始化OLED (I2C, 地址0x3C)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// --- 状态变量 ---
bool isFanOn = false;
bool isValveOpen = false;
int lastButtonState = HIGH;

// --- 时间戳 (非阻塞核心) ---
unsigned long lastDhtReadTime = 0;
unsigned long lastSerialSendTime = 0;
unsigned long lastOledUpdateTime = 0;
const long dhtInterval = 2000;    
const long serialInterval = 2000; 
const long oledInterval = 500; // 屏幕刷新不要太快，防闪烁
float currentTemp = 0.0;
float currentHumi = 0.0;

void setup() {
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  analogWrite(PWMA, 0); 

  Serial.begin(9600);
  Serial.setTimeout(10); 
  dht.begin();
  
  // 初始化OLED
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB08_tr); // 选择字体
  
  // 初始化舵机（默认关闭状态：0度）
  valveServo.attach(SERVO_PIN);
  valveServo.write(0);
  isValveOpen = false;
  
  Serial.println("Industrial System Booted...");
}

void loop() {
  unsigned long currentMillis = millis();

  // ==========================================
  // 任务1：本地物理按键检测（手动控制风扇）
  // ==========================================
  int currentButtonState = digitalRead(BUTTON_PIN);
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    isFanOn = !isFanOn;
    analogWrite(PWMA, isFanOn ? 150 : 0); 
    delay(50); 
  }
  lastButtonState = currentButtonState;

  // ==========================================
  // 任务2：读取DHT11与自动控制逻辑（2秒一次）
  // ==========================================
  if (currentMillis - lastDhtReadTime >= dhtInterval) {
    lastDhtReadTime = currentMillis;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      currentTemp = t;
      currentHumi = h;

      // 【工业自动化逻辑】温度大于28度自动开阀门和风扇
      if (currentTemp > 28.0) {
        if (!isFanOn) { isFanOn = true; analogWrite(PWMA, 150); }
        if (!isValveOpen) { isValveOpen = true; valveServo.write(90); } // 舵机转到90度
      } else {
        if (isFanOn) { isFanOn = false; analogWrite(PWMA, 0); }
        if (isValveOpen) { isValveOpen = false; valveServo.write(0); }  // 舵机回到0度
      }
    }
  }

  // ==========================================
  // 任务3：接收远程指令（扩展了阀门控制）
  // ==========================================
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n'); 
    cmd.trim(); 
    
    if (cmd == "FAN_ON")  { isFanOn = true;  analogWrite(PWMA, 150); }
    if (cmd == "FAN_OFF") { isFanOn = false; analogWrite(PWMA, 0); }
    
    // 阀门控制指令
    if (cmd == "VALVE_OPEN")  { isValveOpen = true; valveServo.write(90); }
    if (cmd == "VALVE_CLOSE") { isValveOpen = false; valveServo.write(0); }
  }

  // ==========================================
  // 任务4：更新OLED显示（500ms一次，防闪烁）
  // ==========================================
  if (currentMillis - lastOledUpdateTime >= oledInterval) {
    lastOledUpdateTime = currentMillis;
    
    u8g2.clearBuffer();         // 清除内部缓冲区
    u8g2.setCursor(0, 15);      
    u8g2.print("Temp: "); u8g2.print(currentTemp); u8g2.print(" C");
    u8g2.setCursor(0, 30);
    u8g2.print("Humi: "); u8g2.print(currentHumi); u8g2.print(" %");
    u8g2.setCursor(0, 45);
    u8g2.print("Fan: "); u8g2.print(isFanOn ? "ON" : "OFF");
    u8g2.setCursor(0, 60);
    u8g2.print("Valve: "); u8g2.print(isValveOpen ? "OPEN" : "CLOSED");
    u8g2.sendBuffer();          // 发送到屏幕显示
  }

  // ==========================================
  // 任务5：通过串口向ESP32发送数据（扩展了阀门状态V:）
  // ==========================================
  if (currentMillis - lastSerialSendTime >= serialInterval) {
    lastSerialSendTime = currentMillis;
    Serial.print("T:"); Serial.print(currentTemp);
    Serial.print(",H:"); Serial.print(currentHumi);
    Serial.print(",F:"); Serial.print(isFanOn ? 1 : 0);
    Serial.print(",V:"); Serial.println(isValveOpen ? 1 : 0); // 新增阀门状态
  }
}
