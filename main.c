/*
 * Copyright 2013 Alan Burlison, alan@bleaklow.com.  All rights reserved.
 * Use is subject to license terms.  See LICENSE.txt for details.
 */

#include <stdio.h>
#include <ctype.h>
#include <LUFA/Drivers/USB/USB.h>
#include "usb_serial.h"
#include "platform.h"
    extern USB_ClassInfo_CDC_Device_t serialDevice;

void main(void) {
    init();
    usb_serial_init_stdio(USB_SERIAL_IN_TEXT | USB_SERIAL_OUT_TEXT);
    clock_start();
    for (;;) {
        CDC_Device_USBTask(&serialDevice);
        
        usb_serial_run();
        
        if (usb_serial_input_lines()) {
            putchar(':');
            char c = getchar();
            while (c != '\n') {
                putchar(c);                
                c = getchar();
            }
            putchar('\n');
            
        }
    }

}
