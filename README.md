spif: SpiNNaker peripheral interface
====================================

![SpiNNaker Peripheral Interface](spif.png)

This repository contains an FPGA design to interface event-based peripherals, such as event cameras or DVS, to a SpiNNaker system through a High-Speed Serial Link (HSSL).

The repository contains two designs that target different FPGA platforms:

- `spif_on_zu9eg` -- Xilinx ZCU102 development board (ZYNQ Ultrascale+)
- `spif_on_7z015` -- Trenz Electronic TE0715-04-15 board (ZYNQ 7000).
- `spif_common`   -- contains the modules that are used in both designs.

The designs are built on top of [spI/O](https://github.com/SpiNNakerManchester/spio), the library of FPGA designs and reusable modules for I/O and internal connectivity in SpiNNaker systems.

The designs were implemented and verified using `Xilinx Vivado version 2019.2`.

Both platforms connect to the SpiNNaker system using a SATA cable and the spiNNlink protocol used in SpiNNaker systems for board-to-board interconnect.


<a name="evt_fmt"></a>`spif` event format
-----------------------------------------

`spif` accepts events through Ethernet UDP ports and USB and transfers them to SpiNNaker.

- events arriving on UDP ports are transferred _as is_ to SpiNNaker,
- events arriving on USB are mapped to `spif` events before being transferred to SpiNNaker.

`spif` events are 32-bit numbers with the following bit assignments:

|  bits   | event field  | comment               |
|--------:|:------------:|:----------------------|
|    [31] |  timestamp   | 0: present, 1: absent |
| [30:16] | x coordinate |                       |
|    [15] | polarity     |                       |
|  [14:0] | y coordinate |                       |

In the current implementation timestamps are never used - time models itself!


spiNNlink
---------

spiNNlink is the high-speed, serial (hss) board-to-board SpiNNaker interconnect.  It is implemented on three Spartan-6 FPGAs present on the SpiNN-5 board and is designed to provide transparent board-to-board connectivity. The details of the spiNNlink protocol are described in the [spiNNlink Frame Transport Specification](http://spinnakermanchester.github.io/docs/spiNNlink_frame_transport.pdf).

Verilog code for the spiNNlink modules is located in [spio/modules/hss_multiplexer](https://github.com/SpiNNakerManchester/spio/tree/master/modules/hss_multiplexer). See [spio/designs/spinnaker_fpgas](https://github.com/SpiNNakerManchester/spio/tree/master/designs/spinnaker_fpgas) for an example of how to use spiNNlink.

The following open-access publication describes the SpiNNlink specification and implementation:

LA Plana, J Garside, J Heathcote, J Pepper, S Temple, S Davidson, M Luján and S Furber, *spiNNlink: FPGA-Based Interconnect for the Million-Core SpiNNaker System*, in IEEE Access, vol. 8, pp. 84918-84928, 2020, doi: [10.1109/ACCESS.2020.2991038](https://doi.org/10.1109/ACCESS.2020.2991038).


Authors
-------

The designs in this repository are largely the work of:

* LA Plana (The University of Manchester)

with input from J Conradt (KTH), JE Pedersen (KTH), JP Romero Bermúdez (KTH), AG Rowley (UManchester), S Davidson (UManchester) and O Rhodes (UManchester).


Acknowledgments
---------------

The designs in this reposiory are being developed as part of a collaboration between the Advanced Processor Technologies group at The University of Manchester and the Neurocomputing Systems Lab at KTH Royal Institute of Technology, Stockholm.

Ongoing development of the SpiNNaker Project is supported by the EU ICT Flagship Human Brain Project under Grant H2020-945539. LA Plana has been supported, in part, by the RAIN Hub, which is funded by the Industrial Strategy Challenge Fund, part of the government’s modern Industrial Strategy. The fund is delivered by UK Research and Innovation and managed by EPSRC under grant EP/R026084/1.

We gratefully acknowledge these institutions for their support.
