/*
 * main.c
 *
 * Created: 26/05/2022 2:26:14 AM
 *  Author: Kirollos
 */ 

#define F_CPU 1000000
#define RDY
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

volatile float analog_reading;
volatile unsigned int digital_reading;
volatile  float tempC;
volatile  float tempF;
volatile char tempmode = 0; // 0 - C, 1 - F

volatile float d7seg_val = 0;
volatile unsigned char d7seg_mux = 0;

volatile char rdy = 0;

volatile int ADC_Counter = 0; // Delay counter for the ADC process

unsigned char d7seg_map[] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71 , /*r*/0b01010000, /*y*/ 0b01101110};

int main(void)
{
	OSCCAL = 0xFF; // [HARDWARE] Calibration required by our ATMega328p internal RC oscillator
	DDRB = 0xFF; // 7-segment out pins
	DDRD = 0xF0; // 7-segment enable pins [D7 - D4]
	
	// 7 segment time muliplexing timer
	
	TCCR0A |= 1 << WGM01; // CTC mode
	
	TCCR0B |= 3; // Prescalar = 64
	//OCR0A = 157; // 10.048 ms                                OCR0A = required delay*clk freq/prescalar
	OCR0A = 100;   // Reduced to 100 to overcome the inaccurate frequency provided by the internal RC oscillator
	
	TIMSK0 |= 1 << OCIE0A; // Timer0 interrupt enable
	sei(); // Global Interrupt enable
	
	// -------------------------------
	
	// ADC Registers
	
	ADMUX = 0x00; // ADC0
	ADCSRA |= (1 << ADEN) | (1 << ADATE) | (1 << ADIE); // ADC Enable, Auto Trigger Enable, Interrupt Enable
	ADCSRA |= (1 << ADPS2) ; // Prescalar = 16
	ADCSRB = 0x00; // free running
	
	// -------------------------------
	
	// External interrupts
	DDRD &= ~( (1<<2) | (1<<3) ); // D2 and D3 as input ports
	PORTD |= ( (1<<2) | (1<<3) ); // pull-up
	EIMSK |= (1<<INT0) | (1<<INT1); // Enable both external interrupts
	EICRA = 0b1010; // Trigger on falling edge on both interrupts
	
	#ifdef RDY
	
	// Init phase::  Quick indication that the Microcontroller is functioning properly
	char rdy_counter = 5;
	while(!rdy)
	{
		if(!rdy_counter) // write rdy on the 7-segment
		{
			d7seg_val = 0xfefe;
			rdy = 1;
			_delay_ms(1500);
		}
		d7seg_val = rdy_counter--;
		_delay_ms(1000);
	}
	
	#endif
	
	ADCSRA |= 1 << ADSC; // Start ADC Conversion
	
    while(1)
    {
		
    }
}

ISR(INT0_vect) // INT0 - Celsius mode
{
	tempmode = 0;
}

ISR(INT1_vect) // INT1 - Fahrenheit mode
{
	tempmode = 1;
}

ISR(TIMER0_COMPA_vect) // 7-segment time multiplexer
{
	unsigned char v = 0;
	if(d7seg_val >= 100)
	{
		unsigned char x = (char)d7seg_val;
		switch(d7seg_mux)
		{
			case 0:
				//C or F
				if(tempmode)
					v = 0xF;
				else
					v = 0xC;
				break;
			case 1:
				v = x%10;
				break;
			case 2:
				v = (x%100)/10;
				break;
			case 3:
				v = x/100;
				break;
		}
	}
	else
	{
		unsigned char x = (char)d7seg_val;
		switch(d7seg_mux) // 7seg:  6  9  .4  C
		{// let x = 69.4 ,, 
			case 0:
			//C or F
			if(tempmode)
				v = 0xF;
			else
				v = 0xC;
			break;
			case 1:
			v = (d7seg_val - x)*10;
			break;
			case 2:
			v = (x%10);
			break;
			case 3:
			v = (x/10);
			break;
		}
	}
	#ifdef RDY
	if(d7seg_val == 0xfefe) // rdy-process
	{
		switch(d7seg_mux)
		{
			case 0:
			v = 0x11; // y
			break;
			case 1:
			v = 0xd; // d
			break;
			case 2:
			v = 0x10; // r
			break;
			case 3:
			v = 0;  // blank
			break;
		}
	}
	#endif
	PORTD |= 0xF0; // Disable all 7-segments (common cathode)
	PORTD &= ~(1 << (4+d7seg_mux)); // Enable the specific common cathode
	if(d7seg_mux==3 && v == 0) // If the most significant digit is zero, disable that 7-segment
		PORTD |= 0xF0;         // 
	PORTB = d7seg_map[v];
	if(d7seg_val < 100 && d7seg_mux==2)	//
		PORTB += 0x80;					// decimal point [PORT B7] enabled if the temperature value is less than 100 degrees (C or F)
	
	if((++d7seg_mux) == 4) // Recycle the mux
		d7seg_mux = 0;     //
}

ISR(ADC_vect) // ADC Interrupt
{
	digital_reading = ADCW;
	if(++ADC_Counter < 1023) return; // delay timer for ADC to take different readings every specific interval
	ADC_Counter = 0;
	analog_reading = (digital_reading*500.0)/1023; // (ADC/1023) * 5v * 100.0 (sensitivity)
	if(analog_reading >= 100.0)
		analog_reading = (unsigned char) analog_reading;
	
	tempC = (float)(analog_reading); // 10mV/C
	tempF = (tempC*9/5) + 32;// (°C × 9/5) + 32 = °F
	//ADCSRA |= (1<<ADSC);
	
	if(tempmode)
	{	// F
		d7seg_val = tempF;
	}
	else
	{
		d7seg_val = tempC;
	}
}