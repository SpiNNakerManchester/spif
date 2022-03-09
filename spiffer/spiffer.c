//************************************************//
//*                                              *//
//*          spif UDP/USB listener               *//
//*                                              *//
//* started automatically at boot time           *//
//* exits with -1 if problems found              *//
//*                                              *//
//* lap - 09/03/2022                             *//
//*                                              *//
//************************************************//

#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Inivation camera support
#include <libcaer/libcaer.h>
#include <libcaer/devices/davis.h>
#include <libcaer/devices/device_discover.h>

// spif support
#include "spif_remote.h"


// constants
#define SPIF_BATCH_SIZE   256
#define SPIF_NUM_PIPES    2

#define UDP_PORT_BASE     3333


//global variables
// signals
struct sigaction signal_cfg;

// threads
pthread_t      listener[SPIF_NUM_PIPES];
pthread_attr_t attr;

// spif
int    spif_pipe[SPIF_NUM_PIPES];
uint * spif_buf[SPIF_NUM_PIPES];

// spif state (spif pipes, UDP ports and USB devices)
// keep track of number of USB cameras connected
enum spif_cameras_t {no = 0, one = 1, two = 2} spif_cameras;

// UDP listener
int pipe_skt[SPIF_NUM_PIPES];

// log file
//TODO: change to system/kernel log
FILE * lf;


//--------------------------------------------------------------------
// stop spiffer
// - systemd request or error condition
//
// returns -1 on error condition, 0 otherwise
//--------------------------------------------------------------------
void spiffer_stop (int ec) {
  for (int pipe = 0; pipe < SPIF_NUM_PIPES; pipe++) {
    // cancel threads

    // close UDP ports
    close (pipe_skt[pipe]);
    fprintf (lf, "UDP port %d closed\n", UDP_PORT_BASE + pipe);

    // close spif pipes
    spif_close (spif_pipe[pipe]);
    fprintf (lf, "spif pipe%d closed\n", pipe);
  }

  // say goodbye
  fprintf (lf, "spiffer stopped\n");

  // close log
  fclose (lf);

  exit (ec);
}


//--------------------------------------------------------------------
// setup Ethernet UDP server to listen for events
// - create and bind on socket
//
// returns -1 if problems found
//--------------------------------------------------------------------
int udp_srv_setup (int eth_port) {
  // create UDP socket
  int skt = socket (AF_INET, SOCK_DGRAM, 0);
  if (skt == -1) {
    return (-1);
  }

  // configure server environment
  struct sockaddr_in srv_addr;
  srv_addr.sin_family      = AF_INET;
  srv_addr.sin_port        = htons (eth_port);
  srv_addr.sin_addr.s_addr = INADDR_ANY;
  bzero (&(srv_addr.sin_zero), 8);

  // bind UDP socket
  if (bind (skt, (struct sockaddr *) &srv_addr,
      sizeof (struct sockaddr)) == -1) {
    close (skt);
    return (-1);
  }

  return (skt);
}


//--------------------------------------------------------------------
// receive events through Ethernet UDP port and forward it to spif
//
// returns 0 if no events available
//--------------------------------------------------------------------
void * udp_listener (void * data) {
  int pipe = (int) data;

  // announce that listener is ready
  fprintf (lf, "listening UDP %d-> pipe%d\n", UDP_PORT_BASE + pipe, pipe);
  (void) fflush (lf);

  int    sp = spif_pipe[pipe];
  uint * sb = spif_buf[pipe];
  size_t ss = SPIF_BATCH_SIZE * sizeof (uint);
  int    ps = pipe_skt[pipe];

  while (1) {
    // get next batch of events
    //NOTE: blocks until events are available
    int rcv_bytes = recv (ps, (void *) sb, ss, 0);

    // trigger a transfer to SpiNNaker
    spif_transfer (sp, rcv_bytes);

    // wait until spif finishes the current transfer
    //NOTE: report when waiting too long!
    int wc = 0;
    while (spif_busy (sp)) {
      wc++;
      if (wc < 0) {
        fprintf (lf, "error: spif not responding\n");
	(void) fflush (lf);
        wc = 0;
      }
    }
  }
}


//--------------------------------------------------------------------
// get the next batch of events from Ethernet
//
// returns 0 if no events available
//--------------------------------------------------------------------
int usb_discover (void) {
  caerDeviceDiscoveryResult discovered;
  ssize_t result = caerDeviceDiscover(CAER_DEVICE_DISCOVER_ALL, &discovered);
  if (result < 0) {
    fprintf (lf, "error detecting Iniviation cameras\n");
    (void) fflush (lf);
    return (0);
  }

  return (result);
}


