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
  reg                       tb_clk;
  reg                       tb_reset;

  reg                       dut_clk;
  reg                       dut_reset;

  reg                [63:0] tb_cycle_counter;

  reg                [31:0] tb_pkt_counter;
  reg                       tb_pkt_send;

  reg   [PACKET_BITS - 1:0] tb_pkt_in_data;
  reg                       tb_pkt_in_vld;
  wire                      tb_pkt_in_rdy;

  reg                [31:0] tb_output_tick;
  reg                 [9:0] tb_output_size;

  wire               [31:0] tb_evt_data;
  wire                [3:0] tb_evt_keep;
  wire                      tb_evt_last;
  wire                      tb_evt_vld;
  reg                       tb_evt_rdy;

  wire                      tb_out_drp_pkt_cnt;

  //---------------------------------------------------------------
  // dut: event distpatcher
  //---------------------------------------------------------------
  evt_dispatcher dut (
      .clk                (dut_clk)
    , .reset              (dut_reset)

      // incoming peripheral output packets from receiver
    , .pkt_data_in        (tb_pkt_in_data)
    , .pkt_vld_in         (tb_pkt_in_vld)
    , .pkt_rdy_out        (tb_pkt_in_rdy)

      // event frame parameters
    , .output_tick_in     (tb_output_tick)
    , .output_size_in     (tb_output_size)

      // outgoing peripheral output events
    , .evt_data_out       (tb_evt_data)
    , .evt_keep_out       (tb_evt_keep)
    , .evt_last_out       (tb_evt_last)
    , .evt_vld_out        (tb_evt_vld)
    , .evt_rdy_in         (tb_evt_rdy)

    // dropped packet counter enable
    , .out_drp_cnt_out    (tb_out_drp_pkt_cnt)
    );
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // cycle counter is useful to control the tests
  //---------------------------------------------------------------
  always @ (posedge tb_clk or posedge tb_reset)
    if (tb_reset)
      tb_cycle_counter <= 0;
    else
      tb_cycle_counter <= tb_cycle_counter + 1;
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // packet counter is useful to control the packet data
  //---------------------------------------------------------------
  always @ (posedge tb_clk or posedge tb_reset)
    if (tb_reset)
      tb_pkt_counter <= 0;
    else
      if (tb_pkt_in_vld && tb_pkt_in_rdy)
        tb_pkt_counter <= tb_pkt_counter + 1;
    
  always_comb
    tb_pkt_send = !tb_reset && ((tb_cycle_counter % 3) == 2);
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // drive dut peripheral output packet interface
  //---------------------------------------------------------------
  always_comb
    tb_pkt_in_data = tb_pkt_counter << KEY_LSB;

  always @ (posedge tb_clk or posedge tb_reset)
    if (tb_reset)
      tb_pkt_in_vld <= 1'b0;
    else
      if (tb_pkt_send)
        tb_pkt_in_vld <= 1'b1;
      else
        if (tb_pkt_in_rdy)
          tb_pkt_in_vld <= 1'b0;
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // drive dut peripheral output event interface
  //---------------------------------------------------------------
  always @ (posedge tb_clk or posedge tb_reset)
    if (tb_reset)
      tb_output_tick <= 32'dx;
    else
      //lap tb_output_tick <= 32'd2000;
      tb_output_tick <= 32'd15;

  always @ (posedge tb_clk or posedge tb_reset)
    if (tb_reset)
      tb_output_size <= 10'dx;
    else
      tb_output_size <= 10'd256;

  always @ (posedge tb_clk or posedge tb_reset)
    if (tb_reset)
      tb_evt_rdy <= 1'bx;
    else
      if ((tb_cycle_counter >= 150) && (tb_cycle_counter < 200))
        tb_evt_rdy <= 1'b0;
      else
        tb_evt_rdy <= 1'b1;
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // dut clock signals
  //---------------------------------------------------------------
  initial
  begin
    dut_clk = 1'b0;

    forever
      # DUT_CLK_HALF dut_clk = ~dut_clk;
  end
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // dut reset signal
  //---------------------------------------------------------------
  initial
  begin
    dut_reset = 1'b0;

    // wait a few clock cycles before triggering the reset signal
    # INIT_DELAY dut_reset = 1'b1;

    # RST_DELAY  dut_reset = 1'b0;  // release dut
  end
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // testbench clock signal
  //---------------------------------------------------------------
  initial
  begin
    tb_clk = 1'b0;

    forever
      # TB_CLK_HALF tb_clk = ~tb_clk;
  end
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // testbench reset signal
  //---------------------------------------------------------------
  initial
  begin
    tb_reset = 1'b0;

    // wait a few clock cycles before triggering the reset signal
    # (INIT_DELAY / 2) tb_reset = 1'b1;

    # RST_DELAY        tb_reset = 1'b0;  // release testbench
  end
  //---------------------------------------------------------------
endmodule
