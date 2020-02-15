// kgsws' NRF USB transceiver and programmer
//

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <nrf.h>

#include "usb.h"
#include "radio.h"

#define TXF_ADDR	1
#define TXF_DATA	2

#define CMD_WREN	6
#define CMD_WRDIS	4
#define CMD_RDSR	5
#define CMD_WRSR	1
#define CMD_READ	3
#define CMD_PROGRAM	2
#define CMD_E_PAGE	0x52
#define CMD_E_ALL	0x62

#define FSR_RDYN      _BV(4)

enum
{
	//// radio
	// write
	PTIN_SETTINGS,	// global settings
	PTIN_PIPES,	// all 5 pipes
	PTIN_PIPE,	// only single pipe
	PTIN_TX_SETADDR,// set Tx address
	PTIN_TX_PACKET,	// send packet
	PTIN_RX_MODE,
	// read
	PTIN_GET_SETTINGS,
	PTIN_GET_PIPES,

	//// programmer
	PTIN_SET_FSR = 0x10,
	PTIN_PROGRAM,	// write 32 bytes
	PTIN_ERASE_PAGE,
	PTIN_ERASE_ALL,
	PTIN_GET_FSR,
	PTIN_READ,
	PTIN_PRGOUT,

	//// set mode
	PTIN_MODE = 0xFF
};

enum
{
	// radio
	PTOUT_SETTINGS,
	PTOUT_PIPES,	// also contains last Tx address
	PTOUT_RX_PACKET,// p_rx_t
	// programmer
	PTOUT_FSR = 0x10,
	PTOUT_READ,
	// message
	PTOUT_MESSAGE = 0xFF
};

enum
{
	MODE_HIZ,
	MODE_LEDS,
	MODE_PROG,
	NUM_MODES
};

typedef struct
{
	uint8_t type;
	uint8_t channel;
	uint8_t rate;
	uint8_t crc;
	uint8_t adw;
} config_t;

typedef struct
{
	uint8_t size;
	uint8_t addr[5];
} pipe_t;

typedef struct
{
	uint8_t size;
	uint8_t addr;
} pipe_small_t;

typedef struct
{
	uint8_t type;
	pipe_t pipl[2];
	pipe_small_t pips[4];
	// last Tx addres is here for type PTOUT_PIPES
} pipes_t;

typedef struct
{
	uint8_t type;
	uint8_t pipe;
	union
	{
		pipe_t pipl;
		pipe_small_t pips;
	};
} pipe_single_t;

typedef struct
{
	uint8_t type;
	uint8_t pipe;
	uint8_t buff[32];
} p_rx_t;

extern volatile __xdata uint8_t in1buf[];
extern volatile __xdata uint8_t in1bc;
extern volatile __xdata uint8_t in1cs;

extern volatile __xdata uint8_t out1buf[];
extern volatile __xdata uint8_t out1bc;

__xdata uint8_t *dst;

uint8_t mode = MODE_HIZ;

uint8_t rxe;
uint8_t crc;
uint8_t dyn;

#define err_msg(x)	{in1buf[0]=PTOUT_MESSAGE;in1buf[1]=x;in1bc=2;while(!(in1cs & 0x02));break;}

#define PROG_CS(x)	P03 = !x;wait(64)
#define PROG_OUT(x)	P05 = x;wait(64)

#define in_conf	((__xdata config_t*)out1buf)
#define in_pipe	((__xdata pipes_t*)out1buf)
#define in_pips	((__xdata pipe_single_t*)out1buf)

#define out_rx		((__xdata p_rx_t*)in1buf)
#define out_conf	((__xdata config_t*)in1buf)
#define out_pipe	((__xdata pipes_t*)in1buf)

inline void wait(uint8_t count)
{
	while(count--);
}
/*
inline uint8_t spi_transmit(uint8_t in)
{
	uint8_t i = 128;
	uint8_t ret = 0;

	while(i)
	{
		wait(30);
		if(in & i)
			P01 = 1;
		else
			P01 = 0;
		wait(30);
		P00 = 1;
		if(P02)
			ret |= i;
		wait(30);
		P00 = 0;
		i >>= 1;
	}
	wait(30);
	return ret;
}
/*/
// HW SPI
inline uint8_t spi_transmit(uint8_t in)
{
	SMDAT = in;
	wait(64);
	return SMDAT;
}

inline uint8_t prg_read_fsr()
{
	uint8_t ret;
	PROG_CS(1);
	spi_transmit(CMD_RDSR);
	ret = spi_transmit(0xFF);
	PROG_CS(0);
	return ret;
}

inline void prg_enable_write()
{
	// something is wrong with each command alone
	uint8_t fsr = prg_read_fsr();
	PROG_CS(1);
	spi_transmit(CMD_WRSR);
	spi_transmit(_BV(5) | (fsr & _BV(3))); // info table support
	PROG_CS(0);
	PROG_CS(1);
	spi_transmit(CMD_WREN);
	PROG_CS(0);
}

