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

/*
 * LUFA configuration.
 */

#ifndef LUFAConfig_h
#define LUFAConfig_h

#if (ARCH != ARCH_AVR8)
#error "Unknown MCU architecture"
#endif

#define DISABLE_TERMINAL_CODES
#define ORDERED_EP_CONFIG
#define USE_STATIC_OPTIONS (USB_DEVICE_OPT_FULLSPEED | USB_OPT_REG_ENABLED | USB_OPT_AUTO_PLL)
#define USB_DEVICE_ONLY
#define NO_SOF_EVENTS
#define USE_FLASH_DESCRIPTORS
#define FIXED_CONTROL_ENDPOINT_SIZE 8
#define FIXED_NUM_CONFIGURATIONS 1
#define NO_DEVICE_REMOTE_WAKEUP
#define NO_DEVICE_SELF_POWER

#endif
