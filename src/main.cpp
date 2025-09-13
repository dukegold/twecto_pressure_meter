#define DEBUG_EN
#include "driver/adc.h"
#include <U8g2lib.h>
#include <I2C_eeprom.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include "RTClib.h"
#include <HardwareSerial.h>
#include <WiFi.h>              // Built-in
#include <PubSubClient.h>
#include "Sys_Variables.h"
#include "BluetoothSerial.h"
#include "time_keeping.h"
#include "ble_parser.h"
#include "lcd_display.h"
#include "datastore.h"
#include "Sensors.h"
#include <esp_task_wdt.h>

void IRAM_ATTR B_ENTER_ISR();
void IRAM_ATTR B_UP_ISR();
void IRAM_ATTR B_DOWN_ISR();
void IRAM_ATTR RAIN_INCR_ISR();
  
/******* Code objects **********/
BluetoothSerial SerialBT;
U8G2_ST7920_128X64_F_SW_SPI lcd(U8G2_R0, /*SCK*/ LCDSCKPIN, /*MOSI*/ LCDMOSIPIN,/* CS=*/ LCDCSPIN, /* reset=*/ U8X8_PIN_NONE);
RTC_DS1307 rtc;
I2C_eeprom eeprom(0x50, I2C_DEVICESIZE_24LC32);
// MQTT Related
WiFiClient espClient;
PubSubClient mqttClient(espClient);
Sensors sensors;
/******* Code Variables for EEPROM**********/
uint8_t   prehr;
uint8_t   dataSend;
uint8_t   log_choice;
uint8_t   lastReadDay;
uint32_t  slots;
char      wifi_ssid[40];
char      wifi_password[40];
float     sensor_zero_off[16];
float     sensor_cf[16];
/******* Code Variables for Logic**********/
float sensor_val[NUM_SENSORS]; // Sensor value
bool  sensor_con[NUM_SENSORS];
uint8_t rain_hour, rain_day;
volatile uint32_t  rain_incr_cnt = 0, last_micros_rg, last_micros_up, last_micros_dn;
uint8_t screen = 0;
bool loop_exit_update = false, log_choice_update = false;
bool saveStatus = true;
void(* resetFunc)(void) = 0;
boolean sdFound, rtc_found, bmp180_found, usb_module_found;
long int lastTime, lcdTime, menuTime, wifiTime;
String buf;
bool backLight = true, wifi_con = false;
char str_rep[75];
uint8_t last_log_min,logging_interval_minute;
byte second = 0,minute = 0,hour = 0,weekday = 0, monthday = 0,month = 0, year = 0;
volatile bool up_button = false, down_button = false, enter_button = false;
const char *log_intvl_str[] ={
  "1   Hour",
  "2  Hours",
  "4  Hours",
  "6  Hours",
  "12 Hours",
  "24 Hours",
  "2  Mins",
  "5  Mins",
  "10 Mins",
  "15 Mins",};        

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Disconnected from WiFi access point");
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.wifi_sta_disconnected.reason);
  resetFunc();
}

