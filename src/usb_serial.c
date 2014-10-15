// Copyright 2014 Zachary Pincus (zpincus@wustl.edu / zplab.wustl.edu)
// This file is part of IOTool.
// 
// IOTool is free software; you can redistribute it and/or modify
// it under the terms of version 2 of the GNU General Public License as
// published by the Free Software Foundation.

#include "usb_serial.h"
#include <avr/pgmspace.h>

// Delete character.
#define DEL 0x7F

bool usb_serial_echo = true;
volatile bool has_line = false;
char input_buffer[USB_IBUF];
volatile char *buffer_cursor = input_buffer;
char *buffer_end = input_buffer + USB_IBUF;


/*
 * Public API.
 */

void usb_serial_init() {
    // Initialise LUFA.
    USB_Init();
}

int16_t usb_serial_write_byte(uint8_t byte) {
    // Line must be up.
    if (state != UP) {
        return EOF;
    }

    char c = (char) byte;
    // CRLF handling.
    if (c == '\n') {
        if (CDC_Device_SendByte(&serialDevice, '\r') != ENDPOINT_READYWAIT_NoError) {
            return EOF;
        }
    }

    // Output the character.
    if (CDC_Device_SendByte(&serialDevice, c) != ENDPOINT_READYWAIT_NoError) {
        return EOF;
    }
    return c;
}


void usb_serial_process_byte(uint8_t byte) {
    char c = (char) byte;
    switch (c) {
        // CR or NL - convert both to '\n' but echo "\r\n".
        case '\n':
        case '\r':
            if (buffer_cursor < buffer_end) {
                *buffer_cursor++ = '\n';
                has_line = true;
                if (usb_serial_echo) {
                    CDC_Device_SendString(&serialDevice, "\r\n");
                }
            }
            break;
        // Backspace/DEL - roll back the input buffer by one, but not if
        // empty or if the last char was a '\n'.  Echo "\b \b".
        case '\b':
        case DEL:
            if (buffer_cursor > input_buffer) {
                volatile char *p = buffer_cursor - 1;
                // Only erase the character if it isn't a '\n'.
                if (*p != '\n') {
                    buffer_cursor = p;
                    if (usb_serial_echo) {
                        CDC_Device_SendString(&serialDevice, "\b \b");
                    }
                }
            }
            break;
        // Save and echo the character if there is space (leave 1 for '\n').
        default:
            if (buffer_cursor < buffer_end - 1) {
                *buffer_cursor++ = c;
                if (usb_serial_echo) {
                    CDC_Device_SendByte(&serialDevice, c);
                }
            }
            break;
    }
}

void usb_serial_flush(void) {
    CDC_Device_USBTask(&serialDevice);
}

char *usb_serial_read_line(void) {
    while(!has_line) {
        while(state != UP) {
            CDC_Device_USBTask(&serialDevice);
        }
        CDC_Device_USBTask(&serialDevice);
        uint16_t avail = CDC_Device_BytesReceived(&serialDevice);
        while (!has_line && avail--) {
            usb_serial_process_byte(CDC_Device_ReceiveByte(&serialDevice));
        }
    }
    // convert newline to null char to terminate string
    *(buffer_cursor - 1) = '\0';
    buffer_cursor = input_buffer;
    has_line = false;
    // the input buffer can get overwritten if more usb-serial processing is allowed
    // to happen before the consumer of the buffer deals with it. Make sure this isn't
    // the case!!
    return input_buffer;
}

void usb_serial_write_string(const char *data) {
    while (*data != '\0') {
        if (usb_serial_write_byte(*data++) == EOF) {
            return;
        }
    }
    CDC_Device_USBTask(&serialDevice);
}

void usb_serial_write_string_P(const char *data) {
    while (pgm_read_byte(data) != 0x00) {
        if (usb_serial_write_byte(pgm_read_byte(data++)) == EOF) {
            return;
        }
    }
    CDC_Device_USBTask(&serialDevice);
}

uint8_t usb_serial_wait_byte(void) {
    while(state != UP) {
        CDC_Device_USBTask(&serialDevice);
    }
    
    while (!CDC_Device_BytesReceived(&serialDevice)) {
        CDC_Device_USBTask(&serialDevice);
    }
    return (uint8_t) CDC_Device_ReceiveByte(&serialDevice);
}

bool usb_serial_has_byte(uint8_t *byte_out) {
    if (state != UP) {
        return false;
    }
    CDC_Device_USBTask(&serialDevice);
    if (!CDC_Device_BytesReceived(&serialDevice)) {
        return false;
    }
    *byte_out = (uint8_t) CDC_Device_ReceiveByte(&serialDevice);
    return true;
}
