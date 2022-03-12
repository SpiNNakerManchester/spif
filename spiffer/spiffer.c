//************************************************//
//*                                              *//
//*          spif UDP/USB listener               *//
//*                                              *//
//* started automatically at boot time           *//
//* exits with -1 if problems found              *//
//*                                              *//
//* lap - 09/03/2022                             *//
//*                                              *//
//************************************************//

#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Inivation camera support
#include <libcaer/libcaer.h>
#include <libcaer/devices/davis.h>
#include <libcaer/devices/device_discover.h>

// spif support
#include "spif_remote.h"

// spiffer onstants and function prototypes
# include "spiffer.h"


//global variables
// signals
struct sigaction signal_cfg;

// threads
pthread_t listener[SPIF_NUM_PIPES];

// spif
int    spif_pipe[SPIF_NUM_PIPES];
uint * spif_buf[SPIF_NUM_PIPES];

// UDP listener
int udp_skt[SPIF_NUM_PIPES];

// USB listener
usb_devs_t usb_devs;

// log file
//TODO: change to system/kernel log
FILE * lf;


//--------------------------------------------------------------------
// stop spiffer
// caused by systemd request or error condition
//--------------------------------------------------------------------
void spiffer_stop (int ec) {
  // shutdown spif pipes,
  for (int pipe = 0; pipe < SPIF_NUM_PIPES; pipe++) {
    // cancel thread,
    (void) pthread_cancel (listener[pipe]);
    pthread_join (listener[pipe], NULL);

    // close UDP port,
    close (udp_skt[pipe]);
    fprintf (lf, "UDP port %i closed\n", UDP_PORT_BASE + pipe);

    // close USB device,
    (void) caerDeviceClose (&usb_devs.dev_hdl[pipe]);

    // and close spif pipe
    spif_close (spif_pipe[pipe]);
    fprintf (lf, "spif pipe%i closed\n", pipe);
  }

  // say goodbye,
  fprintf (lf, "spiffer stopped\n");

  // and close log
  fclose (lf);

  exit (ec);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// setup Ethernet UDP servers to listen for events
// create and bind on socket
//
// returns SPIFFER_ERROR if problems found
//--------------------------------------------------------------------
int udp_init (void) {
  for (int pipe = 0; pipe < SPIF_NUM_PIPES; pipe++) {
    int eth_port = UDP_PORT_BASE + pipe;

    // create UDP socket,
    int skt = socket (AF_INET, SOCK_DGRAM, 0);
    if (skt == SPIFFER_ERROR) {
      fprintf (lf, "error: failed to create socket for UDP port %i\n", eth_port);
      return (SPIFFER_ERROR);
    }

    // configure server,
    struct sockaddr_in srv_addr;
    srv_addr.sin_family      = AF_INET;
    srv_addr.sin_port        = htons (eth_port);
    srv_addr.sin_addr.s_addr = INADDR_ANY;
    bzero (&(srv_addr.sin_zero), 8);

    // bind socket,
    if (bind (skt, (struct sockaddr *) &srv_addr, sizeof (struct sockaddr)) == SPIFFER_ERROR) {
      close (skt);
      fprintf (lf, "error: failed to bind socket for UDP port %i\n", eth_port);
      return (SPIFFER_ERROR);
    }

    // and map socket to pipe
    udp_skt[pipe] = skt;
  }

  return (SPIFFER_OK);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// receive events through Ethernet UDP port and forward them to spif
//
// expects event to arrive in spif format - no mapping is done
//
// terminated as a result of signal servicing
//--------------------------------------------------------------------
void * udp_listener (void * data) {
  int pipe = (int) data;

  // announce that listener is ready
  fprintf (lf, "listening UDP %i-> pipe%i\n", UDP_PORT_BASE + pipe, pipe);
  (void) fflush (lf);

  int    sp = spif_pipe[pipe];
  uint * sb = spif_buf[pipe];
  size_t ss = SPIF_BATCH_SIZE * sizeof (uint);
  int    us = udp_skt[pipe];

  // get event batches from UDP port and send them to spif
  while (1) {
    // get next batch of events (blocking),
    //NOTE: this is a thread cancellation point
    int rcv_bytes = recv (us, (void *) sb, ss, 0);

    // trigger a transfer to SpiNNaker,
    spif_transfer (sp, rcv_bytes);

    // and wait until spif finishes the transfer
    //NOTE: report if waiting for too long!
    int wc = 0;
    while (spif_busy (sp)) {
      wc++;
      if (wc < 0) {
        fprintf (lf, "error: spif not responding\n");
        (void) fflush (lf);
        wc = 0;
      }
    }
  }
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// attempt to open and configure USB device
//
// returns SPIFFER_OK on success SPIFFER_ERROR on error
//--------------------------------------------------------------------
int usb_dev_config (int pipe) {
  // open device and give it a device ID
  //TODO: update for other device types
  caerDeviceHandle camera = caerDeviceOpen (pipe + 1, CAER_DEVICE_DAVIS, 0, 0, NULL);
  if (camera == NULL) {
    fprintf (lf, "error: failed to open event camera\n");
    (void) fflush (lf);
    return (SPIFFER_ERROR);
  }

  // report camera info
  struct caer_davis_info davis_info = caerDavisInfoGet (camera);
  fprintf (lf, "%s --- ID: %d, Master: %d, DVS X: %d, DVS Y: %d, Logic: %d.\n",
           davis_info.deviceString, davis_info.deviceID, davis_info.deviceIsMaster,
           davis_info.dvsSizeX, davis_info.dvsSizeY, davis_info.logicVersion
           );
  (void) fflush (lf);

  // send the default configuration before using the device
  bool rc = caerDeviceSendDefaultConfig (camera);

  if (!rc) {
    fprintf (lf, "error: failed to send camera default configuration\n");
    (void) fflush (lf);
    return (SPIFFER_ERROR);
  }

  // adjust number of events that a container packet can hold
  rc = caerDeviceConfigSet (camera, CAER_HOST_CONFIG_PACKETS,
                            CAER_HOST_CONFIG_PACKETS_MAX_CONTAINER_PACKET_SIZE,
                            USB_EVTS_PER_PKT
                            );

  if (!rc) {
    fprintf (lf, "error: failed to set camera container packet number\n");
    (void) fflush (lf);
    return (SPIFFER_ERROR);
  }

  // turn on data sending
  caerDeviceDataStart (camera, NULL, NULL, NULL, &usb_rm_dev, (void *) pipe);

  fprintf (lf, "started USB device 0x%08x\n", (uint) pipe);
  (void) fflush (lf);

  // set data transmission to blocking mode
  caerDeviceConfigSet (camera, CAER_HOST_CONFIG_DATAEXCHANGE,
                       CAER_HOST_CONFIG_DATAEXCHANGE_BLOCKING, true
                       );

  // update spiffer state
  usb_devs.dev_hdl[pipe] = camera;
  (void) strcpy (usb_devs.dev_sn[pipe], davis_info.deviceSerialNumber);

  (void) fflush (lf);
  return (SPIFFER_OK);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// attempt to discover new supported devices connected to the USB bus
//
// returns the number of discovered devices or SPIFFER_ERROR on error
//--------------------------------------------------------------------
/* //lap int usb_discover_new (void) {
  caerDeviceDiscoveryResult devices;

  ssize_t dev_cnt = caerDeviceDiscover(CAER_DEVICE_DISCOVER_ALL, &devices);

  // check for errors in the discovery
  if (dev_cnt < 0) {
    fprintf (lf, "error detecting new USB devices\n");
    (void) fflush (lf);
    free (devices);
    return (SPIFFER_ERROR);
  }

  // check if no supported devices found
  if (dev_cnt == 0) {
    fprintf (lf, "ignoring unsupported USB devices\n");
    (void) fflush (lf);
    free (devices);
    return (0);
  }

  fprintf (lf, "discovered %i USB device%c\n", dev_cnt, dev_cnt > 1 ? 's' : '\0');
  (void) fflush (lf);

  // bubble-sort device serial numbers
  //TODO: fix
  if (dev_cnt > 1) {
    int * sn_order;
    sn_order = (int *) malloc (dev_cnt * sizeof (int));
    if (sn_order == NULL) {
      fprintf (lf, "error detecting Inivation cameras\n");
      (void) fflush (lf);
      free (devices);
      return (SPIFFER_ERROR);
    }

    sn_order[0] = 0;
    for (int dv = 1; dv < dev_cnt; dv++) {
      continue;
    }
  }

  // limit the number of discovered devices that will be serviced
  if (dev_cnt > (SPIF_NUM_PIPES - usb_devs.dev_cnt)) {
    dev_cnt = SPIF_NUM_PIPES - usb_devs.dev_cnt;
  }

  free (devices);
  return (dev_cnt);
  }*/ //lap
int usb_discover_new (void) {
  if (usb_devs.dev_cnt >= SPIF_NUM_PIPES) {
      fprintf (lf, "warning: too many USB devices\n");
      (void) fflush (lf);
      return (SPIFFER_ERROR);
  }

  fprintf (lf, "adding 1 USB device\n");

  // open and configure new device
  int rc = usb_dev_config (usb_devs.dev_cnt);

  (void) fflush (lf);
  return (rc == SPIFFER_OK ? 1 : rc);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// add new USB device
//
// no return value
//--------------------------------------------------------------------
void usb_add_dev (void) {
  fprintf (lf, "USB device connected\n");
  (void) fflush (lf);

  // discover new supported USB devices
  int ndd = usb_discover_new ();

  // process discovered devices
  for (int dv = 0; dv < ndd; dv++) {
    // pipe to be used for device
    uint pipe = usb_devs.dev_cnt + dv;

    // terminate current listener,
    pthread_cancel (listener[pipe]);
    pthread_join (listener[pipe], NULL);

    // and start new USB listener
    (void) pthread_create (&listener[pipe], NULL, usb_listener, (void *) pipe);
  }

  // update number of connected devices
  usb_devs.dev_cnt += ndd;
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// remove USB device
//
// no return value
//--------------------------------------------------------------------
void usb_rm_dev (void * data) {

  fprintf (lf, "USB device %u disconnected\n", (uint) data);
  (void) fflush (lf);

  int pipe = (int) data;

  if (pipe < SPIF_NUM_PIPES) {
    // terminate USB listener
    pthread_cancel (listener[pipe]);
    pthread_join (listener[pipe], NULL);

    // start UDP listener
    (void) pthread_create (&listener[pipe], NULL, udp_listener, (void *) pipe);

    // update number of connected USB devices
    usb_devs.dev_cnt--;
  }
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// get a batch of events from USB device
// map them to spif events with format:
//
//    [31] timestamp (0: present, 1: absent)
// [30:16] 15-bit x coordinate
//    [15] polarity
//  [14:0] 15-bit y coordinate
//
// timestamps are ignored - time models itself
//
// returns the number of events in the batch
//--------------------------------------------------------------------
int usb_get_events (caerDeviceHandle dev, uint * buf) {
  // keep track of the number of valid events received
  uint evt_num = 0;

  while (1) {
    // this is a safe place to cancel this thread
    pthread_testcancel ();

    // get events from USB device
    caerEventPacketContainer packetContainer = caerDeviceDataGet (dev);
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


//--------------------------------------------------------------------
// receive events from a USB device and forward them to spif
//
// terminated as a result of signal servicing
//--------------------------------------------------------------------
void * usb_listener (void * data) {
  int pipe = (int) data;

  fprintf (lf, "listening USB -> pipe%i\n", pipe);
  (void) fflush (lf);

  int              sp = spif_pipe[pipe];
  uint *           sb = spif_buf[pipe];
  caerDeviceHandle ud = usb_devs.dev_hdl[pipe];

  while (1) {
    // get next batch of events
    //NOTE: blocks until events are available
    int rcv_bytes = usb_get_events (ud, sb);

    // trigger a transfer to SpiNNaker
    spif_transfer (sp, rcv_bytes);

    // wait until spif finishes the current transfer
    //NOTE: report when waiting too long!
    int wc = 0;
    while (spif_busy (sp)) {
      wc++;
      if (wc < 0) {
        fprintf (lf, "warning: spif not responding\n");
        (void) fflush (lf);
        wc = 0;
      }
    }
  }
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// initialise USB device state
//
// returns SPIFFER_OK on success or SPIFFER_ERROR on error
//--------------------------------------------------------------------
int usb_init (void) {
  // initialise USB device count
  usb_devs.dev_cnt = 0;

  return (SPIFFER_OK);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// initialise spif pipes
//
// returns SPIFFER_OK on success or SPIFFER_ERROR on error
//--------------------------------------------------------------------
int spif_pipes_init (void) {
  // spif data batch size - also buffer size
  size_t batch_size = SPIF_BATCH_SIZE * sizeof (uint);

  // open spif pipes and get corresponding buffer
  for (int pipe = 0; pipe < SPIF_NUM_PIPES; pipe++) {
    // open spif pipe
    spif_pipe[pipe] = spif_open (pipe);
    if (spif_pipe[pipe] == SPIFFER_ERROR) {
      fprintf (lf, "error: unable to open spif pipe%i\n", pipe);
      return (SPIFFER_ERROR);
    }

    // get pipe buffer
    spif_buf[pipe] = (uint *) spif_get_buffer (spif_pipe[pipe], batch_size);
    if (spif_buf[pipe] == NULL) {
      fprintf (lf, "error: failed to get buffer for spif pipe%i\n", pipe);
      return (SPIFFER_ERROR);
    }

    fprintf (lf, "spif pipe%i opened\n", pipe);
  }

  return (SPIFFER_OK);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// initialise system signal services
//
// no return value
//--------------------------------------------------------------------
void sig_init (void) {
  // set up signal servicing
  signal_cfg.sa_flags = 0;
  signal_cfg.sa_handler = &sig_service;

  // register signal service routine
  sigaction(SIGINT,  &signal_cfg, NULL);
  sigaction(SIGUSR1, &signal_cfg, NULL);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// service system signals
//
// SIGUSR1 indicates that a USB device has been connected
// SIGINT  requests spiffer to stop
//
// no return value
//--------------------------------------------------------------------
void sig_service (int signum) {
  switch (signum) {
    case SIGUSR1:
      // new USB device connected
      usb_add_dev ();
      break;

    default:
      // spiffer shut down requested
      spiffer_stop (SPIFFER_OK);
  }
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// main thread:
// opens spif pipes
// sets up a UDP server per spif pipe
// creates a listener thread per spif pipe
// configures signal management
//--------------------------------------------------------------------
int main (int argc, char *argv[])
{
  (void) argc;
  (void) argv;

  // open log file,
  //TODO: needs a permanent home
  lf = fopen ("/tmp/spiffer.log", "a");
  if (lf == NULL) {
    spiffer_stop (SPIFFER_ERROR);
  }

  // say hello,
  fprintf (lf, "spiffer started\n");

  // open spif pipes and set up buffers,
  if (spif_pipes_init () == SPIFFER_ERROR) {
    spiffer_stop (SPIFFER_ERROR);
  }

  // set up UDP servers - one for every pipe,
  if (udp_init () == SPIFFER_ERROR) {
    spiffer_stop (SPIFFER_ERROR);
  }

  // set up USB status,
  if (usb_init () == SPIFFER_ERROR) {
    spiffer_stop (SPIFFER_ERROR);
  }

  // start a thread for every spif pipe,
  for (int pipe = 0; pipe < SPIF_NUM_PIPES; pipe++) {
    // always start with UDP listeners, which can be active at any time
    (void) pthread_create (&listener[pipe], NULL, udp_listener, (void *) pipe);
  }

  // configure system signal service,
  sig_init ();

  // update log,
  (void) fflush (lf);

  // and go to sleep - let the listeners do the work
  while (1) {
    pause ();
  }
}
//--------------------------------------------------------------------
