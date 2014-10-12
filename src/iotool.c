/*
 * Copyright 2013 Alan Burlison, alan@bleaklow.com.  All rights reserved.
 * Use is subject to license terms.  See LICENSE.txt for details.
 */

#include "usb_serial.h"
#include "platform.h"
#include "interpreter.h"

void main(void) {
    platform_init();
    usb_serial_init();
    interpreter_init();

    for (;;) {
        interpret_line(usb_serial_read_line());
    }

}
