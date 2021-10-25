// -------------------------------------------------------------------------
//  pkt_assembler
//
//  assembles SpiNNaker multicast packets from incoming events
//
// -------------------------------------------------------------------------
// AUTHOR
//  lap - luis.plana@manchester.ac.uk
// -------------------------------------------------------------------------
// DETAILS
//  Created on       : 21 Oct 2020
//  Last modified on : Tue  7 Sep 17:35:31 BST 2021
//  Last modified by : lap
// -------------------------------------------------------------------------
// COPYRIGHT
//  Copyright (c) The University of Manchester, 2020-2021.
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
module pkt_assembler
(
  input  wire                     clk,
  input  wire                     reset,

  // event filter configuration
  input  wire              [31:0] fl_val_in [`NUM_FLREGS_PIPE - 1:0],
  input  wire              [31:0] fl_msk_in [`NUM_FLREGS_PIPE - 1:0],

  // event mapper configuration
  input  wire              [31:0] mp_key_in,
  input  wire              [31:0] mp_fld_msk_in [`NUM_MPREGS_PIPE - 1:0],
  input  wire [`MPSFT_BITS - 1:0] mp_fld_sft_in [`NUM_MPREGS_PIPE - 1:0],
  input  wire              [31:0] mp_fld_lmt_in [`NUM_MPREGS_PIPE - 1:0],

  // event inputs
  input  wire              [31:0] evt_data_in,
  input  wire                     evt_vld_in,
  output reg                      evt_rdy_out,

  // packet outputs
  output reg    [`PKT_BITS - 1:0] pkt_data_out,
  output reg                      pkt_vld_out,
  input  wire                     pkt_rdy_in
);

  
  // use local parameters for consistent definitions
  localparam NUM_MPR = `NUM_MPREGS_PIPE;
  localparam NUM_FLR = `NUM_FLREGS_PIPE;
  

  //---------------------------------------------------------------
  // internal signals
  //---------------------------------------------------------------
  // filter out / drop event
  wire [NUM_FLR - 1:0] evt_filter_int;
  wire [NUM_MPR - 1:0] evt_drop_int;
  
  // mapped event data - to be used as packet key
  wire [31:0] mapped_data_int;

  // interface status
  wire evt_present_int = evt_vld_in && !evt_drop_int && !evt_filter_int && evt_rdy_out;
  wire pkt_busy_int = pkt_vld_out && !pkt_rdy_in;

  // input event interface
  reg         parked_int;
  reg  [31:0] parked_data_int;

  // map input event data to output packet key
  wire [31:0] evt_field_int [NUM_MPR - 1:0];
  wire [31:0] field_acc_int [NUM_MPR - 1:0];

  // check if event must be filtered out
  genvar i;
  generate
    for (i = 0; i < NUM_FLR; i = i + 1)
      assign evt_filter_int[i] = (fl_val_in[i] == (evt_data_in & fl_msk_in[i]));
  endgenerate

  // extract, shift and OR together event fields
  // and check that field are within their limits
  //NOTE: could use an OR tree instead!
  generate
    begin
      for (i = 0; i < NUM_MPR; i = i + 1)
        begin
          // compute shift register 2's complement
          wire [4:0] left_shift = ~mp_fld_sft_in[i][4:0] + 1;

          // use 2's complement if left shift (negative shift value)
          assign evt_field_int[i] = mp_fld_sft_in[i][5] ?
            (evt_data_in & mp_fld_msk_in[i]) << left_shift :
            (evt_data_in & mp_fld_msk_in[i]) >> mp_fld_sft_in[i][4:0];

          // check event limit
          assign evt_drop_int[i] = evt_field_int[i] > mp_fld_lmt_in[i];
        end

      //NOTE: field accumulator 0 is a special case - see below
      for (i = 1; i < NUM_MPR; i = i + 1)
        assign field_acc_int[i] = field_acc_int[i - 1] | evt_field_int[i];
    end
  endgenerate

  // use the mapper routing key as the mapping base
  assign field_acc_int[0] = mp_key_in | evt_field_int[0];
  assign mapped_data_int  = field_acc_int[NUM_MPR - 1];

  // park mapped data when output is busy
  always @ (posedge clk or posedge reset)
    if (reset)
      parked_int <= 1'b0;
    else
      if (evt_present_int && pkt_busy_int)
        parked_int <= 1'b1;
      else if (pkt_rdy_in)
        parked_int <= 1'b0;

  always @ (posedge clk)
    if (evt_present_int && pkt_busy_int)
      parked_data_int <= mapped_data_int;

  // don't accept a new key when parked or parking data
  always @ (posedge clk or posedge reset)
    if (reset)
      evt_rdy_out <= 1'b0;
    else
      casex ({parked_int, evt_present_int, pkt_busy_int})
        //NOTE: 3'b111 must not happen - data loss!
        3'bx11,                        // busy and parking
        3'b1x1 : evt_rdy_out <= 1'b0;  // busy and parked 

        //NOTE: 3'b110 must not happen - data loss!
        3'bxx0,                        // not busy
        3'b001 : evt_rdy_out <= 1'b1;  // busy but park available
      endcase

  // output packet interface
  reg   [31:0] pkt_key_int;
  wire  [31:0] pkt_pld_int = 32'h0000_0000;
  wire         pkt_pty_int = ~(^pkt_key_int ^ ^pkt_pld_int);
  wire   [7:0] pkt_hdr_int = {7'b000_0000, pkt_pty_int};

  // used parked key when available
  always @ (*)
    if (parked_int)
      pkt_key_int = parked_data_int;
    else
      pkt_key_int = mapped_data_int;

  // packet data must not change when busy 
  always @ (posedge clk)
    if (!pkt_busy_int && (parked_int || evt_present_int))
      pkt_data_out <= {pkt_pld_int, pkt_key_int, pkt_hdr_int};

  always @ (posedge clk or posedge reset)
    if (reset)
      pkt_vld_out <= 1'b0;
    else
      casex ({parked_int, evt_present_int, pkt_busy_int})
        3'b000 : pkt_vld_out <= 1'b0;  // not busy and no data

        3'b1x0,                        // not busy and parked data
        3'bx10,                        // not busy and new data
        3'bxx1 : pkt_vld_out <= 1'b1;  // busy
      endcase
  //---------------------------------------------------------------
endmodule
