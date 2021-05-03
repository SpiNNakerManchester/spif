//************************************************//
//*                                              *//
//* transmit data from Ethernet to SpiNNaker     *//
//*                                              *//
//* - processing ticks measured in ms            *//
//*                                              *//
//* exits with -1 if problems found              *//
//*                                              *//
//* lap - 20/12/2020                             *//
//*                                              *//
//* TODO:                                        *//
//* - deal with 64-bit timestamp wrap around     *//
//*                                              *//
//************************************************//

#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/un.h>

#include "axi_address_and_register_map.h"


#define NSEC_PER_SEC      1000000000

//NOTE: value needs to be adjusted if processing tick changes
#define PROC_TICK_NSEC    1000000

#define DATA_BATCH_SIZE    256


//NOTE: global variables simplify interfaces and speed up processing
FILE  * data_strm;
uint    eth_data[DATA_BATCH_SIZE];

uint  * axi_registers;
uint  * axi_tx_data;


//--------------------------------------------------------------------
// setup AXI memory-mapped interface
// - configuration registers
// - transmit data register
//
// exits with -1 if problems found
//--------------------------------------------------------------------
void setup_axi () {
  int fd = open ("/dev/mem", O_RDWR);

  if (fd < 1) {
    printf ("error: unable to open /dev/mem\n");
    exit (-1);
  }

  // map AXI_LITE registers (configuration) - 64KB address space
  axi_registers = (unsigned int *) mmap (
    NULL, 0x10000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, AXI_LITE
    );

  // map AXI_FULL data register (transmission) - 64KB address space
  axi_tx_data = (unsigned int *) mmap (
    NULL, 0x10000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, AXI_FULL
    );

  // close /dev/mem and drop root privileges
  close (fd);
  if (setuid (getuid ()) < 0) {
    exit (-1);
  }

  // configure/initialise AXI FIFO for transmission
  (void) axi_registers[0];          // read  ISR - reset value 0x01d00000
  axi_registers[0] = 0xffffffff;    // clear ISR
  (void) axi_registers[0];          // read  ISR - clear value 0x00000000
  (void) axi_registers[1];          // read  IER - reset value 0x00000000
  axi_registers[1] = 0x00000000;    // clear IER
  (void) axi_registers[3];          // read  TFV - reset value 0x000001fc
  (void) axi_registers[7];          // read  RFO - reset value 0x00000000
}


//--------------------------------------------------------------------
// setup Ethernet server to receive data
// - create, bind, listen and accept on socket
// - setup data stream (from socket)
//
// returns -1 if problems found
//--------------------------------------------------------------------
int setup_eth_srv (int eth_port) {
  struct sockaddr_in dir;
  struct sockaddr    client;
  socklen_t          client_size;
  int                socket_id;
  int                sckt_cli;


  // configure Ethernet environment
  memset(&dir, 0, sizeof (dir));
  dir.sin_port = eth_port;
  dir.sin_family = AF_INET;
  dir.sin_addr.s_addr = INADDR_ANY;

  // create socket
  socket_id = socket (AF_INET, SOCK_STREAM, 0);
  if (socket_id == -1) {
    return (-1);
  }

  // configure socket
  int idReuse = 1;
  if (setsockopt (socket_id, SOL_SOCKET, SO_REUSEADDR, &idReuse, sizeof (idReuse))==-1) {
    return (-1);
  }

  // bind socket
  if (bind (socket_id, (struct sockaddr *) &dir, sizeof (dir)) == -1) {
    close (socket_id);
    return (-1);
  }

  // start listening on socket
  if (listen (socket_id, 1) == -1) {
    close(socket_id);
    return (-1);
  }

  // accept data on socket
  client_size = sizeof (client);
  sckt_cli = accept (socket_id, &client, &client_size);
  if (sckt_cli == -1) {
    return (-1);
  }

  // open data stream for Ethernet READ
  data_strm = fdopen (sckt_cli, "r");
  if (data_strm == NULL) {
    printf ("error: cannot open raw event stream\n");
    return (-1);
  }

  return (0);
}


//--------------------------------------------------------------------
// get the next batch of data from stream
//
// returns 0 if no data available
//--------------------------------------------------------------------
int get_raw_data_batch () {
  return (fread (eth_data, sizeof (uint), DATA_BATCH_SIZE, data_strm));
}


//--------------------------------------------------------------------
// receives data from Ethernet and forwards it to SpiNNaker
//
// exits with -1 if problems found
//--------------------------------------------------------------------
int main (int argc, char * argv[])
{
  // auxiliary counters
  ulong total_data = 0;
  ulong wait_cycles = 0;

  // time-tracking variables
  struct timespec proc_first;
  struct timespec proc_cur;

  // check that required arguments were provided
  if (argc < 2) {
    printf ("usage: %s <port>\n", argv[0]);
    exit (-1);
  }

  // setup AXI memory-mapped access and drop privileges
  setup_axi ();

  // setup Ethernet server to receive data
  if (setup_eth_srv (atoi (argv[1])) == -1) {
    exit (-1);
  }

  // get initial processing timer value as reference
  clock_gettime(CLOCK_MONOTONIC, &proc_first);

  // iterate over the entire event file
  uint data_items;
  while (data_items = get_raw_data_batch ()) {
    // wait until AXI transmitter has room for data batch
    //NOTE: make sure not to wait forever!
    int wc = 0;
    while (axi_registers[3] < data_items) {
      wc++;
      if (wc < 0) {
	exit (-1);
      }
    }

    // write data batch to AXI transmitter
    for (uint i = 0; i < data_items; i++) {
      axi_tx_data[0] = eth_data[i];
    }

    // write length of data batch (in bytes!)
    axi_registers[5] = data_items * sizeof (uint);

    // stats: total event data items and wait cycles
    total_data += data_items;
    wait_cycles += wc;
  }

  // compute processing time
  clock_gettime (CLOCK_MONOTONIC, &proc_cur);
  long proc_dif = (proc_cur.tv_nsec - proc_first.tv_nsec) / PROC_TICK_NSEC;
  long pd2 = (proc_cur.tv_sec - proc_first.tv_sec) * (NSEC_PER_SEC / PROC_TICK_NSEC);
  proc_dif += pd2;

  // report total_data, wait cycles and elapsed processing ticks
  printf ("raw events  = %lu\n", total_data);
  printf ("wait cycles = %lu\n", wait_cycles);
  printf ("proc ticks  = %lu\n", proc_dif);

  fclose (data_strm);
  exit (0);
}
