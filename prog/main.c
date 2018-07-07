// kgsws' USB nRF programmer
//

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <usb.h>

#define DEBUG_SCAN

#define	VENDOR_ID	0x16C0
#define	PRODUCT_ID	0x05DC

#define VENDOR_STRING	"kgsws"
#define PRODUCT_STRING	"nRF24L Transceiver"
#define PRODUCT_VERSION	"4.0"

#define FSR_RESERVED0 (1 << 0)
#define FSR_RDISIP    (1 << 1)
#define FSR_RDISMB    (1 << 2)
#define FSR_INFEN     (1 << 3)
#define FSR_RDYN      (1 << 4)
#define FSR_WEN       (1 << 5)
#define FSR_STP       (1 << 6)
#define FSR_DBG   (1 << 7)

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

enum
{
	MODE_HIZ,
	MODE_LEDS,
	MODE_PROG,
	NUM_MODES
};

enum
{
	ACT_NONE,
	ACT_READ,
	ACT_WRITE,
	ACT_SETID,
	ACT_UNLOCK,
	// flags
	ACT_DATAPAGE = 0x20, // read / write data page
	ACT_INFOPAGE = 0x40, // read / write info page
	ACT_ERASE = 0x80 // erase chip
};

usb_dev_handle *handle;

volatile int irq_run;
volatile int irq_wait = -1;
uint8_t irq_buff[64];
volatile uint8_t *irqret = irq_buff + 1;

int error = 0;

uint8_t infopage[512];
uint8_t page[512];

uint8_t fsr;

usb_dev_handle *P_Scan()
{
	struct usb_bus *busses;
	struct usb_bus *bus;
	struct usb_device *device;
	usb_dev_handle *dh = NULL;
	char temp[32];
	char serial[32];

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
				printf("- found device; ");
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
				// add now add to list
				if(!(strlen(temp) < sizeof(PRODUCT_STRING) + sizeof(PRODUCT_VERSION) - 1 || strcmp(temp + sizeof(PRODUCT_STRING), PRODUCT_VERSION)))
				{
					if(usb_claim_interface(dh, 0))
					{
						printf("- found programmer %s, unable to claim ... skipping\n", serial);
					} else {
						printf("- using programmer ID: %s\n", serial);
						return dh;
					}
				}
				usb_close(dh);
			}
		}
	}
	return NULL;
}

int irq_thread(void *unused)
{
	int i, size;

	irq_run = 1;
	while(irq_run)
	{
		i = usb_interrupt_read(handle, 0x81, irq_buff, sizeof(irq_buff), 0);
		if(i > 0)
		{
//printf("IRQin: %i\n", irq_buff[0]);
			irq_wait = irq_buff[0];
		}
	}
	return 0;
}

//
//	USB commands
//

void P_RxMode(int enable)
{
	uint8_t req[] = {PTIN_RX_MODE, !!enable};
	usb_bulk_write(handle, 0x01, (void*)req, sizeof(req), 100);
}

void P_ModuleMode(int mode)
{
	uint8_t req[] = {PTIN_MODE, mode};
	usb_bulk_write(handle, 0x01, (void*)req, sizeof(req), 100);
}

void P_FSR_set(uint8_t fsr)
{
	uint8_t req[] = {PTIN_SET_FSR, fsr};
	if(usb_bulk_write(handle, 0x01, (void*)req, sizeof(req), 100) != sizeof(req))
		error = 1;
}

uint8_t P_FSR_get()
{
	int count = 100;
	uint8_t req[] = {PTIN_GET_FSR};

	irq_wait = -1;
	if(usb_bulk_write(handle, 0x01, (void*)req, sizeof(req), 100) != sizeof(req))
	{
		error = 1;
		return 0xFF;
	}
	while(count--)
	{
		if(irq_wait == PTOUT_FSR)
			break;
		if(irq_wait == PTOUT_MESSAGE)
			break;
		usleep(1*1000);
	}
	if(!count)
		error = 2;
	if(irq_wait == PTOUT_MESSAGE)
		error = 3;
	return *irqret;
}

void P_ErasePage(int page)
{
	uint8_t req[] = {PTIN_ERASE_PAGE, page};
	if(usb_bulk_write(handle, 0x01, (void*)req, sizeof(req), 100) != sizeof(req))
	{
		printf("- page erase failed\n");
		error = 1;
	}
}

