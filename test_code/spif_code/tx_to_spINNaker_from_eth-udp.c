//************************************************//
//*                                              *//
//* transmit data from Ethernet to SpiNNaker     *//
//* ** transport Ethernet data over UDP          *//
//*                                              *//
//* - processing ticks measured in ms            *//
//*                                              *//
//* exits with -1 if problems found              *//
//*                                              *//
//* lap - 11/05/2021                             *//
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
  (void) axi_registers[SOF_ISR];          // read  ISR - reset value 0x01d00000
  axi_registers[SOF_ISR] = 0xffffffff;    // clear ISR
  (void) axi_registers[SOF_ISR];          // read  ISR - clear value 0x00000000
  (void) axi_registers[SOF_IER];          // read  IER - reset value 0x00000000
  axi_registers[SOF_IER] = 0x00000000;    // clear IER
  (void) axi_registers[SOF_TFV];          // read  TFV - reset value 0x000001fc
  (void) axi_registers[SOF_RFO];          // read  RFO - reset value 0x00000000
}


//--------------------------------------------------------------------
// setup Ethernet UDP server to receive data
// - create and bind on socket
//
// returns -1 if problems found
//--------------------------------------------------------------------
int setup_eth_udp_srv (int eth_port) {
  // create UDP socket
  int srv_sckt = socket (AF_INET, SOCK_DGRAM, 0);
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

  return (srv_sckt);
}


//--------------------------------------------------------------------
// get the next batch of data from Ethernet
//NOTE: may need the client address in the future
//
// returns 0 if no data available
//--------------------------------------------------------------------
int get_raw_data_batch (int srv_sckt, struct sockaddr_in * cli_addr) {

  size_t buf_len = DATA_BATCH_SIZE * sizeof (uint);

  socklen_t addr_len = sizeof (struct sockaddr);

  return (recvfrom (srv_sckt, (void *) eth_data, buf_len, 0,
                    (struct sockaddr *) cli_addr, &addr_len));
}


//--------------------------------------------------------------------
// receives data from Ethernet and forwards it to SpiNNaker
//
// exits with -1 if problems found
//--------------------------------------------------------------------
int main (int argc, char * argv[])
{
  // source-related variables
  struct sockaddr_in cli_addr;

  // event checks
  uint ts_err = 0;
  uint xc_err = 0;
  uint yc_err = 0;

  // auxiliary counters
  ulong total_items = 0;
  ulong wait_cycles = 0;

  // time-tracking variables
  struct timespec proc_first;
  struct timespec proc_cur;

  // check that required arguments were provided
  if (argc < 3) {
    printf ("usage: %s <port> <num_data_items_to_receive>\n", argv[0]);
    exit (-1);
  }

  // required arguments
  int port = atoi (argv[1]);
  ulong expected_items = atoi (argv[2]);

  // setup AXI memory-mapped access and drop privileges
  setup_axi ();

  // setup Ethernet UDP server to receive data
  int srv_sckt = setup_eth_udp_srv (port);
  if (srv_sckt == -1) {
    exit (-1);
  }

  // announce task
  printf ("receiving %lu data items on UDP port %d\n", expected_items, port);

  // get initial data batch to start time measurement
  int rcv_bytes = get_raw_data_batch (srv_sckt, &cli_addr);

  // get initial processing timer value as reference
  clock_gettime(CLOCK_MONOTONIC, &proc_first);

  // receive data from Ethernet
  do {
    // finish if no data received
    if (rcv_bytes <= 0) {
      break;
    }

    uint data_items = rcv_bytes / sizeof (uint);

    // wait until AXI transmitter has room for data batch
    //NOTE: make sure not to wait forever!
    int wc = 0;
    while (axi_registers[SOF_TFV] < data_items) {
      wc++;
      if (wc < 0) {
        exit (-1);
      }
    }

    // write data batch to AXI transmitter
    for (uint i = 0; i < data_items; i++) {
      uint data = eth_data[i];
      uint ts = data & 0x80000000;
      uint xc = (data & 0x7fff0000) >> 16;
      uint yc = data & 0x00007fff;

      // check for NO timestamp
      if (ts == 0) {
	ts_err++;
      }

      // check for sensible X coordinate
      if (xc > 640) {
	xc_err++;
      }

      // check for sensible Y coordinate
      if (yc > 640) {
	yc_err++;
      }
			   
      axi_tx_data[SOF_DWR] = data;
    }

    // write length of data batch (in bytes!)
    axi_registers[SOF_LEN] = data_items * sizeof (uint);

    // stats: total data items and wait cycles
    total_items += data_items;
    wait_cycles += wc;

    // get next data batch
    rcv_bytes = get_raw_data_batch (srv_sckt, &cli_addr);
  } while (total_items < expected_items);

  // compute processing time
  clock_gettime (CLOCK_MONOTONIC, &proc_cur);
  long proc_dif = (proc_cur.tv_nsec - proc_first.tv_nsec) / PROC_TICK_NSEC;
  long pd2 = (proc_cur.tv_sec - proc_first.tv_sec) * (NSEC_PER_SEC / PROC_TICK_NSEC);
  proc_dif += pd2;

  // report total_items, wait cycles and elapsed processing ticks
  printf ("data items  = %lu\n", total_items);
  printf ("wait cycles = %lu\n", wait_cycles);
  printf ("proc ticks  = %lu\n", proc_dif);

  // report event errors
  printf ("ts_err = %u\n", ts_err);
  printf ("xc_err = %u\n", xc_err);
  printf ("yc_err = %u\n", yc_err);

  exit (0);
}
