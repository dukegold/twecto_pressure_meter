#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H
#include <U8g2lib.h>
#include <I2C_eeprom.h>
#include "time_keeping.h"
#include "Sys_Variables.h"
enum lcd_choices{
  SET_DATE = 1,
  SET_TIME,
  SET_LOG_CYCLE,
  ABOUT,
  CONFIG
};
  

uint8_t lcd_float_input(u8g2_t *u8g2, const char *title, const char *pre, float *value, float lo, float hi, float interval, uint8_t digits, const char *post);
bool update_display(U8G2_ST7920_128X64_F_SW_SPI *lcd, uint8_t screen, float *sensor_values, bool *sensor_con, bool rtc_found, bool wifi_con);

bool update_display(U8G2_ST7920_128X64_F_SW_SPI *lcd, uint8_t screen, float *sensor_values, bool *sensor_con, bool rtc_found, bool wifi_con){
  char str_rep[64];
  lcd->clearBuffer();
  /****** Default screen *****/
  lcd->setFont(u8g2_font_5x7_mf);
  getDateFullc(str_rep);
  if(rtc_found) lcd->drawStr(0,8,str_rep);
  else lcd->drawStr(0,8,"Rtc failure");
  if(wifi_con)
    lcd->drawStr(98,8,"wifi");
  lcd->drawStr(20,63,"For Menu press ENT");
  lcd->drawLine(0,10,127,10);
  lcd->setFont(u8g2_font_t0_11_mf);  
  //Draw Sensor value
  for(int k = 0; k < SENSOR_PER_SCREEN; k++){
    int sens_num = (SENSOR_PER_SCREEN*screen)+k;
    if(sens_num < NUM_SENSORS){
    //dtostrf(spm_val,6,2,str_rep);
    if(sensor_con[sens_num])
      sprintf(str_rep,"%s: %.2f %s",sensor_names[sens_num], sensor_values[sens_num], sensor_units[sens_num]);
    else
      sprintf(str_rep,"%s: NC",sensor_names[sens_num]);
    
    lcd->drawStr(7,25+k*10,str_rep);
  }
    
  }
  //lcd->drawStr(6,40,"Log cycle");
  //lcd->drawStr(6,52,log_intvl_str[log_choice-1]);
  lcd->sendBuffer();
  return true;
}

void menuOptions(U8G2_ST7920_128X64_F_SW_SPI *lcd, uint8_t *log_choice, float *sensor_cf, I2C_eeprom *eeprom, bool *log_choice_update, bool *loop_exit_update) {
  boolean loopExit = false;
  char str_rep[64];
  lcd->clear();
  //Wait for button release
  while(ENTER);

  do {
    const char *str = 
      "Set Date\n"
      "Set Time\n"
      "Set log cycle\n"
      "About\n"
      "Configuration";
    uint8_t choice = 0;
    choice = lcd->userInterfaceSelectionList(
                                            "menu",
                                            choice, 
                                            str);
    switch (choice) {
    case SET_DATE: {
      uint8_t day_r,month_r,year_r;
      byte second = 0,minute = 0,hour = 0,weekday = 0, monthday = 0,month = 0, year = 0;
      readTime(&second,&minute,&hour,&weekday, &day_r,&month_r,&year_r);
      lcd->userInterfaceInputValue("Set Day", "", &day_r, 1, 31, 2, "");
      lcd->userInterfaceInputValue("Set Month", "", &month_r, 1, 12, 2, "");
      lcd->userInterfaceInputValue("Set year", "2000+", &year_r, 0, 99, 2, "");
      readTime(&second,&minute,&hour,&weekday, &monthday,&month, &year);
      setRTCTime(second, minute, hour, 0, day_r, month_r, year_r);   
      lcd->userInterfaceMessage("Set_Date", "Done","","ok");
      break;
    }
    case SET_TIME: {
      uint8_t minute_r,hour_r;
      byte second = 0,minute = 0,hour = 0,weekday = 0, monthday = 0,month = 0, year = 0;
      readTime(&second, &minute_r, &hour_r,&weekday, &monthday,&month, &year);
      lcd->userInterfaceInputValue("Set Hour", "", &hour_r, 0, 23, 2, "");
      lcd->userInterfaceInputValue("Set Minute", "", &minute_r, 0, 59, 2, "");
      readTime(&second,&minute,&hour,&weekday, &monthday,&month, &year);
      setRTCTime(second, minute_r, hour_r, 0, monthday, month, year);
      lcd->userInterfaceMessage("Set Time", "Done","","ok");
      break;
    }
    case SET_LOG_CYCLE: {
      const char *log_intvl_str =
        "1  Hr\n"
        "2  Hr\n"
        "4  Hr\n"
        "6  Hr\n" 
        "12 Hr\n"
        "24 Hr\n"
        "2  min\n"
        "5  min\n"
        "10 min\n"
        "15 min";
      uint8_t _log_choice;
      _log_choice = lcd->userInterfaceSelectionList(
                                                   "Log Interval",
                                                   *log_choice, 
                                                   log_intvl_str);
      if (_log_choice) {
        *log_choice = _log_choice;
        eeprom->writeByte(EEP_LOG_CHOICE,*log_choice);
        *log_choice_update = true;
        lcd->userInterfaceMessage("Log Interval", "Updated","","ok");
      } 
      else lcd->userInterfaceMessage("Log Interval", "Not Updated","","ok");
      break;
    }
    case ABOUT: {
      lcd->setFont(u8g2_font_5x7_mf);
      sprintf(str_rep,"Device ID: %s\nSerial No:  %s\n,IP:%s",DEVICE_ID,DEV_SER,WiFi.localIP().toString().c_str());
      lcd->userInterfaceMessage("WMS Recorder",
			       COMPANY_TITLE,
			       str_rep,
			       "ok");
      lcd->setFont(u8g2_font_t0_11_mf);
      break;
    }
    case CONFIG: {
      uint16_t code = 0;
      uint8_t digit = 0;
      lcd->userInterfaceInputValue("Enter Code 1st digit", "", &digit, 0, 9, 1, "");
      code = digit*1000;
      lcd->userInterfaceInputValue("Enter Code 2nd digit", "", &digit, 0, 9, 1, "");
      code += digit*100;
      lcd->userInterfaceInputValue("Enter Code 3rd digit", "", &digit, 0, 9, 1, "");
      code += digit*10;
      lcd->userInterfaceInputValue("Enter Code 4th digit", "", &digit, 0, 9, 1, "");
      code += digit;
      if(code == AUTH_PASSCODE){
        for(int x = 0; x < NUM_SENSORS; x++){
          lcd_float_input(lcd->getU8g2(),sensor_names[x],"CF: ",&sensor_cf[x],-1000,1000,0.01,8,"");
          eeprom->writeBlock(EEP_SN0_CF+(x<<2),(uint8_t*) &sensor_cf[x],4);
        }
      }
      else{
        lcd->userInterfaceMessage("Incorrect Code", "Try again","","ok");
      }
      break;
    }  
    case 0: {
      *loop_exit_update = true;
      loopExit = true;
    }
      break;
    }
    if (loopExit) {
      lcd->clearBuffer();
      break;
    }
  } while (1);
}