void setup() {
  Serial.begin(115200);
  /*********************** Set GPIO Modes **********************/
  pinMode(LCDLIGHTPIN, OUTPUT);
  adc_power_acquire(); // To fix the interrupts on GPIO 36 and 39
  pinMode(ENTER_PIN, INPUT_PULLUP);
  pinMode(UP_PIN, INPUT_PULLUP);
  pinMode(DOWN_PIN, INPUT_PULLUP);
  pinMode(EXIT_PIN, INPUT_PULLUP);
  pinMode(19,INPUT_PULLUP);
  pinMode(RAIN_SENSOR_PIN,INPUT_PULLUP);
  /*********************** End Set GPIO Modes **********************/
  /******************* Intialize GPIO Interrupts ******************/
  attachInterrupt(ENTER_PIN, B_ENTER_ISR, FALLING);
  attachInterrupt(UP_PIN   , B_UP_ISR   , FALLING);
  attachInterrupt(DOWN_PIN , B_DOWN_ISR , FALLING);
  attachInterrupt(RAIN_SENSOR_PIN, RAIN_INCR_ISR, RISING);
  /********************** LCD Initialization **********************/
  lcd.begin(/*Select=*/ ENTER_PIN, /*Right/Next=*/ U8X8_PIN_NONE, /*Left/Prev=*/ U8X8_PIN_NONE, /*Up=*/ UP_PIN, /*Down=*/ DOWN_PIN, /*Home/Cancel=*/ EXIT_PIN);
  lcd.clearBuffer();
  lcd.setDrawColor(1);
  digitalWrite(LCDLIGHTPIN,HIGH);
  backLight = true;
  // Logo
  lcd.drawXBMP( 0,0, 128, 64, TW_bits);
  lcd.sendBuffer();
  delay(1000);
  lcd.clearBuffer();
  lcd.setFont(u8g2_font_helvB10_tf);
  lcd.drawStr(40,11,"TweeX");
  lcd.setFont(u8g2_font_helvB08_tf);
  lcd.drawStr(10,20,"Weather Monitoring System");
  lcd.drawXBMP( 31,34, 67, 30, hand_bits);
  lcd.setFont(u8g2_font_helvB08_tf);
  lcd.drawStr(12,29,"Measure with Trust");
  lcd.sendBuffer();
  delay(1000);
  lcd.clearBuffer();
  lcd.drawStr(10,36,"Initializing....");
  lcd.sendBuffer();
  /******************* Intialize EEPROM ******************/
  Wire.begin();
  if(!eeprom.begin())
    Serial.println("EEPROM Init Fail..");

  if(eeprom.readByte(EEP_MAGIC_NUM) != EEPROM_MAGIC_NUM){
    //Defaults
    prehr                     = 0                        ; //Previous logging hour.
    dataSend                  = 0                        ; //Data sent in the current hour.
    log_choice                = 1                        ; //Logging interval selection.
    lastReadDay               = 0                        ; //Last day when logging was done.
    slots                     = 0xffffffff               ; //Current slots where logging has/has not occurred.
    sprintf(wifi_ssid,DEF_WIFI_SSID)                     ; //Wi-Fi SSID.
    sprintf(wifi_password,DEF_WIFI_PASS)                 ; //Wi-Fi Password.
    sensor_zero_off[0]        = 0                        ; //Zero calibration offset Sensor 0
    sensor_zero_off[1]        = 0                        ; //Zero calibration offset Sensor 1
    sensor_zero_off[2]        = 0                        ; //Zero calibration offset Sensor 2
    sensor_zero_off[3]        = 0                        ; //Zero calibration offset Sensor 3
    sensor_zero_off[4]        = 0                        ; //Zero calibration offset Sensor 4
    sensor_zero_off[5]        = 0                        ; //Zero calibration offset Sensor 5
    sensor_zero_off[6]        = 0                        ; //Zero calibration offset Sensor 6
    sensor_zero_off[7]        = 0                        ; //Zero calibration offset Sensor 7
    sensor_zero_off[8]        = 0                        ; //Zero calibration offset Sensor 8
    sensor_zero_off[9]        = 0                        ; //Zero calibration offset Sensor 9
    sensor_zero_off[10]       = 0                        ; //Zero calibration offset Sensor 10
    sensor_zero_off[11]       = 0                        ; //Zero calibration offset Sensor 11
    sensor_zero_off[12]       = 0                        ; //Zero calibration offset Sensor 12
    sensor_zero_off[13]       = 0                        ; //Zero calibration offset Sensor 13
    sensor_zero_off[14]       = 0                        ; //Zero calibration offset Sensor 14
    sensor_zero_off[15]       = 0                        ; //Zero calibration offset Sensor 15
    sensor_cf[0]              = 0                        ; //Correction Factor Sensor 0
    sensor_cf[1]              = 0                        ; //Correction Factor Sensor 1
    sensor_cf[2]              = 0                        ; //Correction Factor Sensor 2
    sensor_cf[3]              = 0                        ; //Correction Factor Sensor 3
    sensor_cf[4]              = 0                        ; //Correction Factor Sensor 4
    sensor_cf[5]              = 0                        ; //Correction Factor Sensor 5
    sensor_cf[6]              = 0                        ; //Correction Factor Sensor 6
    sensor_cf[7]              = 0                        ; //Correction Factor Sensor 7
    sensor_cf[8]              = 0                        ; //Correction Factor Sensor 8
    sensor_cf[9]              = 0                        ; //Correction Factor Sensor 9
    sensor_cf[10]             = 0                        ; //Correction Factor Sensor 10
    sensor_cf[11]             = 0                        ; //Correction Factor Sensor 11
    sensor_cf[12]             = 0                        ; //Correction Factor Sensor 12
    sensor_cf[13]             = 0                        ; //Correction Factor Sensor 13
    sensor_cf[14]             = 0                        ; //Correction Factor Sensor 14
    sensor_cf[15]             = 0                        ; //Correction Factor Sensor 15
    //EEPROM Default Updates
    eeprom.writeByte(EEP_PRE_HR,prehr);
    eeprom.writeByte(EEP_DATA_SENT,dataSend);
    eeprom.writeByte(EEP_LOG_CHOICE,log_choice);
    eeprom.writeByte(EEP_LAST_DAY,lastReadDay);
    eeprom.writeBlock(EEP_LOG_SLOTS,(uint8_t*) &slots,4);
    eeprom.writeByte(EEP_MAGIC_NUM,EEPROM_MAGIC_NUM);
    eeprom.writeBlock(EEP_WIFI_SSID,(uint8_t*) wifi_ssid,40);
    eeprom.writeBlock(EEP_WIFI_PASS,(uint8_t*) wifi_password,40);
    eeprom.writeBlock(EEP_SN0_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[0],4);
    eeprom.writeBlock(EEP_SN1_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[1],4);
    eeprom.writeBlock(EEP_SN2_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[2],4);
    eeprom.writeBlock(EEP_SN3_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[3],4);
    eeprom.writeBlock(EEP_SN4_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[4],4);
    eeprom.writeBlock(EEP_SN5_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[5],4);
    eeprom.writeBlock(EEP_SN6_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[6],4);
    eeprom.writeBlock(EEP_SN7_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[7],4);
    eeprom.writeBlock(EEP_SN8_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[8],4);
    eeprom.writeBlock(EEP_SN9_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[9],4);
    eeprom.writeBlock(EEP_SN10_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[10],4);
    eeprom.writeBlock(EEP_SN11_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[11],4);
    eeprom.writeBlock(EEP_SN12_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[12],4);
    eeprom.writeBlock(EEP_SN13_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[13],4);
    eeprom.writeBlock(EEP_SN14_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[14],4);
    eeprom.writeBlock(EEP_SN15_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[15],4);
    eeprom.writeBlock(EEP_SN0_CF,(uint8_t*) &sensor_cf[0],4);
    eeprom.writeBlock(EEP_SN1_CF,(uint8_t*) &sensor_cf[1],4);
    eeprom.writeBlock(EEP_SN2_CF,(uint8_t*) &sensor_cf[2],4);
    eeprom.writeBlock(EEP_SN3_CF,(uint8_t*) &sensor_cf[3],4);
    eeprom.writeBlock(EEP_SN4_CF,(uint8_t*) &sensor_cf[4],4);
    eeprom.writeBlock(EEP_SN5_CF,(uint8_t*) &sensor_cf[5],4);
    eeprom.writeBlock(EEP_SN6_CF,(uint8_t*) &sensor_cf[6],4);
    eeprom.writeBlock(EEP_SN7_CF,(uint8_t*) &sensor_cf[7],4);
    eeprom.writeBlock(EEP_SN8_CF,(uint8_t*) &sensor_cf[8],4);
    eeprom.writeBlock(EEP_SN9_CF,(uint8_t*) &sensor_cf[9],4);
    eeprom.writeBlock(EEP_SN10_CF,(uint8_t*) &sensor_cf[10],4);
    eeprom.writeBlock(EEP_SN11_CF,(uint8_t*) &sensor_cf[11],4);
    eeprom.writeBlock(EEP_SN12_CF,(uint8_t*) &sensor_cf[12],4);
    eeprom.writeBlock(EEP_SN13_CF,(uint8_t*) &sensor_cf[13],4);
    eeprom.writeBlock(EEP_SN14_CF,(uint8_t*) &sensor_cf[14],4);
    eeprom.writeBlock(EEP_SN15_CF,(uint8_t*) &sensor_cf[15],4);
  }
  else{
    //Load from EEPROM
    prehr = eeprom.readByte(EEP_PRE_HR);
    dataSend = eeprom.readByte(EEP_DATA_SENT);
    log_choice = eeprom.readByte(EEP_LOG_CHOICE);
    lastReadDay = eeprom.readByte(EEP_LAST_DAY);
    eeprom.readBlock(EEP_LOG_SLOTS,(uint8_t*) &slots,4);
    eeprom.readBlock(EEP_WIFI_SSID,(uint8_t*) wifi_ssid,40);
    eeprom.readBlock(EEP_WIFI_PASS,(uint8_t*) wifi_password,40);
    eeprom.readBlock(EEP_SN0_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[0],4);
    eeprom.readBlock(EEP_SN1_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[1],4);
    eeprom.readBlock(EEP_SN2_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[2],4);
    eeprom.readBlock(EEP_SN3_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[3],4);
    eeprom.readBlock(EEP_SN4_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[4],4);
    eeprom.readBlock(EEP_SN5_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[5],4);
    eeprom.readBlock(EEP_SN6_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[6],4);
    eeprom.readBlock(EEP_SN7_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[7],4);
    eeprom.readBlock(EEP_SN8_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[8],4);
    eeprom.readBlock(EEP_SN9_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[9],4);
    eeprom.readBlock(EEP_SN10_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[10],4);
    eeprom.readBlock(EEP_SN11_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[11],4);
    eeprom.readBlock(EEP_SN12_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[12],4);
    eeprom.readBlock(EEP_SN13_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[13],4);
    eeprom.readBlock(EEP_SN14_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[14],4);
    eeprom.readBlock(EEP_SN15_ZERO_CAL_OFF,(uint8_t*) &sensor_zero_off[15],4);
    eeprom.readBlock(EEP_SN0_CF,(uint8_t*) &sensor_cf[0],4);
    eeprom.readBlock(EEP_SN1_CF,(uint8_t*) &sensor_cf[1],4);
    eeprom.readBlock(EEP_SN2_CF,(uint8_t*) &sensor_cf[2],4);
    eeprom.readBlock(EEP_SN3_CF,(uint8_t*) &sensor_cf[3],4);
    eeprom.readBlock(EEP_SN4_CF,(uint8_t*) &sensor_cf[4],4);
    eeprom.readBlock(EEP_SN5_CF,(uint8_t*) &sensor_cf[5],4);
    eeprom.readBlock(EEP_SN6_CF,(uint8_t*) &sensor_cf[6],4);
    eeprom.readBlock(EEP_SN7_CF,(uint8_t*) &sensor_cf[7],4);
    eeprom.readBlock(EEP_SN8_CF,(uint8_t*) &sensor_cf[8],4);
    eeprom.readBlock(EEP_SN9_CF,(uint8_t*) &sensor_cf[9],4);
    eeprom.readBlock(EEP_SN10_CF,(uint8_t*) &sensor_cf[10],4);
    eeprom.readBlock(EEP_SN11_CF,(uint8_t*) &sensor_cf[11],4);
    eeprom.readBlock(EEP_SN12_CF,(uint8_t*) &sensor_cf[12],4);
    eeprom.readBlock(EEP_SN13_CF,(uint8_t*) &sensor_cf[13],4);
    eeprom.readBlock(EEP_SN14_CF,(uint8_t*) &sensor_cf[14],4);
    eeprom.readBlock(EEP_SN15_CF,(uint8_t*) &sensor_cf[15],4);
  }
  /*************** End Initialize EEPROM  ********************/
  update_log_inverval_minutes(&logging_interval_minute,log_choice);
  /*********************** Intialize Wifi ****************************/
  WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.mode(WIFI_STA);
  #ifdef DEBUG_EN
  Serial.println("Connecting Wifi ...");
  #endif

  WiFi.begin(wifi_ssid, wifi_password);
  for(int i = 0;i<10;i++) {
    if(WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
      delay(250);
      #ifdef DEBUG_EN
      Serial.print('.');
      #endif
    }
  }
  wifi_con = (WiFi.status() == WL_CONNECTED);
  /*********************** End Intialize Wifi ****************************/
  
  /**************** Bluetooth Setup ***************************/
  SerialBT.begin(DEVICE_ID); //Bluetooth device name
  /**************** End Bluetooth Setup ***************************/
    
  //########## SD Card Initialization ##########
  SPI.begin();
  sdFound = true;
  if (!SD.begin(SDCSPIN)) {
    #ifdef DEBUG_EN
    Serial.println("Err! SD Not Found.");
    #endif
    sdFound = false;
  }

  //############ RTC Initialization ###########
  if (! rtc.begin()) {
    #ifdef DEBUG_EN
    Serial.println("Couldn't find RTC");
    #endif
    rtc_found = false;
  } else {
    rtc_found = true;
  }
  //############ Sensors Initialization ###########
  sensors.begin();
  /*********************** Initialize MQTT Broker **********************/
  mqttClient.setServer(IOT_SERVER, 1883);
  if(wifi_con){
    mqttClient.connect(MQTT_CLIENT_ID,MQTT_USERNAME,MQTT_PASS);
  }
  enter_button = false;
  up_button = false;
  down_button = false;
  lcdTime = millis();
  /*********** Status screen here *********/
  readTime(&second,&minute,&hour,&weekday, &monthday,&month, &year); //refresh current time from RTC
  lcd.clearBuffer();
  lcd.setFont(u8g2_font_5x7_mf);
  lcd.drawStr(10,8,"Time");
  if(rtc_found) lcd.drawStr(62,8,getTime24().c_str());
  else lcd.drawStr(62,8,"Init fail");
  lcd.drawStr(10,17,"Wifi");
  if(wifi_con) lcd.drawStr(62,17,wifi_ssid);
  else lcd.drawStr(62,17,"NA");
  lcd.drawStr(10,35,"SDCard");
  if(sdFound) lcd.drawStr(62,35,"Initialized");
  else lcd.drawStr(62,35,"Init fail");
  lcd.sendBuffer();
  delay(2000);
  lcd.clearBuffer();
  last_log_min = (minute + logging_interval_minute)%60;
  loop_exit_update = true;
  #ifdef DEBUG_EN
  Serial.print("Log choice:");
  Serial.println(log_choice);
  Serial.print("Interval(min):");
  Serial.println(logging_interval_minute);
  Serial.print("last_log_min:");
  Serial.println(last_log_min);
  #endif
  // Rain sensor variables
  rain_hour = hour;
  rain_day = monthday;
  //Setup WDT
  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL);
}

