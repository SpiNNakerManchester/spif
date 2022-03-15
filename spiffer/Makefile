INCLUDE_DIR = ../test_code/include

CFLAGS += -I$(INCLUDE_DIR)
USBCFLAGS += $(CFLAGS) -std=c11 -pedantic -Wall -Wextra -O2

HFILES = $(INCLUDE_DIR)/spif.h $(INCLUDE_DIR)/spif_remote.h

spiffer: spiffer.c
	gcc $(USBCFLAGS) -o spiffer spiffer.c -D_DEFAULT_SOURCE=1 -lcaer -lpthread

install: spiffer 99-spiffer.rules spiffer.service
	-systemctl stop spiffer
	cp -p spiffer /usr/local/bin/
	chmod a=rx /usr/local/bin/spiffer
	chown root:root /usr/local/bin/spiffer
	cp -p 99-spiffer.rules /etc/udev/rules.d/
	udevadm control --reload
	cp -p spiffer.service /lib/systemd/system/
	rm -f /etc/systemd/system/multi-user.target.wants/spiffer.service
	ln -fn /lib/systemd/system/spiffer.service /etc/systemd/system/spiffer.service
	rm -f /tmp/spiffer.log
	systemctl daemon-reload
	systemctl enable spiffer
	systemctl start spiffer

tidy:
	/bin/rm -f *.0 *.elf *~

clean: tidy
	/bin/rm -f spiffer
