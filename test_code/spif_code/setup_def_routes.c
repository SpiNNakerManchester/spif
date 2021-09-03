//************************************************//
//*                                              *//
//* write spif default input routing entries     *//
//* route event packet according to Y coord      *//
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

#include "spif.h"


#define SPIF_SIZE_DEF   0x1000


uint const DEF_ENTRIES  = 8;
uint const DEF_KEYS[]   = {0, 1, 2, 3, 4, 5, 6, 7};
uint const DEF_MASKS[]  = {7, 7, 7, 7, 7, 7, 7, 7};
uint const DEF_ROUTES[] = {0, 1, 2, 3, 4, 5, 6, 7};


//--------------------------------------------------------------------
// write spif default input routing entries
// route event packet according to Y coord
//
// exits with -1 if problems found
//--------------------------------------------------------------------
int main (int argc, char* argv[])
{
  char * cname = basename (argv[0]);

  // setup access to spif
  //NOTE: size is not important as spif buffer will not be used
  //NOTE: pipe is not important as it will not be used
  if (spif_setup (0, SPIF_SIZE_DEF) == NULL) {
    printf ("%s: unable to access spif\n", cname);
    exit (-1);
  }

  // setup default routing
  for (uint i = 0; i < DEF_ENTRIES; i++) {
    spif_write_reg (SPIF_ROUTER_KEY   + i, DEF_KEYS[i]);
    spif_write_reg (SPIF_ROUTER_MASK  + i, DEF_MASKS[i]);
    spif_write_reg (SPIF_ROUTER_ROUTE + i, DEF_ROUTES[i]);
    printf ("route %u: key/%u mask/%u route/%u\n",
      i, DEF_KEYS[i], DEF_MASKS[i], DEF_ROUTES[i]);
  }

  // clean up and finish
  spif_close ();
  return 0;
}
