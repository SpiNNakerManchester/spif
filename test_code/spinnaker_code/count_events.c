// SpiNNaker API
#include "spin1_api.h"
#include "spif_config.h"


// ------------------------------------------------------------------------
// constants
// ------------------------------------------------------------------------
#define TIMER_TICK_PERIOD  1000     // 1 ms
#define RUN_TIME           40       // (in seconds)
#define TIMEOUT            (RUN_TIME * 1000000 / TIMER_TICK_PERIOD)

#define EAST               0
#define NORTH_EAST         1
#define NORTH              2
#define WEST               3
#define SOUTH_WEST         4
#define SOUTH              5

#define OUT_LINK           SOUTH


// ------------------------------------------------------------------------
// macros
// ------------------------------------------------------------------------
#define ROUTE_TO_CORE(p)   (1 << (p + 6))
#define ROUTE_TO_LINK(l)   (1 << l)


//NOTE: this packet key checks lower 3 bits of the event x coordinate
#define COORD_SHIFT        3
#define PKT_KEY(p)         (((p - 1) << COORD_SHIFT) | MPR_KEY)
#define PKT_MSK            ((0x07 << COORD_SHIFT) | 0xffff0000)

//event to packet mapper parameters
#define MPR_KEY            0xeeee0000
#define MPR_MASK_0         0x0000ffff


// ------------------------------------------------------------------------
// global variables
// ------------------------------------------------------------------------
uchar lead_0_0;
uint  rec_pkts = 0;

// ------------------------------------------------------------------------
// code
// ------------------------------------------------------------------------
void start_spif (uint a, uint b)
{
  (void) a;
  (void) b;

  // clear all spif packet counters
  spif_reset_counters ();

  // configure peripheral input routing table
  //NOTE: route based on 3 least-significant bits
  for (uint i = 0; i < 8; i++) {
	spif_set_routing_key (i, i);

	spif_set_routing_mask (i, 0x00000007);

	spif_set_routing_route (i, i);
  }

  // configure peripheral input mapper
  //NOTE: map event directly to key
  spif_set_mapper_key (MPR_KEY);

  spif_set_mapper_mask (0, MPR_MASK_0);
}


uint app_init ()
{
  uint core = spin1_get_core_id ();

  // initialise MC routing table entries
  // -------------------------------------------------------------------
  uint entry = rtr_alloc (1);
  if (entry == 0) {
    return (FAILURE);
  }

  // set a MC routing entry to catch packets that match my id lsb
  rtr_mc_set (entry,
	      PKT_KEY(core),
	      PKT_MSK,
	      ROUTE_TO_CORE(core)
    );

  if (lead_0_0) {
    // initialise spif configuration MC routing table entries
    // -----------------------------------------------------------------
    if (spif_init () == FAILURE) {
      return (FAILURE);
    }

    // kick-start spif configuration
    // -----------------------------------------------------------------
    spin1_schedule_callback (start_spif, 0, 0, 1);
  }

  // initialise router
  // -------------------------------------------------------------------
  if (leadAp) {
    // turn on packet error counter
    rtr[RTR_CONTROL] |= 0x00000030;
  }

  // initialise user variables - for result reporting
  // -------------------------------------------------------------------
  sark.vcpu->user0 = 0;
  sark.vcpu->user1 = 0;
  sark.vcpu->user2 = 0;
  sark.vcpu->user3 = 0;

  return (SUCCESS);
}


void count_packets (uint key, uint payload)
{
  (void) key;
  (void) payload;

  // count packets
  rec_pkts++;
}


void timertick (uint ticks, uint null)
{
  (void) null;

  if (ticks == 1) {
    // allow peripheral input
    if (lead_0_0) {
      spif_start_input ();
    }
  }
  
  if (ticks >= TIMEOUT)
  {
    // stop peripheral input
    if (lead_0_0) {
      spif_stop_input ();
    }

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

  // the leadAp core on (0, 0) configures peripheral routes
  lead_0_0 = leadAp && (spin1_get_chip_id () == 0);

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
