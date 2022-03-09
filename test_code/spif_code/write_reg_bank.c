//************************************************//
//*                                              *//
//* write spif register                          *//
//*                                              *//
//* exits with -1 if problems found              *//
//*                                              *//
//* lap - 21/08/2021                             *//
//*                                              *//
//************************************************//

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>

#include "spif_remote.h"

#define BUF_SIZE_DEF   0x1000


//--------------------------------------------------------------------
// write a spif register passed as argument
// write data also passed as argument
//
// exits with -1 if problems found
//--------------------------------------------------------------------
int main (int argc, char* argv[])
{
  char * cname = basename (argv[0]);
  // check that enough arguments are present
  if (argc < 3) {
    printf ("usage: %s <register_offset> <data>\n", cname);
    exit (-1);
  }

  // required arguments
  uint reg_addr = atoi (argv[1]);
  uint reg_data = atoi (argv[2]);

  // setup access to spif
  //NOTE: size is not important as spif buffer will not be used
  //NOTE: pipe is not important as it will not be used
  if (spif_setup (0, BUF_SIZE_DEF) == NULL) {
    printf ("%s: unable to access spif\n", cname);
    exit (-1);
  }

  // write to the register
  spif_write_reg (reg_addr, reg_data);
  printf ("reg[%u] = %d (0x%08x)\n", reg_addr, reg_data, reg_data);

  // clean up and finish
  spif_close (spif_fd);
  return 0;
}
