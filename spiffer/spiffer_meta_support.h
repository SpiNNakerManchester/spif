//************************************************//
//*                                              *//
//*        spiffer Metavision SDK support        *//
//*                                              *//
//* lap - 11/08/2023                             *//
//*                                              *//
//************************************************//

#ifndef __spiffer_meta_H__
#define __spiffer_meta_H__


#include "metavision/sdk/base/events/event_cd.h"
#include <metavision/hal/device/device_discovery.h>
#include <metavision/hal/device/device.h>
#include <metavision/hal/facilities/i_hw_identification.h>
#include <metavision/hal/facilities/i_event_decoder.h>
#include <metavision/hal/facilities/i_events_stream.h>
#include <metavision/hal/facilities/i_events_stream_decoder.h>

// spif and spiffer constants and function prototypes
#include "spif.h"
#include "spiffer.h"

int spif_busy (uint pipe);
int spif_transfer (uint pipe, int length);


//--------------------------------------------------------------------
// include here any Metavision SDK initialisation
//--------------------------------------------------------------------
void spiffer_meta_init (void);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// attempt to discover and open cameras supported by Metavision SDK
//--------------------------------------------------------------------
void spiffer_meta_discover_devs (void);
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
void * spiffer_meta_usb_listener (void * data);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// shutdown device
//--------------------------------------------------------------------
void spiffer_meta_shutdown_dev (int dev);
//--------------------------------------------------------------------


#endif /* __spiffer_meta_H__ */
