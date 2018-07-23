/*
 * Copyright:      Jan Homann
 * Author:         Jan Homann
 * Version:        1.1 (Update to Pointer & Struct)
 */

#ifndef F_CPU
#define F_CPU 8000000
#endif

/* Header´s */ 
#include <inttypes.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

/* Slave Address */
#define RX8564 0xA2 

/* RX8564_set_CLKOUT Frequency */
#define F_32768Hz 0x80
#define F_1024Hz  0x81
#define F_32Hz	  0x82
#define F_1Hz	  0x83
#define F_STOP	  0x00


/* register addresses */
#define control_1_register			0x00
#define control_2_register			0x01
#define secounds_register			0x02
#define minutes_register			0x03
#define hours_register				0x04
#define day_register				0x05
#define day_name_register			0x06
#define month_Register				0x07
#define year_Register				0x08
#define minute_alert_register		0x09
#define hour_alert_register			0x0A
#define day_alert_register			0x0B
#define day_Name_alert_register		0x0C
#define	CLKOUT_frequency_register	0x0D
#define timer_control_register		0x0E
#define timer_register				0x0F



 typedef struct
 {
	 uint8_t	Sec;
	 uint8_t	Min;
	 uint8_t	Std;
	 uint8_t	Mon;
	 uint8_t	Day;
	 uint8_t	DayN;
	 uint16_t	Yea;
	 
	 uint8_t	Alrt_Min;
	 uint8_t	Alrt_Std;
	 uint8_t	Alrt_Day;
	 uint8_t	Alrt_Dayn;
	 
	 uint8_t	New_Std;
	 
 }RX8564_t;


/***Prototype functions***/
	
/* send the time to the RTC (Dec to BCD) */
/* Parameter :
				hour	 = z.B 07
				minutes	 = z.B 54
				secounds = z.B 00
*/
void RX8564_set_Time (uint8_t hour, uint8_t minutes , uint8_t secounds);

/* send the date to the RTC (Dec to BCD) */
/* Parameter :
				day			= z.B 16
				weak_day	= z.B 0 ( for Sunday. please look at the function )
				month		= z.B 02
				year		= z.B 2015
*/
void RX8564_set_Date(uint8_t day,uint8_t weak_day, uint8_t month, uint16_t year);

/* send the alert parameter to the RTC (Dec to BCD) */
/* Parameter :
				day			= z.B 16
				weak_day	= z.B 0 ( for Sunday. please look at the function )
				hour		= z.B 02
				minutes		= z.B 2015
*/
void RX8564_set_Alert(uint8_t day, uint8_t week_day, uint8_t hour, uint8_t minutes);

/* configure the CLKOUT pin from the RX8564 */
/* Parameter :
				frequency	=	F_32768Hz 
								F_1024Hz  
								F_32Hz	  
								F_1Hz	  
*/
void RX8564_set_CLKOUT(uint8_t frequency);

/* read the time & date from the RX8564 */
/* Parameter : RX8564_t struct type
*/
void RX8564_get_Data(RX8564_t *Buffer);


/* read the Timer Count from the RX8564 */
/* Parameter : none

	returns the Timer stand
*/
uint8_t RX8564_read_Timer(void);

void RX8564_set_Timer_Control(uint8_t mask);

void RX8564_set_Control_2(uint8_t mask);