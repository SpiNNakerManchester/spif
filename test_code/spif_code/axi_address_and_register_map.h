//***********************************************//
//* memory-mapped interface to high-speed link  *//
//*                                             *//
//***********************************************//

#ifndef __AXI_ADDR_MAP_H__
#define __AXI_ADDR_MAP_H__

//---------------------------------------------------------------
// FPGA selection
//NOTE: un-comment the required FPGA #define
//---------------------------------------------------------------
#define TARGET_XC7Z015   // Zynq7 on TE0715 board
//#define TARGET_XCZU9EG   // Zynq Ultrascale+ on zcu102 board
//---------------------------------------------------------------

//---------------------------------------------------------------
// addresses of memory-mapped FPGA interfaces
//---------------------------------------------------------------
#ifdef TARGET_XC7Z015

// SPIF_OUT FIFO configuration interface
#define AXI_LITE          0x43c00000

// SPIF_OUT FIFO data interface
#define AXI_FULL          0x43c10000

// SPIF register bank interface
#define APB_BRIDGE        0x43c20000

#endif /* TARGET_XC7Z015 */

#ifdef TARGET_XCZU9EG

// SPIF OUT FIFO configuration interface
#define AXI_LITE          0xa0010000

// SPIF OUT FIFO data interface
#define AXI_FULL          0xa0020000

// SPIF register bank interface
#define APB_BRIDGE        0xa0030000

#endif /* TARGET_XCZU9EG */
//---------------------------------------------------------------


//---------------------------------------------------------------
// SPIF_OUT FIFO (SOF) control and status registers
//---------------------------------------------------------------
#define SOF_ISR 0      // interrupt status register
#define SOF_IER 1      // interrupt enable register
#define SOF_TFV 3      // transmitter FIFO vacancy register
#define SOF_LEN 5      // transmitter length register
#define SOF_RFO 7      // receiver FIFO occupancy register
//---------------------------------------------------------------


//---------------------------------------------------------------
// SPIF_OUT FIFO (SOF) data ports
//---------------------------------------------------------------
#define SOF_DWR 0      // data write port
//---------------------------------------------------------------

#endif /* __AXI_ADDR_MAP_H__ */