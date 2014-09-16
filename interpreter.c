#include <avr/io.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <avr/pgmspace.h>

#define PWM16_MAX (uint16_t) 1<<10-1
struct pin {
    volatile uint8_t *const port;    // The output port location.
    volatile uint8_t *const pin;    // The input port location.
    volatile uint8_t *const ddr;     // The data direction location.
    const uint8_t pin_mask; // mask of the relevant bit for that pin
    volatile void *const ocr; // compare register for PWM, null for non-PWM pins
    bool pwm16; // if true, ocr is a 16-bit integer register
    volatile uint8_t *const tccr; // timer control register for connecting and disconnecting PWM
    const uint8_t tccr_mask; // set high to connect the PWM
};

#define INIT_PIN(_PORT, _PIN) { &PORT##_PORT, &PIN##_PORT, &DDR##_PORT, BIT(PORT##_PORT##_PIN), NULL, false, NULL, 0}
#define INIT_PWM_PIN(_PORT, _PIN, _TIMER, _CHANNEL, _16, _TCCR) { &PORT##_PORT, &PIN##_PORT, &DDR##_PORT, BIT(PORT##_PORT##_PIN),\
     &OCR##_TIMER##_CHANNEL, _16, &_TCCR, BIT(COM##_TIMER##_CHANNEL##1)}
#define INIT_PWM8_PIN(_PORT, _PIN, _TIMER, _CHANNEL) INIT_PWM_PIN(_PORT, _PIN, _TIMER, _CHANNEL, false, TCCR##_TIMER##A)
#define INIT_PWM16_PIN(_PORT, _PIN, _TIMER, _CHANNEL) INIT_PWM_PIN(_PORT, _PIN, _TIMER, _CHANNEL, true, TCCR##_TIMER##A)

struct pin pins[] = {
  /* 0  */  INIT_PIN(B, 0),
  /* 1  */  INIT_PIN(B, 1),    
  /* 2  */  INIT_PIN(B, 2),
  /* 3  */  INIT_PIN(B, 3),
  /* 4  */  INIT_PIN(B, 4),
  /* 5  */  INIT_PWM16_PIN(B, 5, 1, A),
  /* 6  */  INIT_PWM16_PIN(B, 6, 1, B),
  /* 7  */  INIT_PWM8_PIN(B, 7, 0, A),
  /* 8  */  INIT_PIN(C, 6),
  /* 9  */  INIT_PWM8_PIN(C, 7, 4, A),
  /* 10 */  INIT_PWM8_PIN(D, 0, 0, B),
  /* 11 */  INIT_PIN(D, 1),    
  /* 12 */  INIT_PIN(D, 2),
  /* 13 */  INIT_PIN(D, 3),
  /* 14 */  INIT_PIN(D, 4),
  /* 15 */  INIT_PIN(D, 5),
  /* 16 */  INIT_PIN(D, 6),
  /* 17 */  INIT_PWM_PIN(D, 7, 4, D, false, TCCR4C),
  /* 18 */  INIT_PIN(E, 6),
  /* 19 */  INIT_PIN(F, 0),
  /* 20 */  INIT_PIN(F, 1),    
  /* 21 */  INIT_PIN(F, 4),
  /* 22 */  INIT_PIN(F, 5),
  /* 23 */  INIT_PIN(F, 6),
  /* 24 */  INIT_PIN(F, 7)
}

#define SET_PIN_LOW(_PIN_IDX, _REGISTER) SET_MASK_LO(*(pins[_PIN_IDX]._REGISTER), pins[_PIN_IDX].pin_mask)
#define SET_PIN_HIGH(_PIN_IDX, _REGISTER) SET_MASK_HI(*(pins[_PIN_IDX]._REGISTER), pins[_PIN_IDX].pin_mask)
#define GET_PIN(_PIN_IDX, _REGISTER) GET_MASK(*(pins[_PIN_IDX]._REGISTER), pins[_PIN_IDX].pin_mask)
#define ENABLE_PWM(_PIN_IDX) SET_MASK_HI(*(pins[_PIN_IDX].tccr), pins[_PIN_IDX].tccr_mask)
#define DISABLE_PWM(_PIN_IDX) SET_MASK_LO(*(pins[_PIN_IDX].tccr), pins[_PIN_IDX].tccr_mask)

volatile bool run_serial_tasks_from_isr;
volatile bool quit_run;
volatile uint16_t ms_timer;
volatile uint16_t ms_timer_target;
volatile bool ms_timer_done;

