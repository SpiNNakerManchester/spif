// -------------------------------------------------------------------------
//  pkt_router
//
//  packet router inspired by the SpiNNaker router
//  directs incoming packets to the 8 channels transported on the HSSL
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
module pkt_router
#(
  parameter PACKET_BITS  = 72,
  parameter KEY_LSB      = 8,
  parameter NUM_CHANNELS = 8,
  parameter NUM_ENTRIES  = 16
)
(
  // routing table components
  input  wire              [31:0] reg_key_in   [NUM_ENTRIES - 1:0],
  input  wire              [31:0] reg_mask_in  [NUM_ENTRIES - 1:0],
  input  wire               [2:0] reg_route_in [NUM_ENTRIES - 1:0],

  // incoming packet
  input  wire [PACKET_BITS - 1:0] pkt_in_data_in,
  input  wire                     pkt_in_vld_in,
  output wire                     pkt_in_rdy_out,

  // outgoing packet channels
  output wire [PACKET_BITS - 1:0] pkt_out_data_out [NUM_CHANNELS - 1:0],
  output wire                     pkt_out_vld_out  [NUM_CHANNELS - 1:0],
  input  wire                     pkt_out_rdy_in   [NUM_CHANNELS - 1:0]
);

  //---------------------------------------------------------------
  // internal signals
  //---------------------------------------------------------------
  wire [15:0] hit;
  reg   [2:0] route;

  wire [31:0] packet_key = pkt_in_data_in[KEY_LSB +: 32];

  // ternary CAM-like routing table
  genvar te;
  generate
    for (te = 0; te < NUM_ENTRIES; te = te + 1)
      begin : routing_table
        //NOTE: bit-wise and - it is a mask!
        assign hit[te] = ((packet_key & reg_mask_in[te]) == reg_key_in[te]);
      end
  endgenerate

  // priority-encode table hits
  always @(*)
    casex (hit)
      16'bxxxx_xxxx_xxxx_xxx1: route = reg_route_in[ 0];
      16'bxxxx_xxxx_xxxx_xx10: route = reg_route_in[ 1];
      16'bxxxx_xxxx_xxxx_x100: route = reg_route_in[ 2];
      16'bxxxx_xxxx_xxxx_1000: route = reg_route_in[ 3];
      16'bxxxx_xxxx_xxx1_0000: route = reg_route_in[ 4];
      16'bxxxx_xxxx_xx10_0000: route = reg_route_in[ 5];
      16'bxxxx_xxxx_x100_0000: route = reg_route_in[ 6];
      16'bxxxx_xxxx_1000_0000: route = reg_route_in[ 7];
      16'bxxxx_xxx1_0000_0000: route = reg_route_in[ 8];
      16'bxxxx_xx10_0000_0000: route = reg_route_in[ 9];
      16'bxxxx_x100_0000_0000: route = reg_route_in[10];
      16'bxxxx_1000_0000_0000: route = reg_route_in[11];
      16'bxxx1_0000_0000_0000: route = reg_route_in[12];
      16'bxx10_0000_0000_0000: route = reg_route_in[13];
      16'bx100_0000_0000_0000: route = reg_route_in[14];
      16'b1000_0000_0000_0000: route = reg_route_in[15];
      default:                 route = 0;
    endcase

  genvar chan;
  generate
    for (chan = 0; chan < NUM_CHANNELS; chan = chan + 1)
      begin : route_data_vld
        // incoming packet to all outputs
        assign pkt_out_data_out[chan] = pkt_in_data_in;

        // route pkt_in valid signal to selected channel only
        assign pkt_out_vld_out[chan] = pkt_in_vld_in && (route == chan);
      end
  endgenerate


  // route selected channel ready signal to pkt_in rdy
  //NOTE: signal ready if no table hit!
  assign pkt_in_rdy_out = pkt_out_rdy_in[route] || !hit;
  //---------------------------------------------------------------
endmodule
