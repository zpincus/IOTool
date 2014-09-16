/*
 * Copyright 2014 Alan Burlison, alan@bleaklow.com.  All rights reserved.
 * Use is subject to license terms.  See LICENSE.txt for details.
 */

/*
 * Requires and is based in part on example code from:
 *     LUFA Library Copyright (C) Dean Camera, 2014.
 *     dean [at] fourwalledcubicle [dot] com www.lufa-lib.org
 * See License.txt in the LUFA distribution for LUFA license details.
 */

/*
 * LUFA-based CDC-ACM serial port support.  This extends the LUFA implementation
 * by adding basic line discipline handling - CR/NL handling, echoing, backspace
 * handling etc.
 */

#include "usb_serial.h"
#include "utils.h"
#include <avr/pgmspace.h>
/*
 * FILE buffer and line discipline handling.
 */

// Delete character.
#define DEL 0x7F

// IO buffer - circular queue.
typedef struct {
   volatile char     *start;
   volatile char     *end;
   volatile char     *head;
   volatile char     *tail;
   volatile uint8_t len;
} iobuf_t;

// Forward declarations.
static int iobuf_putchar(char c, FILE *restrict file);
static int iobuf_getchar(FILE *restrict file);

// IO handling globals.
volatile uint8_t nextRun;
volatile uint8_t mode;
volatile uint8_t ilines;
volatile uint8_t ticks;

// Output data structure.
static FILE output = {
  .put = &iobuf_putchar,
  .get = NULL,
  .flags = _FDEV_SETUP_WRITE,
  .udata = NULL
};

// Input data structures.
volatile char in[USB_IBUFSZ];
iobuf_t ibuf = { in, in + USB_IBUFSZ - 1, in, in, 0 };
static FILE input = {
  .put = NULL,
  .get = &iobuf_getchar,
  .flags = _FDEV_SETUP_READ,
  .udata = &ibuf
};

// Enqueue a character.  Assumes there is enough space.
static void enqueue(char c, iobuf_t *restrict iobuf)
{
    *iobuf->head++ = c;
    if (iobuf->head > iobuf->end) {
        iobuf->head = iobuf->start;
    }
    iobuf->len++;
}

// Dequeue a character.  Assumes there is one available.
static int dequeue(iobuf_t *restrict iobuf)
{
    char c = *iobuf->tail++;
    if (iobuf->tail > iobuf->end) {
        iobuf->tail = iobuf->start;
    }
    iobuf->len--;
    return c;
}

// Run the LUFA CDC and USB tasks and buffer any pending input.  Performs input
// line discipline handling.
static void run(void)
{
    //     CDC_Device_USBTask(&serialDevice);
    // USB_USBTask();
    // Find out if there is any pending input.  If not, wait for 25 msec.
    uint16_t avail =  CDC_Device_BytesReceived(&serialDevice);
            if (! avail) {
                nextRun = 1;
               // ticks = 0;
                return;
            }

    // Text mode - echo and provide simple line editing.
    if (mode & USB_SERIAL_IN_TEXT) {
        while (avail--) {
            char c = CDC_Device_ReceiveByte(&serialDevice);
            switch (c) {
            // CR or NL - convert both to '\n' but echo "\r\n".
            case '\n':
            case '\r':
                if (ibuf.len < USB_IBUFSZ) {
                    enqueue('\n', &ibuf);
                    ilines++;
                    CDC_Device_SendString(&serialDevice, "\r\n");
 //CDC_Device_Flush(&serialDevice);
//                     CDC_Device_USBTask(&serialDevice);
                } else {
                    SET_MASK_HI(input.flags, __SERR);
                }
                break;
            // Backspace/DEL - roll back the input buffer by one, but not if
            // empty or if the last char was a '\n'.  Echo "\b \b".
            case '\b':
            case DEL:
                if (ibuf.len > 0) {
                    // Back up 1 char, with wraparound.
                    volatile char *p = ibuf.head - 1;
                    if (p < ibuf.start) {
                        p = ibuf.end;
                    }
                    // Only erase the character if it isn't a '\n'.
                    if (*p != '\n') {
                        ibuf.head = p;
                        ibuf.len--;
                        CDC_Device_SendString(&serialDevice, "\b \b");
                        // CDC_Device_Flush(&serialDevice);

                    }
                }
                break;
            // Save and echo the character if there is space (leave 1 for '\n').
            default:
                if (ibuf.len < USB_IBUFSZ - 1) {
                    enqueue(c, &ibuf);
                    CDC_Device_SendByte(&serialDevice, c);
 //CDC_Device_Flush(&serialDevice);
                     //CDC_Device_USBTask(&serialDevice);
                } else {
                    SET_MASK_HI(input.flags, __SERR);
                }
                break;
            }
        }

    // Binary mode - no echoing or editing.
    } else {
        while (avail--) {
            char c = CDC_Device_ReceiveByte(&serialDevice);
            if (ibuf.len < USB_IBUFSZ) {
                // Save the character, update the line count if necessary.
                enqueue(c, &ibuf);
                if (c == '\n') {
                    ilines++;
                }
            } else {
                SET_MASK_HI(input.flags, __SERR);
            }
        }
    }

    // Run again in 1 msec, as there may be more data coming.
      //  nextRun = 1;
    //ticks = 0;
}

