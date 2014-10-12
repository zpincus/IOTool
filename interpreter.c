#include <avr/io.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <avr/pgmspace.h>
#include "interpreter.h"
#include "usb_serial.h"
#include "utils.h"

#define PWM16_MAX (uint16_t) (1<<10)-1
struct pin {
    char name[3];
    volatile uint8_t *const port;    // The output port location.
    volatile uint8_t *const pin;    // The input port location.
    volatile uint8_t *const ddr;     // The data direction location.
    const uint8_t pin_mask; // mask of the relevant bit for that pin
    volatile void *const ocr; // compare register for PWM, null for non-PWM pins
    bool pwm16; // if true, ocr is a 16-bit integer register
    volatile uint8_t *const tccr; // timer control register for connecting and disconnecting PWM
    const uint8_t tccr_mask; // set high to connect the PWM
};

#ifdef ARDUINO_PIN_NAMES
#define INIT_PIN(_ARD_NAME, _PORT, _PIN) {#_ARD_NAME, &PORT##_PORT, &PIN##_PORT, &DDR##_PORT, BIT(PORT##_PORT##_PIN), NULL, false, NULL, 0}
#define INIT_PWM_PIN(_ARD_NAME, _PORT, _PIN, _TIMER, _CHANNEL, _16, _TCCR) {#_ARD_NAME, &PORT##_PORT, &PIN##_PORT, &DDR##_PORT, BIT(PORT##_PORT##_PIN),\
     &OCR##_TIMER##_CHANNEL, _16, &_TCCR, BIT(COM##_TIMER##_CHANNEL##1)}
#else
#define INIT_PIN(_ARD_NAME, _PORT, _PIN) {#_PORT #_PIN, &PORT##_PORT, &PIN##_PORT, &DDR##_PORT, BIT(PORT##_PORT##_PIN), NULL, false, NULL, 0}
#define INIT_PWM_PIN(_ARD_NAME, _PORT, _PIN, _TIMER, _CHANNEL, _16, _TCCR) {#_PORT #_PIN, &PORT##_PORT, &PIN##_PORT, &DDR##_PORT, BIT(PORT##_PORT##_PIN),\
     &OCR##_TIMER##_CHANNEL, _16, &_TCCR, BIT(COM##_TIMER##_CHANNEL##1)}
#endif
#define INIT_PWM8_PIN(_ARD_NAME, _PORT, _PIN, _TIMER, _CHANNEL) INIT_PWM_PIN(_ARD_NAME, _PORT, _PIN, _TIMER, _CHANNEL, false, TCCR##_TIMER##A)
#define INIT_PWM16_PIN(_ARD_NAME, _PORT, _PIN, _TIMER, _CHANNEL) INIT_PWM_PIN(_ARD_NAME, _PORT, _PIN, _TIMER, _CHANNEL, true, TCCR##_TIMER##A)

struct pin pins[] = {
    INIT_PIN(SS, B, 0),
    INIT_PIN(SC, B, 1),
    INIT_PIN(MO, B, 2),
    INIT_PIN(MI, B, 3),
    INIT_PIN(8, B, 4),
    INIT_PWM16_PIN(9, B, 5, 1, A),
    INIT_PWM16_PIN(10, B, 6, 1, B),
    INIT_PWM8_PIN(11, B, 7, 0, A),
    INIT_PIN(5, C, 6),
    INIT_PWM8_PIN(13, C, 7, 4, A),
    INIT_PWM8_PIN(3, D, 0, 0, B),
    INIT_PIN(2, D, 1),
    INIT_PIN(RX, D, 2),
    INIT_PIN(TX, D, 3),
    INIT_PIN(4, D, 4),
    INIT_PIN(TL, D, 5),
    INIT_PIN(12, D, 6),
    INIT_PWM_PIN(6, D, 7, 4, D, false, TCCR4C),
    INIT_PIN(7, E, 6),
    INIT_PIN(A5, F, 0),
    INIT_PIN(A4, F, 1),    
    INIT_PIN(A3, F, 4),
    INIT_PIN(A2, F, 5),
    INIT_PIN(A1, F, 6),
    INIT_PIN(A0, F, 7)
};

#define PIN_MAX ARRAYLEN(pins)-1

#define SET_PIN_LOW(_PIN_IDX, _REGISTER) SET_MASK_LO(*(pins[_PIN_IDX]._REGISTER), pins[_PIN_IDX].pin_mask)
#define SET_PIN_HIGH(_PIN_IDX, _REGISTER) SET_MASK_HI(*(pins[_PIN_IDX]._REGISTER), pins[_PIN_IDX].pin_mask)
#define GET_PIN(_PIN_IDX, _REGISTER) GET_MASK(*(pins[_PIN_IDX]._REGISTER), pins[_PIN_IDX].pin_mask)
#define ENABLE_PWM(_PIN_IDX) SET_MASK_HI(*(pins[_PIN_IDX].tccr), pins[_PIN_IDX].tccr_mask)
#define DISABLE_PWM(_PIN_IDX) SET_MASK_LO(*(pins[_PIN_IDX].tccr), pins[_PIN_IDX].tccr_mask)