ISR(TIMER3_COMPA_vect) {
    ms_timer++; // add one ms
    if (ms_timer == ms_timer_target) {
        ms_timer_done = true;
    }
    OCR3A += 2000; // fire ISR again in 1 ms (wraparound expected; works great)
}

ISR(TIMER3_COMPC_vect) {
    if (run_serial_tasks_from_isr) {
        CDC_Device_USBTask(&serialDevice);
    }
    USB_USBTask();        
    OCR3C += 30000; // fire ISR again in 15 ms (wraparound expected; works great)
}

MS_TIMER_MASK = BIT(OCIE3A);
USB_TIMER_MASK = BIT(OCIE3C);

void init_clocks(void) {
    // initialize ISR-related shared variables
    ms_timer = 0;
    
    // Timer/Counter0
    TCCR0A = BIT(WGM00) | BIT(WGM01); // Fast 8-bit PWM (freq=62.5 kHz), no outputs connected
    TIMSK0 = 0; // no interrupts
    TCCR0B = BIT(CS00); // start clock, no prescaling (freq=16 MHz, period=62.5 ns)
    
    // Timer/Counter1
    TCCR1A = BIT(WGM11); // set WGM to mode 14: fast PWM; TOP of counter defined by ICR1
    TCCR1B = BIT(WGM13) | BIT(WGM12);
    ICR1 = PWM16_MAX;
    TIMSK1 = 0;// no interrupts
    TCCR1B |= BIT(CS10); // start clock, no prescaling (freq=16 MHz, period=62.5 ns)

    // Timer/Counter3
    // USB should be initialized first; this turns on the USB-handling ISR
    TCCR3A = 0; // Normal mode
    OCR3C = 0; // fire off USB timer right away once enabled
    TIMSK3 = USB_TIMER_MASK; // interrupt on OCR3C for usb tasks (we'll use OCR3A and OCR3B for millisecond and microsecond timing)
    TIFR3 = 0; // make sure no interrupts are queued
    TCCR3B = BIT(CS31); // start clock, prescaler=8 (freq=2 MHz, period=0.5 microseconds)
    
    // Timer/Counter4
    TCCR4A = BIT(PWM4A); // Enable PWM on OCR4A
    TCCR4C = BIT(PWM4D); // Enable PWM on OCR4D
    TCCR4D = 0; // Fast PWM (WGM bits zero)
    OCR4C = 1<<8; // use 8-bit PWM
    TCCR4B = BIT(CS41) // start clock, prescaler=2 (freq=8 MHz, period=125 ns)
}


void wait_high(void *params) {
    uint8_t pin_number = * (uint8_t *) params;
    SET_PIN_LOW(pin_number, ddr); // set pin for input
    SET_PIN_HIGH(pin_number, port); // enable pullup resistor
    while (!GET_PIN(pin_number, pin) && !quit_run) {}
}

void wait_low(void *params) {
    uint8_t pin_number = * (uint8_t *) params;
    SET_PIN_LOW(pin_number, ddr); // set pin for input
    SET_PIN_HIGH(pin_number, port); // enable pullup resistor
    while (GET_PIN(pin_number, pin) && !quit_run) {}
}

void wait_change(void *params) {
    uint8_t pin_number = * (uint8_t *) params;
    SET_PIN_LOW(pin_number, ddr); // set pin for input
    uint8_t current = GET_PIN(pin_number, pin);
    while (GET_PIN(pin_number, pin) != current && !quit_run) {}
}

void delay_milliseconds(void *params) {
    uint16_t ms_delay = * (uint16_t) params;
    ms_timer = 0;
    ms_timer_target = ms_delay;
    ms_timer_done = false;
    TIMSK3 |= MS_TIMER_MASK; // enable millisecond timer
    while (!ms_timer_done && !quit_run) {}
    TIMSK3 &= ~MS_TIMER_MASK; // enable millisecond timer
}

void delay_microseconds(void *params) {
    uint16_t us_delay = * (uint16_t) params;
    TIMSK3 = 0; // no USB interrupts; won't get "quit" signal
    TIFR3 = BIT(OCF3B) // clear any timer-match flags present
    OCR3B = TCNT3 + us_delay; // set up match time (wraparound expected; works great)
    while (!(TIFR3 & BIT(OCF3B))) {}
    TIMSK3 = USB_TIMER_MASK
}

