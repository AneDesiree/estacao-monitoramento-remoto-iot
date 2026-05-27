// ----------- SENSOR -----------

#define F_CPU 16000000UL
#define BAUD 9600
#define MYUBRR ((F_CPU / (16UL * BAUD)) - 1)

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdint.h>

// --------- VARI�VEIS ---------
volatile unsigned char received = 0;
volatile unsigned char last_cmd = 0;
volatile unsigned long ms_ticks = 0;

volatile unsigned char send_data_flag = 0; // envia 1/s
volatile unsigned char send_once_flag = 0; // envia pedido �nico

// Valores do sensor
unsigned char sensor_values[] = {
    220,215,200,190,195,180,155,140,130,120,115,
    100,90,80,75,70,72,70,68,66,65,50,70,75,75,
    75,70,70,80,85,90,100,110,120,130,140,150
};
const unsigned char n_valores = sizeof(sensor_values)/sizeof(sensor_values[0]);
unsigned char index = 0;

// --------- PINOS ---------
#define TX_SOFT PB1

void init_pins(void) {
    DDRB |= (1 << TX_SOFT); // pino soft UART
    PORTB |= (1 << TX_SOFT); // linha idle HIGH
    DDRD = (1 << PD1);      // TX0 UART hardware
    PORTD = 0x00;
}

// --------- TIMER 1 MS ---------
void timer_init() {
    TCCR1A = 0x00;
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10); 
    OCR1A = 249; // 1 ms
    TIMSK1 = (1 << OCIE1A);
    TCNT1 = 0;
    sei();
}

ISR(TIMER1_COMPA_vect) {
    ms_ticks++;
}

// --------- UART HARDWARE ---------
void uart_init(unsigned int ubrr) {
    UBRR0H = (unsigned char)(ubrr >> 8);
    UBRR0L = (unsigned char)(ubrr);
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void uart_transmit(unsigned char data) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = data;
}

// --------- SOFT UART ASCII ---------
void uart_transmit_soft(unsigned char data) {
    PORTB &= ~(1 << TX_SOFT);
    _delay_us(104); // 9600 baud

    // 8 bits de dados
    for (int i = 0; i < 8; i++) {
        if (data & 0x01) PORTB |= (1 << TX_SOFT);
        else PORTB &= ~(1 << TX_SOFT);
        data >>= 1;
        _delay_us(104);
    }

    // stop bit
    PORTB |= (1 << TX_SOFT);
    _delay_us(104);
}

// Envia n�mero como ASCII pelo soft UART
void uart_print_number(unsigned char val) {
    char buf[4];
    sprintf(buf, "%u", val);
    for (int i = 0; buf[i]; i++)
        uart_transmit_soft(buf[i]);
    uart_transmit_soft(' ');  
}

// --------- ISR UART HARDWARE ---------
ISR(USART_RX_vect) {
    last_cmd = UDR0;
    received = 1;
}

// --------- MAIN ---------
int main(void) {
    init_pins();
    uart_init(MYUBRR);
    timer_init();
    sei();

    unsigned long last_send_time = 0;

    while (1) {
        // Processa comando da esta��o
        if (received) {
            received = 0;
            switch (last_cmd) {
                case 'I': // IDLE: envia 1/s
                    send_data_flag = 1;
                    send_once_flag = 0;
                    break;
                case 'S': // Sleep: para de enviar
                    send_data_flag = 0;
                    send_once_flag = 0;
                    break;
                case 'R': // Pedido �nico
                    send_once_flag = 1;
                    break;
            }
        }

        // -------- Envio de dado 1/s em IDLE --------
        if (send_data_flag && (ms_ticks - last_send_time >= 1000UL)) {
            unsigned char val = sensor_values[index];
            uart_transmit(val);        // para esta��o
            uart_print_number(val);    // para terminal ASCII
            index = (index + 1) % n_valores;
            last_send_time = ms_ticks;
        }

        // -------- Envio �nico imediato (pedido 'R') --------
        if (send_once_flag) {
            unsigned char val = sensor_values[index];
            uart_transmit(val);        // para esta��o
            uart_print_number(val);    // para terminal ASCII
            index = (index + 1) % n_valores;
            send_once_flag = 0;        // limpa flag imediatamente
            last_send_time = ms_ticks; // evita envio 1/s colidindo
        }
    }
}
