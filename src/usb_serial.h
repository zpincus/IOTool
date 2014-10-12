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
#include <stdbool.h>

//  input buffer size
#ifndef USB_IBUFSZ
#define USB_IBUFSZ 80
#endif

extern bool usb_serial_echo;

void usb_serial_init(void);
int16_t usb_serial_write_byte(uint8_t byte);
void usb_serial_flush(void);
void usb_serial_write_string(const char *data);
void usb_serial_write_string_P(const char *data);
uint8_t usb_serial_wait_byte(void);
bool usb_serial_has_byte(uint8_t *byte_out);
void usb_serial_process_byte(uint8_t byte);
char *usb_serial_read_line(void);

#endif	/* usb_serial_h */
