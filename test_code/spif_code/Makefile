INCLUDE_DIR = ../include

CFLAGS += -I$(INCLUDE_DIR)
USBCFLAGS += $(CFLAGS) -std=c11 -pedantic -Wall -Wextra -O2

APPS = udp2SpiNN usb2SpiNN read_reg_bank write_reg_bank
HFILES = $(INCLUDE_DIR)/spif.h $(INCLUDE_DIR)/spif_remote.h

all: $(APPS)

udp2SpiNN: udp2SpiNN.c $(HFILES)
	gcc $(CFLAGS) -o $@ $@.c

usb2SpiNN: usb2SpiNN.c $(HFILES)
	gcc $(USBCFLAGS) -o usb2SpiNN usb2SpiNN.c -D_DEFAULT_SOURCE=1 -lcaer

read_reg_bank: read_reg_bank.c $(HFILES)
	gcc $(CFLAGS) -o $@ $@.c

write_reg_bank: write_reg_bank.c $(HFILES)
	gcc $(CFLAGS) -o $@ $@.c

clean:
	/bin/rm -f $(APPS)
