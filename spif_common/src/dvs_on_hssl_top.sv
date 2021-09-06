// -------------------------------------------------------------------------
//  dvs_on_hssl_top
//
//  DVS input to SpiNN-5 board through High-Speed Serial Link (HSSL)
//  top-level module: processor subsystem + HSSL interface +
//                    Gigabit transceiver + virtual I/O +
//                    clocks and resets
//
// -------------------------------------------------------------------------
// AUTHOR
//  lap - luis.plana@manchester.ac.uk
// -------------------------------------------------------------------------
// DETAILS
//  Created on       : 21 Oct 2020
//  Last modified on : Thu 15 Jul 18:56:59 BST 2021
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
`include "dvs_on_hssl_top.h"
`include "hssl_reg_bank.h"


`timescale 1ps/1ps
module dvs_on_hssl_top
(
  // differential reference clock inputs
  input  wire gt_refclk_p,
  input  wire gt_refclk_n,

`ifdef TARGET_XC7Z015
  // unused exported ports on FPGA 7z015
  inout  wire [14:0] DDR_addr,
  inout  wire  [2:0] DDR_ba,
  inout  wire        DDR_cas_n,
  inout  wire        DDR_ck_n,
  inout  wire        DDR_ck_p,
  inout  wire        DDR_cke,
  inout  wire        DDR_cs_n,
  inout  wire  [3:0] DDR_dm,
  inout  wire [31:0] DDR_dq,
  inout  wire  [3:0] DDR_dqs_n,
  inout  wire  [3:0] DDR_dqs_p,
  inout  wire        DDR_odt,
  inout  wire        DDR_ras_n,
  inout  wire        DDR_reset_n,
  inout  wire        DDR_we_n,
  inout  wire        FIXED_IO_ddr_vrn,
  inout  wire        FIXED_IO_ddr_vrp,
  inout  wire [53:0] FIXED_IO_mio,
  inout  wire        FIXED_IO_ps_clk,
  inout  wire        FIXED_IO_ps_porb,
  inout  wire        FIXED_IO_ps_srstb,
