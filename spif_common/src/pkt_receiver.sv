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
    parameter PACKET_BITS  = 72,
    parameter NUM_CREGS    = 3
)
(
  input  wire                     clk,
  input  wire                     reset,

  // incoming packets from transceiver
  input  wire [PACKET_BITS - 1:0] pkt_data_in,
  input  wire                     pkt_vld_in,
  output wire                     pkt_rdy_out,

  // packet counters
  input  wire              [31:0] reg_ctr_in [NUM_CREGS - 1:0],
  input  wire              [31:0] reply_key_in,

  // register bank interface
  output wire               [7:0] prx_addr_out,
  output wire              [31:0] prx_wdata_out,
  output wire                     prx_en_out,

  // diagnostic counter packet
  output reg  [PACKET_BITS - 1:0] dcp_data_out,
  output reg                      dcp_vld_out,
  input  wire                     dcp_rdy_in,

  // packet counter enables
  output wire               [1:0] prx_cnt_out
);

  //---------------------------------------------------------------
  // internal signals
  //---------------------------------------------------------------
  //NOTE: emergency routing bits replaced with packet type code!
  wire cfg_pkt = pkt_data_in[4];
  wire pld_pst = pkt_data_in[1];       // payload presence signals r/w
  wire ctr_num = pkt_data_in[8 +: 4];

  // receiver always ready!
  assign pkt_rdy_out = 1'b1;

  // basic register write interface
  //NOTE: enable only when payload present in input packet!
  assign prx_addr_out  = pkt_data_in[8 +: 8];
  assign prx_wdata_out = pkt_data_in[40 +: 32];
  assign prx_en_out    = pld_pst && pkt_vld_in && pkt_rdy_out;

  // basic counter read interface
  //NOTE: enable only when payload not present in input packet!
  //TODO: implement!

  // received packet counter enable signals
  assign prx_cnt_out[1] = ( cfg_pkt && pkt_vld_in && pkt_rdy_out);
  assign prx_cnt_out[0] = (!cfg_pkt && pkt_vld_in && pkt_rdy_out);
  //---------------------------------------------------------------
endmodule
