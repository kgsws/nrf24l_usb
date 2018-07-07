/*******************************************************************************************************
 Copyright (c) 2011, Carson Morrow (carson@carsonmorrow.com)
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are permitted provided
 that the following conditions are met:

 *Redistributions of source code must retain the above copyright notice, this list of conditions and the
	following disclaimer.
 *Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
	the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************************************/

#ifndef USB_DEFS_H__
#define USB_DEFS_H__

#include <stdint.h>

// Standard request codes
#define USB_REQ_GET_STATUS         0x00
#define USB_REQ_CLEAR_FEATURE      0x01
#define USB_REQ_RESERVED_1         0x02
#define USB_REQ_SET_FEATURE        0x03
#define USB_REQ_RESERVED_2         0x04
#define USB_REQ_SET_ADDRESS        0x05
#define USB_REQ_GET_DESCRIPTOR     0x06
#define USB_REQ_SET_DESCRIPTOR     0x07
#define USB_REQ_GET_CONFIGURATION  0x08
#define USB_REQ_SET_CONFIGURATION  0x09
#define USB_REQ_GET_INTERFACE      0x0a
#define USB_REQ_SET_INTERFACE      0x0b
#define USB_REQ_SYNCH_FRAME        0x0c

// Descriptor types
#define USB_DESC_DEVICE           0x01
#define USB_DESC_CONFIGURATION    0x02
#define USB_DESC_STRING           0x03
#define USB_DESC_INTERFACE        0x04
#define USB_DESC_ENDPOINT         0x05
#define USB_DESC_DEVICE_QUAL      0x06
#define USB_DESC_OTHER_SPEED_CONF 0x07
#define USB_DESC_INTERFACE_POWER  0x08

// Endpoint types
#define USB_ENDPOINT_TYPE_BULK    0x02
#define USB_ENDPOINT_TYPE_INT     0x03

// Clear hsnak bit
#define USB_EP0_HSNAK() do {ep0cs = 0x02; } while(0)
// Set stall and dstall bits to stall during setup data transaction
#define USB_EP0_STALL() do {ep0cs = 0x11; } while(0)

// Interrupt codes
/*
#define INT_SUDAV    0x00
#define INT_SOF      0x04
#define INT_SUTOK    0x08
#define INT_SUSPEND  0x0C
#define INT_USBRESET 0x10
#define INT_EP0IN    0x18
#define INT_EP0OUT   0x1C
#define INT_EP1IN    0x20
#define INT_EP1OUT   0x24
#define INT_EP2IN    0x28
*/
// [kg] values for USBCON
#define INT_SUDAV    0x00
#define INT_SOF      0x01
#define INT_SUTOK    0x02
#define INT_SUSPEND  0x03
#define INT_USBRESET 0x04
#define INT_EP0IN    0x06
#define INT_EP0OUT   0x07
#define INT_EP1IN    0x08
#define INT_EP1OUT   0x09
#define INT_EP2IN    0x0A

#define USB_BM_STATE_CONFIGURED           0x01
#define USB_BM_STATE_ALLOW_REMOTE_WAKEUP  0x02

typedef struct
{
	volatile uint8_t bLength;
	volatile uint8_t bDescriptorType;
	volatile uint16_t bcdUSB;
	volatile uint8_t bDeviceClass;
	volatile uint8_t bDeviceSubClass;
	volatile uint8_t bDeviceProtocol;
	volatile uint8_t bMaxPacketSize0;
	volatile uint16_t idVendor;
	volatile uint16_t idProduct;
	volatile uint16_t bcdDevice;
	volatile uint8_t iManufacturer;
	volatile uint8_t iProduct;
	volatile uint8_t iSerialNumber;
	volatile uint8_t bNumConfigurations;
} usb_dev_desc_t;

typedef struct
{
	volatile uint8_t bLength;
	volatile uint8_t bDescriptorType;
	volatile uint16_t wTotalLength;
	volatile uint8_t bNumInterfaces;
	volatile uint8_t bConfigurationValue;
	volatile uint8_t iConfiguration;
	volatile uint8_t bmAttributes;
	volatile uint8_t bMaxPower;
} usb_conf_desc_t;

typedef struct
{
	volatile uint8_t bLength;
	volatile uint8_t bDescriptorType;
	volatile uint8_t bInterfaceNumber;
	volatile uint8_t bAlternateSetting;
	volatile uint8_t bNumEndpoints;
	volatile uint8_t bInterfaceClass;
	volatile uint8_t bInterfaceSubClass;
	volatile uint8_t bInterfaceProtocol;
	volatile uint8_t iInterface;
} usb_if_desc_t;

typedef struct
{
	volatile uint8_t bLength;
	volatile uint8_t bDescriptorType;
	volatile uint8_t bEndpointAddress;
	volatile uint8_t bmAttributes;
	volatile uint16_t wMaxPacketSize;
	volatile uint8_t bInterval;
} usb_ep_desc_t;

typedef enum 
{
	ATTACHED,
	POWERED,
	DEFAULT,
	ADDRESSED,
	CONFIGURED,
	SUSPENDED
} usb_state_t;

#endif // USB_DEFS_H__