`endif

  // transceiver HSSL data ports
  input  wire gt_rxn_in,
  input  wire gt_rxp_in,
  output wire gt_txn_out,
  output wire gt_txp_out
);

  
  // use local parameters for consistent definitions
  localparam HW_VERSION     = `SPIF_VER_NUM;
  localparam HW_NUM_PIPES   = `SPIF_NUM_PIPES;
  localparam TARGET_FPGA    = `FPGA_MODEL;
  localparam HW_SNTL_BITS   = `HW_SNTL_BITS;

  localparam PACKET_BITS    = `PKT_BITS;
  localparam NUM_CHANNELS   = `NUM_CHANS;

  localparam RADDR_BITS     = `RADDR_BITS;
  localparam NUM_HREGS      = `NUM_HREGS;
  localparam NUM_RREGS      = `NUM_RREGS;
  localparam NUM_CREGS      = `NUM_CREGS;
  localparam NUM_MREGS_PIPE = `NUM_MREGS_PIPE;
  localparam NUM_MREGS      = `NUM_MREGS;

  localparam MSFT_BITS      = `MSFT_BITS;
  localparam RRTE_BITS      = `RRTE_BITS;


  //---------------------------------------------------------------
  // internal signals
  //---------------------------------------------------------------
  // clocks and resets
  wire        tl_freerun_clk_int;
  wire        tl_reset_all_int;

  // processor subsystem signals
  wire        ps_peripheral_reset_0_int;
  wire        ps_pl_clk0_int;

  wire        axi_clk_int;
  wire        axi_resetn_int;

  wire [39:0] apb_paddr_int;
  wire        apb_penable_int;
  wire [31:0] apb_prdata_int;
  wire        apb_pready_int;
  wire        apb_psel_int;
  wire        apb_pslverr_int;
  wire [31:0] apb_pwdata_int;
  wire        apb_pwrite_int;

  wire [31:0] evt_data_int [HW_NUM_PIPES - 1:0];
  wire        evt_vld_int  [HW_NUM_PIPES - 1:0];
  wire        evt_rdy_int  [HW_NUM_PIPES - 1:0];

  // register bank signals
  //  - HSSL interface control
  wire        hi_stop_int;

  // - packet counters
  wire [31:0] pkt_ctr_int [NUM_CREGS - 1:0];
  wire [31:0] pkt_reply_key_int;

  //  - wait values for packet dropping
  wire [31:0] rtr_drop_wait_int;
  wire [31:0] prx_drop_wait_int;

  //  - packet routing table
  wire             [31:0] rt_key_int   [NUM_RREGS - 1:0];
  wire             [31:0] rt_mask_int  [NUM_RREGS - 1:0];
  wire  [RRTE_BITS - 1:0] rt_route_int [NUM_RREGS - 1:0];

  //  - event mapper registers
  wire             [31:0] mp_key_int  [HW_NUM_PIPES - 1:0];
  wire             [31:0] mp_fmsk_int [NUM_MREGS - 1:0];
  wire  [MSFT_BITS - 1:0] mp_fsft_int [NUM_MREGS - 1:0];

  // - packet receiver interface
  wire  [RADDR_BITS - 1:0] prx_addr_int;
  wire              [31:0] prx_wdata_int;
  wire                     prx_en_int;

  //  - diagnostic counter signals
  wire [NUM_CREGS - 1:0] ctr_cnt_int;
  wire             [1:0] prx_cnt_int;
  wire             [1:0] rt_cnt_int;

  // HSSL interface signals
  wire                      hi_clk_int;
  wire                      hi_reset_int;
  wire                [1:0] hi_loss_of_sync_int;
  wire                      hi_handshake_complete_int;
  wire                      hi_version_mismatch_int;
  wire [HW_SNTL_BITS - 1:0] hi_idsi_int;

  // Gigabit transceiver signals
  wire        gt_freerun_clk_int;
  wire        gt_reset_all_int;

  wire        gt_tx_usrclk_active_int;
  wire        gt_tx_reset_datapath_int;
  wire        gt_tx_reset_done_int;

  wire        gt_rx_reset_datapath_int;
  wire        gt_rx_reset_done_int;

  wire        gt_tx_usrclk_int;
  wire        gt_tx_usrclk2_int;
  wire [31:0] gt_tx_data_int;
  wire  [3:0] gt_tx_charisk_int;

  wire        gt_rx_usrclk_int;
  wire        gt_rx_usrclk2_int;
  wire [31:0] gt_rx_data_int;
  wire        gt_rx_commadet_int;
  wire  [3:0] gt_rx_charisk_int;
  wire  [3:0] gt_rx_disperr_int;
  wire  [3:0] gt_rx_chariscomma_int;
  wire  [3:0] gt_rx_encerr_int;
  wire        gt_rx_bufstatus_int;

  // Virtual I/O signals
  wire        vio_freerun_clk_int;
  wire        vio_reset_all_int;
  wire        vio_tx_reset_datapath_int;
  wire        vio_rx_reset_datapath_int;
  wire  [2:0] vio_loopback_int;
  //---------------------------------------------------------------


  //---------------------------------------------------------------
  // HSSL interface and transceiver clocks
  // free-running clock for the transceiver block derived from the
  // PL clock provided by the processor subsystem.
  //NOTE: (/2) avoids reconfiguration of the processor subsystem
  //PL clock
  //---------------------------------------------------------------
  wire pl_clk0_buf_int;

  reg  clk_enable_int = 1'b0;

  //NOTE: this buffer may be redundant - used only for clk_enable_int
  BUFG fast_clk (
        .I (ps_pl_clk0_int)
      , .O (pl_clk0_buf_int)
    );

  // toggle CE every clock cycle to divide p2pl_clk frequency by 2
  always @(posedge pl_clk0_buf_int)
    clk_enable_int <= ~clk_enable_int;

  //NOTE: tl_freerun_clk_int has 25% duty cycle / same pulse width as ps_pl_clk0_int
  BUFGCE slow_clk (
        .I  (ps_pl_clk0_int)
      , .CE (clk_enable_int)
      , .O  (tl_freerun_clk_int)
    );

  assign gt_freerun_clk_int = tl_freerun_clk_int;

  assign vio_freerun_clk_int = tl_freerun_clk_int;

  assign hi_clk_int  = gt_tx_usrclk2_int;

  assign axi_clk_int = gt_tx_usrclk2_int;
  //---------------------------------------------------------------


  //---------------------------------------------------------------
  // generate/buffer reset signals
  //---------------------------------------------------------------
  // buffer ps_peripheral_reset_0_int
  wire peripheral_reset_0_buf_int;
  IBUF ps_peripheral_reset_0_int_buffer (
        .I (ps_peripheral_reset_0_int)
      , .O (peripheral_reset_0_buf_int)
    );

  // global and function-specific resets
  assign tl_reset_all_int = peripheral_reset_0_buf_int || vio_reset_all_int;

  assign gt_reset_all_int = tl_reset_all_int;

  assign gt_tx_reset_datapath_int = vio_tx_reset_datapath_int;

  assign gt_rx_reset_datapath_int = vio_rx_reset_datapath_int;

  assign hi_reset_int = !gt_tx_reset_done_int || !gt_tx_usrclk_active_int;

  assign axi_resetn_int = gt_tx_usrclk_active_int;
  //---------------------------------------------------------------


  //---------------------------------------------------------------
  // processor subsystem -
  // implements an AXI4-stream interface to the HSSL and
  // provides the free-running clock and the reset signal
  //---------------------------------------------------------------
  wire [31:0] axi_evt_data_int [HW_NUM_PIPES - 1:0];
  wire        axi_evt_vld_int  [HW_NUM_PIPES - 1:0];
  wire        axi_evt_rdy_int  [HW_NUM_PIPES - 1:0];

  proc_sys ps (
      .peripheral_reset_0       (ps_peripheral_reset_0_int)
    , .pl_clk0_0                (ps_pl_clk0_int)

      // AXI interface clock and reset generated from transceiver block
    , .s_axi_aresetn_0          (axi_resetn_int)
    , .s_axi_aclk_0             (axi_clk_int)

      // APB interface to register bank
    , .APB_M_0_paddr            (apb_paddr_int)
    , .APB_M_0_penable          (apb_penable_int)
    , .APB_M_0_prdata           (apb_prdata_int)
    , .APB_M_0_pready           (apb_pready_int)
    , .APB_M_0_psel             (apb_psel_int)
    , .APB_M_0_pslverr          (apb_pslverr_int)
    , .APB_M_0_pwdata           (apb_pwdata_int)
    , .APB_M_0_pwrite           (apb_pwrite_int)

      // AXI stream interface to first event processing pipe
    , .AXI_STR_TXD_0_tdata      (axi_evt_data_int[0])
    , .AXI_STR_TXD_0_tkeep      ()
    , .AXI_STR_TXD_0_tlast      ()
    , .AXI_STR_TXD_0_tvalid     (axi_evt_vld_int[0])
    , .AXI_STR_TXD_0_tready     (axi_evt_rdy_int[0])
    , .mm2s_prmry_reset_out_n_0 ()

`ifdef PIPE1
      // AXI stream interface to second event processing pipe
    , .AXI_STR_TXD_1_tdata      (axi_evt_data_int[1])
    , .AXI_STR_TXD_1_tkeep      ()
    , .AXI_STR_TXD_1_tlast      ()
    , .AXI_STR_TXD_1_tvalid     (axi_evt_vld_int[1])
    , .AXI_STR_TXD_1_tready     (axi_evt_rdy_int[1])
    , .mm2s_prmry_reset_out_n_1 ()
