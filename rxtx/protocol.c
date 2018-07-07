#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <pthread.h>
#include <usb.h>
#include "protocol.h"

#define TXF_ADDR	1
#define TXF_DATA	2

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

gboolean StatusGtk(void *got);
gboolean ConfigGtk(setconf_t *got);
gboolean PipesGtk(setpipe_t *got);
gboolean AddPacketGtk(packet_t *got);

extern int psizes[6];
extern uint8_t config_reg;
extern uint8_t dynlen;

struct usb_device *device_list[MAX_DEVICES];
int devices_count = 0;
extern GtkListStore *dev_store;

usb_dev_handle *handle = NULL;

volatile int irq_run;
pthread_t dev_th;
uint8_t irq_buff[64];

#define irq_rx		((p_rx_t*)irq_buff)
#define irq_conf	((config_t*)irq_buff)
#define irq_pipe	((pipes_t*)irq_buff)

int irq_thread(void *unused)
{
	int i, size;
	packet_t *packet;
	setconf_t *setconf;
	setpipe_t *setpipe;

	irq_run = 1;
	while(irq_run)
	{
		i = usb_interrupt_read(handle, 0x81, irq_buff, sizeof(irq_buff), 0);
		if(i > 0)
		{
			switch(irq_buff[0])
			{
				case PTOUT_SETTINGS:
					setconf = malloc(sizeof(setconf_t));
					if(setconf)
					{
						setconf->channel = irq_conf->channel;
						setconf->rate = irq_conf->rate;
						setconf->crc = irq_conf->crc;
						setconf->adw = irq_conf->adw;
						g_idle_add((GSourceFunc)ConfigGtk, setconf);
					}
				break;
				case PTOUT_PIPES:
					setpipe = malloc(sizeof(setpipe_t));
					if(setpipe)
					{
						// 0
						setpipe->pipe[0].size = irq_pipe->pipl[0].size;
						memcpy(setpipe->pipe[0].addr, irq_pipe->pipl[0].addr, 5);
						// 1
						setpipe->pipe[1].size = irq_pipe->pipl[1].size;
						memcpy(setpipe->pipe[1].addr, irq_pipe->pipl[1].addr, 5);
						// fake-fill 2 - 5
						for(i = 2; i < 6; i++)
							memcpy(setpipe->pipe[i].addr + 1, irq_pipe->pipl[1].addr + 1, 4);
						// 2
						setpipe->pipe[2].size = irq_pipe->pips[0].size;
						setpipe->pipe[2].addr[0] = irq_pipe->pips[0].addr;
						// 3
						setpipe->pipe[3].size = irq_pipe->pips[1].size;
						setpipe->pipe[3].addr[0] = irq_pipe->pips[1].addr;
						// 4
						setpipe->pipe[4].size = irq_pipe->pips[2].size;
						setpipe->pipe[4].addr[0] = irq_pipe->pips[2].addr;
						// 5
						setpipe->pipe[5].size = irq_pipe->pips[3].size;
						setpipe->pipe[5].addr[0] = irq_pipe->pips[3].addr;
						// OK
						g_idle_add((GSourceFunc)PipesGtk, setpipe);
					}
				break;
				case PTOUT_RX_PACKET: 
					// request packet
					packet = malloc(sizeof(packet_t));
					if(packet)
					{
						size = i - (sizeof(p_rx_t) - 32);
						packet->pipe = irq_rx->pipe;
						packet->len = size;
						memcpy(packet->data, irq_rx->buff, size);
						g_idle_add((GSourceFunc)AddPacketGtk, packet);
					}
				break;
			}
		}
//		printf("IRQ_TH: %i; %i\n", i, cmd);
	}
	return 0;
}

