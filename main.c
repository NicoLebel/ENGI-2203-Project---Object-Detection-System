#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <stdlib.h>
#include "USART0.h"

#define SERVO1_PIN PIND4 // D11
#define SERVO2_PIN PINB4  // D12
#define TOP 40000
#define SERVO_SPEED_1 410
#define SERVO_SPEED_2 360
#define TOP_1 110
#define TOP_0 220
#define SINE_TABLE_SIZE 64

float dutycycle = 7.5;
volatile int x = 7;
volatile unsigned int answer[8], answer_index = 0;
volatile unsigned int count0 = 0, count1 = 0, count2_1 = 0, count2_0 = 0, previous = 0, current = 0, test_count = 0, sendbit = 0, value = 0;
volatile uint8_t count = 0, number = 0b11001010;
volatile uint16_t servo1_ticks = 150; // 1.5ms = center (0 degrees)
volatile uint16_t servo2_ticks = 150;
volatile uint16_t timer_ticks = 0;
volatile uint8_t current_servo = 0;
volatile uint8_t input_mode = 0;
char angle_str[4] = {0};
uint8_t angle_idx = 0;

FILE *fio_0 = &usart0_Stream;

const float sine_table[SINE_TABLE_SIZE] = {
	50, 55, 60, 64, 69, 73, 78, 82,
	85, 89, 92, 94, 96, 98, 99, 99,
	100, 100, 99, 97, 96, 94, 91, 89,
	85, 82, 78, 73, 69, 64, 60, 55,
	50, 45, 40, 35, 31, 26, 22, 18,
	15, 11, 8, 6, 4, 2, 1, 0,
	0, 0, 1, 2, 4, 6, 8, 11,
	15, 18, 22, 26, 31, 35, 40, 45
};

const char keypad[4][3] = {
	{'1','2','3'},
	{'4','5','6'},
	{'7','8','9'},
	{'*','0','#'}
};

void timer1_init_ctc() {
	
	TCCR0A = (1<<COM0B1) | (1<<WGM00) | (1<<WGM01);
	OCR0A = TOP_1;
	OCR0B = (dutycycle/100)*TOP_1;
	TIMSK0 = (1<<TOIE0);
 	TCCR0B = (1<<WGM02);/* | (1<<CS01);*/
	
	TCCR1B |= (1 << WGM12); // CTC mode
	TCCR1B |= (1 << CS11) | (1<<ICES1);  // Prescaler 8, 2MHz clock
	OCR1A = 20;             // Compare every 10 micro sec
	TIMSK1 |= (1 << OCIE1A) | (1<<ICIE1);
	sei();

	TCCR2A = (1<<WGM21); // Set CTC mode
	TIMSK2 = (1<<OCIE2A); // Enable COMPA interrupt
	OCR2A = 250; // TOP value for a 1 ms period
// 	TCNT2 = 0;
	
	init_uart0(103);
	fprintf_P(fio_0, PSTR("System Booted, built %s on %s\n\r"), __TIME__, __DATE__);
}

void gpio_init() {
	// Servo outputs on D11 and D12
	DDRB |= (1 << SERVO2_PIN);
	PORTB &= ~(1 << SERVO2_PIN);

	// Keypad rows: PC0–PC3 (A0–A3) as output
	DDRC |= 0b00001111;
	PORTC |= 0b00001111;

	// Keypad columns:
	// PC4 (A4), PC5 (A5), PD2 as input with pull-up
	DDRC &= ~((1 << PINC4) | (1 << PINC5));
	PORTC |= (1 << PINC4) | (1 << PINC5);

	DDRD &= ~(1 << PIND2);
	DDRD |= (1<<PORTD5) | (1<<PORTD4) | (1<<PORTD7) | (1<<PORTD6) | (1<<SERVO1_PIN) ;
	PORTD |= (1 << PIND2);
	PORTD &= ~(1 << SERVO1_PIN);
}

char keypad_getkey() {
	for (uint8_t row = 0; row < 4; row++) {
		PORTC |= 0b00001111;         // Set all rows high
		PORTC &= ~(1 << row);        // Pull one row low
		_delay_us(5);                // Wait for settling

		if (!(PINC & (1 << PINC4))) {
			while (!(PINC & (1 << PINC4)));
			return keypad[row][0];
		}
		if (!(PINC & (1 << PINC5))) {
			while (!(PINC & (1 << PINC5)));
			return keypad[row][1];
		}
		if (!(PIND & (1 << PIND2))) {
			while (!(PIND & (1 << PIND2)));
			return keypad[row][2];
		}
	}
	return 0;
}

