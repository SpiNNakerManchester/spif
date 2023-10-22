spiffer: spif UDP/USB listener
===============================

This repository contains software that implements a multithreaded UDP/USB listener that forwards incoming events to SpiNNaker through the spif hardware.


Current implementation features and limitations
-----------------------------------------------

- started automatically during boot as a Linux service unit brokered by systemd,
- USB device connections signalled by udev,
- discovers and supports Inivation Davis cameras through the `libcaer` library. Adding other camera types supported by the library should not be a big task,
- discovers and supports Prophesee cameras through the `OpenEB` library.
- See below to add support for other cameras might be more challenging,
- switches between UDP and USB listening as a result of USB camera connections and disconnections.
- listens on UDP port 3333 and forwards events to spif pipe0,
- listens on UDP port 3334 and forwards events to spif pipe1,
- sorts USB cameras by serial number and connects the lower number to pipe0 and the higher number to pipe1,
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

`spiffer` has been compiled, installed and tested on the Ubuntu 20.04 distribution of Linux on both 32- and 64-bit ARM platforms.

`spiffer` requires cmake - version 3.5 or higher.

`spiffer` requires the pthread library.

`spiffer` requires the libcaer library to support Inivation Davis cameras. If not present, `spiffer` will compile without Inivation camera support.

`spiffer` requires the OpenEB library to support Prophesee cameras. If not present, `spiffer` will compile without Prophesee camera support.

Installation
------------

`spiffer` operation requires setting new udev rules and creating a systemd service unit.

> use `sudo make install` to install  `spiffer`


Supporting new USB devices
--------------------------

1. New UDEV rules should be added to `99-spiffer.rules` to detect device connections.

2. The following functionality is necessary but may not be sufficient:

- a function to discover and open the device and return a device handle,
- a function to read the device serial number or unique ID,
- a function to read events from the device and translate them into [`spiffer` events](#evt_fmt),
- a function to close the device.

See [`spiffer_caer_support.cpp`](https://github.com/SpiNNakerManchester/spif/blob/master/spiffer/spiffer_caer_support.cpp) and [`spiffer_meta_supporrt.cpp`](https://github.com/SpiNNakerManchester/spif/blob/master/spiffer/spiffer_meta_support.cpp) for examples of the above functionality for Inivation and Prophesee cameras, respectively.
