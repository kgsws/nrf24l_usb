# nRF24L tools
This repository contains tools for nRF24L development.
Successful compilation should give you access to nRF24L programming and RF debugging.

- firmware
  - custom firmware for nRF24LU1 based dongle
  - since only 4 wires are used for programming, any dongle with these exposed will do
  - there are many compatible dongles found on ebay
  - compile using SDCC
  - this firmware supports
    - programming of any other nRF24L device
    - packet send and recv
  - not all nRF24L radio features are supported
- rxtx
  - simple testing GTK+ packet sniffer and sender
  - requires libgtk+-3.0 and libusb
  - (try it with two dongles)
- prog
  - simple command line programmer app
  - requires libusb


#### credits
Carson Morrow for working USB library for nRF24LU1. See `firmware/usb.c` for more info.

