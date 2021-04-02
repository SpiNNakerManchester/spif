# ----------------------------------------------------------------------------------------------------------------------
# -lap 31/03/2021 
# -- based on UltraScale FPGAs Transceivers Wizard IP example design-level XDC file
# ----------------------------------------------------------------------------------------------------------------------

# Location constraints for differential reference clock buffers
#NOTE: the IP core-level XDC constrains the transceiver channel data pin locations
# ----------------------------------------------------------------------------------------------------------------------
set_property PACKAGE_PIN C7 [get_ports gt_refclk_n]
set_property PACKAGE_PIN C8 [get_ports gt_refclk_p]

# Clock constraints for clocks provided as inputs to the core
#NOTE: the IP core-level XDC constrains clocks produced by the core.
# ----------------------------------------------------------------------------------------------------------------------
create_clock -period 6.734 -name clk_gt_refclk_p [get_ports gt_refclk_p]

# some design modules are clocked at half-speed: constrain generated clock
create_generated_clock -name tl_freerun_clk_int -source [get_pins slow_clk/I] -edges {1 2 5} [get_pins slow_clk/O]

# make sure that the original clock and the propagated/generated clocks are in the same delay group
set_property CLOCK_DELAY_GROUP hb_clocks [get_nets ps_pl_clk0_int]
set_property CLOCK_DELAY_GROUP hb_clocks [get_nets pl_clk0_buf_int]
set_property CLOCK_DELAY_GROUP hb_clocks [get_nets tl_freerun_clk_int]

# False path constraints
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
