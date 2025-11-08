//#define F_CPU 8385000UL
#define BAUD 9600
#define MYUBRR F_CPU/16/BAUD-1

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <stdio.h>

//#define DEBUG

volatile uint8_t T1_flag = 0, T2_flag = 0, T3_flag = 0, T4_flag = 0;
volatile static uint16_t time_units = 0;
volatile static uint16_t T1_start_time = 0, T2_start_time = 0;
volatile static uint16_t T3_start_time = 0, T4_start_time = 0;
volatile static uint8_t T1_active = 0, T2_active = 0, T3_active = 0, T4_active = 0;

#ifdef DEBUG
	void uart_init(void)
	{
		uint16_t ubrr = MYUBRR;
		UBRRH = (uint8_t)(ubrr >> 8);
		UBRRL = (uint8_t)(ubrr & 0xFF);
		UCSRB = (1 << TXEN);
		UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);
	}

	void uart_send_char(char c)
	{
		while (!(UCSRA & (1 << UDRE)));
		UDR = c;
	}

	int uart_putchar(char c, FILE *stream)
	{
		if (c == '\n') uart_putchar('\r', stream);
		uart_send_char(c);
		return 0;
	}

	FILE uart_output = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);
#endif

void init_ports(void)
{
	/*Настраиваем все пины портов А на вход*/
	DDRA = 0x00;

	/*Не подаем питание на порты А*/
	PORTA = 0x00;

	/*Настраиваем все пины портов B на выход*/
	DDRB = 0xFF;

	/*Не подаем питание на порты B*/
	PORTB = 0x00;
	
	/*Настраиваем первые 7 пинов порта C  на выход*/
	/*0x7F = 0b01111111*/
	DDRC = 0x7F;
	
	/*Не подаем питание на порты C*/
	PORTC = 0x00;
}

void timer_init(void)
{
	OCR1A = 1299;
	TCCR1A = 0x00;
	TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
	TIMSK |= (1 << OCIE1A);
}

ISR(TIMER1_COMPA_vect)
{
	time_units++;

	#ifdef DEBUG
		printf("Time: %u, T1: %d, T2: %d, T3: %d, T4: %d\n", time_units, T1_flag, T2_flag, T3_flag, T4_flag);
	#endif
	
	/*T1*/
	if (time_units - T1_start_time >= 2) {
		T1_start_time = time_units;
		T1_flag ^= 1;
		PORTC ^= (1 << PC3);
	}

	/*T2*/
	if (!T2_active && (time_units - T2_start_time >= 8)) {
		T2_start_time = time_units;
		T2_active = 1;
		T2_flag = 1;
		PORTC |= (1 << PC4);
	}

	if (T2_active && (time_units - T2_start_time >= 2)) {
		T2_active = 0;
		T2_flag = 0;
		PORTC &= ~(1 << PC4);
	}

	/*T3*/
	if (!T3_active && (time_units - T3_start_time >= 12)) {
		T3_start_time = time_units;
		T3_active = 1;
		T3_flag = 1;
		PORTC |= (1 << PC5);
	}

	if (T3_active && (time_units - T3_start_time >= 2)) {
		T3_active = 0;
		T3_flag = 0;
		PORTC &= ~(1 << PC5);
	}

	/*T4*/
	if (!T4_active && (time_units - T4_start_time >= 16)) {
		T4_start_time = time_units;
		T4_active = 1;
		T4_flag = 1;
		PORTC |= (1 << PC6);
	}

	if (T4_active && (time_units - T4_start_time >= 2)) {
		T4_active = 0;
		T4_flag = 0;
		PORTC &= ~(1 << PC6);
	}
}

uint8_t count_bits(uint8_t value)
{
	uint8_t count = 0;
	for (uint8_t i = 0; i < 8; ++i) {
		/*Если бит в позиции i == 1, то прибавляем count
		 *Например: value = 0x11010011, i = 3, порядок Little Endian
		 *тогда данное условие вернёт 0, т.к. 3 бит в value не равен 1
		 */
		if (value & (1 << i))
			count++;
	}

	return count;
}

void update_outputs(void)
{
	uint8_t inputs = PINA;
	static uint8_t delayed_inputs = 0;
	#ifdef DEBUG
		static uint8_t inputs_changed = 0;
	#endif

	if (T2_flag) {
		delayed_inputs = ~inputs;
		#ifdef DEBUG
			inputs_changed = 1;
		#endif
	}
	else
		delayed_inputs = 0x00;
	PORTB = delayed_inputs;

	#ifdef DEBUG
		if (inputs_changed) {
			printf("Inputs: 0x%02X, Delayed: 0x%02X\n", inputs, delayed_inputs);
			inputs_changed = 0;
		}
	#endif

	uint8_t x1 = (inputs >> 0) & 1;
	uint8_t x2 = (inputs >> 1) & 1;
	uint8_t x3 = (inputs >> 2) & 1;
	uint8_t x4 = (inputs >> 3) & 1;
	uint8_t x5 = (inputs >> 4) & 1;
	uint8_t x6 = (inputs >> 5) & 1;
	uint8_t x7 = (inputs >> 6) & 1;
	uint8_t x8 = (inputs >> 7) & 1;

	uint8_t part1 = x1 | x2 | (x3 & T2_flag) | (x4 & T4_flag);
	uint8_t part2 = x5 | x6 | (x7 & T1_flag) | (x8 & T3_flag);
	uint8_t y9 = part1 & (~part2);

	if (y9) PORTC |= (1 << PC0); else PORTC &= ~(1 << PC0);
	uint8_t y10 = x1 & x3 & x5;
	if (y10) PORTC |= (1 << PC1); else PORTC &= ~(1 << PC1);

	uint8_t y11 = (count_bits(inputs) % 2 == 0);
	if (y11) PORTC |= (1 << PC2); else PORTC &= ~(1 << PC2);

	#ifdef DEBUG
		static uint8_t last_y9 = 0, last_y10 = 0, last_y11 = 0;
		if (y9 != last_y9 || y10 != last_y10 || y11 != last_y11) {
			printf("y9: %d, y10: %d, y11: %d", y9, y10, y11);
			last_y9 = y9;
			last_y10 = y10;
			last_y11 = y11;
		}
	#endif
}

int main(void)
{
	init_ports();
	timer_init();
	
	#ifdef DEBUG
		uart_init();
		stdout = &uart_output;
	#endif
	/*Разрешаем глобальные прерывания*/
	sei();

	#ifdef DEBUG
		printf("System started\n");
		printf("ATmega32 - Timer Test\n");
	#endif

	while(1) {
		update_outputs();
		
		/*Ставим небольшу задержку для стабильности работы*/
		_delay_ms(10);
	}

	return 0;
}