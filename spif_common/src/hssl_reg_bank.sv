// -------------------------------------------------------------------------
//  hssl_reg_bank
//
//  registers used for configuration and data collection
//
// -------------------------------------------------------------------------
// AUTHOR
//  lap - luis.plana@manchester.ac.uk
// -------------------------------------------------------------------------
// DETAILS
//  Created on       : 21 Oct 2020
//  Last modified on : Thu  1 Jul 09:54:58 BST 2021
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
module hssl_reg_bank
#(
    parameter NUM_HREGS = 2,
    parameter NUM_RREGS = 16,
    parameter NUM_CREGS = 2,
    parameter NUM_MREGS = 4
)
(
  input  wire                      clk,
  input  wire                      resetn,

  // APB interface
  input  wire                [0:0] apb_psel_in,
  input  wire                      apb_penable_in,
  input  wire                      apb_pwrite_in,

  input  wire               [39:0] apb_paddr_in,
  input  wire               [31:0] apb_pwdata_in,
  output reg                [31:0] apb_prdata_out,

  output reg                 [0:0] apb_pready_out,
  output wire                [0:0] apb_pslverr_out,

  // pkt receiver interface
  input  wire                [7:0] prx_addr_in,
  input  wire               [31:0] prx_data_in,
  input  wire                      prx_vld_in,

  // pkt counter interface
  input  wire    [NUM_CREGS - 1:0] ctr_cnt_in,

  // hssl interface
  output wire                      hssl_stop_out,

  // input router interface
  output reg                [31:0] reg_rt_key_out   [NUM_RREGS - 1:0],
  output reg                [31:0] reg_rt_mask_out  [NUM_RREGS - 1:0],
  output reg                 [2:0] reg_rt_route_out [NUM_RREGS - 1:0],

  // mapper interface
  output wire               [31:0] mp_key,
  output reg                [31:0] reg_mp_fmsk_out [NUM_MREGS - 1:0],
  output reg                 [2:0] reg_mp_fsft_out [NUM_MREGS - 1:0]
);

  genvar i;

  localparam HREGS = 3'd0;
  localparam KREGS = 3'd1;
  localparam MREGS = 3'd2;
  localparam RREGS = 3'd3;
  localparam CREGS = 3'd4;
  localparam AREGS = 3'd5;
  localparam SREGS = 3'd6;

  localparam SEC_BITS = 3;
  localparam REG_BITS = 4;

  //NOTE: APB addresses are "byte" addresses - turn to "word"!
  localparam APB_SEC_LSB = 6;
  localparam APB_NUM_LSB = 2;

  //NOTE: packet addresses are already "word" addresses
  localparam PRX_SEC_LSB = 4;
  localparam PRX_NUM_LSB = 0;

  //---------------------------------------------------------------
  // internal signals
  //---------------------------------------------------------------
  reg [31:0] reg_hssl_out [NUM_RREGS - 1:0];

  assign hssl_stop_out = reg_hssl_out[0][0];
  assign mp_key = reg_hssl_out[1];

  // APB register access
  wire       apb_read    = apb_psel_in && !apb_pwrite_in;
  wire       apb_write   = apb_psel_in &&  apb_pwrite_in;
  wire [2:0] apb_reg_sec = apb_paddr_in[APB_SEC_LSB +: SEC_BITS];
  wire [3:0] apb_reg_num = apb_paddr_in[APB_NUM_LSB +: REG_BITS];

  // packet register access
  wire       prx_write   = prx_vld_in;
  wire [2:0] prx_reg_sec = prx_addr_in[PRX_SEC_LSB +: SEC_BITS];
  wire [3:0] prx_reg_num = prx_addr_in[PRX_NUM_LSB +: REG_BITS];

  // detect simultaneous APB and packet writes
  wire reg_wr_cflt = apb_read && prx_write;

  // APB status
  // delay APB if simultaneous APB and packet writes
  always @ (posedge clk or negedge resetn)
    if (resetn == 0)
      apb_pready_out <= 1'b0;
    else
      apb_pready_out <= !reg_wr_cflt;

  assign apb_pslverr_out = 1'b0;

  // diagnostic counters
  //NOTE: register writes have priority over counting
  reg [31:0] ctr_reg_int [NUM_CREGS - 1:0];
  generate
    for (i = 0; i < NUM_CREGS; i = i + 1)
      begin
        wire apb_ctr_wr = apb_write && (apb_reg_sec == CREGS) && (apb_reg_num == i);
        wire prx_ctr_wr = prx_write && (prx_reg_sec == CREGS) && (prx_reg_num == i);

        always @ (posedge clk or negedge resetn)
          if (resetn == 0)
            ctr_reg_int[i] <= 32'd0;
          else
            casex ({prx_ctr_wr, apb_ctr_wr, ctr_cnt_in[i]})
              3'b1xx: ctr_reg_int[i] <= prx_data_in;
              3'b01x: ctr_reg_int[i] <= apb_pwdata_in;
              3'b001: ctr_reg_int[i] <= ctr_reg_int[i] + 1;
            endcase
      end
  endgenerate

  // register writes
  //NOTE: packet writes have priority - APB writes can be delayed
  always @ (posedge clk or negedge resetn)
    if (resetn == 0)
      begin
        reg_hssl_out[0] <= 1'b0;
      end
    else
      if (prx_write)
        case (prx_reg_sec)
          HREGS: reg_hssl_out[prx_reg_num]     <= prx_data_in;
          KREGS: reg_rt_key_out[prx_reg_num]   <= prx_data_in;
          MREGS: reg_rt_mask_out[prx_reg_num]  <= prx_data_in;
          RREGS: reg_rt_route_out[prx_reg_num] <= prx_data_in;
          AREGS: reg_mp_fmsk_out[prx_reg_num]  <= prx_data_in;
          SREGS: reg_mp_fsft_out[prx_reg_num]  <= prx_data_in;
        endcase
      else if (apb_write)
        case (apb_reg_sec)
          HREGS: reg_hssl_out[apb_reg_num]     <= apb_pwdata_in;
          KREGS: reg_rt_key_out[apb_reg_num]   <= apb_pwdata_in;
          MREGS: reg_rt_mask_out[apb_reg_num]  <= apb_pwdata_in;
          RREGS: reg_rt_route_out[apb_reg_num] <= apb_pwdata_in;
          AREGS: reg_mp_fmsk_out[apb_reg_num]  <= apb_pwdata_in;
          SREGS: reg_mp_fsft_out[apb_reg_num]  <= apb_pwdata_in;
        endcase

  // APB register reads
  //NOTE: packet reads not yet supported
  always @ (posedge clk)
    if (apb_read)
      case (apb_reg_sec)
        HREGS:   apb_prdata_out <= reg_hssl_out[apb_reg_num];
        KREGS:   apb_prdata_out <= reg_rt_key_out[apb_reg_num];
        MREGS:   apb_prdata_out <= reg_rt_mask_out[apb_reg_num];
        RREGS:   apb_prdata_out <= reg_rt_route_out[apb_reg_num];
        CREGS:   apb_prdata_out <= ctr_reg_int[apb_reg_num];
        AREGS:   apb_prdata_out <= reg_mp_fmsk_out[apb_reg_num];
        SREGS:   apb_prdata_out <= reg_mp_fsft_out[apb_reg_num];
        default: apb_prdata_out <= 32'hdead_beef;
      endcase
  //---------------------------------------------------------------
endmodule
