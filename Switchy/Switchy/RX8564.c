/*
 * Copyright:      Jan Homann
 * Author:         Jan Homann
 * Version:        1.0
 */
  
 
#include <util/delay.h>
#include <avr/interrupt.h>
#include "RX8564.h"
#include "i2cmaster.h"
 
/* here are the score after the functions -> Time_To_ASCII_String or Date_To_ASCII_String */    
char rx8564_time[9]     = {0,0,0,0,0,0,0,0,0};
char rx8564_date[12]    = {0,0,0,0,0,0,0,0,0,0,0,0};

void RX8564_set_Time (uint8_t hour, uint8_t minutes , uint8_t secounds)
{
     
    /* right time format ? else leave the function */
    if ((hour > 24) || (minutes > 59))
    {
        return;
    }
     
    uint8_t rx8564_time[2];
    uint8_t tmp_hour;
    uint8_t tmp_min;
    uint8_t tmp_sec;
         
    /* DEC to BCD */
    rx8564_time[0] = (hour / 10 ) << 4;  // ten
    rx8564_time[1] = (hour % 10 );      // one
    tmp_hour = rx8564_time[0] | rx8564_time[1];
     
    rx8564_time[0] = (minutes / 10 ) << 4;  // ten
    rx8564_time[1] = (minutes % 10 );       // one
    tmp_min = rx8564_time[0] | rx8564_time[1];  
     
    rx8564_time[0] = (secounds / 10 ) << 4;  // ten
    rx8564_time[1] = (secounds % 10 );      // one
    tmp_sec = rx8564_time[0] | rx8564_time[1];
     
    i2c_start_wait(RX8564+I2C_WRITE);
    i2c_write(secounds_register);
    i2c_write(tmp_sec); // secounds
    i2c_write(tmp_min); // minutes
    i2c_write(tmp_hour); // hours
    i2c_stop();
} 
   
void RX8564_set_Date(uint8_t day,uint8_t week_day, uint8_t month, uint16_t year)
{
     
    /* right date format ? else leave the function */
    if ((day > 31) || (day == 0) || (week_day > 6) || (month > 12) || (year < 2015))
    {
        return;
    }
     
    uint8_t     rx8564_date[2];
    uint8_t     tmp_day;
    uint8_t     tmp_month;
    uint8_t     tmp_year;
    uint8_t     tmp_week_day;
     
         
    /* DEC to BCD */
    rx8564_date[0] = (day / 10 ) << 4;    // ten
    rx8564_date[1] = (day % 10 );       // one
    tmp_day = rx8564_date[0] | rx8564_date[1];
         
    rx8564_date[0] = (week_day / 10 ) << 4;   // ten
    rx8564_date[1] = (week_day % 10 );      // one
    tmp_week_day = rx8564_date[0] | rx8564_date[1];
         
    rx8564_date[0] = (month / 10 ) << 4;  // ten
    rx8564_date[1] = (month % 10 );         // one
    tmp_month = rx8564_date[0] | rx8564_date[1];
     
    /* offset */
    tmp_year = year - 2000;
     
    /* DEC to HEX */
    rx8564_date[0] = (tmp_year / 10 ) << 4;   // ten
    rx8564_date[1] = (tmp_year % 10 );      // one
    tmp_year = rx8564_date[0] | rx8564_date[1];
     
     
    i2c_start_wait(RX8564+I2C_WRITE); 
    i2c_write(day_register);
    i2c_write(tmp_day); // day
    i2c_write(tmp_week_day); // name of the day (WeekDays ( Mo - So ))
    i2c_write(tmp_month); // month
    i2c_write(tmp_year); // year                     
    i2c_stop();     
     
}
  