void pwm8(void *params) {
    uint8_t pin_number = * (uint8_t *) params;
    uint8_t pwm_value = * (uint8_t *) (params + 1);
    SET_PIN_HIGH(pin_number, ddr); // set pin for output
    *((volatile uint16_t *) pins[pin_number].ocr) = pwm_value;
    ENABLE_PWM(pin_number);
}

void pwm16(void *params) {
    uint8_t pin_number = * (uint8_t *) params;
    uint16_t pwm_value = * (uint16_t *) (params + 1);
    SET_PIN_HIGH(pin_number, ddr); // set pin for output
    *((volatile uint16_t *) pins[pin_number].ocr) = pwm_value;
    ENABLE_PWM(pin_number);
}

void set_high(void *params) {
    uint8_t pin_number = * (uint8_t *) params;
    if (pins[pin_number].ocr != NULL) {
        DISABLE_PWM(pin_number);
    }
    SET_PIN_HIGH(pin_number, ddr); // set pin for output
    SET_PIN_HIGH(pin_number, port); // set pin for output
}

void set_low(void *params) {
    uint8_t pin_number = * (uint8_t *) params;
    if (pins[pin_number].ocr != NULL) {
        DISABLE_PWM(pin_number);
    }
    SET_PIN_HIGH(pin_number, ddr); // set pin for output
    SET_PIN_LOW(pin_number, port); // set pin for output
}

void set_tristate(void *params) {
    uint8_t pin_number = * (uint8_t *) params;
    if (pins[pin_number].ocr != NULL) {
        DISABLE_PWM(pin_number);
    }
    SET_PIN_LOW(pin_number, ddr); // set pin for input
    SET_PIN_LOW(pin_number, port); // disable pullup resistor
}

void char_receive(void *params) {
    run_serial_tasks_from_isr = false; // we'll do this ourselves
    while (!CDC_Device_BytesReceived(&serialDevice)) {
        CDC_Device_USBTask(&serialDevice);
    }
    uint8_t input = CDC_Device_ReceiveByte(&serialDevice);
    if (input == QUIT_BYTE) {
        quit_run = true;
    }
    // otherwise discard input
    run_serial_tasks_from_isr = true;
}

void char_transmit(void *params) {
    uint8_t output = * (uint8_t *) params;
    usb_putchar_raw(output);
    CDC_Device_USBTask(&serialDevice);
}

void loop(void *params) {
    uint8_t goto_index = * (uint8_t *) params;
    uint16_t *num_times = (uint8_t *) (params + 1);
    if (*num_times > 0) {
        *num_times--;
        program_counter = goto_index;
    }
}

typedef void (function_t)(void *);
function_t program[256];
uint8_t program_heap[768];
void *heap_pointers[256];
uint8_t program_size;
void *heap_end;
uint8_t program_counter;

void clear_program(void) {
    program_size = 0;
    heap_end = (void *) program_heap;
}

void run_program(uint16_t loops) {
    uint16_t i;
    quit_run = false;
    for (i = 0; i < loops; i++) {
        program_counter = 0;
        while (program_counter < program_size) {
            if (quit_run) {
                return;
            }
            uint8_t current_pc = program_counter;
            program_counter++; // increment first to allow functions to manipulate the PC.
            program[current_pc](heap_pointers[current_pc]);
        }
        
    }
}


bool parse_uint8(char *in, uint8_t max, void **dst_p, char **after) {
    unsigned long ulpin = strtoul(in, after, 10);
    if (errno || ulpin > max) {
        return false;
    }
    uint8_t *dst = (uint8_t *) *dst_p;
    *dst = (uint8_t) ulpin;
    *dst_p++;
    return true;
}


bool parse_uint16(char *in, uint16_t max, void **dst_p, char **after) {
    unsigned long ulpin = strtoul(in, after, 10);
    if (errno || ulpin > max) {
        return false;
    }
    uint16_t *dst = (uint16_t *) *dst_p;
    *dst = (uint16_t) ulpin;
    *dst_p += 2; // added two bytes
    return true;
}

bool parse_space_to_end(char *in) {
    while (*in != '\0') {
        if (!isspace(*in++)) {
            return false;
        }
        return true;
    }
}

typedef enum (NOERR, BAD_FUNC, BAD_PARAM, NOT_PWM) err_t;
typedef enum (CLEAR, RUN, ADD_STEP) input_action_t;

