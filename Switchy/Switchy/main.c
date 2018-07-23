/*
 * TimeSwitch.c
 *
 * Created: 13.11.2016 15:58:36
 * Author : Jan Homann
 */ 
#define F_CPU					16e6

#define SAVE_TIME				150
#define FLASH_TIME_DATE_CHAR	50
#define MENUE_TIMEOUT			15
#define DISPLAY_AUTO_OFF		17

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <string.h>
#include <stdlib.h>
#include "lcd.h"
#include "ttostr.h"
#include "i2cmaster.h"
#include "RX8564.h"
#include "hard_def.h"

/*
*	Time for Switch
*/
typedef struct  
{
	uint8_t HoursOn[5];
	uint8_t HoursOff[5];	
	uint8_t MinutesOn[5];
	uint8_t MinutesOff[5];	
	uint8_t SecondsOn[5];
	uint8_t SecondsOff[5];

}SwitchTimeEEP_t;
SwitchTimeEEP_t SwitchTimeEEP EEMEM =
{
	.HoursOn	= {12,12,12,12,12},	
	.HoursOff	= {12,12,12,12,12},
	.MinutesOn	= {5,5,5,5,5},
	.MinutesOff	= {5,5,5,5,5},						
	.SecondsOn	= {0,0,0,0,0},		
	.SecondsOff	= {0,0,0,0,0},
};

uint8_t DispAlwaysOnEEP EEMEM = 0;

/*
*	PWM
*/
typedef struct 
{
	#define LCD_CONTRAST	0
	#define LED_1			1
	#define LED_2			2
	#define LED_3			3
	#define LED_4			4
	#define LCD_BRIGHTNESS  5
	uint8_t Channel[6];
}pwm_t;

volatile static pwm_t Pwm;

/*
*	APP Time Base
*/
typedef struct  
{
	uint16_t Us;
	uint16_t Ms;
	uint8_t Sec;
	uint8_t TmeOutSec;
	uint8_t DispTmeOut;
}userTime_t;
volatile userTime_t Time;

uint16_t FlashX;

/*
*	Encoder
*/
typedef struct
{
	#define ENCODER_RIGHT_DETECTED	1<<0
	#define ENCODER_LEFT_DETECTED	1<<1

	int8_t	Cnt;
}enc_t;
volatile enc_t Encoder;

int8_t EncTable[16] = {0,0,-1,0,0,0,0,1,0,0,0,0,0,0,0,0};

/*
*	Strings for LCD Display
*/
typedef struct
{
	char *PrjName;
	char *SWV;
	char *HWV;
	char *Build;
	char *Author;
	char *SetTime;
	char *SetDate;
	char *SetIllumination;
	char *Time[10];	
	char *SwitchTimeEnable;
	char *ToggleTriac;
	char *ManuelMode;
	char *TriacOn;
	char *TriacOff;	
	char *SubmenueTime[5];
	char *SubmenueManuel[2];
	char *SubmenueExit;
	char *hours;
	char *minutes;
	char *seconds;
	char *TriacState[2];
	char *DispAlwaysOn_Off[3];
	char *SystemInfo;
}AppStrings_t;

AppStrings_t const Strings =
{
	.PrjName			=		"     SwitchY    ",
	.SWV				=		"SwitchY SW. V1.1",
	.HWV				=		"SwitchY HW. V1.1",
	.Build				=		"23.05.2017      ",
	.Author				=		"  J.H Elec.(C)  ",
	.SetTime			=		"-> Set Sys. Time",
	.SetDate			=		"-> Set Sys. Date",
	.SetIllumination	=		"-> Brightness   ",
	.SwitchTimeEnable	=		" *Active Triac* ",
	.ToggleTriac		=		"<- Togg.Triac ->",	
	.ManuelMode			=		"***Manu. Mode***",
	.TriacOn			=		"       On       ",
	.TriacOff			=		"       Off      ",
	.SubmenueExit		=		"      Exit?     ",
	.hours				=		" [hh]"		      ,
	.minutes			=		" [mm]"			  ,
	.seconds			=		" [ss]"			  ,
	.SubmenueTime		=	{
								" -Triac[1] Cnfg.",
								" -Triac[2] Cnfg.",
								" -Triac[3] Cnfg.",
							},
	.SubmenueManuel		=	{	"  Manual On [x] ",
								"  Manual On [ ] ",
							},
	.Time				=	{
								"   Time [On]    ",
								"   Time [Off]   ",
							
								"   Time [On]    ",
								"   Time [Off]   ",
							
								"   Time [On]    ",
								"   Time [Off]   ",
							},
	.TriacState			 =	{
								"->State: Enable ",
								"->State: Disable",
							},

	.DispAlwaysOn_Off	=	{	"Disp. always On ",
								"->   On [x]     ",
								"->   On [ ]     ",
							},
	.SystemInfo			=       "  System Info.  ",
							
};

typedef struct  
{
	char Buffer[16];
	uint8_t HoursOn [5];
	uint8_t HoursOff[5];

	uint8_t MinutesOn [5];
	uint8_t MinutesOff[5];
	
	uint8_t SecondsOn [5];
	uint8_t SecondsOff[5];

	int8_t  Menue;
	uint8_t oldState; 
	uint8_t newState;
	uint8_t EnableTime;
			
	uint8_t TriacSec;

	volatile uint8_t TriacSecComp;

	uint8_t SwitchManuelOn;

	int8_t	SubmenueState;
	
	uint8_t ChangeDisplay;
	
	uint8_t ChangeSystemInfo;

}app_t; app_t APP;

volatile uint16_t	ReadRTC				= 0;
volatile uint8_t	DispAlwaysOn		= 0;
volatile uint16_t	DisplayFade			= 0;
volatile uint8_t	AppState			= 0;

int16_t checkEnc(volatile enc_t *ENC)
{
	volatile static uint8_t Last = 0;

	Last = (Last << 2) & 0x0F;
	if(!(PINC & PHASE_A))	Last |= 0x01;
	if(!(PINC & PHASE_B))	Last |= 0x02;

	ENC->Cnt +=	EncTable[Last];

	return ENC->Cnt;
}