void P_EraseAll()
{
	uint8_t req[] = {PTIN_ERASE_ALL};
	if(usb_bulk_write(handle, 0x01, (void*)req, sizeof(req), 100) != sizeof(req))
	{
		printf("- full erase failed\n");
		error = 1;
	}
}

void P_read(uint16_t addr, uint8_t *buff)
{
	int count = 100;
	uint8_t req[] = {PTIN_READ, addr, addr >> 8};

	irq_wait = -1;
	if(usb_bulk_write(handle, 0x01, (void*)req, sizeof(req), 100) != sizeof(req))
	{
		error = 1;
		return;
	}
	while(count--)
	{
		if(irq_wait == PTOUT_READ)
			break;
		if(irq_wait == PTOUT_MESSAGE)
			break;
		usleep(1*1000);
	}
	if(!count)
		error = 2;
	if(irq_wait == PTOUT_MESSAGE)
		error = 3;
	memcpy(buff, (void*)irqret, 32);
}

void P_ReadPage(uint16_t addr, uint8_t *dst)
{
	int count = 512/32;

	while(count--)
	{
		P_read(addr, dst);
		if(error)
			break;
		dst += 32;
		addr += 32;
	}
}

void P_write(uint16_t addr, uint8_t *buff)
{
	uint8_t req[35] = {PTIN_PROGRAM, addr, addr >> 8};

	memcpy(req + 3, buff, 32);
	if(usb_bulk_write(handle, 0x01, (void*)req, sizeof(req), 100) != sizeof(req))
	{
		printf("- write error\n");
		error = 1;
	}
}

void P_WritePage(uint16_t addr, uint8_t *buff)
{
	int count = 512/32;

	while(count--)
	{
		P_write(addr, buff);
		if(error)
			break;
		addr += 32;
		buff += 32;
	}
}

//
//
//

void P_read_infopage(int count)
{
	uint8_t *dst = infopage;
	uint16_t addr = 0;

	while(count--)
	{
		P_read(addr, dst);
		if(error)
			break;
		dst += 32;
		addr += 32;
	}
}

int P_fsr_check(int infopage)
{
	fsr = P_FSR_get();
	if(error)
	{
		printf("- failed to read FSR\n");
		return 1;
	}
	switch(infopage)
	{
		case 0:
			if(fsr == 0xFF)
			{
				printf("- invalid FSR, is device connected?\n");
				return 1;
			}
		break;
		case 1:
			if(!(fsr & FSR_INFEN))
			{
				printf("- info page mode failed\n");
				return 1;
			}
		break;
		case 2:
			if(fsr & FSR_INFEN)
			{
				printf("- normal page mode failed\n");
				return 1;
			}
		break;
	}

	return 0;
}

int get_hex(char in)
{
	if(in >= '0' && in <= '9')
		return in - '0';
	if(in >= 'A' && in <= 'F')
		return in - 'A' + 10;
	if(in >= 'a' && in <= 'f')
		return in - 'a' + 10;
	return -1;
}

int parse_id(char *in, uint8_t *out)
{
	uint8_t num;
	int i, j, n;

	for(i = 0; i < 10; i++)
	{
		if(!*in)
			return 1;
		n = get_hex(*in);
		if(n < 0)
			return 1;
		if(i & 1)
		{
			*out |= n;
			out++;
		} else
			*out = n << 4;
		j = !j;
		in++;
	}
	if(*in)
		return 1;
	return 0;
}