//--------------------------------------------------------------------
// get the next batch of events from Ethernet
//
// returns 0 if no events available
//--------------------------------------------------------------------
void * usb_listener (void * data) {
  int pipe = (int) data;

  fprintf (lf, "listening usb -> pipe%i\n", pipe);
  (void) fflush (lf);

  while (1) {sleep (10);};
}


//--------------------------------------------------------------------
// get the next batch of events from Ethernet
//
// service received signals
// SIGUSR1 indicates that a USB device has been connected
// SIGUSR2 indicates that a USB device was disconnected
// SIGINT  is a request to stop

// returns 0 if no events available
//--------------------------------------------------------------------
void sig_service (int signum) {
  switch (signum) {
    case SIGUSR1:  // USB connection event
      // check camera type
      if (usb_discover ()) {
        fprintf (lf, "Iniviation camera connected\n");
      } else {
        fprintf (lf, "Prophesee camera connected\n");
      }
      (void) fflush (lf);

      switch (spif_cameras) {
        case no:
	  spif_cameras = one;
	  pthread_cancel (listener[0]);
          (void) pthread_create (&listener[0], &attr, usb_listener, (void*) 0);
	  return;
        case one:
	  spif_cameras = two;
	  pthread_cancel (listener[1]);
          (void) pthread_create (&listener[1], &attr, usb_listener, (void*) 1);
	  return;
        default:
	  fprintf (lf, "ignoring USB event\n");
	  (void) fflush (lf);
      }
      return;
    case SIGUSR2:  // USB disconnection event
      fprintf (lf, "camera disconnected\n");
      (void) fflush (lf);
      switch (spif_cameras) {
        case one:
	  spif_cameras = no;
	  pthread_cancel (listener[0]);
          (void) pthread_create (&listener[0], &attr, udp_listener, (void*) 0);
	  return;
        case two:
	  spif_cameras = one;
	  pthread_cancel (listener[1]);
          (void) pthread_create (&listener[1], &attr, udp_listener, (void*) 1);
	  return;
        default:
	  fprintf (lf, "ignoring USB event\n");
	  (void) fflush (lf);
      }
      return;
    default:
      // shut down spiffer
      spiffer_stop (0);
  }
}


//--------------------------------------------------------------------
// opens eaxh spif pipe
// sets up a UDP server for each spif pipe
// creates a listener thread for each spif pipe
// configures signal management to support USB devices
//--------------------------------------------------------------------
int main (int argc, char *argv[])
{
  (void) argc;
  (void) argv;

  // open log file
  //TODO: needs a permanent home
  lf = fopen ("/tmp/spiffer.log", "a");
  if (lf == NULL) {
    exit (1);
  } else {
  }

  // say hello
  fprintf (lf, "spiffer started\n");
  (void) fflush (lf);


  // spif buffer size
  size_t batch_size = SPIF_BATCH_SIZE * sizeof (uint);

  // open spif pipes and get corresponding buffer
  for (int pipe = 0; pipe < SPIF_NUM_PIPES; pipe++) {
    // open spif pipe
    spif_pipe[pipe] = spif_open (pipe);
    if (spif_pipe[pipe] == -1) {
      fprintf (lf, "error: unable to open spif pipe%d\n", pipe);
      (void) fflush (lf);
      spiffer_stop (-1);
    }

    // get pipe buffer
    spif_buf[pipe] = (uint *) spif_get_buffer (spif_pipe[pipe], batch_size);
    if (spif_buf[pipe] == NULL) {
      fprintf (lf, "error: failed to get buffer for spif pipe%d\n", pipe);
      (void) fflush (lf);
      spiffer_stop (-1);
    }

    fprintf (lf, "spif pipe%d opened\n", pipe);
    (void) fflush (lf);
  }

  // set up thread attributes
  (void) pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);

  for (int pipe = 0; pipe < SPIF_NUM_PIPES; pipe++) {
    // setup Ethernet UDP servers to receive events
    pipe_skt[pipe] = udp_srv_setup (UDP_PORT_BASE + pipe);
    if (pipe_skt[pipe] == -1) {
      fprintf (lf, "error: failed to listen on UDP port %d\n", UDP_PORT_BASE + pipe);
      (void) fflush (lf);
      spiffer_stop (-1);
    }

    // start a thread for every spif pipe
    //NOTE: always start with UDP listeners, which can be active at any time
    (void) pthread_create (&listener[pipe], &attr, udp_listener, (void*) pipe);
  }

  // set up signal servicing
  signal_cfg.sa_flags = 0;
  signal_cfg.sa_handler = &sig_service;

  // register signal service routine
  sigaction(SIGINT,  &signal_cfg, NULL);
  sigaction(SIGUSR1, &signal_cfg, NULL);
  sigaction(SIGUSR2, &signal_cfg, NULL);

  while (1) {
    for (int pipe = 0; pipe < SPIF_NUM_PIPES; pipe++) {
      pthread_join (listener[pipe], NULL);
    }
  }
}
