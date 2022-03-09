INCLUDE_DIR = ../test_code/include

CFLAGS += -I$(INCLUDE_DIR)
USBCFLAGS += $(CFLAGS) -std=c11 -pedantic -Wall -Wextra -O2

APPS = spiffer
HFILES = $(INCLUDE_DIR)/spif.h $(INCLUDE_DIR)/spif_remote.h

all: $(APPS)

spiffer: spiffer.c
	gcc $(USBCFLAGS) -o spiffer spiffer.c -D_DEFAULT_SOURCE=1 -lcaer -lpthread

tidy:
	/bin/rm -f *.0 *.elf *~

clean: tidy
	/bin/rm -f $(APPS)
