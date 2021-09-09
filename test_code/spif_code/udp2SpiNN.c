//************************************************//
//*                                              *//
//* transfer data from Ethernet to SpiNNaker     *//
//* ** receive Ethernet data over UDP            *//
//*                                              *//
//* - processing ticks measured in ms            *//
//*                                              *//
//* exits with -1 if problems found              *//
//*                                              *//
//* lap - 21/06/2021                             *//
//*                                              *//
//************************************************//

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <libgen.h>

#include "spif_remote.h"


#define NSEC_PER_SEC      1000000000

#define PROC_TICK_NSEC    1000000

#define DATA_BATCH_SIZE   256


//NOTE: made global to avoid unnecessary passing
//      around as function parameters
int srv_sckt;


//--------------------------------------------------------------------
// setup Ethernet UDP server to receive data
// - create and bind on socket
//
// returns -1 if problems found
//--------------------------------------------------------------------
int udp_srv_setup (int eth_port) {
  // create UDP socket
  srv_sckt = socket (AF_INET, SOCK_DGRAM, 0);
  if (srv_sckt == -1) {
    return (-1);
  }

  // configure server environment
  struct sockaddr_in srv_addr;
  srv_addr.sin_family      = AF_INET;
  srv_addr.sin_port        = htons (eth_port);
  srv_addr.sin_addr.s_addr = INADDR_ANY;
  bzero (&(srv_addr.sin_zero), 8);

  // bind UDP socket
  if (bind (srv_sckt, (struct sockaddr *) &srv_addr,
      sizeof (struct sockaddr)) == -1) {
    close (srv_sckt);
    return (-1);
  }

  return (0);
}


//--------------------------------------------------------------------
// get the next batch of data from Ethernet
//
// returns 0 if no data available
//--------------------------------------------------------------------
int udp_get_data_batch (uint * buf, size_t batch_size) {

  return (recv (srv_sckt, (void *) buf, batch_size, 0));
}


//--------------------------------------------------------------------
// receives data from Ethernet and forwards it to SpiNNaker
//
// exits with -1 if problems found
//--------------------------------------------------------------------
int main (int argc, char * argv[])
{
  // check that required arguments were provided
  char * cname = basename (argv[0]);
  if (argc < 4) {
    printf ("usage: %s <event_pipe> <udp_port> <num_data_items_to_receive/0 = unlimited>\n",
	    cname);
    exit (-1);
  }

  // required arguments
  uint  pipe           = atoi (argv[1]);
  int   udp_port       = atoi (argv[2]);
  ulong expected_items = atoi (argv[3]);

  // setup Ethernet UDP server to receive data
  if (udp_srv_setup (udp_port) == -1) {
    printf ("%s: failed to setup UDP server\n", cname);
    exit (-1);
  }

  // get pointer to spif buffer
  size_t const batch_size = DATA_BATCH_SIZE * sizeof (uint);
  uint * spif_buffer = (uint *) spif_setup (pipe, batch_size);
  if (spif_buffer == NULL) {
    printf ("%s: unable to access spif\n", cname);
    exit (-1);
  }

  // announce task
  if (expected_items) {
    printf ("receiving %lu data items on UDP port %d\n", expected_items, udp_port);
  } else {
    printf ("receiving unlimited data items on UDP port %d <ctrl-c to exit>\n", udp_port);
  }

  // statistics counters
  ulong total_items = 0;
  ulong wait_cycles = 0;

  // time-tracking variables
  struct timespec proc_first;
  struct timespec proc_cur;

  // wait for initial data batch to start time measurement
  int rcv_bytes = udp_get_data_batch (spif_buffer, batch_size);

  // get initial processing timer value as reference
  clock_gettime(CLOCK_MONOTONIC, &proc_first);

  // iterate to receive data from Ethernet
  while (!expected_items || (total_items < expected_items)) {
    // finish if no data received
    if (rcv_bytes <= 0) {
      break;
    }

    // trigger a transfer to SpiNNaker
    //NOTE: length of transfer in bytes!
    spif_transfer (rcv_bytes);

    // stats: total data items received
    total_items += rcv_bytes / sizeof (uint);

    // wait until spif finishes the current transfer
    //NOTE: make sure not to wait forever!
    int wc = 0;
    while (spif_busy ()) {
      wc++;
      if (wc < 0) {
        printf ("%s: wait cycles exceeded limit\n", cname);
        exit (-1);
      }
    }

    // stats: total wait cycles
    wait_cycles += wc;

    // get next data batch
    rcv_bytes = udp_get_data_batch (spif_buffer, batch_size);
  }

  // compute processing time
  clock_gettime (CLOCK_MONOTONIC, &proc_cur);
  long proc_dif = (proc_cur.tv_nsec - proc_first.tv_nsec) / PROC_TICK_NSEC;
  long dif_sec  = (proc_cur.tv_sec - proc_first.tv_sec) * (NSEC_PER_SEC / PROC_TICK_NSEC);
  proc_dif += dif_sec;

  // report total_items, wait cycles and elapsed processing ticks
  printf ("data items  = %lu\n", total_items);
  printf ("wait cycles = %lu\n", wait_cycles);
  printf ("proc ticks  = %lu\n", proc_dif);

  exit (0);
}
