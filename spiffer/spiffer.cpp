//************************************************//
//*                                              *//
//*       spif UDP/USB/SPiNNaker listener        *//
//*                                              *//
//* started automatically at boot time           *//
//* exits with -1 if problems found              *//
//*                                              *//
//* includes support for Inivation cameras       *//
//* lap - 09/03/2022                             *//
//*                                              *//
//* added (C++) support for Prophesee cameras    *//
//* lap - 11/08/2023                             *//
//*                                              *//
//************************************************//

#include <csignal>
#include <ctime>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>

#include <unistd.h>

// spif support
#include "spif_remote.h"

// spiffer constants and function prototypes
# include "spiffer.h"

// event camera support
#ifdef CAER_SUPPORT
#include "spiffer_caer_support.h"
#endif

// Prophesee camera support
#ifdef META_SUPPORT
#include "spiffer_meta_support.h"
#endif


//global variables
// signals
bool             signal_usr1_taken;
pthread_mutex_t  signal_mtx = PTHREAD_MUTEX_INITIALIZER;
struct sigaction signal_term_cfg;
struct sigaction signal_usr1_cfg;

// threads
pthread_t listener[SPIF_HW_PIPES_NUM];
pthread_t out_listener[SPIF_HW_PIPES_NUM];
pthread_t spinn_listener[SPIF_HW_PIPES_NUM];

// spif pipe data
int pipe_num_in  = 0;
int pipe_num_out = 0;

int    pipe_fd[SPIF_HW_PIPES_NUM];
uint * pipe_buf[SPIF_HW_PIPES_NUM];
uint * pipe_out_buf[SPIF_HW_PIPES_NUM];

// used to pass integers as (void *)
int dev_to_ptr[SPIFFER_USB_DISCOVER_CNT + 1];

// SpiNNaker listener
int out_start[SPIF_HW_PIPES_NUM];

// UDP listener
//NOTE: also listen to UDP ports for output commands
int udp_skt[SPIF_HW_PIPES_NUM];
int out_udp_skt[SPIF_HW_PIPES_NUM];

struct sockaddr_in client_addr[SPIF_HW_PIPES_NUM];
socklen_t          client_addr_len[SPIF_HW_PIPES_NUM];

// USB devices
usb_devs_t      usb_devs;
pthread_mutex_t usb_mtx = PTHREAD_MUTEX_INITIALIZER;

// log file
//TODO: maybe change to system/kernel log
FILE * lf;


