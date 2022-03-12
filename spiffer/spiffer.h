//************************************************//
//*                                              *//
//*          spif UDP/USB listener               *//
//*                                              *//
//*                                              *//
//* lap - 09/03/2022                             *//
//*                                              *//
//************************************************//

#ifndef __SPIFFER_H__
#define __SPIFFER_H__


// constants
#define SPIFFER_ERROR     -1
#define SPIFFER_OK        0
#define SPIF_BATCH_SIZE   256
#define SPIF_NUM_PIPES    2
#define UDP_PORT_BASE     3333
#define USB_EVTS_PER_PKT  256
#define USB_DISCOVER_CNT  SPIF_NUM_PIPES


// USB listener
typedef char caer_sn_t[9];

typedef struct usb_devs {
  int              dev_cnt;                  // number of connected USB devices
  int              dev_id;                   // next device ID
  caerDeviceHandle dev_hdl[SPIF_NUM_PIPES];  // USB device handle
} usb_devs_t;


//--------------------------------------------------------------------
// stop spiffer
// caused by systemd request or error condition
//--------------------------------------------------------------------
void spiffer_stop (int ec);
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
// sort USB devices by serial number
//
// no return value
//--------------------------------------------------------------------
void usb_sort (int ndv, caerDeviceHandle * dvh, int * sorted);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// attempt to open and configure USB device
//
// returns SPIFFER_OK on success or SPIFFER_ERROR on error
//--------------------------------------------------------------------
int usb_dev_config (int pipe, caerDeviceHandle dh);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// attempt to discover new supported devices connected to the USB bus
// sort devices by serial number for consistent mapping to spif
//
//NOTE: the caer discover library function seems to fail sometimes
//
// returns the number of discovered devices (0 on error)
//--------------------------------------------------------------------
int usb_discover_devs (int old_pipe);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// add new USB devices
//
// no return value
//--------------------------------------------------------------------
void usb_add_devs (void * data);
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
int usb_get_events (caerDeviceHandle dev, uint * buf);
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
// SIGINT  requests spiffer to stop
//
// no return value
//--------------------------------------------------------------------
void sig_service (int signum);
//--------------------------------------------------------------------


#endif /* __SPIFFER_H__ */
