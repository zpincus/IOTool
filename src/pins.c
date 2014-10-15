// Copyright 2014 Zachary Pincus (zpincus@wustl.edu / zplab.wustl.edu)
// This file is part of IOTool.
// 
// IOTool is free software; you can redistribute it and/or modify
// it under the terms of version 2 of the GNU General Public License as
// published by the Free Software Foundation.

#include "pins.h"

#ifdef ARDUINO_PIN_NAMES
#define INIT_PIN(_ARD_NAME, _PORT, _PIN) {#_ARD_NAME, &PORT##_PORT, &PIN##_PORT, &DDR##_PORT, BIT(PORT##_PORT##_PIN), NULL, false, NULL, 0}
#define INIT_PWM_PIN(_ARD_NAME, _PORT, _PIN, _TIMER, _CHANNEL, _16, _TCCR) {#_ARD_NAME, &PORT##_PORT, &PIN##_PORT, &DDR##_PORT, BIT(PORT##_PORT##_PIN),\
     &OCR##_TIMER##_CHANNEL, _16, &_TCCR, BIT(COM##_TIMER##_CHANNEL##1)}
#else
#define INIT_PIN(_ARD_NAME, _PORT, _PIN) {#_PORT #_PIN, &PORT##_PORT, &PIN##_PORT, &DDR##_PORT, BIT(PORT##_PORT##_PIN), NULL, false, NULL, 0}
#define INIT_PWM_PIN(_ARD_NAME, _PORT, _PIN, _TIMER, _CHANNEL, _16, _TCCR) {#_PORT #_PIN, &PORT##_PORT, &PIN##_PORT, &DDR##_PORT, BIT(PORT##_PORT##_PIN),\
     &OCR##_TIMER##_CHANNEL, _16, &_TCCR, BIT(COM##_TIMER##_CHANNEL##1)}
#endif
#define INIT_PWM8_PIN(_ARD_NAME, _PORT, _PIN, _TIMER, _CHANNEL) INIT_PWM_PIN(_ARD_NAME, _PORT, _PIN, _TIMER, _CHANNEL, false, TCCR##_TIMER##A)
#define INIT_PWM16_PIN(_ARD_NAME, _PORT, _PIN, _TIMER, _CHANNEL) INIT_PWM_PIN(_ARD_NAME, _PORT, _PIN, _TIMER, _CHANNEL, true, TCCR##_TIMER##A)

struct pin pins[] = {
    INIT_PIN(SS, B, 0),
    INIT_PIN(SC, B, 1),
    INIT_PIN(MO, B, 2),
    INIT_PIN(MI, B, 3),
    INIT_PIN(8, B, 4),
    INIT_PWM16_PIN(9, B, 5, 1, A),
    INIT_PWM16_PIN(10, B, 6, 1, B),
    INIT_PWM8_PIN(11, B, 7, 0, A),
    INIT_PIN(5, C, 6),
    INIT_PWM8_PIN(13, C, 7, 4, A),
    INIT_PWM8_PIN(3, D, 0, 0, B),
    INIT_PIN(2, D, 1),
    INIT_PIN(RX, D, 2),
    INIT_PIN(TX, D, 3),
    INIT_PIN(4, D, 4),
    INIT_PIN(TL, D, 5),
    INIT_PIN(12, D, 6),
    INIT_PWM_PIN(6, D, 7, 4, D, false, TCCR4C),
    INIT_PIN(7, E, 6),
    INIT_PIN(A5, F, 0),
    INIT_PIN(A4, F, 1),    
    INIT_PIN(A3, F, 4),
    INIT_PIN(A2, F, 5),
    INIT_PIN(A1, F, 6),
    INIT_PIN(A0, F, 7)
};

uint8_t PIN_MAX = ARRAYLEN(pins) - 1;