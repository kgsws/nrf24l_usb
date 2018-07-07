TARGET   = flash.ihx
MCU      = NRF24LU1
CFLAGS   = --model-small --std-sdcc99 -D$(MCU) -I../include
LFLAGS   = --model-small --code-loc 0x0000 --code-size 0x4000 --xram-loc 0x8000 --xram-size 0x800
ASFLAGS  = -plosgff
RELFILES = main.rel radio.rel usb.rel usb_config.rel

$(TARGET): $(RELFILES)
	sdcc $(CFLAGS) $(LFLAGS) $(RELFILES) -o $(TARGET)
	objcopy -Iihex -Obinary $(TARGET) firmware.bin

%.rel: %.c
	sdcc $(CFLAGS) -c $<

%.rel: %.s
	sdas8051 $(ASFLAGS) $@ $< 

clean:
	rm --force *.ihx *.lnk *.lst *.map *.rel *.rst *.sym *.mem *.asm *.lk firmware.bin flash.cdb flash.omf

all: clean main

flash:
	nrfprog write firmware.bin
