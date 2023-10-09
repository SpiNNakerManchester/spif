spiffer:
	mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=release ..
	cd build && make

install: spiffer 99-spiffer.rules spiffer.service
	-systemctl stop spiffer
	cp -p build/spiffer /usr/local/bin/
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

clean:
	/bin/rm -rf build
