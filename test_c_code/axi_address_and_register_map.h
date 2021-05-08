//***********************************************//
//* memory-mapped interface to high-speed link  *//
//*                                             *//
//***********************************************//

#ifndef __AXI_ADDR_MAP_H__
#define __AXI_ADDR_MAP_H__

// addresses of memory-mapped FPGA interfaces 
// SPIF_OUT FIFO configuration interface
#define AXI_LITE          0x43c00000

// SPIF_OUT FIFO data interface
#define AXI_FULL          0x43c10000

// SPIF register bank interface
#define APB_BRIDGE        0x43c20000

#endif
