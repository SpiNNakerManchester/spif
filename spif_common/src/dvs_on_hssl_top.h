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
//  Last modified on : Sun  2 Oct 17:47:23 CEST 2022
//  Last modified by : $Author: plana $
// -------------------------------------------------------------------------
// COPYRIGHT
//  Copyright (c) The University of Manchester, 2021-2022.
//  SpiNNaker Project
//  Advanced Processor Technologies Group
//  School of Computer Science
// -------------------------------------------------------------------------


`ifndef DVS_ON_HSSL_TOP_H
`define DVS_ON_HSSL_TOP_H

//---------------------------------------------------------------
// implementation parameters
//---------------------------------------------------------------
`define HW_VER_BITS    24
`define HW_PIPE_BITS   4
`define HW_OUTP_BITS   4
`define HW_FPGA_BITS   2
//---------------------------------------------------------------

//---------------------------------------------------------------
// hardware version
// semantic versioning [MM = major, mm = minor, pp = patch]
//---------------------------------------------------------------
`define SPIF_VER_STR      "0.1.0"
`define SPIF_VER_NUM      `HW_VER_BITS'h000100  // 24'hMMmmpp
//---------------------------------------------------------------

//---------------------------------------------------------------
// number of parallel event-processing pipelines
//NOTE: define or undef pipes appropriately
//NOTE: PIPE0 should be defined always!
//---------------------------------------------------------------
`define SPIF_NUM_PIPES    `HW_PIPE_BITS'd2
`define PIPE1
`undef  PIPE2
`undef  PIPE3
//---------------------------------------------------------------

//---------------------------------------------------------------
// number of output pipelines
//NOTE: currently only 1 output pipe is supported
//---------------------------------------------------------------
`define SPIF_NUM_OUTPS    `HW_OUTP_BITS'd1
//---------------------------------------------------------------

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
`define FPGA_XC7Z015        `HW_FPGA_BITS'd1    // Zynq7 on TE0715 board
`define FPGA_XCZU9EG        `HW_FPGA_BITS'd2    // Zynq Ultrascale+ on zcu102 board

`define FPGA_UNKNOWN        `HW_FPGA_BITS'd0    // unsupported FPGA
//---------------------------------------------------------------

//---------------------------------------------------------------
// FPGA-dependent parameters
//---------------------------------------------------------------
`ifdef TARGET_XC7Z015
  // Zynq7 on TE0715 board
  `define FPGA_MODEL        `FPGA_XC7Z015
  `define MGTCLK_PERIOD     6667
  `define APB_ADR_BITS      32
`elsif TARGET_XCZU9EG
  // Zynq Ultrascale+ on zcu102 board
  `define FPGA_MODEL        `FPGA_XCZU9EG
  `define MGTCLK_PERIOD     6734
  `define APB_ADR_BITS      40
`else
  // unsupported FPGA
  `define FPGA_MODEL        `FPGA_UNKNOWN
  `define MGTCLK_PERIOD     0
  `define APB_ADR_BITS      0
`endif
//---------------------------------------------------------------

`endif
