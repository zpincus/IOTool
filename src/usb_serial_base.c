/*
 * Requires and is based in part on example code from:
 *     LUFA Library Copyright (C) Dean Camera, 2014.
 *     dean [at] fourwalledcubicle [dot] com www.lufa-lib.org
 * See License.txt in the LUFA distribution for LUFA license details.
 * 
 * Based in part on code copyright  2014 Alan Burlison, alan@bleaklow.com.
 * Use is subject to license terms. See LICENSE.txt for details.
 */

/*
 * LUFA-based CDC-ACM serial port setup.
 */

#include "usb_serial_base.h"
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "utils.h"

/*
 * USB CDC device configuration and management.
 */

// Address of the device-to-host notification in endpoint.
#define NOTIFY_ENDPOINT (ENDPOINT_DIR_IN | 2)

// Address of the device-to-host data in endpoint.
#define TRANSMIT_ENDPOINT (ENDPOINT_DIR_IN | 3)

// Address of the host-to-device data out endpoint.
#define RECEIVE_ENDPOINT (ENDPOINT_DIR_OUT | 4)

// Size in bytes of the notification in endpoint.
#define NOTIFY_ENDPOINT_SIZE 8

// Size in bytes of the data in and out endpoints.
#define TXRX_ENDPOINT_SIZE 64


// Type define for the device configuration descriptor structure.
typedef struct {
    USB_Descriptor_Configuration_Header_t config;
    // CDC Control Interface.
    USB_Descriptor_Interface_t CCIInterface;
    USB_CDC_Descriptor_FunctionalHeader_t functionalHeader;
    USB_CDC_Descriptor_FunctionalACM_t functionalACM;
    USB_CDC_Descriptor_FunctionalUnion_t CDCFunctionalUnion;
    USB_Descriptor_Endpoint_t notificationEndpoint;
    // CDC Data Interface.
    USB_Descriptor_Interface_t DCIInterface;
    USB_Descriptor_Endpoint_t outputEndpoint;
    USB_Descriptor_Endpoint_t inputEndpoint;
} USB_Descriptor_Configuration_t;

// Device descriptor.
static const USB_Descriptor_Device_t PROGMEM deviceDescriptor = {
    .Header = {
        .Size = sizeof(USB_Descriptor_Device_t),
        .Type = DTYPE_Device
    },
    .USBSpecification = VERSION_BCD(1, 1, 0),
    .Class = CDC_CSCP_CDCClass,
    .SubClass = CDC_CSCP_NoSpecificSubclass,
    .Protocol = CDC_CSCP_NoSpecificProtocol,
    .Endpoint0Size = FIXED_CONTROL_ENDPOINT_SIZE,
    .VendorID = 0x03EB, // LUFA private-use VID
    .ProductID = 0x2044, // LUFA CDC Demo Application PID
    .ReleaseNumber = VERSION_BCD(0, 0, 1),
    .ManufacturerStrIndex = 0x01,
    .ProductStrIndex = 0x02,
    .SerialNumStrIndex = 0x03,
    .NumberOfConfigurations = FIXED_NUM_CONFIGURATIONS
};

// Configuration descriptor.
static const USB_Descriptor_Configuration_t PROGMEM configurationDescriptor = {
    .config = {
        .Header = {
            .Size = sizeof(USB_Descriptor_Configuration_Header_t),
            .Type = DTYPE_Configuration
        },
        .TotalConfigurationSize = sizeof(USB_Descriptor_Configuration_t),
        .TotalInterfaces = 2,
        .ConfigurationNumber = 1,
        .ConfigurationStrIndex = NO_DESCRIPTOR,
        .ConfigAttributes =
          USB_CONFIG_ATTR_RESERVED | USB_CONFIG_ATTR_SELFPOWERED,
        .MaxPowerConsumption = USB_CONFIG_POWER_MA(100)
    },
    .CCIInterface = {
        .Header = {
            .Size = sizeof(USB_Descriptor_Interface_t),
            .Type = DTYPE_Interface
        },
        .InterfaceNumber = 0,
        .AlternateSetting = 0,
        .TotalEndpoints = 1,
        .Class = CDC_CSCP_CDCClass,
        .SubClass = CDC_CSCP_ACMSubclass,
        .Protocol = CDC_CSCP_ATCommandProtocol,
        .InterfaceStrIndex = NO_DESCRIPTOR
    },
    .functionalHeader = {
        .Header = {
            .Size = sizeof(USB_CDC_Descriptor_FunctionalHeader_t),
            .Type = DTYPE_CSInterface
        },
        .Subtype = CDC_DSUBTYPE_CSInterface_Header,
        .CDCSpecification = VERSION_BCD(1, 1, 0),
    },
    .functionalACM = {
        .Header = {
            .Size = sizeof(USB_CDC_Descriptor_FunctionalACM_t),
            .Type = DTYPE_CSInterface
        },
        .Subtype = CDC_DSUBTYPE_CSInterface_ACM,
        .Capabilities = 0x06,
    },
    .CDCFunctionalUnion = {
        .Header = {
            .Size = sizeof(USB_CDC_Descriptor_FunctionalUnion_t),
            .Type = DTYPE_CSInterface
        },
        .Subtype = CDC_DSUBTYPE_CSInterface_Union,
        .MasterInterfaceNumber = 0,
        .SlaveInterfaceNumber = 1,
    },
    .notificationEndpoint = {
        .Header = {
            .Size = sizeof(USB_Descriptor_Endpoint_t),
            .Type = DTYPE_Endpoint
        },
        .EndpointAddress = NOTIFY_ENDPOINT,
        .Attributes =
          EP_TYPE_INTERRUPT | ENDPOINT_ATTR_NO_SYNC | ENDPOINT_USAGE_DATA,
        .EndpointSize = NOTIFY_ENDPOINT_SIZE,
        .PollingIntervalMS = 0xFF
    },
    .DCIInterface = {
        .Header = {
            .Size = sizeof(USB_Descriptor_Interface_t),
            .Type = DTYPE_Interface
        },
        .InterfaceNumber = 1,
        .AlternateSetting = 0,
        .TotalEndpoints = 2,
        .Class = CDC_CSCP_CDCDataClass,
        .SubClass = CDC_CSCP_NoDataSubclass,
        .Protocol = CDC_CSCP_NoDataProtocol,
        .InterfaceStrIndex = NO_DESCRIPTOR
    },
    .outputEndpoint = {
        .Header = {
            .Size = sizeof(USB_Descriptor_Endpoint_t),
            .Type = DTYPE_Endpoint
        },
        .EndpointAddress = RECEIVE_ENDPOINT,
        .Attributes =
          (EP_TYPE_BULK | ENDPOINT_ATTR_NO_SYNC | ENDPOINT_USAGE_DATA),
        .EndpointSize = TXRX_ENDPOINT_SIZE,
        .PollingIntervalMS = 0x05
    },
    .inputEndpoint = {
        .Header = {
            .Size = sizeof(USB_Descriptor_Endpoint_t),
            .Type = DTYPE_Endpoint
        },
        .EndpointAddress = TRANSMIT_ENDPOINT,
        .Attributes =
          (EP_TYPE_BULK | ENDPOINT_ATTR_NO_SYNC | ENDPOINT_USAGE_DATA),
        .EndpointSize = TXRX_ENDPOINT_SIZE,
        .PollingIntervalMS = 0x05
    }
};