void checkOverflow( uint8_t *Value, int16_t minValue, int16_t maxValue)
{
	int8_t UtoS = *Value;

	if (UtoS > maxValue)
	UtoS = minValue;
	
	if (UtoS < 0)
	UtoS = maxValue;

	*Value = UtoS;
}

uint8_t DecToBcd(uint8_t Value)
{
	uint8_t Buffer[5];
	Buffer[0] = (Value / 10) << 4;
	Buffer[1] = Value % 10;
	
	return Buffer[0] | Buffer[1];
}

uint8_t BcdToDec(uint8_t Value)
{
	uint8_t tmp[2];
	tmp[0] = (Value & 0xF0) >> 4;
	tmp[1] = Value & 0x0F;
	tmp[0] *= 10;
	tmp[0] += tmp[1];
	Value = tmp[0];

	return Value;
}

void setToggleTriacTime(app_t *TriacTime)
{
	/* Seconds */
	while (1)
	{
		TriacTime->TriacSec += checkEnc(&Encoder);
		Encoder.Cnt = 0;

		lcd_gotoxy(0,1);

		APP.Buffer[0]	= '-';
		APP.Buffer[1]	= '>';
		APP.Buffer[2]	= ' ';
		APP.Buffer[3]   = ' ';
		APP.Buffer[4]   = ((APP.TriacSec / 100) % 10)	+ 48;
		APP.Buffer[5]   = ((APP.TriacSec / 10) % 10)	+ 48;
		APP.Buffer[6]   = (APP.TriacSec  % 10)			+ 48;
		APP.Buffer[7]	= ' ';
		APP.Buffer[8]	= '|';
		APP.Buffer[9]	= ' ';
		APP.Buffer[10]	= 'X';
		APP.Buffer[11]	= 'X';
		APP.Buffer[12]	= 'X';
		APP.Buffer[13]	= ' ';
		APP.Buffer[14]	= ' ';
		APP.Buffer[15]	= ' ';
		APP.Buffer[16]	= ' ';
		APP.Buffer[17]	= '\0';		
		lcd_puts(APP.Buffer);

		if (!(PINC & ENTER))
		{
			_delay_ms(SAVE_TIME);
			break;
		}
	}
}

void setSwitchTimeEEP(uint8_t num, uint8_t OnOrOff)
{	
	/* Hours */
	while (1)
	{
		lcd_gotoxy(0,1);

		if (OnOrOff)
		{
			APP.HoursOn[num] += checkEnc(&Encoder);
			Encoder.Cnt = 0;
			checkOverflow(&APP.HoursOn[num],0,23);

			FlashX++;
			if (FlashX<FLASH_TIME_DATE_CHAR)
			{
				lcd_puts(dec_ttostr(APP.HoursOn[num],APP.MinutesOn[num],APP.SecondsOn[num],0b00000111));
			}
			if (FlashX>FLASH_TIME_DATE_CHAR)
			{
				lcd_puts(dec_ttostr(APP.HoursOn[num],APP.MinutesOn[num],APP.SecondsOn[num],0b00000110));
			}
			if (FlashX>FLASH_TIME_DATE_CHAR*2)
			{
				FlashX = 0;
			}
		}
		else
		{
			APP.HoursOff[num] += checkEnc(&Encoder);
			Encoder.Cnt = 0;
			checkOverflow(&APP.HoursOff[num],0,23);
				
			FlashX++;
			if (FlashX<FLASH_TIME_DATE_CHAR)
			{
				lcd_puts(dec_ttostr(APP.HoursOff[num],APP.MinutesOff[num],APP.SecondsOff[num],0b00000111));
			}
			if (FlashX>FLASH_TIME_DATE_CHAR)
			{
				lcd_puts(dec_ttostr(APP.HoursOff[num],APP.MinutesOff[num],APP.SecondsOff[num],0b00000110));			
			}
			if (FlashX>FLASH_TIME_DATE_CHAR*2)
			{
				FlashX = 0;
			}

		}
		lcd_puts(Strings.hours);	
		
		if (!(PINC & ENTER))
		{
			_delay_ms(SAVE_TIME); 
			break;
		}		

	}
	/* Minutes */
	while (1)
	{
		lcd_gotoxy(0,1);

		if (OnOrOff)
		{
			APP.MinutesOn[num] += checkEnc(&Encoder);
			Encoder.Cnt = 0;
			checkOverflow(&APP.MinutesOn[num],0,59);

			FlashX++;
			if (FlashX<FLASH_TIME_DATE_CHAR)
			{
				lcd_puts(dec_ttostr(APP.HoursOn[num],APP.MinutesOn[num],APP.SecondsOn[num],0b00000111));
			}
			if (FlashX>FLASH_TIME_DATE_CHAR)
			{
				lcd_puts(dec_ttostr(APP.HoursOn[num],APP.MinutesOn[num],APP.SecondsOn[num],0b00000101));
			}
			if (FlashX>FLASH_TIME_DATE_CHAR*2)
			{
				FlashX = 0;
			}
		}
		else
		{
			APP.MinutesOff[num] += checkEnc(&Encoder);
			Encoder.Cnt = 0;
			checkOverflow(&APP.MinutesOff[num],0,59);
			
			FlashX++;
			if (FlashX<FLASH_TIME_DATE_CHAR)
			{
				lcd_puts(dec_ttostr(APP.HoursOff[num],APP.MinutesOff[num],APP.SecondsOff[num],0b00000111));
			}
			if (FlashX>FLASH_TIME_DATE_CHAR)
			{
				lcd_puts(dec_ttostr(APP.HoursOff[num],APP.MinutesOff[num],APP.SecondsOff[num],0b00000101));
			}
			if (FlashX>FLASH_TIME_DATE_CHAR*2)
			{
				FlashX = 0;
			}

		}
		lcd_puts(Strings.hours);
		
		if (!(PINC & ENTER))
		{
			_delay_ms(SAVE_TIME);
			break;
		}
	}
	/* Seconds */
	while (1)
	{
		lcd_gotoxy(0,1);

		if (OnOrOff)
		{
			APP.SecondsOn[num] += checkEnc(&Encoder);
			Encoder.Cnt = 0;
			checkOverflow(&APP.SecondsOn[num],0,59);

			FlashX++;
			if (FlashX<FLASH_TIME_DATE_CHAR)
			{
				lcd_puts(dec_ttostr(APP.HoursOn[num],APP.MinutesOn[num],APP.SecondsOn[num],0b00000111));
			}
			if (FlashX>FLASH_TIME_DATE_CHAR)
			{
				lcd_puts(dec_ttostr(APP.HoursOn[num],APP.MinutesOn[num],APP.SecondsOn[num],0b00000011));
			}
			if (FlashX>FLASH_TIME_DATE_CHAR*2)
			{
				FlashX = 0;
			}
		}
		else
		{
			APP.SecondsOff[num] += checkEnc(&Encoder);
			Encoder.Cnt = 0;
			checkOverflow(&APP.SecondsOff[num],0,59);
			
			FlashX++;
			if (FlashX<FLASH_TIME_DATE_CHAR)
			{
				lcd_puts(dec_ttostr(APP.HoursOff[num],APP.MinutesOff[num],APP.SecondsOff[num],0b00000111));
			}
			if (FlashX>FLASH_TIME_DATE_CHAR)
			{
				lcd_puts(dec_ttostr(APP.HoursOff[num],APP.MinutesOff[num],APP.SecondsOff[num],0b00000011));
			}
			if (FlashX>FLASH_TIME_DATE_CHAR*2)
			{
				FlashX = 0;
			}

		}
		lcd_puts(Strings.hours);
		
		if (!(PINC & ENTER))
		{
			_delay_ms(SAVE_TIME);
			break;
		}
	}
	if (OnOrOff)
	{
		eeprom_update_byte(&SwitchTimeEEP.HoursOn[num],APP.HoursOn[num]);
		eeprom_busy_wait();
		eeprom_update_byte(&SwitchTimeEEP.MinutesOn[num],APP.MinutesOn[num]);
		eeprom_busy_wait();
		eeprom_update_byte(&SwitchTimeEEP.SecondsOn[num],APP.SecondsOn[num]);
		eeprom_busy_wait();
	}
	else
	{
		eeprom_update_byte(&SwitchTimeEEP.HoursOff[num],APP.HoursOff[num]);
		eeprom_busy_wait();
		eeprom_update_byte(&SwitchTimeEEP.MinutesOff[num],APP.MinutesOff[num]);
		eeprom_busy_wait();
		eeprom_update_byte(&SwitchTimeEEP.SecondsOff[num],APP.SecondsOff[num]);
		eeprom_busy_wait();
	}

	lcd_gotoxy(0,1);
	lcd_puts("-> Saved!       ");
	_delay_ms(SAVE_TIME);
	
	Time.DispTmeOut = 0;		
	Time.TmeOutSec	= 0;
}

