// -------------------------------------------------------------------------
//  hssl_interface
//
//  DVS input to SpiNN-5 board through High-Speed Serial Link (HSSL)
//  interface to the Gigabit transceiver
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
module hssl_interface
#(
  parameter PACKET_BITS           = 72,
  parameter NUM_CHANNELS          = 8,
  parameter NUM_CLKC_FOR_LOSS     = 128,
  parameter NUM_CLKC_FOR_SYNC     = 4,
  parameter NUM_CLKC_FOR_RX_RESET = 4,
  parameter NUM_CLKC_FOR_IDLE     = 1000
)
(
  input  wire                     clk,
  input  wire                     reset,

  input  wire [PACKET_BITS - 1:0] txpkt_data_in  [NUM_CHANNELS - 1:0],
  input  wire                     txpkt_vld_in   [NUM_CHANNELS - 1:0],
  output wire                     txpkt_rdy_out  [NUM_CHANNELS - 1:0],

  output wire [PACKET_BITS - 1:0] rxpkt_data_out [NUM_CHANNELS - 1:0],
  output wire                     rxpkt_vld_out  [NUM_CHANNELS - 1:0],
  input  wire                     rxpkt_rdy_in   [NUM_CHANNELS - 1:0],

  output reg                [1:0] loss_of_sync_state_out,
  output wire                     handshake_complete_out,
  output wire                     version_mismatch_out,

  output wire              [15:0] idsi_out,
  input  wire                     stop_in,

  output wire              [31:0] tx_data_out,
  output wire               [7:0] tx_charisk_out,
  output wire                     tx_elecidle_out,

  output reg                      rx_reset_datapath_out,
  input  wire              [31:0] rx_data_in,
  input  wire                     rx_commadet_in,
  input  wire               [3:0] rx_chariscomma_in,
  input  wire               [3:0] rx_charisk_in,
  input  wire               [3:0] rx_disperr_in,
  input  wire               [3:0] rx_encerr_in
);

  //---------------------------------------------------------------
  // internal signals
  //---------------------------------------------------------------
  // spiNNlink - tx/rx controller signals
  wire [31:0] trmt_data_int;
  wire  [3:0] trmt_charisk_int;
  wire        trmt_vld_int;
  wire        trmt_rdy_int;

  wire [31:0] rcvd_data_int;
  wire  [3:0] rcvd_charisk_int;
  wire        rcvd_vld_int;

  // status signals
  wire        handshake_phase_int;
  //---------------------------------------------------------------


  //---------------------------------------------------------------
  // spiNNlink multiplexer -
  // tx side assembles and transmits spiNNlink frames
  // rx side receives and disassembles spiNNlink frames 
  //---------------------------------------------------------------
  spio_hss_multiplexer_spinnlink spinnlink ( 
        .clk        (clk)
      , .rst        (reset)

      // diagnostic signals from frame assembler
      , .reg_sfrm   ()
      , .reg_looc   ()
      , .reg_crdt   ()
      , .reg_empt   ()
      , .reg_full   ()

      // diagnostic/control signals from/to frame transmitter
      , .reg_tfrm   ()
      , .reg_stop   (stop_in)

      // diagnostic signals from frame disassembler
      , .reg_dfrm   ()
      , .reg_crce   ()
      , .reg_frme   ()
      , .reg_rnak   ()
      , .reg_rack   ()
      , .reg_rooc   ()
      , .reg_cfcr   ()

      // diagnostic signals from packet dispatcher
      , .reg_rfrm   ()
      , .reg_busy   ()
      , .reg_lnak   ()
      , .reg_lack   ()
      , .reg_cfcl   ()

      // to high-speed serial: assembled frames out
      , .hsl_data   (trmt_data_int)
      , .hsl_kchr   (trmt_charisk_int)
      , .hsl_vld    (trmt_vld_int)
      , .hsl_rdy    (trmt_rdy_int)

      // from high-speed serial: assembled frames in
      , .ihsl_data  (rcvd_data_int)
      , .ihsl_kchr  (rcvd_charisk_int)
      , .ihsl_vld   (rcvd_vld_int)

      // incoming packet streams
      , .pkt_data0  (txpkt_data_in[0])
      , .pkt_vld0   (txpkt_vld_in[0])
      , .pkt_rdy0   (txpkt_rdy_out[0])

      , .pkt_data1  (txpkt_data_in[1])
      , .pkt_vld1   (txpkt_vld_in[1])
      , .pkt_rdy1   (txpkt_rdy_out[1])

      , .pkt_data2  (txpkt_data_in[2])
      , .pkt_vld2   (txpkt_vld_in[2])
      , .pkt_rdy2   (txpkt_rdy_out[2])

      , .pkt_data3  (txpkt_data_in[3])
      , .pkt_vld3   (txpkt_vld_in[3])
      , .pkt_rdy3   (txpkt_rdy_out[3])

      , .pkt_data4  (txpkt_data_in[4])
      , .pkt_vld4   (txpkt_vld_in[4])
      , .pkt_rdy4   (txpkt_rdy_out[4])

      , .pkt_data5  (txpkt_data_in[5])
      , .pkt_vld5   (txpkt_vld_in[5])
      , .pkt_rdy5   (txpkt_rdy_out[5])

      , .pkt_data6  (txpkt_data_in[6])
      , .pkt_vld6   (txpkt_vld_in[6])
      , .pkt_rdy6   (txpkt_rdy_out[6])

      , .pkt_data7  (txpkt_data_in[7])
      , .pkt_vld7   (txpkt_vld_in[7])
      , .pkt_rdy7   (txpkt_rdy_out[7])

      // outgoing packet streams
      , .opkt_data0 (rxpkt_data_out[0])
      , .opkt_vld0  (rxpkt_vld_out[0])
      , .opkt_rdy0  (rxpkt_rdy_in[0])

      , .opkt_data1 (rxpkt_data_out[1])
      , .opkt_vld1  (rxpkt_vld_out[1])
      , .opkt_rdy1  (rxpkt_rdy_in[1])

      , .opkt_data2 (rxpkt_data_out[2])
      , .opkt_vld2  (rxpkt_vld_out[2])
      , .opkt_rdy2  (rxpkt_rdy_in[2])

      , .opkt_data3 (rxpkt_data_out[3])
      , .opkt_vld3  (rxpkt_vld_out[3])
      , .opkt_rdy3  (rxpkt_rdy_in[3])

      , .opkt_data4 (rxpkt_data_out[4])
      , .opkt_vld4  (rxpkt_vld_out[4])
      , .opkt_rdy4  (rxpkt_rdy_in[4])

      , .opkt_data5 (rxpkt_data_out[5])
      , .opkt_vld5  (rxpkt_vld_out[5])
      , .opkt_rdy5  (rxpkt_rdy_in[5])

      , .opkt_data6 (rxpkt_data_out[6])
      , .opkt_vld6  (rxpkt_vld_out[6])
      , .opkt_rdy6  (rxpkt_rdy_in[6])

      , .opkt_data7 (rxpkt_data_out[7])
      , .opkt_vld7  (rxpkt_vld_out[7])
      , .opkt_rdy7  (rxpkt_rdy_in[7])
    );
  //---------------------------------------------------------------


  //---------------------------------------------------------------
  // HSSL interface Loss-of-sync state machine
  //---------------------------------------------------------------
  localparam LOSS_OF_SYNC_ST  = 2'b10;
  localparam RESYNC_ST        = 2'b01;
  localparam SYNC_ACQUIRED_ST = 2'b00;

  // data invalid if wrong disparity or wrong 8b/10 encoding
  wire invalid_data = (|rx_disperr_in) || (|rx_encerr_in);

  // keep track of clock cycles spent in RESYNC
  reg [7:0] loss_of_sync_cnt_int;
  always @ (posedge clk, posedge reset)
    if (reset)
      loss_of_sync_cnt_int = 0;
    else
      case (loss_of_sync_state_out)
        RESYNC_ST:
          loss_of_sync_cnt_int = loss_of_sync_cnt_int + 1;

        SYNC_ACQUIRED_ST:
          if (!handshake_complete_out)
            loss_of_sync_cnt_int = loss_of_sync_cnt_int + 1;
          else
            loss_of_sync_cnt_int = 0;

        default:
          loss_of_sync_cnt_int = 0;
      endcase

  // sync acquired after NUM_CLKC_FOR_SYNC clock cycles in RESYNC
  //NOTE: check conditions for loss of sync!
  always @ (posedge clk, posedge reset)
    if (reset)
      loss_of_sync_state_out = LOSS_OF_SYNC_ST;
    else
      case (loss_of_sync_state_out)
        LOSS_OF_SYNC_ST:
          if (rx_commadet_in)
            loss_of_sync_state_out = RESYNC_ST;

        RESYNC_ST:
          if (invalid_data)
            loss_of_sync_state_out = LOSS_OF_SYNC_ST;
          else if (loss_of_sync_cnt_int >= NUM_CLKC_FOR_SYNC)
            loss_of_sync_state_out = SYNC_ACQUIRED_ST;

        SYNC_ACQUIRED_ST:
          if (invalid_data || (loss_of_sync_cnt_int >= NUM_CLKC_FOR_LOSS))
            loss_of_sync_state_out = LOSS_OF_SYNC_ST;

        default:  // should never happen
          loss_of_sync_state_out = LOSS_OF_SYNC_ST;
      endcase

  // keep track of clock cycles spent in RX RESET
  reg [2:0] reset_rx_cnt_int;
  always @ (posedge clk, posedge reset)
    if (reset)
      reset_rx_cnt_int = 0;
    else
      case (loss_of_sync_state_out)
        LOSS_OF_SYNC_ST: reset_rx_cnt_int = reset_rx_cnt_int + 1;
        default:         reset_rx_cnt_int = 0;
      endcase

  // may need to RESET the RX datapath if the SATA cable reconnected
  always @ (posedge clk, posedge reset)
    if (reset)
      rx_reset_datapath_out = 1'b0;
    else
      case (loss_of_sync_state_out)
        LOSS_OF_SYNC_ST:
          if (reset_rx_cnt_int >= NUM_CLKC_FOR_RX_RESET)
            rx_reset_datapath_out = 1'b0;

        RESYNC_ST:
          if (invalid_data)
            rx_reset_datapath_out = 1'b1;

        SYNC_ACQUIRED_ST:
          if (invalid_data) 
            rx_reset_datapath_out = 1'b1;

        default:  // should not happen
          rx_reset_datapath_out = 1'b1;
      endcase
  //---------------------------------------------------------------


  //---------------------------------------------------------------
  // drive transceiver tx data
  //---------------------------------------------------------------
  spio_hss_multiplexer_tx_control tx_control (
      .CLK_IN                 (clk)
    , .RESET_IN               (reset)

    , .SCRMBL_IDL_DAT         (1'b0)
    , .REG_IDSO_IN            (16'hdead)

    , .HANDSHAKE_COMPLETE_IN  (handshake_complete_out)
    , .HANDSHAKE_PHASE_IN     (handshake_phase_int)

      // transceiver tx data interface
    , .TXDATA_OUT             (tx_data_out)
    , .TXCHARISK_OUT          (tx_charisk_out)

      // incoming frame interface
    , .TXDATA_IN              (trmt_data_int)
    , .TXCHARISK_IN           (trmt_charisk_int)
    , .TXVLD_IN               (trmt_vld_int)
    , .TXRDY_OUT              (trmt_rdy_int)
    );

  // make transmitter electrically idle after
  // reset to indicate reset to far receiver
  reg [9:0] idle_cnt_int;
  always @ (posedge clk, posedge reset)
    if (reset)
      idle_cnt_int = NUM_CLKC_FOR_IDLE;
    else
      if (idle_cnt_int != 0)
        idle_cnt_int = idle_cnt_int - 1;

  assign tx_elecidle_out = (idle_cnt_int != 0);
  //---------------------------------------------------------------


  //---------------------------------------------------------------
  // process transceiver rx data
  //---------------------------------------------------------------
  spio_hss_multiplexer_rx_control rx_control (
      .CLK_IN                 (clk)
    , .RESET_IN               (reset)

    , .REG_IDSI_OUT           (idsi_out)

    , .HANDSHAKE_COMPLETE_OUT (handshake_complete_out)
    , .HANDSHAKE_PHASE_OUT    (handshake_phase_int)
    , .VERSION_MISMATCH_OUT   (version_mismatch_out)

      // transceiver rx data interface
    , .RXDATA_IN              (rx_data_in)
    , .RXCHARISCOMMA_IN       (rx_chariscomma_in)
    , .RXLOSSOFSYNC_IN        (loss_of_sync_state_out)
    , .RXCHARISK_IN           (rx_charisk_in)

    // outgoing frame interface
    , .RXDATA_OUT             (rcvd_data_int)
    , .RXCHARISK_OUT          (rcvd_charisk_int)
    , .RXVLD_OUT              (rcvd_vld_int)
    );
  //---------------------------------------------------------------
endmodule
