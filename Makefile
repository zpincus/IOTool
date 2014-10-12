# Copyright 2012 Alan Burlison, alan@bleaklow.com.  All rights reserved.
# Use is subject to license terms.  See LICENSE.txt for details.

MCU = atmega32u4
F_CPU = 16000000L
PROGRAMMER = avrdude

INC_DIRS = \
    ../../lib \
    ../../lib/LUFA \
    .
LIB_DIRS = \
    ../../lib/LUFA/Drivers/USB/Core \
    ../../lib/LUFA/Drivers/USB/Core/AVR8 \
    ../../lib/LUFA/Drivers/USB/Class/Device
EXTRA_FLAGS = -DBAUD_TOL=3 \
    -DF_USB=$(F_CPU) -DUSE_LUFA_CONFIG_HEADER -DARCH=ARCH_AVR8

GCC_HOME = /usr/local/CrossPack-AVR

AVRDUDE_PORT = /dev/tty.usbmodemfd121
MONITOR_PORT = /dev/tty.usbmodem0xFFF1
AVRDUDE_TYPE = avr109

include Makefile.master
