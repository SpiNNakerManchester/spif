//************************************************//
//*                                              *//
//* spif FPGA interfaces definitions             *//
//*                                              *//
//* lap - 08/09/2021                             *//
//*                                              *//
//************************************************//

#ifndef __SPIF_FPGA_H__
#define __SPIF_FPGA_H__


//---------------------------------------------------------------
// FPGA selection
//NOTE: un-comment the required FPGA #define
//---------------------------------------------------------------
#define TARGET_XC7Z015   // Zynq7 on TE0715 board
//#define TARGET_XCZU9EG   // Zynq Ultrascale+ on zcu102 board
//---------------------------------------------------------------


//---------------------------------------------------------------
// addresses of memory-mapped spif interfaces 
//---------------------------------------------------------------
#ifdef TARGET_XC7Z015

// ---------------------------------
// reserved DDR memory range for DMA buffers 
//NOTE: AXI_HP0 PL -> DDR interface
#define DDR_RES_MEM_ADDR  0x3fffc000
#define DDR_RES_MEM_SIZE  0x4000
#define PIPE_MEM_SIZE     (DDR_RES_MEM_SIZE / 4)
// ---------------------------------

// ---------------------------------
// spif configuration registers
//NOTE: AXI_GP0 PS -> PL interface
#define APB_REGS_ADDR     0x43c20000
#define APB_REGS_SIZE     0x10000
// ---------------------------------

// ---------------------------------
// spif DMA controller configuration
//NOTE: AXI_GP0 PS -> PL interface
#define DMA_REGS_ADDR     0x40400000
#define DMA_REGS_SIZE     0x10000
// ---------------------------------

#endif /* TARGET_XC7Z015 */

#ifdef TARGET_XCZU9EG

// ---------------------------------
// reserved DDR memory range for DMA buffers 
//NOTE: AXI_HP0_FPD PL -> DDR interface
#define DDR_RES_MEM_ADDR  0x7feff000
#define DDR_RES_MEM_SIZE  0x1000
// ---------------------------------

// ---------------------------------
// spif configuration registers
//NOTE: AXI_HPM0_FPD PS -> PL interface
#define APB_REGS_ADDR     0xa0030000
#define APB_REGS_SIZE     0x10000
// ---------------------------------

// ---------------------------------
// spif DMA controller configuration
//NOTE: AXI_HPM0_FPD PS -> PL interface
#define DMA_REGS_ADDR     0xa0000000
#define DMA_REGS_SIZE     0x10000
// ---------------------------------

#endif /* TARGET_XCZU9EG */
//---------------------------------------------------------------


#endif /* __SPIF_FPGA_H__ */
