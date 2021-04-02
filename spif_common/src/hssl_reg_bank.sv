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
//  Last modified on : Mon  9 Nov 08:54:58 GMT 2020
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
    parameter NUM_REGS      = 1,
    parameter NUM_ENTRIES   = 16,
    parameter ROUTE_MASK    = 3'b111,
    parameter REG_SEC_LSB   = 6,
    parameter REG_NUM_LSB   = 2
)
(
  input  wire        clk,
  input  wire        resetn,

  // APB interface
  input  wire  [0:0] apb_psel_in,
  input  wire        apb_penable_in,
  input  wire        apb_pwrite_in,

  input  wire [39:0] apb_paddr_in,
  input  wire [31:0] apb_pwdata_in,
  output reg  [31:0] apb_prdata_out,

  output wire  [0:0] apb_pready_out,
  output wire  [0:0] apb_pslverr_out,

  // register interface
  output reg   [0:0] reg_hssl_out  [NUM_REGS - 1:0],
  output reg  [31:0] reg_key_out   [NUM_ENTRIES - 1:0],
  output reg  [31:0] reg_mask_out  [NUM_ENTRIES - 1:0],
  output reg   [2:0] reg_route_out [NUM_ENTRIES - 1:0]
);

  //---------------------------------------------------------------
  // internal signals
  //---------------------------------------------------------------
  wire [1:0] reg_sec = apb_paddr_in[REG_SEC_LSB +: 2];
  wire [3:0] reg_num = apb_paddr_in[REG_NUM_LSB +: 4];

  // register writes
  always @ (posedge clk or negedge resetn)
    if (resetn == 0)
      reg_hssl_out[0] <= 1'b0;
    else
      if (apb_psel_in && apb_penable_in && apb_pwrite_in)
        case (reg_sec)
          2'd0: reg_hssl_out[reg_num]  <= apb_pwdata_in;
          2'd1: reg_key_out[reg_num]   <= apb_pwdata_in;
          2'd2: reg_mask_out[reg_num]  <= apb_pwdata_in;
          2'd3: reg_route_out[reg_num] <= apb_pwdata_in;
        endcase

  // register reads
  always @ (posedge clk)
    if (apb_psel_in && !apb_pwrite_in)
      case (reg_sec)
        2'd0: apb_prdata_out <= reg_hssl_out[reg_num];
        2'd1: apb_prdata_out <= reg_key_out[reg_num];
        2'd2: apb_prdata_out <= reg_mask_out[reg_num];
        2'd3: apb_prdata_out <= reg_route_out[reg_num];
      endcase
      ;

  // APB status
  assign apb_pready_out  =  1'b1;
  assign apb_pslverr_out =  1'b0;
  //---------------------------------------------------------------
endmodule
