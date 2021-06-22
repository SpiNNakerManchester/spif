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


// addresses of memory-mapped FPGA interfaces 

// ---------------------------------
// AXI_HP0 PL -> DDR interface

// reserved DDR range for DMA buffers 
#define DDR_RES_MEM_ADDR  0x3ffff000
#define DDR_RES_MEM_SIZE  0x1000
// ---------------------------------

// ---------------------------------
// AXI_GP0 PS -> PL interface

// DMA controller configuration
#define DMA_AXI_LITE      0x40400000
// ---------------------------------

// ---------------------------------
// DMA controller registers
#define DMACR            0   // control
#define DMASR            1   // status
#define DMASA            6   // source 
#define DMALEN           10  // length

// DMA controller commands/status
#define DMA_RESET        0x04
#define DMA_RUN          0x01
#define DMA_IDLE_MSK     0x02
// ---------------------------------

//NOTE: global to speed up processing
extern volatile uint * dma_registers;


//--------------------------------------------------------------------
// setup DMA controller through the AXI memory-mapped interface
//
// returns address of DMA data buffer -- NULL if problems found
//--------------------------------------------------------------------
uint * dma_setup () {
  //NOTE: make sure that mapped memory is *not* cached!
  int fd = open ("/dev/mem", O_RDWR | O_SYNC);

  if (fd < 1) {
    printf ("error: unable to open /dev/mem\n");
    return (NULL);
  }

  // map reserved DDR memory to DMA buffer virtual address
  uint * dma_buffer = (unsigned int *) mmap (
    NULL, DDR_RES_MEM_SIZE, PROT_READ | PROT_WRITE,
    MAP_SHARED, fd, DDR_RES_MEM_ADDR
    );

  // map DMA registers (configuration) - 64KB address space
  dma_registers = (unsigned int *) mmap (
    NULL, 0x10000, PROT_READ | PROT_WRITE,
    MAP_SHARED, fd, DMA_AXI_LITE
    );

  // close /dev/mem and drop root privileges
  close (fd);
  if (setuid (getuid ()) < 0) {
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
//
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
//
//--------------------------------------------------------------------
uint dma_idle (void) {
  return (dma_registers[DMASR] & DMA_IDLE_MSK);
}
//--------------------------------------------------------------------

#endif