//--------------------------------------------------------------------
// write current time to log file
//--------------------------------------------------------------------
void log_time (void) {
  time_t now;
  time (&now);
  fprintf (lf, "[%.24s] ", ctime (&now));
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// shutdown input listeners and USB devices
//
// needed when USB devices are connected or disconnected
//--------------------------------------------------------------------
void spiffer_input_shutdown (int discon_dev) {
  int discon_pipe = (discon_dev != SPIFFER_USB_NO_DEVICE) ? usb_devs.params[discon_dev].pipe : SPIFFER_USB_NO_DEVICE;

  // shutdown input listeners,
  for (int pipe = 0; pipe < pipe_num_in; pipe++) {
    // cancel listener thread,
    if (pipe == discon_pipe) {
      switch (usb_devs.params[discon_dev].type) {
#ifdef CAER_SUPPORT
      case CAER:
        (void) pthread_cancel (listener[pipe]);

        // and wait for it to exit.
        pthread_join (listener[pipe], NULL);

        break;
#endif
#ifdef META_SUPPORT
      case META:
        // if a Prophesee camera is disconnected it will exit the thread
        break;
#endif
      default:
        log_time ();
        fprintf (lf, "warning: ignoring unsupported camera type\n");
        break;
      }
    } else {
      (void) pthread_cancel (listener[pipe]);
      pthread_join (listener[pipe], NULL);
    }
  }

  // shutdown USB devices,
  for (int dv = 0; dv < usb_devs.cnt; dv++) {
    if (dv != discon_dev) {
      switch (usb_devs.params[dv].type) {
#ifdef CAER_SUPPORT
      case CAER:
        spiffer_caer_shutdown_dev (dv);
        break;
#endif
#ifdef META_SUPPORT
      case META:
        spiffer_meta_shutdown_dev (dv);
        break;
#endif
      default:
        log_time ();
        fprintf (lf, "warning: ignoring unsupported camera type\n");
        break;
      }
    }
  }

  (void) fflush (lf);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// stop spiffer
// caused by systemd request or error condition
//--------------------------------------------------------------------
void spiffer_stop (int ec) {
  // shutdown input listeners and USB devices,
  spiffer_input_shutdown (SPIFFER_USB_NO_DEVICE);

  // close UDP ports,
  for (int pipe = 0; pipe < pipe_num_in; pipe++) {
    close (udp_skt[pipe]);
  }

  // shutdown SpiNNaker and output listeners,
  for (int pipe = 0; pipe < pipe_num_out; pipe++) {
    // shutdown SpiNNaker listener,
    (void) pthread_cancel (spinn_listener[pipe]);
    pthread_join (spinn_listener[pipe], NULL);

    // shutdown output UDP listener,
    (void) pthread_cancel (out_listener[pipe]);
    pthread_join (out_listener[pipe], NULL);

    // and close UDP port
    close (out_udp_skt[pipe]);
  }

  // close all spif pipes,
  int pipe_max_num = (pipe_num_in >= pipe_num_out) ? pipe_num_in : pipe_num_out;
  for (int pipe = 0; pipe < pipe_max_num; pipe++) {
    // close spif pipe
    spif_close (pipe);
  }

  // say goodbye,
  log_time ();
  fprintf (lf, "spiffer stopped\n");
  (void) fflush (lf);

  // and close log
  fclose (lf);

  exit (ec);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// setup SpiNNaker output through spif
// setup Ethernet UDP servers to listen for output control messages
// create and bind on socket
//
// start SpiNNaker listener threads
//
// returns SPIFFER_ERROR if problems found
//--------------------------------------------------------------------
int spiNNaker_init (void) {
  // start SpiNNaker listeners
  for (int pipe = 0; pipe < pipe_num_out; pipe++) {
    (void) pthread_create (&spinn_listener[pipe], NULL,
                           spiNNaker_listener, (void *) &dev_to_ptr[pipe]);
  }

  return (SPIFFER_OK);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// receive events from SpiNNaker through spif
//
// expects event to arrive in spif format - no mapping is done
//
// terminated as a result of signal servicing
//--------------------------------------------------------------------
void * spiNNaker_listener (void * data) {
  int pipe = *((int *) data);

  // block signals - should be handled in a different thread
  sigset_t set;
  sigfillset (&set);
  sigprocmask (SIG_BLOCK, &set, NULL);

  // announce that output has started
  log_time ();
  fprintf (lf, "listening SpiNNaker -> outpipe%i (UDP %i client)\n",
           pipe, SPIFFER_UDP_PORT_BASE - (pipe + 1));
  (void) fflush (lf);

  uint * sb = pipe_out_buf[pipe];
  size_t ss = SPIFFER_BATCH_SIZE * sizeof (uint);
  int    us = out_udp_skt[pipe];

  // get event batches from SpiNNaker
  while (1) {
    // trigger a transfer from SpiNNaker (blocking),
    int rcv_bytes = spif_get_output (pipe, ss);

    // check for cancellation if zero bytes
    if (rcv_bytes == 0) {
      pthread_testcancel ();
      continue;
    }

    // send data to output client - if active
    if (out_start[pipe] != 0) {
      sendto (us, sb, rcv_bytes, 0,
              (struct sockaddr *) &client_addr[pipe],
              client_addr_len[pipe]);
    }
  }
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// setup Ethernet UDP servers to listen for events
// create and bind on socket
//
//  start UDP listener threads
//
// returns SPIFFER_ERROR if problems found
//--------------------------------------------------------------------
int udp_init (void) {
  // set up input UDP servers
  for (int pipe = 0; pipe < pipe_num_in; pipe++) {
    int eth_port = SPIFFER_UDP_PORT_BASE + pipe;

    // create UDP socket,
    int skt = socket (AF_INET, SOCK_DGRAM, 0);
    if (skt == SPIFFER_ERROR) {
      log_time ();
      fprintf (lf, "error: failed to create socket for UDP port %i\n", eth_port);
      return (SPIFFER_ERROR);
    }

    // configure server,
    struct sockaddr_in srv_addr;
    srv_addr.sin_family      = AF_INET;
    srv_addr.sin_port        = htons (eth_port);
    srv_addr.sin_addr.s_addr = INADDR_ANY;
    bzero (&(srv_addr.sin_zero), 8);

    // bind socket,
    if (bind (skt, (struct sockaddr *) &srv_addr, sizeof (struct sockaddr)) == SPIFFER_ERROR) {
      close (skt);
      log_time ();
      fprintf (lf, "error: failed to bind socket for UDP port %i\n", eth_port);
      return (SPIFFER_ERROR);
    }

    //  and map socket to pipe
    udp_skt[pipe] = skt;
  }

  // set up output command UDP servers
  for (int pipe = 0; pipe < pipe_num_out; pipe++) {
    int eth_port = SPIFFER_UDP_PORT_BASE - (pipe + 1);

    // create UDP socket,
    int skt = socket (AF_INET, SOCK_DGRAM, 0);
    if (skt == SPIFFER_ERROR) {
      log_time ();
      fprintf (lf, "error: failed to create socket for UDP port %i\n", eth_port);
      return (SPIFFER_ERROR);
    }

    // configure server,
    struct sockaddr_in srv_addr;
    srv_addr.sin_family      = AF_INET;
    srv_addr.sin_port        = htons (eth_port);
    srv_addr.sin_addr.s_addr = INADDR_ANY;
    bzero (&(srv_addr.sin_zero), 8);

    // bind socket,
    if (bind (skt, (struct sockaddr *) &srv_addr, sizeof (struct sockaddr)) == SPIFFER_ERROR) {
      close (skt);
      log_time ();
      fprintf (lf, "error: failed to bind socket for UDP port %i\n", eth_port);
      return (SPIFFER_ERROR);
    }

    //  and map socket to pipe
    out_udp_skt[pipe] = skt;
  }

  // start output command UDP listeners
  for (int pipe = 0; pipe < pipe_num_out; pipe++) {
    (void) pthread_create (&out_listener[pipe], NULL,
                           out_udp_listener, (void *) &dev_to_ptr[pipe]);
  }

  return (SPIFFER_OK);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// receive events through Ethernet UDP port and forward them to spif
//
// expects event to arrive in spif format - no mapping is done
//
// terminated as a result of signal servicing
//--------------------------------------------------------------------
void * udp_listener (void * data) {
  int pipe = *((int *) data);

  // block signals - should be handled in a different thread
  sigset_t set;
  sigfillset (&set);
  sigprocmask (SIG_BLOCK, &set, NULL);

  // announce that listener is ready
  log_time ();
  fprintf (lf, "listening UDP %i -> pipe%i\n", SPIFFER_UDP_PORT_BASE + pipe, pipe);
  (void) fflush (lf);

  uint * sb = pipe_buf[pipe];
  size_t ss = SPIFFER_BATCH_SIZE * sizeof (uint);
  int    us = udp_skt[pipe];

  // get event batches from UDP port and send them to spif
  while (1) {
    // get next batch of events (blocking),
    //NOTE: this is a thread cancellation point
    int rcv_bytes = recv (us, (void *) sb, ss, 0);

    // trigger a transfer to SpiNNaker,
    spif_transfer (pipe, rcv_bytes);

    // and wait until spif finishes the transfer
    //NOTE: report if waiting for too long!
    int wc = 0;
    while (spif_busy (pipe)) {
      wc++;
      if (wc < 0) {
        log_time ();
        fprintf (lf, "error: spif not responding\n");
        (void) fflush (lf);
        wc = 0;
      }
    }
  }
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// receive events output commands through Ethernet UDP port
//
// terminated as a result of signal servicing
//--------------------------------------------------------------------
void * out_udp_listener (void * data) {
  int pipe = *((int *) data);

  // block signals - should be handled in a different thread
  sigset_t set;
  sigfillset (&set);
  sigprocmask (SIG_BLOCK, &set, NULL);

  // announce that listener is ready
  log_time ();
  fprintf (lf, "listening UDP %i for output commands\n",
           SPIFFER_UDP_PORT_BASE - (pipe + 1));
  (void) fflush (lf);

  client_addr_len[pipe] = sizeof (struct sockaddr);

  int us = out_udp_skt[pipe];
  int ds = SPIFFER_BATCH_SIZE;
  int dd[SPIFFER_BATCH_SIZE];

  while (1) {
    // wait for connection from output client
    //NOTE: keep client IP address and UDP port
    int rcv_bytes = recvfrom (us, dd, ds * sizeof (uint), 0,
                              (struct sockaddr *) &client_addr[pipe],
                              (socklen_t *) &client_addr_len[pipe]);

    // execute received output commands
    uint num_cmds = rcv_bytes / sizeof (uint);
    for (uint i = 0; i < num_cmds; i++) {
      uint cmd = dd[i] & SPIF_OUT_CMD_MASK;
      uint val = dd[i] & SPIF_OUT_VAL_MASK;
      log_time ();
      switch (cmd) {
      case SPIF_OUT_START:
        out_start[pipe] = 1;
        fprintf (lf, "starting outpipe%i\n", pipe);
        break;
      case SPIF_OUT_STOP:
        out_start[pipe] = 0;
        fprintf (lf, "stopping outpipe%i\n", pipe);
        break;
      case SPIF_OUT_SET_TICK:
        spif_set_out_tick (pipe, val);
        fprintf (lf, "setting outpipe%i tick to %i\n", pipe, val);
        break;
      case SPIF_OUT_SET_LEN:
        spif_set_out_len (pipe, val);
        fprintf (lf, "setting outpipe%i frame length to %i\n", pipe, val);
        break;
      default:
        fprintf (lf, "warning: unknown output command received UDP %i\n",
                 SPIFFER_UDP_PORT_BASE - (pipe + 1));
      }

      (void) fflush (lf);
    }
  }
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// sort USB devices by serial number and assign pipes
//
// no return value
//--------------------------------------------------------------------
void usb_sort_pipes () {
  // create a sorted list of serial numbers,
  int sorted[SPIFFER_USB_DISCOVER_CNT];

  // start with an unsorted list,
  for (int dv = 0; dv < usb_devs.cnt; dv++) {
    sorted[dv] = dv;
  }

  // straightforward bubble sort - small number of devices!
  for (int i = 0; i < (usb_devs.cnt - 1); i++) {
    for (int j = 0; j < (usb_devs.cnt - i - 1); j++) {
      if (strcmp (usb_devs.params[sorted[j]].sn, usb_devs.params[sorted[j + 1]].sn) > 0) {
        // swap
        int temp      = sorted[j];
        sorted[j]     = sorted[j + 1];
        sorted[j + 1] = temp;
      }
    }
  }

  // associate each device, in sorted order, with a spif pipe
  for (int p = 0; p < usb_devs.cnt; p++) {
    usb_devs.params[sorted[p]].pipe = p;
  }
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// attempt to discover new devices connected to the USB bus
// sort devices by serial number for consistent mapping to spif pipes
//
// discon_dev = disconnected USB device, SPIFFER_USB_NO_DEVICE for unknown
//--------------------------------------------------------------------
void usb_discover_devs (int discon_dev) {
  // start from a clean state,
  usb_devs.cnt = 0;

#ifdef META_SUPPORT
  // find prophesee devices (updates usb_devs.cnt),
  spiffer_meta_discover_devs ();
#endif

#ifdef CAER_SUPPORT
  // find Inivation devices (updates usb_devs.cnt),
  spiffer_caer_discover_devs ();
#endif

  // and assign pipes to devices (sorted by serial number)
  if (usb_devs.cnt > 0) {
    usb_sort_pipes ();
  }
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// survey USB devices
// called at start up and on USB device connection and disconnection
//
// data = pipe associated with disconnected USB device, -1 for unknown
//
// no return value
//--------------------------------------------------------------------
void usb_survey_devs (void * data) {
  // try to grab the lock - keep other threads out
  if (pthread_mutex_trylock (&usb_mtx)) {
    // other thread surveying USB devices - leave without doing anything
    return;
  };

  // report disconnected device
  int discon_dev;
  if (data == NULL) {
    discon_dev = SPIFFER_USB_NO_DEVICE;
  } else {
    discon_dev = *((int *) data);
    if (discon_dev < SPIFFER_USB_DISCOVER_CNT) {
      log_time ();
      fprintf (lf, "device %s disconnected\n", usb_devs.params[discon_dev].sn);
    }
  }

  // if needed shutdown input listeners and USB devices,
  if (discon_dev < SPIFFER_USB_DISCOVER_CNT) {
    spiffer_input_shutdown (discon_dev);
  }

  // try to discover supported USB devices
  usb_discover_devs (discon_dev);

  // start USB listeners on discovered devices
  for (int dv = 0; dv < usb_devs.cnt; dv++) {
    switch (usb_devs.params[dv].type) {
#ifdef CAER_SUPPORT
    case CAER:
      (void) pthread_create (&listener[usb_devs.params[dv].pipe], NULL, spiffer_caer_usb_listener, (void *) &dev_to_ptr[dv]);
      break;
#endif
#ifdef META_SUPPORT
    case META:
      (void) pthread_create (&listener[usb_devs.params[dv].pipe], NULL, spiffer_meta_usb_listener, (void *) &dev_to_ptr[dv]);
      break;
#endif
    default:
      log_time ();
      fprintf (lf, "warning: ignoring unsupported camera type\n");
      break;
    }
  }

  // and start UDP listeners on the rest of the pipes
  for (int pipe = usb_devs.cnt; pipe < pipe_num_in; pipe++) {
    (void) pthread_create (&listener[pipe], NULL, udp_listener, (void *) &dev_to_ptr[pipe]);
  }

  // release the lock
  pthread_mutex_unlock (&usb_mtx);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// initialise USB device state
//
// returns SPIFFER_OK on success or SPIFFER_ERROR on error
//--------------------------------------------------------------------
int usb_init (void) {
  for (int dev = 0; dev <= SPIFFER_USB_DISCOVER_CNT; dev++) {
    dev_to_ptr[dev] = dev;
  }

  usb_devs.cnt = 0;

#ifdef CAER_SUPPORT
  // Libcaer initialisation,
  spiffer_caer_init ();
#endif

#ifdef META_SUPPORT
  // Metavision SDK library initialisation,
  spiffer_meta_init ();
#endif

  return (SPIFFER_OK);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// initialise spif pipes
//
// returns SPIFFER_OK on success or SPIFFER_ERROR on error
//--------------------------------------------------------------------
int spif_pipes_init (void) {
  // spif data batch size - also buffer size
  size_t batch_size = SPIFFER_BATCH_SIZE * sizeof (uint);

  // try to open spif pipe 0
  pipe_fd[0] = spif_open (0);
  if (pipe_fd[0] == SPIFFER_ERROR) {
    log_time ();
    fprintf (lf, "error: unable to open pipe0 - ");
    switch (errno) {
    case ENOENT:
      fprintf (lf, "no spif device\n");
      break;
    case EBUSY:
      fprintf (lf, "spif device busy\n");
      break;
    case ENODEV:
      fprintf (lf, "no connection to SpiNNaker\n");
      break;
    default:
      fprintf (lf, "%s\n", strerror (errno));
    }

    return (SPIFFER_ERROR);
  }

  // report hardware version number and number of event pipes
  int spif_ver = spif_read_reg (0, SPIF_VERSION);
  pipe_num_in  = (spif_ver & SPIF_PIPES_MSK) >> SPIF_PIPES_SHIFT;
  pipe_num_out = (spif_ver & SPIF_OUTPS_MSK) >> SPIF_OUTPS_SHIFT;
  log_time ();
  fprintf (lf,
    "spif interface found [hw version %d.%d.%d - event pipes: in/%d out/%d]\n",
    (spif_ver & SPIF_MAJ_VER_MSK) >> SPIF_MAJ_VER_SHIFT,
    (spif_ver & SPIF_MIN_VER_MSK) >> SPIF_MIN_VER_SHIFT,
    (spif_ver & SPIF_PAT_VER_MSK) >> SPIF_PAT_VER_SHIFT,
    pipe_num_in,
    pipe_num_out
    );

  // check that hw and sw number of pipes are compatible
  int pipe_max_num = (pipe_num_in >= pipe_num_out) ? pipe_num_in : pipe_num_out;
  if (pipe_max_num > SPIF_HW_PIPES_NUM) {
    log_time ();
    fprintf (lf, "error: incompatible number of hw and sw pipes\n");
    return (SPIFFER_ERROR);
  }

  // open the rest of the pipes
  for (int pipe = 1; pipe < pipe_max_num; pipe++) {
    // open spif pipe
    pipe_fd[pipe] = spif_open (pipe);
    if (pipe_fd[pipe] == SPIFFER_ERROR) {
      log_time ();
      fprintf (lf, "error: unable to open spif pipe%i\n", pipe);
      return (SPIFFER_ERROR);
    }
  }

  // set up input buffers
  for (int pipe = 0; pipe < pipe_num_in; pipe++) {
    // get pipe buffer
    pipe_buf[pipe] = (uint *) spif_get_buffer (pipe, batch_size);
    if (pipe_buf[pipe] == NULL) {
      log_time ();
      fprintf (lf, "error: failed to get buffer for spif pipe%i\n", pipe);
      return (SPIFFER_ERROR);
    }
  }

  // set up output buffers
  for (int pipe = 0; pipe < pipe_num_out; pipe++) {
    // get pipe buffer
    pipe_out_buf[pipe] = (uint *) spif_get_output_buffer (pipe, batch_size);
    if (pipe_out_buf[pipe] == NULL) {
      log_time ();
      fprintf (lf, "error: failed to get output buffer for spif pipe%i\n", pipe);
      return (SPIFFER_ERROR);
    }
  }

  return (SPIFFER_OK);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// initialise system signal services
//
// returns SPIFFER_OK on success or SPIFFER_ERROR on error
//--------------------------------------------------------------------
int sig_init (void) {
  signal_usr1_taken = false;

  // set up USR1 signal servicing
  signal_usr1_cfg.sa_handler = &sig_usr1;
  signal_usr1_cfg.sa_flags = 0;
  sigemptyset (&signal_usr1_cfg.sa_mask);

  // register signal service routine
  if (sigaction (SIGUSR1,  &signal_usr1_cfg, NULL) == SPIFFER_ERROR) {
    return (SPIFFER_ERROR);
  }

  // set up TERM signal servicing
  signal_term_cfg.sa_handler = &sig_term;
  signal_term_cfg.sa_flags = 0;
  sigemptyset (&signal_term_cfg.sa_mask);
  sigaddset   (&signal_term_cfg.sa_mask, SIGTERM);

  return (sigaction (SIGTERM, &signal_term_cfg, NULL));
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// service TERM signal
//
// SIGTERM requests spiffer to stop
//
// no return value
//--------------------------------------------------------------------
void sig_term (int signum) {
  (void) signum;

  // spiffer shut down requested
  spiffer_stop (SPIFFER_OK);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// service USR1 signal
//
// SIGUSR1 indicates that a USB device has been connected
//
// no return value
//--------------------------------------------------------------------
void sig_usr1 (int signum) {
  (void) signum;

  // try to grab the lock - keep other threads out
  if (pthread_mutex_trylock (&signal_mtx)) {
    // other thread servicing the signal - leave without doing anything
    return;
  };

  if (signal_usr1_taken) {
    signal_usr1_taken = false;
  } else {
    // need to survey USB devices
    usb_survey_devs (NULL);

    // avoid responding to repeated signal instances
    sigset_t s;

    sigpending (&s);
    if (sigismember (&s, SIGUSR1)) {
      signal_usr1_taken = true;
    }
  }

  // release the lock
  pthread_mutex_unlock (&signal_mtx);
}
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// main thread:
// opens spif pipes
// sets up a UDP server per spif pipe
// creates a listener thread per spif pipe
// configures signal management
//--------------------------------------------------------------------
int main (int argc, char *argv[])
{
  (void) argc;
  (void) argv;

  // open log file,
  lf = fopen (log_name, "a");
  if (lf == NULL) {
    spiffer_stop (SPIFFER_ERROR);
  }

  // say hello,
  log_time ();
  fprintf (lf, "spiffer v%u.%u.%u started\n",
           SPIFFER_VER_MAJ, SPIFFER_VER_MIN, SPIFFER_VER_PAT);

  // open spif pipes and set up buffers,
  if (spif_pipes_init () == SPIFFER_ERROR) {
    spiffer_stop (SPIFFER_ERROR);
  }

  // initialise output control
  for (int pipe = 0; pipe < pipe_num_out; pipe++) {
    out_start[pipe] = 0;
  }

  // set up UDP servers - one for every pipe,
  if (udp_init () == SPIFFER_ERROR) {
    spiffer_stop (SPIFFER_ERROR);
  }

  // set up initial USB state and survey USB devices,
  if (usb_init () == SPIFFER_ERROR) {
    spiffer_stop (SPIFFER_ERROR);
  }

  // set up SpiNNaker listeners - one for every output pipe,
  if (spiNNaker_init () == SPIFFER_ERROR) {
    spiffer_stop (SPIFFER_ERROR);
  }

  // configure system signal service,
  if (sig_init () == SPIFFER_ERROR) {
    spiffer_stop (SPIFFER_ERROR);
  }

  // update log,
  (void) fflush (lf);

  // discover USB devces only if no USB-triggered signals already caused discovery
  //NOTE: allow enough time for signals to be procesed
  sleep (SPIFFER_SIG_DLY);
  if (usb_devs.cnt == 0) {
    usb_survey_devs (&dev_to_ptr[SPIFFER_USB_DISCOVER_CNT]);
  }

  // and go to sleep - let the listeners do the work
  while (1) {
    pause ();
  }
}
//--------------------------------------------------------------------
