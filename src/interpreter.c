// Copyright 2014 Zachary Pincus (zpincus@wustl.edu / zplab.wustl.edu)
// This file is part of IOTool.
//
// IOTool is free software; you can redistribute it and/or modify
// it under the terms of version 2 of the GNU General Public License as
// published by the Free Software Foundation.

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include "interpreter.h"
#include "usb_serial.h"
#include "pins.h"
#include "commands.h"


volatile bool run_serial_tasks_from_isr = false;
volatile bool running;
volatile uint16_t ms_timer = 0;
volatile uint16_t ms_timer_target;
volatile bool ms_timer_done;

#define PWM16_MAX (uint16_t) (1<<10)-1
#define MAX_PROGRAM_STEPS 256
#define HEAP_PER_STEP 3
#define MAX_LOOP_COMMANDS 10

#define AVCC_ADMUX BIT(REFS0)
#define AREF_ADMUX 0

const char ECHO_OFF_PSTR[] PROGMEM = "\x80\xff";
#define PROMPT '>'


command_t program[MAX_PROGRAM_STEPS];
uint8_t program_heap[MAX_PROGRAM_STEPS*HEAP_PER_STEP];
uint16_t program_size = 0; // must be uint16 to be able to hold the value of 256, indicating that program is full
uint8_t program_counter;
uint16_t loop_current_values[MAX_LOOP_COMMANDS];
uint16_t loop_initial_values[MAX_LOOP_COMMANDS];
bool loop_active[MAX_LOOP_COMMANDS];
uint8_t num_loop_commands = 0;
typedef enum {IMMEDIATE, ON_RUN} mode_t;
mode_t execute_mode = IMMEDIATE;

ISR(TIMER3_COMPA_vect) {
    ms_timer++; // add one ms
    if (ms_timer == ms_timer_target) {
        ms_timer_done = true;
    }
    OCR3A += 2000; // fire ISR again in 1 ms (wraparound expected; works great)
}

ISR(TIMER3_COMPC_vect) {
    uint8_t data;
    if (run_serial_tasks_from_isr && usb_serial_has_byte(&data)) {
        if (data == QUIT_BYTE) {
            running = false;
        } else {
            usb_serial_process_byte(data);
        }
    }
    USB_USBTask();
    OCR3C += 30000; // fire ISR again in 15 ms (wraparound expected; works great)
}

void interpreter_init(void) {

    // Timer/Counter0
    TCCR0A = BIT(WGM00) | BIT(WGM01); // Fast 8-bit PWM (freq=62.5 kHz), no outputs connected
    TIMSK0 = 0; // no interrupts
    TCCR0B = BIT(CS00); // start clock, no prescaling

    // Timer/Counter1
    TCCR1A = BIT(WGM11); // set WGM to mode 14: fast PWM; TOP of counter defined by ICR1
    TCCR1B = BIT(WGM13) | BIT(WGM12);
    ICR1 = PWM16_MAX;
    TIMSK1 = 0;// no interrupts
    TCCR1B |= BIT(CS10); // start clock, no prescaling (freq=16 MHz, period=62.5 ns)

    // Timer/Counter3
    // USB must be initialized before this function is called, as the below turns on the USB-handling ISR
    TCCR3A = 0; // Normal mode
    OCR3C = 0; // fire off USB timer right away once enabled
    TIMSK3 = USB_TIMER_MASK; // interrupt on OCR3C for usb tasks (we'll use OCR3A and OCR3B for millisecond and microsecond timing)
    TIFR3 = 0; // make sure no interrupts are queued
    TCCR3B = TIMER3_ENABLE; // start clock, prescaler=8 (freq=2 MHz, period=0.5 microseconds)

    // Timer/Counter4
    TCCR4A = BIT(PWM4A); // Enable PWM on OCR4A
    TCCR4C = BIT(PWM4D); // Enable PWM on OCR4D
    TCCR4D = 0; // Fast PWM (WGM bits zero)
    OCR4C = (1<<8)-1; // use 8-bit PWM
    TCCR4B = BIT(CS41); // start clock, prescaler=2 (freq=8 MHz, period=125 ns)

    sei(); // enable interrupts

    // ADC
    ADCSRA = BIT(ADPS2) | BIT(ADPS0); // prescaler = 32 gives 500 kHz ADC clock: fast, but some resolution loss over 128 kHz
    ADCSRB = BIT(ADHSM); // "high speed mode" purportedly gives better ADC resolution at fast clocks
    ADMUX = AVCC_ADMUX; // Default to AVcc as the voltage ref
    ADCSRA |= BIT(ADEN) | BIT(ADSC); // enable ADC and start a conversion to initialize the circuitry
    while (GET_BIT(ADCSRA, ADSC)) {} // wait for the first conversion to end
}


