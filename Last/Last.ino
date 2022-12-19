#include <WiFi.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

//========================================

//Distance sensor
#define Dis_sensor 39

//mp3 module
#define MP3 17

//DHT11
#define DHTPIN 33
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

//Soil_sensor
#define SOIL 36

//Relays
#define Motor_Relay 25
#define LED_Relay 32

//LCD
//Test_Type
LiquidCrystal_I2C lcd(0x27, 16, 2);
//Production_Type
//LiquidCrystal_I2C lcd(0x3F, 16, 2);

// wifi & DB
char Domain[] = "pnxelec.iptime.org";
IPAddress server_addr;
int port = 1989;
char user[] = "nonamed";
char password[] = "Mariadb";

char ssid[] = "user";
char pass[] = "88888888";

WiFiClient client;
MySQL_Connection conn((Client *)&client);
MySQL_Cursor cur = MySQL_Cursor(&conn);

long id = 1;
//char Device[] = "Test_Type";
char Device[] = "Production_Type";

//========================================

byte happy[6][8] = {
  {0b00000, 0b00011, 0b00100, 0b01000, 0b01000, 0b01000, 0b10000, 0b10000},
  {0b11111, 0b00000, 0b00000, 0b00000, 0b10001, 0b10001, 0b10001, 0b00000},
  {0b00000, 0b11000, 0b00100, 0b00010, 0b00010, 0b00010, 0b00001, 0b00001},
  {0b10000, 0b10001, 0b01001, 0b01000, 0b01000, 0b00100, 0b00011, 0b00000},
  {0b00000, 0b00000, 0b11111, 0b11111, 0b00000, 0b00000, 0b00000, 0b11111},
  {0b00001, 0b10001, 0b10010, 0b00010, 0b00010, 0b00100, 0b11000, 0b00000}
};

byte tired[6][8] = {
  {0b00000, 0b00011, 0b00100, 0b01001, 0b01010, 0b01000, 0b10000, 0b10000},
  {0b11111, 0b00000, 0b01010, 0b10001, 0b00000, 0b10001, 0b10001, 0b10001},
  {0b00000, 0b11000, 0b00100, 0b10010, 0b01010, 0b00010, 0b00001, 0b00001},
  {0b10000, 0b10000, 0b01000, 0b01001, 0b01000, 0b00100, 0b00011, 0b00000},
  {0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000, 0b00000, 0b11111},
  {0b00001, 0b00001, 0b00010, 0b10010, 0b00010, 0b00100, 0b11000, 0b00000}
};

//========================================

