// SpiNNaker API
#include "spin1_api.h"


// ------------------------------------------------------------------------
// constants
// ------------------------------------------------------------------------
#define TIMER_TICK_PERIOD  1000     // 1 ms
#define RUN_TIME           40       // (in seconds)
#define TIMEOUT            (RUN_TIME * 1000000 / TIMER_TICK_PERIOD)


// ------------------------------------------------------------------------
// macros
// ------------------------------------------------------------------------
#define ROUTE_TO_CORE(p)   (1 << (p + 6))

//NOTE: this packet key checks lower 3 bits of the event x coordinate
#define PKT_KEY(p)         (((p - 1) << 16) & PKT_MSK)
#define PKT_MSK            0x00070000


// ------------------------------------------------------------------------
// global variables
// ------------------------------------------------------------------------
uint rec_pkts = 0;


// ------------------------------------------------------------------------
// code
// ------------------------------------------------------------------------
uint app_init ()
{
  // initialise MC routing table entries
  // -------------------------------------------------------------------
  uint entry = rtr_alloc (1);
  if (entry == 0)
  {
    return (FAILURE);
  }

  uint core = spin1_get_core_id ();

  // initialise MC routing table entries
  // -------------------------------------------------------------------
  // set a MC routing table entry to catch packets that match my id lsb
  rtr_mc_set (entry,
	      PKT_KEY(core),
	      PKT_MSK,
	      ROUTE_TO_CORE(core)
    );

  // initialise router
  // -------------------------------------------------------------------
  // turn on packet error counter
  rtr[RTR_CONTROL] |= 0x00000030;

  return (SUCCESS);
}


void count_packets (uint key, uint payload)
{
  // count packets
  rec_pkts++;
}


void timertick (uint ticks, uint null)
{
  if (ticks >= TIMEOUT)
  {
    // finish simulation
    spin1_exit (0);
  }
}


// ------------------------------------------------------------------------
// main
// ------------------------------------------------------------------------
void c_main()
{
  io_printf (IO_BUF, ">> count_events\n");

  // initialize application
  uint res = app_init ();

  if (res == SUCCESS)
  {
    // set timer tick value (in microseconds)
    spin1_set_timer_tick (TIMER_TICK_PERIOD);

    // register callbacks
    spin1_callback_on (TIMER_TICK, timertick, -1);
    spin1_callback_on (MC_PACKET_RECEIVED, count_packets, 0);
    spin1_callback_on (MCPL_PACKET_RECEIVED, count_packets, 0);

    // go
    spin1_start (SYNC_NOWAIT);

    // report results
    sark.vcpu->user1 = rec_pkts;
    io_printf (IO_BUF, "received %u packets\n", rec_pkts);
  }
  else
  {
    io_printf (IO_BUF, "error: failed to initialise");
  }

  io_printf (IO_BUF, "<< count_events\n");
}