// Language descriptor.
static const USB_Descriptor_String_t PROGMEM languageString = {
    .Header = {
        .Size = USB_STRING_LEN(1),
        .Type = DTYPE_String
    },
    .UnicodeString = { LANGUAGE_ID_ENG }
};

// Manufacturer descriptor.
static const USB_Descriptor_String_t PROGMEM manufacturerString = {
    .Header = {
        .Size = USB_STRING_LEN(15),
        .Type = DTYPE_String
    },
    .UnicodeString = L"zplab.wustl.edu"
};

// Product descriptor.
static const USB_Descriptor_String_t PROGMEM productString = {
    .Header = {
        .Size = USB_STRING_LEN(11),
        .Type = DTYPE_String
    },
    .UnicodeString = L"IOTool"
};

static const USB_Descriptor_String_t PROGMEM serialString = {
    .Header = {
        .Size = USB_STRING_LEN(6),
        .Type = DTYPE_String
    },
    .UnicodeString = SERIAL_NUMBER
};


// LUFA CDC Class driver interface configuration and state information.
USB_ClassInfo_CDC_Device_t serialDevice = {
    .Config = {
        .ControlInterfaceNumber = 0,
        .DataINEndpoint = {
            .Address = TRANSMIT_ENDPOINT,
            .Size = TXRX_ENDPOINT_SIZE,
            .Banks = 1,
        },
        .DataOUTEndpoint = {
            .Address = RECEIVE_ENDPOINT,
            .Size = TXRX_ENDPOINT_SIZE,
            .Banks = 1,
        },
        .NotificationEndpoint = {
            .Address = NOTIFY_ENDPOINT,
            .Size = NOTIFY_ENDPOINT_SIZE,
            .Banks = 1,
        },
    },
};



uint8_t state;

// Event handler for the library USB Configuration Changed event.  Call the
// appropriate LUFA routine and record success/failure.
void EVENT_USB_Device_ConfigurationChanged(void)
{
    if (CDC_Device_ConfigureEndpoints(&serialDevice)) {
        SET_MASK_HI(state, CFGD);
    } else {
        SET_MASK_LO(state, CFGD);
    }
}

// Event handler for the library USB Control Request event, call the appropriate
// LUFA library routine.
void EVENT_USB_Device_ControlRequest(void) {
    CDC_Device_ProcessControlRequest(&serialDevice);
}

// Event handler for the library USB disconnection event.  Record the event.
void EVENT_USB_Device_Disconnect(void)
{
    SET_MASK_LO(state, CFGD);
}

// Event handler for control line change events.  Record the DTR state.
void EVENT_CDC_Device_ControLineStateChanged(
  USB_ClassInfo_CDC_Device_t *const info)
{
    if (info->State.ControlLineStates.HostToDevice & CDC_CONTROL_LINE_OUT_DTR) {
        SET_MASK_HI(state, DTR);
    } else {
        SET_MASK_LO(state, DTR);
    }
}

// Return the requested device descriptor.
uint16_t CALLBACK_USB_GetDescriptor(const uint16_t value, const uint8_t index,
  const void **const address) {
    const uint8_t type = (value >> 8);
    const uint8_t number = (value & 0xFF);
    switch (type) {
        case DTYPE_Device:
            *address = &deviceDescriptor;
            return sizeof(USB_Descriptor_Device_t);
        case DTYPE_Configuration:
            *address = &configurationDescriptor;
            return sizeof(USB_Descriptor_Configuration_t);
        case DTYPE_String:
            switch (number) {
                case 0x00:
                    *address = &languageString;
                    return pgm_read_byte(&languageString.Header.Size);
                case 0x01:
                    *address = &manufacturerString;
                    return pgm_read_byte(&manufacturerString.Header.Size);
                case 0x02:
                    *address = &productString;
                    return pgm_read_byte(&productString.Header.Size);
                case 0x03:
                    *address = &serialString;
                    return pgm_read_byte(&serialString.Header.Size);
            }
    }
    return 0;
}
