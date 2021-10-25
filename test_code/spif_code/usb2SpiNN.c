//************************************************//
//*                                              *//
//* transfer data from USB to SpiNNaker          *//
//*                                              *//
//* - processing ticks measured in ms            *//
//*                                              *//
//* exits with -1 if problems found              *//
//*                                              *//
//* lap - 10/09/2021                             *//
//*                                              *//
//************************************************//

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <libgen.h>

#include <libcaer/libcaer.h>
#include <libcaer/devices/davis.h>

#include "spif_remote.h"


#define NSEC_PER_SEC      1000000000

#define PROC_TICK_NSEC    1000000

#define DATA_BATCH_SIZE   256

#define EVTS_PER_PACKET   256

//NOTE: made global to avoid unnecessary passing
//      around as function parameters
static caerDeviceHandle camera;


//--------------------------------------------------------------------
// returns -1 if problems found
//--------------------------------------------------------------------
int usb_setup (void) {

  // open a DAVIS camera and give it a device ID of 1
  //NOTE: don't care about USB bus or SN restrictions
  camera = caerDeviceOpen (1, CAER_DEVICE_DAVIS, 0, 0, NULL);
  if (camera == NULL) {
    printf ("error: failed to get event camera\n");
    return (-1);
  }

  // report camera info
  struct caer_davis_info davis_info = caerDavisInfoGet (camera);
  printf ("%s --- ID: %d, Master: %d, DVS X: %d, DVS Y: %d, Logic: %d.\n",
	  davis_info.deviceString, davis_info.deviceID, davis_info.deviceIsMaster,
	  davis_info.dvsSizeX, davis_info.dvsSizeY, davis_info.logicVersion
	  );

  // send the default configuration before using the device
  //NOTE: no configuration is sent automatically!
  caerDeviceSendDefaultConfig (camera);

  // check initial configuration values
  uint config_val;
  caerDeviceConfigGet (camera, CAER_HOST_CONFIG_PACKETS,
    CAER_HOST_CONFIG_PACKETS_MAX_CONTAINER_PACKET_SIZE, &config_val
    );

  caerDeviceConfigGet (camera, CAER_HOST_CONFIG_PACKETS,
    CAER_HOST_CONFIG_PACKETS_MAX_CONTAINER_INTERVAL, &config_val
    );

  caerDeviceConfigGet (camera, CAER_HOST_CONFIG_DATAEXCHANGE,
    CAER_HOST_CONFIG_DATAEXCHANGE_BUFFER_SIZE, &config_val
    );

  // adjust number of events that a container packet can hold
  bool rc = caerDeviceConfigSet (camera, CAER_HOST_CONFIG_PACKETS,
    CAER_HOST_CONFIG_PACKETS_MAX_CONTAINER_PACKET_SIZE, EVTS_PER_PACKET
    );

  if (!rc) {
    printf ("error: failed to set camera container packet number\n");
    return (-1);
  }

  caerDeviceConfigGet (camera, CAER_HOST_CONFIG_PACKETS,
    CAER_HOST_CONFIG_PACKETS_MAX_CONTAINER_PACKET_SIZE, &config_val
    );

  // turn on data sending 
  caerDeviceDataStart (camera, NULL, NULL, NULL, NULL, NULL);

  // set data transmission to blocking mode
  caerDeviceConfigSet (camera, CAER_HOST_CONFIG_DATAEXCHANGE,
    CAER_HOST_CONFIG_DATAEXCHANGE_BLOCKING, true
  );

  return (0);
}


//--------------------------------------------------------------------
// get the next batch of data from USB
//
// returns 0 if no data available
//--------------------------------------------------------------------
int usb_get_data_batch (uint * buf) {
  // keep track of the number of valid events received
  uint evt_num = 0;

  while (1) {
    caerEventPacketContainer packetContainer = caerDeviceDataGet (camera);
    if (packetContainer == NULL) {
      continue;
    }

    // only interested in 'polarity' events
    caerPolarityEventPacket polarity_packet = (caerPolarityEventPacket)
      caerEventPacketContainerGetEventPacket (packetContainer, POLARITY_EVENT);

    if (polarity_packet == NULL) {
      continue;
    }

    // process all events in the packet
    uint evts_in_pkt = caerEventPacketHeaderGetEventNumber(&(polarity_packet)->packetHeader);
    for (uint i = 0; i < evts_in_pkt; i++) {
      // get event polarity and coordinates
      caerPolarityEventConst event = caerPolarityEventPacketGetEventConst (polarity_packet, i);
      if (!caerPolarityEventIsValid (event)) {
        continue;
      }

      bool     pol = caerPolarityEventGetPolarity (event);
      uint16_t x   = caerPolarityEventGetX (event);
      uint16_t y   = caerPolarityEventGetY (event);

      // format event and store in buffer
      buf[evt_num++] = 0x80000000 | (x << 16) | (pol << 15) | y;
    }

    // release packet container
    caerEventPacketContainerFree (packetContainer);

    if (evt_num) {
      return (evt_num);
    }
  }
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
  if (argc < 3) {
    printf ("usage: %s <event_pipe> <num_data_items_to_receive/0 = unlimited>\n",
	    cname);
    exit (-1);
  }

  // required arguments
  uint  pipe           = atoi (argv[1]);
  ulong expected_items = atoi (argv[2]);

  // setup USB to receive data
  if (usb_setup () == -1) {
    printf ("%s: failed to setup USB connection\n", cname);
    exit (-1);
  }

  // get pointer to spif buffer
  size_t const buf_size = DATA_BATCH_SIZE * sizeof (uint);
  uint * spif_buffer = (uint *) spif_setup (pipe, buf_size);
  if (spif_buffer == NULL) {
    printf ("%s: unable to access spif\n", cname);
    exit (-1);
  }

  // announce task
  if (expected_items) {
    printf ("receiving %lu data items from USB device\n", expected_items);
  } else {
    printf ("receiving unlimited data items from USB device <ctrl-c to exit>\n");
  }

  // statistics counters
  ulong total_items = 0;
  ulong wait_cycles = 0;

  // time-tracking variables
  struct timespec proc_first;
  struct timespec proc_cur;

  // wait for initial data batch to start time measurement
  int rcv_items = usb_get_data_batch (spif_buffer);

  // get initial processing timer value as reference
  clock_gettime(CLOCK_MONOTONIC, &proc_first);

  // iterate to receive data from Ethernet
  while (!expected_items || (total_items < expected_items)) {
    // finish if no data received
    if (rcv_items <= 0) {
      break;
    }

    // trigger a transfer to SpiNNaker
    //NOTE: length of transfer in bytes!
    spif_transfer (rcv_items * sizeof (uint));

    // stats: total data items received
    total_items += rcv_items;

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
    rcv_items = usb_get_data_batch (spif_buffer);
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
