#ifndef DATASTORE_H
#define DATASTORE_H
#include "Sys_Variables.h"
#include "time_keeping.h"
#include <SD.h>
#include <arduino.h>
#include <PubSubClient.h>

String getFilename() {
  String s = "/";
  s += DEVICE_ID;
  s += "_";
  s += getMonthYear();
  s += ".csv";
  return s;
}
void saveData(float *sensor_val, bool *sensor_con) {
  String filename = getFilename();
  bool writeHeader;
  File file;
  if (SD.exists((char*)filename.c_str())) {
    writeHeader = false;
    file = SD.open(filename.c_str(), FILE_APPEND);
  }
  else{
    writeHeader = true;
    file = SD.open(filename.c_str(), FILE_WRITE);
  }
  String header;
  header = ",Date,Time,";
  for(int j = 0;j<NUM_SENSORS;j++){
    header += sensor_names[j];
    header += " (";
    header += sensor_units[j];
    header += "),";
  }
  if (file) {
    if (writeHeader) {
      file.println(",############,##################################,######################,###################");
      file.println(",############,Report,,###################");
      file.println(",############,WMS Data Recorder,,###################");
      file.print(",############,Device ID: ");
      file.print(DEVICE_ID);
      file.print(", Serial No: ");
      file.print(DEV_SER);
      file.println(",###################");
      file.println(",############,,,###################");
      file.println(",############,Powered by Teknowish,,###################");
      file.println(",############,##################################,######################,###################");
      file.println(header.c_str());
    }
    #ifdef DEBUG_EN
    Serial.println("Data Saved");
    #endif
    file.print(",");
    file.print(getDate().c_str());
    file.print(",");
    file.print(getTime24().c_str());
    file.print(",");
    for(int j = 0;j<NUM_SENSORS;j++){
      if(sensor_con[j])
        file.print(sensor_val[j]);
      else
        file.print("NC");
      file.print(",");
    }
    file.print("\n");
    file.close();
    #ifdef DEBUG_EN
    Serial.println("SD Data logged");
    #endif
  }
  else{
    #ifdef DEBUG_EN
    Serial.println("SD File Open failed");
    #endif
  }  
}

void sensor_mqtt(PubSubClient *mqttClient, float *sensor_val,bool *sensor_con){
  String payload;
  payload = "{";
  for(int j = 0;j<NUM_SENSORS;j++){
      payload += "\"";
      payload += sensor_mqtt_param[j];
      payload += "\": ";
      if(sensor_con[j])
        payload += sensor_val[j];
      else
        payload += "-99999";
      if(j != (NUM_SENSORS-1))
        payload += ",";
  }
  payload += "}";
  mqttClient->publish(IOT_PUB_TOPIC, payload.c_str());
}

#endif