uint8_t lcd_float_input(u8g2_t *u8g2, const char *title, const char *pre, float *value, float lo, float hi, float interval, uint8_t digits, const char *post)
{
  uint8_t line_height;
  uint8_t height;
  u8g2_uint_t pixel_height;
  u8g2_uint_t  y, yy;
  u8g2_uint_t  pixel_width;
  u8g2_uint_t  x, xx;
  
  float local_value = *value;
  //uint8_t r; /* not used ??? */
  uint8_t event;

  /* only horizontal strings are supported, so force this here */
  u8g2_SetFontDirection(u8g2, 0);

  /* force baseline position */
  u8g2_SetFontPosBaseline(u8g2);
  
  /* calculate line height */
  line_height = u8g2_GetAscent(u8g2);
  line_height -= u8g2_GetDescent(u8g2);
  
  
  /* calculate overall height of the input value box */
  height = 1;	/* value input line */
  height += u8x8_GetStringLineCnt(title);

  /* calculate the height in pixel */
  pixel_height = height;
  pixel_height *= line_height;


  /* calculate offset from top */
  y = 0;
  if ( pixel_height < u8g2_GetDisplayHeight(u8g2)  )
    {
      y = u8g2_GetDisplayHeight(u8g2);
      y -= pixel_height;
      y /= 2;
    }
  
  /* calculate offset from left for the label */
  x = 0;
  pixel_width = u8g2_GetUTF8Width(u8g2, pre);
  pixel_width += u8g2_GetUTF8Width(u8g2, "0") * digits;
  pixel_width += u8g2_GetUTF8Width(u8g2, post);
  if ( pixel_width < u8g2_GetDisplayWidth(u8g2) )
    {
      x = u8g2_GetDisplayWidth(u8g2);
      x -= pixel_width;
      x /= 2;
    }
  
  /* event loop */
  for(;;)
    {
      u8g2_FirstPage(u8g2);
      do
        {
          /* render */
          yy = y;
          yy += u8g2_DrawUTF8Lines(u8g2, 0, yy, u8g2_GetDisplayWidth(u8g2), line_height, title);
          xx = x;
          xx += u8g2_DrawUTF8(u8g2, xx, yy, pre);
          char buf[10];
          dtostrf(local_value,6,2,buf);
          xx += u8g2_DrawUTF8(u8g2, xx, yy, buf);
          u8g2_DrawUTF8(u8g2, xx, yy, post);
        } while( u8g2_NextPage(u8g2) );
    
#ifdef U8G2_REF_MAN_PIC
      return 0;
#endif
    
      for(;;)
        {
          event = u8x8_GetMenuEvent(u8g2_GetU8x8(u8g2));
          if ( event == U8X8_MSG_GPIO_MENU_SELECT )
            {
              *value = local_value;
              return 1;
            }
          else if ( event == U8X8_MSG_GPIO_MENU_HOME )
            {
              return 0;
            }
          else if ( event == U8X8_MSG_GPIO_MENU_NEXT || event == U8X8_MSG_GPIO_MENU_UP )
            {
              if ( local_value >= hi )
                local_value = lo;
              else
                local_value += interval;
              break;
            }
          else if ( event == U8X8_MSG_GPIO_MENU_PREV || event == U8X8_MSG_GPIO_MENU_DOWN )
            {
              if ( local_value <= lo )
                local_value = hi;
              else
                local_value -= interval;
              break;
            }        
        }
    }
  
  /* never reached */
  //return r;  
}
#endif
