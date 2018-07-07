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

#include <nrf.h>
#include <stdbool.h>

#include "usb.h"

/** Leaves the minimum of the two arguments */
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// USB register map
volatile __xdata __at (0xC640) uint8_t out1buf[USB_EP1_SIZE];
volatile __xdata __at (0xC680) uint8_t in1buf[USB_EP1_SIZE];
volatile __xdata __at (0xC6C0) uint8_t out0buf[MAX_PACKET_SIZE_EP0];
volatile __xdata __at (0xC700) uint8_t in0buf[MAX_PACKET_SIZE_EP0];
volatile __xdata __at (0xC781) uint8_t bout1addr;
volatile __xdata __at (0xC782) uint8_t bout2addr;
volatile __xdata __at (0xC783) uint8_t bout3addr;
volatile __xdata __at (0xC784) uint8_t bout4addr;
volatile __xdata __at (0xC785) uint8_t bout5addr;
volatile __xdata __at (0xC788) uint8_t binstaddr;
volatile __xdata __at (0xC789) uint8_t bin1addr;
volatile __xdata __at (0xC78A) uint8_t bin2addr;
volatile __xdata __at (0xC78B) uint8_t bin3addr;
volatile __xdata __at (0xC78C) uint8_t bin4addr;
volatile __xdata __at (0xC78D) uint8_t bin5addr;
volatile __xdata __at (0xC7A8) uint8_t ivec;
volatile __xdata __at (0xC7A9) uint8_t in_irq;
volatile __xdata __at (0xC7AA) uint8_t out_irq;
volatile __xdata __at (0xC7AB) uint8_t usbirq;
volatile __xdata __at (0xC7AC) uint8_t in_ien;
volatile __xdata __at (0xC7AD) uint8_t out_ien;
volatile __xdata __at (0xC7AE) uint8_t usbien;
volatile __xdata __at (0xC7B4) uint8_t ep0cs;
volatile __xdata __at (0xC7B5) uint8_t in0bc;
volatile __xdata __at (0xC7B6) uint8_t in1cs;
volatile __xdata __at (0xC7B7) uint8_t in1bc;
volatile __xdata __at (0xC7B8) uint8_t in2cs;
volatile __xdata __at (0xC7B9) uint8_t in2bc;
volatile __xdata __at (0xC7C5) uint8_t out0bc;
volatile __xdata __at (0xC7C6) uint8_t out1cs;
volatile __xdata __at (0xC7C7) uint8_t out1bc;
volatile __xdata __at (0xC7D6) uint8_t usbcs;
volatile __xdata __at (0xC7DE) uint8_t inbulkval;
volatile __xdata __at (0xC7DF) uint8_t outbulkval;
volatile __xdata __at (0xC7E0) uint8_t inisoval;
volatile __xdata __at (0xC7E1) uint8_t outisoval;
volatile __xdata __at (0xC7E8) uint8_t setupbuf[8];

