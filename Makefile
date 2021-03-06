######################################
# IOTool User-configurable variables #
######################################
# Set LUFA_PATH to the relative or absolute path to the LUFA source directory
LUFA_PATH    = ../lufa-LUFA-140928/LUFA

# If using an Arduino device and you want to use Arduino pin names, uncomment
# the following line:
#ARD_PINS     = -DARDUINO_PIN_NAMES

# Set this to the serial port (/dev/tty/something on Mac/Linux; COMx on Windows)
# that appears when the device is attached and the reset button is pressed.
AVRDUDE_PORT = /dev/tty.usbmodem1411

# Variables that control how the USB serial device enumerates to the host.
# These are especially useful with linux udev rules.
# Note that "SERIAL_NUMBER" is not actually treated as a hex constant --
# these are all pasted into unicode strings by macros.
MANUFACTURER  = zplab.wustl.edu
PRODUCT_NAME  = IOTool
SERIAL_NUMBER = 0xFFFF

#############################
# IOTool Internal Variables #
#############################
MCU          = atmega32u4
ARCH         = AVR8
F_CPU        = 16000000
F_USB        = $(F_CPU)
OPTIMIZATION = 3
TARGET       = IOTool
SRC          = $(wildcard src/*.c) $(LUFA_SRC_USB_DEVICE) $(LUFA_PATH)/Drivers/USB/Class/Device/CDCClassDevice.c
USB_DEFS     = -DMANUFACTURER=$(MANUFACTURER) -DPRODUCT_NAME=$(PRODUCT_NAME) -DSERIAL_NUMBER=$(SERIAL_NUMBER)
CC_FLAGS     = $(ARD_PINS) $(USB_DEFS) -DUSE_LUFA_CONFIG_HEADER -Isrc -Wall -Werror
LD_FLAGS     =
OBJDIR       = build

AVRDUDE_PROGRAMMER = avr109
AVRDUDE_FLAGS      =

# Upload target
upload: all avrdude

# Include LUFA build script makefiles
include $(LUFA_PATH)/Build/lufa_core.mk
include $(LUFA_PATH)/Build/lufa_sources.mk
include $(LUFA_PATH)/Build/lufa_build.mk
include $(LUFA_PATH)/Build/lufa_avrdude.mk

