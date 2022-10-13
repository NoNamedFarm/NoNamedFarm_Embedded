#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <SocketIOclient.h>
#include <Hash.h>

ESP8266WiFiMulti WiFiMulti;
SocketIOclient socketIO;

#define ssid "user"
#define pswd "00000000"
#define addr "10.11.100.100"
#define port 8880
#define URL "/socket.io/?EIO=4"

#define USE_SERIAL Serial

#define M1 5
#define M2 4
#define Relay 16
#define sbi(x) digitalWrite(x, HIGH)
#define cbi(x) digitalWrite(x, LOW)

bool Check_Water = false;
bool Check_Light = false;

void socketIOEvent(socketIOmessageType_t type, uint8_t * payload, size_t Length) {
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
    {
      USE_SERIAL.printf("[Socket.IO] get event: %s\n", payload);
      String cmd = (char*)payload;
      if(cmd.indexOf("Light") >= 0){
        if(cmd.indexOf("ON") >= 0) Check_Light = true;
        else if(cmd.indexOf("OFF") >= 0) Check_Light = false;
      }
      else if(cmd.indexOf("Water") >= 0){
        if(cmd.indexOf("ON") >= 0) Check_Water = true;
        else if(cmd.indexOf("OFF") >= 0) Check_Water = false;
      }
      break;
    }
    case sIOtype_ACK:
      USE_SERIAL.printf("[Socket.IO] get ack: %u\n", Length);
      hexdump(payload, Length);
      break;
    case sIOtype_ERROR:
      USE_SERIAL.println("[Socket.IO] can connected");
      USE_SERIAL.printf("            get error_code : %u\n", Length);
      hexdump(payload, Length);
      break;
    case sIOtype_BINARY_EVENT:
      USE_SERIAL.printf("[Socket.IO] get binary: %u\n", Length);
      hexdump(payload, Length);
      break;
    case sIOtype_BINARY_ACK:
      USE_SERIAL.printf("[Socket.IO] get binary ack: %u\n", Length);
      hexdump(payload, Length);
      break;
  }
  return;
}

void setup() {
  pinMode(M1, OUTPUT);
  cbi(M1);
  pinMode(M2, OUTPUT);
  cbi(M2);
  pinMode(Relay, OUTPUT);
  cbi(Relay);

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

#define Time 2500

unsigned long sensor_time = 0;

void loop() {
  socketIO.loop();

  unsigned long Now = millis();

  if(Now - sensor_time > Time){
    if(Check_Water == true) {sbi(M1);  cbi(M2);}
    else if(Check_Water == false) {cbi(M1);  cbi(M2);}

    if(Check_Light == true) sbi(Relay);
    else if(Check_Light == false) cbi(Relay);
  }
}
