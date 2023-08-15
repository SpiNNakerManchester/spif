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
//TODO: change to system/kernel log
extern FILE * lf;

//--------------------------------------------------------------------
// attempt to discover and open cameras supported by Metavision SDK
//
// returns the number of discovered devices (0 on error)
//--------------------------------------------------------------------
int spiffer_meta_discover_devs (void) {
  // number of discovered devices
  int ndd = 0;

  // find prophesee devices
  auto v = Metavision::DeviceDiscovery::list();
  if (v.size () == 0) {
    return (ndd);
  }

  // identify each device found
  std::unique_ptr<Metavision::Device> device;
  if (v.size ()) {
    for (auto s : v) {
      try {
        // open device
        device = Metavision::DeviceDiscovery::open(s);
      } catch (int err) {
        log_time ();
        fprintf (lf, "error %d: cannot open device: %s\n", err, s.c_str ());
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

      if (usb_devs.cnt >= USB_DISCOVER_CNT) {
        break;
      }
    }
  }

  log_time ();
  fprintf (lf, "discovered %i Prophesee device%c\n", ndd, ndd == 1 ? ' ' : 's');
  (void) fflush (lf);

  return (ndd);
}
//--------------------------------------------------------------------
//--------------------------------------------------------------------
// receive events from a USB device and forward them to spif
//
// terminated as a result of signal servicing
//--------------------------------------------------------------------
void * spiffer_meta_usb_listener (void * data) {
  int pipe = *((int *) data);

  uint *                               sb = pipe_buf[pipe];
  std::unique_ptr<Metavision::Device>  ud = std::move (usb_devs.params[pipe].meta_hdl);

  Metavision::I_EventsStream                      * events_stream         = nullptr;
  Metavision::I_EventsStreamDecoder               * events_stream_decoder = nullptr;
  Metavision::I_EventDecoder<Metavision::EventCD> * cd_event_decoder;
  Metavision::I_EventsStream::RawData             * ev_raw_data;

  uint evt_ctr = 0;
  long rcv_bytes;

  log_time ();
  fprintf (lf, "listening USB %s -> pipe%i\n", usb_devs.params[pipe].sn, pipe);
  (void) fflush (lf);

  // open event stream
  events_stream = ud->get_facility<Metavision::I_EventsStream>();
  if (!events_stream) {
    return (NULL);
  }

  // get event stream dispatcher
  events_stream_decoder = ud->get_facility<Metavision::I_EventsStreamDecoder>();

  // get CD (contrast detection) event decoder
  cd_event_decoder = ud->get_facility<Metavision::I_EventDecoder<Metavision::EventCD>>();

  // register event processing callback
  if (cd_event_decoder) {
      // Register a lambda function to be called on every CD events
      cd_event_decoder->add_event_buffer_callback(
          [pipe, sb, &evt_ctr](const Metavision::EventCD *begin, const Metavision::EventCD *end) {
            for (auto it = begin; it != end; ++it) {
              sb[evt_ctr++] = 0x80000000 | (it->x << 16) | (it->p << 15) | it->y;
              if (evt_ctr == SPIFFER_BATCH_SIZE) {
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
          });
  }

  // start streaming events
  events_stream->start ();

  while (1) {
    // this is a safe place to cancel this thread
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
    pthread_testcancel ();
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);

    // process next batch of events
    if (events_stream->wait_next_buffer() <= 0) {
      continue;
    }

    ev_raw_data = events_stream->get_latest_raw_data (rcv_bytes);

    events_stream_decoder->decode(ev_raw_data, ev_raw_data + rcv_bytes);

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

    evt_ctr = 0;
  }
}
//--------------------------------------------------------------------
