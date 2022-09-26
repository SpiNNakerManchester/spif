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
//  Last modified on : Tue  7 Sep 17:35:31 BST 2021
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

`ifndef HSSL_REG_BANK_H
`define HSSL_REG_BANK_H

`include "spio_hss_multiplexer_common.h"
`include "dvs_on_hssl_top.h"

//---------------------------------------------------------------
// implementation parameters
//---------------------------------------------------------------
`define HW_SNTL_BITS     16
//---------------------------------------------------------------

//---------------------------------------------------------------
// registers are associated by type in sections of 16 registers
//---------------------------------------------------------------
`define SEC_BITS         4
`define REG_BITS         4
//---------------------------------------------------------------

//---------------------------------------------------------------
// register sections
//---------------------------------------------------------------
`define IFCAS_SEC        `SEC_BITS'd0   // interface control & status
`define RTKEY_SEC        `SEC_BITS'd1   // input router keys
`define RTMSK_SEC        `SEC_BITS'd2   // input router masks
`define RTRTE_SEC        `SEC_BITS'd3   // input router routes
`define DCCNT_SEC        `SEC_BITS'd4   // diagnostic counters
`define MPKEY_SEC        `SEC_BITS'd5   // mapper keys
`define MPMSK_SEC        `SEC_BITS'd6   // mapper field masks
`define MPSFT_SEC        `SEC_BITS'd7   // mapper field shifts
`define MPLMT_SEC        `SEC_BITS'd8   // mapper field limits
`define FLVAL_SEC        `SEC_BITS'd9   // filter values
`define FLMSK_SEC        `SEC_BITS'd11  // filter masks
//---------------------------------------------------------------

//---------------------------------------------------------------
// number of registers by type
//---------------------------------------------------------------
`define NUM_IFREGS       7
`define NUM_RTREGS       16
`define NUM_DCREGS       5
`define NUM_MPREGS_PIPE  4
`define NUM_MPREGS       (`NUM_MPREGS_PIPE * HW_NUM_PIPES)
`define NUM_FLREGS_PIPE  8
`define NUM_FLREGS       (`NUM_FLREGS_PIPE * HW_NUM_PIPES)
//---------------------------------------------------------------

//---------------------------------------------------------------
// register addresses are "word" addresses
//---------------------------------------------------------------
`define SEC_LSB          `REG_BITS;
`define REG_LSB          0;
`define REG_ADR_BITS     (`SEC_BITS + `REG_BITS)
//---------------------------------------------------------------

//---------------------------------------------------------------
// reading non-existent registers returns this value
//---------------------------------------------------------------
`define BAD_REG          32'hdead_beef;
//---------------------------------------------------------------

//---------------------------------------------------------------
// some registers are not 32 bit in size
//---------------------------------------------------------------
`define RTRTE_BITS       3
`define MPSFT_BITS       6
//---------------------------------------------------------------

`endif