void run_input_line(char *line) {
    // can assume line is null-terminated
    if (parse_space_to_end(line)) {
        return;
    }
    
    uint8_t line_len = strlen(line);
    if (line_len < 2) {
        usb_send_string_P(PSTR("Invalid input\n"))
    }
    char *params = line + 2; // at worst, points to null byte terminating the string
    char *after = params;
    if (strncmp_P(line, PSTR("wh"), 2) == 0) {
        function = wait_high;
        success = parse_uint8(params, ARRAYLEN(pins)-1, &new_heap_end, &after);
    } else if (strncmp_P(line, PSTR("wl"), 2) == 0) {


    uint8_t parsed[8];
    if (c1 == 'c' && c2 == 'l') { // clear
        to_call = CLEAR;
    } else if (c1 == 'r' && c2 == 'n') { // run
        to_call = RUN;
        success = parse_uint16(params, 0xFFFF, &(void *) parsed, &after);
    } else {
        return BAD_FUNC;
    }
    if !(success && parse_space_to_end(after)) {
        return BAD_PARAM;
    }
    switch (to_call) {
        case CLEAR:
            clear_program();
            break;
        case RUN:
            uint16_t loops = *(uint16_t *)parsed;
            run_program(loops);
            break;
    }
    return NOERR;
}

err_t add_program_step(char *line) {
    // can assume line is null-terminated and is at least 2 chars in length
    if (parse_space_to_end(line)) {
        return NOERR;
    }
    function_t function;

    char *params = line + 2; // at worst, points to null byte terminating the string
    char *after = params;
    void *new_heap_end = heap_end;
    bool success = true;
    if (strncmp_P(line, PSTR("wh"), 2) == 0) {
        function = wait_high;
        success = parse_uint8(params, ARRAYLEN(pins)-1, &new_heap_end, &after);
    } else if (strncmp_P(line, PSTR("wl"), 2) == 0) {
        function = wait_low;
        success = parse_uint8(params, ARRAYLEN(pins)-1, &new_heap_end, &after);
    } else if (strncmp_P(line, PSTR("wc"), 2) == 0) {
        function = wait_change;
        success = parse_uint8(params, ARRAYLEN(pins)-1, &new_heap_end, &after);
    } else if (strncmp_P(line, PSTR("dm"), 2) == 0) {
        function = delay_milliseconds;
        success = parse_uint16(params, 0xFFFF, &new_heap_end, &after));
    } else if (strncmp_P(line, PSTR("du"), 2) == 0) {
        function = delay_microseconds;
        success = parse_uint16(params, 0xFFFF, &new_heap_end, &after));
    } else if (strncmp_P(line, PSTR("pm"), 2) == 0) {
        success = parse_uint8(params, ARRAYLEN(pins)-1, &new_heap_end, &after);
        if (!success) {
            return BAD_PARAM;
        }
        params = after;
        struct pin *pin = pins + *(uint8_t *)(new_heap_end-1); // dig out parsed pin number
        if (pin->ocr == NULL) {
            return NOT_PWM;
        }
        if (pin->pwm16) {
            function = pwm16;
            success = parse_uint16(params, PWM16_MAX, &new_heap_end, &after);
        } else {
            function = pwm8;
            success = parse_uint8(params, 255, &new_heap_end, &after);
    } else if (strncmp_P(line, PSTR("sh"), 2) == 0) {
        function = set_high;
        success = parse_uint8(params, ARRAYLEN(pins)-1, &new_heap_end, &after);
    } else if (strncmp_P(line, PSTR("sl"), 2) == 0) {
        function = set_low;
        success = parse_uint8(params, ARRAYLEN(pins)-1, &new_heap_end, &after);
    } else if (strncmp_P(line, PSTR("st"), 2) == 0) {
        function = set_tristate;
        success = parse_uint8(params, ARRAYLEN(pins)-1, &new_heap_end, &after);
    } else if (strncmp_P(line, PSTR("ct"), 2) == 0) {
        function = char_transmit;
        success = parse_uint8(params, 255, &new_heap_end, &after);
    } else if (strncmp_P(line, PSTR("cr"), 2) == 0) {
        function = char_receive;
    } else if (strncmp_P(line, PSTR("lo"), 2) == 0) {
        function = loop;
        success = parse_uint8(params, 255, &new_heap_end, &after);
        if (!success) {
            return BAD_PARAM;
        }
        params = after;
        success = parse_uint16(params, 0xFFFF, &new_heap_end, &after);
    } else {
        return BAD_FUNC;
    }
        
    if !(success && parse_space_to_end(after)) {
        return BAD_PARAM;
    }
    // if everything worked, add the function to the list
    program[program_size] = function;
    heap_pointers[program_size] = heap_end;
    program_size++;
    heap_end = new_heap_end;
    return NOERR;
}