void setAppSysTime(RX8564_t *Time)
{
	RX8564_get_Data(Time);

 	Time->Std = BcdToDec(Time->Std);
 	Time->Min = BcdToDec(Time->Min);
 	Time->Sec = BcdToDec(Time->Sec);

	/* Hours */
	while (1)
	{
		Time->Std += checkEnc(&Encoder);
		Encoder.Cnt = 0;

		checkOverflow(&Time->Std,0,23);
		lcd_gotoxy(0,1);

		FlashX++;
		if (FlashX<FLASH_TIME_DATE_CHAR)
		{
			lcd_puts(dec_ttostr(Time->Std,Time->Min,Time->Sec,0b00000111));
		}
		if (FlashX>FLASH_TIME_DATE_CHAR)
		{
			lcd_puts(dec_ttostr(Time->Std,Time->Min,Time->Sec,0b00000110));
		}
		if (FlashX>FLASH_TIME_DATE_CHAR*2)
		{
			FlashX = 0;
		}

		lcd_puts(Strings.hours);	
		if (!(PINC & ENTER))
		{
			_delay_ms(SAVE_TIME); 
			break;
		}
	}
	
	/* Minutes */
	while (1)
	{
		Time->Min += checkEnc(&Encoder);
		Encoder.Cnt = 0;

		checkOverflow(&Time->Min,0,59);
		lcd_gotoxy(0,1);
		FlashX++;
		if (FlashX<FLASH_TIME_DATE_CHAR)
		{
			lcd_puts(dec_ttostr(Time->Std,Time->Min,Time->Sec,0b00000111));
		}
		if (FlashX>FLASH_TIME_DATE_CHAR)
		{
			lcd_puts(dec_ttostr(Time->Std,Time->Min,Time->Sec,0b00000101));
		}
		if (FlashX>FLASH_TIME_DATE_CHAR*2)
		{
			FlashX = 0;
		}
		
		lcd_puts(Strings.minutes);			
		if (!(PINC & ENTER))
		{
			_delay_ms(SAVE_TIME); 
			break;
		}

	}
	/* Seconds */
	while (1)
	{
		Time->Sec += checkEnc(&Encoder);
		Encoder.Cnt = 0;

		checkOverflow(&Time->Sec,0,59);
		lcd_gotoxy(0,1);
		FlashX++;
		if (FlashX<FLASH_TIME_DATE_CHAR)
		{
			lcd_puts(dec_ttostr(Time->Std,Time->Min,Time->Sec,0b00000111));
		}
		if (FlashX>FLASH_TIME_DATE_CHAR)
		{
			lcd_puts(dec_ttostr(Time->Std,Time->Min,Time->Sec,0b00000011));
		}
		if (FlashX>FLASH_TIME_DATE_CHAR*2)
		{
			FlashX = 0;
		}
		lcd_puts(Strings.seconds);			
		if (!(PINC & ENTER))
		{
			_delay_ms(SAVE_TIME);
			break;
		}

	}
		RX8564_set_Time(Time->Std,Time->Min,Time->Sec);
		
		lcd_gotoxy(0,1);
		lcd_puts("-> Saved!       ");
		_delay_ms(SAVE_TIME);
		
	APP.Menue = 0;
}

