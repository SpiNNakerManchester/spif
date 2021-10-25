//************************************************//
//*                                              *//
//* functions to interact with the spif          *//
//* through the spif kernel driver               *//
//*                                              *//
//* lap - 03/08/2021                             *//
//*                                              *//
//************************************************//

#ifndef __SPIF_DRV_H__
#define __SPIF_DRV_H__

#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

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
// ---------------------------------


// ---------------------------------
// spif file descriptor associated with spif device
static int spif_fd;
static int dummy;    // convenient place holder
// ---------------------------------


//--------------------------------------------------------------------
// opens requested spif device for access
//
// returns -1 if error
//--------------------------------------------------------------------
int spif_open (uint pipe)
{
  // create device name from pipe
  char fname[11];
  (void) sprintf (fname, "/dev/spif%u", pipe);

  // open spif device
  spif_fd = open (fname, O_RDWR | O_SYNC);
  if (spif_fd == -1) {
    switch (errno) {
    case ENOENT:
      printf ("spif error: no spif device\n");
      break;
    case EBUSY:
      printf ("spif error: spif device busy\n");
      break;
    case ENODEV:
      printf ("spif error: no connection to SpiNNaker\n");
      break;
    default:
      printf ("spif error: [%s]\n", strerror (errno));
    }
  }

  return spif_fd;
}


//--------------------------------------------------------------------
// set up access to spif through requested pipe device
//
// returns NULL if error
//--------------------------------------------------------------------
void * spif_setup (uint pipe, int buf_size)
{
  // check requested buffer size
  if (buf_size > SPIF_BUF_MAX_SIZE) {
    printf ("spif error: buffer size exceeds maximum\n");
    return (NULL);
  }

  // open spif device
  if (spif_open (pipe) == -1) {
    return (NULL);
  }

  // map spif memory into user-space buffer 
  void * buffer = mmap (
    NULL, SPIF_BUF_MAX_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
    spif_fd, 0
    );

  if (buffer == MAP_FAILED) {
    printf ("spif error: unable to map spif buffer [%s]\n", strerror (errno));
    close (spif_fd);
    return (NULL);
  }

  return (buffer);
}


//--------------------------------------------------------------------
// close up access to spif
//
// returns spif device file handle
//--------------------------------------------------------------------
void spif_close (void)
{
  close (spif_fd);
}


//--------------------------------------------------------------------
// read spif register
//
// returns read value
//--------------------------------------------------------------------
int spif_read_reg (unsigned int reg)
{
  // encode spif read register request
  unsigned int req = (reg << 16) | SPIF_REG_RD;

  // send request to spif and convey result
  //NOTE: ioctl never fails with this request
  (void) ioctl (spif_fd, req, (void *) &dummy);

  return dummy;
}


//--------------------------------------------------------------------
// write spif register
//--------------------------------------------------------------------
void spif_write_reg (unsigned int reg, int val)
{
  // encode spif read register request
  unsigned int req = (reg << 16) | SPIF_REG_WR;

  // send request to spif
  //NOTE: ioctl never fails with this request
  (void) ioctl (spif_fd, req, (void *) (long) val);
}
// ---------------------------------


//--------------------------------------------------------------------
// read spif busy status
//
// returns 0 if spif idle (no ongoing transfer)
//--------------------------------------------------------------------
int spif_busy (void)
{
  // send request to spif and convey result
  //NOTE: ioctl never fails with this request
  (void) ioctl (spif_fd, SPIF_STATUS_RD, (void *) &dummy);

  return dummy;
}


//--------------------------------------------------------------------
// start a transfer to SpiNNaker
//
// returns 0 if transfer succeeds
//--------------------------------------------------------------------
int spif_transfer (int length)
{
  // send request to spif and convey result
  return (ioctl (spif_fd, SPIF_TRANSFER, (void *) (long) length));
}


//--------------------------------------------------------------------
// spif service request
//
// returns 0 if request succeeds
//--------------------------------------------------------------------
int spif_req (unsigned int req, int * val)
{
  // send request to spif and convey result
  int rc = ioctl (spif_fd, req, (void *) val);
  if (rc == -1) {
    printf ("spif error: spif request %d failed [%s]\n", req, strerror (errno));
    return (-1);
  }

  return (rc);
}


#endif /* __SPIF_DRV_H__ */
