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

`include "hssl_reg_bank.h"


`timescale 1ps/1ps
module hssl_reg_bank
#(
    parameter HW_NUM_PIPES = 1
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

  // packet receiver interface
  input  wire   [RADDR_BITS - 1:0] prx_addr_in,
  input  wire               [31:0] prx_wdata_in,
  input  wire                      prx_en_in,

  // packet counter enables interface
  input  wire    [NUM_CREGS - 1:0] ctr_cnt_in,

  // status signals
  input  wire  [HW_VER_BITS - 1:0] hw_version_in,
  input  wire [HW_PIPE_BITS - 1:0] hw_pipe_num_in,
  input  wire [HW_FPGA_BITS - 1:0] fpga_model_in,
  input  wire                      hs_complete_in,
  input  wire                      hs_mismatch_in,
  input  wire [HW_SNTL_BITS - 1:0] idsi_in,

  // hssl interface
  output wire                      hssl_stop_out,

  // packet counter interface
  output reg                [31:0] reg_ctr_out [NUM_CREGS - 1:0],
  output wire               [31:0] reply_key_out,

  // wait values for packet dropping
  output wire               [31:0] input_wait_out,
  output wire               [31:0] output_wait_out,

  // input router interface
  output reg                [31:0] reg_rt_key_out   [NUM_RREGS - 1:0],
  output reg                [31:0] reg_rt_mask_out  [NUM_RREGS - 1:0],
  output reg     [RRTE_BITS - 1:0] reg_rt_route_out [NUM_RREGS - 1:0],

  // mapper interface
  output reg                [31:0] reg_mp_key_out  [HW_NUM_PIPES - 1:0],
  output reg                [31:0] reg_mp_fmsk_out [NUM_MREGS - 1:0],
  output reg     [MSFT_BITS - 1:0] reg_mp_fsft_out [NUM_MREGS - 1:0]
);


  // use local parameters for consistent definitions
  localparam HW_VER_BITS    = `HW_VER_BITS;
  localparam HW_PIPE_BITS   = `HW_PIPE_BITS;
  localparam HW_FPGA_BITS   = `HW_FPGA_BITS;
  localparam HW_SNTL_BITS   = `HW_SNTL_BITS;

  localparam NUM_HREGS      = `NUM_HREGS;
  localparam NUM_RREGS      = `NUM_RREGS;
  localparam NUM_CREGS      = `NUM_CREGS;
  localparam NUM_MREGS_PIPE = `NUM_MREGS_PIPE;
  localparam NUM_MREGS      = `NUM_MREGS;

  // registers are associated by type in sections of 16 registers
  localparam SEC_BITS       = `SEC_BITS;
  localparam REG_BITS       = `REG_BITS;

  // addresses are "word" addresses
  localparam SEC_LSB        = `SEC_LSB;
  localparam REG_LSB        = `REG_LSB;
  localparam RADDR_BITS     = `RADDR_BITS;

  // register type / section
  localparam HREGS_SEC      = `HREGS_SEC;  // interface control & status
  localparam KREGS_SEC      = `KREGS_SEC;  // input router keys
  localparam MREGS_SEC      = `MREGS_SEC;  // input router masks
  localparam RREGS_SEC      = `RREGS_SEC;  // input router routes
  localparam CREGS_SEC      = `CREGS_SEC;  // diagnostic counters
  localparam PREGS_SEC      = `PREGS_SEC;  // mapper keys
  localparam AREGS_SEC      = `AREGS_SEC;  // mapper field masks
  localparam SREGS_SEC      = `SREGS_SEC;  // mapper field shifts

  localparam BAD_REG        = `BAD_REG;

  localparam MSFT_BITS      = `MSFT_BITS;
  localparam RRTE_BITS      = `RRTE_BITS;

  //NOTE: APB addresses are "byte" addresses - turn to "word"!
  localparam APB_SEC_LSB    = SEC_LSB + 2;
  localparam APB_REG_LSB    = REG_LSB + 2;

  // general purpose registers
  localparam HSSL_STOP_REG  = 0;
  localparam REPLY_KEY_REG  = 2;
  localparam IN_WAIT_REG    = 3;
  localparam OUT_WAIT_REG   = 4;

  // not real registers - collect signals
  localparam STATUS_REG     = 14;
  localparam HW_VER_REG     = 15;

  // register default values
  localparam HSSL_STOP_DEF  = 1'b0;
  localparam REPLY_KEY_DEF  = 32'hffff_fd00;  // remote reply routing key
  localparam IN_WAIT_DEF    = 32;
  localparam OUT_WAIT_DEF   = 32;


  //---------------------------------------------------------------
  // internal signals
  //---------------------------------------------------------------
  reg [31:0] reg_hssl_int [NUM_HREGS - 1:0];

  // drive output signals
  assign hssl_stop_out   = reg_hssl_int[HSSL_STOP_REG][0];
  assign reply_key_out   = reg_hssl_int[REPLY_KEY_REG];
  assign input_wait_out  = reg_hssl_int[IN_WAIT_REG];
  assign output_wait_out = reg_hssl_int[OUT_WAIT_REG];

  // APB register access
  wire                  apb_read    = apb_psel_in && !apb_pwrite_in;
  wire                  apb_write   = apb_psel_in &&  apb_pwrite_in;
  wire [SEC_BITS - 1:0] apb_sec = apb_paddr_in[APB_SEC_LSB +: SEC_BITS];
  wire [REG_BITS - 1:0] apb_reg = apb_paddr_in[APB_REG_LSB +: REG_BITS];

  // packet register access
  wire                  prx_write   = prx_en_in;
  wire [SEC_BITS - 1:0] prx_sec = prx_addr_in[SEC_LSB +: SEC_BITS];
  wire [REG_BITS - 1:0] prx_reg = prx_addr_in[REG_LSB +: REG_BITS];

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
  genvar i;
  generate
    begin
      for (i = 0; i < NUM_CREGS; i = i + 1)
        begin
          wire apb_ctr_wr = apb_write && (apb_sec == CREGS_SEC) && (apb_reg == i);
          wire prx_ctr_wr = prx_write && (prx_sec == CREGS_SEC) && (prx_reg == i);

          always @ (posedge clk or negedge resetn)
            if (resetn == 0)
              reg_ctr_out[i] <= 32'd0;
            else
              casex ({prx_ctr_wr, apb_ctr_wr, ctr_cnt_in[i]})
                3'b1xx: reg_ctr_out[i] <= prx_wdata_in;
                3'b01x: reg_ctr_out[i] <= apb_pwdata_in;
                3'b001: reg_ctr_out[i] <= reg_ctr_out[i] + 1;
              endcase
        end
    end
  endgenerate

  // register writes
  //NOTE: packet writes have priority - APB writes can be delayed
  always @ (posedge clk or negedge resetn)
    if (resetn == 0)
      begin
        reg_hssl_int[HSSL_STOP_REG] <= HSSL_STOP_DEF;
        reg_hssl_int[REPLY_KEY_REG] <= REPLY_KEY_DEF;
        reg_hssl_int[IN_WAIT_REG]   <= IN_WAIT_DEF;
        reg_hssl_int[OUT_WAIT_REG]  <= OUT_WAIT_DEF;
      end
    else
      if (prx_write)
        case (prx_sec)
          HREGS_SEC: reg_hssl_int[prx_reg]     <= prx_wdata_in;
          KREGS_SEC: reg_rt_key_out[prx_reg]   <= prx_wdata_in;
          MREGS_SEC: reg_rt_mask_out[prx_reg]  <= prx_wdata_in;
          RREGS_SEC: reg_rt_route_out[prx_reg] <= prx_wdata_in;
          PREGS_SEC: reg_mp_key_out[prx_reg]   <= prx_wdata_in;
          AREGS_SEC: reg_mp_fmsk_out[prx_reg]  <= prx_wdata_in;
          SREGS_SEC: reg_mp_fsft_out[prx_reg]  <= prx_wdata_in;
        endcase
      else if (apb_write)
        case (apb_sec)
          HREGS_SEC: reg_hssl_int[apb_reg]     <= apb_pwdata_in;
          KREGS_SEC: reg_rt_key_out[apb_reg]   <= apb_pwdata_in;
          MREGS_SEC: reg_rt_mask_out[apb_reg]  <= apb_pwdata_in;
          RREGS_SEC: reg_rt_route_out[apb_reg] <= apb_pwdata_in;
          PREGS_SEC: reg_mp_key_out[apb_reg]   <= apb_pwdata_in;
          AREGS_SEC: reg_mp_fmsk_out[apb_reg]  <= apb_pwdata_in;
          SREGS_SEC: reg_mp_fsft_out[apb_reg]  <= apb_pwdata_in;
        endcase

  // APB register reads
  //NOTE: packet reads not yet supported
  always @ (posedge clk)
    if (apb_read)
      case (apb_sec)
        HREGS_SEC: if (apb_reg < NUM_HREGS)
                     apb_prdata_out <= reg_hssl_int[apb_reg];
                   else if (apb_reg == STATUS_REG)
                     apb_prdata_out <= {12'h5ec, fpga_model_in, hs_mismatch_in, hs_complete_in, idsi_in};
                   else if (apb_reg == HW_VER_REG)
                     apb_prdata_out <= {4'h0, hw_pipe_num_in, hw_version_in};
                   else
                     apb_prdata_out <= BAD_REG;

        KREGS_SEC: if (apb_reg < NUM_RREGS)
                     apb_prdata_out <= reg_rt_key_out[apb_reg];
                   else
                     apb_prdata_out <= BAD_REG;

        MREGS_SEC: if (apb_reg < NUM_RREGS)
                     apb_prdata_out <= reg_rt_mask_out[apb_reg];
                   else
                     apb_prdata_out <= BAD_REG;

        RREGS_SEC: if (apb_reg < NUM_RREGS)
                     apb_prdata_out <= reg_rt_route_out[apb_reg];
                   else
                     apb_prdata_out <= BAD_REG;

        CREGS_SEC: if (apb_reg < NUM_CREGS)
                     apb_prdata_out <= reg_ctr_out[apb_reg];
                   else
                     apb_prdata_out <= BAD_REG;

        PREGS_SEC: if (apb_reg < HW_NUM_PIPES)
                     apb_prdata_out <= reg_mp_key_out[apb_reg];
                   else
                     apb_prdata_out <= BAD_REG;

        AREGS_SEC: if (apb_reg < NUM_MREGS)
                     apb_prdata_out <= reg_mp_fmsk_out[apb_reg];
                   else
                     apb_prdata_out <= BAD_REG;

        SREGS_SEC: if (apb_reg < NUM_MREGS)
                     apb_prdata_out <= $signed(reg_mp_fsft_out[apb_reg]);
                   else
                     apb_prdata_out <= BAD_REG;

        default: apb_prdata_out <= BAD_REG;
      endcase
  //---------------------------------------------------------------
endmodule
