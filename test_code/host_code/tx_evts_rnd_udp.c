//************************************************//
//*                                              *//
//* transmit "random" events to UDP server       *//
//*                                              *//
//* - events are pseudo-random in X and Y coords *//
//* - processing tick resolution in ns           *//
//*                                              *//
//* exits with -1 if problems found              *//
//*                                              *//
//* lap - 20/12/2020                             *//
//*                                              *//
//* TODO:                                        *//
//*                                              *//
//************************************************//

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <libgen.h>


#define NPIPES            2

#define NSEC_PER_SEC      1000000000

#define PROC_TICK_NSEC    1000000

#define EVT_BATCH_SIZE    256

#define EVT_NO_TS         0x80000000
#define EVT_X_POS         16
#define EVT_X_BITS        3
#define EVT_PIPE_POS      (EVT_X_POS + EVT_X_BITS)

#define NCHIPS            8
#define NCORES            8

//NOTE: global variables simplify interfaces and speed up processing
int                evt_socket;
struct sockaddr_in server_addr;

uint    evt_data[EVT_BATCH_SIZE];


//--------------------------------------------------------------------
// setup Ethernet UDP client to send data
// - create socket and prepare for data comms
//
// returns -1 if problems found
//--------------------------------------------------------------------
int setup_eth_udp_cli (char * eth_serv, int eth_port) {
  struct hostent * server;

  server = gethostbyname (eth_serv);

  // create socket
  if ((evt_socket = socket (AF_INET, SOCK_DGRAM, 0)) == -1) {
    return (-1);
  }

  // configure Ethernet environment
  server_addr.sin_family = AF_INET;
  server_addr.sin_port   = htons (eth_port);
  server_addr.sin_addr   = *((struct in_addr *) server->h_addr);
  bzero (&(server_addr.sin_zero), 8);

  return (0);
}


//--------------------------------------------------------------------
// checks arguments and sends data
//
// exits with -1 if problems found
//--------------------------------------------------------------------
int main (int argc, char * argv[])
{
  char * cname = basename (argv[0]);

  // check that required arguments were provided
  if (argc < 7) {
    printf ("usage: %s <pipe> <server> <port> <num_data_items_to_send> <throttle> <seed>\n", cname);
    exit (-1);
  }

  // required arguments
  uint   pipe          = atoi (argv[1]);
  char * server        = argv[2];
  uint   port          = atoi (argv[3]);
  uint   items_to_send = atoi (argv[4]);
  uint   throttle      = atoi (argv[5]);
  int    seed          = atoi (argv[6]);

  // check argument values
  if (pipe >= NPIPES) {
    printf ("%s: pipe %u does not exist\n", cname, pipe);
    exit (-1);
  }

  // pipe mask for events
  uint const PCM = (NPIPES - 1) << EVT_PIPE_POS;

  int addr_len = sizeof (struct sockaddr);

  // event-related variables
  ulong total_items = 0;

  // processing tick-related variables
  struct timespec proc_first;
  struct timespec proc_cur;

  // setup Ethernet UDP client to send raw event data
  if (setup_eth_udp_cli (server, port) == -1) {
    printf("%s: unable to connect to server\n", cname);
    exit (-1);
  }

  printf ("sending %u data items (throttle: %u) to %s:%u pipe %u\n",
	  items_to_send, throttle, server, port, pipe);

  // fill the event data buffer
  srand (seed);
  uint item = 0;
  while (item < EVT_BATCH_SIZE) {
    for (uint x = 0; x < NCORES; x++) {
      for (uint y = 0; y < NCHIPS; y++) {
	//adjust random event according to pipe
	uint evt = rand () & ~PCM;
	evt |= pipe << EVT_PIPE_POS;

	// No timestamps
        evt_data[item] = EVT_NO_TS | evt;

	item++;
	if (item >= EVT_BATCH_SIZE) {
	  break;
	}
      }

      if (item >= EVT_BATCH_SIZE) {
	break;
      }
    }
  }

  // get initial processing timer value as reference
  clock_gettime(CLOCK_MONOTONIC, &proc_first);

  // iterate over the entire event file
  uint data_items;
  do {
    // write data batch to server
    uint bs = sendto (evt_socket, evt_data, EVT_BATCH_SIZE * sizeof (uint),
        0, (struct sockaddr *) &server_addr, sizeof (struct sockaddr));

    if (bs < 0) {
      printf ("error: write to server failed\n");
      break;
    }

    // stats: total sent data items
    data_items = bs / sizeof (uint);
    total_items += data_items;

    // throttle sender
    for (uint i = 0; i < throttle; i++) {
      continue;
    }
  } while (total_items <= items_to_send);

  // compute processing time
  clock_gettime (CLOCK_MONOTONIC, &proc_cur);
  long proc_dif = proc_cur.tv_nsec - proc_first.tv_nsec;
  proc_dif += NSEC_PER_SEC * (proc_cur.tv_sec - proc_first.tv_sec);

  // report total_items, wait cycles and elapsed processing ticks
  printf ("raw events  = %lu\n", total_items);
  printf ("proc ticks  = %lu\n", proc_dif / PROC_TICK_NSEC);

  exit (0);
}
