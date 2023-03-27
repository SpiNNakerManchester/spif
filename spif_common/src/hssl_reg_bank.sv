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
//  Last modified on : Sun  2 Oct 17:47:23 CEST 2022
//  Last modified by : lap
// -------------------------------------------------------------------------
// COPYRIGHT
//  Copyright (c) The University of Manchester, 2020-2022.
//  SpiNNaker Project
//  Advanced Processor Technologies Group
//  School of Computer Science
// -------------------------------------------------------------------------
// TODO
//  * everything
// -------------------------------------------------------------------------

`include "dvs_on_hssl_top.h"
`include "hssl_reg_bank.h"


`timescale 1ps/1ps
module hssl_reg_bank
#(
    parameter HW_VERSION   = 0,
    parameter HW_NUM_PIPES = 1,
    parameter HW_NUM_OUTPS = 1,
    parameter TARGET_FPGA  = 0
)
(
  input  wire                       clk,
  input  wire                       resetn,

  // APB interface
  input  wire                 [0:0] apb_psel_in,
  input  wire                       apb_penable_in,
  input  wire                       apb_pwrite_in,

  input  wire [`APB_ADR_BITS - 1:0] apb_paddr_in,
  input  wire                [31:0] apb_pwdata_in,
  output reg                 [31:0] apb_prdata_out,

  output reg                  [0:0] apb_pready_out,
  output wire                 [0:0] apb_pslverr_out,

  // packet receiver interface
  input  wire [`REG_ADR_BITS - 1:0] prx_addr_in,
  input  wire                [31:0] prx_wdata_in,
  input  wire                       prx_en_in,

  // packet counter enables interface
  input  wire    [`NUM_DCREGS - 1:0] ctr_cnt_in,

  // status signals
  input  wire                       hs_complete_in,
  input  wire                       hs_mismatch_in,
  input  wire [`HW_SNTL_BITS - 1:0] idsi_in,

  // hssl interface
  output wire                       hssl_stop_out,

  // packet counter interface
  output reg                 [31:0] reg_ctr_out [`NUM_DCREGS - 1:0],
  output wire                [31:0] reply_key_out,

  // wait values for packet dropping
  output wire                [31:0] input_wait_out,
  output wire                [31:0] output_wait_out,

  // event frame parameters
  output wire                [31:0] output_tick_out,
  output wire                 [9:0] output_size_out,

  // input router interface
  output reg                 [31:0] reg_rt_key_out   [`NUM_RTREGS - 1:0],
  output reg                 [31:0] reg_rt_mask_out  [`NUM_RTREGS - 1:0],
  output reg    [`RTRTE_BITS - 1:0] reg_rt_route_out [`NUM_RTREGS - 1:0],

  // mapper interface
  output reg                 [31:0] reg_mp_key_out  [HW_NUM_PIPES - 1:0],
  output reg                 [31:0] reg_mp_fmsk_out [`NUM_MPREGS - 1:0],
  output reg    [`MPSFT_BITS - 1:0] reg_mp_fsft_out [`NUM_MPREGS - 1:0],
  output reg                 [31:0] reg_mp_flmt_out [`NUM_MPREGS - 1:0],

  // filter
  output reg                 [31:0] reg_fl_val_out [`NUM_FLREGS - 1:0],
  output reg                 [31:0] reg_fl_msk_out [`NUM_FLREGS - 1:0],

  // distiller
  output reg                 [31:0] reg_ds_key_out [`NUM_DSREGS - 1:0],
  output reg                 [31:0] reg_ds_msk_out [`NUM_DSREGS - 1:0],
  output reg    [`DSSFT_BITS - 1:0] reg_ds_sft_out [`NUM_DSREGS - 1:0]
);


  // use local parameters for consistent definitions
  localparam NUM_IFREGS     = `NUM_IFREGS;
  localparam NUM_RTREGS     = `NUM_RTREGS;
  localparam NUM_DCREGS     = `NUM_DCREGS;
  localparam NUM_MPREGS     = `NUM_MPREGS;
  localparam NUM_FLREGS     = `NUM_FLREGS;
  localparam NUM_DSREGS     = `NUM_DSREGS;

  // registers are associated by type in sections of 16 registers
  localparam SEC_BITS       = `SEC_BITS;
  localparam REG_BITS       = `REG_BITS;

  // addresses are "word" addresses
  localparam SEC_LSB        = `SEC_LSB;
  localparam REG_LSB        = `REG_LSB;

  // register type / section
  localparam IFCAS_SEC      = `IFCAS_SEC;  // interface control & status
  localparam RTKEY_SEC      = `RTKEY_SEC;  // input router keys
  localparam RTMSK_SEC      = `RTMSK_SEC;  // input router masks
  localparam RTRTE_SEC      = `RTRTE_SEC;  // input router routes
  localparam DCCNT_SEC      = `DCCNT_SEC;  // diagnostic counters
  localparam MPKEY_SEC      = `MPKEY_SEC;  // mapper keys
  localparam MPMSK_SEC      = `MPMSK_SEC;  // mapper field masks
  localparam MPSFT_SEC      = `MPSFT_SEC;  // mapper field shifts
  localparam MPLMT_SEC      = `MPLMT_SEC;  // mapper field limits
  localparam FLVAL_SEC      = `FLVAL_SEC;  // filter values
  localparam FLMSK_SEC      = `FLMSK_SEC;  // filter masks
  localparam DSKEY_SEC      = `DSKEY_SEC;  // filter masks
  localparam DSMSK_SEC      = `DSMSK_SEC;  // filter masks
  localparam DSSFT_SEC      = `DSSFT_SEC;  // filter masks

  localparam BAD_REG        = `BAD_REG;

  //NOTE: APB addresses are "byte" addresses - turn to "word"!
  localparam APB_SEC_LSB    = SEC_LSB + 2;
  localparam APB_REG_LSB    = REG_LSB + 2;

  // general purpose registers
  localparam HSSL_STOP_REG  = 0;
  localparam SOFT_RES_REG   = 1;
  localparam REPLY_KEY_REG  = 2;
  localparam IN_WAIT_REG    = 3;
  localparam OUT_WAIT_REG   = 4;
  localparam OUT_TICK_REG   = 5;
  localparam OUT_SIZE_REG   = 6;

  // not real registers - collect signals
  localparam STATUS_REG     = 14;
  localparam HW_VER_REG     = 15;

  // register default values
  localparam HSSL_STOP_DEF  = 1'b0;
  localparam REPLY_KEY_DEF  = 32'hffff_fd00;  // remote reply routing key
  localparam IN_WAIT_DEF    = 32;
  localparam OUT_WAIT_DEF   = 0;
  localparam OUT_TICK_DEF   = 1000;
  localparam OUT_SIZE_DEF   = 256;

  localparam RT_KEY_DEF     = 32'hffff_ffff;  // force a miss
  localparam RT_MSK_DEF     = 32'h0000_0000;
  localparam RT_RTE_DEF     =  3'd0;

  localparam MP_KEY_DEF     = 32'h0000_0000;
  localparam MP_MSK_DEF     = 32'h0000_0000;
  localparam MP_SFT_DEF     = `MPSFT_BITS'd0;
  localparam MP_LMT_DEF     = 32'hffff_ffff;

  localparam FL_VAL_DEF     = 32'hffff_ffff;
  localparam FL_MSK_DEF     = 32'h0000_0000;

  localparam DS_KEY_DEF     = 32'h0000_0000;
  localparam DS_MSK_DEF     = 32'hffff_ffff;
  localparam DS_SFT_DEF     = `DSSFT_BITS'd0;

  //---------------------------------------------------------------
  // internal signals
  //---------------------------------------------------------------
  reg [31:0] reg_hssl_int [NUM_IFREGS - 1:0];

  // drive output signals
  assign hssl_stop_out   = reg_hssl_int[HSSL_STOP_REG][0];
  assign reply_key_out   = reg_hssl_int[REPLY_KEY_REG];
  assign input_wait_out  = reg_hssl_int[IN_WAIT_REG];
  assign output_wait_out = reg_hssl_int[OUT_WAIT_REG];
  assign output_tick_out = reg_hssl_int[OUT_TICK_REG];
  assign output_size_out = reg_hssl_int[OUT_SIZE_REG];

  // APB register access
  wire                  apb_read  = apb_psel_in && !apb_pwrite_in;
  wire                  apb_write = apb_psel_in &&  apb_pwrite_in;
  wire [SEC_BITS - 1:0] apb_sec   = apb_paddr_in[APB_SEC_LSB +: SEC_BITS];
  wire [REG_BITS - 1:0] apb_reg   = apb_paddr_in[APB_REG_LSB +: REG_BITS];

  // packet register access
  wire                  prx_write = prx_en_in;
  wire [SEC_BITS - 1:0] prx_sec   = prx_addr_in[SEC_LSB +: SEC_BITS];
  wire [REG_BITS - 1:0] prx_reg   = prx_addr_in[REG_LSB +: REG_BITS];

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
      for (i = 0; i < NUM_DCREGS; i = i + 1)
        begin
          wire apb_ctr_wr = apb_write && (apb_sec == DCCNT_SEC) && (apb_reg == i);
          wire prx_ctr_wr = prx_write && (prx_sec == DCCNT_SEC) && (prx_reg == i);

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
        reg_hssl_int[OUT_TICK_REG]  <= OUT_TICK_DEF;
        reg_hssl_int[OUT_SIZE_REG]  <= OUT_SIZE_DEF;
        reg_rt_key_out              <= '{NUM_RTREGS   {RT_KEY_DEF}};
        reg_rt_mask_out             <= '{NUM_RTREGS   {RT_MSK_DEF}};
        reg_rt_route_out            <= '{NUM_RTREGS   {RT_RTE_DEF}};
        reg_mp_key_out              <= '{HW_NUM_PIPES {MP_KEY_DEF}};
        reg_mp_fmsk_out             <= '{NUM_MPREGS   {MP_MSK_DEF}};
        reg_mp_fsft_out             <= '{NUM_MPREGS   {MP_SFT_DEF}};
        reg_mp_flmt_out             <= '{NUM_MPREGS   {MP_LMT_DEF}};
        reg_fl_val_out              <= '{NUM_FLREGS   {FL_VAL_DEF}};
        reg_fl_msk_out              <= '{NUM_FLREGS   {FL_MSK_DEF}};
        reg_ds_key_out              <= '{NUM_DSREGS   {DS_KEY_DEF}};
        reg_ds_msk_out              <= '{NUM_DSREGS   {DS_MSK_DEF}};
        reg_ds_sft_out              <= '{NUM_DSREGS   {DS_SFT_DEF}};
      end
    else
      // SOFT_RES_REG is not a real register - restore default values
      if ((prx_write && (prx_sec == IFCAS_SEC) && (prx_reg == SOFT_RES_REG)) ||
          (apb_write && (apb_sec == IFCAS_SEC) && (apb_reg == SOFT_RES_REG)))
        begin
          reg_hssl_int[HSSL_STOP_REG] <= HSSL_STOP_DEF;
          reg_hssl_int[REPLY_KEY_REG] <= REPLY_KEY_DEF;
          reg_hssl_int[IN_WAIT_REG]   <= IN_WAIT_DEF;
          reg_hssl_int[OUT_WAIT_REG]  <= OUT_WAIT_DEF;
          reg_hssl_int[OUT_TICK_REG]  <= OUT_TICK_DEF;
          reg_hssl_int[OUT_SIZE_REG]  <= OUT_SIZE_DEF;
          reg_rt_key_out              <= '{NUM_RTREGS   {RT_KEY_DEF}};
          reg_rt_mask_out             <= '{NUM_RTREGS   {RT_MSK_DEF}};
          reg_rt_route_out            <= '{NUM_RTREGS   {RT_RTE_DEF}};
          reg_mp_key_out              <= '{HW_NUM_PIPES {MP_KEY_DEF}};
          reg_mp_fmsk_out             <= '{NUM_MPREGS   {MP_MSK_DEF}};
          reg_mp_fsft_out             <= '{NUM_MPREGS   {MP_SFT_DEF}};
          reg_mp_flmt_out             <= '{NUM_MPREGS   {MP_LMT_DEF}};
          reg_fl_val_out              <= '{NUM_FLREGS   {FL_VAL_DEF}};
          reg_fl_msk_out              <= '{NUM_FLREGS   {FL_MSK_DEF}};
          reg_ds_key_out              <= '{NUM_DSREGS   {DS_KEY_DEF}};
          reg_ds_msk_out              <= '{NUM_DSREGS   {DS_MSK_DEF}};
          reg_ds_sft_out              <= '{NUM_DSREGS   {DS_SFT_DEF}};
        end
      else if (prx_write)
        case (prx_sec)
          IFCAS_SEC: reg_hssl_int[prx_reg]     <= prx_wdata_in;
          RTKEY_SEC: reg_rt_key_out[prx_reg]   <= prx_wdata_in;
          RTMSK_SEC: reg_rt_mask_out[prx_reg]  <= prx_wdata_in;
          RTRTE_SEC: reg_rt_route_out[prx_reg] <= prx_wdata_in;
          MPKEY_SEC: reg_mp_key_out[prx_reg]   <= prx_wdata_in;
          MPMSK_SEC: reg_mp_fmsk_out[prx_reg]  <= prx_wdata_in;
          MPSFT_SEC: reg_mp_fsft_out[prx_reg]  <= prx_wdata_in;
          MPLMT_SEC: reg_mp_flmt_out[prx_reg]  <= prx_wdata_in;
          FLVAL_SEC: reg_fl_val_out[prx_reg]   <= prx_wdata_in;
          FLMSK_SEC: reg_fl_msk_out[prx_reg]   <= prx_wdata_in;
          DSKEY_SEC: reg_ds_key_out[prx_reg]   <= prx_wdata_in;
          DSMSK_SEC: reg_ds_msk_out[prx_reg]   <= prx_wdata_in;
          DSSFT_SEC: reg_ds_sft_out[prx_reg]   <= prx_wdata_in;
        endcase
      else if (apb_write)
        case (apb_sec)
          IFCAS_SEC: reg_hssl_int[apb_reg]     <= apb_pwdata_in;
          RTKEY_SEC: reg_rt_key_out[apb_reg]   <= apb_pwdata_in;
          RTMSK_SEC: reg_rt_mask_out[apb_reg]  <= apb_pwdata_in;
          RTRTE_SEC: reg_rt_route_out[apb_reg] <= apb_pwdata_in;
          MPKEY_SEC: reg_mp_key_out[apb_reg]   <= apb_pwdata_in;
          MPMSK_SEC: reg_mp_fmsk_out[apb_reg]  <= apb_pwdata_in;
          MPSFT_SEC: reg_mp_fsft_out[apb_reg]  <= apb_pwdata_in;
          MPLMT_SEC: reg_mp_flmt_out[apb_reg]  <= apb_pwdata_in;
          FLVAL_SEC: reg_fl_val_out[apb_reg]   <= apb_pwdata_in;
          FLMSK_SEC: reg_fl_msk_out[apb_reg]   <= apb_pwdata_in;
          DSKEY_SEC: reg_ds_key_out[apb_reg]   <= apb_pwdata_in;
          DSMSK_SEC: reg_ds_msk_out[apb_reg]   <= apb_pwdata_in;
          DSSFT_SEC: reg_ds_sft_out[apb_reg]   <= apb_pwdata_in;
        endcase

  // APB register reads
  //NOTE: packet reads not yet supported
  always @ (posedge clk)
    if (apb_read)
      case (apb_sec)
        IFCAS_SEC: if (apb_reg < NUM_IFREGS)
                     apb_prdata_out <= reg_hssl_int[apb_reg];
                   else if (apb_reg == STATUS_REG)
                     apb_prdata_out <= {12'h5ec, TARGET_FPGA, hs_mismatch_in, hs_complete_in, idsi_in};
                   else if (apb_reg == HW_VER_REG)
                     apb_prdata_out <= {HW_NUM_OUTPS, HW_NUM_PIPES, HW_VERSION};
                   else
                     apb_prdata_out <= BAD_REG;

        RTKEY_SEC: if (apb_reg < NUM_RTREGS)
                     apb_prdata_out <= reg_rt_key_out[apb_reg];
                   else
                     apb_prdata_out <= BAD_REG;

        RTMSK_SEC: if (apb_reg < NUM_RTREGS)
                     apb_prdata_out <= reg_rt_mask_out[apb_reg];
                   else
                     apb_prdata_out <= BAD_REG;

        RTRTE_SEC: if (apb_reg < NUM_RTREGS)
                     apb_prdata_out <= reg_rt_route_out[apb_reg];
                   else
                     apb_prdata_out <= BAD_REG;

        DCCNT_SEC: if (apb_reg < NUM_DCREGS)
                     apb_prdata_out <= reg_ctr_out[apb_reg];
                   else
                     apb_prdata_out <= BAD_REG;

        MPKEY_SEC: if (apb_reg < HW_NUM_PIPES)
                     apb_prdata_out <= reg_mp_key_out[apb_reg];
                   else
                     apb_prdata_out <= BAD_REG;

        MPMSK_SEC: if (apb_reg < NUM_MPREGS)
                     apb_prdata_out <= reg_mp_fmsk_out[apb_reg];
                   else
                     apb_prdata_out <= BAD_REG;

        MPSFT_SEC: if (apb_reg < NUM_MPREGS)
                     apb_prdata_out <= $signed(reg_mp_fsft_out[apb_reg]);
                   else
                     apb_prdata_out <= BAD_REG;

        MPLMT_SEC: if (apb_reg < NUM_MPREGS)
                     apb_prdata_out <= reg_mp_flmt_out[apb_reg];
                   else
                     apb_prdata_out <= BAD_REG;

        FLVAL_SEC: if (apb_reg < NUM_FLREGS)
                     apb_prdata_out <= reg_fl_val_out[apb_reg];
                   else
                     apb_prdata_out <= BAD_REG;

        FLMSK_SEC: if (apb_reg < NUM_FLREGS)
                     apb_prdata_out <= reg_fl_msk_out[apb_reg];
                   else
                     apb_prdata_out <= BAD_REG;

        DSKEY_SEC: if (apb_reg < NUM_DSREGS)
                     apb_prdata_out <= reg_ds_key_out[apb_reg];
                   else
                     apb_prdata_out <= BAD_REG;

        DSMSK_SEC: if (apb_reg < NUM_DSREGS)
                     apb_prdata_out <= reg_ds_msk_out[apb_reg];
                   else
                     apb_prdata_out <= BAD_REG;

        DSSFT_SEC: if (apb_reg < NUM_DSREGS)
                     apb_prdata_out <= reg_ds_sft_out[apb_reg];
                   else
                     apb_prdata_out <= BAD_REG;

        default: apb_prdata_out <= BAD_REG;
      endcase
  //---------------------------------------------------------------
endmodule