// [kg] serial from info page
volatile __xdata __at (0x000B) uint8_t info_id[5];
__xdata uint8_t chip_id[sizeof(info_id) * 4 + 2]; // it is gonna be string descriptor
const uint8_t hextab[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

static uint8_t usb_current_config;
static usb_state_t usb_state;

// [kg] single packet now
static void packet_isr_ep0_in(__code uint8_t const *volatile data_ptr, uint8_t size);
static void serial_isr_ep0_in();

static void usb_process_get_descriptor();
static void isr_sudav();
static void usb_std_endpoint_request();
static void usb_std_interface_request();
static void usb_std_device_request();

static void delay_ms(uint16_t ms);

bool usb_irq( void )
{
	bool ret = false;

	// [kg] using USBCON instead of 'ivec' seems to fix many problems
	switch(USBCON & 0x1F)
	{
		case INT_USBRESET: // Bus reset
			// Clear interrupt flag
			usbirq = 0x10;
			// Reset internal states
			usb_state = DEFAULT;
			usb_current_config = 0;
			break;
		case INT_SUDAV: // SETUP data packet
			// Clear interrupt flag
			usbirq = 0x01;
			// Process setup data
			isr_sudav();
			break;
		case INT_SOF: // Start of Frame packet
			// Clear interrupt flag
			usbirq = 0x02;
			break;
		case INT_SUTOK: // Setup token
			// Clear interrupt flag
			usbirq = 0x04;
			break;
		case INT_SUSPEND: // SUSPEND signal
			// Clear interrupt flag
			usbirq = 0x08;
			break;
		case INT_EP0IN:
			// Clear interrupt flag
			in_irq = 0x01;
			// We are getting a ep0in interrupt when the host sends ACK and do not
			//   have any more data to send
			// Arm the in0bsy bit by writing to byte count reg
			in0bc = 0;
			// ACK the status stage
			USB_EP0_HSNAK();
			break;
		case INT_EP0OUT:
			// Clear interrupt flag
			out_irq = 0x01;
			break;
		case INT_EP1IN:
			// Clear interrupt flag
			in_irq = 0x02;
			// User code will have already filled IN1 buffer and set byte count
			// USB controller clears busy flag when data is sent
			break;
		case INT_EP1OUT:
			// Clear interrupt flag
			out_irq = 0x02;
			// Set packet rx flag
			ret = true;
			break;
		default:
			break;
	}

	return ret;
}

void usb_init(void)
{
	uint8_t i;
	// [kg] read ID from info page
	FSR_INFEN = 1;
	chip_id[0] = sizeof(chip_id);
	chip_id[1] = 3;
	for(i = 0; i < sizeof(info_id); i++)
	{
		chip_id[i*4+5] = 0;
		chip_id[i*4+4] = hextab[info_id[i] & 15];
		chip_id[i*4+3] = 0;
		chip_id[i*4+2] = hextab[info_id[i] >> 4];
	}
	FSR_INFEN = 0;

	// Setup state information
	usb_state = DEFAULT;

	// Setconfig configuration information
	usb_current_config = 0;

	// Disconnect from USB-bus since we are in this routine from a power on and not a soft reset
	usbcs |= 0x08;
	delay_ms(50);
	usbcs &= ~0x08;

	// Set up interrupts and clear interrupt flags
	usbien = 0x1d;
	in_ien = 0x01;
	in_irq = 0x1f;
	out_ien = 0x01;
	out_irq = 0x1f;

	// Setup the USB RAM
	bout1addr = MAX_PACKET_SIZE_EP0/2;
	bout2addr = 0x0000;
	bout3addr = 0x0000;
	bout4addr = 0x0000;
	bout5addr = 0x0000;
	binstaddr = 0xc0;
	bin1addr = MAX_PACKET_SIZE_EP0/2;
	bin2addr = 0x0000;
	bin3addr = 0x0000;
	bin4addr = 0x0000;
	bin5addr = 0x0000;

	// Set up endpoint interrupts
	inbulkval = 0x03; // IN0, IN1
	outbulkval = 0x03; // OUT0, OUT1
	inisoval = 0x00; // not used
	outisoval = 0x00; // not used
	in_ien = 0x03; // IN0, IN1
	out_ien = 0x03; // OUT0, OUT1
	// Write to byte count reg to arm for next EP1 OUT transfer
	out1bc = 0xff;
}

static void isr_sudav()
{
	// setupbuf[0] == bmRequestType
	if ( ( setupbuf[0] & 0x60 ) == 0x00 )
	{
		switch ( setupbuf[0] & 0x03 )
		{
			case 0: // Device
				usb_std_device_request();
				break;
			case 1: // Interface
				usb_std_interface_request();
				break;
			case 2: // Endpoint
				usb_std_endpoint_request();
				break;
			default: // Stall on unsupported recipient
				USB_EP0_STALL();
				break;
		}
	}
	else
	{
		// Stall on unsupported requests
		USB_EP0_STALL();
	}
}

static void usb_std_endpoint_request()
{
	switch( setupbuf[1] ) // bRequest
	{
		case USB_REQ_GET_STATUS:
			if ( usb_state == CONFIGURED )
			{
				// Return Halt feature status
				if ( setupbuf[4] == 0x81 )
					in0buf[0] = in1cs & 0x01;
				else if ( setupbuf[4] == 0x82 )
					in0buf[0] = in2cs & 0x01;
				else if ( setupbuf[4] == 0x01 )
					in0buf[0] = out1cs & 0x01;
				in0bc = 0x02;
			}
			break;

		default:
			USB_EP0_STALL();
	}
}

static void usb_std_interface_request()
{
	switch( setupbuf[1] ) // bRequest
	{
		case USB_REQ_GET_STATUS:
			if ( usb_state == CONFIGURED )
			{
				// All values are reserved for interfaces
				in0buf[0] = 0x00;
				in0buf[1] = 0x00;
				in0bc = 0x02;
			}
			break;

		default:
			USB_EP0_STALL();
	}
}

static void usb_std_device_request()
{
	switch( setupbuf[1] ) // bRequest
	{
		case USB_REQ_GET_STATUS:
			// We must be in ADDRESSED or CONFIGURED state, and wIndex must be 0
			if ( ( usb_state == ADDRESSED || usb_state == CONFIGURED ) && ( setupbuf[4] == 0x00 ) )
			{
				// We aren't self-powered and we don't support remote wakeup
				in0buf[0] = 0x00;
				in0buf[1] = 0x00;
				in0bc = 0x02;
			}
			else
			{
				// Stall for invalid requests
				USB_EP0_STALL();
			}
			break;

		case USB_REQ_SET_ADDRESS:
			// USB controller takes care of setting our address
			usb_state = ADDRESSED;
			break;

		case USB_REQ_GET_DESCRIPTOR:
			usb_process_get_descriptor();
			break;

		case USB_REQ_GET_CONFIGURATION:
			if ( usb_state == ADDRESSED )
			{
				in0buf[0] = 0x00;
				in0bc = 0x01;
			}
			else if ( usb_state == CONFIGURED )
			{
				in0buf[0] = usb_current_config;
				in0bc = 0x01;
			}
			else
			{
				// Behavior not specified in other states, so STALL
				USB_EP0_STALL();
			}
			break;

		case USB_REQ_SET_CONFIGURATION:
			// setupbuf[2] == wValue
			if ( setupbuf[2] == 0x00 )
			{
				usb_state = ADDRESSED;
				usb_current_config = 0x00;
				// Since there isn't a data stage for this request,
				//   we have to explicitly clear the NAK bit
				USB_EP0_HSNAK();
			}
			else if ( setupbuf[2] == 0x01 )
			{
				usb_state = CONFIGURED;
				usb_current_config = 0x01;
				// Since there isn't a data stage for this request,
				//   we have to explicitly clear the NAK bit
				USB_EP0_HSNAK();
			}
			else
			{
				// Stall for invalid config values
				USB_EP0_STALL();
			}
			break;

		default:
	}
}

static void usb_process_get_descriptor()
{
    // Switch on descriptor type
    switch ( setupbuf[3] )
    {
        case USB_DESC_DEVICE:
		// Transfer device descriptor
		packet_isr_ep0_in((uint8_t*)&g_usb_dev_desc, MIN(setupbuf[6], sizeof(usb_dev_desc_t)));
            break;

        case USB_DESC_CONFIGURATION:
		// We only support one configuration, so always return it
		packet_isr_ep0_in((uint8_t*)&g_usb_conf_desc, MIN(setupbuf[6], sizeof(usb_conf_desc_basestation_t)));
            break;

        case USB_DESC_STRING:
		// [kg] support more strings
		switch(setupbuf[2])
		{
			case 0:
				packet_isr_ep0_in((void*)&string_zero, MIN(setupbuf[6], sizeof(string_zero)));
			break;
			case 1:
				packet_isr_ep0_in((void*)&g_usb_string_desc_1, MIN(setupbuf[6], STRS_VENDOR));
			break;
			case 2:
				packet_isr_ep0_in((void*)&g_usb_string_desc_2, MIN(setupbuf[6], STRS_PRODUCT));
			break;
			case 3:
				serial_isr_ep0_in();
			break;
			default:
				USB_EP0_STALL();
			break;
		}
            break;
        default:
			// Just ignore all other requests and ACK status stage
			USB_EP0_STALL();
            break;
    }
}

// [kg] now single packet
static void packet_isr_ep0_in(__code uint8_t const *volatile data_ptr, uint8_t size)
{
	__xdata uint8_t *volatile data_dst = in0buf;
	uint8_t i;

	i = size;
	// Copy data to the USB-controller buffer
	data_dst = in0buf;
	while(size--)
	{
		*data_dst = *data_ptr;
		data_dst++;
		data_ptr++;
	}

	// Tell the USB-controller how many bytes to send
	// If a IN is received from host after this the USB-controller will send the data
	in0bc = i;
}

// [kg] special ID case
static void serial_isr_ep0_in()
{
	__xdata uint8_t *volatile data_dst = in0buf;
	__xdata uint8_t const *data_ptr = chip_id;
	uint8_t i;

	i = sizeof(chip_id);
	// Copy data to the USB-controller buffer
	data_dst = in0buf;
	while(i--)
	{
		*data_dst = *data_ptr;
		data_dst++;
		data_ptr++;
	}

	// Tell the USB-controller how many bytes to send
	// If a IN is received from host after this the USB-controller will send the data
	in0bc = sizeof(chip_id);
}

static void delay_ms(uint16_t ms)
{
	uint16_t i, j;

	for(i = 0; i < ms; i++ )
	{
		for( j = 0; j < 1403; j++)
		{
			__asm
			nop
			__endasm;
		}
	}
}
