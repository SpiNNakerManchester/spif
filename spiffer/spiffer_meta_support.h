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
// attempt to discover and open cameras supported by Metavision SDK
//
// returns the number of discovered devices (0 on error)
//--------------------------------------------------------------------
int spiffer_meta_discover_devs (void);
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// receive events from a USB device and forward them to spif
//
// terminated as a result of signal servicing
//--------------------------------------------------------------------
void * spiffer_meta_usb_listener (void * data);
//--------------------------------------------------------------------


#endif /* __spiffer_meta_H__ */
