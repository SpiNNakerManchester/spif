// -------------------------------------------------------------------------
//  pkt_receiver
//
//  checks incoming SpiNNaker multicast packets for errors and steers
//  them to the peripheral interface or the configuration block
//
// -------------------------------------------------------------------------
// AUTHOR
//  lap - luis.plana@manchester.ac.uk
// -------------------------------------------------------------------------
// DETAILS
//  Created on       : 21 Jun 2021
//  Last modified on : Thu  1 Jul 09:54:58 BST 2021
//  Last modified by : lap
// -------------------------------------------------------------------------
// COPYRIGHT
//  Copyright (c) The University of Manchester, 2021.
//  SpiNNaker Project
//  Advanced Processor Technologies Group
//  School of Computer Science
// -------------------------------------------------------------------------
// TODO
//  * everything
// -------------------------------------------------------------------------


`timescale 1ps/1ps
module pkt_receiver
#(
    parameter PACKET_BITS  = 72
)
(
  input  wire                     clk,
  input  wire                     reset,

  // incoming packets from transceiver
  input  wire [PACKET_BITS - 1:0] pkt_data_in,
  input  wire                     pkt_vld_in,
  output wire                     pkt_rdy_out,

  // register bank interface
  output wire               [7:0] prx_addr_out,
  output wire              [31:0] prx_data_out,
  output wire                     prx_vld_out,

  // packet counters
  output wire               [1:0] prx_cnt_out
);

  //---------------------------------------------------------------
  // internal signals
  //---------------------------------------------------------------
  // receiver always ready!
  assign pkt_rdy_out = 1'b1;

  // basic register interface
  assign prx_addr_out = pkt_data_in[8 +: 8];
  assign prx_data_out = pkt_data_in[40 +: 32];
  assign prx_vld_out  = pkt_vld_in;

  // received packet counter enable signals
  //NOTE: packet type encoded in emergency routing bits!
  assign prx_cnt_out[1] = ( pkt_data_in[4] && pkt_vld_in); 
  assign prx_cnt_out[0] = (!pkt_data_in[4] && pkt_vld_in); 
  //---------------------------------------------------------------
endmodule
