// -------------------------------------------------------------------------
//  hssl_vio
//
//  DVS input to SpiNN-5 board through High-Speed Serial Link (HSSL)
//  virtual I/O for HSSL interface and GTH block
//
// -------------------------------------------------------------------------
// AUTHOR
//  lap - luis.plana@manchester.ac.uk
// -------------------------------------------------------------------------
// DETAILS
//  Created on       : 21 Oct 2020
//  Last modified on : Wed 21 Oct 16:13:42 BST 2020
//  Last modified by : $Author: plana $
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
module hssl_vio (
  input  wire        clk,

  input  wire  [1:0] probe_in0,
  input  wire  [0:0] probe_in1,
  input  wire  [0:0] probe_in2,
  input  wire  [3:0] probe_in3,
  input  wire  [0:0] probe_in4,
  input  wire  [0:0] probe_in5,
  input  wire  [0:0] probe_in6,
  input  wire  [0:0] probe_in7,
  input  wire  [0:0] probe_in8,
  input  wire  [2:0] probe_in9,
  input  wire [15:0] probe_in10,
  input  wire [15:0] probe_in11,
  input  wire [31:0] probe_in12,
  input  wire [31:0] probe_in13,

  output wire  [0:0] probe_out0,
  output wire  [0:0] probe_out1,
  output wire  [0:0] probe_out2,
  output wire  [0:0] probe_out3,
  output wire  [0:0] probe_out4,
  output wire  [0:0] probe_out5,
  output wire  [2:0] probe_out6
);

  //---------------------------------------------------------------
  // internal signals
  //---------------------------------------------------------------
  genvar i;

  wire  [1:0] probe_in0_sync;
  wire  [0:0] probe_in1_sync;
  wire  [0:0] probe_in2_sync;
  wire  [3:0] probe_in3_sync;
  wire  [0:0] probe_in4_sync;
  wire  [0:0] probe_in5_sync;
  wire  [0:0] probe_in6_sync;
  wire  [0:0] probe_in7_sync;
  wire  [0:0] probe_in8_sync;
  wire  [2:0] probe_in9_sync;
  wire [15:0] probe_in10_sync;
  wire [15:0] probe_in11_sync;
  wire [31:0] probe_in12_sync;
  wire [31:0] probe_in13_sync;
  //---------------------------------------------------------------


  //---------------------------------------------------------------
  // synchronise GTH block probed signals to clk
  //---------------------------------------------------------------
  generate
    for (i = 0; i < 2; i = i + 1)
    begin : probe_in0_synchro
      (* DONT_TOUCH = "TRUE" *)
        bit_synchronizer bit_synchronizer_vio_probe_in0_inst (
          .clk_in (clk),
          .i_in   (probe_in0[i]),
          .o_out  (probe_in0_sync[i])
        );
    end

    (* DONT_TOUCH = "TRUE" *)
    bit_synchronizer bit_synchronizer_probe_in1_inst (
      .clk_in (clk),
      .i_in   (probe_in1[0]),
      .o_out  (probe_in1_sync[0])
      );

    (* DONT_TOUCH = "TRUE" *)
    bit_synchronizer bit_synchronizer_probe_in2_inst (
      .clk_in (clk),
      .i_in   (probe_in2[0]),
      .o_out  (probe_in2_sync[0])
      );

    for (i = 0; i < 4; i = i + 1)
    begin : probe_in3_synchro
      (* DONT_TOUCH = "TRUE" *)
      bit_synchronizer bit_synchronizer_probe_in3_inst (
        .clk_in (clk),
        .i_in   (probe_in3[i]),
        .o_out  (probe_in3_sync[i])
        );
    end

    (* DONT_TOUCH = "TRUE" *)
    bit_synchronizer bit_synchronizer_probe_in4_inst (
      .clk_in (clk),
      .i_in   (probe_in4[0]),
      .o_out  (probe_in4_sync[0])
      );

    (* DONT_TOUCH = "TRUE" *)
    bit_synchronizer bit_synchronizer_probe_in5_inst (
      .clk_in (clk),
      .i_in   (probe_in5[0]),
      .o_out  (probe_in5_sync[0])
      );

    (* DONT_TOUCH = "TRUE" *)
    bit_synchronizer bit_synchronizer_probe_in6_inst (
      .clk_in (clk),
      .i_in   (probe_in6[0]),
      .o_out  (probe_in6_sync[0])
      );

    (* DONT_TOUCH = "TRUE" *)
    bit_synchronizer bit_synchronizer_probe_in7_inst (
      .clk_in (clk),
      .i_in   (probe_in7[0]),
      .o_out  (probe_in7_sync[0])
      );

    (* DONT_TOUCH = "TRUE" *)
    bit_synchronizer bit_synchronizer_probe_in8_inst (
      .clk_in (clk),
      .i_in   (probe_in8[0]),
      .o_out  (probe_in8_sync[0])
      );

    for (i = 0; i < 3; i = i + 1)
    begin : probe_in9_synchro
      (* DONT_TOUCH = "TRUE" *)
      bit_synchronizer bit_synchronizer_probe_in9_inst (
        .clk_in (clk),
        .i_in   (probe_in9[i]),
        .o_out  (probe_in9_sync[i])
        );
    end

    for (i = 0; i < 16; i = i + 1)
    begin : probes_in10_synchro
      (* DONT_TOUCH = "TRUE" *)
      bit_synchronizer bit_synchronizer_probe_in10_inst (
        .clk_in (clk),
        .i_in   (probe_in10[i]),
        .o_out  (probe_in10_sync[i])
        );
    end

    for (i = 0; i < 16; i = i + 1)
    begin : probes_in11_synchro
      (* DONT_TOUCH = "TRUE" *)
      bit_synchronizer bit_synchronizer_probe_in11_inst (
        .clk_in (clk),
        .i_in   (probe_in11[i]),
        .o_out  (probe_in11_sync[i])
        );
    end

    for (i = 0; i < 32; i = i + 1)
    begin : probe_in12_synchro
      (* DONT_TOUCH = "TRUE" *)
      bit_synchronizer bit_synchronizer_probe_in12_inst (
        .clk_in (clk),
        .i_in   (probe_in12[i]),
        .o_out  (probe_in12_sync[i])
        );
    end

    for (i = 0; i < 32; i = i + 1)
    begin : probe_in13_synchro
      (* DONT_TOUCH = "TRUE" *)
      bit_synchronizer bit_synchronizer_probe_in13_inst (
        .clk_in (clk),
        .i_in   (probe_in13[i]),
        .o_out  (probe_in13_sync[i])
      );
    end
  endgenerate
  //---------------------------------------------------------------


  //---------------------------------------------------------------
  // virtual I/O block
  //---------------------------------------------------------------
  vio_0 vio_0_inst (
      .clk        (clk)

    , .probe_in0  (probe_in0_sync)
    , .probe_in1  (probe_in1_sync)
    , .probe_in2  (probe_in2_sync)
    , .probe_in3  (probe_in3_sync)
    , .probe_in4  (probe_in4_sync)
    , .probe_in5  (probe_in5_sync)
    , .probe_in6  (probe_in6_sync)
    , .probe_in7  (probe_in7_sync)
    , .probe_in8  (probe_in8_sync)
    , .probe_in9  (probe_in9_sync)
    , .probe_in10 (probe_in10_sync)
    , .probe_in11 (probe_in11_sync)
    , .probe_in12 (probe_in12_sync)
    , .probe_in13 (probe_in13_sync)

    , .probe_out0 (probe_out0)
    , .probe_out1 (probe_out1)
    , .probe_out2 (probe_out2)
    , .probe_out3 (probe_out3)
    , .probe_out4 (probe_out4)
    , .probe_out5 (probe_out5)
    , .probe_out6 (probe_out6)
    );
  //---------------------------------------------------------------
endmodule
