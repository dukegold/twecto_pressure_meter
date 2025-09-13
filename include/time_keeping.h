#ifndef TIME_KEEPING_H
#define TIME_KEEPING_H
#include <Wire.h>
#include <stdint.h>
#include <Arduino.h>

void readTime(byte *second,byte *minute, byte *hour,byte *weekday, byte *monthday, byte *month, byte *year);
// Data logging Slots update Function
void update_slots(uint32_t *slots, int log_choice){
  
    switch(log_choice){
    case 1:{
      *slots = 0x00ffffff;
      break;
    }
    case 2:{
      *slots = 0x00555555;
      break;
    }
    case 3:{
      *slots = 0x00111111;
      break;
    }
    case 4:{
      *slots = 0x00041041;
      break;
    }
    case 5:{
      *slots = 0x00001001;
      break;
    }
    case 6:{
      *slots = 0x00000001;
      break;
    }
    }
    #ifdef DEBUG_EN
    Serial.print("Log choice:");
    Serial.println(log_choice);
    Serial.print("Slots:");
    Serial.println(*slots);
    #endif
}
// Data logging Interval Minutes update Function
void update_log_inverval_minutes(uint8_t *logging_interval_minute, int log_choice){
  if(log_choice < 7)
    *logging_interval_minute = 0;
  else if(log_choice == 7)
    *logging_interval_minute = 2;
  else if(log_choice > 7)
    *logging_interval_minute = 5 *(log_choice - 7);
  #ifdef DEBUG_EN
    Serial.print("Interval(min):");
    Serial.println(*logging_interval_minute);
    #endif
}
// RTC Functions
const int DS1307 = 0x68;
/** @brief Returns current month as int type (1 to 12).
 *
 *  @return Month
 */
int getMonth() {
  byte second = 0,minute = 0,hour = 0,weekday = 0, monthday = 0,month = 0, year = 0;
  readTime(&second,&minute, &hour,&weekday, &monthday, &month,&year);
  return (int)month;
}
int getDay() {
  byte second = 0,minute = 0,hour = 0,weekday = 0, monthday = 0,month = 0, year = 0;
  readTime(&second,&minute, &hour,&weekday, &monthday, &month,&year);
  return (int)monthday;
}
int getYear() {
  byte second = 0,minute = 0,hour = 0,weekday = 0, monthday = 0,month = 0, year = 0;
  readTime(&second,&minute, &hour,&weekday, &monthday, &month,&year);
  return (int)year;
}
int getSecond() {
  byte second = 0,minute = 0,hour = 0,weekday = 0, monthday = 0,month = 0, year = 0;
  readTime(&second,&minute, &hour,&weekday, &monthday, &month,&year);
  return (int)second;
}
int getMinute() {
  byte second = 0,minute = 0,hour = 0,weekday = 0, monthday = 0,month = 0, year = 0;
  readTime(&second,&minute, &hour,&weekday, &monthday, &month,&year);
  return (int)minute;
}
int getHour() {
  byte second = 0,minute = 0,hour = 0,weekday = 0, monthday = 0,month = 0, year = 0;
  readTime(&second,&minute, &hour,&weekday, &monthday, &month,&year);
  return (int)hour;
}

byte decToBcd(byte val) {
  return ((val / 10 * 16) + (val % 10));
}

byte bcdToDec(byte val) {
  return ((val / 16 * 10) + (val % 16));
}

void readTime(byte *second,byte *minute, byte *hour,byte *weekday, byte *monthday, byte *month, byte *year) {
  Wire.beginTransmission(DS1307);
  Wire.write(byte(0));
  Wire.endTransmission();
  Wire.requestFrom(DS1307, 7);
  *second = bcdToDec(Wire.read());
  *minute = bcdToDec(Wire.read());
  *hour = bcdToDec(Wire.read());
  *weekday = bcdToDec(Wire.read());
  *monthday = bcdToDec(Wire.read());
  *month = bcdToDec(Wire.read());
  *year = bcdToDec(Wire.read());
}

void setRTCTime(byte second, byte minute, byte hour, byte weekday, byte monthday, byte month, byte year) {
  Wire.beginTransmission(DS1307);
  Wire.write(byte(0));
  Wire.write(decToBcd(0));
  Wire.write(decToBcd(minute));
  Wire.write(decToBcd(hour));
  Wire.write(decToBcd(0));
  Wire.write(decToBcd(monthday));
  Wire.write(decToBcd(month));
  Wire.write(decToBcd(year));
  Wire.endTransmission();
}

String getTime24() {
  byte second = 0,minute = 0,hour = 0,weekday = 0, monthday = 0,month = 0, year = 0;
  readTime(&second,&minute, &hour,&weekday, &monthday, &month,&year);
  char buf1[3];
  char buf2[3];
  sprintf(buf1, "%02d", hour);
  sprintf(buf2, "%02d", minute);
  return String(buf1) + ":" + String(buf2);
}

String getDate() {
  byte second = 0,minute = 0,hour = 0,weekday = 0, monthday = 0,month = 0, year = 0;
  readTime(&second,&minute, &hour,&weekday, &monthday, &month,&year);
  return String(monthday) + F("/") + String(month) + F("/") + String(year);
}

String getMonthYear() {
  byte second = 0,minute = 0,hour = 0,weekday = 0, monthday = 0,month = 0, year = 0;
  readTime(&second,&minute, &hour,&weekday, &monthday, &month,&year);
  return String(month) + F("_") + String(year);
}

String getDateFull() {
  byte second = 0,minute = 0,hour = 0,weekday = 0, monthday = 0,month = 0, year = 0;
  readTime(&second,&minute, &hour,&weekday, &monthday, &month,&year);
  return String(monthday) + F("/") + String(month) + F("/") + String(year) + F(" ") + String(hour) + F(":") + String(minute);
}

void getDateFullc(char* buf) {
  byte second = 0,minute = 0,hour = 0,weekday = 0, monthday = 0,month = 0, year = 0;
  readTime(&second,&minute, &hour,&weekday, &monthday, &month,&year);
  sprintf(buf, "%02d/%02d/%02d %02d:%02d",monthday,month,year,hour,minute);
  //return String(monthday) + F("/") + String(month) + F("/") + String(year) + F(" ") + String(hour) + F(":") + String(minute);
}

#endif
