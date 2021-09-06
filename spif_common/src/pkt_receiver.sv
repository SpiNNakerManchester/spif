// -------------------------------------------------------------------------
//  pkt_receiver
//
//  checks incoming SpiNNaker multicast packets for errors and steers
//  them to the peripheral interface or the configuration block
//
// -------------------------------------------------------------------------
// AUTHOR
//  lap - luis.plana@manchester.ac.uk
// -------------------------------------------------------------------------
// DETAILS
//  Created on       : 21 Jun 2021
//  Last modified on : Thu 15 Jul 18:56:59 BST 2021
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
`include "hssl_reg_bank.h"


`timescale 1ps/1ps
module pkt_receiver
(
  input  wire                     clk,
  input  wire                     reset,

  // incoming packets from transceiver
  input  wire [PACKET_BITS - 1:0] pkt_data_in,
  input  wire                     pkt_vld_in,
  output reg                      pkt_rdy_out,

  // packet counters
  input  wire              [31:0] reg_ctr_in [NUM_CREGS - 1:0],
  input  wire              [31:0] reply_key_in,

  // register bank interface
  output wire  [RADDR_BITS - 1:0] prx_addr_out,
  output wire              [31:0] prx_wdata_out,
  output wire                     prx_en_out,

  // diagnostic counter packet
  output wire [PACKET_BITS - 1:0] dcp_data_out,
  output wire                     dcp_vld_out,
  input  wire                     dcp_rdy_in,

  // peripheral packet
  output reg  [PACKET_BITS - 1:0] per_data_out,
  output reg                      per_vld_out,
  input  wire                     per_rdy_in,

  // packet counter enables
  output wire               [1:0] prx_cnt_out
);


  // use local parameters for consistent definitions
  localparam PKT_KEY_SZ = 32;
  localparam PKT_PLD_SZ = 32;

  localparam PACKET_BITS = `PKT_BITS;
  localparam PKT_LNG_BIT = 1;
  localparam PKT_CFG_BIT = 4;
  localparam PKT_KEY_BIT = 8;
  localparam PKT_PLD_BIT = PKT_KEY_BIT + PKT_KEY_SZ;

  // counters
  localparam RADDR_BITS  = `RADDR_BITS;

  localparam NUM_CREGS   = `NUM_CREGS;
  localparam SEC_BITS    = `SEC_BITS;
  localparam REG_BITS    = `REG_BITS;
  localparam CREGS_SEC   = `CREGS_SEC;
  localparam BAD_REG     = `BAD_REG;

  // switch ports
  localparam PER_OUT     = 0;
  localparam CFG_OUT     = 1;
  localparam SP          = 2;


  //---------------------------------------------------------------
  // internal signals
  //---------------------------------------------------------------
  // switch output ports
  //NOTE: must be split into peripheral and configuration interfaces
  wire [(PACKET_BITS * SP) - 1:0] swi_data;
  wire                 [SP - 1:0] swi_vld;
  wire                 [SP - 1:0] swi_rdy;

  wire        [PACKET_BITS - 1:0] cfg_data;
  wire                            cfg_vld;
  wire                            cfg_rdy;

  //NOTE: emergency routing bits replaced with packet type code!
  wire cfg_pkt = pkt_data_in[PKT_CFG_BIT];

  wire [SP - 1:0] route;
  assign route[PER_OUT] = !cfg_pkt;
  assign route[CFG_OUT] =  cfg_pkt;


  //---------------------------------------------------------------
  // input switch
  //---------------------------------------------------------------
  // split peripheral and configuration packets
  spio_switch
  #(
        .PKT_BITS             (PACKET_BITS)
      , .NUM_PORTS            (SP)
      )
  swi (
        .CLK_IN               (clk)
      , .RESET_IN             (reset)

      , .IN_OUTPUT_SELECT_IN  (route)

      , .IN_DATA_IN           (pkt_data_in)
      , .IN_VLD_IN            (pkt_vld_in)
      , .IN_RDY_OUT           (pkt_rdy_out)

      , .OUT_DATA_OUT         (swi_data)
      , .OUT_VLD_OUT          (swi_vld)
      , .OUT_RDY_IN           (swi_rdy)

      , .BLOCKED_OUTPUTS_OUT  ()
      , .SELECTED_OUTPUTS_OUT ()

        //NOTE: no packet dropping!
      , .DROP_IN              (1'b0)

      , .DROPPED_DATA_OUT     ()
      , .DROPPED_OUTPUTS_OUT  ()
      , .DROPPED_VLD_OUT      ()
      );

  // peripheral packets go out directly from switch output
  assign per_data_out     = swi_data[PER_OUT * PACKET_BITS +: PACKET_BITS];
  assign per_vld_out      = swi_vld[PER_OUT];
  assign swi_rdy[PER_OUT] = per_rdy_in;

  // config packets managed internally
  assign cfg_data         = swi_data[CFG_OUT * PACKET_BITS +: PACKET_BITS];
  assign cfg_vld          = swi_vld[CFG_OUT];
  assign swi_rdy[CFG_OUT] = cfg_rdy;
  //---------------------------------------------------------------


  //---------------------------------------------------------------
  // configuration packet handling
  //---------------------------------------------------------------
  // payload presence indicates read or write
  wire pld_pst = cfg_data[PKT_LNG_BIT];

  // config packet always ready on writes, check on reads
  assign cfg_rdy = cfg_vld && (pld_pst || dcp_rdy_in);

  // diagnostic counters can be read
  //NOTE: non-existing counters always read BAD_REG!
  wire [(SEC_BITS + REG_BITS) - 1:0] ctr_offset;
  wire              [SEC_BITS - 1:0] ctr_sec;
  wire              [REG_BITS - 1:0] ctr_reg;
  wire                               ctr_ok;

  assign ctr_offset = cfg_data[PKT_KEY_BIT +: RADDR_BITS];
  assign ctr_reg    = cfg_data[PKT_KEY_BIT +: REG_BITS];
  assign ctr_sec    = cfg_data[(PKT_KEY_BIT + REG_BITS) +: SEC_BITS];
  assign ctr_ok     = (ctr_reg < NUM_CREGS) && (ctr_sec == CREGS_SEC);

  wire   [7:0] dcp_hdr;
  wire  [31:0] dcp_key;
  wire  [31:0] dcp_pld;
  wire         dcp_pty;

  // basic counter read interface
  //NOTE: enable only when payload not present in configuration packet!
  // assemble reply packet with counter data
  //NOTE: use (ER bits == 2'b11) to indicate diagnostics packet!
  assign dcp_hdr = {7'b001_1001, dcp_pty};
  assign dcp_key = reply_key_in | ctr_offset;
  assign dcp_pld = ctr_ok ? reg_ctr_in[ctr_reg] : BAD_REG;
  assign dcp_pty = (^dcp_key ^ ^dcp_pld);

  // send assembled reply packet back
  assign dcp_data_out = {dcp_pld, dcp_key, dcp_hdr};
  assign dcp_vld_out  = cfg_vld && !pld_pst;

  // basic register write interface
  //NOTE: enable only when payload present in configuration packet!
  assign prx_en_out    = cfg_vld && pld_pst;
  assign prx_addr_out  = cfg_data[PKT_KEY_BIT +: RADDR_BITS];
  assign prx_wdata_out = cfg_data[PKT_PLD_BIT +: PKT_PLD_SZ];
  //---------------------------------------------------------------


  //---------------------------------------------------------------
  // packet counter enables
  //---------------------------------------------------------------
  // received packet counter enable signals
  assign prx_cnt_out[1] = ( cfg_pkt && pkt_vld_in && pkt_rdy_out);
  assign prx_cnt_out[0] = (!cfg_pkt && pkt_vld_in && pkt_rdy_out);
  //---------------------------------------------------------------
endmodule
