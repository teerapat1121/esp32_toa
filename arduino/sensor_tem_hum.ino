#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <HTTPClient.h>  // สำหรับ HTTP POST

const char* ssid = "CTC-WIFI";
const char* password = "";
const char* mqtt_server = "broker.emqx.io";
const char* serverName = "http://172.31.14.43:8080/upload-data";
WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_AHTX0 am2315c;

long lastMsg = 0;
long interval = 2000;  // 2 วินาที
char msg[50];
int option = 0;        // ตัวแปรสำหรับเก็บค่า test/option
int control = 0;       // ตัวแปรควบคุมเงื่อนไข (0=ปิด, 1=เปิด)
long lastDatabaseUpdate = 0; // เก็บเวลาส่งข้อมูลครั้งสุดท้าย
long updateInterval = 7200000; // 2 ชั่วโมงในมิลลิวินาที 7200000
int sensorFailCount = 0;  // ตัวนับความล้มเหลวของเซนเซอร์
const int maxSensorFails = 10;  // จำนวนครั้งที่อนุญาตให้ล้มเหลวก่อนรีบูต
const int ssrPin = 26;  // พินควบคุม SSR (เปลี่ยนจาก ledPin)
const int ledPin = 14;
const int ledSSR = 12;  // พิน LED ทำงานเมื่อ SSR เปิดใช้งาน

bool readSensorData(float &temperature, float &humidity) {
  sensors_event_t humEvent, tempEvent;
  am2315c.getEvent(&humEvent, &tempEvent);
  if (!isnan(humEvent.relative_humidity) && !isnan(tempEvent.temperature)) {
    temperature = tempEvent.temperature;
    humidity = humEvent.relative_humidity;
    return true;
  }
  return false;
}


void setup() {
  Serial.begin(115200);
  pinMode(ssrPin, OUTPUT);
  digitalWrite(ssrPin, LOW);  // ปิด SSR ตอนเริ่มต้น

  pinMode(ledPin, OUTPUT); // กำหนดพิน LED เป็น OUTPUT
  digitalWrite(ledPin, LOW); // ปิด LED ตอนเริ่มต้น

  pinMode(ledSSR, OUTPUT);  // กำหนดพิน LED SSR
  digitalWrite(ledSSR, LOW);  // ปิด LED SSR ตอนเริ่มต้น

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  if (!am2315c.begin()) {
    Serial.println("ไม่พบ AM2315C! ตรวจสอบการเชื่อมต่อของสาย");
    while (1) delay(10);
  }
  Serial.println("AM2315C พบแล้ว");
}

void setup_wifi() {
  delay(10);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Toa")) {
      Serial.println("connected");
      client.subscribe("test/option");   // สมัครสมาชิก topic test/option
      client.subscribe("test/control"); // สมัครสมาชิก topic test/control
    } else {
      Serial.print("failed, rc=");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

void ensureWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    setup_wifi();
  }
}

void ensureMQTTConnection() {
  if (!client.connected()) {
    reconnect();
  }
}

void handleTestOption(String message) {
  if (message == "1") {
    option = 1;
    Serial.println("Option set to 1");
  } else if (message == "2") {
    option = 2;
    Serial.println("Option set to 2");
  } else if (message == "3") {
    option = 3;
    Serial.println("Option set to 3");
  }
   else if (message == "4") {
    option = 4;
    Serial.println("Option set to 4");
  }
   else if (message == "5") {
    option = 5;
    Serial.println("Option set to 5");
  }
}

void handleTestControl(String message) {
  message.trim();
  Serial.print("Received control message: ");
  Serial.println(message);

  if (message == "1") {
    control = 1;
    Serial.println("Control set to 1 (SSR condition enabled)");
    digitalWrite(ledPin, LOW);  // ปิด LED (ไม่เกี่ยวข้องในโหมดนี้)

    if (client.publish("test/status", "on", true)) {
      Serial.println("Published 'on' to MQTT topic.");
    } else {
      Serial.println("Failed to publish 'on'.");
    }

  } else if (message == "0") {
    control = 0;
    Serial.println("Control set to 0 (SSR condition disabled)");
    digitalWrite(ssrPin, LOW);  // ปิด SSR
    digitalWrite(ledPin, HIGH); // เปิด LED

    if (client.publish("test/status", "off", true)) {
      Serial.println("Published 'off' to MQTT topic.");
    } else {
      Serial.println("Failed to publish 'off'.");
    }
  }
}




// ฟังก์ชัน callback เมื่อได้รับข้อความจาก topic ที่ subscribe
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  // ตรวจสอบข้อความที่ได้รับจาก topic test/option
  if (String(topic) == "test/control") {
    handleTestControl(message);
  } else if (String(topic) == "test/option") {
    handleTestOption(message);
  } else {
    Serial.println("Unknown topic received.");
  }


}
void sendToDatabase(float temperature, float humidity, int option) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    // รวมค่า option ไปใน payload
    String payload = "{\"temperature\":" + String(temperature) +
                     ",\"humidity\":" + String(humidity) +
                     ",\"control_mode\":" + String(option) + "}";

    int httpResponseCode = http.POST(payload);

    if (httpResponseCode == 200 || httpResponseCode == 201) {
      Serial.println("Data sent successfully!");
    } else {
      Serial.print("Data failed to send! HTTP Response code: ");
      Serial.println(httpResponseCode);
      String response = http.getString();
      Serial.print("Server Response: ");
      Serial.println(response);  // แสดงข้อความที่ได้รับจาก server
    }

    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);

    http.end(); // ปิดการเชื่อมต่อ
  } else {
    Serial.println("WiFi disconnected, cannot send data.");
  }
}

