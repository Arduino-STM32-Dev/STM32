// STM32F103C8T6 UART 通信测试
// PA9  = Serial1 TX (接 ESP32 的 D16/RX2)
// PA10 = Serial1 RX (接 ESP32 的 D17/TX2)

void setup() {
  // 初始化与 ESP32 通信的串口，波特率 115200
  Serial1.begin(115200);
  
  // 板载 LED (PC13) 初始化，用来指示程序运行状态
  pinMode(PC13, OUTPUT);
  
  // 给一点时间让串口稳定
  delay(1000);
  Serial1.println("STM32 已启动，等待 ESP32 发送数据...");
}

void loop() {
  if (Serial1.available() > 0) {
    String command = Serial1.readStringUntil('\n');
    command.trim(); // 清除回车和换行
    
    // 打印接收到的指令
    Serial1.print("STM32 收到: ");
    Serial1.println(command);
    
    // 根据指令控制 PC13 绿灯
    if (command == "LED_ON") {
      digitalWrite(PC13, LOW);  // 大部分 BluePill 板载 LED 是低电平点亮
      Serial1.println("LED 已打开");
    } 
    else if (command == "LED_OFF") {
      digitalWrite(PC13, HIGH); // 关灯
      Serial1.println("LED 已关闭");
    }
  }
}