//************************************************//
//*                                              *//
//* functions to interact with spif              *//
//*                                              *//
//* lap - 21/06/2021                             *//
//*                                              *//
//************************************************//

#ifndef __SPIF_IF_H__
#define __SPIF_IF_H__

#include <sys/mman.h>
#include <fcntl.h>


//---------------------------------------------------------------
// FPGA selection
//NOTE: un-comment the required FPGA #define
//---------------------------------------------------------------
#define TARGET_XC7Z015   // Zynq7 on TE0715 board
//#define TARGET_XCZU9EG   // Zynq Ultrascale+ on zcu102 board
//---------------------------------------------------------------

#define HW_NUM_PIPES      2


// addresses of memory-mapped FPGA interfaces 

#ifdef TARGET_XC7Z015

// ---------------------------------
// AXI_HP0 PL -> DDR interface

// reserved DDR range for DMA buffers 
#define DDR_RES_MEM_ADDR  0x3fffc000
#define DDR_RES_MEM_SIZE  0x4000
#define PIPE_MEM_SIZE     (DDR_RES_MEM_SIZE / 4)
// ---------------------------------

// ---------------------------------
// AXI_GP0 PS -> PL interface

// spif configuration registers
#define APB_REGS_ADDR     0x43c20000
#define APB_REGS_SIZE     0x10000

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


//--------------------------------------------------------------------
// spif support constants and functions
//--------------------------------------------------------------------
#define SPIF_BUF_MAX_SIZE 4096

#define SPIF_MAPPER_NUM   4
#define SPIF_ROUTER_NUM   16
#define SPIF_COUNT_NUM    4

#define SPIF_BUSY         1

// ---------------------------------
// spif registers
// ---------------------------------
#define SPIF_MAPPER_KEY    1
#define SPIF_REPLY_KEY     2
#define SPIF_IN_DROP_WAIT  3
#define SPIF_OUT_DROP_WAIT 4
#define SPIF_ROUTER_KEY    16
#define SPIF_ROUTER_MASK   32
#define SPIF_ROUTER_ROUTE  48
#define SPIF_COUNT_OUT     64
#define SPIF_COUNT_CONFIG  65
#define SPIF_COUNT_IN_DROP 66
#define SPIF_COUNT_IN      67
#define SPIF_MAPPER_MASK   80
#define SPIF_MAPPER_SHIFT  96
// ---------------------------------


static volatile uint * dma_registers;
static volatile uint * apb_registers;


//--------------------------------------------------------------------
// setup spif DMA controller through the AXI memory-mapped interface
//
// returns address of DMA data buffer -- NULL if problems found
//--------------------------------------------------------------------
void * spif_setup (uint pipe, size_t buf_size) {
  // check that requested pipe exists
  if (pipe >= HW_NUM_PIPES) {
    printf ("error: requested event pipe does not exist\n");
    return (NULL);
  }

  // check that requested size fits in the reserved memory space
  if (buf_size > PIPE_MEM_SIZE) {
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
// returns spif device file handle
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


#endif /* __SPIF_IF_H__ */