void loop(){
  /*************************************************************
   *  Parse Bluetooth Messages
   *************************************************************/
  ble_parse(&SerialBT,
            wifi_password,
            wifi_ssid,
            &eeprom);
  /*************************************************************
   *  Time Refresh from RTC
   *************************************************************/
  readTime(&second,&minute,&hour,&weekday, &monthday,&month, &year);
  /*************************************************************
   *  LCD Update handling
   *************************************************************/
  if ((millis() - lastTime > 5000) | loop_exit_update) {
    lastTime = millis();
    sensors.sensor_update(sensor_val,sensor_cf, sensor_con);
    update_display(&lcd, screen, sensor_val, sensor_con, rtc_found, wifi_con);
    loop_exit_update = false;
  }
  
  /*************************************************************
   *  Handle Button inputs
   *************************************************************/
  if (enter_button) { // Enter LCD menu
    enter_button = false;
    if(!backLight){
      digitalWrite(LCDLIGHTPIN,HIGH);
      backLight = true;
    }
    //Remove interrupts
    detachInterrupt(ENTER_PIN);
    detachInterrupt(UP_PIN);
    detachInterrupt(DOWN_PIN);
    pinMode(ENTER_PIN, INPUT_PULLUP);
    pinMode(UP_PIN, INPUT_PULLUP);
    pinMode(DOWN_PIN, INPUT_PULLUP);
    menuOptions(&lcd,&log_choice, sensor_cf,&eeprom,&log_choice_update, &loop_exit_update);
    //Reattach interrupts
    attachInterrupt(ENTER_PIN, B_ENTER_ISR, FALLING);
    attachInterrupt(UP_PIN   , B_UP_ISR   , FALLING);
    attachInterrupt(DOWN_PIN , B_DOWN_ISR , FALLING);
    lcdTime = millis();
  }
  if(up_button || down_button){
    if(!backLight){
      digitalWrite(LCDLIGHTPIN,HIGH);
      backLight = true;
    }
    if(up_button){
      screen = (screen == MAX_SCREENS) ? 0 : screen+1;
    }
    else{
      screen = (screen == 0) ? MAX_SCREENS : screen-1;
    }
    up_button = false;
    down_button = false;
    loop_exit_update = true;
    lcdTime = millis();
  }
  if (EXIT) { //Exit: Long press will Restart the system. Time Span: 3 Sec
    // digitalWrite(LCDLIGHTPIN,HIGH);
    // backLight = true;
    // long int lastTime = millis();
    // while (EXIT)
    //   if (millis() - lastTime > 3000) {
    //     resetFunc();
    //   }
  }
  /*************************************************************
   *  Turn off LCD backlight after timeout
   *************************************************************/
  if (((millis() - lcdTime) > LCD_TIMEOUT) & backLight) {
    digitalWrite(LCDLIGHTPIN,LOW);
    backLight = false;
  }
  /*************************************************************
   *  Retry Wifi Connection if it disconnects in the middle
   *  of the loop. Keep trying after every WIFI_RETRY_MS
   *  miliseconds.
   *************************************************************/
  if(millis() - wifiTime > WIFI_RETRY_MS) {
    wifiTime = millis();
    wifi_con = (WiFi.status() == WL_CONNECTED);
    if(!wifi_con){
      WiFi.reconnect();
    }
    wifi_con = (WiFi.status() == WL_CONNECTED);
  }
  /*************************************************************
   *  Reconnect to MQTT broker if wifi disconnects.
   *************************************************************/
  if(wifi_con && !mqttClient.connected()){
    mqttClient.connect(MQTT_CLIENT_ID,MQTT_USERNAME,MQTT_PASS);
  }
  /*************************************************************
   *  Update Logging slots/interval after a selection update.
   *************************************************************/
  if ((lastReadDay != monthday) | log_choice_update) {
    log_choice_update = false;
    lastReadDay = monthday;
    eeprom.writeByte(EEP_LAST_DAY,lastReadDay);
    update_slots(&slots, log_choice);
    update_log_inverval_minutes(&logging_interval_minute,log_choice);
    eeprom.writeBlock(EEP_LOG_SLOTS,(uint8_t*) &slots,4);
  }
  /*************************************************************
   *  Perform Data Logging.
   *************************************************************/
  bool data_log_min;
  data_log_min = (last_log_min > minute)?
     (last_log_min - minute >= logging_interval_minute): //Wrap
     (minute - last_log_min >= logging_interval_minute); //Straight

  if ((slots & (1<<hour)) && log_choice < 7){ //Log data if slot reached
    saveData(sensor_val,sensor_con);
    if (mqttClient.connected() && wifi_con)
      sensor_mqtt(&mqttClient, sensor_val,sensor_con);
    slots &= ~(1<<hour);
    eeprom.writeBlock(EEP_LOG_SLOTS,(uint8_t*) &slots,4);
  }
  else if ((log_choice >= 7) && data_log_min){ //Or if set minutes interval passed
    #ifdef DEBUG_EN
    Serial.print("last_log_min:");
    Serial.println(last_log_min);
    #endif
    last_log_min = minute;
    saveData(sensor_val,sensor_con );
    if (mqttClient.connected() && wifi_con)
      sensor_mqtt(&mqttClient, sensor_val,sensor_con);
    data_log_min = false;
  }
  // Rain sensor increment/reset
  if(rain_incr_cnt != 0){
    sensors.incr_rain_cnt(rain_incr_cnt);
    rain_incr_cnt = 0;
  }
  if(rain_hour != hour){
    rain_hour = hour;
    sensors.reset_hourly_rain();
  }
  if(rain_day != monthday){
    rain_day = monthday;
    sensors.reset_daily_rain();
  }
  // MQTT loop to process messages
  if (mqttClient.connected() && wifi_con)
    mqttClient.loop();
  //Reset WDT Timer
  esp_task_wdt_reset();
}
/*************************************************************
 *  Interrupt Service Routines
 *************************************************************/
void IRAM_ATTR B_ENTER_ISR() {
    enter_button = true;
}
void IRAM_ATTR B_UP_ISR() {
  if((micros() - last_micros_up) >= SW_DEBOUNCE_TIME * 1000){
    up_button = true;
    last_micros_up = micros();
  }
}
void IRAM_ATTR B_DOWN_ISR() {
  if((micros() - last_micros_dn) >= SW_DEBOUNCE_TIME * 1000){
    down_button = true;
    last_micros_dn = micros();
  }
}
void IRAM_ATTR RAIN_INCR_ISR() {
  if((micros() - last_micros_rg) >= DEBOUNCE_TIME * 1000) { 
    rain_incr_cnt++;
    last_micros_rg = micros();
  }
}
