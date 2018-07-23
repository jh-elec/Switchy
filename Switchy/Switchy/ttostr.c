
/*************************************************************************************
*				Generate a Time String (12:00:00) or Date String (12.01.2015 Mo)
*	Autor	: Jan Homann
*	Version : 1.0
*	Date	: 26.11.2015
***************************************************************************************/


#include <avr/io.h>
#include "ttostr.h"


/* contains the time string */
char time[11];
char date[16];


char *bcd_ttostr(uint8_t hour, uint8_t min, uint8_t sec)
{
	/* BCD to ASCII */
	time[0] = ' ';
	time[1] = ' ';
	time[2] = ' ';
	time[3] = ' ';
	time[4] = (hour >> 4) + 48;
	time[5] = (hour & 0x0F) + 48;
	time[6] = ':';
	time[7] = (min >> 4) + 48;
	time[8] = (min & 0x0F) + 48;
	time[9] = ':';
	time[10] = (sec >> 4) + 48;
	time[11] = (sec & 0x0F) + 48;
	time[12] = ' ';
	time[13] = ' ';
	time[14] = ' ';
	time[15] = ' ';
	time[16] = '\0';
	
	return time;
}

char *dec_ttostr(uint8_t hour, uint8_t min, uint8_t sec, uint8_t blink_)
{
	/* DEC to ASCII */
	time[0] = ' ';
	time[1]  = ' ';
	time[2]  = ' ';
	time[3] = ' ';

	if (blink_ & 1<<0)
	{
		time[4]  = ((hour / 10)) + 48;
		time[5]  = (hour % 10) + 48;
	}
	else
	{
		time[4] = ' ';
		time[5] = ' ';
	}

	time[6]  = ':';

	if(blink_ & 1<<1)
	{
		time[7]  = ((min / 10)) + 48;
		time[8]  = (min % 10) + 48;
	}
	else
	{
		time[7] = ' ';
		time[8] = ' ';
	}

	time[9]  = ':';

	if (blink_ & 1<<2)
	{
		time[10]  = ((sec / 10)) + 48;
		time[11]  = (sec % 10) + 48;
	}
	else
	{
		time[10] = ' ';
		time[11] = ' ';
	}


	time[12] = ' ';
	time[13] = ' ';
	time[14] = ' ';
	time[15] = ' ';
	time[16] = '\0';
	
	return time;
}

char *bcd_dtostr(uint8_t day, uint8_t month, uint16_t year, char Day_Name)
{
	uint8_t tmp;
	
	/* BCD  to ASCII */
	date[0]		= ' ';
	date[1]		= ' ';
	date[2]		= (day >> 4) + 48; // ten
	date[3]		= (day & 0x0F) + 48; // ones
	date[4]		= '.'; // point
	date[5]		= (month >> 4) + 48; // ten
	date[6]		= (month & 0x0F) + 48; // ones
	date[7]		= '.'; // point
	
	date[8]		= (year / 1000) + 48; // thousand 
	tmp			= year % 1000;
	
	date[9]		= (tmp / 100) + 48; // hounder
	tmp			= tmp % 100;
	
	year -= 2000;
	
	date[10]		=  (tmp >> 4) + 48; // tens
	date[11]		=  (tmp & 0x0F) + 48;
	date[12]	= ' '; // empty 

	switch(Day_Name)
	{
		case 1 :{date[13] = 'S'; date[14] = 'o';}break; // So
		case 2 :{date[13] = 'M'; date[14] = 'o';}break; // Mo
		case 3 :{date[13] = 'D'; date[14] = 'i';}break; // Di
		case 4 :{date[13] = 'M'; date[14] = 'i';}break; // Mi
		case 5 :{date[13] = 'D'; date[14] = 'o';}break; // Do
		case 6 :{date[13] = 'F'; date[14] = 'r';}break; // Fr
		case 7 :{date[13] = 'S'; date[14] = 'a';}break;	// Sa
		
		default :{date[13] = '\0';}break; // string termination
	}
	
	date[15] = ' ';
	date[16] = '\0'; // string termination

	
	
	return date;
}

char *dec_dtostr(uint8_t day, uint8_t month, uint16_t year, char Day_Name, uint8_t blink_)
{
	uint8_t tmp;
	
	date[0] = ' ';
	date[1] = ' ';
	
	
	/* DEC  to ASCII */
	if (blink_ & 1<<0)
	{
			date[2]		= (day / 10) + 48; // ten
			date[3]		= (day % 10) + 48; // ones
	}
	else
	{
			date[2]		= ' '; // ten
			date[3]		= ' '; // ones
	}
	
	date[4]		= '.'; // point
	
	if (blink_ & 1<<1)
	{
		date[5]		= (month / 10) + 48; // ten
		date[6]		= (month % 10) + 48; // ones
	}
	else
	{
		date[5]		= ' '; // ten
		date[6]		= ' '; // ones
	}
	
	date[7]		= '.'; // point
	
	
	date[8]		= (year / 1000) + 48; // thousand
	tmp			= year % 1000;
		
	date[9]		= (tmp / 100) + 48; // hounder
	tmp			= tmp % 100;	
	
	year -= 2000;	
	
	if (blink_ & 1<<2)
	{
		date[10]		=  (tmp / 10) + 48; // tens
		date[11]		=  (tmp % 10) + 48;
	}
	else
	{
		date[10]		=  ' '; // tens
		date[11]		=  ' ';
	}
	

	date[12]	= ' '; // empty

	if (blink_ & 1<<3)
	{
		switch (Day_Name)
		{
			case 1 :{date[13] = 'S'; date[14] = 'o';}break; // So
			case 2 :{date[13] = 'M'; date[14] = 'o';}break; // Mo
			case 3 :{date[13] = 'D'; date[14] = 'i';}break; // Di
			case 4 :{date[13] = 'M'; date[14] = 'i';}break; // Mi
			case 5 :{date[13] = 'D'; date[14] = 'o';}break; // Do
			case 6 :{date[13] = 'F'; date[14] = 'r';}break; // Fr
			case 7 :{date[13] = 'S'; date[14] = 'a';}break;	// Sa			
		}

	}
	else
	{
		date[13] = ' '; 
		date[14] = ' ';
	}
	
	date[15] = ' ';
	date[16] = '\0'; // string termination

	return date;
}

