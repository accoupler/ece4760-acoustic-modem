/*******************************************
**********	  Acoustic Modem 	************
*******************************************/

// Standard headers	
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <math.h>
#include <util/delay.h>
#include <string.h>

// Project Definitions
#include "sin_vals.h"

// 6:10 fixed point math defines
#define int2fix(a) (((uint16_t)(a))<<10)
#define fix2int(a) ((uint8_t)((a)>>10))
#define float2fix(a) ((uint16_t)((a)*1024.0))
#define fix2float(a) ((float)(a)/1024.0)

// Buffer status defines
#define IDLE -2
#define START_BIT -1
#define STOP_BIT 8

// Decoding defines
#define IN_BUFFER_SIZE 64
#define NOTIFY_FREQ 16

// Send variables
uint8_t output_sample_num = 0;
uint8_t output_buffer[8] = {0,0,0,0,0,0,0,0};
volatile int8_t output_buffer_status[8] = {IDLE,IDLE,IDLE,IDLE,IDLE,IDLE,IDLE,IDLE};
uint8_t next_buffer = 0;
uint8_t output_bitpattern = 0;
uint8_t bitmasks[8] = {0b00000001, 0b00000010, 0b00000100, 0b00001000, 0b00010000, 0b00100000, 0b01000000, 0b10000000};
uint8_t freqCache[64];

const uint16_t f4000_increment = float2fix(10.);
const uint16_t f4000_quarterphase = float2fix(2.5);

// Receive variables
volatile int8_t input_buffer[IN_BUFFER_SIZE];
volatile uint8_t input_ready = 0;
int8_t resample_buffer_sin[7] = {0,0,0,0,0,0,0};
int8_t resample_buffer_cos[7] = {0,0,0,0,0,0,0};
uint8_t resample_buffer_position = 0;
int16_t sin_acc = 0, cos_acc = 0;
uint16_t input_buffer_resample_position = 0;
uint8_t input_buffer_pos = 0;
volatile uint8_t public_input_buffer_position = 0;
void init();

ISR(TIMER1_COMPA_vect) {
	uint8_t i;
	PORTD &= ~_BV(PB7);
	PORTC = freqCache[output_sample_num];
	output_sample_num = (output_sample_num + 1) & 0x3f;
	PORTD |= _BV(PB7);	
	input_buffer[input_buffer_pos] = ADCH - 0x7F;
	//UDR0 = input_buffer[input_buffer_pos];
	ADCSRA |= _BV(ADSC);
	public_input_buffer_position = input_buffer_pos;
	//input_ready |= ((input_buffer_pos & (NOTIFY_FREQ * 2 - 1)) == NOTIFY_FREQ);
	input_buffer_pos = (input_buffer_pos+1) & (IN_BUFFER_SIZE - 1);
	if(output_sample_num == 0) {
		PORTD ^= _BV(PB5);	// Toggle this so we know our burst period
		output_bitpattern = 0;
		for(i=0; i<8; ++i) {
			if(output_buffer_status[i] == IDLE) {continue;}
			else if(output_buffer_status[i] == START_BIT) {
				output_bitpattern |= bitmasks[i];
			}
			else if(output_buffer_status[i] == STOP_BIT) {
				output_buffer_status[i] = IDLE;
				continue;
			}
			else if (output_buffer[i] & 0x80) {
				output_bitpattern |= bitmasks[i];
			}
			if (output_buffer_status[i] != START_BIT) {
				output_buffer[i] = output_buffer[i] << 1;
			}
			++output_buffer_status[i];
		}
		memcpy_P(freqCache, freqTable[output_bitpattern], sizeof(uint8_t) * 64);
	}
}

ISR(TIMER1_COMPB_vect, ISR_NAKED) {
	//PORTD ^= (1 << PB2);
	sei();
	sleep_cpu();
	reti();
}

