// kgsws' nRF24L01 library
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <nrf.h>
#include "radio.h"

uint8_t nrf_status = 0xff;

void NRF_EnableFeatures();

inline uint8_t NRF_Transmit(uint8_t dat)
{
	RFDAT = dat;
	RFSPIF = 0;
	while(!RFSPIF);
	return RFDAT;
}

void NRF_SetReg(uint8_t reg, uint8_t out)
{
	RFCSN = 0;
	NRF_Transmit(NCMD_W_REGISTER | reg);
	NRF_Transmit(out);
	RFCSN = 1;
}

void NRF_Init()
{
	// init SPI
	RFCTL = 0x11;
	RFCON = 0x06;
	//// some defaults
	// enable features
	NRF_EnableFeatures();
	// disable dyn payload for all pipes
	NRF_SetReg(NREG_DYNPD, 0);
	// enable dyn payload, only global
	NRF_SetReg(NREG_FEATURE, NCFG_EN_DPL);
	// no auto ACK
	NRF_SetReg(NREG_EN_AA, 0);
	// no retransmit
	NRF_SetReg(NREG_SETUP_RETR, 0);
	// default rate, and power
	NRF_SetReg(NREG_RF_SETUP, 0b00001110);
	// enable all pipes
	NRF_SetReg(NREG_EN_RXADDR, 0b00111111);
}

uint8_t NRF_ReadStatus()
{
	RFCSN = 0;
	nrf_status = NRF_Transmit(NCMD_STATUS);
	RFCSN = 1;
	return nrf_status;
}

void NRF_Command(uint8_t cmd)
{
	RFCSN = 0;
	nrf_status = NRF_Transmit(cmd);
	RFCSN = 1;
}

uint8_t NRF_CommandRead(uint8_t cmd)
{
	uint8_t ret;
	RFCSN = 0;
	nrf_status = NRF_Transmit(cmd);
	ret = NRF_Transmit(0xFF);
	RFCSN = 1;
	return ret;
}

void NRF_GetRegister(uint8_t reg, __xdata uint8_t *buff, uint8_t length)
{
	RFCSN = 0;
	nrf_status = NRF_Transmit(NCMD_R_REGISTER | reg);
	while(length--)
	{
		*buff = NRF_Transmit(0xFF);
		buff++;
	}
	RFCSN = 1;
}

void NRF_SetRegister(uint8_t reg, __xdata uint8_t *buff, uint8_t length)
{
	RFCSN = 0;
	nrf_status = NRF_Transmit(NCMD_W_REGISTER | reg);
	while(length--)
	{
		NRF_Transmit(*buff);
		buff++;
	}
	RFCSN = 1;
}

void NRF_ReadData(__xdata uint8_t *buf, uint8_t count)
{
	RFCSN = 0;
	nrf_status = NRF_Transmit(NCMD_R_RX_PAYLOAD);
	while(count--)
	{
		*buf = NRF_Transmit(0xFF);
		buf++;
	}
	RFCSN = 1;
}

void NRF_WriteData(__xdata uint8_t *buf, uint8_t count)
{
	RFCSN = 0;
	nrf_status = NRF_Transmit(NCMD_W_TX_PAYLOAD);
	while(count--)
	{
		NRF_Transmit(*buf);
		buf++;
	}
	RFCSN = 1;
}

void NRF_ClearIRQ()
{
	RFCSN = 0;
	nrf_status = NRF_Transmit(NCMD_W_REGISTER | NREG_STATUS);
	NRF_Transmit(0b01110000);
	RFCSN = 1;
}

uint8_t NRF_GetReg(uint8_t reg)
{
	uint8_t ret;
	RFCSN = 0;
	NRF_Transmit(NCMD_R_REGISTER | reg);
	ret = NRF_Transmit(0xFF);
	RFCSN = 1;
	return ret;
}

void NRF_EnableFeatures()
{
	RFCSN = 0;
	NRF_Transmit(NCMD_ACTIVATE);
	NRF_Transmit(0x73);
	RFCSN = 1;
}
/*
void NRF_Scan(uint8_t *buff)
{
	uint8_t i;
	uint8_t j = 128;
	uint8_t old;
	uint8_t *dst = buff;

	NRF_ON();
	// backup channel
	old = NRF_GetReg(NREG_RF_CH);
	// setup buffer
	memset(buff, 0, 64);
	// setup special RX mode
	NRF_Command(NCMD_FLUSH_TX);
	NRF_Command(NCMD_FLUSH_RX);
	NRF_SetReg(NREG_EN_AA, 0);
	NRF_SetReg(NREG_RF_SETUP, 0x0F);
	NRF_SetReg(NREG_EN_RXADDR, 0b00111111);
	// enable RX; 2b CRC
	NRF_SetReg(NREG_CONFIG, NCFG_PRIM_RX | NCFG_PWR_UP | NCFG_MASK_MAX_RT | NCFG_MASK_TX_DS | NCFG_MASK_RX_DR | NCFG_EN_CRC | NCFG_CRCO);

	while(j--)
	{
		for(i = 0; i < 128; i++)
		{
			// set channel
			NRF_SetReg(NREG_RF_CH, i);
			// enable
			NRF_OFF();
			_delay_us(400);
			// read RPD
			if(i & 1)
			{
				if(NRF_GetReg(9) & 1 && (*dst >> 4) < 15)
					*dst += 0x10;
				dst++;
			} else {
				if(NRF_GetReg(9) & 1 && (*dst & 15) < 15)
					*dst += 1;
			}
			// disable
			NRF_ON();
			_delay_us(10);
		}
	}
	// finish
	NRF_ClearIRQ();
	NRF_Command(NCMD_FLUSH_RX);
	NRF_SetReg(NREG_RF_CH, old);
}
*/