void WiFi_init(){
  Serial.println();
  Serial.println();
  Serial.print("[WiFi] Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  while(WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  String ip = WiFi.localIP().toString();
  Serial.printf("[WiFi] Connected %s\n", ip.c_str());
}

void DB_init(){
  WiFi.hostByName(Domain, server_addr);

  Serial.println("[DB] Connecting...");
  if (conn.connect(server_addr, port, user, password)) {
    delay(1000);
    Serial.println("[DB] Success to DB!");
  }
  else Serial.println("[DB] Connection failed.");
}

void Insert(char* sql){
  if(conn.connected()){
    cur.execute(sql);
  }
}

void Update(char* sql){
  if(conn.connected()){
    cur.execute(sql);
  }
}

long Select(char* sql){
  row_values *row = NULL;
  long Cmd;
  
  if(conn.connected()){
    cur.execute(sql);
    cur.get_columns();
    do{
      row = cur.get_next_row();
      if(row != NULL){
        Cmd = atol(row->values[0]);
      }
    } while(row != NULL);
    cur.close();
  }
  
  return Cmd;
}

void setup_Face(bool face){
  if(face == true){
    for(int i=0; i<6; i++){
      lcd.createChar(0x00 + i, happy[i]);
    }
  }
  else{
    for(int i=0; i<6; i++){
      lcd.createChar(0x00 + i, tired[i]);
    }
  }
}

void print_Face(){
  lcd.setCursor(6,0);
  for(int i=0; i<3; i++) lcd.write(0x00 + i);
  lcd.setCursor(6,1);
  for(int i=0; i<3; i++) lcd.write(0x03 + i);
}

//========================================

void setup() {
  dht.begin();
  lcd.init();
  lcd.backlight();

  pinMode(Dis_sensor, INPUT);
  pinMode(SOIL, INPUT);
  pinMode(MP3, OUTPUT);
  pinMode(Motor_Relay, OUTPUT);
  pinMode(LED_Relay, OUTPUT);
  digitalWrite(MP3, HIGH);
  digitalWrite(Motor_Relay, LOW);
  digitalWrite(LED_Relay, LOW);

  delay(10);
  
  Serial.begin(115200);
  //while (!Serial); // wait for serial port to connect. Needed for Leonardo only

  WiFi_init();
  DB_init();

  char SETUP_SQL[200] = "INSERT INTO nonamed.farm(air_humidity, device_id, is_light, is_water, soil_humidity, temperature) VALUE(50, 'Device_001', false, false, 500, 22.0)";
  sprintf(SETUP_SQL, "INSERT INTO nonamed.farm(air_humidity, device_id, is_light, is_water, soil_humidity, temperature) VALUE(50, '%s', '0', '0', 500, 22.0)", Device);
  Insert(SETUP_SQL);
}

float Temp = 0;
float Humi = 0;
int Soil = 0;
int Distance = 0;

bool Check_Water = false;
bool Check_Light = false;

unsigned long long Update_time = 0;
unsigned long long Select_time = 0;
unsigned long long sensor_time = 0;
unsigned long long actuator_time = 0;
unsigned long long sound_time = 0;

//시간 조정 필요
#define Move_Update 2000
#define Move_Select 1000
#define Move_Sensor 1000
#define Move_Actuator 3000
#define Move_Speaker 1000

void loop() {
  unsigned long long Now = millis();

  //센서 작동
  if(Now - sensor_time > Move_Sensor){
    sensor_time = Now;

    //온습도 측정
    Temp = dht.readTemperature();
    Humi = dht.readHumidity();
    Soil = analogRead(SOIL);

    //거리 측정
    Distance = analogRead(Dis_sensor);
    Serial.printf("Distance : %d\n", Distance);

    //온습도 측정 불가 시 오류 메시지 출력
    if(isnan(Temp)) Serial.println("[DHT11] Failed to read temp");
    else Serial.printf("temp : %f\n", Temp);
    if(isnan(Humi)) Serial.println("[DHT11] Failed to read humi");
    else Serial.printf("humi : %f\n", Humi);
    if(isnan(Soil)) Serial.println("[Soil_Sensor] Failed to read");
    else Serial.printf("soil : %d\n", Soil);

    //LCD 표정 출력
    if((Temp < 30 && Temp > 10) && (Soil < 3071 && Soil > 1023)){
      setup_Face(true);
      print_Face();
    }
    else{
      setup_Face(false);
      print_Face();
    }
  }

  //엑추에이터 작동
  if(Now - actuator_time > Move_Actuator){
    actuator_time = Now;
    Serial.println("Actuator");

    //등 on/off
    if(Check_Light){
      digitalWrite(LED_Relay, HIGH);
    }
    else{
      digitalWrite(LED_Relay, LOW);
    }

    //물 on/off
    if(Check_Water){
      digitalWrite(Motor_Relay, HIGH);
    }
    else{
      digitalWrite(Motor_Relay, LOW);
    }
  }

  //DB update 작동
  if(Now - Update_time > Move_Update){
    Update_time = Now;
    Serial.println("Update");

    char UPDATE_SQL[150] = "Update";
    sprintf(UPDATE_SQL, "UPDATE nonamed.farm SET temperature = %.2f, air_humidity = %.2f, soil_humidity = %d WHERE device_id = \'%s\'", Temp, Humi, Soil, Device);
    Update(UPDATE_SQL);
  }

  //DB select 작동
  if(Now - Select_time > Move_Select){
    Select_time = Now;
    Serial.println("Select");

    char SELECT_SQL[150] = "SELECT";
    long Cmd;
    //--------------------------------------------------------
    //Select Water
    sprintf(SELECT_SQL, "SELECT is_water FROM nonamed.farm WHERE device_id = '%s'", Device);
    Cmd = Select(SELECT_SQL);
    Serial.println(Cmd);
    if(Cmd == 1) Check_Water = true;
    else if(Cmd == 0) Check_Water = false;

    //--------------------------------------------------------
    //Select Light
    sprintf(SELECT_SQL, "SELECT is_light FROM nonamed.farm WHERE device_id = '%s'", Device);
    Cmd = Select(SELECT_SQL);
    Serial.println(Cmd);
    if(Cmd == 1) Check_Light = true;
    else if(Cmd == 0) Check_Light = false;
  }

  //칭찬 작동
  if(Now - sound_time > Move_Speaker){
    sound_time = Now;
    Serial.println("Speaker");

    if(Distance > 2000){
      digitalWrite(MP3, LOW);
      delay(100);
      digitalWrite(MP3, HIGH);
    }
  }
}