`endif

`ifdef PIPE2
      // AXI stream interface to third event processing pipe
    , .AXI_STR_TXD_2_tdata      (axi_evt_data_int[2])
    , .AXI_STR_TXD_2_tkeep      ()
    , .AXI_STR_TXD_2_tlast      ()
    , .AXI_STR_TXD_2_tvalid     (axi_evt_vld_int[2])
    , .AXI_STR_TXD_2_tready     (axi_evt_rdy_int[2])
    , .mm2s_prmry_reset_out_n_2 ()
`endif

`ifdef PIPE3
      // AXI stream interface to fourth event processing pipe
    , .AXI_STR_TXD_3_tdata      (axi_evt_data_int[3])
    , .AXI_STR_TXD_3_tkeep      ()
    , .AXI_STR_TXD_3_tlast      ()
    , .AXI_STR_TXD_3_tvalid     (axi_evt_vld_int[3])
    , .AXI_STR_TXD_3_tready     (axi_evt_rdy_int[3])
    , .mm2s_prmry_reset_out_n_3 ()
`endif

`ifdef TARGET_XC7Z015
    // unused exported ports on 7z015
    , .DDR_addr                 (DDR_addr)
    , .DDR_ba                   (DDR_ba)
    , .DDR_cas_n                (DDR_cas_n)
    , .DDR_ck_n                 (DDR_ck_n)
    , .DDR_ck_p                 (DDR_ck_p)
    , .DDR_cke                  (DDR_cke)
    , .DDR_cs_n                 (DDR_cs_n)
    , .DDR_dm                   (DDR_dm)
    , .DDR_dq                   (DDR_dq)
    , .DDR_dqs_n                (DDR_dqs_n)
    , .DDR_dqs_p                (DDR_dqs_p)
    , .DDR_odt                  (DDR_odt)
    , .DDR_ras_n                (DDR_ras_n)
    , .DDR_reset_n              (DDR_reset_n)
    , .DDR_we_n                 (DDR_we_n)
    , .FIXED_IO_ddr_vrn         (FIXED_IO_ddr_vrn)
    , .FIXED_IO_ddr_vrp         (FIXED_IO_ddr_vrp)
    , .FIXED_IO_mio             (FIXED_IO_mio)
    , .FIXED_IO_ps_clk          (FIXED_IO_ps_clk)
    , .FIXED_IO_ps_porb         (FIXED_IO_ps_porb)
    , .FIXED_IO_ps_srstb        (FIXED_IO_ps_srstb)