void setAppSysDate(RX8564_t *Time)
{
	RX8564_get_Data(Time);

	Time->Day = BcdToDec(Time->Day);
	Time->Mon = BcdToDec(Time->Mon);
	Time->Yea = BcdToDec(Time->Yea);
	
	Time->Yea += 2000;
	
	/* Day */
	while (1)
	{
		Time->Day += checkEnc(&Encoder);
		Encoder.Cnt = 0;

		checkOverflow(&Time->Day,0,31);
		lcd_gotoxy(0,1);

		FlashX++;
		if (FlashX<FLASH_TIME_DATE_CHAR)
		{
			lcd_puts(dec_dtostr(Time->Day,Time->Mon,Time->Yea,Time->DayN,0b00001111));
		}
		if (FlashX>FLASH_TIME_DATE_CHAR)
		{
			lcd_puts(dec_dtostr(Time->Day,Time->Mon,Time->Yea,Time->DayN,0b00001110));
		}
		if (FlashX>FLASH_TIME_DATE_CHAR*2)
		{
			FlashX = 0;
		}
		
		if (!(PINC & ENTER))
		{
			_delay_ms(SAVE_TIME);
			break;
		}
	}
	/* Month */
	while (1)
	{
		Time->Mon += checkEnc(&Encoder);
		Encoder.Cnt = 0;

		checkOverflow(&Time->Mon,0,12);
		lcd_gotoxy(0,1);
		
		FlashX++;
		if (FlashX<FLASH_TIME_DATE_CHAR)
		{
			lcd_puts(dec_dtostr(Time->Day,Time->Mon,Time->Yea,Time->DayN,0b00001111));
		}
		if (FlashX>FLASH_TIME_DATE_CHAR)
		{
			lcd_puts(dec_dtostr(Time->Day,Time->Mon,Time->Yea,Time->DayN,0b00001101));
		}
		if (FlashX>FLASH_TIME_DATE_CHAR*2)
		{
			FlashX = 0;
		}
		
		if (!(PINC & ENTER))
		{
			_delay_ms(SAVE_TIME);
			break;
		}
	}
	/* Year */
	while (1)
	{	  
		Time->Yea += checkEnc(&Encoder);
 		Encoder.Cnt = 0;
		
		if (Time->Yea < 2017)
		{
			Time->Yea = 2017;
		}
		if (Time->Yea > 2099)
		{
			Time->Yea = 2099;
		}
		
		lcd_gotoxy(0,1);
		
		FlashX++;
		if (FlashX<FLASH_TIME_DATE_CHAR)
		{
			lcd_puts(dec_dtostr(Time->Day,Time->Mon,Time->Yea,Time->DayN,0b00001111));
		}
		if (FlashX>FLASH_TIME_DATE_CHAR)
		{
			lcd_puts(dec_dtostr(Time->Day,Time->Mon,Time->Yea,Time->DayN,0b00001011));
		}
		if (FlashX>FLASH_TIME_DATE_CHAR*2)
		{
			FlashX = 0;
		}
	
		if (!(PINC & ENTER))
		{
			_delay_ms(SAVE_TIME);
			break;
		}
	}
	/* Day Name */
	while (1)
	{
		Time->DayN += checkEnc(&Encoder);
		Encoder.Cnt = 0;

		checkOverflow(&Time->DayN,0,7);
		lcd_gotoxy(0,1);

		FlashX++;
		if (FlashX<FLASH_TIME_DATE_CHAR)
		{
			lcd_puts(dec_dtostr(Time->Day,Time->Mon,Time->Yea,Time->DayN,0b00001111));
		}
		if (FlashX>FLASH_TIME_DATE_CHAR)
		{
			lcd_puts(dec_dtostr(Time->Day,Time->Mon,Time->Yea,Time->DayN,0b00000111));
		}
		if (FlashX>FLASH_TIME_DATE_CHAR*2)
		{
			FlashX = 0;
		}

		if (!(PINC & ENTER))
		{
			_delay_ms(250);
			break;
		}
	}
		RX8564_set_Date(Time->Day,Time->DayN,Time->Mon,Time->Yea);
		
		lcd_gotoxy(0,1);
		lcd_puts("-> Saved!       ");
		_delay_ms(SAVE_TIME);
		
		APP.Menue = 0;
}

uint8_t CompareSwitchTime(app_t *SwitchTime, RX8564_t *SysTime)
{
	static uint8_t SwitchEnableOn = 0;

	for (uint8_t x = 0 ; x < 3 ; x++)
	{
		if (DecToBcd(SwitchTime->HoursOn[x]) == (SysTime->Std))
		{
			if (DecToBcd(SwitchTime->MinutesOn[x]) == (SysTime->Min))
			{
				if (DecToBcd(SwitchTime->SecondsOn[x]) ==  (SysTime->Sec))
				{
					SwitchEnableOn |= (1<<x);
				}
			}
		}
	}

	for (uint8_t x = 0 ; x < 3 ; x++)
	{
		if (DecToBcd(SwitchTime->HoursOff[x]) == (SysTime->Std))
		{
			if (DecToBcd(SwitchTime->MinutesOff[x]) == (SysTime->Min))
			{
				if (DecToBcd(SwitchTime->SecondsOff[x]) ==  (SysTime->Sec))
				{
					SwitchEnableOn &= ~(1<<x);
				}
			}
		}
	}
	
	return SwitchEnableOn;
}

uint8_t CompareSwitchTime_(app_t *SwitchTime)
{
	uint8_t SwitchEnableOn = 0;
		
	for (uint8_t x = 0 ; x < 3 ; x++)
	{
		if ((SwitchTime->HoursOn[x]) == (SwitchTime->HoursOff[x]))
		{
			if ((SwitchTime->MinutesOn[x]) == (SwitchTime->MinutesOff[x]))
			{
				if ((SwitchTime->SecondsOn[x]) ==  (SwitchTime->SecondsOff[x]))
				{
					SwitchEnableOn |= (1<<x);
				}
			}
		}
	}
	
	return SwitchEnableOn;
}

RX8564_t RX8564_Buffer;

void checkSwitchTime(void)
{
	APP.EnableTime = CompareSwitchTime(&APP,&RX8564_Buffer);
	
	if (APP.EnableTime & 1<<0 || APP.SwitchManuelOn & 1<<5)
	{
		Pwm.Channel[LED_2]	= 5;
		TRIAC_PORT &= ~(TRIAC1);
	}
	else
	{
		Pwm.Channel[LED_2] = 0;
		TRIAC_PORT |= (TRIAC1);
	}
	if(APP.EnableTime & 1<<1 || APP.SwitchManuelOn & 1<<6)
	{
		Pwm.Channel[LED_3]	= 5;
		TRIAC_PORT &= ~(TRIAC2);
	}
	else
	{
		Pwm.Channel[LED_3] = 0;
		TRIAC_PORT |= (TRIAC2);
	}
	if (APP.EnableTime & 1<<2 || APP.SwitchManuelOn & 1<<7)
	{
		Pwm.Channel[LED_4]	= 5;
		TRIAC_PORT &= ~(TRIAC3);
	}
	else
	{
		Pwm.Channel[LED_4] = 0;
		TRIAC_PORT |= (TRIAC3);
	}
}

