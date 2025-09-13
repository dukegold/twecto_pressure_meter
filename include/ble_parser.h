#ifndef BLE_PARSER_H
#define BLE_PARSER_H
#include "Sys_Variables.h"
#include <BluetoothSerial.h>
#include <WiFi.h>
#include <I2C_eeprom.h>

bool ble_parse(BluetoothSerial *SerialBT,
               char            *wifi_password,
               char            *wifi_ssid,
               I2C_eeprom      *eeprom)
{
  while(SerialBT->available()){
    char buf = SerialBT->read();
    int pass_len = 0;
    int index = 0;
    while(BLE_PASS[pass_len] != '\0'){
      pass_len++;
    }
    if(buf == '#'){
      //Flush
      while(SerialBT->available())
        SerialBT->read();
      SerialBT->print("Enter Password: ");
      bool comp_done = false, auth = true;
      index = 0;
      while((!comp_done) && (index < pass_len)){
        if(SerialBT->available()){
          buf = SerialBT->read();
          if(BLE_PASS[index] != buf){
            comp_done = true;
            auth = false;
          }
          index++;
        }
      }
      if(SerialBT->available()){
        buf = SerialBT->read();
        if((buf != '\r') && (buf != '\n')){
          auth = false;
        }
      }
      //Flush
      while(SerialBT->available())
        SerialBT->read();
      buf = '\0';
      if(auth){
        SerialBT->println("Authentication success->");
        SerialBT->println("Enter new SSID:");
        index = 0;
        while(buf != '\r'){
          if(SerialBT->available()){
            buf = SerialBT->read();
            if(buf != '\r'){
              wifi_ssid[index] = buf;
              index++;
            }
          }
        }
        wifi_ssid[index] = '\0';
        buf = '#';
        while(SerialBT->available())
          SerialBT->read();
        SerialBT->print("New SSID is:");
        SerialBT->print(wifi_ssid);      
        SerialBT->println("\nEnter new password:");
        index = 0;
        while(buf != '\r'){
          if(SerialBT->available()){
            buf = SerialBT->read();
            if(buf != '\r'){
              wifi_password[index] = buf;
              index++;
            }
          }
        }
        wifi_password[index] = '\0';
        SerialBT->print("\nNew password is:");
        SerialBT->print(wifi_password);
        while(SerialBT->available())
          SerialBT->read();
        WiFi.begin(wifi_ssid, wifi_password);
        SerialBT->println("Connecting to WiFi ..");
        int j = 0;
        while (WiFi.status() != WL_CONNECTED) {
          SerialBT->print('.');
          delay(1000);
          j++;
          if(j == 20){
            SerialBT->println("Failed to connect Wifi. Check credentials");
            break;
          }
        }
        SerialBT->println(WiFi.localIP());
        for(int i=0;i<40;i++){
          eeprom->writeByte(EEP_WIFI_SSID+i,wifi_ssid[i]);
          eeprom->writeByte(EEP_WIFI_PASS+i,wifi_password[i]);
        }
        return true;
      }// Authentication Success
      else{
        SerialBT->println("Authentication Failure.");
        return false;
      }// Authentication Failure
    }
  }
  return true;
}
#endif