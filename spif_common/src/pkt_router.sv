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
//  Last modified on : Thu 15 Jul 18:56:59 BST 2021
//  Last modified by : lap
// -------------------------------------------------------------------------
// COPYRIGHT
//  Copyright (c) The University of Manchester, 2020-2021.
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
  parameter NUM_RREGS    = 16,
  parameter KEY_LSB      = 8,
  parameter NUM_CHANNELS = 8
)
(
  input  wire                     clk,
  input  wire                     reset,

  // routing table components
  input  wire              [31:0] reg_key_in   [NUM_RREGS - 1:0],
  input  wire              [31:0] reg_mask_in  [NUM_RREGS - 1:0],
  input  wire               [2:0] reg_route_in [NUM_RREGS - 1:0],

  // incoming packet
  input  wire [PACKET_BITS - 1:0] pkt_in_data_in,
  input  wire                     pkt_in_vld_in,
  output reg                      pkt_in_rdy_out,

  // outgoing packet channels
  output reg  [PACKET_BITS - 1:0] pkt_out_data_out [NUM_CHANNELS - 1:0],
  output reg                      pkt_out_vld_out  [NUM_CHANNELS - 1:0],
  input  wire                     pkt_out_rdy_in   [NUM_CHANNELS - 1:0],

  // packet counter
  output wire               [1:0] rt_cnt_out
);

  //---------------------------------------------------------------
  // internal signals
  //---------------------------------------------------------------
  wire               [31:0] packet_key;
  wire    [NUM_RREGS - 1:0] hit;
  wire                      routing;
  reg                 [2:0] route;
  wire                      dropped;

  // route input packet
  assign packet_key = pkt_in_data_in[KEY_LSB +: 32];

  // ternary CAM-like routing table
  genvar te;
  generate
    for (te = 0; te < NUM_RREGS; te = te + 1)
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

  //---------------------------------------------------------------
  // output stages
  //---------------------------------------------------------------
  // switch output ports
  //NOTE: must be split into output channels
  wire [(PACKET_BITS * NUM_CHANNELS) - 1:0] swi_data;
  wire                 [NUM_CHANNELS - 1:0] swi_vld;
  wire                 [NUM_CHANNELS - 1:0] swi_rdy;

  // switch expects route in different format
  wire [NUM_CHANNELS - 1:0] swi_route = hit ? (1 << route) : 0;

  spio_switch
  #(
        .PKT_BITS             (PACKET_BITS)
      , .NUM_PORTS            (NUM_CHANNELS)
      )
  swi (
        .CLK_IN               (clk)
      , .RESET_IN             (reset)

      , .IN_OUTPUT_SELECT_IN  (swi_route)

      , .IN_DATA_IN           (pkt_in_data_in)
      , .IN_VLD_IN            (pkt_in_vld_in)
      , .IN_RDY_OUT           (pkt_in_rdy_out)

      , .OUT_DATA_OUT         (swi_data)
      , .OUT_VLD_OUT          (swi_vld)
      , .OUT_RDY_IN           (swi_rdy)

      , .BLOCKED_OUTPUTS_OUT  ()
      , .SELECTED_OUTPUTS_OUT ()

        //NOTE: packet dropping not yet implemented!
      , .DROP_IN              (1'b0)

      , .DROPPED_DATA_OUT     ()
      , .DROPPED_OUTPUTS_OUT  ()
      , .DROPPED_VLD_OUT      (dropped)
      );

  // packets go out directly from switch outputs
  genvar chan;
  generate
    for (chan = 0; chan < NUM_CHANNELS; chan = chan + 1)
      begin : output_channels
        assign pkt_out_data_out[chan] = swi_data[chan * PACKET_BITS +: PACKET_BITS];
        assign pkt_out_vld_out[chan]  = swi_vld[chan];
        assign swi_rdy[chan]          = pkt_out_rdy_in[chan];
      end
  endgenerate

  // packet counter enable signals
  //NOTE: packets with a routing table miss are counted as dropped!
  assign rt_cnt_out[0] = dropped;
  assign rt_cnt_out[1] = pkt_in_vld_in && pkt_in_rdy_out && !dropped;
  
  //---------------------------------------------------------------
endmodule
