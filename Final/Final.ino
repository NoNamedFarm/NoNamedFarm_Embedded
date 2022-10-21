#include <WiFi.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define DHTPIN 0
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define SCL 5
#define SDA 4
LiquidCrystal_I2C lcd(0x3F, 16, 2);
//LiquidCrystal_I2C lcd(0x27, 16, 2);

#define SOIL A0

#define M1 5
#define M2 4

#define Relay 16

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

byte mac_addr[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

//IPAddress server_addr(10,0,1,35);  // IP of the MySQL *server* here
IPAddress server_addr("domain : diary-database.ctmdbcffdqs4.ap-northeast-2.rds.amazonaws.com/heal");
char user[] = "root";              // MySQL user login username
char password[] = "mexfav-9tAjjo-hypveh";        // MySQL user login password

// WiFi card example
char ssid[] = "2316";            // your SSID
char pass[] = "999999999";       // your SSID Password

WiFiClient client;            // Use this for WiFi instead of EthernetClient
MySQL_Connection conn((Client *)&client);

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

  pinMode(M1, OUTPUT);
  pinMode(M2, OUTPUT);
  pinMode(Relay, OUTPUT);
  digitalWrite(M1, LOW);
  digitalWrite(M2, LOW);
  digitalWrite(Relay, LOW);
  
  Serial.begin(115200);
  while (!Serial); // wait for serial port to connect. Needed for Leonardo only

  // Begin WiFi section
  int status = WiFi.begin(ssid, pass);
  if ( status != WL_CONNECTED) {
    Serial.println("Couldn't get a wifi connection");
    while(true);
  }
  // print out info about the connection:
  else {
    Serial.println("Connected to network");
    IPAddress ip = WiFi.localIP();
    Serial.print("My IP address is: ");
    Serial.println(ip);
  }
  // End WiFi section

  Serial.println("Connecting...");
  if (conn.connect(server_addr, 3306, user, password)) {
    delay(1000);
  }
  else
    Serial.println("Connection failed.");
  conn.close();
}

float Temp = 0;
int Soil = 0;
bool Check_Water = false;
bool Check_Light = false;

unsigned long Insert_time = 0;
unsigned long Select_time = 0;
unsigned long sensor_time = 0;

char INSERT_SQL[] = "INSERT INTO test_arduino.hello_arduino (message) VALUES ('Hello, Arduino!')";
char SELECT_SQL[] = "SELECT Light FROM test.test";

void loop() {
  unsigned long Now = millis();

  if(Now - sensor_time > 30000){
    sensor_time = Now;

    Temp = dht.readTemperature();
    Soil = analogRead(SOIL);

    if(isnan(Temp)){
      Serial.println("Failed to read from DHT11!");
      continue;
    }
    if(isnan(Soil)){
      Serial.println("Failed to read from Soil_Sensor");
      continue;
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
  
  if(Now - Insert_time > 30000){
    Insert_time = Now;
    
    if(WiFi.status != WL_CONNECTED){
      Serial.println("try reconnect");
  
      WiFi.begin(ssid, pass);
      while(WiFi.status() != WL_CONNECTED){
        delay(500);
        Serial.print(".");
      }
  
      Serial.println();
      Serial.println("WiFi Connected!");
      Serial.println();
      Serial.println("Wifi connected!");
      Serial.println("\nConnected to network");
      Serial.print("My IP address is: ");
      Serial.println(WiFi.localIP());
    }
  
    sprintf(INSERT_SQL, "INSERT INTO test.test Values (%d, %d)", Temp, Soil);
  
    if(conn.connected()){
      cursor.execute(INSERT_SQL);
    }
  }

  if(Now - Select_time > 30000){
    Select_time - Now;

    if(WiFi.status != WL_CONNECTED){
      Serial.println("try reconnect");
  
      WiFi.begin(ssid, pass);
      while(WiFi.status() != WL_CONNECTED){
        delay(500);
        Serial.print(".");
      }
  
      Serial.println();
      Serial.println("WiFi Connected!");
      Serial.println();
      Serial.println("Wifi connected!");
      Serial.println("\nConnected to network");
      Serial.print("My IP address is: ");
      Serial.println(WiFi.localIP());
    }
    
    row_values *row = NULL;
    long head_count = 0;
    delay(1000);

    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
    SELECT_SQL[] = "SELECT Light FROM test.test";
    cur_mem.execute(Select_SQL);
    column_names *columns = cur_mem.get_columns();

    do{
      row = cur_mem.get_next_row();
      if(row != NULL){
        head_count = atoi(row.values[0]);
      }
    } while(row != NULL);
    delete cur_mem;

    Serial.print("NYC pop = ");
    Serial.println(head_count);
    delay(500);

    SELECT_SQL[] = "SELECT Water FROM test.test";
    cur.execute(Select_SQL);
    cur.get_columns();
    do{
      row = cur.get_next_row();
      if(row != NULL){
        head_count = atoi(row.values[0]);
      }
    } while(row != NULL);
    cur.close();

    Serial.print("NYC pop = ");
    Serial.pritnln(head_count);
    Serial.print("NYC pop increased by 12 = ");
    Serial.println(head_count + 12);
  }
}
