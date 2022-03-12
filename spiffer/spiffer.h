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
#define SPIF_BATCH_SIZE   256
#define SPIF_NUM_PIPES    2
#define UDP_PORT_BASE     3333
#define USB_EVTS_PER_PKT  256
#define USB_DISCONN_CNT   10000
#define USB_DEV_DISCOVER  NULL


// USB listener
typedef char caer_sn_t[9];

typedef struct usb_devs {
  int              dev_cnt;                  // number of connected USB devices
  caerDeviceHandle dev_hdl[SPIF_NUM_PIPES];  // USB device handle
  caer_sn_t        dev_sn[SPIF_NUM_PIPES];   // USB device serial number
} usb_devs_t;


//--------------------------------------------------------------------
// stop spiffer
// caused by systemd request or error condition
//
// returns -1 on error condition, 0 otherwise
//--------------------------------------------------------------------
void spiffer_stop (int ec);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// setup Ethernet UDP server to listen for events
// create and bind on socket
//
// returns -1 if problems found
//--------------------------------------------------------------------
int udp_srv_setup (int eth_port);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// receive events through Ethernet UDP port and forward them to spif
//
// lives until cancelled by the arrival of a signal
//--------------------------------------------------------------------
void * udp_listener (void * data);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// attempt to discover CAER devices connected to the USB bus
//
// returns the number of devices or -1 on detection error
//--------------------------------------------------------------------
int usb_discover (void);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// add new USB device
//
// returns -1 on error condition, 0 otherwise
//--------------------------------------------------------------------
void usb_add_dev (void);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// remove USB device
//
// returns -1 on error condition, 0 otherwise
//--------------------------------------------------------------------
void usb_rm_dev (void * data);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// get the next batch of events from USB device
//
// returns 0 if no data available
//--------------------------------------------------------------------
int usb_get_events (caerDeviceHandle dev, uint * buf);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// receive events from a USB device and forward them to spif
//
// lives until cancelled by the arrival of a signal
//--------------------------------------------------------------------
void * usb_listener (void * data);
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
void sig_service (int signum);
//--------------------------------------------------------------------


#endif /* __SPIFFER_H__ */
