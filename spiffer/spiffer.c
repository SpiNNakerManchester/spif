//************************************************//
//*                                              *//
//*       spif UDP/USB/SPiNNaker listener        *//
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

// spif support
#include "spif_remote.h"

// spiffer constants and function prototypes
# include "spiffer.h"


//global variables
// signals
struct sigaction signal_cfg;

// threads
pthread_t       listener[SPIF_HW_PIPES_NUM];
pthread_t       out_listener[SPIF_HW_PIPES_NUM];
pthread_mutex_t usb_mtx = PTHREAD_MUTEX_INITIALIZER;

// spif pipe data
int pipe_num_in  = 0;
int pipe_num_out = 0;

int    pipe_fd[SPIF_HW_PIPES_NUM];
uint * pipe_buf[SPIF_HW_PIPES_NUM];
uint * pipe_out_buf[SPIF_HW_PIPES_NUM];

// SpiNNaker listener
FILE * of;

// UDP listener
int udp_skt[SPIF_HW_PIPES_NUM];

// USB listener
usb_devs_t usb_devs;


//--------------------------------------------------------------------
// stop spiffer
// caused by systemd request or error condition
//--------------------------------------------------------------------
void spiffer_stop (int ec) {
  // shutdown input pipes,
  for (int pipe = 0; pipe < pipe_num_in; pipe++) {
    // shutdown listener,
    (void) pthread_cancel (listener[pipe]);
    pthread_join (listener[pipe], NULL);

    // shutdown USB device,
    (void) caerDeviceDataStop (usb_devs.dev_hdl[pipe]);
    (void) caerDeviceClose (&usb_devs.dev_hdl[pipe]);

    // and close UDP port
    close (udp_skt[pipe]);
  }

  // shutdown output pipes,
  for (int pipe = 0; pipe < pipe_num_out; pipe++) {
    // shutdown listener,
    (void) pthread_cancel (out_listener[pipe]);
    pthread_join (out_listener[pipe], NULL);
  }

  // close all pipes,
  int pipe_max_num = (pipe_num_in >= pipe_num_out) ? pipe_num_in : pipe_num_out;
  for (int pipe = 0; pipe < pipe_max_num; pipe++) {
    // close spif pipe
    spif_close (pipe);
  }

  // say goodbye,
  fprintf (lf, "spiffer stopped\n");

  (void) fflush (lf);

  // and close log
  fclose (lf);

  exit (ec);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// setup SpiNNaker output through spif
//--------------------------------------------------------------------
int spiNNaker_init (void) {
  of = fopen ("/tmp/spiffer_out.txt", "w");
  if (of == NULL) {
    fprintf (lf, "error: cannot open output file\n");
    return (SPIFFER_ERROR);
  }

  // start SpiNNaker listeners
  for (int pipe = 0; pipe < pipe_num_out; pipe++) {
    (void) pthread_create (&out_listener[pipe], NULL,
			   spiNNaker_listener, (void *) pipe);
  }

  return (SPIFFER_OK);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// receive events from SpiNNaker through spif
//
// expects event to arrive in spif format - no mapping is done
//
// terminated as a result of signal servicing
//--------------------------------------------------------------------
void * spiNNaker_listener (void * data) {
  int pipe = (int) data;

  // announce that listener is ready
  fprintf (lf, "listening SpiNNaker -> outpipe%i\n", pipe);
  (void) fflush (lf);

  int    sp = pipe_fd[pipe];
  uint * sb = pipe_out_buf[pipe];
  size_t ss = SPIFFER_BATCH_SIZE * sizeof (uint);

  // get event batches from SpiNNaker
  while (1) {
    // trigger a transfer from SpiNNaker (blocking),
    int rcv_bytes = spif_get_output (sp, ss);

    // copy data to output file
    int num_events = rcv_bytes / sizeof (uint);
    for (int e = 0; e < num_events; e++) {
      fprintf (of, "0x%08x\n", sb[e]);
    }
    (void) fflush (of);
  }
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// setup Ethernet UDP servers to listen for events
// create and bind on socket
//
// returns SPIFFER_ERROR if problems found
//--------------------------------------------------------------------
int udp_init (void) {
  for (int pipe = 0; pipe < pipe_num_in; pipe++) {
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

    //  and map socket to pipe
    udp_skt[pipe] = skt;
  }

  // start UDP listeners
  for (int pipe = 0; pipe < pipe_num_in; pipe++) {
    (void) pthread_create (&listener[pipe], NULL, udp_listener, (void *) pipe);
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

  int    sp = pipe_fd[pipe];
  uint * sb = pipe_buf[pipe];
  size_t ss = SPIFFER_BATCH_SIZE * sizeof (uint);
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
// attempt to configure USB device
//
// returns SPIFFER_OK on success or SPIFFER_ERROR on error
//--------------------------------------------------------------------
int usb_dev_config (int pipe, caerDeviceHandle dh) {
  // send the default configuration before using the device
  bool rc = caerDeviceSendDefaultConfig (dh);

  if (!rc) {
    fprintf (lf, "error: failed to send camera default configuration\n");
    return (SPIFFER_ERROR);
  }

  // adjust number of events that a container packet can hold
  rc = caerDeviceConfigSet (dh, CAER_HOST_CONFIG_PACKETS,
                            CAER_HOST_CONFIG_PACKETS_MAX_CONTAINER_PACKET_SIZE,
                            USB_EVTS_PER_PKT
                            );

  if (!rc) {
    fprintf (lf, "error: failed to set camera container packet number\n");
    return (SPIFFER_ERROR);
  }

  // turn on data transmission
  rc = caerDeviceDataStart (dh, NULL, NULL, NULL, &usb_survey_devs, (void *) pipe);

  if (!rc) {
    fprintf (lf, "error: failed to start camera data transmission\n");
    return (SPIFFER_ERROR);
  }

  // set spiffer data reception to blocking mode
  rc = caerDeviceConfigSet (dh, CAER_HOST_CONFIG_DATAEXCHANGE,
                            CAER_HOST_CONFIG_DATAEXCHANGE_BLOCKING, true
                            );

  if (!rc) {
    fprintf (lf, "error: failed to set blocking mode\n");
    return (SPIFFER_ERROR);
  }

  return (SPIFFER_OK);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// sort USB devices by serial number
//
// no return value
//--------------------------------------------------------------------
void usb_sort (int ndv, serial_t * dvn, int * sorted) {
  // prepare to sort device serial numbers,
  for (int dv = 0; dv < ndv; dv++) {
    sorted[dv] = dv;
  }

  // straightforward bubble sort - small number of devices!
  for (int i = 0; i < (ndv - 1); i++) {
    for (int j = 0; j < (ndv - i - 1); j++) {
      if (strcmp (dvn[sorted[j]], dvn[sorted[j + 1]]) > 0) {
        // swap
        int temp      = sorted[j];
        sorted[j]     = sorted[j + 1];
        sorted[j + 1] = temp;
      }
    }
  }
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// attempt to discover new devices connected to the USB bus
// sort devices by serial number for consistent mapping to spif pipes
//
// discon_dev = known disconnected USB device, -1 for unknown
//
// returns the number of discovered devices (0 on error)
//--------------------------------------------------------------------
int usb_discover_devs (int discon_dev) {
  // number of discovered devices
  int ndd = 0;

  // number of devices with configuration attempts 
  int ncd = 0;

  // number of serviced devices
  int nsd = 0;

  // keep track of discovered device handles and serial numbers
  caerDeviceHandle dvh[USB_DISCOVER_CNT];
  serial_t         dvn[USB_DISCOVER_CNT];

  // shutdown listeners and connected USB devices
  for (int pipe = 0; pipe < pipe_num_in; pipe++) {
    // shutdown listener,
    (void) pthread_cancel (listener[pipe]);
    pthread_join (listener[pipe], NULL);

    // shutdown USB device,
    if (pipe != discon_dev) {
      (void) caerDeviceDataStop (usb_devs.dev_hdl[pipe]);
      (void) caerDeviceClose (&usb_devs.dev_hdl[pipe]);
      usb_devs.dev_hdl[pipe] = NULL;
    }
  }

  // start from a clean state
  usb_devs.dev_cnt = 0;
  int dev_id = 1;

  // discover devices by trying to open them
  while (ndd < USB_DISCOVER_CNT) {
    // try to open a new device and give it a device ID,
    //TODO: update for other device types
    caerDeviceHandle dh = caerDeviceOpen (dev_id, CAER_DEVICE_DAVIS, 0, 0, NULL);

    if (dh == NULL) {
      // no more devices available
      break;
    }

    // update device ID
    dev_id++;

    // remember device handle
    dvh[ndd] = dh;

    // remember device serial number
    struct caer_davis_info davis_info = caerDavisInfoGet (dh);
    (void) strcpy (dvn[ndd], davis_info.deviceSerialNumber);

    ndd++;
  }

  fprintf (lf, "discovered %i USB device%c\n", ndd, ndd == 1 ? ' ' : 's');

  // check if no devices found
  if (ndd == 0) {
    return (0);
  }

  // sort discovered devices by serial number,
  int sorted[USB_DISCOVER_CNT];
  usb_sort (ndd, dvn, sorted);

  // configure devices,
  while ((ncd < ndd) && (nsd < pipe_num_in)) {
    int sdv = sorted[ncd];

    if (usb_dev_config (sdv, dvh[ncd]) == SPIFFER_ERROR) {
      // on error close device,
      (void) caerDeviceClose (&dvh[ncd]);
    } else {
      // one more connected device
      nsd++;

      struct caer_davis_info davis_info = caerDavisInfoGet (dvh[ncd]);

      // report camera info
      fprintf (lf, "%s --- ID: %d, Master: %d, DVS X: %d, DVS Y: %d, Logic: %d.\n",
               davis_info.deviceString, davis_info.deviceID, davis_info.deviceIsMaster,
               davis_info.dvsSizeX, davis_info.dvsSizeY, davis_info.logicVersion
               );

      // update spiffer state
      usb_devs.dev_cnt++;
      usb_devs.dev_hdl[sdv] = dvh[ncd];
      (void) strcpy (usb_devs.dev_sn[sdv], dvn[ncd]);
    }

    // and move to next device
    ncd++;
  }

  // and close devices that will not be serviced
  for (int d = ncd; d < ndd; d++) {
    (void) caerDeviceClose (&dvh[d]);
  }

  return (nsd);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// survey USB devices
// called at start up and on USB device connection and disconnection
//
// data = known disconnected USB device, -1 for unknown
//
// no return value
//--------------------------------------------------------------------
void usb_survey_devs (void * data) {
  // grab the lock - keep other threads out
  pthread_mutex_lock (&usb_mtx);

  int discon_dev = (int) data;

  // try to discover supported USB devices
  int ndd = usb_discover_devs (discon_dev);

  // start USB listeners on discovered devices
  for (int pipe = 0; pipe < ndd; pipe++) {
    (void) pthread_create (&listener[pipe], NULL, usb_listener, (void *) pipe);
  }

  // and start UDP listeners on the rest of the pipes
  for (int pipe = ndd; pipe < pipe_num_in; pipe++) {
    (void) pthread_create (&listener[pipe], NULL, udp_listener, (void *) pipe);
  }

  // release the lock
  pthread_mutex_unlock (&usb_mtx);

  (void) fflush (lf);
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

  int              sp = pipe_fd[pipe];
  uint *           sb = pipe_buf[pipe];
  caerDeviceHandle ud = usb_devs.dev_hdl[pipe];

  fprintf (lf, "listening USB %s -> pipe%i\n", usb_devs.dev_sn[pipe], pipe);
  (void) fflush (lf);

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
  size_t batch_size = SPIFFER_BATCH_SIZE * sizeof (uint);

  // try to open spif pipe 0
  pipe_fd[0] = spif_open (0);
  if (pipe_fd[0] == SPIFFER_ERROR) {
    fprintf (lf, "error: unable to open spif pipe0\n");
    return (SPIFFER_ERROR);
  }

  // report hardware version number and number of event pipes
  int spif_ver = spif_read_reg (SPIF_VERSION);
  pipe_num_in  = (spif_ver & SPIF_PIPES_MSK) >> SPIF_PIPES_SHIFT;
  pipe_num_out = (spif_ver & SPIF_OUTPS_MSK) >> SPIF_OUTPS_SHIFT;
  fprintf (lf,
    "spif interface found [hw version %d.%d.%d - event pipes: in/%d out/%d]\n",
    (spif_ver & SPIF_MAJ_VER_MSK) >> SPIF_MAJ_VER_SHIFT,
    (spif_ver & SPIF_MIN_VER_MSK) >> SPIF_MIN_VER_SHIFT,
    (spif_ver & SPIF_PAT_VER_MSK) >> SPIF_PAT_VER_SHIFT,
    pipe_num_in,
    pipe_num_out
    );

  // check that hw and sw number of pipes are compatible
  int pipe_max_num = (pipe_num_in >= pipe_num_out) ? pipe_num_in : pipe_num_out;
  if (pipe_max_num > SPIF_HW_PIPES_NUM) {
    fprintf (lf, "error: incompatible number of hw and sw pipes\n");
    return (SPIFFER_ERROR);
  }

  // open the rest of the pipes
  for (int pipe = 1; pipe < pipe_max_num; pipe++) {
    // open spif pipe
    pipe_fd[pipe] = spif_open (pipe);
    if (pipe_fd[pipe] == SPIFFER_ERROR) {
      fprintf (lf, "error: unable to open spif pipe%i\n", pipe);
      return (SPIFFER_ERROR);
    }
  }

  // set up input buffers
  for (int pipe = 0; pipe < pipe_num_in; pipe++) {
    // get pipe buffer
    pipe_buf[pipe] = (uint *) spif_get_buffer (pipe, batch_size);
    if (pipe_buf[pipe] == NULL) {
      fprintf (lf, "error: failed to get buffer for spif pipe%i\n", pipe);
      return (SPIFFER_ERROR);
    }
  }

  // set up output buffers
  for (int pipe = 0; pipe < pipe_num_out; pipe++) {
    // get pipe buffer
    pipe_out_buf[pipe] = (uint *) spif_get_output_buffer (pipe, batch_size);
    if (pipe_out_buf[pipe] == NULL) {
      fprintf (lf, "error: failed to get output buffer for spif pipe%i\n", pipe);
      return (SPIFFER_ERROR);
    }
  }

  return (SPIFFER_OK);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// initialise system signal services
//
// returns SPIFFER_OK on success or SPIFFER_ERROR on error
//--------------------------------------------------------------------
int sig_init (void) {
  // set up signal servicing
  signal_cfg.sa_handler = &sig_service;
  signal_cfg.sa_flags = 0;
  sigemptyset (&signal_cfg.sa_mask);
  sigaddset   (&signal_cfg.sa_mask, SIGTERM);
  sigaddset   (&signal_cfg.sa_mask, SIGUSR1);
  sigaddset   (&signal_cfg.sa_mask, SIGUSR2);

  // register signal service routine
  if (sigaction (SIGUSR1,  &signal_cfg, NULL) == SPIFFER_ERROR) {
    return (SPIFFER_ERROR);
  }

  if (sigaction (SIGUSR2,  &signal_cfg, NULL) == SPIFFER_ERROR) {
    return (SPIFFER_ERROR);
  }

  return (sigaction (SIGTERM, &signal_cfg, NULL));
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// service system signals
//
// SIGUSR1 indicates that a USB device has been connected
// SIGUSR2 ignored (duplicate connection event)
// SIGTERM requests spiffer to stop
//
// no return value
//--------------------------------------------------------------------
void sig_service (int signum) {
  switch (signum) {
    case SIGUSR1:
      // need to survey USB devices
      usb_survey_devs ((void *) -1);
      break;

    case SIGUSR2:
      // ignore bind events
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
  fprintf (lf, "spiffer v%u.%u.%u started\n",
	   SPIFFER_VER_MAJ, SPIFFER_VER_MIN, SPIFFER_VER_PAT);

  // open spif pipes and set up buffers,
  if (spif_pipes_init () == SPIFFER_ERROR) {
    spiffer_stop (SPIFFER_ERROR);
  }

  // set up SpiNNaker listeners - one for every output pipe,
  if (spiNNaker_init () == SPIFFER_ERROR) {
    spiffer_stop (SPIFFER_ERROR);
  }

  // set up UDP servers - one for every pipe,
  if (udp_init () == SPIFFER_ERROR) {
    spiffer_stop (SPIFFER_ERROR);
  }

  // set up initial USB state and survey USB devices,
  if (usb_init () == SPIFFER_ERROR) {
    spiffer_stop (SPIFFER_ERROR);
  }

  // configure system signal service,
  if (sig_init () == SPIFFER_ERROR) {
    spiffer_stop (SPIFFER_ERROR);
  }

  // update log,
  (void) fflush (lf);

  // and go to sleep - let the listeners do the work
  while (1) {
    pause ();
  }
}
//--------------------------------------------------------------------
