// Copyright 2014 Zachary Pincus (zpincus@wustl.edu / zplab.wustl.edu)
// This file is part of IOTool.
// 
// IOTool is free software; you can redistribute it and/or modify
// it under the terms of version 2 of the GNU General Public License as
// published by the Free Software Foundation.

#ifndef usb_serial_h
#define	usb_serial_h

#include "usb_serial_base.h"
#include <LUFA/Drivers/USB/USB.h>
#include <stdbool.h>

//  input buffer size
#ifndef USB_IBUF
#define USB_IBUF 80
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
