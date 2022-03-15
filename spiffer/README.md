spiffer: spif UDP/USB listener
===============================

This repository contains software that implements a multithreaded UDP/USB listener that forwards incoming events to SpiNNaker through the spif hardware.


Current implementation features and limitations
-----------------------------------------------

- started automatically during boot as a Linux service unit brokered by systemd,
- USB device connections signalled by udev,
- discovers and supports Davis cameras only. Adding camera types supported by the `libcaer` library is not a big task. Support for other cameras might be more challenging,
- switches between UDP and USB listening as a result of USB camera connections and disconnections.
- listens on UDP port 3333 and forwards events to spif pipe0,
- listens on UDP port 3334 and forwards events to spif pipe1,
- sorts cameras by serial number and connects the lower number to pipe0 and the higher number to pipe1,
- transfers events arriving on UDP ports _as is_ to spif,
- maps events arriving on USB to [`spiffer` events](#evt_fmt) before transferring them to spif,
- writes a world-readable, root-writable transient log file (`/tmp/spiffer.log`). The log is used to report fatal errors during setup (UDP ports, USB devices and such) and listener status when USB devices connect or disconnect. 


<a name="evt_fmt"></a>`spiffer` events
--------------------------------------

`spiffer` accepts events through Ethernet UDP ports and USB and transfers them to `spif`.

- events arriving on UDP ports are transferred _as is_ to `spif`,
- events arriving on USB are mapped to `spiffer` events before being transferred to `spif`.

`spiffer` events are 32-bit numbers with the following bit assignments:

|  bits   | event field  | comment               |
|--------:|:------------:|:----------------------|
|    [31] |  timestamp   | 0: present, 1: absent |
| [30:16] | x coordinate |                       |
|    [15] | polarity     |                       |
|  [14:0] | y coordinate |                       |

In the current implementation timestamps are __not__ used - time models itself!


Compilation
-----------

> use `make` to compile `spiffer` 

`spiffer` has been compiled, installed and tested on the Ubuntu 20.04 distribution of Linux, on 32- and 64-bit ARM platforms.

`spiffer` requires the pthread library.

`spiffer` requires the libcaer library to support Davis cameras.


Installation
------------

`spiffer` operation requires setting of udev rules and creating systemd service unit.

> use `sudo make install` to install  `spiffer` 

The following steps are taken:

1. `spiffer` is copied to /usr/local/bin/

2. `99-spiffer.rules` is copied to /etc/udev/rules.d/

3. `udevadm control --reload` reloads udev rules

4. `spiffer.service` is copied to /lib/systemd/system/

5. links to `spiffer.service` are created from:

- /etc/systemd/system/
- /etc/systemd/system/multi-user.target.wants/

6. `/tmp/spiffer.log` is reset

7. `systemctl restart spiffer` restarts spiffer


Extending support to new USB devices
------------------------------------

1. new rules should be added to `99-spiffer.rules` to detect device connections.

2. device discovery support should be added to function `usb_discover_devs`.

The following functionality is necessary but may not be sufficient:

- a function to open the device and return a device handle.
- a function to read the device serial number or unique ID.
- a function to read events from the device.
- a function to close the device.