void clear_program(void) {
    program_size = 0;
    num_loop_commands = 0;
}

void run_program(uint16_t num_iters) {
    running = true;
    run_serial_tasks_from_isr = true;
    for (uint16_t i = 0; i < num_iters; i++) {
        program_counter = 0;
        if (!running) {
            break;
        }
        for (int l = 0; l < num_loop_commands; l++) {
            loop_active[l] = false;
        }
        while (running && program_counter < program_size) {
            uint8_t current_pc = program_counter;
            program_counter++; // increment first to allow functions to manipulate the PC.
            program[current_pc](program_heap + current_pc*HEAP_PER_STEP);
        }
    }
    running = false;
    run_serial_tasks_from_isr = false;
}


bool parse_uint8(char **in, uint8_t max, void *dst) {
    char *old_in = *in;
    unsigned long ulpin = strtoul(*in, in, 10);
    if (errno || ulpin > max || old_in == *in) {
        return false;
    }

    *(uint8_t *)dst = (uint8_t) ulpin;
    return true;
}

bool parse_uint16(char **in, uint16_t max, void *dst) {
    char *old_in = *in;
    unsigned long ulpin = strtoul(*in, in, 10);
    if (errno || ulpin > max || old_in == *in) {
        return false;
    }
    *(uint16_t *)dst = (uint16_t) ulpin;
    return true;
}

bool parse_pin(char **in, void *dst) {
    char *in_ptr = *in;
    while (*in_ptr != '\0') {
        if (isspace(*in_ptr)) {
            in_ptr++;
        } else {
            break;
        }
    }
    for (uint8_t i = 0; i < NUM_PINS; i++) {
        if (in_ptr[0] == pins[i].name[0] && (pins[i].name[1] == '\0' || in_ptr[1] == pins[i].name[1])) {
            *(uint8_t *)dst = i;
            if (pins[i].name[1] == '\0') {
                *in = in_ptr + 1;
            } else {
                *in = in_ptr + 2;
            }
            return true;
        }
    }
    return false;
}

bool parse_space_to_end(char *in) {
    while (*in != '\0') {
        if (!isspace(*in++)) {
            return false;
        }
    }
    return true;
}

typedef enum {NOERR, BAD_FUNC, BAD_PARAM, NOT_PWM, NOT_ANALOG, NO_ROOM} err_t;
typedef enum {PROGRAM, END, RUN, ADD_STEP, ECHO_OFF, RESET, AREF} input_action_t;

// forward decls for clarity
err_t add_program_step(char *line, command_t *function_out);
void interpret_line(char *line);

void interpreter_main() {
    usb_serial_write_byte(PROMPT);
    for (;;) {
        interpret_line(usb_serial_read_line());
        usb_serial_write_byte(PROMPT);
    }
}