void lcdMenue(int8_t *MenuePoint, RX8564_t *RTC)
{
	switch(*MenuePoint)
	{
		case 0:
		{
			lcd_gotoxy(0,0);
			if (APP.ChangeDisplay <= 5)
			lcd_puts(bcd_dtostr(RTC->Day,RTC->Mon,RTC->Yea+2000,RTC->DayN));
			if (APP.ChangeDisplay >= 10 && APP.ChangeDisplay <= 11)
			lcd_puts(Strings.SWV);
			if (APP.ChangeDisplay >= 12 && APP.ChangeDisplay <= 13)
			lcd_puts(Strings.HWV);
			if (APP.ChangeDisplay >= 14)
			APP.ChangeDisplay = 0;
		
			lcd_gotoxy(0,1);
			lcd_puts(bcd_ttostr(RTC->Std,RTC->Min,RTC->Sec));
		}break;
		case 1:
		{
			lcd_gotoxy(0,0);
			lcd_puts(Strings.PrjName);
			lcd_gotoxy(0,1);
			lcd_puts(Strings.SetTime);
			if (!(ENCODER_PORT & ENTER))
			{
				_delay_ms(250);
				setAppSysTime(RTC);
				*MenuePoint = 0;
			}
		}break;
		case 2:
		{
			lcd_gotoxy(0,0);
			lcd_puts(Strings.PrjName);
			lcd_gotoxy(0,1);
			lcd_puts(Strings.SetDate);
			if (!(ENCODER_PORT & ENTER))
			{
				_delay_ms(250);
				setAppSysDate(RTC);
			}
			*MenuePoint = 2;
		}break;
		case 3:
		{
			lcd_gotoxy(0,0);
			lcd_puts(Strings.PrjName);
			lcd_gotoxy(0,1);
			lcd_puts(Strings.SubmenueTime[0]);
			if (!(PINC & ENTER))
			{
				_delay_ms(250);
				
				APP.SubmenueState = 1;
				Time.TmeOutSec = 0;
				while(1)
				{
					APP.SubmenueState += checkEnc(&Encoder);
					Encoder.Cnt = 0;

					if (APP.SubmenueState > 5)
					{
						APP.SubmenueState = 5;
					}
					if (APP.SubmenueState < 1)
					{
						APP.SubmenueState = 1;
					}
					
					lcd_gotoxy(0,1);
					if (APP.SubmenueState == 1)
					{
						lcd_puts(Strings.Time[0]);
						if (!(PINC & ENTER))
						{
							_delay_ms(250);
							setSwitchTimeEEP(0,1);
							Time.TmeOutSec = 0;
						}
					}
					if (APP.SubmenueState == 2)
					{
						lcd_puts(Strings.Time[1]);
						if (!(PINC & ENTER))
						{
							_delay_ms(250);
							setSwitchTimeEEP(0,0);
							Time.TmeOutSec = 0;
						}
					}

					if (APP.SubmenueState == 3)
					{
						if (!(PINC & ENTER))
						{
							_delay_ms(150);
							lcd_gotoxy(0,1);
							APP.SwitchManuelOn ^= 1<<5;		
							checkSwitchTime();
						}
						if (APP.SwitchManuelOn & 1<<5)
						{
							lcd_puts(Strings.SubmenueManuel[0]);
						}
						else
						{
							lcd_puts(Strings.SubmenueManuel[1]);
						}
					}
					if (APP.SubmenueState == 4)
					{
						lcd_gotoxy(0,1);
						
						if (CompareSwitchTime_(&APP) & 1<<0)
						{
							lcd_puts(Strings.TriacState[1]);
						}
						else
						{
							lcd_puts(Strings.TriacState[0]);
						}
					}					
					
					if (APP.SubmenueState == 5)
					{
						lcd_puts(Strings.SubmenueExit);
						if (!(PINC & ENTER))
						{
							_delay_ms(250);
							break;
						}
					}

					if (Time.TmeOutSec >= MENUE_TIMEOUT)
					{
						break;
					}
				}
			}	
		}break;
		case 4:
		{
			lcd_gotoxy(0,0);
			lcd_puts(Strings.PrjName);
			lcd_gotoxy(0,1);
			lcd_puts(Strings.SubmenueTime[1]);
			if (!(PINC & ENTER))
			{
				_delay_ms(250);
				
				APP.SubmenueState = 1;
				Time.TmeOutSec = 0;
				while(1)
				{
					
					APP.SubmenueState += checkEnc(&Encoder);
					Encoder.Cnt = 0;

					if (APP.SubmenueState > 5)
					{
						APP.SubmenueState = 5;
					}
					if (APP.SubmenueState < 1)
					{
						APP.SubmenueState = 1;
					}
					
					lcd_gotoxy(0,1);
					if (APP.SubmenueState == 1)
					{
						lcd_puts(Strings.Time[2]);
						if (!(PINC & ENTER))
						{
							_delay_ms(250);
							setSwitchTimeEEP(1,1);
							Time.TmeOutSec = 0;
						}
					}
					if (APP.SubmenueState == 2)
					{
						lcd_puts(Strings.Time[3]);
						if (!(PINC & ENTER))
						{
							_delay_ms(250);
							setSwitchTimeEEP(1,0);
							Time.TmeOutSec = 0;
						}
					}
					if (APP.SubmenueState == 3)
					{
						if (!(PINC & ENTER))
						{
							_delay_ms(150);
							lcd_gotoxy(0,1);
							APP.SwitchManuelOn ^= 1<<6;
							checkSwitchTime();
						}
						if (APP.SwitchManuelOn & 1<<6)
						{
							lcd_puts(Strings.SubmenueManuel[0]);
						}
						else
						{
							lcd_puts(Strings.SubmenueManuel[1]);
						}
					}
					if (APP.SubmenueState == 4)
					{
						lcd_gotoxy(0,1);
						
						if (CompareSwitchTime_(&APP) & 1<<1)
						{
							lcd_puts(Strings.TriacState[1]);
						}
						else
						{
							lcd_puts(Strings.TriacState[0]);
						}
					}
					if (APP.SubmenueState == 5)
					{
						lcd_puts(Strings.SubmenueExit);
						if (!(PINC & ENTER))
						{
							_delay_ms(250);
							break;
						}
					}
					if (Time.TmeOutSec >= MENUE_TIMEOUT)
					{
						break;
					}
				}
			}
		}break;
		case 5:
		{
			lcd_gotoxy(0,0);
			lcd_puts(Strings.PrjName);
			lcd_gotoxy(0,1);
			lcd_puts(Strings.SubmenueTime[2]);
			if (!(PINC & ENTER))
			{
				_delay_ms(250);
				
				APP.SubmenueState = 1;
				Time.TmeOutSec = 0;
				while(1)
				{
					
					APP.SubmenueState += checkEnc(&Encoder);
					Encoder.Cnt = 0;

					if (APP.SubmenueState > 5)
					{
						APP.SubmenueState = 5;
					}
					if (APP.SubmenueState < 1)
					{
						APP.SubmenueState = 1;
					}
					
					lcd_gotoxy(0,1);
					if (APP.SubmenueState == 1)
					{
						lcd_puts(Strings.Time[4]);
						if (!(PINC & ENTER))
						{
							_delay_ms(250);
							setSwitchTimeEEP(2,1);
							Time.TmeOutSec = 0;
						}
					}
					if (APP.SubmenueState == 2)
					{
						lcd_puts(Strings.Time[5]);
						if (!(PINC & ENTER))
						{
							_delay_ms(250);
							setSwitchTimeEEP(2,0);
							Time.TmeOutSec = 0;
						}
					}
					if (APP.SubmenueState == 3)
					{
						if (!(PINC & ENTER))
						{
							_delay_ms(150);
							lcd_gotoxy(0,1);
							APP.SwitchManuelOn ^= 1<<7;
							checkSwitchTime();
						}
						if (APP.SwitchManuelOn & 1<<7)
						{
							lcd_puts(Strings.SubmenueManuel[0]);
						}
						else
						{
							lcd_puts(Strings.SubmenueManuel[1]);
						}
					}
					if (APP.SubmenueState == 4)
					{
						lcd_gotoxy(0,1);
						
						if (CompareSwitchTime_(&APP) & 1<<2)
						{
							lcd_puts(Strings.TriacState[1]);
						}
						else
						{
							lcd_puts(Strings.TriacState[0]);
						}
					}
					if (APP.SubmenueState == 5)
					{
						lcd_puts(Strings.SubmenueExit);
						if (!(PINC & ENTER))
						{
							_delay_ms(250);
							break;
						}
					}
					if (Time.TmeOutSec >= MENUE_TIMEOUT)
					{
						break;
					}
				}
			}
		}break;
		case 6:
		{
			lcd_gotoxy(0,0);
			lcd_puts(Strings.SwitchTimeEnable);
			lcd_gotoxy(0,1);
			
			/* Show Enable Time (which Channel is Enabled by Time) */
			APP.EnableTime |= APP.SwitchManuelOn;
			
			APP.Buffer[0]	= '-';
			APP.Buffer[1]	= '>';
			APP.Buffer[2]	= 'T';
			APP.Buffer[3]	= 'r';
			APP.Buffer[4]	= 'i';
			APP.Buffer[5]	= 'a';
			APP.Buffer[6]	= 'c';
			
		    APP.Buffer[7]	= '[';
			if (APP.EnableTime & 1<<0 || APP.EnableTime & 1<<5)
			{
				APP.Buffer[8]	= '1';
			}
			else
			{
				APP.Buffer[8]	= '-';
			}
			APP.Buffer[9]	= ']';
			
			APP.Buffer[10]	= '[';
			if (APP.EnableTime & 1<<1 || APP.EnableTime & 1<<6)
			{
				APP.Buffer[11]	= '2';
			}
			else
			{
				APP.Buffer[11]	= '-';
			}
			APP.Buffer[12]	= ']';
			
			
			APP.Buffer[13]	= '[';
			if (APP.EnableTime & 1<<3 || APP.EnableTime & 1<<7)
			{
				APP.Buffer[14]	= '3';
			}
			else
			{
				APP.Buffer[14]	= '-';
			}
			APP.Buffer[15]	= ']';
			APP.Buffer[16]	= '\0';
			
			lcd_puts(APP.Buffer);
		}break;
		case 7:
		{
			APP.ChangeSystemInfo=0;
			lcd_gotoxy(0,0);
			lcd_puts(Strings.DispAlwaysOn_Off[0]);
			lcd_gotoxy(0,1);
			if (!(ENCODER_PORT & ENTER))
			{
				_delay_ms(250);
				DispAlwaysOn ^= 1<<0;
				eeprom_update_byte(&DispAlwaysOnEEP,DispAlwaysOn);
			}
			
			if (DispAlwaysOn & 1<<0) // display display is always on
			{
				lcd_puts(Strings.DispAlwaysOn_Off[1]);
			}			
			if (!(DispAlwaysOn & 1<<0)) // display display is always off
			{
				lcd_puts(Strings.DispAlwaysOn_Off[2]);
			}
		}break;
		case 8:
		{
			lcd_gotoxy(0,0);
			lcd_puts(Strings.SystemInfo);
			lcd_gotoxy(0,1);
			if (APP.ChangeSystemInfo<2)
			{
				lcd_puts(Strings.SWV);
			}
			if (APP.ChangeSystemInfo>3)
			{
				lcd_puts(Strings.HWV);
			}
			if (APP.ChangeSystemInfo>6)
			{
				APP.ChangeSystemInfo=0;
			}
		}break;
	}	
}

