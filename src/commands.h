// Copyright 2014 Zachary Pincus (zpincus@wustl.edu / zplab.wustl.edu)
// This file is part of IOTool.
// 
// IOTool is free software; you can redistribute it and/or modify
// it under the terms of version 2 of the GNU General Public License as
// published by the Free Software Foundation.

#ifndef commands_h
#define commands_h

typedef void (*command_t)(void *);

void raw_wait_high(void *params);
void raw_wait_low(void *params);
void raw_wait_change(void *params);
void wait_high(void *params);
void wait_low(void *params);
void wait_change(void *params);
void set_wait_time(void *params);
void delay_milliseconds(void *params);
void delay_microseconds(void *params);
void pwm8(void *params);
void pwm16(void *params);
void set_high(void *params);
void set_low(void *params);
void set_tristate(void *params);
void char_receive(void *params);
void char_transmit(void *params);
void loop(void *params);
void goto_(void *params);
void noop(void *params);


#endif /* commands_h */

