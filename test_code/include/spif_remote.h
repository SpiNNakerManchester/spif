//************************************************//
//*                                              *//
//* functions to interact with the spif          *//
//* through the spif kernel driver               *//
//*                                              *//
//* lap - 03/08/2021                             *//
//*                                              *//
//************************************************//

#ifndef __SPIF_REMOTE_H__
#define __SPIF_REMOTE_H__

#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/ioctl.h>

#include "spif.h"


// ---------------------------------
// spif operations
// ---------------------------------
// driver ioctl unreserved magic number (seq 0xf0 - 0xff)
#define SPIF_IOCTL_TYPE      'i'
#define SPIF_IOCTL_SEQ       0xf0
#define SPIF_NUM_REGS        128

// spif ioctl operation fields
//NOTE: size field used to encode spif register!
#define SPIF_OP_TYPE         (SPIF_IOCTL_TYPE << _IOC_TYPESHIFT)
#define SPIF_OP_REQ_MSK      (((1 << (_IOC_NRBITS + _IOC_TYPEBITS)) - 1) << _IOC_NRSHIFT)
#define SPIF_OP_REQ_SHIFT    _IOC_NRSHIFT
#define SPIF_OP_REG_MSK      ((SPIF_NUM_REGS - 1) << _IOC_SIZESHIFT)
#define SPIF_OP_REG_SHIFT    _IOC_SIZESHIFT
#define SPIF_OP_RD           (_IOC_READ  << _IOC_DIRSHIFT)
#define SPIF_OP_WR           (_IOC_WRITE << _IOC_DIRSHIFT)
#define SPIF_OP_CMD          (_IOC_NONE  << _IOC_DIRSHIFT)

// spif operations
#define SPIF_OP_REQ(r)       (SPIF_OP_CMD | ((SPIF_OP_TYPE | (SPIF_IOCTL_SEQ + r))  << _IOC_NRSHIFT))
#define SPIF_REG_RD          SPIF_OP_REQ(0)
#define SPIF_REG_WR          SPIF_OP_REQ(1)
#define SPIF_STATUS_RD       SPIF_OP_REQ(2)
#define SPIF_TRANSFER        SPIF_OP_REQ(3)
#define SPIF_GET_OUTP        SPIF_OP_REQ(4)
#define SPIF_BUF_SIZE        SPIF_OP_REQ(5)
// ---------------------------------


// ---------------------------------
// spif pipe persitent data
// ---------------------------------
struct pipe_data {
  int    fd;        // pipe device file descriptor
  void * buf_iva;   // input buffer (virtual) address
  void * buf_ova;   // output buffer (virtual) address
  uint   buf_size;  // pipe buffer size
};

static struct pipe_data pipe_data[SPIF_HW_PIPES_NUM];

// convenient static data place holder - to interact with kernel module
static int open_dummy[SPIF_HW_PIPES_NUM];
static int read_dummy[SPIF_HW_PIPES_NUM];
static int busy_dummy[SPIF_HW_PIPES_NUM];
static int out_dummy[SPIF_HW_PIPES_NUM];
// ---------------------------------


