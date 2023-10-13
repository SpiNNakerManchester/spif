//************************************************//
//*                                              *//
//*       spif UDP/USB/SPiNNaker listener        *//
//*                                              *//
//* includes support for Inivation cameras       *//
//* lap - 09/03/2022                             *//
//*                                              *//
//* added (C++) support for Prophesee cameras    *//
//* lap - 11/08/2023                             *//
//*                                              *//
//************************************************//

#ifndef __SPIFFER_H__
#define __SPIFFER_H__


// Inivation camera support
#ifdef CAER_SUPPORT
#include <libcaer/libcaer.h>
#include <libcaer/devices/davis.h>
#endif

// Prophesee camera support
#ifdef META_SUPPORT
#include "metavision/sdk/base/events/event_cd.h"
#include <metavision/hal/device/device_discovery.h>
#include <metavision/hal/device/device.h>
#include <metavision/hal/facilities/i_hw_identification.h>
#include <metavision/hal/facilities/i_event_decoder.h>
#include <metavision/hal/facilities/i_events_stream.h>
#include <metavision/hal/facilities/i_events_stream_decoder.h>
#endif

// constants
#define SPIFFER_VER_MAJ     0
#define SPIFFER_VER_MIN     2
#define SPIFFER_VER_PAT     0
#define SPIFFER_ERROR      -1
#define SPIFFER_OK         0
#define SPIFFER_BATCH_SIZE 256

#define SPIFFER_UDP_PORT_BASE      3333

#define SPIFFER_USB_EVTS_PER_PKT   256
#define SPIFFER_USB_DISCOVER_CNT   SPIF_HW_PIPES_NUM
#define SPIFFER_USB_NO_DEVICE      -1
#define SPIFFER_USB_MIX_CAMERAS    false

#define SPIFFER_EVT_X_SHIFT        16
#define SPIFFER_EVT_X_MASK         0x7fff
#define SPIFFER_EVT_Y_SHIFT        0
#define SPIFFER_EVT_Y_MASK         0x7fff
#define SPIFFER_EVT_P_SHIFT        15
#define SPIFFER_EVT_P_MASK         0x1
#define SPIFFER_EVT_NO_TS          0x80000000

// spif output commands
#define SPIFFER_OUT_START  0x5ec00051
#define SPIFFER_OUT_STOP   0x5ec00050

// log file
//TODO: maybe change to system/kernel log
static const char * log_name = "/tmp/spiffer.log";

// USB devices
typedef char serial_t[9];

typedef enum {
  CAER,
  META
} device_type_t;

typedef struct dev_params {
  int                                 pipe;
  device_type_t                       type;
  serial_t                            sn;
#ifdef CAER_SUPPORT
  caerDeviceHandle                    caer_hdl;
#endif
#ifdef META_SUPPORT
  std::unique_ptr<Metavision::Device> meta_hdl;
#endif
} device_params_t;

typedef struct usb_devs {
  int              cnt;                         // number of connected USB devices
  device_params_t  params[SPIF_HW_PIPES_NUM];   // USB device params
} usb_devs_t;


//--------------------------------------------------------------------
// write current time to log file
//--------------------------------------------------------------------
void log_time (void);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// shutdown input listeners and USB devices
//
// needed when USB devices are connected or disconnected
//--------------------------------------------------------------------
void spiffer_input_shutdown (int discon_dev);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// stop spiffer
// caused by systemd request or error condition
//--------------------------------------------------------------------
void spiffer_stop (int ec);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// setup SpiNNaker output through spif
//--------------------------------------------------------------------
int spiNNaker_init (void);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// receive events from SpiNNaker through spif
//
// expects event to arrive in spif format - no mapping is done
//
// terminated as a result of signal servicing
//--------------------------------------------------------------------
void * spiNNaker_listener (void * data);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// setup Ethernet UDP servers to listen for events
// create and bind on socket
//
// returns SPIFFER_ERROR if problems found
//--------------------------------------------------------------------
int udp_init (void);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// receive events through Ethernet UDP port and forward them to spif
//
// expects event to arrive in spif format - no mapping is done
//
// terminated as a result of signal servicing
//--------------------------------------------------------------------
void * udp_listener (void * data);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// receive output commands through Ethernet UDP port
//
// terminated as a result of signal servicing
//--------------------------------------------------------------------
void * out_udp_listener (void * data);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// attempt to configure USB device
//
// returns SPIFFER_OK on success or SPIFFER_ERROR on error
//--------------------------------------------------------------------
int usb_dev_config (int pipe);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// sort USB devices by serial number and assign pipes
//
// no return value
//--------------------------------------------------------------------
void usb_sort_pipes ();
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// attempt to discover new devices connected to the USB bus
// sort devices by serial number for consistent mapping to spif pipes
//
// discon_dev = known disconnected USB device, -1 for unknown
//--------------------------------------------------------------------
void usb_discover_devs (int discon_dev);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// survey USB devices
// called at start up and on USB device connection and disconnection
//
// data = known disconnected USB device, -1 for unknown
//
// no return value
//--------------------------------------------------------------------
void usb_survey_devs (void * data);
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
int usb_get_events ();
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// receive events from a USB device and forward them to spif
//
// terminated as a result of signal servicing
//--------------------------------------------------------------------
void * usb_listener (void * data);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// initialise USB device state
//
// returns SPIFFER_OK on success or SPIFFER_ERROR on error
//--------------------------------------------------------------------
int usb_init (void);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// initialise spif pipes
//
// returns SPIFFER_OK on success or SPIFFER_ERROR on error
//--------------------------------------------------------------------
int spif_pipes_init (void);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// initialise system signal services
//
// returns SPIFFER_OK on success or SPIFFER_ERROR on error
//--------------------------------------------------------------------
int sig_init (void);
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
void sig_service (int signum);
//--------------------------------------------------------------------


#endif /* __SPIFFER_H__ */
