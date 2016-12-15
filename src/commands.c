// Copyright 2014 Zachary Pincus (zpincus@wustl.edu / zplab.wustl.edu)
// This file is part of IOTool.
//
// IOTool is free software; you can redistribute it and/or modify
// it under the terms of version 2 of the GNU General Public License as
// published by the Free Software Foundation.


#include "commands.h"
#include "interpreter.h"
#include "utils.h"
#include "pins.h"
#include "usb_serial.h"
#include <stdlib.h>
#include <string.h>

uint16_t steady_wait_time_half_us = 20;
uint16_t starting_us_timer;

void undebounced_wait_high(void *params) {
    uint8_t pin_number = *(uint8_t *) params;
    SET_PIN_LOW(pin_number, ddr); // set pin for input
    SET_PIN_HIGH(pin_number, port); // enable pullup resistor
    while (!GET_PIN(pin_number, pin) && running) {}
}

void undebounced_wait_low(void *params) {
    uint8_t pin_number = *(uint8_t *) params;
    SET_PIN_LOW(pin_number, ddr); // set pin for input
    SET_PIN_HIGH(pin_number, port); // enable pullup resistor
    while (GET_PIN(pin_number, pin) && running) {}
}

void undebounced_wait_change(void *params) {
    uint8_t pin_number = *(uint8_t *) params;
    SET_PIN_LOW(pin_number, ddr); // set pin for input
    SET_PIN_HIGH(pin_number, port); // enable pullup resistor
    uint8_t current = GET_PIN(pin_number, pin);
    while (GET_PIN(pin_number, pin) == current && running) {}
}

void steady_wait(uint8_t pin_number, uint8_t target) {
    TCCR3B = TIMER3_DISABLE; // disable timer 3 while we set up the compare registers
    TIFR3 = BIT(OCF3B); // clear any timer-match flags present
    OCR3B = TCNT3 + steady_wait_time_half_us; // set up match time (wraparound expected; works great)
    TCCR3B = TIMER3_ENABLE; // start timer 3
    while (!GET_BIT(TIFR3, OCF3B) && running) {
        if ((GET_PIN(pin_number, pin) != 0) != target) { // if the pin changes, reset the wait time
            //NB: the (GET_PIN(pin_number, pin) != 0) bit above is to convert a pin value, which could be any bit set in the byte, to a strict 0 or 1
            TCCR3B = TIMER3_DISABLE; // disable timer 3 while we set up the compare registers
            TIFR3 = BIT(OCF3B); // clear any timer-match flags present
            OCR3B = TCNT3 + steady_wait_time_half_us; // set up match time (wraparound expected; works great)
            TCCR3B = TIMER3_ENABLE; // start timer 3
        }
    }
}

void wait_high(void *params) {
    uint8_t pin_number = *(uint8_t *) params;
    SET_PIN_LOW(pin_number, ddr); // set pin for input
    SET_PIN_HIGH(pin_number, port); // enable pullup resistor
    while (!GET_PIN(pin_number, pin) && running) {}
    if (steady_wait_time_half_us) {
        steady_wait(pin_number, 1);
    }
}

void wait_low(void *params) {
    uint8_t pin_number = *(uint8_t *) params;
    SET_PIN_LOW(pin_number, ddr); // set pin for input
    SET_PIN_HIGH(pin_number, port); // enable pullup resistor
    while (GET_PIN(pin_number, pin) && running) {}
    if (steady_wait_time_half_us) {
        steady_wait(pin_number, 0);
    }
}

void wait_change(void *params) {
    uint8_t pin_number = *(uint8_t *) params;
    SET_PIN_LOW(pin_number, ddr); // set pin for input
    SET_PIN_HIGH(pin_number, port); // enable pullup resistor
    uint8_t current = GET_PIN(pin_number, pin);
    while (GET_PIN(pin_number, pin) == current && running) {}
    if (steady_wait_time_half_us) {
        steady_wait(pin_number, !current);
    }
}

void set_wait_time(void *params) {
    steady_wait_time_half_us = *(uint16_t *) params;
}

void delay_milliseconds(void *params) {
    uint16_t ms_delay = *(uint16_t *) params;
    ms_timer = 0; // millisecond timer ISR should be disabled, so it's ok to set this without worrying it'll get stomped on
    ms_timer_target = ms_delay;
    ms_timer_done = false;
    if (ms_timer_target == 0) {
        return; // special case for bizarre request of 0 ms delay
    }
    TCCR3B = TIMER3_DISABLE; // disable timer 3 while we set up the compare registers
    TIFR3 = BIT(OCF3A); // clear previous output compare matches so ms timer ISR doesn't fire immediately on being enabled
    SET_MASK_HI(TIMSK3, MS_TIMER_MASK); // enable millisecond timer
    OCR3A = TCNT3 + 2000; // Fire off the millisecond timer 1 ms after starting the clocks
    TCCR3B = TIMER3_ENABLE; // start timer 3
    while (!ms_timer_done && running) {}
    SET_MASK_LO(TIMSK3, MS_TIMER_MASK); // disable millisecond timer
}