void reloadEEP(void)
{
	/* Reload SwitchTime from EEP */
	for (uint8_t x = 0 ; x < 3 ; x++)
	{
		APP.HoursOn[x]	= eeprom_read_byte(&SwitchTimeEEP.HoursOn[x]);
		eeprom_busy_wait();
		APP.HoursOff[x]	= eeprom_read_byte(&SwitchTimeEEP.HoursOff[x]);
		eeprom_busy_wait();
		
		APP.MinutesOn[x]	= eeprom_read_byte(&SwitchTimeEEP.MinutesOn[x]);
		eeprom_busy_wait();
		APP.MinutesOff[x]	= eeprom_read_byte(&SwitchTimeEEP.MinutesOff[x]);
		eeprom_busy_wait();
		
		APP.SecondsOn[x]	= eeprom_read_byte(&SwitchTimeEEP.SecondsOn[x]);
		eeprom_busy_wait();
		APP.SecondsOff[x]	= eeprom_read_byte(&SwitchTimeEEP.SecondsOff[x]);
	}
	DispAlwaysOn = eeprom_read_byte(&DispAlwaysOnEEP);	
}

void initHardware(void)
{
	/* LED´s */
	LED_DDR		|= ((STATE_LED1) | (STATE_LED2) | (STATE_LED3) | (STATE_LED4));
	
	/* LCD - Contrast */
	LCD_DDR		|= (CONTRAST);
	LCD_DDR		|= (ILLUMINATION);
	
	/* Encoder */
	ENCODER_DDR  &= ~((PHASE_A) | (PHASE_B));
	//ENCODER_PORT |=  ((PHASE_A) | (PHASE_B));
	
	DDRC  &= ~ENTER;
	PORTC |= (ENTER);

	/* Triac */
	TRIAC_DDR		|= (TRIAC1 | TRIAC2 | TRIAC3);

	/* Timer 0 Init - PWM */
	TCCR0 |= ((1<<CS00));				
	TCNT0  = 0x00;						
	TIMSK |= (1<<TOIE0);				

	/* Timer 1 Init - Sys. Time Base */
	TCCR1B |= ((1<<CS11));				
	TCCR1B |= (1<<WGM12);				
	OCR1A   = 0x63;						
	TIMSK  |= (1<<OCIE1A);		
}

