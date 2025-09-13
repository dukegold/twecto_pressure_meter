#ifndef SENSORS_H
#define SENSORS_H
#include <ADS1115_WE.h>
#include "Sys_Variables.h"
#include <HardwareSerial.h>
#include <ModbusMaster.h>
#include "DFRobot_EnvironmentalSensor.h"

#define WIND_SPD_SENSOR_SLAVE_ID 1
#define WIND_DIR_SENSOR_SLAVE_ID 2
#define SOLAR_RAD_SENSOR_SLAVE_ID 3

#define MODESWITCH        /*UART:*/1
class Sensors{
  private:
  float                zcal[NUM_SENSORS];
  char                 databuffer[35];
  double               temp;
  ModbusMaster         wind_spd_sensor;
  ModbusMaster         wind_dir_sensor;
  ModbusMaster         solar_rad_sensor;
  DFRobot_EnvironmentalSensor environment{DFRobot_EnvironmentalSensor(SEN050X_DEFAULT_DEVICE_ADDRESS, &Serial1)};
  bool                 df_sensor_con;
  uint32_t             rain_intr_cnt_1h;
  uint32_t             rain_intr_cnt_24h;

  public:

  void begin(void){
    Serial2.begin(9600,SERIAL_8N1,RXD2,TXD2); // Modbus Master
    Serial1.begin(9600,SERIAL_8N1,RXD1,TXD1); // DFRobot sensor
    wind_spd_sensor.begin(WIND_SPD_SENSOR_SLAVE_ID,Serial2);
    solar_rad_sensor.begin(SOLAR_RAD_SENSOR_SLAVE_ID,Serial2);
    wind_dir_sensor.begin(WIND_DIR_SENSOR_SLAVE_ID,Serial2);
    df_sensor_con = true;
    rain_intr_cnt_1h = 0;
    rain_intr_cnt_24h = 0;
    if(environment.begin() != 0){
      Serial.println("Sensor initialize failed!!");
      df_sensor_con = false;
    }
    else
      Serial.println(" Sensor  initialize success!!");
  }

  void incr_rain_cnt(uint32_t count){
    rain_intr_cnt_1h += count; 
    rain_intr_cnt_24h += count; 
  }

  void reset_hourly_rain(void){
    rain_intr_cnt_1h = 0; 
  }
  void reset_daily_rain(void){
    rain_intr_cnt_24h = 0;
  }
  void sensor_update(float *sensor_values, float *sensor_cf, bool *sensor_con){
    int result;
    uint16_t data;
    //Sensor 0: Wind Speed
    // Address 1, 1 register
    result = wind_spd_sensor.readHoldingRegisters(0x0, 1);
    if (result == wind_spd_sensor.ku8MBSuccess){
      data = wind_spd_sensor.getResponseBuffer(0);
      sensor_values[0] = (data/10) + sensor_cf[0];
      sensor_con[0] = true;
    }
    else{
      sensor_values[0] = -1;
      sensor_con[0] = false;
    }
    //Sensor 2:Wind Direction
    // Address 1, 1 register
    result = wind_dir_sensor.readHoldingRegisters(0x1, 1);
    if (result == wind_dir_sensor.ku8MBSuccess){
      data = wind_dir_sensor.getResponseBuffer(0);
      sensor_values[1] = (data) + sensor_cf[1];
      sensor_con[1] = true;
    }
    else{
      sensor_values[1] = -1;
      sensor_con[1] = false;
    }
    //Sensor 2:Temp
    //Sensor 3:Humi
    //Sensor 4:Pressure
    sensor_con[2] = df_sensor_con;
    sensor_con[3] = df_sensor_con;
    sensor_con[4] = df_sensor_con;
    if(df_sensor_con){
    sensor_values[2] = environment.getTemperature(TEMP_C)     + sensor_cf[2];
    sensor_values[3] = environment.getHumidity()              + sensor_cf[3];
    sensor_values[4] = environment.getAtmospherePressure(HPA) + sensor_cf[4];
    }
    //Sensor 5: Rain 1h
    //Sensor 6: Rain 24h
    sensor_con[5] = true;
    sensor_con[6] = true;
    sensor_values[5] = (rain_intr_cnt_1h * 0.254) + sensor_cf[5]; // 1/100th of an inch = 25.4/100 mm
    sensor_values[6] = (rain_intr_cnt_24h * 0.254) + sensor_cf[5]; // 1/100th of an inch = 25.4/100 mm
    //Sensor 7: Solar Radiation
    // Address 1, 1 register
    result = solar_rad_sensor.readHoldingRegisters(0x5, 1);
    if (result == solar_rad_sensor.ku8MBSuccess){
      data = solar_rad_sensor.getResponseBuffer(0);
      sensor_values[7] = (data) + sensor_cf[7];
      sensor_con[7] = true;   
  }
  else {
      sensor_values[7] = -1;
      sensor_con[7] = false;
    }
  }
};
#endif
