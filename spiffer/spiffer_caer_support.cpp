//************************************************//
//*                                              *//
//*            spiffer libcaer support           *//
//*                                              *//
//* lap - 11/08/2023                             *//
//*                                              *//
//************************************************//

#include <pthread.h>
#include "spiffer_caer_support.h"

// global variables
// spif pipes
extern uint * pipe_buf[SPIF_HW_PIPES_NUM];

// used to pass integers as (void *)
extern int int_to_ptr[SPIF_HW_PIPES_NUM];

// USB devices
extern usb_devs_t      usb_devs;
extern pthread_mutex_t usb_mtx;

// log file
extern FILE * lf;


//--------------------------------------------------------------------
// include here any libcaer initialisation
//--------------------------------------------------------------------
void spiffer_caer_init (void) {
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// attempt to configure a camera supported by libcaer
//
// returns SPIFFER_OK on success or SPIFFER_ERROR on error
//--------------------------------------------------------------------
int spiffer_caer_config_dev (caerDeviceHandle dh) {
  // send the default configuration before using the device
  bool rc = caerDeviceSendDefaultConfig (dh);
  if (!rc) {
    log_time ();
    fprintf (lf, "error: failed to send camera default configuration\n");
    (void) fflush (lf);
    return (SPIFFER_ERROR);
  }

  // adjust number of events that a container packet can hold
  rc = caerDeviceConfigSet (dh, CAER_HOST_CONFIG_PACKETS,
                            CAER_HOST_CONFIG_PACKETS_MAX_CONTAINER_PACKET_SIZE,
                            USB_EVTS_PER_PKT
                            );
  if (!rc) {
    log_time ();
    fprintf (lf, "error: failed to set camera container packet size\n");
    (void) fflush (lf);
    return (SPIFFER_ERROR);
  }

  // set event reception to blocking mode
  rc = caerDeviceConfigSet (dh, CAER_HOST_CONFIG_DATAEXCHANGE, CAER_HOST_CONFIG_DATAEXCHANGE_BLOCKING, true);
  if (!rc) {
    log_time ();
    fprintf (lf, "error: failed to set blocking mode\n");
    (void) fflush (lf);
    return (SPIFFER_ERROR);
  }

  return (SPIFFER_OK);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// attempt to discover, open and configure cameras supported by libcaer
//--------------------------------------------------------------------
void spiffer_caer_discover_devs (void) {
  if (!USB_MIX_CAMERAS && (usb_devs.cnt != 0)) {
    log_time ();
    fprintf (lf, "warning: Inivation discovery cancelled - camera mixing disallowed\n");
    (void) fflush (lf);
    return;
  }

  // number of discovered devices
  int ndd = 0;

  // discover devices by trying to open them
  //NOTE: caerDeviceDiscover causes problems when camerasare connected/disconnected.
  while (usb_devs.cnt < USB_DISCOVER_CNT) {
    // try to open a new device and give it a device ID,
    //TODO: update for other device types
    caerDeviceHandle dh = caerDeviceOpen (ndd + 1, CAER_DEVICE_DAVIS, 0, 0, NULL);
    if (dh == NULL) {
      // no more devices available
      break;
    }

    // get camera info
    struct caer_davis_info davis_info = caerDavisInfoGet (dh);

    // configure device
    if (spiffer_caer_config_dev (dh) == SPIFFER_ERROR) {
      // on error close device
      (void) caerDeviceClose (&dh);

      log_time ();
      fprintf (lf, "warning: cannot configure device: %s\n", davis_info.deviceString);

      // and move on to the next one
      continue;
    }

    // report camera info
    log_time ();
    fprintf (lf, "%s\n", davis_info.deviceString);

    // remember device type
    usb_devs.params[usb_devs.cnt].type = CAER;

    // remember device serial number
    (void) strcpy (usb_devs.params[usb_devs.cnt].sn, davis_info.deviceSerialNumber);

    // remember device handle
    usb_devs.params[usb_devs.cnt].caer_hdl = dh;

    // update device count
    usb_devs.cnt++;
    ndd++;
  }

  log_time ();
  fprintf (lf, "discovered %i Inivation device%c\n", ndd, ndd == 1 ? ' ' : 's');
  (void) fflush (lf);

  return;
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
int spiffer_caer_get_events (caerDeviceHandle dev, uint * buf) {
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
      // release packet container
      caerEventPacketContainerFree (packetContainer);
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
void * spiffer_caer_usb_listener (void * data) {
  int dev  = *((int *) data);
  int pipe = usb_devs.params[dev].pipe;

  // block signals - should be handled in main thread
  sigset_t set;
  sigfillset (&set);
  sigprocmask (SIG_BLOCK, &set, NULL);

  uint *           sb = pipe_buf[pipe];
  caerDeviceHandle ud = usb_devs.params[dev].caer_hdl;

  // turn on camera event transmission
  bool rc = caerDeviceDataStart (ud, NULL, NULL, NULL, &usb_survey_devs, data);
  if (!rc) {
    log_time ();
    fprintf (lf, "error: failed to start camera event transmission\n");
    (void) fflush (lf);

    return (nullptr);
  }

  log_time ();
  fprintf (lf, "listening USB %s -> pipe%i\n", usb_devs.params[dev].sn, pipe);
  (void) fflush (lf);

  while (1) {
    // get next batch of events
    //NOTE: blocks until events are available
    int rcv_bytes = spiffer_caer_get_events (ud, sb);

    // trigger a transfer to SpiNNaker
    spif_transfer (pipe, rcv_bytes);

    // wait until spif finishes the current transfer
    //NOTE: report when waiting too long!
    int wc = 0;
    while (spif_busy (pipe)) {
      wc++;
      if (wc < 0) {
        log_time ();
        fprintf (lf, "warning: spif not responding\n");
        (void) fflush (lf);
        wc = 0;
      }
    }
  }
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// shutdown device
//--------------------------------------------------------------------
void spiffer_caer_shutdown_dev (int dev) {
  (void) caerDeviceDataStop (usb_devs.params[dev].caer_hdl);
  (void) caerDeviceClose (&usb_devs.params[dev].caer_hdl);
}
//--------------------------------------------------------------------
