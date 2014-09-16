/*
 * Copyright 2012 Alan Burlison, alan@bleaklow.com.  All rights reserved.
 * Use is subject to license terms.  See LICENSE.txt for details.
 */

/*
 * Requires and is based in part on example code from:
 *     LUFA Library Copyright (C) Dean Camera, 2012.
 *     dean [at] fourwalledcubicle [dot] com www.lufa-lib.org
 * See License.txt in the LUFA distribution for LUFA license details.
 */

/*
 * LUFA-based CDC-ACM serial port support.  This extends the LUFA implementation
 * by adding basic line disciplie handling - CR/NL handling, echoing, backspace
 * handling etc.
 */

#ifndef usb_serial_h
#define	usb_serial_h

#include "usb_serial_base.h"
#include <LUFA/Drivers/USB/USB.h>

// Default input buffer size - can be overriden.
#ifndef USB_IBUFSZ
#define USB_IBUFSZ 80
#endif

// Serial input/output modes.
// Text mode: line buffered, echo input, handle backspace, map NL to CR/NL.
// Binary mode: unbuffered, no echoing, no backspace, no NL mapping.
#define USB_SERIAL_IN_RAW   0x00
#define USB_SERIAL_OUT_RAW  0x00
#define USB_SERIAL_IN_TEXT  0x01
#define USB_SERIAL_OUT_TEXT 0x02
#define USB_SERIAL_RAW      (USB_SERIAL_IN_RAW | USB_SERIAL_OUT_RAW)
#define USB_SERIAL_TEXT     (USB_SERIAL_IN_TEXT | USB_SERIAL_OUT_TEXT)



/*
 * Create a new USB Serial connection, returning two stdio streams.  See above
 * for iomode values.
 */
extern void usb_serial_init(uint8_t iomode, FILE **in, FILE **out);

/*
 * Create a new USB Serial connection, setting stdin, stdout and stderr to the
 * opened streams.  See above for iomode values.
 */
extern void usb_serial_init_stdio(uint8_t iomode);

extern void usb_send_string(const char *data);
extern void usb_send_string_P(const char *data);

/*
 * Return the number of input characters available.
 */
extern int  usb_serial_input_chars(void);

/*
 * Return the number of input lines available.
 */
extern int  usb_serial_input_lines(void);


void clock_start(void);
void clock_stop(void);
bool usb_serial_run(void);


#endif	/* usb_serial_h */
