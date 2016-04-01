/*
 * Copyright 2012 Alan Burlison, alan@bleaklow.com.  All rights reserved.
 * Use is subject to license terms.  See LICENSE.txt for details.
 */

/*
 * Requires and is based in part on example code from:
 *     LUFA Library Copyright (C) Dean Camera, 2012.
 *     dean [at] fourwalledcubicle [dot] com www.lufa-lib.org
 * See License.txt in the LUFA distribution for LUFA license details.
 */


#ifndef usb_serial_base_h
#define	usb_serial_base_h

#include <LUFA/Drivers/USB/USB.h>


// Serial Device to call serial-minding routines on
extern USB_ClassInfo_CDC_Device_t serialDevice;

// Interface state.
#define CFGD 0x01               // Configured.
#define DTR  0x02               // DTR active.
#define UP   (CFGD | DTR)       // Configured and active.

extern uint8_t state;

#endif	/* usb_serial_base_h */