// return whether or not to write a prompt
void interpret_line(char *line) {
    // can assume line is null-terminated
    if (parse_space_to_end(line)) {
        return;
    }

    input_action_t action;
    bool success = true;
    uint16_t num_iters = 0;
    uint8_t admux_val = AVCC_ADMUX;
    char *rest;

    if (strncmp_P(line, PSTR("program"), 7) == 0) {
            action = PROGRAM;
            rest = line+7;
    } else if (strncmp_P(line, PSTR("end"), 3) == 0) {
            action = END;
            rest = line+3;
    } else if (strncmp_P(line, PSTR("run"), 3) == 0) {
        action = RUN;
        num_iters = (uint16_t) strtoul(line+3, &rest, 10);
        if (errno) {
            success = false;
        }
        if (rest == line+3) {
            // no input for num_iters
            num_iters = 1;
        }
    } else if (strncmp_P(line, ECHO_OFF_PSTR, 2) == 0) {
        action = ECHO_OFF;
        rest = line+2;
    } else if (strncmp_P(line, PSTR("reset"), 5) == 0) {
        action = RESET;
        rest = line+5;
    } else if (strncmp_P(line, PSTR("aref"), 4) == 0) {
        action = AREF;
        admux_val = AREF_ADMUX; // use ARef as the voltage ref
        rest = line+4;
    } else if (strncmp_P(line, PSTR("avcc"), 4) == 0) {
        action = AREF;
        admux_val = AVCC_ADMUX; // use AVcc as the voltage ref
        rest = line+4;
    } else {
        action = ADD_STEP;
    }

    if (action != ADD_STEP && (!success || !parse_space_to_end(rest))) {
        usb_serial_write_string_P(PSTR("ERROR: Invalid input\n"));
        return;
    }

    switch (action) {
        err_t result;
        command_t function;
        case RUN:
            run_program(num_iters);
            break;
        case PROGRAM:
            clear_program();
            execute_mode = ON_RUN;
            break;
        case END:
            execute_mode = IMMEDIATE;
            break;
        case ECHO_OFF:
            if (!usb_serial_echo) {
                // always echo back the magic echo-off characters even if echo is already off
                // Thus regardless of the current state of the echo, a user can
                // and expect to read it back plus \r\n.
                usb_serial_write_string_P(ECHO_OFF_PSTR);
                usb_serial_write_byte('\n');
            } else {
                usb_serial_echo = false;
            }
            break;
        case RESET:
            wdt_enable(WDTO_15MS);
            break;
        case AREF:
            ADMUX = admux_val;
            break;
        case ADD_STEP:
            result = add_program_step(line, &function);
            switch (result) {
                case BAD_FUNC:
                    usb_serial_write_string_P(PSTR("ERROR: Unknown function\n"));
                    break;
                case BAD_PARAM:
                    usb_serial_write_string_P(PSTR("ERROR: Could not parse function parameters\n"));
                    break;
                case NOT_PWM:
                    usb_serial_write_string_P(PSTR("ERROR: Specified pin is not PWM-enabled\n"));
                    break;
                case NOT_ANALOG:
                    usb_serial_write_string_P(PSTR("ERROR: Specified pin cannot be used for analog input\n"));
                    break;
                case NO_ROOM:
                    usb_serial_write_string_P(PSTR("ERROR: Too many function steps\n"));
                    break;
                case NOERR:
                    if (execute_mode == IMMEDIATE){
                        if (function != &loop && function != &goto_ && function != char_goto) {
                            // don't run loops in immediate mode, duh.
                            running = true;
                            run_serial_tasks_from_isr = true;
                            function(program_heap + program_size*HEAP_PER_STEP);
                            run_serial_tasks_from_isr = false;
                            running = false;
                        }
                    } else {
                        program[program_size] = function;
                        program_size++;
                        if (function == &loop) {
                            num_loop_commands++;
                        }
                    }
                    break;
            }
    }
}

