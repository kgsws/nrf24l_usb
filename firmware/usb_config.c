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

#include "usb_config.h"

const usb_dev_desc_t g_usb_dev_desc =
{
	sizeof(usb_dev_desc_t), 
	USB_DESC_DEVICE, 
	0x0200,             // bcdUSB
	0xff,               // bDeviceClass
	0xff,               // bDeviceSubclass
	0xff,               // bDeviceProtocol
	MAX_PACKET_SIZE_EP0,// bMaxPacketSize0
	0x16c0,             // idVendor
	0x05dc,             // idProduct
	0x0001,             // bcdDevice - Device Release Number (BCD)
	0x01,               // iManufacturer
	0x02,               // iProduct
	0x03,               // iSerialNumber
	0x01                // bNumConfigurations
};

const usb_conf_desc_basestation_t g_usb_conf_desc = 
{
	{
		sizeof(usb_conf_desc_t),
		USB_DESC_CONFIGURATION,
		sizeof(usb_conf_desc_basestation_t),
		1,            // bNumInterfaces
		1,            // bConfigurationValue
		0,            // iConfiguration
		0x80,         // bmAttributes - bus powered, no remote wakeup
		25,           // bMaxPower
	},
	/* Interface Descriptor 0 */ 
	{
		sizeof(usb_if_desc_t),
		USB_DESC_INTERFACE,
		0,            // bInterfaceNumber
		0,            // bAlternateSetting
		2,            // bNumEndpoints
		0xff,         // bInterfaceClass
		0x00,         // bInterfaceSubClass  
		0xff,         // bInterfaceProtocol 
		0x00,         // iInterface
	},
	/* Endpoint Descriptor EP1IN */
	{
		sizeof(usb_ep_desc_t),
		USB_DESC_ENDPOINT,
		0x81,                   // bEndpointAddress
		USB_ENDPOINT_TYPE_INT,  // bmAttributes
		USB_EP1_SIZE,           // wMaxPacketSize
		0x06                    // bInterval
	},
	/* Endpoint Descriptor EP1OUT */
	{
		sizeof(usb_ep_desc_t),
		USB_DESC_ENDPOINT,
		0x01,                   // bEndpointAddress
		USB_ENDPOINT_TYPE_BULK, // bmAttributes
		USB_EP1_SIZE,           // wMaxPacketSize
		0x06                    // bInterval
	}
};

const usb_str_vendor_t g_usb_string_desc_1 = 
{
	STRS_VENDOR, 0x03,
	STR_VENDOR
};

const usb_str_product_t g_usb_string_desc_2 = 
{
	STRS_PRODUCT, 0x03,
	STR_PRODUCT
};

// This is for setting language American English (String descriptor 0 is an array of supported languages):
const uint8_t string_zero[4] = {0x04, 0x03, 0x09, 0x04 } ;
