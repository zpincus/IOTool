#include "commands.h"
#include "interpreter.h"
#include "utils.h"
#include "pins.h"
#include "usb_serial.h"

uint16_t steady_wait_time_half_us = 20;

void raw_wait_high(void *params) {
    uint8_t pin_number = *(uint8_t *) params;
    SET_PIN_LOW(pin_number, ddr); // set pin for input
    SET_PIN_HIGH(pin_number, port); // enable pullup resistor
    while (!GET_PIN(pin_number, pin) && running) {}
}

void raw_wait_low(void *params) {
    uint8_t pin_number = *(uint8_t *) params;
    SET_PIN_LOW(pin_number, ddr); // set pin for input
    SET_PIN_HIGH(pin_number, port); // enable pullup resistor
    while (GET_PIN(pin_number, pin) && running) {}
}

void raw_wait_change(void *params) {
    uint8_t pin_number = *(uint8_t *) params;
    SET_PIN_LOW(pin_number, ddr); // set pin for input
    SET_PIN_HIGH(pin_number, port); // enable pullup resistor
    uint8_t current = GET_PIN(pin_number, pin);
    while (GET_PIN(pin_number, pin) == current && running) {}
}

void steady_wait(uint8_t pin_number, uint8_t target) {
    while (GET_PIN(pin_number, pin) != target && running) {}
    TCCR3B = TIMER3_DISABLE; // disable timer 3 while we set up the compare registers
    TIFR3 = BIT(OCF3B); // clear any timer-match flags present
    OCR3B = TCNT3 + steady_wait_time_half_us; // set up match time (wraparound expected; works great)
    TCCR3B = TIMER3_ENABLE; // start timer 3
    while (!(TIFR3 & BIT(OCF3B)) && running) {
        if (GET_PIN(pin_number, pin) != target) { // if the pin changes, reset the wait time
            TCCR3B = TIMER3_DISABLE; // disable timer 3 while we set up the compare registers
            OCR3B = TCNT3 + steady_wait_time_half_us; // set up match time (wraparound expected; works great)
            TCCR3B = TIMER3_ENABLE; // start timer 3
        }
    }
}

void wait_high(void *params) {
    uint8_t pin_number = *(uint8_t *) params;
    SET_PIN_LOW(pin_number, ddr); // set pin for input
    SET_PIN_HIGH(pin_number, port); // enable pullup resistor
    steady_wait(pin_number, 1);
}

void wait_low(void *params) {
    uint8_t pin_number = *(uint8_t *) params;
    SET_PIN_LOW(pin_number, ddr); // set pin for input
    SET_PIN_HIGH(pin_number, port); // enable pullup resistor
    steady_wait(pin_number, 0);
}

void wait_change(void *params) {
    uint8_t pin_number = *(uint8_t *) params;
    SET_PIN_LOW(pin_number, ddr); // set pin for input
    SET_PIN_HIGH(pin_number, port); // enable pullup resistor
    uint8_t current = GET_PIN(pin_number, pin);
    steady_wait(pin_number, !current);
}

void set_wait_time(void *params) {
    steady_wait_time_half_us = *(uint16_t *) params;
}

void delay_milliseconds(void *params) {
    uint16_t ms_delay = *(uint16_t *) params;
    ms_timer = 0;
    ms_timer_target = ms_delay;
    ms_timer_done = false;
    TCCR3B = TIMER3_DISABLE; // disable timer 3 while we set up the compare registers
    TIMSK3 |= MS_TIMER_MASK; // enable millisecond timer
    OCR3A = TCNT3; // Fire off the millisecond timer immediately after starting the clocks
    TCCR3B = TIMER3_ENABLE; // start timer 3
    while (!ms_timer_done && running) {}
    TIMSK3 &= ~MS_TIMER_MASK; // disable millisecond timer
}

void delay_microseconds(void *params) {
    uint16_t half_us_delay = *(uint16_t *) params;
    TIMSK3 = 0; // no USB interrupts; won't get "quit" signal
    TIFR3 = BIT(OCF3B); // clear any timer-match flags present
    TCCR3B = TIMER3_DISABLE; // disable timer 3 while we set up the compare registers
    OCR3B = TCNT3 + half_us_delay; // set up match time (wraparound expected; works great)
    TCCR3B = TIMER3_ENABLE; // start timer 3
    while (!(TIFR3 & BIT(OCF3B))) {}
    TIMSK3 = USB_TIMER_MASK;
}

void pwm8(void *params) {
    uint8_t pin_number = *(uint8_t *) params;
    uint8_t pwm_value = *(uint8_t *) (params + 1);
    SET_PIN_HIGH(pin_number, ddr); // set pin for output
    *((volatile uint8_t *) pins[pin_number].ocr) = pwm_value;
    ENABLE_PWM(pin_number);
}

void pwm16(void *params) {
    uint8_t pin_number = *(uint8_t *) params;
    uint16_t pwm_value = *(uint16_t *) (params + 1);
    SET_PIN_HIGH(pin_number, ddr); // set pin for output
    *((volatile uint16_t *) pins[pin_number].ocr) = pwm_value;
    ENABLE_PWM(pin_number);
}

void set_high(void *params) {
    uint8_t pin_number = *(uint8_t *) params;
    if (pins[pin_number].ocr != NULL) {
        DISABLE_PWM(pin_number);
    }
    SET_PIN_HIGH(pin_number, ddr); // set pin for output
    SET_PIN_HIGH(pin_number, port); // set pin for output
}

void set_low(void *params) {
    uint8_t pin_number = *(uint8_t *) params;
    if (pins[pin_number].ocr != NULL) {
        DISABLE_PWM(pin_number);
    }
    SET_PIN_HIGH(pin_number, ddr); // set pin for output
    SET_PIN_LOW(pin_number, port); // set pin for output
}

void set_tristate(void *params) {
    uint8_t pin_number = *(uint8_t *) params;
    if (pins[pin_number].ocr != NULL) {
        DISABLE_PWM(pin_number);
    }
    SET_PIN_LOW(pin_number, ddr); // set pin for input
    SET_PIN_LOW(pin_number, port); // disable pullup resistor
}

void char_receive(void *params) {
    run_serial_tasks_from_isr = false; // we'll do this ourselves
    uint8_t data = usb_serial_wait_byte();
    if (data == QUIT_BYTE) {
        running = false;
    }
    // otherwise discard input
    run_serial_tasks_from_isr = true;
}

void char_transmit(void *params) {
    uint8_t output = *(uint8_t *) params;
    usb_serial_write_byte(output);
    usb_serial_flush();
}

void loop(void *params) {
    uint8_t goto_index = *(uint8_t *) params;
    uint8_t loop_index = *(uint8_t *) (params + 1);
    if (loop_current_values[loop_index] > 0) {
        loop_current_values[loop_index]--;
        program_counter = goto_index;
    }
}

void goto_(void *params) {
    uint8_t goto_index = *(uint8_t *) params;
    program_counter = goto_index;
}

void noop(void *params) {
}
