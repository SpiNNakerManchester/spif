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
  localparam ETH_LED_PERIOD = 500000;
  //---------------------------------------------------------------


  //---------------------------------------------------------------
  // testbench signals
  //---------------------------------------------------------------
  // transceiver differential clock (use true and complement)
  reg refclk0_x1y3;

  // HSSL loop back
  wire ch0_gthxn;
  wire ch0_gthxp;

`ifdef TARGET_XC7Z015
  // Ehternet LEDs
  reg  eth_phy_led0;
  reg  eth_phy_led1;
  wire eth_led0;
  wire eth_led1;
`endif
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


`ifdef TARGET_XC7Z015
  //---------------------------------------------------------------
  //simulate Ethernet PHY LED signals
  //---------------------------------------------------------------
  initial begin
    eth_phy_led0 = 1'b0;
    eth_phy_led1 = 1'b1;
    forever
      begin
        eth_phy_led0 = #(ETH_LED_PERIOD / 2) ~eth_phy_led0;
        eth_phy_led1 = #(ETH_LED_PERIOD / 2) ~eth_phy_led1;
      end
  end
  //---------------------------------------------------------------
`endif


  //---------------------------------------------------------------
  // processor sub-system + HSSL interface + transceiver block
  //---------------------------------------------------------------
  dvs_on_hssl_top dh (
    .gt_refclk_p  (refclk0_x1y3),
    .gt_refclk_n  (~refclk0_x1y3),

`ifdef TARGET_XC7Z015
    .eth_phy_led0 (eth_phy_led0),
    .eth_phy_led1 (eth_phy_led1),
    .eth_led0     (eth_led0),
    .eth_led1     (eth_led1),
`endif

    .gt_rxn_in    (ch0_gthxn),
    .gt_rxp_in    (ch0_gthxp),
    .gt_txn_out   (ch0_gthxn),
    .gt_txp_out   (ch0_gthxp)
    );
  //---------------------------------------------------------------

endmodule
