// -------------------------------------------------------------------------
//  dvs_on_hssl_top
//
//  constants
//
// -------------------------------------------------------------------------
// AUTHOR
//  lap - luis.plana@manchester.ac.uk
// -------------------------------------------------------------------------
// DETAILS
//  Created on       : 28 Mar 2021
//  Last modified on : Sun 28 Mar 18:13:01 BST 2021
//  Last modified by : $Author: plana $
// -------------------------------------------------------------------------
// COPYRIGHT
//  Copyright (c) The University of Manchester, 2021.
//  SpiNNaker Project
//  Advanced Processor Technologies Group
//  School of Computer Science
// -------------------------------------------------------------------------


`ifndef DVS_ON_HSSL_TOP_H
`define DVS_ON_HSSL_TOP_H

//---------------------------------------------------------------
// FPGA selection
//NOTE: un-comment the required FPGA `define
//---------------------------------------------------------------
//`define TARGET_XC7Z015   // Zynq7 on TE0715 board
//`define TARGET_XCZU9EG   // Zynq Ultrascale+ on zcu102 board
//---------------------------------------------------------------

//---------------------------------------------------------------
// supported FPGAs
//---------------------------------------------------------------
`define FPGA_XC7Z015  1    // Zynq7 on TE0715 board
`define FPGA_XCZU9EG  2    // Zynq Ultrascale+ on zcu102 board

`define FPGA_UNKNOWN  0    // unsupported FPGA
//---------------------------------------------------------------

//---------------------------------------------------------------
// FPGA-dependent parameters
//---------------------------------------------------------------
`ifdef TARGET_XC7Z015
  // Zynq7 on TE0715 board
  `define FPGA_MODEL    `FPGA_XC7Z015
  `define MGTCLK_PERIOD 6667
`elsif TARGET_XCZU9EG
  // Zynq Ultrascale+ on zcu102 board
  `define FPGA_MODEL    `FPGA_XCZU9EG
  `define MGTCLK_PERIOD 6734
`else
  // unsupported FPGA
  `define FPGA_MODEL    `FPGA_UNKNOWN
`endif
//---------------------------------------------------------------

`endif
