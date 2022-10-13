#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <SocketIOclient.h>
#include <Hash.h>
#include <DHT.h>
#include <iostream>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

ESP8266WiFiMulti WiFiMulti;
SocketIOclient socketIO;

#define ssid "user"
#define pswd "00000000"
#define addr "10.11.100.100"
#define port 8880
#define URL "/socket.io/?EIO=4"

#define USE_SERIAL Serial

#define SOIL_SENSOR A0
#define DHTPIN 0
#define DHTTYPE DHT11
#define SCL 5
#define SDA 4

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x3F, 16, 2);
//LiquidCrystal_I2C lcd(0x27, 16, 2);

/*
 * Socket.io Event list
 * connect : 연결 시작
 * error : 연결 오류
 * disconnect : 연결 중단
 * (추정)event : 통신을 통한 데이터 수신
 * (추정)ACK : 데이터 송신 시 확인용으로 수신
 */

void socketIOEvent(socketIOmessageType_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case sIOtype_DISCONNECT:
      USE_SERIAL.printf("[Socket.IO] Disconnected!\n");
      break;
    case sIOtype_CONNECT:
      USE_SERIAL.printf("[Socket.IO] Connected to url: %s\n", payload);

      // join default namespace (no auto join in Socket.IO V3)
      socketIO.send(sIOtype_CONNECT, "/");
      break;
    case sIOtype_EVENT:
      USE_SERIAL.printf("[Socket.IO] get event: %s\n", payload);
      break;
    case sIOtype_ACK:
      USE_SERIAL.printf("[Socket.IO] get ack: %u\n", length);
      hexdump(payload, length);
      break;
    case sIOtype_ERROR:
      USE_SERIAL.println("[Socket.IO] can connected");
      USE_SERIAL.printf("            get error_code : %u\n", length);
      hexdump(payload, length);
      break;
    case sIOtype_BINARY_EVENT:
      USE_SERIAL.printf("[Socket.IO] get binary: %u\n", length);
      hexdump(payload, length);
      break;
    case sIOtype_BINARY_ACK:
      USE_SERIAL.printf("[Socket.IO] get binary ack: %u\n", length);
      hexdump(payload, length);
      break;
  }
}

void setup() {
  pinMode(SCL, OUTPUT);
  pinMode(SDA, OUTPUT);
  dht.begin();
  lcd.init();
  lcd.backlight();
  
  USE_SERIAL.begin(9600);
  USE_SERIAL.setDebugOutput(true);

  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();

  for(uint8_t t = 4; t > 0; t--) {
    USE_SERIAL.printf("[Serial] BOOT WAIT %d...\n", t);
    USE_SERIAL.flush();
    delay(1000);
  }

  // AP 비활성화
  if(WiFi.getMode() & WIFI_AP) {
    WiFi.softAPdisconnect(true);
  }

  // AP 추가
  WiFiMulti.addAP(ssid, pswd);
  USE_SERIAL.println("[WiFi] Add Access Point");

  // WiFi가 연결되어있지 않으면 대기
  while(WiFiMulti.run() != WL_CONNECTED) {
    USE_SERIAL.println("[WiFi] Connecting...");
    delay(100);
  }

  // IP를 문자열로 저장 후 출력
  String ip = WiFi.localIP().toString();
  USE_SERIAL.printf("[WiFi] Connected, IP : %s\n", ip.c_str());

  // 소켓io를 주소, 포트, URL을 통해 연결
  socketIO.begin(addr, port, URL);

  // 이벤트가 발생하면 socketIOEvent() 실행
  socketIO.onEvent(socketIOEvent);
}

#define Time 10000

float Temp = 0;
int Soil_Data = 0;

unsigned long messageTimestamp = 0;
unsigned long sensor_time = 0;

void loop() {
  socketIO.loop();

  uint64_t Now = millis();

  if(Now - sensor_time > Time){
    sensor_time = Now;

    Temp = dht.readTemperature();
    Soil_Data = analogRead(SOIL_SENSOR);
    if(isnan(Temp)){
      USE_SERIAL.println("Failed to read from DHT11!");
      return;
    }
    if(isnan(Soil_Data)){
      USE_SERIAL.println("Failed to read from Soil_Sensor!");
      return;
    }
  }

  // 10초마다 실행
  if(Now - messageTimestamp > Time) {
    messageTimestamp = Now;

    // 소켓io를 위한 Json 메시지(String형) 생성
    DynamicJsonDocument doc(1024);
    JsonArray Array = doc.to<JsonArray>();

    // Json에 이벤트 이름 추가
    Array.add("Soil Data");

    // String에 Json 객체가 직렬화되어 저장
    String output;
    serializeJson(doc, output);

    // 메시지 전송
    socketIO.sendEVENT(output);

    // Json 메시지 출력
    USE_SERIAL.println("[Socket.IO] Send Message :");
    USE_SERIAL.println(output);
  }
}
