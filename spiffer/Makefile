INCLUDE_DIR = ../test_code/include

CFLAGS += -I$(INCLUDE_DIR)
USBCFLAGS += $(CFLAGS) -std=c11 -pedantic -Wall -Wextra -O2

HFILES = $(INCLUDE_DIR)/spif.h $(INCLUDE_DIR)/spif_remote.h

spiffer: spiffer.c
	gcc $(USBCFLAGS) -o spiffer spiffer.c -D_DEFAULT_SOURCE=1 -lcaer -lpthread

install: spiffer 99-spiffer.rules spiffer.service
	cp -p spiffer /usr/local/bin/
	cp -p 99-spiffer.rules /etc/udev/rules.d/
	udevadm control --reload
	cp -p spiffer.service /lib/systemd/system/
	ln -fn /lib/systemd/system/spiffer.service /etc/systemd/system/spiffer.service
	ln -fn /lib/systemd/system/spiffer.service /etc/systemd/system/multi-user.target.wants/spiffer.service
	rm -f /tmp/spiffer.log
	systemctl restart spiffer

tidy:
	/bin/rm -f *.0 *.elf *~

clean: tidy
	/bin/rm -f $(APPS)
