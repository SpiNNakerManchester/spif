// -------------------------------------------------------------------------
//  pkt_assembler
//
//  assembles SpiNNaker multicast packets from incoming keys
//
// -------------------------------------------------------------------------
// AUTHOR
//  lap - luis.plana@manchester.ac.uk
// -------------------------------------------------------------------------
// DETAILS
//  Created on       : 21 Oct 2020
//  Last modified on : Mon  9 Nov 08:54:58 GMT 2020
//  Last modified by : lap
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


`timescale 1ps/1ps
module pkt_assembler
#(
    parameter PACKET_BITS  = 72
)
(
  input  wire                     clk,
  input  wire                     reset,

  input  wire              [31:0] evt_data_in,
  input  wire                     evt_vld_in,
  output reg                      evt_rdy_out,

  output reg  [PACKET_BITS - 1:0] pkt_data_out,
  output reg                      pkt_vld_out,
  input  wire                     pkt_rdy_in
);

  //---------------------------------------------------------------
  // internal signals
  //---------------------------------------------------------------
  // interface status
  wire  evt_present_int = evt_vld_in && evt_rdy_out;
  wire  pkt_busy_int = pkt_vld_out && !pkt_rdy_in;

  // input event interface
  reg         parked_int;
  reg  [31:0] parked_data_int;

  // park input data when output is busy
  always @ (posedge clk or posedge reset)
    if (reset)
      parked_int <= 1'b0;
    else
      if (evt_present_int && pkt_busy_int)
        parked_int <= 1'b1;
      else if (pkt_rdy_in)
        parked_int <= 1'b0;

  always @ (posedge clk)
    if (evt_present_int && pkt_busy_int)
      parked_data_int <= evt_data_in;

  // don't accept a new key when parked or parking data
  always @ (posedge clk or posedge reset)
    if (reset)
      evt_rdy_out <= 1'b0;
    else
      casex ({parked_int, evt_present_int, pkt_busy_int})
        //NOTE: 3'b111 must not happen - data loss!
        3'bx11,                        // busy and parking
        3'b1x1 : evt_rdy_out <= 1'b0;  // busy and parked 

        3'bxx0,                        // not busy
        3'b001 : evt_rdy_out <= 1'b1;  // busy but park available
      endcase


  // output packet interface
  reg   [31:0] pkt_key_int;
  wire  [31:0] pkt_pld_int = 32'h0000_0000;
  wire         pkt_pty_int = ~(^pkt_key_int ^ ^pkt_pld_int);
  wire   [7:0] pkt_hdr_int = {7'b000_0000, pkt_pty_int};

  // used parked key when available
  always @ (*)
    if (parked_int)
      pkt_key_int = parked_data_int;
    else
      pkt_key_int = evt_data_in;

  // packet data must not change when busy 
  always @ (posedge clk)
    if (!pkt_busy_int && (parked_int || evt_present_int))
      pkt_data_out <= {pkt_pld_int, pkt_key_int, pkt_hdr_int};

  always @ (posedge clk or posedge reset)
    if (reset)
      pkt_vld_out <= 1'b0;
    else
      casex ({parked_int, evt_present_int, pkt_busy_int})
        3'b000 : pkt_vld_out <= 1'b0;  // not busy and no data

        3'b1x0,                        // not busy and parked data
        3'bx10,                        // not busy and new data
        3'bxx1 : pkt_vld_out <= 1'b1;  // busy
      endcase
  //---------------------------------------------------------------
endmodule
