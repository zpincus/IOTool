/*
 * Copyright 2012 Alan Burlison, alan@bleaklow.com.  All rights reserved.
 * Use is subject to license terms.  See LICENSE.txt for details.
 */

/*
 * Platform configuration.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include "utils.h"
#include "platform.h"

void platform_init(void)
{
    // make sure we're running at full clock
	clock_prescale_set(clock_div_1);

    // Disable watchdog, just in case.
    SET_BIT_LO(MCUSR, WDRF);
    wdt_disable();

    // Enable interrupts.
    sei();
}