void RX8564_set_Alert(uint8_t day, uint8_t week_day, uint8_t hour, uint8_t minutes)
 {
    uint8_t     rx8564_alert[2];
    uint8_t     tmp_day;
    uint8_t     tmp_week_day;
    uint8_t     tmp_hour;
    uint8_t     tmp_minutes;    
     
    /* DEC to BCD */
    rx8564_alert[0] = (day / 10 ) << 4;   // ten
    rx8564_alert[1] = (day % 10 );      // one
    tmp_day = rx8564_alert[0] | rx8564_alert[1];
     
    rx8564_alert[0] = (week_day / 10 ) << 4;  // ten
    rx8564_alert[1] = (week_day % 10 );     // one
    tmp_week_day = rx8564_alert[0] | rx8564_alert[1];
     
    rx8564_alert[0] = (hour / 10 ) << 4;  // ten
    rx8564_alert[1] = (hour % 10 );         // one
    tmp_hour = rx8564_alert[0] | rx8564_alert[1];
      
    rx8564_alert[0] = (minutes / 10 ) << 4;   // ten
    rx8564_alert[1] = (minutes % 10 );          // one
    tmp_minutes = rx8564_alert[0] | rx8564_alert[1];
     
    /* mask out of the MSB from Alert Sector */
//  tmp_minutes     &= 0x7F;
//  tmp_hour        &= 0x7F;
//  tmp_day         &= 0x7F;
//  tmp_week_day    &= 0x7F;
          
    i2c_start_wait(RX8564+I2C_WRITE);
    i2c_write(minute_alert_register);
    i2c_write(tmp_minutes); // minutes alert
    i2c_write(tmp_hour); // hour alert
    i2c_write(tmp_day); // day alert
    i2c_write(tmp_week_day); // weekday alert
    i2c_stop();  
 }
  
void RX8564_set_CLKOUT(uint8_t frequency)
{    
    i2c_start_wait(RX8564+I2C_WRITE);
    i2c_write(CLKOUT_frequency_register);
    i2c_write(frequency);
    i2c_stop();
}
 
void RX8564_get_Data(RX8564_t *Buffer)          
{   
    sei();
    i2c_start_wait(RX8564+I2C_WRITE);
    i2c_write(0x02);
    i2c_rep_start(RX8564+I2C_READ);
    Buffer->Sec         = i2c_readAck();
    Buffer->Min         = i2c_readAck();   
    Buffer->Std         = i2c_readAck();
    Buffer->Day         = i2c_readAck();
    Buffer->DayN        = i2c_readAck();
    Buffer->Mon         = i2c_readAck();
    Buffer->Yea         = i2c_readAck();
    Buffer->Alrt_Min    = i2c_readAck();
    Buffer->Alrt_Std    = i2c_readAck();
    Buffer->Alrt_Day    = i2c_readAck();
    Buffer->Alrt_Dayn   = i2c_readNak();
    i2c_stop();
 
    Buffer->Sec			&= 0x7F;
    Buffer->Min			&= 0x7F;
    Buffer->Std			&= 0x3F;
          
    Buffer->Day			&= 0x3F;
    Buffer->Mon			&= 0x1F;
    Buffer->DayN		&= 0x07;
     
    /* mask the msb from byte ( Alert Flag! ) */
    Buffer->Alrt_Min	&= 0x7F;
    Buffer->Alrt_Std	&= 0x7F;
    Buffer->Alrt_Day    &= 0x7F;
    Buffer->Alrt_Dayn	&= 0x7F;

	volatile static uint8_t new_hour_old;
     
    /* begin a new hour */
	if ((Buffer->Std != new_hour_old) && (Buffer->Min == 0x00) && (Buffer->Sec <= 0x01))
	{
		new_hour_old = Buffer->Std;
		Buffer->New_Std = 0x01;
	}

	      
}
 
uint8_t RX8564_read_Timer(void)
{
    uint8_t timer_tmp;
     
    i2c_start_wait(RX8564+I2C_WRITE);
    i2c_write(0x0F);
    i2c_stop();
     
    i2c_rep_start(RX8564+I2C_READ); 
    timer_tmp = i2c_readNak();
    i2c_stop();
     
    return timer_tmp;
}
 
void RX8564_set_Control_2(uint8_t mask)
{
    i2c_start_wait(RX8564+I2C_WRITE);
    i2c_write(0x01);
    i2c_write(mask);
    i2c_stop(); 
}
 
void RX8564_set_Timer_Control(uint8_t mask)
{
    i2c_start_wait(RX8564+I2C_WRITE);
    i2c_write(0x0E);
    i2c_write(mask);
    i2c_stop(); 
}