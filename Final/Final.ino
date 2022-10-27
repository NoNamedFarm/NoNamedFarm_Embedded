#include <WiFi.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define DHTPIN 33
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

LiquidCrystal_I2C lcd(0x3F, 16, 2);
//LiquidCrystal_I2C lcd(0x27, 16, 2);

#define SOIL 14

#define Motor_Relay 32
#define LED_Relay 25

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

IPAddress server_addr(192,168,137,186);  // IP of the MySQL *server* here
char user[] = "root";            // MySQL user login username
char password[] = "1234";         // MySQL user login password

// WiFi card example
const char ssid[] = "CDMA2000";          // your SSID
const char pass[] = "1q2w3e4r";      // your SSID Password

WiFiClient client;            // Use this for WiFi instead of EthernetClient
MySQL_Connection conn((Client *)&client);
MySQL_Cursor cur = MySQL_Cursor(&conn);

#define Move_Insert 5000
#define Move_Select 5000
#define Move_Sensor 500
#define Move_Actuator 500

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

  Serial.println();
  Serial.println();
  Serial.print("[WiFi] Connecting to ");
  Serial.println(ssid);

  // Begin WiFi section
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  while(WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  String ip = WiFi.localIP().toString();
  Serial.printf("[WiFi] Connected %s\n", ip.c_str());
  // End WiFi section

  Serial.println("[DB] Connecting...");
  if (conn.connect(server_addr, 3306, user, password)) {
    delay(1000);
    Serial.println("[DB] Success to DB!");
  }
  else Serial.println("[DB] Connection failed.");
}

float Temp = 0;
int Soil = 0;
bool Check_Water = false;
bool Check_Light = false;

unsigned long Insert_time = 0;
unsigned long Select_time = 0;
unsigned long sensor_time = 0;
unsigned long actuator_time = 0;

char INSERT_SQL[] = "INSERT INTO test_arduino.hello_arduino (message) VALUES ('Hello, Arduino!')";
char SELECT_SQL[] = "SELECT Light FROM test.test";

void loop() {
  unsigned long Now = millis();

  if(Now - sensor_time > Move_Sensor){
    sensor_time = Now;

    Temp = dht.readTemperature();
    Soil = analogRead(SOIL);

    if(isnan(Temp)){
      Serial.println("[DHT11] Failed to read");
    }
    if(isnan(Soil)){
      Serial.println("[Soil_Sensor] Failed to read");
    }

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

    //Insert temp&humi
    sprintf(INSERT_SQL, "INSERT INTO project.data(Temp, Humi) Values (%.2f, %d)", Temp, Soil);
  
    if(conn.connected()){
      cur.execute(INSERT_SQL);
    }
  }

  if(Now - Select_time > Move_Select){
    Select_time = Now;
    
    row_values *row = NULL;
    String Cmd = "";
    //--------------------------------------------------------
    //Select Water
    sprintf(SELECT_SQL, "SELECT Water FROM project.cmd");
    if(conn.connected()){
      cur.execute(SELECT_SQL);
      cur.get_columns();
      do{
        row = cur.get_next_row();
        if(row != NULL){
          Cmd = row->values[0];
        }
      } while(row != NULL);
      cur.close();
    }
    if(Cmd == "1") Check_Water = true;
    else if(Cmd == "0") Check_Water = false;

    //--------------------------------------------------------
    //Select Light
    sprintf(SELECT_SQL, "SELECT Light FROM project.cmd");
    if(conn.connected()){
      cur.execute(SELECT_SQL);
      cur.get_columns();
      do{
        row = cur.get_next_row();
        if(row != NULL){
          Cmd = row->values[0];
        }
      } while(row != NULL);
      cur.close();
    }
    if(Cmd == "1") Check_Light = true;
    else if(Cmd == "0") Check_Light = false;
  }
}
