// Copyright 2014 Zachary Pincus (zpincus@wustl.edu / zplab.wustl.edu)
// This file is part of IOTool.
// 
// IOTool is free software; you can redistribute it and/or modify
// it under the terms of version 2 of the GNU General Public License as
// published by the Free Software Foundation.

#include "pins.h"

#ifdef ARDUINO_PIN_NAMES
#define INIT_PIN(_ARD_NAME, _PORT, _PIN, _ADC) {#_ARD_NAME, &PORT##_PORT, &PIN##_PORT, &DDR##_PORT, BIT(PORT##_PORT##_PIN), NULL, false, NULL, 0, _ADC}
#define INIT_PWM_PIN(_ARD_NAME, _PORT, _PIN, _TIMER, _CHANNEL, _16, _TCCR, _ADC) {#_ARD_NAME, &PORT##_PORT, &PIN##_PORT, &DDR##_PORT, BIT(PORT##_PORT##_PIN),\
     &OCR##_TIMER##_CHANNEL, _16, &_TCCR, BIT(COM##_TIMER##_CHANNEL##1), _ADC}
#else
#define INIT_PIN(_ARD_NAME, _PORT, _PIN, _ADC) {#_PORT #_PIN, &PORT##_PORT, &PIN##_PORT, &DDR##_PORT, BIT(PORT##_PORT##_PIN), NULL, false, NULL, 0, _ADC}
#define INIT_PWM_PIN(_ARD_NAME, _PORT, _PIN, _TIMER, _CHANNEL, _16, _TCCR, _ADC) {#_PORT #_PIN, &PORT##_PORT, &PIN##_PORT, &DDR##_PORT, BIT(PORT##_PORT##_PIN),\
     &OCR##_TIMER##_CHANNEL, _16, &_TCCR, BIT(COM##_TIMER##_CHANNEL##1), _ADC}
#endif
#define INIT_PWM8_PIN(_ARD_NAME, _PORT, _PIN, _TIMER, _CHANNEL, _ADC) INIT_PWM_PIN(_ARD_NAME, _PORT, _PIN, _TIMER, _CHANNEL, false, TCCR##_TIMER##A, _ADC)
#define INIT_PWM16_PIN(_ARD_NAME, _PORT, _PIN, _TIMER, _CHANNEL, _ADC) INIT_PWM_PIN(_ARD_NAME, _PORT, _PIN, _TIMER, _CHANNEL, true, TCCR##_TIMER##A, _ADC)

struct pin pins[] = {
    INIT_PIN(SS, B, 0, 0),
    INIT_PIN(SC, B, 1, 0),
    INIT_PIN(MO, B, 2, 0),
    INIT_PIN(MI, B, 3, 0),
    INIT_PIN(8, B, 4, 128+35), // ADC11
    INIT_PWM16_PIN(9, B, 5, 1, A, 128+36), // ADC12
    INIT_PWM16_PIN(10, B, 6, 1, B, 128+37), // ADC13
    INIT_PWM8_PIN(11, B, 7, 0, A, 0),
    INIT_PIN(5, C, 6, 0),
    INIT_PWM8_PIN(13, C, 7, 4, A, 0),
    INIT_PWM8_PIN(3, D, 0, 0, B, 0),
    INIT_PIN(2, D, 1, 0),
    INIT_PIN(RX, D, 2, 0),
    INIT_PIN(TX, D, 3, 0),
    INIT_PIN(4, D, 4, 128+32), // ADC8
    INIT_PIN(TL, D, 5, 0),
    INIT_PIN(12, D, 6, 128+33), // ADC9
    INIT_PWM_PIN(6, D, 7, 4, D, false, TCCR4C, 128+34), // ADC10
    INIT_PIN(7, E, 6, 0),
    INIT_PIN(A5, F, 0, 128+0), // ADC0
    INIT_PIN(A4, F, 1, 128+1), // ADC2
    INIT_PIN(A3, F, 4, 128+4), // ADC4
    INIT_PIN(A2, F, 5, 128+5), // ADC5
    INIT_PIN(A1, F, 6, 128+6), // ADC6
    INIT_PIN(A0, F, 7, 128+7)  // ADC7
};

uint8_t NUM_PINS = ARRAYLEN(pins);