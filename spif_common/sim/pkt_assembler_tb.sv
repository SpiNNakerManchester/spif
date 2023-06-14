// -------------------------------------------------------------------------
//  pkt_assembler testbench
//
//  tests packet assembler module
//
// -------------------------------------------------------------------------
// AUTHOR
//  lap - luis.plana@manchester.ac.uk
// -------------------------------------------------------------------------
// DETAILS
//  Created on       : 24 Jul 2021
//  Last modified on : Sat 24 Jul 14:15:37 BST 2021
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

`include "spio_hss_multiplexer_common.h"


`timescale 1ps/1ps
module pkt_assembler_tb ();

  localparam NUM_MREGS    = 4;

  localparam TB_CLK_HALF  = (13.333 / 2);  // currently testing @ 75 MHz
  localparam DUT_CLK_HALF = (13.333 / 2);  // currently testing @ 75 MHz
  localparam INIT_DELAY   = (10 * TB_CLK_HALF);
  localparam RST_DELAY    = (51 * TB_CLK_HALF);  // align with clock posedge

  localparam MP_KEY   = 32'hee00_0000;
  localparam MP_MSK_0 = 32'h00ff_0000;
  localparam MP_SFT_0 = 16;
  localparam MP_MSK_1 = 32'h0000_00ff;
  localparam MP_SFT_1 = -8;


  //---------------------------------------------------------------
  // internal signals
  //---------------------------------------------------------------
  reg                       clk_tb;
  reg                       reset_tb;

  reg                [31:0] pkt_counter_tb;

  reg                       clk_dut;
  reg                       reset_dut;

  reg                [31:0] mp_key_tb;
  reg                [31:0] field_msk_tb [NUM_MREGS - 1:0];
  reg                 [5:0] field_sft_tb [NUM_MREGS - 1:0];
  reg                [31:0] field_lmt_tb [NUM_MREGS - 1:0];

  reg                [31:0] evt_data_tb;
  reg                       evt_vld_tb;
  wire                      evt_rdy_tb;

  wire    [`PKT_BITS - 1:0] pkt_data_tb;
  wire                      pkt_vld_tb;
  reg                       pkt_rdy_tb;


  //---------------------------------------------------------------
  // dut: packet router
  //---------------------------------------------------------------
  pkt_assembler
  dut (
      .clk                (clk_dut)
    , .reset              (reset_dut)

      // mapper data from register bank
    , .mp_key_in          (mp_key_tb)
    , .mp_fld_msk_in      (field_msk_tb)
    , .mp_fld_sft_in      (field_sft_tb)
    , .mp_fld_lmt_in      (field_lmt_tb)

      // incoming events
    , .evt_data_in        (evt_data_tb)
    , .evt_vld_in         (evt_vld_tb)
    , .evt_rdy_out        (evt_rdy_tb)

      // outgoing packets
    , .pkt_data_out       (pkt_data_tb)
    , .pkt_vld_out        (pkt_vld_tb)
    , .pkt_rdy_in         (pkt_rdy_tb)
    );
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // packet counter is useful to control the tests
  //---------------------------------------------------------------
  always @ (posedge clk_tb or posedge reset_tb)
    if (reset_tb)
      pkt_counter_tb <= 0;
    else
    if (evt_vld_tb && evt_rdy_tb)
      pkt_counter_tb <= pkt_counter_tb + 1;
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // drive dut register bank interface
  //---------------------------------------------------------------
  always @ (posedge clk_tb or posedge reset_tb)
    if (reset_tb)
      begin
        mp_key_tb       <= 32'd0;
        field_msk_tb[0] <= 32'd0;
        field_sft_tb[0] <= 32'd0;
        field_lmt_tb[0] <= 32'hffff_ffff;
        field_msk_tb[1] <= 32'd0;
        field_sft_tb[1] <= 32'd0;
        field_lmt_tb[1] <= 32'hffff_ffff;
        field_msk_tb[2] <= 32'd0;
        field_sft_tb[2] <= 32'd0;
        field_lmt_tb[2] <= 32'hffff_ffff;
        field_msk_tb[3] <= 32'd0;
        field_sft_tb[3] <= 32'd0;
        field_lmt_tb[3] <= 32'hffff_ffff;
      end
    else
    begin
      mp_key_tb       <= MP_KEY;
      field_msk_tb[0] <= MP_MSK_0;
      field_sft_tb[0] <= MP_SFT_0;
      field_msk_tb[1] <= MP_MSK_1;
      field_sft_tb[1] <= MP_SFT_1;
    end
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // drive input event interface
  //---------------------------------------------------------------
  always @ (posedge clk_tb or posedge reset_tb)
    if (reset_tb)
      evt_data_tb <= 0;
    else
      if (evt_vld_tb && evt_rdy_tb)
        evt_data_tb <= evt_data_tb + 1;

  always @ (posedge clk_tb or posedge reset_tb)
    if (reset_tb)
      evt_vld_tb <= 1'b0;
    else
      evt_vld_tb <= 1'b1;
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // drive dut output packet interface
  //---------------------------------------------------------------
  always @ (posedge clk_tb or posedge reset_tb)
    if (reset_tb)
      pkt_rdy_tb = 1'b0;
    else
      pkt_rdy_tb = 1'b1;
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
