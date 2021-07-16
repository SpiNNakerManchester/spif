// -------------------------------------------------------------------------
//  pkt_receiver testbench
//
//  tests packet receiver module
//
// -------------------------------------------------------------------------
// AUTHOR
//  lap - luis.plana@manchester.ac.uk
// -------------------------------------------------------------------------
// DETAILS
//  Created on       : 13 Jul 2021
//  Last modified on : Tue 13 Jul 10:54:06 BST 2021
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
module pkt_receiver_tb ();

  localparam PACKET_BITS  = 72;
  localparam NUM_CREGS    = 3;
  localparam KEY_LSB      = 8;

  localparam TB_CLK_HALF  = (13.333 / 2);  // currently testing @ 75 MHz
  localparam DUT_CLK_HALF = (13.333 / 2);  // currently testing @ 75 MHz
  localparam INIT_DELAY   = (10 * TB_CLK_HALF);
  localparam RST_DELAY    = (51 * TB_CLK_HALF);  // align with clock posedge

  localparam READ_CNT0    = {32'h0000_0000, 32'hffff_fd40, 8'h31};
  localparam WRITE_CNT1   = {32'h0000_000d, 32'hffff_fd41, 8'h32};
  localparam READ_CNT2    = {32'h0000_0000, 32'hffff_fd42, 8'h30};
  localparam READ_CNT5    = {32'h0000_0000, 32'hffff_fd45, 8'h31};

  //---------------------------------------------------------------
  // internal signals
  //---------------------------------------------------------------
  reg                       clk_tb;
  reg                       reset_tb;

  reg                [31:0] pkt_counter_tb;

  reg                       clk_dut;
  reg                       reset_dut;

  reg   [PACKET_BITS - 1:0] pkt_in_data_tb;
  reg                       pkt_in_vld_tb;
  wire                      pkt_in_rdy_tb;

  reg                [31:0] reg_ctr_tb [NUM_CREGS - 1:0];
  reg                [31:0] reply_key_tb;

  wire                [7:0] prx_addr_tb;
  wire               [31:0] prx_wdata_tb;
  wire                      prx_en_tb;

  wire  [PACKET_BITS - 1:0] per_data_tb;
  wire                      per_vld_tb;
  reg                       per_rdy_tb;

  wire  [PACKET_BITS - 1:0] dcp_data_tb;
  wire                      dcp_vld_tb;
  reg                       dcp_rdy_tb;

  wire                [1:0] prx_cnt_tb;

  //---------------------------------------------------------------
  // dut: packet receiver
  //---------------------------------------------------------------
  pkt_receiver
  #(
      .NUM_CREGS          (NUM_CREGS)
    )
  dut (
      .clk                (clk_dut)
    , .reset              (reset_dut)

      // incoming packets from transceiver
    , .pkt_data_in        (pkt_in_data_tb)
    , .pkt_vld_in         (pkt_in_vld_tb)
    , .pkt_rdy_out        (pkt_in_rdy_tb)

      // packet counters
    , .reg_ctr_in         (reg_ctr_tb)
    , .reply_key_in       (reply_key_tb)

      // register bank interface
    , .prx_addr_out       (prx_addr_tb)
    , .prx_wdata_out      (prx_wdata_tb)
    , .prx_en_out         (prx_en_tb)

      // diagnostic counter packet
    , .dcp_data_out       (dcp_data_tb)
    , .dcp_vld_out        (dcp_vld_tb)
    , .dcp_rdy_in         (dcp_rdy_tb)

    // peripheral output packet
    , .per_data_out       ()
    , .per_vld_out        ()
    , .per_rdy_in         (per_rdy_tb)

      // packet counter enables
    , .prx_cnt_out        (prx_cnt_tb)
    );
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // packet counter is useful to control the tests
  //---------------------------------------------------------------
  always @ (posedge clk_tb or posedge reset_tb)
    if (reset_tb)
      pkt_counter_tb <= 0;
    else
      if (pkt_in_vld_tb && pkt_in_rdy_tb)
        pkt_counter_tb <= pkt_counter_tb + 1;
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // drive dut register bank interface
  //---------------------------------------------------------------
  always @ (posedge clk_tb or posedge reset_tb)
    if (reset_tb)
      reg_ctr_tb[0] <= 32'd0;
    else
      reg_ctr_tb[0] <= 32'd0;

  always @ (posedge clk_tb or posedge reset_tb)
    if (reset_tb)
      reg_ctr_tb[1] <= 32'd0;
    else
      reg_ctr_tb[1] <= 32'd1;

  always @ (posedge clk_tb or posedge reset_tb)
    if (reset_tb)
      reg_ctr_tb[2] <= 32'd0;
    else
      reg_ctr_tb[2] <= 32'd2;

  always @ (posedge clk_tb or posedge reset_tb)
    if (reset_tb)
      reply_key_tb <= 32'hffff_fd00;
    else
      reply_key_tb <= 32'hffff_fd00;
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // drive dut diagnostic counter packet interface
  //---------------------------------------------------------------
  always @ (posedge clk_tb or posedge reset_tb)
    if (reset_tb)
      dcp_rdy_tb <= 1'b0;
    else
      dcp_rdy_tb <= 1'b1;
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // drive dut peripheral output packet interface
  //---------------------------------------------------------------
  always @ (posedge clk_tb or posedge reset_tb)
    if (reset_tb)
      per_rdy_tb <= 1'b0;
    else
      per_rdy_tb <= 1'b1;
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // drive dut input packet interface
  //---------------------------------------------------------------
  always @ (posedge clk_tb or posedge reset_tb)
    if (reset_tb)
      pkt_in_data_tb <= 0;
    else
      if (pkt_in_vld_tb && pkt_in_rdy_tb)
        if (pkt_counter_tb == 6)
          pkt_in_data_tb <= READ_CNT0;
        else if (pkt_counter_tb == 12)
          pkt_in_data_tb <= WRITE_CNT1;
        else if (pkt_counter_tb == 18)
          pkt_in_data_tb <= READ_CNT2;
        else if (pkt_counter_tb == 24)
          pkt_in_data_tb <= READ_CNT5;
        else
          pkt_in_data_tb <= pkt_counter_tb << KEY_LSB;

  always @ (posedge clk_tb or posedge reset_tb)
    if (reset_tb)
      pkt_in_vld_tb <= 1'b0;
    else
      pkt_in_vld_tb <= 1'b1;
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // dut clock signal
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
    # INIT_DELAY reset_tb = 1'b1;

    # RST_DELAY  reset_tb = 1'b0;  // release testbench
  end
  //---------------------------------------------------------------
endmodule
