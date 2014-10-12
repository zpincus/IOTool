# Run "make help" for target help.

MCU          = atmega32u4
ARCH         = AVR8
F_CPU        = 16000000
F_USB        = $(F_CPU)
OPTIMIZATION = s
TARGET       = IOTool
SRC          = interpreter.c main.c platform.c usb_serial.c usb_serial_base.c $(LUFA_SRC_USB_DEVICE) $(LUFA_SRC_USBCLASS_DEVICE)
LUFA_PATH    = ../../lufa-LUFA-140302/LUFA
CC_FLAGS     = -DUSE_LUFA_CONFIG_HEADER
LD_FLAGS     =
OBJDIR       = build

AVRDUDE_PROGRAMMER = avr109
AVRDUDE_PORT = /dev/tty.usbmodemfd121

# Default target
all:

upload: avrdude

# Include LUFA build script makefiles
include $(LUFA_PATH)/Build/lufa_core.mk
include $(LUFA_PATH)/Build/lufa_sources.mk
include $(LUFA_PATH)/Build/lufa_build.mk
include $(LUFA_PATH)/Build/lufa_avrdude.mk

