spifffer: spif UDP/USB listener
===============================

This repository contains software that implements a multithreaded UDP/USB listener that forwards incoming events to SpiNNaker through the spif hardware.

`spiffer` is installed as a Linux service unit brokered by systemd.

`spiffer` has been compiled, installed and tested on the Ubuntu 20.04 distribution of Linux, on 32- and 64-bit ARM platforms.


Compilation
-----------

`spiffer` requires the pthread library.

Support for Iniviation Davis cameras requires the libcaer library.


Installation
------------

udev support for USB device detection:
/etc/udev/rules.d/99-spiffer.rules

systemd service:
/lib/systemd/system/spiffer.service
/etc/systemd/system/spiffer.service
