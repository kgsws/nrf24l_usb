#include <inttypes.h>

#ifndef __PROTO_H__
#define __PROTO_H__

// USB settings

#define MAX_DEVICES	16

#define	VENDOR_ID	0x16C0
#define	PRODUCT_ID	0x05DC

#define VENDOR_STRING	"kgsws"
#define PRODUCT_STRING	"nRF24L Transceiver"
#define PRODUCT_VERSION	"4.0"

//#define DEBUG_SCAN

typedef struct
{
	int pipe;
	int len;
	uint8_t data[32];
} packet_t;

typedef struct
{
	uint8_t channel;
	uint8_t rate;
	uint8_t crc;
	uint8_t adw;
} setconf_t;

typedef struct
{
	uint8_t size;
	uint8_t addr[5];
} fakepipe_t;

typedef struct
{
	fakepipe_t pipe[6];
} setpipe_t;

extern struct usb_device *device_list[MAX_DEVICES];
extern int devices_count;
extern usb_dev_handle *handle;

void P_Scan();
int P_Init(struct usb_device *dev);
void P_Deinit();

void P_SetSettings(uint8_t channel, uint8_t rate, uint8_t crc, uint8_t adw);
void P_GetSettings();

void P_SetPipes(setpipe_t *set);
void P_GetPipes();

void P_TxAddress(uint8_t *addr);
void P_TxData(uint8_t *buff, int len);

void P_RxMode(int enable);

#endif
