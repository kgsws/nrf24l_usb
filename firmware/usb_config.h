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

#ifndef USB_CONFIG_H__
#define USB_CONFIG_H__

#include "usb_defs.h"

// [kg] strings
#define STR_VENDOR	"k\000g\000s\000w\000s"
#define STR_PRODUCT	"n\000R\000F\0002\0004\000L\000 \000T\000r\000a\000n\000s\000c\000e\000i\000v\000e\000r\000 \0004\000.\0001"
#define STRS_VENDOR	(sizeof(STR_VENDOR)+2)
#define STRS_PRODUCT	(sizeof(STR_PRODUCT)+2)

// [kg] string descriptors
typedef struct
{
	volatile uint8_t bLength;
	volatile uint8_t bDescriptorType;
	volatile uint8_t text[STRS_VENDOR-2];
} usb_str_vendor_t;
typedef struct
{
	volatile uint8_t bLength;
	volatile uint8_t bDescriptorType;
	volatile uint8_t text[STRS_PRODUCT-2];
} usb_str_product_t;

typedef struct
{
    usb_conf_desc_t conf;
    usb_if_desc_t if0;
    usb_ep_desc_t ep1in;
    usb_ep_desc_t ep1out;
} usb_conf_desc_basestation_t;

extern const usb_conf_desc_basestation_t g_usb_conf_desc;
extern const usb_dev_desc_t g_usb_dev_desc;

extern const usb_str_vendor_t g_usb_string_desc_1;
extern const usb_str_product_t g_usb_string_desc_2;
extern const uint8_t string_zero[4];

#define MAX_PACKET_SIZE_EP0 64
#define USB_EP1_SIZE        64

#endif // USB_CONFIG_H__
