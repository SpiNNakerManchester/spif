//************************************************//
//*                                              *//
//* simple SpiNNaker peripheral output client    *//
//*                                              *//
//* exits with -1 if problems found              *//
//*                                              *//
//* lap - 20/10/2022                             *//
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

#include "spif.h"

#define EVT_BATCH_SIZE    256


//NOTE: global variables simplify interfaces and speed up processing
int                evt_socket;
struct sockaddr_in server_addr;

FILE  * raw_evt_file;
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
// setup raw event data stream
// - open event file
//
// returns -1 if problems found
//--------------------------------------------------------------------
int setup_raw_evt_file (char * evt_fn) {
  // open raw event file for write
  raw_evt_file = fopen (evt_fn, "wb");

  if (raw_evt_file == NULL) {
    printf ("error: cannot open raw event filen");
    return (-1);
  }

  return (0);
}


//--------------------------------------------------------------------
// write the next batch of raw events to file
//--------------------------------------------------------------------
int write_raw_evt_batch (uint length) {
  int rc = fwrite (evt_data, 1, length, raw_evt_file);
  fflush (raw_evt_file);
  return (rc);
}


//--------------------------------------------------------------------
// checks arguments and sends data
//
// exits with -1 if problems found
//--------------------------------------------------------------------
int main (int argc, char * argv[])
{
  int addr_len = sizeof (struct sockaddr);
  int num_events = 1024;

  // check that required arguments were provided
  if (argc < 4) {
    printf ("usage: %s <server> <port> <events file name> [events {def: 1024}]\n", argv[0]);
    exit (-1);
  }

  // check if number of events to receive is provided
  if (argc > 4) {
    num_events = atoi (argv[4]);
  }

  // open raw event stream
  if (setup_raw_evt_file (argv[3]) == -1) {
    printf("error: cannot open events file\n");
    exit (-1);
  }

  // setup Ethernet UDP client to receive raw event data
  if (setup_eth_udp_cli (argv[1], atoi (argv[2])) == -1) {
    printf("error: unable to connect to server\n");
    exit (-1);
  }

  // set output parameters and start
  evt_data[0] = SPIF_OUT_SET_LEN + 128;
  evt_data[1] = SPIF_OUT_SET_TICK + 500;
  evt_data[2] = SPIF_OUT_START;
  uint bs = sendto (evt_socket, evt_data, 3 * sizeof (uint),
        0, (struct sockaddr *) &server_addr, sizeof (struct sockaddr));

  if (bs < 0) {
    printf ("error: write to server failed\n");
    exit (-1);
  }

  // receive data from server and iterate over the entire event file
  uint events = 0;
  while (events < num_events) {
    // receive spif output data
    uint br = recv (evt_socket, evt_data, EVT_BATCH_SIZE * sizeof (uint), 0);

    // write output data to file
    (void) write_raw_evt_batch (br);

    // report length of data
    printf ("%u\n", (uint) (br / sizeof (uint)));

    events += br / sizeof (uint);
  }

  fclose (raw_evt_file);

  // send stop command to spif
  evt_data[0] = SPIF_OUT_STOP;
  bs = sendto (evt_socket, evt_data, sizeof (uint),
        0, (struct sockaddr *) &server_addr, sizeof (struct sockaddr));

  if (bs < 0) {
    printf ("error: write to server failed\n");
    exit (-1);
  }

  exit (0);
}