void scrollMessage(int8_t x, uint8_t y, const char *s)
{
	uint8_t col = 0;
	
	
	if (y == 1)
	lcd_gotoxy(0, 1);  // Zweite Zeile
	else
	lcd_gotoxy(0, 1);  // Erste Zeile
	
	
	// Leerzeichen vor dem String
	if (x>=0)
	{
		// Leerzeichen vor dem String
		while((col < x) && (col < (16)))
		{
			lcd_putc(' ');
			col++;
			
		}
		
		// Der String
		while ((*s) && (col < (16)))
		{
			lcd_putc(*s++);
			col++;
		}
		
		// Leerzeichen nach dem String
		while (col < (16))
		{
			lcd_putc(' ');
			col++;
		}
		
		
		} else {
		
		
		if ((x+strlen(s)) <= 0)
		{
			// String links, nur Leerzeichen ausgeben
			while (col < (16))
			{
				lcd_putc(' ');
				col++;
			}
			} else {
			// Der String
			uint8_t i = abs(x);
			while ((s[i]) && (col < (16)))
			{
				lcd_putc(s[i++]);
				col++;
			}
			// Leerzeichen nach dem String
			while (col < (16))
			{
				lcd_putc(' ');
				col++;
			}
			
		}
		
	}
	
}


void Welcome(void)
{
	Pwm.Channel[LED_2] = 255;
	Pwm.Channel[LED_3] = 255;
	Pwm.Channel[LED_4] = 255;
	
	lcd_gotoxy(0,0);
	lcd_puts("Welcome to..    ");
	lcd_gotoxy(0,1);
	lcd_puts(Strings.SWV);
	
	while(Pwm.Channel[LED_2]>0)
	{
		if (Pwm.Channel[LED_2]>0)
		{
			Pwm.Channel[LED_2]--;
			Pwm.Channel[LED_3]--;
			Pwm.Channel[LED_4]--;			
		}
		_delay_ms(2);
	}
	
	lcd_gotoxy(0,0);
	lcd_puts("This is..       ");
	lcd_gotoxy(0,1);
	lcd_puts(Strings.HWV);

	while(Pwm.Channel[LED_2]<255)
	{
		if (Pwm.Channel[LED_2]<255)
		{
			Pwm.Channel[LED_2]++;
			//Pwm.Channel[LED_3]--;
			//Pwm.Channel[LED_4]--;
		}
		_delay_ms(2);
	}
	
	lcd_gotoxy(0,0);
	lcd_puts("Build @         ");
	lcd_gotoxy(0,1);
	lcd_puts(Strings.Build);
	
	while(Pwm.Channel[LED_3]<255)
	{
		if (Pwm.Channel[LED_3]<255)
		{
			//Pwm.Channel[LED_2]++;
			Pwm.Channel[LED_3]++;
			//Pwm.Channel[LED_4]--;
		}
		_delay_ms(2);
	}

	lcd_gotoxy(0,0);
	lcd_puts("Build by        ");
	lcd_gotoxy(0,1);
	lcd_puts(Strings.Author);
	
	while(Pwm.Channel[LED_4]<255)
	{
		if (Pwm.Channel[LED_4]<255)
		{
			//Pwm.Channel[LED_2]++;
			//Pwm.Channel[LED_3]++;
			Pwm.Channel[LED_4]++;
		}		
		_delay_ms(2);
	}
	
	lcd_gotoxy(0,0);
	lcd_puts("Contact -> Jan.");
	lcd_gotoxy(0,1);
	lcd_puts("Homann@yahoo.de");
	
	_delay_ms(1500);
	
	lcd_gotoxy(0,0);
	lcd_puts("Start Now..     ");
	lcd_gotoxy(0,1);
	lcd_puts("Enjoy it!       ");	
	
	while(Pwm.Channel[LED_4]>0)
	{
		if (Pwm.Channel[LED_4]>0)
		{
			Pwm.Channel[LED_2]--;
			Pwm.Channel[LED_3]--;
			Pwm.Channel[LED_4]--;
		}
		_delay_ms(2);
	}
}

uint8_t LCD_PowerUp =0;