//--------------------------------------------------------------------
// opens requested spif pipe for access
//
// opens associated spif device
// gets pipe buffer size
// maps pipe input and output buffers to user space
// keeps pipe data in static memory
//
// returns -1 if error
//--------------------------------------------------------------------
int spif_open (uint pipe)
{
  // create device name from pipe
  char fname[11];
  (void) sprintf (fname, "/dev/spif%u", pipe);

  // open spif device
  int fd = open (fname, O_RDWR | O_SYNC);
  if (fd == -1) {
    return (-1);
  }

  // get device buffer size
  (void) ioctl (fd, SPIF_BUF_SIZE, (void *) &(open_dummy[pipe]));

  // map pipe memory buffer to user space
  //NOTE: input buffer is located at beginning of pipe memory
  void * iva = mmap (NULL, 2 * open_dummy[pipe],
		     PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (iva == MAP_FAILED) {
    close (fd);
    return (-1);
  }

  // map output buffer to user space
  //NOTE: output buffer is located after input buffer
  void * ova = (void *) ((int) iva + open_dummy[pipe]);

  // keep pipe state to service future requests
  pipe_data[pipe].fd       = fd;
  pipe_data[pipe].buf_iva  = iva;
  pipe_data[pipe].buf_ova  = ova;
  pipe_data[pipe].buf_size = open_dummy[pipe];

  return fd;
}


//--------------------------------------------------------------------
// request a spif input buffer associated with a pipe
//
// returns NULL if error
//--------------------------------------------------------------------
void * spif_get_buffer (uint pipe, uint buf_size)
{
  // check requested buffer size
  if (buf_size > pipe_data[pipe].buf_size) {
    return (NULL);
  }

  // return previously mapped address
  return (pipe_data[pipe].buf_iva);
}


//--------------------------------------------------------------------
// request a spif output buffer associated with a pipe
//
// returns NULL if error
//--------------------------------------------------------------------
void * spif_get_output_buffer (uint pipe, uint buf_size)
{
  // check requested buffer size
  if (buf_size > pipe_data[pipe].buf_size) {
    return (NULL);
  }

  // return previously mapped address
  return (pipe_data[pipe].buf_ova);
}


//--------------------------------------------------------------------
// close up access to spif
//
// no return value
//--------------------------------------------------------------------
void spif_close (uint pipe)
{
  close (pipe_data[pipe].fd);
}


//--------------------------------------------------------------------
// read spif register
//
// returns read value
//--------------------------------------------------------------------
int spif_read_reg (uint pipe, unsigned int reg)
{
  // encode spif read register request
  unsigned int req = (reg << 16) | SPIF_REG_RD;

  // send request to spif and convey result
  //NOTE: ioctl never fails with this request
  (void) ioctl (pipe_data[pipe].fd, req, (void *) &(read_dummy[pipe]));

  return read_dummy[pipe];
}


//--------------------------------------------------------------------
// write spif register
//--------------------------------------------------------------------
void spif_write_reg (uint pipe, unsigned int reg, int val)
{
  // encode spif read register request
  unsigned int req = (reg << 16) | SPIF_REG_WR;

  // send request to spif
  //NOTE: ioctl never fails with this request
  (void) ioctl (pipe_data[pipe].fd, req, (void *) (long) val);
}
// ---------------------------------


//--------------------------------------------------------------------
// read spif busy status
//
// returns 0 if spif idle (no ongoing transfer)
//--------------------------------------------------------------------
int spif_busy (uint pipe)
{
  // send request to spif and convey result
  //NOTE: ioctl never fails with this request
  (void) ioctl (pipe_data[pipe].fd, SPIF_STATUS_RD, (void *) &(busy_dummy[pipe]));

  return busy_dummy[pipe];
}


//--------------------------------------------------------------------
// set spif output event tick
//--------------------------------------------------------------------
void spif_set_out_tick (uint pipe, int val)
{
  // encode spif read register request
  unsigned int req = (SPIF_OUT_TICK << 16) | SPIF_REG_WR;

  // send request to spif
  //NOTE: ioctl never fails with this request
  (void) ioctl (pipe_data[pipe].fd, req, (void *) (long) val);
}
// ---------------------------------


//--------------------------------------------------------------------
// set spif output event frame length
//--------------------------------------------------------------------
void spif_set_out_len (uint pipe, int val)
{
  // encode spif read register request
  unsigned int req = (SPIF_OUT_LEN << 16) | SPIF_REG_WR;

  // send request to spif
  //NOTE: ioctl never fails with this request
  (void) ioctl (pipe_data[pipe].fd, req, (void *) (long) val);
}
// ---------------------------------


//--------------------------------------------------------------------
// start a transfer to SpiNNaker
//
// returns 0 if transfer succeeds
//--------------------------------------------------------------------
int spif_transfer (uint pipe, int length)
{
  // send request to spif and convey result
  return (ioctl (pipe_data[pipe].fd, SPIF_TRANSFER, (void *) (long) length));
}


//--------------------------------------------------------------------
// wait for a transfer from SpiNNaker
//
// returns the length of the transfer (in bytes)
//--------------------------------------------------------------------
int spif_get_output (uint pipe, int length)
{
  // dummy is used to send requested length and receive actual length
  out_dummy[pipe] = length;

  // send request to spif and convey result
  ioctl (pipe_data[pipe].fd, SPIF_GET_OUTP, (void *) &(out_dummy[pipe]));

  return (out_dummy[pipe]);
}


#endif /* __SPIF_REMOTE_H__ */