void publishSSRStatus(bool ssrOn) {
  const char* status = ssrOn ? "เปิด" : "ปิด";
  if (client.publish("test/statusSSR", status, true)) {
    Serial.printf("Published '%s' to test/statusSSR.\n", status);
  } else {
    Serial.printf("Failed to publish '%s' to test/statusSSR.\n", status);
  }
}

bool reconnectSensor() {
  Serial.println("Reconnecting to sensor...");
  if (am2315c.begin()) {
    Serial.println("Sensor reconnected successfully.");
    return true;
  } else {
    Serial.println("Failed to reconnect to sensor.");
    return false;
  }
}

void loop() {
  ensureWiFiConnection();  // ตรวจสอบการเชื่อมต่อ WiFi
  ensureMQTTConnection();  // ตรวจสอบการเชื่อมต่อ MQTT
  client.loop();  // รักษาการเชื่อมต่อ MQTT ให้ทำงานต่อเนื่อง

  unsigned long now = millis(); // ใช้ unsigned long ป้องกัน overflow

  // ตรวจสอบการเชื่อมต่อเซ็นเซอร์ก่อนทำงาน
  if (!am2315c.begin()) {
    Serial.println("ไม่พบ AM2315C! ตรวจสอบการเชื่อมต่อของสาย");
    while (true) {
      // หยุดทำงานจนกว่าจะเชื่อมต่อกับเซ็นเซอร์ได้
      delay(1000);  // รอ 1 วินาที
    }
  }

  // ประกาศตัวแปร temperature และ humidity ภายนอก if block
  float temperature = 0.0;
  float humidity = 0.0;

  // อ่านข้อมูลจากเซ็นเซอร์
  if (readSensorData(temperature, humidity)) { 
    sensorFailCount = 0; // รีเซ็ตตัวนับความล้มเหลว
    Serial.printf("Temperature: %.2f °C, Humidity: %.2f%%\n", temperature, humidity);

    snprintf(msg, sizeof(msg), "%.2f", humidity);
    client.publish("test/hum", msg, true);

    snprintf(msg, sizeof(msg), "%.2f", temperature);
    client.publish("test/tem", msg, true);

    // ควบคุม LED และ SSR ตามเงื่อนไข
    if (control == 1 && humidity < 90) { 
       bool ssrOn = (option == 1 && (humidity < 85 || temperature > 35)) ||
                     (option == 2 && (humidity < 85 || temperature > 30)) ||
                     (option == 3 && (humidity < 85 || temperature > 22)) ||
                     (option == 4 && (humidity < 85 || temperature > 25)) ||
                     (option == 5 && (humidity < 85 || temperature > 24));

      digitalWrite(ssrPin, ssrOn ? HIGH : LOW);
      digitalWrite(ledSSR, ssrOn ? HIGH : LOW);
      publishSSRStatus(ssrOn);

      Serial.println(ssrOn ? "SSR is ON, LED SSR is ON" : "SSR is OFF, LED SSR is OFF");
    } else {
      digitalWrite(ssrPin, LOW);
      digitalWrite(ledSSR, LOW);
      publishSSRStatus(false);
      Serial.println("SSR condition disabled (SSR is OFF, LED SSR is OFF)");
    }
  } else {
    sensorFailCount++; // เพิ่มจำนวนครั้งที่อ่านเซ็นเซอร์ล้มเหลว
    Serial.println("Failed to read from sensor.");

    if (sensorFailCount >= maxSensorFails) { 
      Serial.println("Sensor failure limit reached! Restarting ESP32...");
      delay(5000);
      ESP.restart();
    } else {
      Serial.printf("Sensor failed %d/%d times, attempting to reconnect...\n", sensorFailCount, maxSensorFails);
      if (!reconnectSensor()) {
        Serial.println("Sensor reconnect failed.");
        digitalWrite(ssrPin, LOW);
        digitalWrite(ledSSR, LOW);
        publishSSRStatus(false);
      }
    }
  }
  
  // ส่งข้อมูลไปยังฐานข้อมูลทุก 2 ชั่วโมง (updateInterval)
  if (now - lastDatabaseUpdate >= updateInterval) {
    lastDatabaseUpdate = now; // อัพเดทเวลาของการส่งข้อมูลไปยังฐานข้อมูล

    // ใช้ค่า temperature และ humidity ที่อ่านล่าสุด (ไม่ต้องอ่านใหม่)
    if (sensorFailCount == 0) { // ส่งข้อมูลเฉพาะเมื่อเซ็นเซอร์ทำงานปกติ
      sendToDatabase(temperature, humidity, option);
    } else {
      Serial.println("Skipping database update due to sensor failure.");
    }
  }
  delay(2000);
}