// Save a character into the output stream buffer.  Checks line state, blocks
// if necessary.
static int iobuf_putchar(char c, FILE *file)
{
    // Line must be up.
    if (state != UP) {
        SET_MASK_HI(input.flags, __SEOF);
        return EOF;
    }

    // CRLF handling.
    if (mode & USB_SERIAL_OUT_TEXT && c == '\n') {
        if (CDC_Device_SendByte(&serialDevice, '\r')
          != ENDPOINT_READYWAIT_NoError) {
            SET_MASK_HI(input.flags, __SERR);
            return EOF;
        }
    }

    // Output the character.
    if (CDC_Device_SendByte(&serialDevice, c) != ENDPOINT_READYWAIT_NoError) {
        SET_MASK_HI(input.flags, __SERR);
        return EOF;
    }

    return c;
}

// Get a character from the input stream buffer.  Blocks if necessary.
static int iobuf_getchar(FILE *file)
{
    if (state != UP) {
        SET_MASK_HI(output.flags, __SEOF);
        return EOF;
    }

    // Text mode - wait until a whole line is available.
    char c;
    if (mode & USB_SERIAL_IN_TEXT) {
        while (ilines == 0) {
            run();
        }

    // Binary mode - wait until a character is available.
    } else {
        while (ibuf.len == 0) {
            run();
        }
    }

    // Get the next character.
    c = dequeue(&ibuf);
    if (c == '\n') {
        ilines--;
    }
    return c;
}

/*
 * Public API.
 */

// Initialise the virtual serial line using the two streams.
void usb_serial_init(uint8_t iomode, FILE **in, FILE **out) {
    // Initialise avr-libc streams.
    mode = iomode;
    *in  = &input;
    *out = &output;

    // Initialise LUFA.
    USB_Init();
}

// Initialise the virtual serial line using stdio streams.
void usb_serial_init_stdio(uint8_t iomode) {
    usb_serial_init(iomode, &stdin, &stdout);
    stderr = stdout;
}

// Get the number of input characters available.
int usb_serial_input_chars(void)
{
    return ibuf.len;
}

// Get the number of input lines available.
int usb_serial_input_lines(void)
{
    return ilines;
}

void usb_send_string(const char *data) {
    while (*data != '\0') {
        iobuf_putchar(*data++, NULL);
    }
}

void usb_send_string_P(const char *data) {
    while (pgm_read_byte(data) != 0x00) {
        iobuf_putchar(pgm_read_byte(data++));
    }
}


ISR(TIMER0_COMPA_vect)
{
    ticks++;
    if (ticks % 20 == 0) {
        CDC_Device_USBTask(&serialDevice);
        USB_USBTask();        
    }
    
}


/*
 * Start the clock - uses timer0.
 */
void clock_start(void)
{
    //Disable counter 0 - set prescaler to 0.
    TCCR0B = 0;
    
    // Initialize counter 0.
    ticks = 0;
    TCCR0A = BIT(WGM01);     // CTC mode.
    TCNT0 = 0;               // Start from 0.
    OCR0A = 16;             // 16*1024/16 MHz = 1 msec period.
    TIMSK0 = BIT(OCIE0A);    // Enable interrupt on OCR0A compare match.
    
    // Start counter 0 - set prescaler to CLK/1024.
    TCCR0B = BIT(CS02) | BIT(CS00);

}

/*
 * Stop the clock.
 */
void clock_stop(void)
{
    // Disable counter0 - set prescaler to 0.
    TCCR0B = 0;
}



// Run the LUFA USB tasks, if required.
bool usb_serial_run(void)
{
    
    //if (ticks >= nextRun) {
        run();
        return true;
   // }
    return false;
}

