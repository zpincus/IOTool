// Copyright 2014 Zachary Pincus (zpincus@wustl.edu / zplab.wustl.edu)
// This file is part of IOTool.
// 
// IOTool is free software; you can redistribute it and/or modify
// it under the terms of version 2 of the GNU General Public License as
// published by the Free Software Foundation.

#ifndef pins_h
#define pins_h

#include "utils.h"

struct pin {
    char name[3];
    volatile uint8_t *const port;    // The output port location.
    volatile uint8_t *const pin;    // The input port location.
    volatile uint8_t *const ddr;     // The data direction location.
    const uint8_t pin_mask; // mask of the relevant bit for that pin
    volatile void *const ocr; // compare register for PWM, null for non-PWM pins
    bool pwm16; // if true, ocr is a 16-bit integer register
    volatile uint8_t *const tccr; // timer control register for connecting and disconnecting PWM
    const uint8_t tccr_mask; // set high to connect the PWM
};

extern struct pin pins[];
extern uint8_t PIN_MAX;

#define SET_PIN_LOW(_PIN_IDX, _REGISTER) SET_MASK_LO(*(pins[_PIN_IDX]._REGISTER), pins[_PIN_IDX].pin_mask)
#define SET_PIN_HIGH(_PIN_IDX, _REGISTER) SET_MASK_HI(*(pins[_PIN_IDX]._REGISTER), pins[_PIN_IDX].pin_mask)
#define GET_PIN(_PIN_IDX, _REGISTER) GET_MASK(*(pins[_PIN_IDX]._REGISTER), pins[_PIN_IDX].pin_mask)
#define ENABLE_PWM(_PIN_IDX) SET_MASK_HI(*(pins[_PIN_IDX].tccr), pins[_PIN_IDX].tccr_mask)
#define DISABLE_PWM(_PIN_IDX) SET_MASK_LO(*(pins[_PIN_IDX].tccr), pins[_PIN_IDX].tccr_mask)

#endif /* pins_h */
