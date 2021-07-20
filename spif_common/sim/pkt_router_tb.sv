// -------------------------------------------------------------------------
//  pkt_router testbench
//
//  tests packet router module
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
module pkt_router_tb ();

  localparam PACKET_BITS  = 72;
  localparam NUM_RREGS    = 16;
  localparam KEY_LSB      = 8;
  localparam NUM_CHANNELS = 8;

  localparam TB_CLK_HALF  = (13.333 / 2);  // currently testing @ 75 MHz
  localparam DUT_CLK_HALF = (13.333 / 2);  // currently testing @ 75 MHz
  localparam INIT_DELAY   = (10 * TB_CLK_HALF);
  localparam RST_DELAY    = (51 * TB_CLK_HALF);  // align with clock posedge

  localparam DROP_WAIT    = 4;


  //---------------------------------------------------------------
  // internal signals
  //---------------------------------------------------------------
  reg                       clk_tb;
  reg                       reset_tb;

  reg                [31:0] pkt_counter_tb;

  reg                       clk_dut;
  reg                       reset_dut;

  reg                [31:0] drop_wait_tb;

  reg                [31:0] reg_key_tb   [NUM_RREGS - 1:0];
  reg                [31:0] reg_mask_tb  [NUM_RREGS - 1:0];
  reg                 [2:0] reg_route_tb [NUM_RREGS - 1:0];

  reg   [PACKET_BITS - 1:0] pkt_in_data_tb;
  reg                       pkt_in_vld_tb;
  wire                      pkt_in_rdy_tb;

  wire  [PACKET_BITS - 1:0] pkt_out_data_tb [NUM_CHANNELS - 1:0];
  wire                      pkt_out_vld_tb  [NUM_CHANNELS - 1:0];
  reg                       pkt_out_rdy_tb  [NUM_CHANNELS - 1:0];

  wire                [1:0] rt_cnt_tb;

  //---------------------------------------------------------------
  // dut: packet router
  //---------------------------------------------------------------
  pkt_router
  #(
      .NUM_RREGS          (NUM_RREGS)
    )
  dut (
      .clk                (clk_dut)
    , .reset              (reset_dut)

    , .drop_wait_in       (drop_wait_tb)

      // routing table data from register bank
    , .reg_key_in         (reg_key_tb)
    , .reg_mask_in        (reg_mask_tb)
    , .reg_route_in       (reg_route_tb)

      // assembled packet
    , .pkt_in_data_in     (pkt_in_data_tb)
    , .pkt_in_vld_in      (pkt_in_vld_tb)
    , .pkt_in_rdy_out     (pkt_in_rdy_tb)

      // outgoing packets
    , .pkt_out_data_out   (pkt_out_data_tb)
    , .pkt_out_vld_out    (pkt_out_vld_tb)
    , .pkt_out_rdy_in     (pkt_out_rdy_tb)

      // packet counter
    , .rt_cnt_out         (rt_cnt_tb)
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
      drop_wait_tb <= 0;
    else
      drop_wait_tb <= DROP_WAIT;

  genvar te;
  generate
    for (te = 0; te < NUM_RREGS; te = te + 1)
      always @ (posedge clk_tb or posedge reset_tb)
        if (reset_tb)
          begin
            reg_key_tb[te]   <= 32'b0;
            reg_mask_tb[te]  <= 32'b0;
            reg_route_tb[te] <=  3'b0;
          end
        else
          begin
            reg_key_tb[te]   <= te;
            reg_mask_tb[te]  <= 32'h0000_001f;
            reg_route_tb[te] <= te & 32'h0000_0007;
          end
  endgenerate
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // drive dut input packet interface
  //---------------------------------------------------------------
  always @ (posedge clk_tb or posedge reset_tb)
    if (reset_tb)
      pkt_in_data_tb <= 0;
    else
      if (pkt_in_vld_tb && pkt_in_rdy_tb)
        pkt_in_data_tb <= pkt_in_data_tb + (1 << KEY_LSB);

  always @ (posedge clk_tb or posedge reset_tb)
    if (reset_tb)
      pkt_in_vld_tb <= 1'b0;
    else
      pkt_in_vld_tb <= 1'b1;
  //---------------------------------------------------------------

  //---------------------------------------------------------------
  // drive dut output packet interface
  //---------------------------------------------------------------
  genvar chan;
  generate
    for (chan = 0; chan < NUM_CHANNELS; chan = chan + 1)
      always @ (posedge clk_tb or posedge reset_tb)
        if (reset_tb)
          pkt_out_rdy_tb[chan] = 1'b0;
        else
          //pkt_out_rdy_tb[chan] = 1'b1;
          if ((pkt_counter_tb >= 3) && (pkt_counter_tb <= 20))
            pkt_out_rdy_tb[chan] = 1'b0;
          else if (pkt_out_vld_tb[chan])
            pkt_out_rdy_tb[chan] = ~pkt_out_rdy_tb[chan];
  endgenerate
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
