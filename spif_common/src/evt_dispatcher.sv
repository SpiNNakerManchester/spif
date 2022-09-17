// -------------------------------------------------------------------------
//  evt_dispatcher
//
//  extracts events from incoming packets and forwards them to 
//  the DMA engine for peripheral output management
//
// -------------------------------------------------------------------------
// AUTHOR
//  lap - luis.plana@manchester.ac.uk
// -------------------------------------------------------------------------
// DETAILS
//  Created on       : 21 Apr 2022
//  Last modified on : Mon 25 Apr 12:18:04 BST 2022
//  Last modified by : lap
// -------------------------------------------------------------------------
// COPYRIGHT
//  Copyright (c) The University of Manchester, 2022.
//  SpiNNaker Project
//  Advanced Processor Technologies Group
//  School of Computer Science
// -------------------------------------------------------------------------
// TODO
//  * everything
// -------------------------------------------------------------------------

`include "spio_hss_multiplexer_common.h"
`include "hssl_reg_bank.h"


`timescale 1ps/1ps
module evt_dispatcher
#(
  parameter EVT_CNT_BITS = 10,
  parameter TCK_CNT_BITS = 32
)
(
  input  wire                       clk,
  input  wire                       reset,

  // incoming packets from packet receiver
  input  wire     [`PKT_BITS - 1:0] pkt_data_in,
  input  wire                       pkt_vld_in,
  output reg                        pkt_rdy_out,

  // event frame parameters
  input  wire  [TCK_CNT_BITS - 1:0] output_tick_in,
  input  wire  [EVT_CNT_BITS - 1:0] output_size_in,

  // peripheral output event
  output reg                 [31:0] evt_data_out,
  output reg                  [3:0] evt_keep_out,
  output reg                        evt_last_out,
  output reg                        evt_vld_out,
  input  wire                       evt_rdy_in,

  // dropped packet counter enable
  output reg                        out_drp_cnt_out
);

  // use local parameters for consistent definitions
  localparam PACKET_BITS  = `PKT_BITS;
  localparam PKT_KEY_SZ   = 32;
  localparam PKT_PLD_SZ   = 32;
  localparam PKT_LNG_BIT  = 1;
  localparam PKT_CFG_BIT  = 4;
  localparam PKT_KEY_BIT  = 8;
  localparam PKT_PLD_BIT  = PKT_KEY_BIT + PKT_KEY_SZ;

  // counters
  localparam US_CLK_CNT  = (75 - 1);
  localparam US_CNT_BITS = 32;



  //---------------------------------------------------------------
  // internal signals
  //---------------------------------------------------------------
  // interface status bits
  reg        pkt_available;
  reg [31:0] pkt_event;

  reg evt_busy;

  // ready-to-go event used to dispatch frame
  reg [31:0] rtg_data;
  reg        rtg_rdy;

  // control and status
  reg snd_event;
  reg snd_frame;

  reg  [EVT_CNT_BITS - 1:0] frm_cnt;  // keep track of free frame slots
  reg  [TCK_CNT_BITS - 1:0] tck_cnt;  // count tick clock cycles

  
  //---------------------------------------------------------------
  // drive the incoming peripheral output packet interface
  //NOTE: in the current implementation this module never exerts
  //      back pressure - new packets overwrite old ones
  //---------------------------------------------------------------
  always @ (posedge clk or posedge reset)
    if (reset)
      pkt_rdy_out <= 1'b0;
    else
      pkt_rdy_out <= 1'b1;

  always_comb
    pkt_available = pkt_vld_in & pkt_rdy_out;
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // distill an event from the incoming packet
  //NOTE: in the current implementation the packet key is the event
  //---------------------------------------------------------------
  always_comb
    pkt_event = pkt_data_in[PKT_KEY_BIT +: PKT_KEY_SZ];
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // ready-to-go event: always hold the last event received
  // to be used to dispatch a frame
  // NOTE: skip rtg if arriving event is used to dispatch frame
  //---------------------------------------------------------------
  always @ (posedge clk)
    casex ({pkt_available, rtg_rdy, snd_frame})
      3'b1x0,
      3'b11x: rtg_data <= pkt_event; 
    endcase

  always @ (posedge clk or posedge reset)
    if (reset)
      rtg_rdy <= 1'b0;
    else
      casex ({pkt_available, rtg_rdy, snd_frame})
        3'b1x0,
        3'b11x: rtg_rdy <= 1'b1; 
        3'b0x1: rtg_rdy <= 1'b0; 
      endcase
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // event/frame transmission control
  //---------------------------------------------------------------
  // "normal" send rtg to event output - not a dispatch frame!
  always_comb
    snd_event = pkt_available && rtg_rdy && !evt_busy;

  // dispatch frame on tick or when frame is full
  // (i.e., credit left for rtg only)
  always_comb
    snd_frame = ((tck_cnt == 0) && (pkt_available || rtg_rdy))
                || ((frm_cnt == 1) && !evt_busy);

  // keep track of credit per frame (i.e., free frame slots)
  always @ (posedge clk or posedge reset)
    if (reset)
      frm_cnt <= output_size_in;
    else
      if (snd_frame)
        // new frame - get full credit
        frm_cnt <= output_size_in;
      else if (snd_event)
        // event sent - reduce credit
        frm_cnt <= frm_cnt - 1;

  // generate microsecond tick
  //NOTE: this is a periodic tick -- do not stall!
  reg [US_CNT_BITS - 1:0] us_cnt;
  always @ (posedge clk or posedge reset)
    if (reset)
      us_cnt <= US_CLK_CNT;
    else
      if (us_cnt == 0)
        us_cnt <= US_CLK_CNT;
      else
        us_cnt <= us_cnt - 1;

  // generate frame tick (output_tick_in in microseconds)
  //NOTE: this is a periodic tick -- do not stall!
  always @ (posedge clk or posedge reset)
    if (reset)
      tck_cnt <= output_tick_in;
    else
      if (tck_cnt == 0)
        tck_cnt <= output_tick_in;
      else if (us_cnt == 0)
        tck_cnt <= tck_cnt - 1;
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // drive the peripheral event interface
  //---------------------------------------------------------------
  always @ (posedge clk)
    casex ({rtg_rdy, snd_frame, snd_event})
      3'bxx1,
      3'b11x: evt_data_out <= rtg_data;
      3'b01x: evt_data_out <= pkt_event;
    endcase

  always @ (posedge clk)
    evt_keep_out <= 4'b1111;

  always @ (posedge clk)
    if (snd_frame)
      evt_last_out <= 1'b1;
    else if (!evt_busy)
      evt_last_out <= 1'b0;

  always @ (posedge clk or posedge reset)
    if (reset)
      evt_vld_out <= 1'b0;
    else
      if (snd_event || snd_frame)
        evt_vld_out <= 1'b1;
      else if (!evt_busy)
        evt_vld_out <= 1'b0;

  always_comb
    evt_busy = evt_vld_out && !evt_rdy_in;
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // drive the dropped packet counter enable
  //---------------------------------------------------------------
    always @ (posedge clk or posedge reset)
    if (reset)
      out_drp_cnt_out <= 1'b0;
    else
      out_drp_cnt_out <= pkt_available && evt_busy && rtg_rdy;
  //---------------------------------------------------------------
endmodule
