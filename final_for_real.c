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
volatile uint8_t output_buffer[8];
volatile int8_t output_buffer_status[8] = {IDLE,IDLE,IDLE,IDLE,IDLE,IDLE,IDLE,IDLE};
uint8_t next_buffer = 0;
uint8_t output_bitpattern = 0;
uint8_t bitmasks[8] = {0b00000001, 0b00000010, 0b00000100, 0b00001000, 0b00010000, 0b00100000, 0b01000000, 0b10000000};
uint8_t freqCache[64];

// Receive variables
volatile int8_t input_buffer[IN_BUFFER_SIZE];
volatile uint8_t input_ready = 0;
int8_t resample_buffer_sin[7];
int8_t resample_buffer_cos[7];
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
	input_buffer[input_buffer_pos] = ADCH - 128;
	//UDR0 = ADCH;
	ADCSRA |= _BV(ADSC);
	public_input_buffer_position = input_buffer_pos;
	input_ready |= ((input_buffer_pos & (NOTIFY_FREQ * 2 - 1)) == NOTIFY_FREQ);
	input_buffer_pos = (input_buffer_pos+1) & (IN_BUFFER_SIZE - 1);
	if(output_sample_num == 0) {
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
			else if(bitmasks[7-output_buffer_status[i]] & output_buffer[i]) {
				output_bitpattern |= bitmasks[i];
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
	uint16_t analyze_output;
	int16_t analyze_temp;
	uint8_t start = 0, detected=0;
	init();
	while(1) {
		if(UCSR0A & _BV(RXC0)) {	// We've received a serial byte
			while(output_buffer_status[next_buffer] != IDLE) {;}
			output_buffer[next_buffer] = UDR0;
			output_buffer_status[next_buffer] = START_BIT;
			UDR0 = output_buffer[next_buffer];	
			//next_buffer = (next_buffer + 1) & 0x7;
		}
		if(input_ready) {
			if(fix2int(input_buffer_resample_position + float2fix(10./4.)) < public_input_buffer_position){// || fix2int(input_buffer_resample_position + float2fix(10./4.)) - public_input_buffer_position > IN_BUFFER_SIZE / 2) {

				analyze_temp = sin_acc >> 2;
				analyze_output = analyze_temp * analyze_temp;
				analyze_temp = cos_acc >> 2;
				analyze_output += (analyze_temp * analyze_temp);

				if(analyze_output > 50) {
					detected = 1;
					sin_acc = cos_acc = 0;
					memset(resample_buffer_sin, 0, sizeof(uint8_t) * 7);
					memset(resample_buffer_cos, 0, sizeof(uint8_t) * 7);
					if(start==0) {
						start = 8;
						resample_buffer_position = 7;
						//input_buffer_resample_position += int2fix(4);
					}
				}

				if(resample_buffer_position == 7) {
					resample_buffer_position = 0;
					input_buffer_resample_position += float2fix(4.);
					if(detected && start > 0) {
						--start;
						UDR0='1';
					}
					else if(start > 0) {
						--start;
						UDR0='0';
					}
					detected = 0;
				}
				sin_acc -= resample_buffer_sin[resample_buffer_position];
				input_sample = input_buffer[fix2int(input_buffer_resample_position)];
				sin_acc += input_sample;
				resample_buffer_sin[resample_buffer_position] = input_sample;

				cos_acc -= resample_buffer_cos[resample_buffer_position];
				input_sample = input_buffer[fix2int(input_buffer_resample_position + float2fix(10./4.))];
				cos_acc += input_sample;
				resample_buffer_cos[resample_buffer_position] = input_sample;

				++resample_buffer_position;
				input_buffer_resample_position += float2fix(10.);
			}
			else  {
				input_ready = 0;
			}
		}
	}
}

void init() {
	DDRC = 0xff;
	DDRD = _BV(PB7) | _BV(PB2) | _BV(PB1) | _BV(PB0);

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
	//UCSR0A = _BV(U2X0);

	UBRR0 = 520;	// Set baud rate to 2400

	sei();
	set_sleep_mode(SLEEP_MODE_IDLE);
	sleep_enable();
}