inline uint8_t prg_ready_wait()
{
	uint16_t i = 512;
	while(i--)
	{
		wait(32);
		if(!(prg_read_fsr() & FSR_RDYN))
			break;
	}
	return !i;
}

void main()
{
	P0DIR = 0b11111111;
	P0EXP = 0;
	P0ALT = 0;

	NRF_Init();

	// read CRC setting
	crc = NRF_GetReg(NREG_CONFIG) & 0b00001100;

	usb_init();

	while(1)
	{
		// Check for USB interrupt
		if ( USBF )
		{
			// Clear USB interrupt
			USBF = 0;
			// Process interrupt
			if(usb_irq())
			{
				// process incomming packet
				switch(out1buf[0])
				{
//
//	transmitter
//
					case PTIN_SETTINGS:
						NRF_SetReg(NREG_RF_CH, in_conf->channel);
						switch(in_conf->rate)
						{
							default:
								NRF_SetReg(NREG_RF_SETUP, 0b00100110);
							break;
							case 1:
								NRF_SetReg(NREG_RF_SETUP, 0b00000110);
							break;
							case 2:
								NRF_SetReg(NREG_RF_SETUP, 0b00001110);
							break;
						}
						switch(in_conf->crc)
						{
							default:
								crc = 0;
							break;
							case 1:
								crc = 0b00001000;
							break;
							case 2:
								crc = 0b00001100;
							break;
						}
						if(RFCE)
							NRF_SetReg(NREG_CONFIG, crc | NCFG_MASK_MAX_RT | NCFG_MASK_TX_DS | NCFG_PWR_UP | NCFG_PRIM_RX);
						else
							NRF_SetReg(NREG_CONFIG, crc);
						NRF_SetReg(NREG_SETUP_AW, in_conf->adw - 2);
					break;
					case PTIN_PIPES:
						// there is enough flash space and it will be faster without loop
						dyn = 0;
						// pipe 0
						if(in_pipe->pipl[0].size > 32)
							dyn |= 0b00000001;
						else
							NRF_SetReg(NREG_RX_PW_P0, in_pipe->pipl[0].size);
						NRF_SetRegister(NREG_RX_ADDR_P0, in_pipe->pipl[0].addr, 5);
						// pipe 1
						if(in_pipe->pipl[1].size > 32)
							dyn |= 0b00000010;
						else
							NRF_SetReg(NREG_RX_PW_P1, in_pipe->pipl[1].size);
						NRF_SetRegister(NREG_RX_ADDR_P1, in_pipe->pipl[1].addr, 5);
						// pipe 2
						if(in_pipe->pips[0].size > 32)
							dyn |= 0b00000100;
						else
							NRF_SetReg(NREG_RX_PW_P2, in_pipe->pips[0].size);
						NRF_SetReg(NREG_RX_ADDR_P2, in_pipe->pips[0].addr);
						// pipe 3
						if(in_pipe->pips[1].size > 32)
							dyn |= 0b00001000;
						else
							NRF_SetReg(NREG_RX_PW_P3, in_pipe->pips[1].size);
						NRF_SetReg(NREG_RX_ADDR_P3, in_pipe->pips[1].addr);
						// pipe 4
						if(in_pipe->pips[2].size > 32)
							dyn |= 0b00010000;
						else
							NRF_SetReg(NREG_RX_PW_P4, in_pipe->pips[2].size);
						NRF_SetReg(NREG_RX_ADDR_P4, in_pipe->pips[2].addr);
						// pipe 5
						if(in_pipe->pips[3].size > 32)
							dyn |= 0b00100000;
						else
							NRF_SetReg(NREG_RX_PW_P5, in_pipe->pips[3].size);
						NRF_SetReg(NREG_RX_ADDR_P5, in_pipe->pips[3].addr);
						// dynamic payload
						NRF_SetReg(NREG_DYNPD, dyn);
					break;
					case PTIN_PIPE:
						if(in_pips->pipe < 2)
						{
							if(in_pips->pipl.size > 32)
								dyn |= 1 << in_pips->pipe;
							else
								NRF_SetReg(NREG_RX_PW_P0 + in_pips->pipe, in_pips->pipl.size);
							NRF_SetRegister(NREG_RX_ADDR_P0 + in_pips->pipe, in_pips->pipl.addr, 5);
						} else
						if(in_pips->pipe < 6)
						{
							if(in_pips->pips.size > 32)
								dyn |= 1 << in_pips->pipe;
							in_pips->pipe -= 2;
							if(in_pips->pips.size <= 32)
								NRF_SetReg(NREG_RX_PW_P2 + in_pips->pipe, in_pips->pips.size);
							NRF_SetReg(NREG_RX_ADDR_P2 + in_pips->pipe, in_pips->pips.addr);
						} else
							break;
						NRF_SetReg(NREG_DYNPD, dyn);
					break;
					case PTIN_TX_SETADDR:
						// TX addr
						NRF_SetRegister(NREG_TX_ADDR, out1buf + 1, 5);
					break;
					case PTIN_TX_PACKET:
						rxe = RFCE;
						RFCE = 0;
						// prepare
//						NRF_Command(NCMD_FLUSH_TX);
						NRF_SetReg(NREG_STATUS, NSTAT_RX_DR | NSTAT_TX_DS | NSTAT_MAX_RT);
						RFF = 0;
						NRF_SetReg(NREG_CONFIG, crc | NCFG_PWR_UP | NCFG_MASK_RX_DR);
						// wait
						{
							register uint16_t i;
							for(i=0;i<1000;i++);
						}
						// TX data
						NRF_WriteData(out1buf + 1, out1bc - 1);
						// TX mode
						RFCE = 1;
						// wait
						while(!RFF);
						NRF_SetReg(NREG_STATUS, NSTAT_RX_DR | NSTAT_TX_DS | NSTAT_MAX_RT);
						RFF = 0;
						// done
						if(rxe)
							NRF_SetReg(NREG_CONFIG, crc | NCFG_MASK_MAX_RT | NCFG_MASK_TX_DS | NCFG_PWR_UP | NCFG_PRIM_RX);
						else
							RFCE = 0;
					break;
					case PTIN_RX_MODE:
						if(out1buf[1])
						{
							NRF_Command(NCMD_FLUSH_RX);
							NRF_SetReg(NREG_CONFIG, crc | NCFG_MASK_MAX_RT | NCFG_MASK_TX_DS | NCFG_PWR_UP | NCFG_PRIM_RX);
							RFCE = 1;
						} else {
							RFCE = 0;
							NRF_Command(NCMD_FLUSH_RX);
							NRF_SetReg(NREG_STATUS, NSTAT_RX_DR);
						}
					break;
					case PTIN_GET_SETTINGS:
						out_conf->type = PTOUT_SETTINGS;
						out_conf->channel = NRF_GetReg(NREG_RF_CH);
						switch(NRF_GetReg(NREG_RF_SETUP))
						{
							default:
								out_conf->rate = 0;
							break;
							case 0b00000110:
								out_conf->rate = 1;
							break;
							case 0b00001110:
								out_conf->rate = 2;
							break;
						}
						if(crc & 0b00001000)
							out_conf->crc = crc & 0b00000100 ? 2 : 1;
						else
							out_conf->crc = 0;
						out_conf->adw = NRF_GetReg(NREG_SETUP_AW) + 2;
						in1bc = sizeof(config_t);
						while(!(in1cs & 0x02));
					break;
					case PTIN_GET_PIPES:
						// there is enough flash space and it will be faster without loop
						out_pipe->type = PTOUT_PIPES;
						// pipe 0
						if(dyn & 0b00000001)
							out_pipe->pipl[0].size = 0xFF;
						else
							out_pipe->pipl[0].size = NRF_GetReg(NREG_RX_PW_P0);
						NRF_GetRegister(NREG_RX_ADDR_P0, out_pipe->pipl[0].addr, 5);
						// pipe 1
						if(dyn & 0b00000010)
							out_pipe->pipl[1].size = 0xFF;
						else
							out_pipe->pipl[1].size = NRF_GetReg(NREG_RX_PW_P1);
						NRF_GetRegister(NREG_RX_ADDR_P1, out_pipe->pipl[1].addr, 5);
						// pipe 2
						if(dyn & 0b00000100)
							out_pipe->pips[0].size = 0xFF;
						else
							out_pipe->pips[0].size = NRF_GetReg(NREG_RX_PW_P2);
						out_pipe->pips[0].addr = NRF_GetReg(NREG_RX_ADDR_P2);
						// pipe 3
						if(dyn & 0b00001000)
							out_pipe->pips[1].size = 0xFF;
						else
							out_pipe->pips[1].size = NRF_GetReg(NREG_RX_PW_P3);
						out_pipe->pips[1].addr = NRF_GetReg(NREG_RX_ADDR_P3);
						// pipe 4
						if(dyn & 0b00010000)
							out_pipe->pips[2].size = 0xFF;
						else
							out_pipe->pips[2].size = NRF_GetReg(NREG_RX_PW_P4);
						out_pipe->pips[2].addr = NRF_GetReg(NREG_RX_ADDR_P4);
						// pipe 5
						if(dyn & 0b00100000)
							out_pipe->pips[3].size = 0xFF;
						else
							out_pipe->pips[3].size = NRF_GetReg(NREG_RX_PW_P5);
						out_pipe->pips[3].addr = NRF_GetReg(NREG_RX_ADDR_P5);
						// Tx address
						NRF_GetRegister(NREG_TX_ADDR, (void*)&out_pipe[1], 5);
						// send
						in1bc = sizeof(pipes_t) + 5;
						while(!(in1cs & 0x02));
					break;
//
//	programmer
//
					case PTIN_SET_FSR:
						if(mode != MODE_PROG)
							err_msg(0x80)
						// command
						PROG_CS(1);
						spi_transmit(CMD_WRSR);
						spi_transmit(out1buf[1]);
						PROG_CS(0);
					break;
					case PTIN_PROGRAM:
						if(mode != MODE_PROG)
							err_msg(0x80)
						// command
						prg_enable_write();
						PROG_CS(1);
						spi_transmit(CMD_PROGRAM);
						spi_transmit(out1buf[2]);
						spi_transmit(out1buf[1]);
						{
							uint8_t i = 32;
							dst = out1buf + 3;
							while(i--)
							{
								spi_transmit(*dst);
								dst++;
							}
						}
						PROG_CS(0);
						if(prg_ready_wait())
							err_msg(0x81)
					break;
					case PTIN_ERASE_PAGE:
						if(mode != MODE_PROG)
							err_msg(0x80)
						// command
						prg_enable_write();
						PROG_CS(1);
						spi_transmit(CMD_E_PAGE);
						spi_transmit(out1buf[1]);
						PROG_CS(0);
						if(prg_ready_wait())
							err_msg(0x81)
					break;
					case PTIN_ERASE_ALL:
						if(mode != MODE_PROG)
							err_msg(0x80)
						// command
						prg_enable_write();
						PROG_CS(1);
						spi_transmit(CMD_E_ALL);
						PROG_CS(0);
						if(prg_ready_wait())
							err_msg(0x81)
					break;
					case PTIN_GET_FSR:
						if(mode != MODE_PROG)
							err_msg(0x80)
						// command
						PROG_CS(1);
						spi_transmit(CMD_RDSR);
						in1buf[1] = spi_transmit(0xFF);
						PROG_CS(0);
						in1buf[0] = PTOUT_FSR;
						in1bc = 2;
						while(!(in1cs & 0x02));
					break;
					case PTIN_READ:
						if(mode != MODE_PROG)
							err_msg(0x80)
						// command
						PROG_CS(1);
						spi_transmit(CMD_READ);
						spi_transmit(out1buf[2]);
						spi_transmit(out1buf[1]);
						{
							uint8_t i = 32;
							dst = in1buf + 1;
							while(i--)
							{
								*dst = spi_transmit(0xFF);
								dst++;
							}
						}
						PROG_CS(0);
						in1buf[0] = PTOUT_READ;
						in1bc = 33;
						while(!(in1cs & 0x02));
					break;
					case PTIN_PRGOUT:
						if(mode != MODE_PROG)
							err_msg(0x80)
						// set PROG output
						PROG_OUT(out1buf[1]);
					break;
//
//	mode swap
//
					case PTIN_MODE:
						if(out1buf[1] < NUM_MODES)
							mode = out1buf[1];
						switch(mode)
						{
							case MODE_HIZ:
								PROG_OUT(0);
								SMCTRL = 0;
								P0DIR = 0b11111111;
								P0EXP = 0;
								P0ALT = 0;
							break;
							case MODE_LEDS:
								SMCTRL = 0;
								P0DIR = 0b11110000;
								P0EXP = 0;
								P0ALT = 0;
							break;
							case MODE_PROG:
								P0DIR = 0b11010100;
								PROG_CS(0);
								PROG_OUT(0);
								SMCTRL = 0b00010110; // 64 CLKDIV
								P0EXP = 1;
								P0ALT = 0b111;
								spi_transmit(0xFF);
							break;
						}
						
					break;
				}
				// done
				out1bc = 0xff;
			}
		}
		// check RF IRQ
		if(RFCE && RFF)
		{
			NRF_SetReg(NREG_STATUS, NSTAT_RX_DR | NSTAT_TX_DS | NSTAT_MAX_RT);
			RFF = 0;
			while(1)
			{
				uint8_t size;
				uint8_t temp = (NRF_ReadStatus() >> 1) & 7;
				if(temp > 5)
				{
//					NRF_Command(NCMD_FLUSH_RX);
					break;
				}
				size = NRF_CommandRead(NCMD_R_RX_PL_WID);
				if(size > 32)
				{
					NRF_Command(NCMD_FLUSH_RX);
					break;
				}
				out_rx->type = PTOUT_RX_PACKET;
				out_rx->pipe = temp;
				NRF_ReadData(out_rx->buff, size);
				in1bc = sizeof(p_rx_t) - 32 + size;
				while(!(in1cs & 0x02));
			}
		}
	}
}
