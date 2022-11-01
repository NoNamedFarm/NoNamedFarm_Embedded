#include <WiFi.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>

//========================================

//MP3 module
HardwareSerial dfp(2);
DFRobotDFPlayerMini mp3;
#define DF_tx 22
#define DF_rx 23
#define max_index 13

//DHT11
#define DHTPIN 33
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

//Soil_sensor
#define SOIL 14

//Relays
#define Motor_Relay 32
#define LED_Relay 25

//LCD
LiquidCrystal_I2C lcd(0x3F, 16, 2);

// wifi & DB
IPAddress server_addr(192,168,137,186);
char user[] = "root";
char password[] = "1234";

char ssid[] = "CDMA2000";
char pass[] = "1q2w3e4r";

WiFiClient client;
MySQL_Connection conn((Client *)&client);
MySQL_Cursor cur = MySQL_Cursor(&conn);

char Device[] = "Proto_Type";

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
  Serial.println("[DB] Connecting...");
  if (conn.connect(server_addr, 3306, user, password)) {
    delay(1000);
    Serial.println("[DB] Success to DB!");
  }
  else Serial.println("[DB] Connection failed.");
}

void mp3_init(){
  dfp.begin(9600, SERIAL_8N1, DF_rx, DF_tx);
  mp3.begin(dfp);
  mp3.volume(25);
}

void Insert(char* sql){
  if(conn.connected()){
    cur.execute(sql);
  }
}

bool Select(char* sql){
  row_values *row = NULL;
  bool Cmd;
  
  if(conn.connected()){
    cur.execute(sql);
    cur.get_columns();
    do{
      row = cur.get_next_row();
      if(row != NULL){
        Cmd = row->values[0];
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

  pinMode(Motor_Relay, OUTPUT);
  pinMode(LED_Relay, OUTPUT);
  digitalWrite(Motor_Relay, LOW);
  digitalWrite(LED_Relay, LOW);

  delay(10);
  
  Serial.begin(115200);
  while (!Serial); // wait for serial port to connect. Needed for Leonardo only

  WiFi_init();
  DB_init();
  mp3_init();

  char SETUP_SQL[] = "INSERT";
  sprintf(SETUP_SQL, "INSERT INTO nonamed.farm(id,temperature,humidity_atm,humidity_soil,is_water,is_light,name,crop,created_date,user_id) VALUES(%s, 25.00, 0.00, 0, false, false, NULL, NULL, NULL, NULL)", Device);
  Insert(SETUP_SQL);
}

float Temp = 0;
float Humi = 0;
int Soil = 0;

bool Check_Water = false;
bool Check_Light = false;

unsigned long long Insert_time = 0;
unsigned long long Select_time = 0;
unsigned long long sensor_time = 0;
unsigned long long actuator_time = 0;

#define Move_Insert 5000
#define Move_Select 5000
#define Move_Sensor 500
#define Move_Actuator 500

void loop() {
  unsigned long long Now = millis();

  if(Now - sensor_time > Move_Sensor){
    sensor_time = Now;

    Temp = dht.readTemperature();
    Humi = dht.readHumidity();
    Soil = analogRead(SOIL);

    if(isnan(Temp)) Serial.println("[DHT11] Failed to read temp");
    if(isnan(Humi)) Serial.println("[DHT11] Failed to read humi");
    if(isnan(Soil)) Serial.println("[Soil_Sensor] Failed to read");

    if((Temp < 30 && Temp > 10) && (Soil < 768 && Soil > 256)){
      setup_Face(true);
      print_Face();
    }
    else{
      setup_Face(false);
      print_Face();
    }
  }

  if(Now - actuator_time > Move_Actuator){
    actuator_time = Now;
    
    if(Check_Light){
      digitalWrite(LED_Relay, HIGH);
    }
    else{
      digitalWrite(LED_Relay, LOW);
    }

    if(Check_Water){
      digitalWrite(Motor_Relay, HIGH);
    }
    else{
      digitalWrite(Motor_Relay, LOW);
    }
  }

  if(Now - Insert_time > Move_Insert){
    Insert_time = Now;

    char INSERT_SQL[] = "INSERT";
    sprintf(INSERT_SQL, "INSERT INTO nonamed.farm(name,temperature,humidity_atm,humidity_soil) VALUES (%s, %.2f, %.2f, %d)", Device, Temp, Humi, Soil);
  
    Insert(INSERT_SQL);
  }

  if(Now - Select_time > Move_Select){
    Select_time = Now;

    char SELECT_SQL[] = "SELECT";
    bool Cmd = false;
    //--------------------------------------------------------
    //Select Water
    sprintf(SELECT_SQL, "SELECT is_water FROM nonamed.farm");
    Cmd = Select(SELECT_SQL);
    if(Cmd == true) Check_Water = true;
    else if(Cmd == false) Check_Water = false;

    //--------------------------------------------------------
    //Select Light
    sprintf(SELECT_SQL, "SELECT is_light FROM nonamed.farm");
    Cmd = Select(SELECT_SQL);
    if(Cmd == true) Check_Light = true;
    else if(Cmd == false) Check_Light = false;
  }
}
