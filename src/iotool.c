// Copyright 2014 Zachary Pincus (zpincus@wustl.edu / zplab.wustl.edu)
// This file is part of IOTool.
// 
// IOTool is free software; you can redistribute it and/or modify
// it under the terms of version 2 of the GNU General Public License as
// published by the Free Software Foundation.

#include "platform.h"
#include "usb_serial.h"
#include "interpreter.h"

int main(void) {
    platform_init();
    usb_serial_init();
    interpreter_init();

    for (;;) {
        interpret_line(usb_serial_read_line());
    }
    
    return 0;
}
