// -------------------------------------------------------------------------
//  dvs_on_hssl_top_tb
//
//  DVS input to SpiNN-5 board through High-Speed Serial Link (HSSL)
//  testbench for simulation
//
// -------------------------------------------------------------------------
// AUTHOR
//  lap - luis.plana@manchester.ac.uk
// -------------------------------------------------------------------------
// DETAILS
//  Created on       : 21 Oct 2020
//  Last modified on : Wed 21 Oct 16:13:42 BST 2020
//  Last modified by : $Author: plana $
// -------------------------------------------------------------------------
// COPYRIGHT
//  Copyright (c) The University of Manchester, 2020.
//  SpiNNaker Project
//  Advanced Processor Technologies Group
//  School of Computer Science
// -------------------------------------------------------------------------
// TODO
//  * everything
// -------------------------------------------------------------------------

`include "dvs_on_hssl_top.h"


`timescale 1ps/1ps
module dvs_on_hssl_top_tb ();

  //---------------------------------------------------------------
  // testbench constants
  //---------------------------------------------------------------
  // Gigabit transceiver reference clock period
  localparam GT_CLK_PERIOD = `MGTCLK_PERIOD;  // picoseconds
  //---------------------------------------------------------------


  //---------------------------------------------------------------
  // testbench signals
  //---------------------------------------------------------------
  // transceiver differential clock (use true and complement)
  reg refclk0_x1y3;

  // HSSL loop back
  wire ch0_gthxn;
  wire ch0_gthxp;
  //---------------------------------------------------------------


  //---------------------------------------------------------------
  //provide differential clocks to transceiver block
  //NOTE: simulation only!
  //---------------------------------------------------------------
  initial begin
    refclk0_x1y3 = 1'b0;
    forever
      refclk0_x1y3 = #(GT_CLK_PERIOD / 2) ~refclk0_x1y3;
  end
  //---------------------------------------------------------------


  //---------------------------------------------------------------
  // processor sub-system + HSSL interface + transceiver block
  //---------------------------------------------------------------
  dvs_on_hssl_top dh (
    .gt_refclk_p  (refclk0_x1y3),
    .gt_refclk_n  (~refclk0_x1y3),

    .gt_rxn_in    (ch0_gthxn),
    .gt_rxp_in    (ch0_gthxp),
    .gt_txn_out   (ch0_gthxn),
    .gt_txp_out   (ch0_gthxp)
    );
  //---------------------------------------------------------------

endmodule