`endif
    );

  // avoid AXI deadlock - drop incoming events if HSSL interface not ready
  genvar pipe;
  generate
    begin
      for (pipe = 0; pipe < HW_NUM_PIPES; pipe = pipe + 1)
        begin
          assign evt_data_int[pipe]    = axi_evt_data_int[pipe];
          assign evt_vld_int[pipe]     = axi_evt_vld_int[pipe] && hi_handshake_complete_int;
          assign axi_evt_rdy_int[pipe] = evt_rdy_int[pipe] || !hi_handshake_complete_int;
        end
    end
  endgenerate
  //---------------------------------------------------------------


  //---------------------------------------------------------------
  // register bank (APB peripheral)
  //---------------------------------------------------------------
  // assemble counter enable signals together
  assign ctr_cnt_int[0] = prx_cnt_int[0];  // peripheral output pkts
  assign ctr_cnt_int[1] = prx_cnt_int[1];  // config pkts
  assign ctr_cnt_int[2] = rt_cnt_int[0];   // dropped input pkts
  assign ctr_cnt_int[3] = rt_cnt_int[1];   // peripheral input pkts

  hssl_reg_bank
  #(
      .HW_NUM_PIPES    (HW_NUM_PIPES)
    )
  rb (
      .clk              (axi_clk_int)
    , .resetn           (axi_resetn_int)

      // processor APB interface
    , .apb_psel_in      (apb_psel_int)
    , .apb_pwrite_in    (apb_pwrite_int)
    , .apb_penable_in   (apb_penable_int)

    , .apb_paddr_in     (apb_paddr_int)
    , .apb_pwdata_in    (apb_pwdata_int)
    , .apb_prdata_out   (apb_prdata_int)

    , .apb_pready_out   (apb_pready_int)
    , .apb_pslverr_out  (apb_pslverr_int)

      // packet receiver interface
    , .prx_addr_in      (prx_addr_int)
    , .prx_wdata_in     (prx_wdata_int)
    , .prx_en_in        (prx_en_int)

    , .ctr_cnt_in       (ctr_cnt_int)

      // status signals
    , .hw_version_in    (HW_VERSION)
    , .hw_pipe_num_in   (HW_NUM_PIPES)
    , .fpga_model_in    (TARGET_FPGA)
    , .hs_complete_in   (hi_handshake_complete_int)
    , .hs_mismatch_in   (hi_version_mismatch_int)
    , .idsi_in          (hi_idsi_int)

      // hssl
    , .hssl_stop_out    (hi_stop_int)

      // packet counters
    , .reg_ctr_out      (pkt_ctr_int)
    , .reply_key_out    (pkt_reply_key_int)

    , .input_wait_out   (rtr_drop_wait_int)
    , .output_wait_out  (prx_drop_wait_int)

      // input router
    , .reg_rt_key_out   (rt_key_int)
    , .reg_rt_mask_out  (rt_mask_int)
    , .reg_rt_route_out (rt_route_int)

      // event mapper
    , .reg_mp_key_out   (mp_key_int)
    , .reg_mp_fmsk_out  (mp_fmsk_int)
    , .reg_mp_fsft_out  (mp_fsft_int)
    );
  //---------------------------------------------------------------


  //---------------------------------------------------------------
  // event pipes map incoming events into SpiNNaker packets
  //---------------------------------------------------------------
  wire [PACKET_BITS - 1:0] pipe_data_int [HW_NUM_PIPES - 1:0];
  wire                     pipe_vld_int  [HW_NUM_PIPES - 1:0];
  wire                     pipe_rdy_int  [HW_NUM_PIPES - 1:0];

  wire [PACKET_BITS - 1:0] pkt_data_int;
  wire                     pkt_vld_int;
  wire                     pkt_rdy_int;

  generate
    begin
      for (pipe = 0; pipe < HW_NUM_PIPES; pipe = pipe + 1)
        begin : evt_pipe
          pkt_assembler pa (
              .clk                (hi_clk_int)
            , .reset              (hi_reset_int)

              // event mapper registers
            , .mp_key_in          (mp_key_int[pipe])
            , .field_msk_in       (mp_fmsk_int[(NUM_MREGS_PIPE * pipe) +: NUM_MREGS_PIPE])
            , .field_sft_in       (mp_fsft_int[(NUM_MREGS_PIPE * pipe) +: NUM_MREGS_PIPE])

              // incoming event
            , .evt_data_in        (evt_data_int[pipe])
            , .evt_vld_in         (evt_vld_int[pipe])
            , .evt_rdy_out        (evt_rdy_int[pipe])

              // assembled packet to be routed
            , .pkt_data_out       (pipe_data_int[pipe])
            , .pkt_vld_out        (pipe_vld_int[pipe])
            , .pkt_rdy_in         (pipe_rdy_int[pipe])
            );
        end

        if (HW_NUM_PIPES == 1)
          begin
            // connect pipe0 output directly to router
            assign pkt_data_int    = pipe_data_int[0];
            assign pkt_vld_int     = pipe_vld_int[0];
            assign pipe_rdy_int[0] = pkt_rdy_int;

            // avoid deadlock on all other AXI streams
            //NOTE: drop all events that arrive on those interfaces
            for (pipe = 1; pipe < HW_NUM_PIPES; pipe = pipe + 1)
              begin
                assign evt_rdy_int[pipe] = 1'b1;
              end
          end
        else
          begin
            // arbitrate between pipe outputs
            spio_rr_arbiter
            #(
                .PKT_BITS (PACKET_BITS)
              )
            arb (
                .CLK_IN    (hi_clk_int)
              , .RESET_IN  (hi_reset_int)

                // pipe0 packets
              , .DATA0_IN  (pipe_data_int[0])
              , .VLD0_IN   (pipe_vld_int[0])
              , .RDY0_OUT  (pipe_rdy_int[0])

                // pipe1 packets
              , .DATA1_IN  (pipe_data_int[1])
              , .VLD1_IN   (pipe_vld_int[1])
              , .RDY1_OUT  (pipe_rdy_int[1])

                // packets to router
              , .DATA_OUT  (pkt_data_int)
              , .VLD_OUT   (pkt_vld_int)
              , .RDY_IN    (pkt_rdy_int)
              );
          end
    end
  endgenerate   
  //---------------------------------------------------------------


  //---------------------------------------------------------------
  // route packets to HSSL channels
  //---------------------------------------------------------------
  wire [PACKET_BITS - 1:0] rtr_data_int [NUM_CHANNELS - 1:0];
  wire                     rtr_vld_int  [NUM_CHANNELS - 1:0];
  wire                     rtr_rdy_int  [NUM_CHANNELS - 1:0];

  pkt_router pr (
      .clk                (hi_clk_int)
    , .reset              (hi_reset_int)

      // wait value for packet drop
    , .drop_wait_in       (rtr_drop_wait_int)

      // routing table data from register bank
    , .reg_key_in         (rt_key_int)
    , .reg_mask_in        (rt_mask_int)
    , .reg_route_in       (rt_route_int)

      // assembled packet
    , .pkt_in_data_in     (pkt_data_int)
    , .pkt_in_vld_in      (pkt_vld_int)
    , .pkt_in_rdy_out     (pkt_rdy_int)

      // outgoing packets
    , .pkt_out_data_out   (rtr_data_int)
    , .pkt_out_vld_out    (rtr_vld_int)
    , .pkt_out_rdy_in     (rtr_rdy_int)

    // packet counter
    , .rt_cnt_out         (rt_cnt_int)
    );
  //---------------------------------------------------------------


  //---------------------------------------------------------------
  // merge diagnostics read replies with channel0 routed packets
  //---------------------------------------------------------------
  wire [PACKET_BITS - 1:0] dcp_data_int;
  wire                     dcp_vld_int;
  wire                     dcp_rdy_int; 

  wire [PACKET_BITS - 1:0] txp_data_int [NUM_CHANNELS - 1:0];
  wire                     txp_vld_int  [NUM_CHANNELS - 1:0];
  wire                     txp_rdy_int  [NUM_CHANNELS - 1:0];

  // channel0 needs to arbitrate between routed and diagnostics packets
  spio_rr_arbiter
  #(
        .PKT_BITS (PACKET_BITS)
      )
  arb (
        .CLK_IN    (hi_clk_int)
      , .RESET_IN  (hi_reset_int)

        // routed packets
      , .DATA0_IN  (rtr_data_int[0])
      , .VLD0_IN   (rtr_vld_int[0])
      , .RDY0_OUT  (rtr_rdy_int[0])

        // diagnostics packets
      , .DATA1_IN  (dcp_data_int)
      , .VLD1_IN   (dcp_vld_int)
      , .RDY1_OUT  (dcp_rdy_int)

        // packets to channel 0
      , .DATA_OUT  (txp_data_int[0])
      , .VLD_OUT   (txp_vld_int[0])
      , .RDY_IN    (txp_rdy_int[0])
      );

  // channels 1 to (NUM_CHANNELS - 1):
  // no need for arbitration
  genvar chan;
  generate
    for (chan = 1; chan < NUM_CHANNELS; chan = chan + 1)
      begin
        assign txp_data_int[chan] = rtr_data_int[chan];
        assign txp_vld_int[chan]  = rtr_vld_int[chan];
        assign rtr_rdy_int[chan]  = txp_rdy_int[chan];
      end
  endgenerate
  //---------------------------------------------------------------


  //---------------------------------------------------------------
  // process packets arriving from the Gigabit receiver
  // steer to configuration block or peripheral output
  //TODO: peripheral output not yet implemented
  //---------------------------------------------------------------
  // channel 0 receiver is the only one active
  wire [PACKET_BITS - 1:0] rxp_data_int;
  wire                     rxp_vld_int;
  wire                     rxp_rdy_int; 

  // processes packets received from the HSSL channel
  pkt_receiver prx (
      .clk                (hi_clk_int)
    , .reset              (hi_reset_int)

      // incoming packets from transceiver
    , .pkt_data_in        (rxp_data_int)
    , .pkt_vld_in         (rxp_vld_int)
    , .pkt_rdy_out        (rxp_rdy_int)

    // packet counters
    , .reg_ctr_in         (pkt_ctr_int)
    , .reply_key_in       (pkt_reply_key_int)

      // register bank interface
    , .prx_addr_out       (prx_addr_int)
    , .prx_wdata_out      (prx_wdata_int)
    , .prx_en_out         (prx_en_int)

    // diagnostic counter reply packet
    , .dcp_data_out       (dcp_data_int)
    , .dcp_vld_out        (dcp_vld_int)
    , .dcp_rdy_in         (dcp_rdy_int)

    // peripheral output packet
    //NOTE: currently always ready to avoid backpressure!
    //TODO: implement peripheral output packet processing
    , .per_data_out       ()
    , .per_vld_out        ()
    , .per_rdy_in         (1'b1)

      // packet counters
    , .prx_cnt_out        (prx_cnt_int)
    );
  //---------------------------------------------------------------


  //---------------------------------------------------------------
  // HSSL interface
  //---------------------------------------------------------------
  hssl_interface hi (
      .clk                            (hi_clk_int)
    , .reset                          (hi_reset_int)

      // routed packets to be sent to SpiNNaker
    , .txpkt_data_in                  (txp_data_int)
    , .txpkt_vld_in                   (txp_vld_int)
    , .txpkt_rdy_out                  (txp_rdy_int)

      // packets received from SpiNNaker
    , .rxpkt_data_out                 (rxp_data_int)
    , .rxpkt_vld_out                  (rxp_vld_int)
    , .rxpkt_rdy_in                   (rxp_rdy_int)

      // interface status and control
    , .loss_of_sync_state_out         (hi_loss_of_sync_int)
    , .handshake_complete_out         (hi_handshake_complete_int)
    , .version_mismatch_out           (hi_version_mismatch_int)

    , .idsi_out                       (hi_idsi_int)
    , .stop_in                        (hi_stop_int)

      // Gigabit transmitter
    , .tx_data_out                    (gt_tx_data_int)
    , .tx_charisk_out                 (gt_tx_charisk_int)

      // Gigabit receiver
    , .rx_data_in                     (gt_rx_data_int)
    , .rx_commadet_in                 (gt_rx_commadet_int)
    , .rx_charisk_in                  (gt_rx_charisk_int)
    , .rx_disperr_in                  (gt_rx_disperr_int)
    , .rx_chariscomma_in              (gt_rx_chariscomma_int)
    , .rx_encerr_in                   (gt_rx_encerr_int)
    );
  //---------------------------------------------------------------


  //---------------------------------------------------------------
  // Gigabit transceiver and tx/rx clock modules
  //---------------------------------------------------------------
  hssl_transceiver
  # (
      .TARGET_FPGA                  (TARGET_FPGA)
    )
  gt (
      // GT external ports: differential clock and links
      .refclk_pad_n_in              (gt_refclk_n)
    , .refclk_pad_p_in              (gt_refclk_p)

    , .txn_out                      (gt_txn_out)
    , .txp_out                      (gt_txp_out)
    , .rxn_in                       (gt_rxn_in)
    , .rxp_in                       (gt_rxp_in)

      // free-running clock and reset
    , .freerun_clk_in               (gt_freerun_clk_int)
    , .reset_all_in                 (gt_reset_all_int)

      // block resets and done ports
    , .tx_usrclk_active_out         (gt_tx_usrclk_active_int)
    , .tx_reset_datapath_in         (gt_tx_reset_datapath_int)
    , .tx_reset_done_out            (gt_tx_reset_done_int)
    , .rx_reset_datapath_in         (gt_rx_reset_datapath_int)
    , .rx_reset_done_out            (gt_rx_reset_done_int)

      // GT transmitter ports
    , .tx_usrclk_out                (gt_tx_usrclk_int)
    , .tx_usrclk2_out               (gt_tx_usrclk2_int)
    , .tx_data_in                   (gt_tx_data_int)
    , .tx_charisk_in                (gt_tx_charisk_int)

      // GT receiver ports
    , .rx_usrclk_out                (gt_rx_usrclk_int)
    , .rx_usrclk2_out               (gt_rx_usrclk2_int)
    , .rx_data_out                  (gt_rx_data_int)
    , .rx_commadet_out              (gt_rx_commadet_int)
    , .rx_chariscomma_out           (gt_rx_chariscomma_int)
    , .rx_charisk_out               (gt_rx_charisk_int)
    , .rx_disperr_out               (gt_rx_disperr_int)
    , .rx_encerr_out                (gt_rx_encerr_int)
    , .rx_bufstatus_out             (gt_rx_bufstatus_int)

      // GT control and status ports
    , .loopback_in                  (vio_loopback_int)
    , .handshake_complete_in        (hi_handshake_complete_int)
    );
  //---------------------------------------------------------------


  //---------------------------------------------------------------
  // virtual I/O for HSSL interface and Gigabit transceiver
  //---------------------------------------------------------------
  hssl_vio vio (
      .clk              (vio_freerun_clk_int)

      // HSSL interface probes
    , .probe_in0        (hi_loss_of_sync_int)
    , .probe_in1        (hi_handshake_complete_int)
    , .probe_in2        (hi_version_mismatch_int)
    , .probe_in3        ()

      // transceiver probes
    , .probe_in4        ()
    , .probe_in5        ()
    , .probe_in6        ()
    , .probe_in7        (gt_tx_reset_done_int)
    , .probe_in8        (gt_rx_reset_done_int)
    , .probe_in9        (gt_rx_bufstatus_int)
    , .probe_in10       (gt_rx_charisk_int)
    , .probe_in11       (gt_rx_chariscomma_int)
    , .probe_in12       (gt_rx_data_int)
    , .probe_in13       (gt_tx_data_int)

      // virtual control signals
    , .probe_out0       (vio_reset_all_int)
    , .probe_out1       ()
    , .probe_out2       (vio_tx_reset_datapath_int)
    , .probe_out3       ()
    , .probe_out4       (vio_rx_reset_datapath_int)
    , .probe_out5       ()
    , .probe_out6       (vio_loopback_int)
    );
  //---------------------------------------------------------------
endmodule