err_t add_program_step(char *line, command_t *function_out) {
    // can assume line is null-terminated and is at least 2 chars in length
    if (parse_space_to_end(line)) {
        return NOERR;
    }
    if (strlen(line) < 2) {
        return BAD_FUNC;
    }

    command_t function;
    char *params = line + 2; // at worst, points to null byte terminating the string
    uint8_t *heap_end = program_heap + program_size*HEAP_PER_STEP;
    if (program_size == MAX_PROGRAM_STEPS) {
        return NO_ROOM;
    }
    bool success = true;
    if (strncmp_P(line, PSTR("wh"), 2) == 0) {
        function = &wait_high;
        success = parse_pin(&params, heap_end);
    } else if (strncmp_P(line, PSTR("wl"), 2) == 0) {
        function = &wait_low;
        success = parse_pin(&params, heap_end);
    } else if (strncmp_P(line, PSTR("wc"), 2) == 0) {
        function = &wait_change;
        success = parse_pin(&params, heap_end);
    } else if (strncmp_P(line, PSTR("wt"), 2) == 0) {
        function = &set_wait_time;
        success = parse_uint16(&params, 0x7FFF, heap_end);
        (*(uint16_t *) heap_end) *= 2; // the delay is internally in half-microseconds
    } else if (strncmp_P(line, PSTR("uh"), 2) == 0) {
        function = &undebounced_wait_high;
        success = parse_pin(&params, heap_end);
    } else if (strncmp_P(line, PSTR("ul"), 2) == 0) {
        function = &undebounced_wait_low;
        success = parse_pin(&params, heap_end);
    } else if (strncmp_P(line, PSTR("uc"), 2) == 0) {
        function = &undebounced_wait_change;
        success = parse_pin(&params, heap_end);
    } else if (strncmp_P(line, PSTR("dm"), 2) == 0) {
        function = &delay_milliseconds;
        success = parse_uint16(&params, 0xFFFF, heap_end);
    } else if (strncmp_P(line, PSTR("du"), 2) == 0) {
        function = &delay_microseconds;
        success = parse_uint16(&params, 0x7FFF, heap_end);
        (*(uint16_t *) heap_end) *= 2; // the delay is internally in half-microseconds
    } else if (strncmp_P(line, PSTR("tb"), 2) == 0) {
        function = &timer_begin;
    } else if (strncmp_P(line, PSTR("te"), 2) == 0) {
        function = &timer_end;
    } else if (strncmp_P(line, PSTR("pm"), 2) == 0) {
        success = parse_pin(&params, heap_end);
        if (success) {
            struct pin *pin = pins + *(uint8_t *)(heap_end); // dig out parsed pin number
            if (pin->ocr == NULL) {
                return NOT_PWM;
            }
            heap_end++;
            if (pin->pwm16) {
                function = &pwm16;
                success = parse_uint16(&params, PWM16_MAX, heap_end);
            } else {
                function = &pwm8;
                success = parse_uint8(&params, 255, heap_end);
            }
        }
    } else if (strncmp_P(line, PSTR("sh"), 2) == 0) {
        function = &set_high;
        success = parse_pin(&params, heap_end);
    } else if (strncmp_P(line, PSTR("sl"), 2) == 0) {
        function = &set_low;
        success = parse_pin(&params, heap_end);
    } else if (strncmp_P(line, PSTR("st"), 2) == 0) {
        function = &set_tristate;
        success = parse_pin(&params, heap_end);
    } else if (strncmp_P(line, PSTR("rd"), 2) == 0) {
        function = &read_digital;
        success = parse_pin(&params, heap_end);
    } else if (strncmp_P(line, PSTR("ra"), 2) == 0) {
        function = &read_analog;
        success = parse_pin(&params, heap_end);
        if (success) {
            struct pin *pin = pins + *(uint8_t *)(heap_end); // dig out parsed pin number
            if (!pin->adc_mux_bits) {
                return NOT_ANALOG;
            }
        }
    } else if (strncmp_P(line, PSTR("ct"), 2) == 0) {
        function = &char_transmit;
        success = parse_uint8(&params, 255, heap_end);
    } else if (strncmp_P(line, PSTR("cr"), 2) == 0) {
        function = &char_receive;
    } else if (strncmp_P(line, PSTR("cg"), 2) == 0) {
        function = &char_goto;
    } else if (strncmp_P(line, PSTR("lo"), 2) == 0) {
        function = &loop;
        if (num_loop_commands == MAX_LOOP_COMMANDS) {
            return NO_ROOM;
        }
        success = parse_uint8(&params, (uint8_t) MAX_PROGRAM_STEPS-1, heap_end);
        if (success) {
            *(uint8_t *)++heap_end = num_loop_commands;
            success = parse_uint16(&params, 0xFFFF, loop_initial_values+num_loop_commands);
        }
    } else if (strncmp_P(line, PSTR("go"), 2) == 0) {
        function = &goto_;
        success = parse_uint8(&params, (uint8_t) MAX_PROGRAM_STEPS-1, heap_end);
    } else if (strncmp_P(line, PSTR("no"), 2) == 0) {
        function = &noop;
    } else {
        return BAD_FUNC;
    }

    if (!success || !parse_space_to_end(params)) {
        return BAD_PARAM;
    }
    // if everything worked, return the function
    *function_out = function;
    return NOERR;
}