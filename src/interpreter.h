// Copyright 2014 Zachary Pincus (zpincus@wustl.edu / zplab.wustl.edu)
// This file is part of IOTool.
// 
// IOTool is free software; you can redistribute it and/or modify
// it under the terms of version 2 of the GNU General Public License as
// published by the Free Software Foundation.

#ifndef interpreter_h
#define interpreter_h

#include "utils.h"

void interpreter_init(void);
void interpret_line(char *line);

#define QUIT_BYTE 33 // '!' character
#define MS_TIMER_MASK BIT(OCIE3A)
#define USB_TIMER_MASK BIT(OCIE3C)
#define TIMER3_ENABLE BIT(CS31)
#define TIMER3_DISABLE 0

extern uint8_t program_counter;
extern uint16_t loop_initial_values[];
extern uint16_t loop_current_values[];
extern bool loop_active[];
extern volatile bool run_serial_tasks_from_isr;
extern volatile bool running;
extern volatile uint16_t ms_timer;
extern volatile uint16_t ms_timer_target;
extern volatile bool ms_timer_done;


#endif /* interpreter_h */