void reset_input() {
	input_mode = 1;
	current_servo = 0;
	angle_idx = 0;
	angle_str[0] = '\0';
	servo1_ticks = 150;
	servo2_ticks = 150;
}

void handle_key(char key) {
	if (key == '*') {
		fprintf_P(fio_0, PSTR("*"));
		reset_input();
		} else if (key == '#') {
		fprintf_P(fio_0, PSTR("#"));
		if (angle_idx > 0) {
			int is_negative = 0;
			int magnitude = 0;

			if (angle_str[0] == '0' && angle_idx > 1) {
				is_negative = 1;
				magnitude = atoi(&angle_str[1]);
				} else {
				magnitude = atoi(angle_str);
			}

			if (magnitude <= 45) {
				int offset = (magnitude * 100) / 90;

				if (!is_negative) {
					offset = -offset;
				}
				if (input_mode == 2) {
					offset = -offset;
				}

				int pulse_width = 150 + offset;
				if (pulse_width < 100) pulse_width = 100;
				if (pulse_width > 200) pulse_width = 200;

				if (input_mode == 1) {
					servo1_ticks = pulse_width;
					input_mode = 2;
					current_servo = 1;
					} else if (input_mode == 2) {
					servo2_ticks = pulse_width;
					input_mode = 0;
				}
			}

			angle_idx = 0;
			angle_str[0] = '\0';
		}
		} else if (key >= '0' && key <= '9') {
		if (angle_idx < 3 && input_mode > 0) {
			angle_str[angle_idx++] = key;
			angle_str[angle_idx] = '\0';
		}
	}
}

ISR(TIMER0_OVF_vect){
	count = (count + 1) % SINE_TABLE_SIZE;
	count2_1++;
	count2_0++;
	uint8_t verify;
	if(x >= 0){
		verify = (1<<x);
		if((verify & number) == 0){
			sendbit = 0;
			OCR0A = TOP_0;
			OCR0B = (sine_table[count]/100)*TOP_0;
		}
		else{
			sendbit = 1;
			OCR0A = TOP_1;
			OCR0B = (sine_table[count]/100)*TOP_1;
		}
		}else{
		x = 7;
		//TIMSK0 &= ~(1<<TOIE0);
	}
	if(sendbit == 0){
		if(count2_0 == 256){
			x--;
			count2_0 = 0;
			count2_1 = 0;
		}
	}
	if(sendbit == 1){
		if(count2_1 == 512){
			x--;
			count2_0 = 0;
			count2_1 = 0;
		}
	}
}

ISR(TIMER1_COMPA_vect) {
	timer_ticks++;

	// Servo 1: begins at tick 1
	if (timer_ticks == 1) {
		PORTD |= (1 << SERVO1_PIN);
	}
	if (timer_ticks == servo1_ticks) {
		PORTD &= ~(1 << SERVO1_PIN);
	}

	// Servo 2: begins at tick 1000
	if (timer_ticks == 1000) {
		PORTB |= (1 << SERVO2_PIN);
	}
	if (timer_ticks == (1000 + servo2_ticks)) {
		PORTB &= ~(1 << SERVO2_PIN);
	}

	if (timer_ticks >= 2000) {
		timer_ticks = 0;
	}
}

ISR(TIMER1_CAPT_vect){
	count1++;
	if(count0 > 25){ // check if period has been reached
		value++;
		if(count1 >5){
			answer[answer_index] = 1;
			answer_index++;
		}
		else{
			answer[answer_index] = 0;
			answer_index++;
		}
		count1 = 0;
		count0 = 0;
	}
	if(answer_index > 7){
		answer_index = 0;
	}
}

ISR(TIMER2_COMPA_vect){
	count0++;
	test_count++;
}

int main(void) {
	gpio_init();
	timer1_init_ctc();
	int i;
	while (1) {
		if(test_count == 2000){
			fprintf_P(fio_0, PSTR("%d\n"), value);
			for(i = 0;i < 8;i++){
				fprintf_P(fio_0, PSTR("%d"), answer[i]);
			}
			fprintf_P(fio_0, PSTR("\n"));
		}
		char key = keypad_getkey();
		if (key) {
			handle_key(key);
			_delay_ms(50);
		}
	}
}