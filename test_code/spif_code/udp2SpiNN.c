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

#include "dma_spif.h"


#define NSEC_PER_SEC      1000000000

#define PROC_TICK_NSEC    1000000

#define DATA_BATCH_SIZE   256


//NOTE: made global to avoid unnecessary passing
//      around as function parameters
volatile uint * dma_registers;

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
//NOTE: may need the client address to identify peripheral device
//
// returns 0 if no data available
//--------------------------------------------------------------------
int udp_get_data_batch (uint * buf, size_t batch_size) {

  struct sockaddr_in cli_addr;

  socklen_t addr_len = sizeof (struct sockaddr);

  return (recvfrom (srv_sckt, (void *) buf, batch_size, 0,
                    (struct sockaddr *) &cli_addr, &addr_len));
}


//--------------------------------------------------------------------
// receives data from Ethernet and forwards it to SpiNNaker
//
// exits with -1 if problems found
//--------------------------------------------------------------------
int main (int argc, char * argv[])
{
  // check that required arguments were provided
  if (argc < 3) {
    printf ("usage: %s <udp_port> <num_data_items_to_receive>\n", argv[0]);
    exit (-1);
  }

  // required arguments
  int udp_port = atoi (argv[1]);
  ulong expected_items = atoi (argv[2]);

  // setup Ethernet UDP server to receive data
  if (udp_srv_setup (udp_port) == -1) {
    printf ("error: failed to setup UDP server\n");
    exit (-1);
  }

  // get pointer to DMA buffer
  uint * dma_buffer = dma_setup ();
  if (dma_buffer == NULL) {
    printf ("error: failed to setup DMA controller\n");
    exit (-1);
  }

  // announce task
  printf ("receiving %lu data items on UDP port %d\n", expected_items, udp_port);

  // statistics counters
  ulong total_items = 0;
  ulong wait_cycles = 0;

  // time-tracking variables
  struct timespec proc_first;
  struct timespec proc_cur;

  size_t const batch_size = DATA_BATCH_SIZE * sizeof (uint);

  // wait for initial data batch to start time measurement
  int rcv_bytes = udp_get_data_batch (dma_buffer, batch_size);

  // get initial processing timer value as reference
  clock_gettime(CLOCK_MONOTONIC, &proc_first);

  // iterate to receive data from Ethernet
  while (total_items < expected_items) {
    // finish if no data received
    if (rcv_bytes <= 0) {
      break;
    }

    // trigger a DMA transfer with the length of data batch (in bytes!)
    dma_trigger (rcv_bytes);

    // stats: total data items received
    total_items += rcv_bytes / sizeof (uint);

    // wait until DMA controller finishes the current transfer
    //NOTE: make sure not to wait forever!
    int wc = 0;
    while (!dma_idle ()) {
      wc++;
      if (wc < 0) {
        printf ("error: wait cycles exceeded limit\n");
        exit (-1);
      }
    }

    // stats: total wait cycles
    wait_cycles += wc;

    // get next data batch
    rcv_bytes = udp_get_data_batch (dma_buffer, batch_size);
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
