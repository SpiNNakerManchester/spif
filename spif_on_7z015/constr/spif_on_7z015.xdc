# ----------------------------------------------------------------------------------------------------------------------
# -lap 31/03/2021 
# -- based on UltraScale FPGAs Transceivers Wizard IP example design-level XDC file
# ----------------------------------------------------------------------------------------------------------------------

# Channel primitive location constraint
# ----------------------------------------------------------------------------------------------------------------------
#set_property LOC GTPE2_CHANNEL_X0Y0 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[26].*gen_gthe4_channel_inst[0].GTHE4_CHANNEL_PRIM_INST}]

# Location constraints for the transceiver differential TX and RX links
# ----------------------------------------------------------------------------------------------------------------------
set_property PACKAGE_PIN AB7 [get_ports gt_rxn_in]
set_property PACKAGE_PIN AA7 [get_ports gt_rxp_in]
set_property PACKAGE_PIN AB3 [get_ports gt_txn_out]
set_property PACKAGE_PIN AA3 [get_ports gt_txp_out]

# Location constraints for differential reference clock buffers
# ----------------------------------------------------------------------------------------------------------------------
set_property PACKAGE_PIN V5 [get_ports gt_refclk_n]
set_property PACKAGE_PIN U5 [get_ports gt_refclk_p]

# Clock constraints for clocks provided as inputs to the core
#NOTE: the IP core-level XDC constrains clocks produced by the core.
# ----------------------------------------------------------------------------------------------------------------------
create_clock -period 6.667 -name clk_gt_refclk_p [get_ports gt_refclk_p]

# some design modules are clocked at half-speed: constrain generated clock
create_generated_clock -name tl_freerun_clk_int -source [get_pins slow_clk/I] -edges {1 2 5} [get_pins slow_clk/O]

# make sure that the original clock and the propagated/generated clocks are in the same delay group
set_property CLOCK_DELAY_GROUP hb_clocks [get_nets ps_pl_clk0_int]
set_property CLOCK_DELAY_GROUP hb_clocks [get_nets pl_clk0_buf_int]
set_property CLOCK_DELAY_GROUP hb_clocks [get_nets tl_freerun_clk_int]

# False path constraints
set_false_path -from [get_clocks tl_freerun_clk_int] -to [get_clocks clkout0]
set_false_path -from [get_clocks clkout0] -to [get_clocks tl_freerun_clk_int]

#NOTE: synchronisation flip-flops
# ----------------------------------------------------------------------------------------------------------------------
set_false_path -to [get_cells -hierarchical -filter {NAME =~ *bit_synchronizer*inst/i_in_meta_reg}] -quiet
##set_false_path -to [get_cells -hierarchical -filter {NAME =~ *reset_synchronizer*inst/rst_in_*_reg}] -quiet
set_false_path -to [get_pins -filter REF_PIN_NAME=~*D -of_objects [get_cells -hierarchical -filter {NAME =~ *reset_synchronizer*inst/rst_in_meta*}]] -quiet
set_false_path -to [get_pins -filter REF_PIN_NAME=~*PRE -of_objects [get_cells -hierarchical -filter {NAME =~ *reset_synchronizer*inst/rst_in_meta*}]] -quiet
set_false_path -to [get_pins -filter REF_PIN_NAME=~*PRE -of_objects [get_cells -hierarchical -filter {NAME =~ *reset_synchronizer*inst/rst_in_sync1*}]] -quiet
set_false_path -to [get_pins -filter REF_PIN_NAME=~*PRE -of_objects [get_cells -hierarchical -filter {NAME =~ *reset_synchronizer*inst/rst_in_sync2*}]] -quiet
set_false_path -to [get_pins -filter REF_PIN_NAME=~*PRE -of_objects [get_cells -hierarchical -filter {NAME =~ *reset_synchronizer*inst/rst_in_sync3*}]] -quiet
set_false_path -to [get_pins -filter REF_PIN_NAME=~*PRE -of_objects [get_cells -hierarchical -filter {NAME =~ *reset_synchronizer*inst/rst_in_out*}]] -quiet


set_false_path -to [get_cells -hierarchical -filter {NAME =~ *gtwiz_userclk_tx_inst/*gtwiz_userclk_tx_active_*_reg}] -quiet
set_false_path -to [get_cells -hierarchical -filter {NAME =~ *gtwiz_userclk_rx_inst/*gtwiz_userclk_rx_active_*_reg}] -quiet

# debug hub interfaces to the virtual IO block
# ----------------------------------------------------------------------------------------------------------------------
set_property C_CLK_INPUT_FREQ_HZ 50000000 [get_debug_cores dbg_hub]
set_property C_ENABLE_CLK_DIVIDER false [get_debug_cores dbg_hub]
set_property C_USER_SCAN_CHAIN 1 [get_debug_cores dbg_hub]
connect_debug_port dbg_hub/clk [get_nets tl_freerun_clk_int]
