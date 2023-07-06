/*
 * main.c
 *
 * Created: 26/05/2022 2:26:14 AM
 *  Author: Kirollos
 */ 

#define F_CPU 1000000
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

volatile float analog_reading;
volatile unsigned int digital_reading;
volatile  float tempC;
volatile  float tempF;
volatile char tempmode = 0; // 0 - C, 1 - F

//volatile char d7seg_val = 0;
volatile float d7seg_val = 0;
volatile unsigned char d7seg_mux = 0;

volatile char rdy = 0;

volatile char dot_counter = 0;

volatile unsigned char ADC_Counter = 0;

unsigned char d7seg_map[] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71 , /*r*/0b01010000, /*y*/ 0b01101110};

int main(void)
{
	OSCCAL = 0xFF; // [HARDWARE] Calibration required by our ATMega328p internal RC oscillator
	DDRB = 0xFF; // 7-segment out pins
	DDRD = 0xF0; // 7-segment enable pins [D7 - D4]
	
	TCCR0A |= 1 << WGM01; // CTC mode
	//#if F_CPU == 1000000
	// F_CPU = 1M
	TCCR0B |= 3; // Prescalar = 64
	//OCR0A = 157; // 10.048 ms                                OCR0A = required delay*clk freq/prescalar
	OCR0A = 100;
	/*#elif F_CPU == 8000000
	// F_CPU = 8M
	TCCR0B |= 5; // Prescalar = 1024
	OCR0A = 79; // 10.112 ms // 78.125->10ms
	#endif*/
	
	TIMSK0 |= 1 << OCIE0A; // Timer0 interrupt
	sei();
	
	ADMUX = 0x00; // ADC0
	//ADMUX = 1 << REFS0;
	ADCSRA |= (1 << ADEN) /*| (1 << ADATE) | (1 << ADIE)*/ ;//| (1 << ADPS2) | (1 << ADPS0) ; // ADC Enable + Prescaler = 32 //16
	ADCSRA |= (1 << ADATE) | (1 << ADIE);
	//#if F_CPU == 1000000
	// F_CPU = 1M
	//ADCSRA |= (1 << ADPS2) | (1 << ADPS0); // 32
	ADCSRA |= (1 << ADPS2) ;//| (1 << ADPS1) | (0 << ADPS0); // 128
	/*#elif F_CPU == 8000000
	// F_CPU = 8M
	ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // 128
	#endif*/
	ADCSRB = 0x00; // free running
	
	// External interrupts
	DDRD &= ~( (1<<2) | (1<<3) ); // D2 and D3 as input ports
	PORTD |= ( (1<<2) | (1<<3) ); // pull-up
	EIMSK |= (1<<INT0) | (1<<INT1); // Enable both external interrupts
	EICRA = 0b1010; // Trigger on falling edge on both interrupts
	
	// Init phase::
	char rdy_counter = 5;
	while(!rdy)
	{
		if(!rdy_counter)
		{
			d7seg_val = 0xfefe;
			rdy = 1;
			_delay_ms(1500);
		}
		d7seg_val = rdy_counter--;
		_delay_ms(1000);
	}
	//rdy=1;
	
	ADCSRA |= 1 << ADSC;
	
    while(1)
    {
		/*ADCSRA |= (1<<ADSC); // Start conversion
		while(ADCSRA & (1<<ADSC)); // Wait for conversion
		
        digital_reading = ADCW;
        analog_reading = (digital_reading*500.0)/1023; // (ADC/1023) * 5v * 100.0 (sensitivity)
		if(analog_reading >= 100.0)
			analog_reading = (unsigned char) analog_reading;
        //tempC = digital_reading;
        tempC = (float)(analog_reading); // 10mV/C
        tempF = (tempC*9/5) + 32;// (°C × 9/5) + 32 = °F
        //ADCSRA |= (1<<ADSC);
		
		if(tempmode)
		{
			// F
			d7seg_val = tempF;
		}
		else
		{
			d7seg_val = tempC;
		}
		_delay_ms(300);*/
    }
}

ISR(INT0_vect)
{
	tempmode = 0;
}

ISR(INT1_vect)
{
	tempmode = 1;
}

ISR(TIMER0_COMPA_vect)
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
	if(d7seg_val == 0xfefe) // rdy
	{
		switch(d7seg_mux)
		{
			case 0:
			v = 0x11;
			break;
			case 1:
			v = 0xd;
			break;
			case 2:
			v = 0x10;
			break;
			case 3:
			v = 0;
			break;
		}
	}
	PORTD |= 0xF0; // Disable all 7-segments (common cathode)
	PORTD &= ~(1 << (4+d7seg_mux)); // Enable the specific common cathode
	if(d7seg_mux==3 && v == 0) // If the most significant digit is zero, disable that 7-segment
		PORTD |= 0xF0;         // 
	PORTB = d7seg_map[v];
	if(d7seg_val < 100 && d7seg_mux==2)	//
		PORTB += 0x80;					// decimal point
	
	// Least Significant 7-segment:: Create a blinking decimal point every 1 second
	if(++dot_counter >= (100) && d7seg_mux==0)
	{
		PORTB += 0x80;
	}
	if(++dot_counter >= (200))
	{
		dot_counter = 0;
	}
	// ----
	if((++d7seg_mux) == 4)
		d7seg_mux = 0;
}

/*ISR(ADC_vect)
{
	digital_reading = ADCW;
	analog_reading = (digital_reading*500)/1023;
	//tempC = digital_reading;
	tempC = (float)(analog_reading); // 10mV/C
	tempF = (tempC*9/5) + 32;// (°C × 9/5) + 32 = °F
	//ADCSRA |= (1<<ADSC);
}*/

ISR(ADC_vect)
{
	digital_reading = ADCW;
	if(++ADC_Counter < 255) return;
	analog_reading = (digital_reading*500.0)/1023; // (ADC/1023) * 5v * 100.0 (sensitivity)
	if(analog_reading >= 100.0)
	analog_reading = (unsigned char) analog_reading;
	//tempC = digital_reading;
	tempC = (float)(analog_reading); // 10mV/C
	tempF = (tempC*9/5) + 32;// (°C × 9/5) + 32 = °F
	//ADCSRA |= (1<<ADSC);
	
	if(tempmode)
	{
		// F
		d7seg_val = tempF;
	}
	else
	{
		d7seg_val = tempC;
	}
}