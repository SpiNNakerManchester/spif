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
pthread_t      listener[SPIF_NUM_PIPES];
pthread_attr_t attr;

// spif
int    spif_pipe[SPIF_NUM_PIPES];
uint * spif_buf[SPIF_NUM_PIPES];

// UDP listener
int pipe_skt[SPIF_NUM_PIPES];

// USB listener
usb_devs_t usb_devs;
usb_devs_t usb_devs_nxt;

// log file
//TODO: change to system/kernel log
FILE * lf;


//--------------------------------------------------------------------
// stop spiffer
// caused by systemd request or error condition
//
// returns -1 on error condition, 0 otherwise
//--------------------------------------------------------------------
void spiffer_stop (int ec) {
  // shutdown spif pipes,
  for (int pipe = 0; pipe < SPIF_NUM_PIPES; pipe++) {
    // cancel thread,
    pthread_cancel (listener[pipe]);

    // close UDP port,
    close (pipe_skt[pipe]);
    fprintf (lf, "UDP port %i closed\n", UDP_PORT_BASE + pipe);

    // and close spif pipe
    spif_close (spif_pipe[pipe]);
    fprintf (lf, "spif pipe%i closed\n", pipe);
  }

  // close USB devices,
  for (uint pipe = 0; pipe < usb_devs.dev_cnt; pipe++) {
    caerDeviceClose (&usb_devs.dev_hdl[pipe]);
  }

  // say goodbye,
  fprintf (lf, "spiffer stopped\n");

  // and close log
  fclose (lf);

  exit (ec);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// setup Ethernet UDP server to listen for events
// create and bind on socket
//
// returns -1 if problems found
//--------------------------------------------------------------------
int udp_srv_setup (int eth_port) {
  // create UDP socket,
  int skt = socket (AF_INET, SOCK_DGRAM, 0);
  if (skt == -1) {
    return (-1);
  }

  // configure server,
  struct sockaddr_in srv_addr;
  srv_addr.sin_family      = AF_INET;
  srv_addr.sin_port        = htons (eth_port);
  srv_addr.sin_addr.s_addr = INADDR_ANY;
  bzero (&(srv_addr.sin_zero), 8);

  // and bind socket
  if (bind (skt, (struct sockaddr *) &srv_addr, sizeof (struct sockaddr)) == -1) {
    close (skt);
    return (-1);
  }

  return (skt);
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
  int    ps = pipe_skt[pipe];

  // get event batches from UDP port and send them to spif
  while (1) {
    // get next batch of events (blocking),
    int rcv_bytes = recv (ps, (void *) sb, ss, 0);

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
// attempt to discover CAER devices connected to the USB bus
//
// returns the number of devices or -1 on detection error
//--------------------------------------------------------------------
int usb_discover (void) {
  caerDeviceDiscoveryResult devices;
  ssize_t result = caerDeviceDiscover(CAER_DEVICE_DISCOVER_ALL, &devices);
  if (result < 0) {
    fprintf (lf, "error detecting Inivation cameras\n");
    (void) fflush (lf);
    free (devices);
    return (-1);
  }

  // open a DAVIS camera and give it a device ID of 1
  //NOTE: don't care about USB bus or SN restrictions
  caerDeviceHandle camera = caerDeviceOpen (1, CAER_DEVICE_DAVIS, 0, 0, NULL);
  if (camera == NULL) {
    fprintf (lf, "error: failed to open event camera\n");
    (void) fflush (lf);
    free (devices);
    return (-1);
  }

  // report camera info
  struct caer_davis_info davis_info = caerDavisInfoGet (camera);
  fprintf (lf, "%s --- ID: %d, Master: %d, DVS X: %d, DVS Y: %d, Logic: %d.\n",
          davis_info.deviceString, davis_info.deviceID, davis_info.deviceIsMaster,
          davis_info.dvsSizeX, davis_info.dvsSizeY, davis_info.logicVersion
          );

  // update spiffer state
  usb_devs.dev_hdl[0] = camera;
  (void) strcpy (usb_devs.dev_sn[0], davis_info.deviceSerialNumber);

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
    CAER_HOST_CONFIG_PACKETS_MAX_CONTAINER_PACKET_SIZE, USB_EVTS_PER_PKT
    );

  if (!rc) {
    fprintf (lf, "error: failed to set camera container packet number\n");
    (void) fflush (lf);
    free (devices);
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

  (void) fflush (lf);
  free (devices);
  return (1);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// add new USB device
//
// returns -1 on error condition, 0 otherwise
//--------------------------------------------------------------------
void usb_add_dev (void) {
  fprintf (lf, "USB device connected\n");

  switch (usb_devs.dev_cnt) {
    case 0:
      if (usb_discover () > 0) {
        // terminate UDP listener
        pthread_cancel (listener[0]);

        // start USB listener
        (void) pthread_create (&listener[0], &attr, usb_listener, (void *) 0);

        // update number of connected USB devices
        usb_devs.dev_cnt++;
      }
      break;

    default:
      fprintf (lf, "warning: ignoring USB connect - too many devices\n");
  }

  (void) fflush (lf);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// remove USB device
//
// returns -1 on error condition, 0 otherwise
//--------------------------------------------------------------------
void usb_rm_dev (caerDeviceHandle dev) {
  fprintf (lf, "USB device disconnected\n");

  // which pipe is dev attached to?
  //TODO: add discovery when dev == NULL
  uint pipe = 0;
  if (dev != NULL) {
    while ((pipe < usb_devs.dev_cnt) && (dev != usb_devs.dev_hdl[pipe])) {
      pipe++;
    }
  }

  switch (usb_devs.dev_cnt) {
    case 0:
      fprintf (lf, "warning: ignoring USB disconnect - no devices\n");
      break;

    case 1:
      // close USB device
      caerDeviceClose (&dev);

      // terminate USB listener
      pthread_cancel (listener[pipe]);

      // start UDP listener
      (void) pthread_create (&listener[pipe], &attr, udp_listener, (void *) pipe);

      // update number of connected USB devices
      usb_devs.dev_cnt--;
      break;

    default:
      fprintf (lf, "warning: ignoring USB disconnect - too many devices\n");
  }

  (void) fflush (lf);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// geta batch of events from USB device
// map them to spif events with format:
//
//    [31] timestamp (0: present, 1: absent)
// [30:16] 15-bit x coordinate
//    [15] polarity
//  [14:0] 15-bit y coordinate
//
// Currently, timestamps are not supported - time models itself
//
// returns 0 if no data available
//--------------------------------------------------------------------
int usb_get_events (caerDeviceHandle dev, uint * buf) {
  // keep track of the number of valid events received
  uint evt_num = 0;

  // count consecutive empty containers to detect device disconnection
  uint empty_cnt = 0;

  while (1) {
    caerEventPacketContainer packetContainer = caerDeviceDataGet (dev);
    if (packetContainer == NULL) {
      empty_cnt++;

      // too many consecutive empty containers suggest device disconnection
      if (empty_cnt > USB_DISCONN_CNT) {
        // remove "disconnected" device
        usb_rm_dev (dev);
      }

      continue;
    }

    // non-empty container - restart empty count
    empty_cnt = 0;

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
// service system signals
//
// SIGUSR1 indicates that a USB device has been connected
// SIGUSR2 indicates that a USB device was disconnected
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

    case SIGUSR2:
      // USB device disconnected
      usb_rm_dev (USB_DEV_DISCOVER);
      break;

    default:
      // spiffer shut down requested
      spiffer_stop (0);
  }
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// opens spif pipes
// sets up a UDP server per spif pipe
// creates a listener thread per spif pipe
// configures signal management
//--------------------------------------------------------------------
int main (int argc, char *argv[])
{
  (void) argc;
  (void) argv;

  // open log file
  //TODO: needs a permanent home
  lf = fopen ("/tmp/spiffer.log", "a");
  if (lf == NULL) {
    spiffer_stop (-1);
  }

  // say hello
  fprintf (lf, "spiffer started\n");

  // set up thread attributes
  (void) pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);

  // initialise USB device count
  usb_devs.dev_cnt = 0;

  // spif buffer size
  size_t batch_size = SPIF_BATCH_SIZE * sizeof (uint);

  // open spif pipes and get corresponding buffer
  for (int pipe = 0; pipe < SPIF_NUM_PIPES; pipe++) {
    // open spif pipe
    spif_pipe[pipe] = spif_open (pipe);
    if (spif_pipe[pipe] == -1) {
      fprintf (lf, "error: unable to open spif pipe%i\n", pipe);
      spiffer_stop (-1);
    }

    // get pipe buffer
    spif_buf[pipe] = (uint *) spif_get_buffer (spif_pipe[pipe], batch_size);
    if (spif_buf[pipe] == NULL) {
      fprintf (lf, "error: failed to get buffer for spif pipe%i\n", pipe);
      spiffer_stop (-1);
    }

    fprintf (lf, "spif pipe%i opened\n", pipe);

    // setup Ethernet UDP servers to receive events
    pipe_skt[pipe] = udp_srv_setup (UDP_PORT_BASE + pipe);
    if (pipe_skt[pipe] == -1) {
      fprintf (lf, "error: failed to listen on UDP port %i\n", UDP_PORT_BASE + pipe);
      spiffer_stop (-1);
    }

    // start a thread for every spif pipe
    //NOTE: always start with UDP listeners, which can be active at any time
    (void) pthread_create (&listener[pipe], &attr, udp_listener, (void *) pipe);
  }

  // set up signal servicing
  signal_cfg.sa_flags = 0;
  signal_cfg.sa_handler = &sig_service;

  // register signal service routine
  sigaction(SIGINT,  &signal_cfg, NULL);
  sigaction(SIGUSR1, &signal_cfg, NULL);
  sigaction(SIGUSR2, &signal_cfg, NULL);

  // update log
  (void) fflush (lf);

  // go to sleep and let the listeners do the work
  while (1) {
    pause ();
  }
}
//--------------------------------------------------------------------