volatile bool run_serial_tasks_from_isr = false;
volatile bool running;
volatile uint16_t ms_timer = 0;
volatile uint16_t ms_timer_target;
volatile bool ms_timer_done;
#define QUIT_BYTE 33 // '!' character
const char ECHO_OFF_PSTR[] PROGMEM = "\x80\xff";

#define MAX_PROGRAM_STEPS 256
#define HEAP_PER_STEP 3
#define MAX_LOOP_COMMANDS 10

uint16_t steady_wait_time_half_us = 20;

typedef void (*function_t)(void *);
function_t program[MAX_PROGRAM_STEPS];
uint8_t program_heap[MAX_PROGRAM_STEPS*HEAP_PER_STEP];
uint16_t program_size = 0; // must be uint16 to be able to hold the value of 256, indicating that program is full
uint8_t program_counter;
uint16_t loop_current_values[MAX_LOOP_COMMANDS];
uint16_t loop_initial_values[MAX_LOOP_COMMANDS];
uint8_t num_loop_commands = 0;
typedef enum {IMMEDIATE, ON_RUN} mode_t;
mode_t execute_mode = IMMEDIATE;

ISR(TIMER3_COMPA_vect) {
    if (ms_timer == ms_timer_target) {
        ms_timer_done = true;
    }
    ms_timer++; // add one ms
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

#define MS_TIMER_MASK BIT(OCIE3A)
#define USB_TIMER_MASK BIT(OCIE3C)
#define TIMER3_ENABLE BIT(CS31)
#define TIMER3_DISABLE 0

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
}


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
            loop_current_values[l] = loop_initial_values[l];
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
    for (uint8_t i = 0; i < PIN_MAX; i++) {        
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

typedef enum {NOERR, BAD_FUNC, BAD_PARAM, NOT_PWM, NO_ROOM} err_t;
typedef enum {PROGRAM, END, RUN, ADD_STEP, ECHO_OFF} input_action_t;

// forwared decl
err_t add_program_step(char *line, function_t *function_out);

void interpret_line(char *line) {
    // can assume line is null-terminated
    if (parse_space_to_end(line)) {
        return;
    }
    
    input_action_t action;
    bool success = true;
    uint16_t num_iters = 0;
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
    } else {
        action = ADD_STEP;
    }

    if (action != ADD_STEP && (!success || !parse_space_to_end(rest))) {
        usb_serial_write_string_P(PSTR("ERROR: Invalid input\n"));
        return;
    }
    
    switch (action) {
        err_t result;
        function_t function;
        case RUN:
            run_program(num_iters);
            usb_serial_write_string_P(PSTR("DONE\n"));
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
                usb_serial_write_string_P(ECHO_OFF_PSTR);
            } else {
                usb_serial_echo = false;
            }
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
                case NO_ROOM:
                    usb_serial_write_string_P(PSTR("ERROR: Too many function steps\n"));
                    break;
                case NOERR:
                    if (execute_mode == IMMEDIATE){
                        if (function != &loop && function != &goto_) {
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

err_t add_program_step(char *line, function_t *function_out) {
    // can assume line is null-terminated and is at least 2 chars in length
    if (parse_space_to_end(line)) {
        return NOERR;
    }
    if (strlen(line) < 2) {
        return BAD_FUNC;
    }
    
    function_t function;
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
    } else if (strncmp_P(line, PSTR("rh"), 2) == 0) {
        function = &raw_wait_high;
        success = parse_pin(&params, heap_end);
    } else if (strncmp_P(line, PSTR("rl"), 2) == 0) {
        function = &raw_wait_low;
        success = parse_pin(&params, heap_end);
    } else if (strncmp_P(line, PSTR("rc"), 2) == 0) {
        function = &raw_wait_change;
        success = parse_pin(&params, heap_end);
    } else if (strncmp_P(line, PSTR("dm"), 2) == 0) {
        function = &delay_milliseconds;
        success = parse_uint16(&params, 0xFFFF, heap_end);
    } else if (strncmp_P(line, PSTR("du"), 2) == 0) {
        function = &delay_microseconds;
        success = parse_uint16(&params, 0x7FFF, heap_end);
        (*(uint16_t *) heap_end) *= 2; // the delay is internally in half-microseconds
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
    } else if (strncmp_P(line, PSTR("ct"), 2) == 0) {
        function = &char_transmit;
        success = parse_uint8(&params, 255, heap_end);
    } else if (strncmp_P(line, PSTR("cr"), 2) == 0) {
        function = &char_receive;
    } else if (strncmp_P(line, PSTR("lo"), 2) == 0) {
        function = &loop;
        success = parse_uint8(&params, (uint8_t) MAX_PROGRAM_STEPS-1, heap_end);
        if (success) {
            if (num_loop_commands == MAX_LOOP_COMMANDS) {
                return NO_ROOM;
            }
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