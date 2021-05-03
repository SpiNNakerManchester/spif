//***********************************************//
//* memory-mapped interface to high-speed link  *//
//*                                             *//
//***********************************************//

#ifndef __HSSL_AXI_IF_H__
#define __HSSL_AXI_IF_H__

// addresses of memory-mapped FPGA interfaces 
// HPM0 PS -> PL interface
#define CDMA_CFG          0xa0000000

// HSSL FIFO configuration interface
#define AXI_LITE          0x43c00000

// HSSL FIFO data interface
#define AXI_FULL          0x43c10000

// HSSL register bank interface
#define APB_BRIDGE        0x43c20000


// CDMA registers
#define CDMACR            0
#define CDMASR            1
#define CDMASA            6
#define CDMASAH           7
#define CDMADA            8
#define CDMADAH           9
#define CDMABT            10

#define CDMA_RESET        0x04
#define CDMA_INIT         0x20
#define CDMA_IDLE_MSK     0x2

#endif
