//************************************************//
//*                                              *//
//* functions to interact with spif              *//
//* from the spif board processor                *//
//*                                              *//
//* lap - 08/09/2021                             *//
//*                                              *//
//************************************************//

#ifndef __SPIF_REMOTE_H__
#define __SPIF_REMOTE_H__

#include <sys/mman.h>
#include <fcntl.h>

#include "spif_fpga.h"
#include "spif.h"


//--------------------------------------------------------------------
// spif registers are static globals for performance
//NOTE: they are "constant" addresses once initialised
//--------------------------------------------------------------------
static volatile uint * dma_registers;
static volatile uint * apb_registers;
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// setup spif DMA controller through the AXI memory-mapped interface
//
// returns address of DMA data buffer -- NULL if problems found
//--------------------------------------------------------------------
void * spif_setup (uint pipe, size_t buf_size) {
  // check that requested pipe exists
  if (pipe >= SPIF_HW_PIPES_NUM) {
    printf ("error: requested event pipe does not exist\n");
    return (NULL);
  }

  // check that requested size fits in the reserved memory space
  if (buf_size > SPIF_BUF_MAX_SIZE) {
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
  void * rsvd_mem = mmap (
    NULL, DDR_RES_MEM_SIZE, PROT_READ | PROT_WRITE,
    MAP_SHARED, fd, DDR_RES_MEM_ADDR
    );

  if (rsvd_mem == MAP_FAILED) {
    printf ("error: unable to access reserved memory space\n");
    return (NULL);
  }

  // map APB (configuration) registers to process address space
  apb_registers = (uint *) mmap (
    NULL, APB_REGS_SIZE, PROT_READ | PROT_WRITE,
    MAP_SHARED, fd, APB_REGS_ADDR
    );

  // map DMA (configuration) registers to process address space
  off_t dma_addr = DMA_REGS_ADDR + (pipe * DMA_REGS_SIZE);
  dma_registers = (uint *) mmap (
    NULL, DMA_REGS_SIZE, PROT_READ | PROT_WRITE,
    MAP_SHARED, fd, dma_addr
    );

  // close /dev/mem and drop root privileges
  close (fd);
  if (setuid (getuid ()) < 0) {
    (void) munmap (rsvd_mem, DDR_RES_MEM_SIZE);
    printf ("error: unable to access DMA memory space\n");
    return (NULL);
  }

  // check that everything went well
  if ((apb_registers == MAP_FAILED) || (dma_registers == MAP_FAILED)) {
    (void) munmap (rsvd_mem, DDR_RES_MEM_SIZE);
    printf ("error: unable to access DMA memory space\n");
    return (NULL);
  }

  // locate dma buffer according to pipe
  void * dma_buffer = rsvd_mem + (pipe * PIPE_MEM_SIZE);

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
  dma_registers[DMASA] = (uint) DDR_RES_MEM_ADDR + (uint) (pipe * PIPE_MEM_SIZE);

  return (dma_buffer);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// close up access to spif
//
//NOTE: exists only for compatibility with the spif kernel driver
//--------------------------------------------------------------------
void spif_close (void)
{
}


//--------------------------------------------------------------------
// read spif register
//
// returns read value
//--------------------------------------------------------------------
int spif_read_reg (unsigned int reg)
{
  return apb_registers[reg];
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// write spif register
//--------------------------------------------------------------------
void spif_write_reg (unsigned int reg, int val)
{
  apb_registers[reg] = val;
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// check status of spif DMA controller
//
// returns 0 if DMA controller idle
//--------------------------------------------------------------------
uint spif_busy (void) {
  return (!(dma_registers[DMASR] & DMA_IDLE_MSK));
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// trigger a transfer to SpiNNaker
//--------------------------------------------------------------------
void spif_transfer (uint size) {
  // write length of data batch (in bytes!)
  dma_registers[DMALEN] = size;
}
//--------------------------------------------------------------------


#endif /* __SPIF_REMOTE_H__ */