void P_Scan()
{
	struct usb_bus *busses;
	struct usb_bus *bus;
	struct usb_device *device;
	usb_dev_handle *dh = NULL;
	char temp[32];
	char serial[32];

	devices_count = 0;
	usb_find_busses();
	usb_find_devices();
	busses = usb_get_busses();
	for(bus = busses; bus && !dh; bus = bus->next)
	{
		for(device = bus->devices; device && !dh; device = device->next)
		{
			if(device->descriptor.idVendor == VENDOR_ID && device->descriptor.idProduct == PRODUCT_ID)
			{
#ifdef DEBUG_SCAN
				printf("found device ");
#endif
				dh = usb_open(device);
				if(!dh)
				{
#ifdef DEBUG_SCAN
					printf("- failed to open\n");
#endif
					continue;
				}
				if(usb_get_string_simple(dh, 1, temp, 32) <= 0)
				{
#ifdef DEBUG_SCAN
					printf("vendor string error\n");
#endif
					usb_close(dh);
					dh = NULL;
					continue;
				}
#ifdef DEBUG_SCAN
				printf("vendor: %s; ", temp);
#endif
				if(strcmp(temp, VENDOR_STRING))
				{
#ifdef DEBUG_SCAN
					printf("- unsupported\n", temp);
#endif
					usb_close(dh);
					dh = NULL;
					continue;
				}
				if(usb_get_string_simple(dh, 2, temp, 32) <= 0)
				{
#ifdef DEBUG_SCAN
					printf("product string error\n");
#endif
					usb_close(dh);
					dh = NULL;
					continue;
				}
#ifdef DEBUG_SCAN
				printf("product: %s; ", temp);
#endif
				if(strncmp(temp, PRODUCT_STRING, sizeof(PRODUCT_STRING) - 1))
				{
#ifdef DEBUG_SCAN
					printf("- wrong product\n", temp);
#endif
					usb_close(dh);
					dh = NULL;
					continue;
				}
				if(usb_get_string_simple(dh, 3, serial, 32) <= 0)
				{
#ifdef DEBUG_SCAN
					printf("serial string error\n");
#endif
					usb_close(dh);
					dh = NULL;
					continue;
				}
#ifdef DEBUG_SCAN
				printf("ID: %s\n", serial);
#endif
				usb_close(dh);
				// add now add to list
				if(strlen(temp) < sizeof(PRODUCT_STRING) + sizeof(PRODUCT_VERSION) - 1 || strcmp(temp + sizeof(PRODUCT_STRING), PRODUCT_VERSION))
				{
					// invalid
					gtk_list_store_insert_with_values(dev_store, NULL, -1, 0, "red", 1, serial, -1);
					device_list[devices_count] = NULL;
					devices_count++;
				} else {
					// valid
					gtk_list_store_insert_with_values(dev_store, NULL, -1, 0, "green", 1, serial, -1);
					device_list[devices_count] = device;
					devices_count++;
				}
				if(devices_count < MAX_DEVICES)
					// force scan end
					dh = NULL;
			}
		}
	}
}

int P_Init(struct usb_device *dev)
{
	uint8_t cmd;

	handle = usb_open(dev);
	if(!handle)
	{
		StatusGtk((void*)0);
		return 1;
	}

	if(usb_claim_interface(handle, 0))
	{
		StatusGtk((void*)1);
		return 1;
	}

	pthread_create(&dev_th, NULL, (void*)irq_thread, NULL);
	usleep(10*1000);

	P_GetSettings();

	return 0;
}

void P_Deinit()
{
	irq_run = 0;
	P_RxMode(FALSE);
	usb_release_interface(handle, 0);
	pthread_join(dev_th, NULL);
	usb_close(handle);
	handle = NULL;
}

void P_GetSettings()
{
	uint8_t req[1] = {PTIN_GET_SETTINGS};
	usb_bulk_write(handle, 0x01, (void*)req, sizeof(req), 100);
}

void P_GetPipes()
{
	uint8_t req[1] = {PTIN_GET_PIPES};
	usb_bulk_write(handle, 0x01, (void*)req, sizeof(req), 100);
}

void P_SetSettings(uint8_t channel, uint8_t rate, uint8_t crc, uint8_t adw)
{
	config_t req = {PTIN_SETTINGS, channel, rate, crc, adw};
	usb_bulk_write(handle, 0x01, (void*)&req, sizeof(req), 100);
}

void P_SetPipes(setpipe_t *set)
{
	pipes_t pipes;

	pipes.type = PTIN_PIPES;
	// 0
	pipes.pipl[0].size = set->pipe[0].size;
	memcpy(pipes.pipl[0].addr, set->pipe[0].addr, 5);
	// 1
	pipes.pipl[1].size = set->pipe[1].size;
	memcpy(pipes.pipl[1].addr, set->pipe[1].addr, 5);
	// 2
	pipes.pips[0].size = set->pipe[2].size;
	pipes.pips[0].addr = set->pipe[2].addr[0];
	// 3
	pipes.pips[1].size = set->pipe[3].size;
	pipes.pips[1].addr = set->pipe[3].addr[0];
	// 4
	pipes.pips[2].size = set->pipe[4].size;
	pipes.pips[2].addr = set->pipe[4].addr[0];
	// 5
	pipes.pips[3].size = set->pipe[5].size;
	pipes.pips[3].addr = set->pipe[5].addr[0];

	usb_bulk_write(handle, 0x01, (void*)&pipes, sizeof(pipes_t), 100);
}

void P_RxMode(int enable)
{
	uint8_t req[2] = {PTIN_RX_MODE, !!enable};
	usb_bulk_write(handle, 0x01, (void*)req, sizeof(req), 100);
}

void P_TxAddress(uint8_t *addr)
{
	uint8_t req[6] = {PTIN_TX_SETADDR};

	memcpy(req + 1, addr, 5);
	usb_bulk_write(handle, 0x01, (void*)req, sizeof(req), 100);
}

void P_TxData(uint8_t *buff, int len)
{
	uint8_t req[33] = {PTIN_TX_PACKET};

	if(len > 32)
		len = 32;

	memcpy(req + 1, buff, len);
	usb_bulk_write(handle, 0x01, (void*)&req, len + 1, 100);
}

