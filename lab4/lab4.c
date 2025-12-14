#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define BAUD 9600
#define UBBR_VALUE F_CPU / (16 * BAUD) - 1

volatile uint8_t int0_pressed = 0;
volatile uint8_t usart_tx_complete = 0;

void int0_init(void) 
{
	/* Enabling the external pin interrupt
	 * (External Interrupt Request 0 Enabling) */
	GICR |= (1 << INT0);
	/* Setting up Interrupt Sense Control
	 * The falling edge (like pushing the button) generates
	 * an interrupt request */
	MCUCR |= (1 << ISC01);
	MCUCR &= ~(1 << ISC00);
}

void usart_init(unsigned int baud)
{
	/* UBRR = F_CPU/(16*BAUD) - 1 
	 * writing the baud value to high byte and low byte */
	UBRRH = (unsigned char)(baud >> 8);
	UBRRL = (unsigned char)baud;
	/* Enabling the USART Receiver (RXEN) 
	 * and USART Transmitter (TXEN) */
	UCSRB = (1 << RXEN) | (1 << TXEN) | (1 << TXCIE);
	/* Selecting the 1character size: 8 bits */
	UCSRC = (1 << UCSZ1) | (1 << UCSZ0) | (1 << URSEL);
}

void usart_send_char(char c)
{
	/* waiting when transmit buffer (UDR) is ready
	 * to receive new data */
	while (!(UCSRA & (1 << UDRE)));
	/* put the data to transmit buffer */
	UDR = c;
}

void usart_send_string(const char *str)
{
	while (*str) {
		usart_send_char(*str++);
	}
}

void spi_init(void) 
{
	DDRB |= (1 << DDB5) | (1 << DDB7) | (1 << DDB4);
	DDRB &= ~(1 << DDB6);
	/* Enabling SPI Interrupts;
	 * Enabling SPI
	 * Selecting Master SPI mode;
	 * Setting up SPI Clock Rate to F_CPU/16 */
	SPCR = (1 << SPIE) | (1 << SPE) | (1 << MSTR) | (1 << SPR0);
}

uint8_t spi_transfer(uint8_t data) 
{
	/* Starting the transmit of the data */
	SPDR = data;
	/* Waiting for end of transmition */
	while (!(SPSR & (1 << SPIF)));

	return SPDR;
}

void gpio_init(void)
{
	/* leds */
	DDRB |= (1 << PB0) | (1 << PB1) | (1 << PB2);
	/* turn off the leds */
	PORTB &= ~((1 << PB0) | (1 << PB1) | (1 << PB2));

	DDRD &= ~(1 << PD2);
	PORTD |= (1 << PD2);

	DDRD |= (1 << PD1);
	DDRD &= ~(1 << PD0);
}

ISR(INT0_vect)
{
	int0_pressed = 1;
	PORTB |= (1 << PB2);
	_delay_ms(500);
	PORTB &= ~(1 << PB2);
}

ISR(SPI_STC_vect)
{
	PORTB |= (1 << PB0);
	_delay_ms(500);
	PORTB &= ~(1 << PB0);
}

ISR(USART_TXC_vect)
{
	usart_tx_complete = 1;
	PORTB |= (1 << PB1);
	_delay_ms(500);
	PORTB &= ~(1 << PB1);
}

int main(void)
{
	gpio_init();
	usart_init(UBBR_VALUE);
	spi_init();
	int0_init();

	sei();

	while(1) {
		if (int0_pressed) {
			int0_pressed = 0;
			usart_send_string("INT0 comleted\n");
		}

		if (usart_tx_complete) {
			usart_tx_complete = 0;
			spi_transfer(0xAA);
		}
	}

	return 0;
}

