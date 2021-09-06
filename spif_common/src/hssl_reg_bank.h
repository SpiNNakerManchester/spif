// -------------------------------------------------------------------------
//  hssl_reg_bank
//
//  constants
//
// -------------------------------------------------------------------------
// AUTHOR
//  lap - luis.plana@manchester.ac.uk
// -------------------------------------------------------------------------
// DETAILS
//  Created on       : 06 Sep 2021
//  Last modified on : Mon  6 Sep 09:52:12 BST 2021
//  Last modified by : lap
// -------------------------------------------------------------------------
// COPYRIGHT
//  Copyright (c) The University of Manchester, 2021.
//  SpiNNaker Project
//  Advanced Processor Technologies Group
//  School of Computer Science
// -------------------------------------------------------------------------
// TODO
// -------------------------------------------------------------------------

`include "spio_hss_multiplexer_common.h"


`ifndef HSSL_REG_BANK_H
`define HSSL_REG_BANK_H

//---------------------------------------------------------------
// implementation parameters
//---------------------------------------------------------------
`define HW_VER_BITS    24
`define HW_PIPE_BITS   4
`define HW_FPGA_BITS   2

`define HW_SNTL_BITS   16
//---------------------------------------------------------------

//---------------------------------------------------------------
// number of registers by type
//---------------------------------------------------------------
`define NUM_HREGS      5
`define NUM_RREGS      16
`define NUM_CREGS      4
`define NUM_MREGS_PIPE 4
`define NUM_MREGS      (NUM_MREGS_PIPE * HW_NUM_PIPES)
//---------------------------------------------------------------

//---------------------------------------------------------------
// registers are associated by type in sections of 16 registers
//---------------------------------------------------------------
`define SEC_BITS       3
`define REG_BITS       4
//---------------------------------------------------------------

//---------------------------------------------------------------
// register type / section
//---------------------------------------------------------------
`define HREGS_SEC      3'd0  // interface control & status
`define KREGS_SEC      3'd1  // input router keys
`define MREGS_SEC      3'd2  // input router masks
`define RREGS_SEC      3'd3  // input router routes
`define CREGS_SEC      3'd4  // diagnostic counters
`define PREGS_SEC      3'd5  // mapper keys
`define AREGS_SEC      3'd6  // mapper field masks
`define SREGS_SEC      3'd7  // mapper field shifts
//---------------------------------------------------------------

//---------------------------------------------------------------
// register addresses are "word" addresses
//---------------------------------------------------------------
`define SEC_LSB        `REG_BITS;
`define REG_LSB        0;
`define RADDR_BITS     (`SEC_BITS + `REG_BITS)
//---------------------------------------------------------------

//---------------------------------------------------------------
// reading non-existent registers returns this value
//---------------------------------------------------------------
`define BAD_REG        32'hdead_beef;
//---------------------------------------------------------------

//---------------------------------------------------------------
// some registers are not 32 bit in size
//---------------------------------------------------------------
`define RRTE_BITS      3
`define MSFT_BITS      6
//---------------------------------------------------------------

`endif
