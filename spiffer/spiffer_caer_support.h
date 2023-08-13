//************************************************//
//*                                              *//
//*            spiffer libcaer support           *//
//*                                              *//
//* lap - 11/08/2023                             *//
//*                                              *//
//************************************************//

#ifndef __SPIF_CAER_H__
#define __SPIF_CAER_H__


#include <libcaer/libcaer.h>
#include <libcaer/devices/davis.h>
#include <libcaer/devices/device_discover.h>


// spif and spiffer constants and function prototypes
#include "spif.h"
#include "spiffer.h"


int spif_busy (uint pipe);
int spif_transfer (uint pipe, int length);


//--------------------------------------------------------------------
// attempt to configure a camera supported by libcaer
//
// returns SPIFFER_OK on success or SPIFFER_ERROR on error
//--------------------------------------------------------------------
int spif_caer_config_dev (int pipe, caerDeviceHandle dh);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// attempt to discover, open and configure cameras supported by libcaer
//
// returns the number of discovered devices (0 on error)
//--------------------------------------------------------------------
int spif_caer_discover_devs (void);
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
int spif_caer_usb_get_events (caerDeviceHandle dev, uint * buf);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// receive events from a USB device and forward them to spif
//
// terminated as a result of signal servicing
//--------------------------------------------------------------------
void * spif_caer_usb_listener (void * data);
//--------------------------------------------------------------------


#endif /* __SPIF_CAER_H__ */