int main(int argc, char **argv)
{
	int action = ACT_NONE;
	pthread_t dev_th;
	uint8_t new_id[5];
	uint8_t buff[32768];
	FILE *f;
	// flashop
	int i, addr, count, size;
	uint8_t *dst;

	printf("kgsws' nRF programmer\n");

	memset(buff, 0xFF, sizeof(buff));
	if(argc > 1)
	{
		if(!strcmp(argv[1], "erase"))
		{
			action = ACT_ERASE;
		} else
		if(!strcmp(argv[1], "read"))
		{
			if(argc < 3)
			{
				printf("- no output file specified\n");
				return 1;
			}
			action = ACT_READ;
		} else
		if(!strcmp(argv[1], "write"))
		{
			if(argc < 3)
			{
				printf("- no input file specified\n");
				return 1;
			}
			f = fopen(argv[2], "rb");
			if(!f)
			{
				printf("- failed to open input file\n");
				return 1;
			}
			size = fread(buff, 1, sizeof(buff), f);
			fclose(f);
			if(size <= 0)
			{
				printf("- input file error\n");
				return 1;
			}
			action = ACT_WRITE;
			if(argc > 3 && !strcmp(argv[3], "erase"))
				action |= ACT_ERASE;
		} else
		if(!strcmp(argv[1], "datar")) // storage page (0xFC00 - 0xFFFF)
		{
			if(argc < 3)
			{
				printf("- no output file specified\n");
				return 1;
			}
			action = ACT_READ | ACT_DATAPAGE;
		} else
		if(!strcmp(argv[1], "dataw")) // storage page (0xFC00 - 0xFFFF)
		{
			if(argc < 3)
			{
				printf("- no input file specified\n");
				return 1;
			}
			f = fopen(argv[2], "rb");
			if(!f)
			{
				printf("- failed to open input file\n");
				return 1;
			}
			size = fread(buff, 1, sizeof(buff), f);
			fclose(f);
			if(size <= 0)
			{
				printf("- input file error\n");
				return 1;
			}
			action = ACT_WRITE | ACT_DATAPAGE;
			if(argc > 3 && !strcmp(argv[3], "erase"))
				action |= ACT_ERASE;
		} else		if(!strcmp(argv[1], "id"))
		{
			if(argc < 3)
			{
				printf("- no ID specified\n");
				return 1;
			}
			if(parse_id(argv[2], new_id))
			{
				printf("- invalid ID specified\n");
				return 1;
			}
			action = ACT_SETID | ACT_INFOPAGE;
		} else
		if(!strcmp(argv[1], "unlock"))
		{
			action = ACT_UNLOCK | ACT_INFOPAGE;
		} else
		if(!strcmp(argv[1], "infor"))
		{
			if(argc < 3)
			{
				printf("- no output file specified\n");
				return 1;
			}
			action = ACT_READ | ACT_INFOPAGE;
		} else
		if(!strcmp(argv[1], "infow"))
		{
			if(argc < 3)
			{
				printf("- no input file specified\n");
				return 1;
			}
			f = fopen(argv[2], "rb");
			if(!f)
			{
				printf("- failed to open input file\n");
				return 1;
			}
			size = fread(buff, 1, sizeof(buff), f);
			fclose(f);
			if(size != 512)
			{
				printf("- input file error, 512b required\n");
				return 1;
			}
			action = ACT_WRITE | ACT_INFOPAGE;
		} else
		{
			printf( "usage: %s action [path [erase]]\n" \
				"Supported actions:\n" \
				"\terase:\terase flash\n" \
				"\tread:\tread from flash to file\n" \
				"\twrite:\twrite from file to flash (supports 'erase' as third argument)\n" \
				"\tid:\tset new device ID\n" \
				"\tunlock:\terase and unlock locked device\n" \
				"\tinfor:\tread info page\n" \
				"\tinfow:\twrite new info page\n" \
				"\tunlock:\terase flash and modify infopage\n" \
				"\tdatar:\tread data page (0xFC00 - 0xFFFF)\n" \
				"\tdataw:\twrite data page (0xFC00 - 0xFFFF)\n"
				, argv[0]);
			return 1;
		}
	}

	usb_init();

	handle = P_Scan();
	if(!handle)
	{
		printf("- unable to find programmer\n");
		return 1;
	}

//
//	standart init, common for every action
//

	// disable Rx mode, just in case
	P_RxMode(0);

	// set prog mode
	P_ModuleMode(MODE_PROG);

	pthread_create(&dev_th, NULL, (void*)irq_thread, NULL);
	usleep(1000);

	// check device presence
	if(P_fsr_check(0))
		goto exit;

	// show some info
	printf("- FSR = %i; DBG: %i; STP: %i; RDISMB: %i; RDISIP: %i\n", fsr, !!(fsr&FSR_DBG), !!(fsr&FSR_STP), !!(fsr&FSR_RDISMB), !!(fsr&FSR_RDISIP));

	// enable info page
	P_FSR_set(FSR_INFEN);

	// check info page mode
	if(P_fsr_check(1))
		goto exit;

	// read info page
	P_read_infopage(action & ACT_INFOPAGE ? 512/32 : 1);
	if(error)
	{
		printf("- failed to read info page\n");
		goto exit;
	}

	// print serial
	printf("- target ID: %02X%02X%02X%02X%02X\n", infopage[0x0B], infopage[0x0C], infopage[0x0D], infopage[0x0E], infopage[0x0F]);

	//! special case, chip unlock
	if(action == (ACT_UNLOCK | ACT_INFOPAGE))
	{
		printf("- unlock\n");
		// do full erase; this will unlock flash
		P_EraseAll();
		if(error)
			goto exit;
		// write back new info page
		infopage[0x20] = 0xFF;
		infopage[0x21] = 0xFF;
		infopage[0x22] = 0xFF;
		infopage[0x23] = 0xFF;
		infopage[0x24] = 0xFF;
		P_WritePage(0, infopage);
		if(error)
			goto exit;
	}
	//! special case, set new ID
	if(action == (ACT_SETID | ACT_INFOPAGE))
	{
		printf("- modify ID\n");
		// erase info page
		P_ErasePage(0);
		if(error)
			goto exit;
		// write back new info page
		memcpy(infopage + 0x0B, new_id, sizeof(new_id));
		P_WritePage(0, infopage);
		if(error)
			goto exit;
	}
	//! special case, read info page
	if(action == (ACT_READ | ACT_INFOPAGE))
	{
		action = ACT_NONE;
		f = fopen(argv[2], "wb");
		if(!f)
		{
			printf("- failed to create output file\n");
			goto exit;
		}
		fwrite(infopage, 1, sizeof(infopage), f);
		fclose(f);
	}
	//! special case, write info page
	if(action == (ACT_WRITE | ACT_INFOPAGE))
	{
		action = ACT_NONE;
		P_ErasePage(0);
		if(error)
			goto exit;
		P_WritePage(0, buff);
		if(error)
			goto exit;
		P_ReadPage(0, page);
		if(error)
			goto exit;
		if(memcmp(buff, page, 512))
		{
			printf("- page verification error\n");
			goto exit;
		}
	}

	// disable info page
	P_FSR_set(0);

	// check normal mode
	if(P_fsr_check(2))
		goto exit;

//
//	do specified action
//

	if(action & ACT_ERASE)
	{
		printf("- erasing memory\n");
		P_EraseAll();
		if(error)
			goto exit;
	}

	switch(action & 15)
	{
		case ACT_READ:
		{
			int base;
			int count;
			// read flash
			if(action & ACT_DATAPAGE)
			{
				addr = 0x4400;
				count = 2;
				base = 34;
			} else
			{
				addr = 0;
				count = 32;
				base = 0;
			}
			dst = buff;
			for(i = 0; i < count; i++)
			{
				printf("- reading page %i\n", i + base);
				P_ReadPage(addr, dst);
				if(error)
					goto exit;
				addr += 512;
				dst += 512;
			}
			f = fopen(argv[2], "wb");
			if(!f)
			{
				printf("- failed to create output file\n");
				goto exit;
			}
			fwrite(buff, 1, count * 512, f);
			fclose(f);
		}
		break;
		case ACT_WRITE:
		{
			int base;
			// write flash
			if(action & ACT_DATAPAGE)
			{
				addr = 0x4400;
				base = 34;
			} else
			{
				addr = 0;
				base = 0;
			}
			dst = buff;
			count = (size+511) / 512;
			for(i = 0; i < count; i++)
			{
				printf("- flash page %i\n", i + base);
				if(!(action & ACT_ERASE))
				{
					P_ErasePage(i + base);
					if(error)
						goto exit;
				}
				P_WritePage(addr, dst);
				if(error)
					goto exit;
				P_ReadPage(addr, page);
				if(error)
					goto exit;
				if(memcmp(dst, page, 512))
				{
					printf("- page verification error\n");
					goto exit;
				}
				addr += 512;
				dst += 512;
			}
		}
		break;
	}

	// finish
	printf("- finished\n");
exit:
	P_ModuleMode(MODE_HIZ);
	irq_run = 0;
	usb_release_interface(handle, 0);
	pthread_join(dev_th, NULL);
	usb_close(handle);

	if(error)
		return error;
	return 0;
}

