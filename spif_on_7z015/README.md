SpiNNaker interface implemented on Zynq 7000
============================================

This repository contains an FPGA design to interface an event-based
sensor, such as an event camera or DVS, to a SpiNNaker system through
a High-Speed Serial Link (HSSL).

The design targets the Trenz Electronic TE7015-04-15 board (ZYNQ 7000).
It connects to the SpiNNaker system using a SATA cable.

The design is built on top of
[spI/O](https://github.com/SpiNNakerManchester/spio), the library of
FPGA designs and reusable modules for I/O and internal connectivity in
SpiNNaker systems.

Authors
-------

The design in this repository is largely the work of:

* Luis A. Plana (The University of Manchester)

Acknowledgments
---------------

Ongoing development of the SpiNNaker Project is supported by
the EU ICT Flagship Human Brain Project under Grant H2020-945539.
LA Plana is supported by the RAIN Hub, which is
funded by the Industrial Strategy Challenge Fund, part of the government’s
modern Industrial Strategy. The fund is delivered by UK Research and
Innovation and managed by EPSRC under grant EP/R026084/1.

We gratefully acknowledge these institutions for their support.
