// -------------------------------------------------------------------------
//  evt_dispatcher testbench
//
//  tests event dispatcher module
//
// -------------------------------------------------------------------------
// AUTHOR
//  lap - luis.plana@manchester.ac.uk
// -------------------------------------------------------------------------
// DETAILS
//  Created on       : 25 Apr 2022
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


`timescale 1ns/1ps
module evt_dispatcher_tb ();

  localparam PACKET_BITS  = 72;
  localparam KEY_LSB      = 8;

  localparam EVT_CNT_BITS = 10;
  localparam TCK_CNT_BITS = 32;

  localparam TB_CLK_HALF  = (13.333 / 2);  // currently testing @ 75 MHz
  localparam DUT_CLK_HALF = (13.333 / 2);  // currently testing @ 75 MHz
  localparam INIT_DELAY   = (10 * TB_CLK_HALF);
  localparam RST_DELAY    = (51 * TB_CLK_HALF);  // align with clock posedge


  //---------------------------------------------------------------
  // internal signals
  //---------------------------------------------------------------
  reg                       clk_tb;
  reg                       reset_tb;

  reg                       clk_dut;
  reg                       reset_dut;

  reg                [63:0] cycle_counter_tb;

  reg                [31:0] pkt_counter_tb;

  reg   [PACKET_BITS - 1:0] pkt_in_data_tb;
  reg                       pkt_in_vld_tb;
  wire                      pkt_in_rdy_tb;

  reg                [31:0] output_tick_tb;
  reg                 [9:0] output_size_tb;

  wire               [31:0] evt_data_tb;
  wire                [3:0] evt_keep_tb;
  wire                      evt_last_tb;
  wire                      evt_vld_tb;
  reg                       evt_rdy_tb;

  wire                      out_drp_pkt_cnt_tb;

  //---------------------------------------------------------------
  // dut: packet receiver
  //---------------------------------------------------------------
  evt_dispatcher dut (
      .clk                (clk_dut)
    , .reset              (reset_dut)

      // incoming peripheral output packets from receiver
    , .pkt_data_in        (pkt_in_data_tb)
    , .pkt_vld_in         (pkt_in_vld_tb)
    , .pkt_rdy_out        (pkt_in_rdy_tb)

      // event frame parameters
    , .output_tick_in     (output_tick_tb)
    , .output_size_in     (output_size_tb)

      // outgoing peripheral output events
    , .evt_data_out       (evt_data_tb)
    , .evt_keep_out       (evt_keep_tb)
    , .evt_last_out       (evt_last_tb)
    , .evt_vld_out        (evt_vld_tb)
    , .evt_rdy_in         (evt_rdy_tb)

    // dropped packet counter enable
    , .out_drp_cnt_out    (out_drp_pkt_cnt_tb)
    );
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // cycle counter is useful to control the tests
  //---------------------------------------------------------------
  always @ (posedge clk_tb or posedge reset_tb)
    if (reset_tb)
      cycle_counter_tb <= 0;
    else
      cycle_counter_tb <= cycle_counter_tb + 1;
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // packet counter is useful to control the packet data
  //---------------------------------------------------------------
  always @ (posedge clk_tb or posedge reset_tb)
    if (reset_tb)
      pkt_counter_tb <= 0;
    else
      if (pkt_in_vld_tb && pkt_in_rdy_tb)
        pkt_counter_tb <= pkt_counter_tb + 1;
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // drive dut peripheral output packet interface
  //---------------------------------------------------------------
  always @ (posedge clk_tb or posedge reset_tb)
    if (reset_tb)
      pkt_in_data_tb <= 0;
    else
      if (pkt_in_vld_tb && pkt_in_rdy_tb)
        pkt_in_data_tb <= pkt_counter_tb << KEY_LSB;

  always @ (posedge clk_tb or posedge reset_tb)
    if (reset_tb)
      pkt_in_vld_tb <= 1'bx;
    else
      pkt_in_vld_tb <= 1'b1;
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // drive dut peripheral output event interface
  //---------------------------------------------------------------
  always @ (posedge clk_tb or posedge reset_tb)
    if (reset_tb)
      output_tick_tb <= 32'dx;
    else
      //lap output_tick_tb <= 32'd2000;
      output_tick_tb <= 32'd10;

  always @ (posedge clk_tb or posedge reset_tb)
    if (reset_tb)
      output_size_tb <= 10'dx;
    else
      output_size_tb <= 10'd256;

  always @ (posedge clk_tb or posedge reset_tb)
    if (reset_tb)
      evt_rdy_tb <= 1'bx;
    else
      if ((cycle_counter_tb >= 150) && (cycle_counter_tb < 200))
        evt_rdy_tb <= 1'b0;
      else
        evt_rdy_tb <= 1'b1;
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // dut clock signals
  //---------------------------------------------------------------
  initial
  begin
    clk_dut = 1'b0;

    forever
      # DUT_CLK_HALF clk_dut = ~clk_dut;
  end
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // dut reset signal
  //---------------------------------------------------------------
  initial
  begin
    reset_dut = 1'b0;

    // wait a few clock cycles before triggering the reset signal
    # INIT_DELAY reset_dut = 1'b1;

    # RST_DELAY  reset_dut = 1'b0;  // release dut
  end
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // testbench clock signal
  //---------------------------------------------------------------
  initial
  begin
    clk_tb = 1'b0;

    forever
      # TB_CLK_HALF clk_tb = ~clk_tb;
  end
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // testbench reset signal
  //---------------------------------------------------------------
  initial
  begin
    reset_tb = 1'b0;

    // wait a few clock cycles before triggering the reset signal
    # (INIT_DELAY / 2) reset_tb = 1'b1;

    # RST_DELAY        reset_tb = 1'b0;  // release testbench
  end
  //---------------------------------------------------------------
endmodule
