//************************************************//
//*                                              *//
//* functions to interact with the spif          *//
//* DMA controller                               *//
//*                                              *//
//* lap - 21/06/2021                             *//
//*                                              *//
//************************************************//

#ifndef __DMA_SPIF_IF_H__
#define __DMA_SPIF_IF_H__

#include <sys/mman.h>
#include <fcntl.h>


//---------------------------------------------------------------
// FPGA selection
//NOTE: un-comment the required FPGA #define
//---------------------------------------------------------------
#define TARGET_XC7Z015   // Zynq7 on TE0715 board
//#define TARGET_XCZU9EG   // Zynq Ultrascale+ on zcu102 board
//---------------------------------------------------------------

// addresses of memory-mapped FPGA interfaces 

#ifdef TARGET_XC7Z015

// ---------------------------------
// AXI_HP0 PL -> DDR interface

// reserved DDR range for DMA buffers 
#define DDR_RES_MEM_ADDR  0x3ffff000
#define DDR_RES_MEM_SIZE  0x1000
// ---------------------------------

// ---------------------------------
// AXI_GP0 PS -> PL interface

// DMA controller configuration
#define DMA_REGS_ADDR     0x40400000
#define DMA_REGS_SIZE     0x10000
// ---------------------------------

#endif /* TARGET_XC7Z015 */

#ifdef TARGET_XCZU9EG

// ---------------------------------
// AXI_HP0_FPD PL -> DDR interface

// reserved DDR range for DMA buffers 
#define DDR_RES_MEM_ADDR  0x7feff000
#define DDR_RES_MEM_SIZE  0x1000
// ---------------------------------

// ---------------------------------
// AXI_HPM0_FPD PS -> PL interface

// DMA controller configuration
#define DMA_REGS_ADDR     0xa0000000
#define DMA_REGS_SIZE     0x10000
// ---------------------------------

#endif /* TARGET_XCZU9EG */

// ---------------------------------
// DMA controller registers
#define DMACR             0   // control
#define DMASR             1   // status
#define DMASA             6   // source 
#define DMALEN            10  // length

// DMA controller commands/status
#define DMA_RESET         0x04
#define DMA_RUN           0x01
#define DMA_IDLE_MSK      0x02
// ---------------------------------

//NOTE: global to speed up processing
extern volatile uint * dma_registers;


//--------------------------------------------------------------------
// setup DMA controller through the AXI memory-mapped interface
//
// returns address of DMA data buffer -- NULL if problems found
//--------------------------------------------------------------------
void * dma_setup (size_t buf_size) {
  // check that requested size fits in the reserved memory space
  if (buf_size > DDR_RES_MEM_ADDR) {
    printf ("error: requested buffer size exceeds limit\n");
    return (NULL);
  }

  //NOTE: make sure that mapped memory is *not* cached!
  int fd = open ("/dev/mem", O_RDWR | O_SYNC);

  if (fd < 1) {
    printf ("error: unable to access DMA memory space\n");
    return (NULL);
  }

  // map reserved DDR memory to process address space
  void * dma_buffer = mmap (
    NULL, DDR_RES_MEM_SIZE, PROT_READ | PROT_WRITE,
    MAP_SHARED, fd, DDR_RES_MEM_ADDR
    );

  // map DMA (configuration) registers to process address space
  dma_registers = (uint *) mmap (
    NULL, DMA_REGS_SIZE, PROT_READ | PROT_WRITE,
    MAP_SHARED, fd, DMA_REGS_ADDR
    );

  // close /dev/mem and drop root privileges
  close (fd);
  if (setuid (getuid ()) < 0) {
    printf ("error: unable to access DMA memory space\n");
    return (NULL);
  }

  // check that everything went well
  if ((dma_buffer == MAP_FAILED) || (dma_registers == MAP_FAILED)) {
    printf ("error: unable to access DMA memory space\n");
    return (NULL);
  }

  // configure/initialise DMA controller for transmission
  // reset DMA controller - reset interrupts
  dma_registers[DMACR] = DMA_RESET;

  // wait for reset to complete
  while (dma_registers[DMACR] & DMA_RESET) {
    continue;
  }

  // start DMA controller
  dma_registers[DMACR] = DMA_RUN;

  // write dma_buffer physical address to DMA controller
  dma_registers[DMASA] = (uint) DDR_RES_MEM_ADDR;

  return (dma_buffer);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// trigger a DMA transfer
//--------------------------------------------------------------------
void dma_trigger (uint size) {
  // write length of data batch (in bytes!)
  dma_registers[DMALEN] = size;
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// check status of DMA controller
//
// returns 0 if DMA controller busy
//--------------------------------------------------------------------
uint dma_idle (void) {
  return (dma_registers[DMASR] & DMA_IDLE_MSK);
}
//--------------------------------------------------------------------

#endif