void delay_microseconds(void *params) {
    uint16_t half_us_delay = *(uint16_t *) params;
    TIMSK3 = 0; // no USB interrupts; won't get "quit" signal
    TIFR3 = BIT(OCF3B); // clear any timer-match flags present
    TCCR3B = TIMER3_DISABLE; // disable timer 3 while we set up the compare registers
    OCR3B = TCNT3 + half_us_delay; // set up match time (wraparound expected; works great)
    TCCR3B = TIMER3_ENABLE; // start timer 3
    while (!GET_BIT(TIFR3, OCF3B)) {}
    TIMSK3 = USB_TIMER_MASK;
}

void timer_begin(void *params) {
    ms_timer = 0; // millisecond timer ISR should be disabled, so it's ok to set this without worrying it'll get stomped on
    TCCR3B = TIMER3_DISABLE; // disable timer 3 while we set up the compare registers
    TIFR3 = BIT(OCF3A); // clear previous output compare matches so ms timer ISR doesn't fire immediately on being enabled
    SET_MASK_HI(TIMSK3, MS_TIMER_MASK); // enable millisecond timer
    OCR3A = TCNT3 + 2000; // Fire off the millisecond timer 1 ms after starting the clocks
    starting_us_timer = TCNT3;
    TCCR3B = TIMER3_ENABLE; // start timer 3
}

void timer_end(void *params) {
    TCCR3B = TIMER3_DISABLE; // disable timer 3 while we read registers
    uint16_t us_timer = TCNT3;
    SET_MASK_LO(TIMSK3, MS_TIMER_MASK); // disable millisecond timer
    TCCR3B = TIMER3_ENABLE; // start timer 3
    uint16_t half_us_timed = (us_timer - starting_us_timer) % 2000; // wraparound expected for the subtraction; no problem
    uint32_t us_timed = ((uint32_t) ms_timer)*1000 + half_us_timed/2;
    if (half_us_timed % 2) {
        us_timed++;
    }
    char result[11];
    ultoa(us_timed, result, 10);
    usb_serial_write_string(result);
    usb_serial_write_byte('\n');
    usb_serial_flush();
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

void char_goto(void *params) {
    run_serial_tasks_from_isr = false; // we'll do this ourselves
    uint8_t data = usb_serial_wait_byte();
    if (data == QUIT_BYTE) {
        running = false;
    }
    program_counter = data;
    run_serial_tasks_from_isr = true;
}

void char_transmit(void *params) {
    uint8_t output = *(uint8_t *) params;
    usb_serial_write_byte(output);
    usb_serial_flush();
}

void read_digital(void *params) {
    uint8_t pin_number = *(uint8_t *) params;
    SET_PIN_LOW(pin_number, ddr); // set pin for input
    SET_PIN_LOW(pin_number, port); // disable pullup resistor
    uint8_t value = GET_PIN(pin_number, pin);
    if (steady_wait_time_half_us) {
        TCCR3B = TIMER3_DISABLE; // disable timer 3 while we set up the compare registers
        TIFR3 = BIT(OCF3B); // clear any timer-match flags present
        OCR3B = TCNT3 + steady_wait_time_half_us; // set up match time (wraparound expected; works great)
        TCCR3B = TIMER3_ENABLE; // start timer 3
        while (!GET_BIT(TIFR3, OCF3B) && running) {
            uint8_t now_value = GET_PIN(pin_number, pin);
            if (now_value != value) { // if the pin changes, reset the wait time
                value = now_value;
                TCCR3B = TIMER3_DISABLE; // disable timer 3 while we set up the compare registers
                OCR3B = TCNT3 + steady_wait_time_half_us; // set up match time (wraparound expected; works great)
                TCCR3B = TIMER3_ENABLE; // start timer 3
            }
        }
    }
    if (value) {
        usb_serial_write_byte('1');
    } else {
        usb_serial_write_byte('0');
    }
    usb_serial_write_byte('\n');
    usb_serial_flush();
}

void read_analog(void *params) {
    uint8_t pin_number = *(uint8_t *) params;
    ADC_MUX(pin_number);
    SET_BIT_HI(ADCSRA, ADSC); // start ADC conversion
    while (GET_BIT(ADCSRA, ADSC)) {} // wait for the conversion to end
    char result[5];
    utoa((uint16_t) ADC, result, 10);
    usb_serial_write_string(result);
    usb_serial_write_byte('\n');
    usb_serial_flush();
}

void loop(void *params) {
    uint8_t goto_index = *(uint8_t *) params;
    uint8_t loop_index = *(uint8_t *) (params + 1);
    if (!loop_active[loop_index]) {
        // Initialize loop variables if we're not already in this particular loop.
        // This allows for nested loops to work properly.
        loop_current_values[loop_index] = loop_initial_values[loop_index];
        loop_active[loop_index] = true;
    }
    if (loop_current_values[loop_index] > 0) {
        loop_current_values[loop_index]--;
        program_counter = goto_index;
    } else {
        loop_active[loop_index] = false;
    }
}

void goto_(void *params) {
    uint8_t goto_index = *(uint8_t *) params;
    program_counter = goto_index;
}

void noop(void *params) {
}
