//************************************************//
//*                                              *//
//*        spiffer Metavision SDK support        *//
//*                                              *//
//* lap - 11/08/2023                             *//
//*                                              *//
//************************************************//

#include "spiffer_meta_support.h"

// global variables
// spif pipes
extern uint * pipe_buf[SPIF_HW_PIPES_NUM];

// USB devices
extern usb_devs_t      usb_devs;
extern pthread_mutex_t usb_mtx;

// log file
extern FILE * lf;

// used to pass integers as (void *)
extern int int_to_ptr[SPIF_HW_PIPES_NUM];


//--------------------------------------------------------------------
// include here any Metavision SDK initialisation
//--------------------------------------------------------------------
void spiffer_meta_init (void) {
  // do not show Metavision warnings
  setenv ("MV_LOG_LEVEL", "ERROR", 1);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// attempt to discover and open cameras supported by Metavision SDK
//--------------------------------------------------------------------
void spiffer_meta_discover_devs (void) {
  if (!SPIFFER_USB_MIX_CAMERAS && (usb_devs.cnt != 0)) {
    log_time ();
    fprintf (lf, "warning: Prophesee discovery cancelled - camera mixing disallowed\n");
    (void) fflush (lf);
    return;
  }

  // number of discovered devices
  int ndd = 0;

  // find prophesee devices
  auto v = Metavision::DeviceDiscovery::list();

  // identify each device found
  std::unique_ptr<Metavision::Device> device;
  if (v.size ()) {
    for (auto s : v) {
      try {
        // open device
        device = Metavision::DeviceDiscovery::open(s);
      } catch (int err) {
        log_time ();
        fprintf (lf, "error %i: cannot open device: %s\n", err, s.c_str ());
        continue;
      }

      // get camera info
      Metavision::I_HW_Identification *hw_identification = device->get_facility<Metavision::I_HW_Identification>();

      // report camera info
      log_time ();
      fprintf (lf, "%s\n", s.c_str ());

      // remember device type
      usb_devs.params[usb_devs.cnt].type = META;

      // remember device serial number
      (void) strcpy (usb_devs.params[usb_devs.cnt].sn, hw_identification->get_system_info()["Serial"].c_str ());

      // remember device handle
      usb_devs.params[usb_devs.cnt].meta_hdl = std::move (device);

      // update device count
      usb_devs.cnt++;
      ndd++;

      if (usb_devs.cnt >= SPIFFER_META_DISCOVER_CNT) {
        break;
      }
    }
  }

  log_time ();
  fprintf (lf, "discovered %i Prophesee device%c\n", ndd, ndd == 1 ? ' ' : 's');
  (void) fflush (lf);

  return;
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// receive events from a USB device and forward them to spif
// map them to spif events with format:
//
//    [31] timestamp (0: present, 1: absent)
// [30:16] 15-bit x coordinate
//    [15] polarity
//  [14:0] 15-bit y coordinate
//
// timestamps are ignored - time models itself
//
// terminated as a result of signal servicing
//--------------------------------------------------------------------
void * spiffer_meta_usb_listener (void * data) {
  int dev  = *((int *) data);
  int pipe = usb_devs.params[dev].pipe;

  // block signals - should be handled in main thread
  sigset_t set;
  sigfillset (&set);
  sigprocmask (SIG_BLOCK, &set, NULL);

  uint *                               sb = pipe_buf[pipe];
  std::unique_ptr<Metavision::Device>  ud = std::move (usb_devs.params[dev].meta_hdl);

  Metavision::I_EventsStream                      * events_stream         = nullptr;
  Metavision::I_EventsStreamDecoder               * events_stream_decoder = nullptr;
  Metavision::I_EventDecoder<Metavision::EventCD> * cd_event_decoder;
  Metavision::I_EventsStream::RawData             * ev_raw_data;

  uint evt_ctr = 0;
  long rcv_bytes;

  // open event stream
  events_stream = ud->get_facility<Metavision::I_EventsStream>();
  if (!events_stream) {
    log_time ();
    fprintf (lf, "error: failed to start camera event transmission\n");
    (void) fflush (lf);

    return (nullptr);
  }

  // get event stream dispatcher
  events_stream_decoder = ud->get_facility<Metavision::I_EventsStreamDecoder>();

  // get CD (contrast detection) event decoder
  cd_event_decoder = ud->get_facility<Metavision::I_EventDecoder<Metavision::EventCD>>();

  // register event processing callback
  if (cd_event_decoder) {
    // Register a lambda function to be called on every CD event
    cd_event_decoder->add_event_buffer_callback (
      [pipe, sb, &evt_ctr](const Metavision::EventCD *first, const Metavision::EventCD *last) {
        for (auto it = first; it != last; ++it) {
          uint pol = it->p;
          uint x   = it->x;
          uint y   = it->y;

          // format event and store in buffer
          sb[evt_ctr] = SPIFFER_EVT_NO_TS | (x << SPIFFER_EVT_X_SHIFT) | (pol << SPIFFER_EVT_P_SHIFT) | (y << SPIFFER_EVT_Y_SHIFT);
          if (++evt_ctr == SPIFFER_BATCH_SIZE) {
            // trigger a transfer to SpiNNaker
            spif_transfer (pipe, evt_ctr);

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

            evt_ctr = 0;
          }
        }
      }
    );
  }

  // start streaming events
  events_stream->start ();

  log_time ();
  fprintf (lf, "listening USB %s -> pipe%i\n", usb_devs.params[dev].sn, pipe);
  (void) fflush (lf);

  while (1) {
    // this is a safe place to cancel this thread
    pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, 0);
    pthread_testcancel ();
    pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, 0);

    // process next batch of events
    short rc = events_stream->wait_next_buffer();

    // an error (rc < 0) usually means that the camera was disconnected
    if (rc < 0) {
      log_time ();
      fprintf (lf, "warning: device %s stopped responding\n", usb_devs.params[dev].sn);
      (void) fflush (lf);

      // trigger a device survey
      usb_survey_devs (&int_to_ptr[dev]);

      // shutdown disconnected camera
      spiffer_meta_shutdown_dev (dev);

      // wait for thread cancelling
      pause ();
    }

    // if buffer empty try again
    if (rc == 0) {
      continue;
    }

    ev_raw_data = events_stream->get_latest_raw_data (rcv_bytes);

    events_stream_decoder->decode(ev_raw_data, ev_raw_data + rcv_bytes);

    // trigger a transfer to SpiNNaker
    if (evt_ctr != 0) {
      spif_transfer (pipe, evt_ctr);
    }

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

    evt_ctr = 0;
  }
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// shutdown device
//
//TODO: needs updating!
//--------------------------------------------------------------------
void spiffer_meta_shutdown_dev (int dev) {
  // destroy camera object
  usb_devs.params[dev].meta_hdl.reset ();
}
//--------------------------------------------------------------------
