// -------------------------------------------------------------------------
//  hssl_transceiver
//
//  DVS input to SpiNN-5 board through High-Speed Serial Link (HSSL)
//  Gigabit transceiver + support modules
//
//  Different FPGAs have different transceiver modules. This module
//  currently supports the following FPGA/Transceiver combinations:
//
//  XC7Z015 / GTP --- Zynq7 on Trenz Electronic TE0715 board
//  XCZU9EG / GTH --- Zynq Ultrascale+ on Xilinx zcu102 board
//
// -------------------------------------------------------------------------
// AUTHOR
//  lap - luis.plana@manchester.ac.uk
// -------------------------------------------------------------------------
// DETAILS
//  Created on       : 28 Mar 2021
//  Last modified on : Sun 28 Mar 17:47:41 BST 2021
//  Last modified by : $Author: plana $
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

`include "dvs_on_hssl_top.h"


`timescale 1ps/1ps
module hssl_transceiver
#(
  parameter TARGET_FPGA = `FPGA_XC7Z015
)
(
  // GT external ports: differential clock and links
  input  wire         refclk_pad_n_in,
  input  wire         refclk_pad_p_in,
  output wire         txn_out,
  output wire         txp_out,
  input  wire         rxn_in,
  input  wire         rxp_in,

  // free-running clock and reset
  input  wire         freerun_clk_in,
  input  wire         reset_all_in,

  // GT block resets and done signals
  output wire         tx_usrclk_active_out,
  input  wire         tx_reset_datapath_in,
  output wire         tx_reset_done_out,
  input  wire         rx_reset_datapath_in,
  output wire         rx_reset_done_out,

  // GT transmitter ports
  output wire         tx_usrclk_out,
  output wire         tx_usrclk2_out,
  input  wire  [31:0] tx_data_in,
  input  wire   [3:0] tx_charisk_in,

  // GT receiver ports
  output wire         rx_usrclk_out,
  output wire         rx_usrclk2_out,
  output wire  [31:0] rx_data_out,
  output wire         rx_commadet_out,
  output wire   [3:0] rx_chariscomma_out,
  output wire   [3:0] rx_charisk_out,
  output wire   [3:0] rx_disperr_out,
  output wire   [3:0] rx_encerr_out,
  output wire         rx_bufstatus_out,

  // GT control and status
  input  wire         tx_elecidle_in,
  input  wire         tx_polarity_in,
  input  wire   [2:0] loopback_in
);

  //---------------------------------------------------------------
  // internal signals
  //---------------------------------------------------------------
  genvar i;
  //---------------------------------------------------------------


  //---------------------------------------------------------------
  // Gigabit transceiver and tx/rx clock blocks
  //NOTE: transceiver model and interface varies with target FPGA
  //---------------------------------------------------------------
  generate
    case (TARGET_FPGA)
      `FPGA_XC7Z015:
        begin
          //---------------------------------------------------------------
          // GTP transceiver + support modules
          //---------------------------------------------------------------
          gtp_x0y0_3Gbs gtp_x0y0_3Gbs_inst (
              .sysclk_in                      (freerun_clk_in)
            , .soft_reset_tx_in               (reset_all_in)
            , .soft_reset_rx_in               (reset_all_in)
            , .dont_reset_on_data_error_in    (1'b0)

            , .q0_clk1_gtrefclk_pad_n_in      (refclk_pad_n_in)
            , .q0_clk1_gtrefclk_pad_p_in      (refclk_pad_p_in)
            , .gt0_tx_mmcm_lock_out           (tx_usrclk_active_out)
            , .gt0_rx_mmcm_lock_out           ()
            , .gt0_tx_fsm_reset_done_out      ()
            , .gt0_rx_fsm_reset_done_out      ()
            , .gt0_data_valid_in              (1'b1)
 
            , .gt0_txusrclk_out               (tx_usrclk_out)
            , .gt0_txusrclk2_out              (tx_usrclk2_out)
            , .gt0_rxusrclk_out               ()
            , .gt0_rxusrclk2_out              ()

            , .gt0_loopback_in                (loopback_in)

            , .gt0_rxclkcorcnt_out            ()

            , .gt0_rxdata_out                 (rx_data_out)

            , .gt0_rxchariscomma_out          (rx_chariscomma_out)
            , .gt0_rxcharisk_out              (rx_charisk_out)
            , .gt0_rxdisperr_out              (rx_disperr_out)
            , .gt0_rxnotintable_out           (rx_encerr_out)

            , .gt0_gtprxn_in                  (rxn_in)
            , .gt0_gtprxp_in                  (rxp_in)

            , .gt0_rxbyteisaligned_out        ()
            , .gt0_rxcommadet_out             (rx_commadet_out)
            , .gt0_rxmcommaalignen_in         (1'b1)
            , .gt0_rxpcommaalignen_in         (1'b1)

            , .gt0_dmonitorout_out            ()

            , .gt0_rxlpmhfhold_in             (1'b0)
            , .gt0_rxlpmlfhold_in             (1'b0)

            , .gt0_rxoutclkfabric_out         ()

            , .gt0_gtrxreset_in               (rx_reset_datapath_in)
            , .gt0_rxlpmreset_in              (1'b0)

            , .gt0_rxresetdone_out            (rx_reset_done_out)

            , .gt0_rxpolarity_in              (1'b0)

            , .gt0_gttxreset_in               (tx_reset_datapath_in)
            , .gt0_txuserrdy_in               (1'b1)

            , .gt0_txdata_in                  (tx_data_in)

            , .gt0_txelecidle_in              (tx_elecidle_in)

            , .gt0_txcharisk_in               (tx_charisk_in)

            , .gt0_gtptxn_out                 (txn_out)
            , .gt0_gtptxp_out                 (txp_out)

            , .gt0_txoutclkfabric_out         ()
            , .gt0_txoutclkpcs_out            ()

            , .gt0_txresetdone_out            (tx_reset_done_out)

            , .gt0_txpolarity_in              (tx_polarity_in)

            , .gt0_eyescanreset_in            (1'b0)
            , .gt0_rxuserrdy_in               (1'b1)

            , .gt0_eyescandataerror_out       ()
            , .gt0_eyescantrigger_in          (1'b0)

            , .gt0_drpaddr_in                 (9'd0)
            , .gt0_drpdi_in                   (16'd0)
            , .gt0_drpdo_out                  ()
            , .gt0_drpen_in                   (1'b0)
            , .gt0_drprdy_out                 ()
            , .gt0_drpwe_in                   (1'b0)

            , .gt0_pll0reset_out              ()
            , .gt0_pll0outclk_out             ()
            , .gt0_pll0outrefclk_out          ()
            , .gt0_pll0lock_out               ()
            , .gt0_pll0refclklost_out         ()
            , .gt0_pll1outclk_out             ()
            , .gt0_pll1outrefclk_out          ()
            );
          //---------------------------------------------------------------
        end

      `FPGA_XCZU9EG:
        begin
          //---------------------------------------------------------------
          // differential reference clock buffer for MGTREFCLK0_X1Y3
          //---------------------------------------------------------------
          wire mgtrefclk0_x1y3_int;

          IBUFDS_GTE4 #(
                .REFCLK_EN_TX_PATH  (1'b0)
              , .REFCLK_HROW_CK_SEL (2'b00)
              , .REFCLK_ICNTL_RX    (2'b00)
            ) 
            IBUFDS_GTE4_MGTREFCLK0_X1Y3_INST (
                .I     (refclk_pad_p_in)
              , .IB    (refclk_pad_n_in)
              , .CEB   (1'b0)
              , .O     (mgtrefclk0_x1y3_int)
              , .ODIV2 ()
            );
          //---------------------------------------------------------------


          //---------------------------------------------------------------
          // reset signals
          //---------------------------------------------------------------
          wire [0:0] gth_txpmaresetdone_int;
          wire [0:0] gth_rxpmaresetdone_int;

          assign gth_userclk_tx_reset_int = ~(&gth_txpmaresetdone_int);
          assign gth_userclk_rx_reset_int = ~(&gth_rxpmaresetdone_int);
          //---------------------------------------------------------------


          //---------------------------------------------------------------
          // transceiver control
          //---------------------------------------------------------------
          wire  [7:0] gth_txctrl2_int = {4'h0, tx_charisk_in};

          wire [15:0] gth_rxctrl0_int;
          wire [15:0] gth_rxctrl1_int;
          wire  [7:0] gth_rxctrl2_int;
          wire  [7:0] gth_rxctrl3_int;

          assign rx_charisk_out     = gth_rxctrl0_int[3:0];
          assign rx_disperr_out     = gth_rxctrl1_int[3:0];
          assign rx_chariscomma_out = gth_rxctrl2_int[3:0];
          assign rx_encerr_out      = gth_rxctrl3_int[3:0];
          //---------------------------------------------------------------


          //---------------------------------------------------------------
          // GTH transceiver + tx and rx user clock helper blocks
          //---------------------------------------------------------------
          gth_x1y11_3Gbs_example_wrapper example_wrapper_inst (
                .gtwiz_reset_clk_freerun_in              (freerun_clk_in)
              , .gtwiz_reset_all_in                      (reset_all_in)

              , .gtrefclk00_in                           (mgtrefclk0_x1y3_int)

              , .qpll0outclk_out                         ()
              , .qpll0outrefclk_out                      ()
              , .gtpowergood_out                         ()

              , .loopback_in                             (loopback_in)

              , .gthrxn_in                               (rxn_in)
              , .gthrxp_in                               (rxp_in)
              , .gthtxn_out                              (txn_out)
              , .gthtxp_out                              (txp_out)

              , .gtwiz_userclk_tx_reset_in               (gth_userclk_tx_reset_int)
              , .gtwiz_userclk_tx_srcclk_out             ()
              , .gtwiz_userclk_tx_usrclk_out             (tx_usrclk_out)
              , .gtwiz_userclk_tx_usrclk2_out            (tx_usrclk2_out)
              , .gtwiz_userclk_tx_active_out             (tx_usrclk_active_out)

              , .gtwiz_userclk_rx_reset_in               (gth_userclk_rx_reset_int)
              , .gtwiz_userclk_rx_srcclk_out             ()
              , .gtwiz_userclk_rx_usrclk_out             (rx_usrclk_out)
              , .gtwiz_userclk_rx_usrclk2_out            (rx_usrclk2_out)
              , .gtwiz_userclk_rx_active_out             ()

              , .gtwiz_reset_tx_pll_and_datapath_in      (1'b0)
              , .gtwiz_reset_tx_datapath_in              (tx_reset_datapath_in)
              , .gtwiz_reset_tx_done_out                 (tx_reset_done_out)
              , .txpmaresetdone_out                      (gth_txpmaresetdone_int)

              , .gtwiz_reset_rx_pll_and_datapath_in      (1'b0)
              , .gtwiz_reset_rx_datapath_in              (rx_reset_datapath_in)
              , .gtwiz_reset_rx_done_out                 (rx_reset_done_out)
              , .gtwiz_reset_rx_cdr_stable_out           ()
              , .rxpmaresetdone_out                      (gth_rxpmaresetdone_int)

              , .gtwiz_userdata_tx_in                    (tx_data_in)
              , .gtwiz_userdata_rx_out                   (rx_data_out)

              , .tx8b10ben_in                            (1'b1)
              , .txctrl0_in                              (16'h0000)
              , .txctrl1_in                              (16'h0000)
              , .txctrl2_in                              (gth_txctrl2_int)

              , .txelecidle_in                           (tx_elecidle_in)

              , .rx8b10ben_in                            (1'b1)
              , .rxbufreset_in                           (1'b0)
              , .rxcommadeten_in                         (1'b1)
              , .rxmcommaalignen_in                      (1'b1)
              , .rxpcommaalignen_in                      (1'b1)
              , .rxbufstatus_out                         (rx_bufstatus_out)
              , .rxbyteisaligned_out                     ()
              , .rxbyterealign_out                       ()
              , .rxclkcorcnt_out                         ()
              , .rxcommadet_out                          (rx_commadet_out)
              , .rxctrl0_out                             (gth_rxctrl0_int)
              , .rxctrl1_out                             (gth_rxctrl1_int)
              , .rxctrl2_out                             (gth_rxctrl2_int)
              , .rxctrl3_out                             (gth_rxctrl3_int)
            );
          //---------------------------------------------------------------
        end
    endcase
  endgenerate

  //---------------------------------------------------------------
endmodule
