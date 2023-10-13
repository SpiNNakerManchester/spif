//************************************************//
//*                                              *//
//*            spiffer libcaer support           *//
//*                                              *//
//* lap - 11/08/2023                             *//
//*                                              *//
//************************************************//

#ifndef __spiffer_caer_H__
#define __spiffer_caer_H__


#include <libcaer/libcaer.h>
#include <libcaer/devices/davis.h>
#include <libcaer/devices/device_discover.h>

#include <unistd.h>
#include <signal.h>

// spif and spiffer constants and function prototypes
#include "spif.h"
#include "spiffer.h"

#define SPIFFER_CAER_DISCOVER_CNT  SPIFFER_USB_DISCOVER_CNT

int spif_busy (uint pipe);
int spif_transfer (uint pipe, int length);


//--------------------------------------------------------------------
// include here any libcaer initialisation
//--------------------------------------------------------------------
void spiffer_caer_init (void);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// attempt to configure a camera supported by libcaer
//
// returns SPIFFER_OK on success or SPIFFER_ERROR on error
//--------------------------------------------------------------------
int spiffer_caer_config_dev (int pipe, caerDeviceHandle dh);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// attempt to discover, open and configure cameras supported by libcaer
//--------------------------------------------------------------------
void spiffer_caer_discover_devs (void);
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
int spiffer_caer_get_events (caerDeviceHandle dev, uint * buf);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// receive events from a USB device and forward them to spif
//
// terminated as a result of signal servicing
//--------------------------------------------------------------------
void * spiffer_caer_usb_listener (void * data);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// shutdown device
//--------------------------------------------------------------------
void spiffer_caer_shutdown_dev (int dev);
//--------------------------------------------------------------------


#endif /* __spiffer_caer_H__ */