int main() {
	int8_t input_sample;
	int32_t analyze_output;
	int16_t analyze_temp;
	uint8_t start = 0, detected=0;
	uint8_t x =0, i=0;
	init();
	while(1) {
		if(UCSR0A & _BV(RXC0)) {	// We've received a serial byte
			while(output_buffer_status[next_buffer] != IDLE) {;}
			output_buffer[next_buffer] = UDR0;
			output_buffer_status[next_buffer] = START_BIT;
			//UDR0 = output_buffer[next_buffer];	
			//next_buffer = (next_buffer + 1) & 0x7;
		}
		//if(input_ready) {
			x = fix2int(input_buffer_resample_position + f4000_quarterphase);
			if((x <= public_input_buffer_position && public_input_buffer_position - x <= 10) ||
			   (x > public_input_buffer_position && x - public_input_buffer_position > 10)) {
				//UDR0 = public_input_buffer_position;
				sin_acc -= resample_buffer_sin[resample_buffer_position];
				input_sample = input_buffer[fix2int(input_buffer_resample_position)];
				sin_acc += input_sample;
				resample_buffer_sin[resample_buffer_position] = input_sample;
				//UDR0 = input_sample;
				//UDR0 = sin_acc;

				cos_acc -= resample_buffer_cos[resample_buffer_position];
				input_sample = input_buffer[fix2int(input_buffer_resample_position + f4000_quarterphase)];
				cos_acc += input_sample;
				resample_buffer_cos[resample_buffer_position] = input_sample;
				//UDR0 = input_sample;
				//UDR0 = cos_acc;

				analyze_temp = sin_acc;// >> 2;
				//UDR0 = analyze_temp;
				analyze_output = analyze_temp * analyze_temp;
				analyze_temp = cos_acc;// >> 2;
				//UDR0 = analyze_temp;
				analyze_output += (analyze_temp * analyze_temp);

				if(analyze_output > 6400) {
					detected = 1;
					sin_acc = cos_acc = 0;
					for(i=0; i<7; ++i) {
						resample_buffer_sin[i] = 0;
						resample_buffer_cos[i] = 0;
					}
					if(start==0) {
						start = 10;
						resample_buffer_position = 6;
					}
				}

				if(resample_buffer_position++ == 6) {
					resample_buffer_position = 0;
					//input_buffer_resample_position -= float2fix(4.);
					if (start > 0) {
						sin_acc = cos_acc = 0;
					for(i=0; i<7; ++i) {
						resample_buffer_sin[i] = 0;
						resample_buffer_cos[i] = 0;
					}
					}
					if (start > 0) {
					//	UDR0 = analyze_output >> 8;
						//UDR0 = analyze_output;
					}
					
					if(detected && start > 0) {
						--start;
						UDR0='1';
						input_buffer_resample_position += int2fix(4);
					}
					else if(start > 0) {
						--start;
						input_buffer_resample_position += int2fix(4);
						UDR0='0';
					}
					detected = 0;
				}

				input_buffer_resample_position += f4000_increment;
			}
			/*else  {
				input_ready = 0;
			}
		}*/
	}
}

void init() {
	DDRC = 0xff;
	DDRD = _BV(PB7) | _BV(PB5) | _BV(PB2) | _BV(PB1) | _BV(PB0);

	/* Setup sampling timer */
	TCCR1B = _BV(WGM12) | _BV(CS10);	// Clear on timer compare, no prescaling
	OCR1A = 500;				// Fire every 25us = 40000Hz (our sampling frequency)
	OCR1B = 480;
	TIMSK1 = _BV(OCIE1A) | _BV(OCIE1B);		// Enable output compare A interrupt

	/* Setup AD converter */
	ADMUX = _BV(REFS0) | _BV(ADLAR);
	ADCSRA = _BV(ADEN) | _BV(ADSC) | 6;

	/* Setup UART */
	UCSR0B = _BV(RXEN0) | _BV(TXEN0);	// Enable receive and transmit
	UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);		// Set 8-bit frames
	UCSR0A = _BV(U2X0);

	UBRR0 = 10;	// Set baud rate to 2400

	//memset(resample_buffer_sin, 0, sizeof(int8_t) * 7);
	//memset(resample_buffer_cos, 0, sizeof(int8_t) * 7);

	//memset(output_buffer, 0, sizeof(uint8_t) * 8);

	sei();
	set_sleep_mode(SLEEP_MODE_IDLE);
	sleep_enable();
}