int main(void)
{	
	/* init hardware */
	initHardware();
	
	/* reload settings */
	reloadEEP();

	/* lcd init */
	lcd_init(LCD_DISP_ON);

	/* init I2C */
	i2c_init();

	/* set 1Hz output */
	RX8564_set_CLKOUT(F_1Hz);

	/* interrupt enable */
	sei();

	/* default pwm settings */
	Pwm.Channel[LCD_CONTRAST]	= 35;
	Pwm.Channel[LCD_BRIGHTNESS] = 100;
	Pwm.Channel[LED_1]			= 0;
	Pwm.Channel[LED_2]			= 0;
	Pwm.Channel[LED_3]			= 0;
	Pwm.Channel[LED_4]			= 0;

	/* read first time from rtc */
	RX8564_get_Data(&RX8564_Buffer);
	
	/* show welcome message */
	Welcome();

    while (1) 
    {	
		/* Auto Exit from x MenuePoint */
		if (Time.TmeOutSec >= MENUE_TIMEOUT){

			Time.TmeOutSec = 0;
			APP.Menue = 0 ;
		}
		
		if (Time.DispTmeOut >= DISPLAY_AUTO_OFF && (!(DispAlwaysOn & 1<<0)) && DisplayFade >= 10){

			DisplayFade = 0;
			if (Pwm.Channel[LCD_BRIGHTNESS] > 0){
				Pwm.Channel[LCD_BRIGHTNESS]--;
			}

		}

		if (Encoder.Cnt){
		
			LCD_PowerUp = 0xA5;
		}
		if ((Pwm.Channel[LCD_BRIGHTNESS] < 100) && (DisplayFade >= 10) && LCD_PowerUp == 0xA5){
					
			DisplayFade = 0;
			Pwm.Channel[LCD_BRIGHTNESS]++;
		}
		if (Pwm.Channel[LCD_BRIGHTNESS]>=100){
			LCD_PowerUp=0;
		}
			
		/* New MenuePoint? Clear AutoExit Counter */
		APP.newState = APP.Menue;
		if (APP.oldState != APP.newState){
			
			Time.TmeOutSec				= 0;
			Time.DispTmeOut				= 0;
			APP.oldState = APP.newState;
		}
		
		/* Press "Enter" and Reset Light_Auto_Off */
		if(!(PINC & ENTER)){

			Time.TmeOutSec				= 0;
			Time.DispTmeOut				= 0;
			Pwm.Channel[LCD_BRIGHTNESS] = 100;
		}
	
		/* Refresh MenuePoint */
		APP.Menue += checkEnc(&Encoder);
		Encoder.Cnt = 0;

		if (ReadRTC >= 250){
			
			ReadRTC = 0;
			RX8564_get_Data(&RX8564_Buffer);
			CompareSwitchTime(&APP,&RX8564_Buffer);
		}
		
		/* check menue overflow */
		if (APP.Menue>8)
		APP.Menue=8;
		if (APP.Menue<=0)
		APP.Menue=0;
		
		lcdMenue(&APP.Menue,&RX8564_Buffer);
		checkSwitchTime();
	}	
}

ISR(TIMER0_OVF_vect)
{	
	static uint8_t pwm_cnt = 0;

	if (pwm_cnt >= 100)
	pwm_cnt = 0;
	else
	pwm_cnt++;

	if (pwm_cnt >= Pwm.Channel[0])
	LCD_PORT &= ~(CONTRAST);	
	else
	LCD_PORT |= CONTRAST;

	if (pwm_cnt >= Pwm.Channel[LED_1])
	LED_PORT	|=  (STATE_LED1);
	else
	LED_PORT	&=   ~(STATE_LED1);

	if (pwm_cnt >= Pwm.Channel[LED_2])
	LED_PORT	|=  (STATE_LED2);
	else
	LED_PORT	&=  ~(STATE_LED2);
	
	if (pwm_cnt >= Pwm.Channel[LED_3])
	LED_PORT	|=  (STATE_LED3);
	else
	LED_PORT	&=  ~(STATE_LED3);

	if (pwm_cnt >= Pwm.Channel[LED_4])
	LED_PORT	|=   (STATE_LED4);
	else
	LED_PORT	&=  ~(STATE_LED4);

	if (pwm_cnt >= Pwm.Channel[LCD_BRIGHTNESS])
	LCD_PORT	&=  ~(ILLUMINATION);
	else
	LCD_PORT	|=   (ILLUMINATION);	
	
	TCNT0 = 0xA0;
}

ISR(TIMER1_COMPA_vect)
{
	#define HEARTBEAT_BRIGHT	15
	#define HEARTBEAT_ZYKLUS	1500
	
	Time.Us++;
	static uint16_t Heartbeat = 0;
	Heartbeat++;
	if (Heartbeat < HEARTBEAT_ZYKLUS)
	{
		Pwm.Channel[LED_1] = HEARTBEAT_BRIGHT;
	}
	if (Heartbeat > HEARTBEAT_ZYKLUS)
	{
		Pwm.Channel[LED_1] = 0;
	}
	if(Heartbeat > (HEARTBEAT_ZYKLUS*2) && Heartbeat < (HEARTBEAT_ZYKLUS*3))
	{
		Pwm.Channel[LED_1] = HEARTBEAT_BRIGHT;
	}
	if (Heartbeat > (HEARTBEAT_ZYKLUS*3) && Heartbeat < (HEARTBEAT_ZYKLUS*4))
	{
		Pwm.Channel[LED_1] = 0;
	}
	if (Heartbeat > (HEARTBEAT_ZYKLUS*4) && Heartbeat < (HEARTBEAT_ZYKLUS*5))
	{
		Pwm.Channel[LED_1] = HEARTBEAT_BRIGHT;
	}
	if (Heartbeat > ((uint16_t)HEARTBEAT_ZYKLUS*42))
	{
		Heartbeat = 0;
	}
	
	if (Time.Us >= 10)
	{
		Time.Us = 0;
		Time.Ms++;
		ReadRTC++;
		DisplayFade++;
		checkEnc(&Encoder);
	}
	if (Time.Ms >= 1000)
	{
		APP.ChangeSystemInfo++;
		Time.Ms = 0;
		Time.Sec++;
		Time.TmeOutSec++;
		Time.DispTmeOut++;
		APP.TriacSecComp++;
		APP.ChangeDisplay++;
	}
	
	if (Time.Sec > 59)
	Time.Sec = 0;
}