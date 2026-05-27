//--------------- ESTAÇÃO DE MONITORAMENTO DE LUMINOSIDADE -----------------

#define F_CPU 16000000UL
#define BAUD 9600
#define MYUBRR ((F_CPU / (16UL * BAUD)) - 1)

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>

// ---- VARIÁVEIS ----
volatile unsigned char sample_ready = 0;
volatile unsigned char last_value = 0;

unsigned char sent_idle_cmd = 0;
signed char low_count = 0;
unsigned char high_count = 0;

typedef enum {
    STATE_IDLE,
    STATE_POWER_DOWN
} State;

volatile State state = STATE_IDLE;

// ----------- PINOS -------------
void init_pins(void) {

    DDRB  = (1 << PB0) | (1 << PB1);
    PORTB = 0xFC;

    DDRC  = 0x00;
    PORTC = 0xFF;

    DDRD  = (1 << PD1);
    PORTD = 0xFC;
}

// ----------- OTIMIZAÇÃO DE ENERGIA -------------
void power_optimization(void) {

    // Desliga comparador analógico
    ACSR |= (1 << ACD);

    // Desliga ADC
    ADCSRA &= ~(1 << ADEN);

    // Remove entradas digitais do ADC
    DIDR0 = 0x3F;

    // Corta clock de periféricos não usados
    PRR |= (1 << PRADC) | (1 << PRSPI) | (1 << PRTWI);
}

// ----------- BOD OFF ANTES DO SLEEP -------------
void disable_bod_before_sleep(void) {

    MCUCR |= (1 << BODSE) | (1 << BODS);
    MCUCR = (MCUCR & ~(1 << BODSE)) | (1 << BODS);
}

// ------------ UART --------------
void uart_init(unsigned int ubrr) {

    UBRR0H = (unsigned char)(ubrr >> 8);
    UBRR0L = (unsigned char)ubrr;

    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void uart_transmit(unsigned char data) {

    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = data;
}

// ------------ INTERRUPÇÃO UART -------------
ISR(USART_RX_vect) {

    last_value = UDR0;
    sample_ready = 1;
}

// ------------ WATCHDOG TIMER -------------
void wdt_init(void) {

    cli();

    MCUSR &= ~(1 << WDRF);

    WDTCSR = (1 << WDCE) | (1 << WDE);

    // ~8s
    WDTCSR = (1 << WDIE) | (1 << WDP3) | (1 << WDP0);

    sei();
}

ISR(WDT_vect) {
    // apenas wake-up
}

// --------- IDLE MODE ---------
void idle_mode(void) {

    PORTB |= (1 << PB0);

    if (!sent_idle_cmd) {

        uart_transmit('I');
        _delay_ms(50);
        sent_idle_cmd = 1;
    }

    set_sleep_mode(SLEEP_MODE_IDLE);
    sleep_enable();

    disable_bod_before_sleep();
    sleep_cpu();

    sleep_disable();

    if (sample_ready) {

        sample_ready = 0;

        unsigned char v = last_value;

        if (v <= 70)
            low_count++;
        else
            low_count = 0;

        if (low_count >= 3) {

            low_count = 0;
            sent_idle_cmd = 0;

            uart_transmit('S');
            _delay_ms(50);

            PORTB &= ~(1 << PB0);
            state = STATE_POWER_DOWN;
        }
    }
}

// --------- POWER DOWN MODE ---------
void power_down_mode(void) {

    PORTB &= ~(1 << PB0);

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();

    disable_bod_before_sleep();
    sleep_cpu();

    sleep_disable();
}

// --------- WAKE HANDLE ---------
void wake_handle(void) {

    PORTB |= (1 << PB1);

    _delay_ms(100);

    uart_transmit('R');
    _delay_ms(20);

    while (!sample_ready);

    unsigned char v = last_value;
    sample_ready = 0;

    PORTB &= ~(1 << PB1);

    if (v > 70)
        high_count++;
    else
        high_count = 0;

    if (high_count >= 3) {

        high_count = 0;
        sent_idle_cmd = 0;
        state = STATE_IDLE;

    } else {

        power_down_mode();
    }
}

// ------------- MAIN --------------
int main(void) {

    init_pins();

    power_optimization();

    uart_init(MYUBRR);

    wdt_init();

    sei();

    while (1) {

        switch (state) {

            case STATE_IDLE:
                idle_mode();
                break;

            case STATE_POWER_DOWN:
                power_down_mode();
                wake_handle();
                break;
        }
    }
}