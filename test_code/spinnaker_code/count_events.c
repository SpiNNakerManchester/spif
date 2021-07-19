// SpiNNaker API
#include "spin1_api.h"
#include "spif_config.h"


// ------------------------------------------------------------------------
// constants
// ------------------------------------------------------------------------
#define TIMER_TICK_PERIOD  1000     // 1 ms
#define RUN_TIME           60       // (in seconds)
#define TIMEOUT            (RUN_TIME * 1000000 / TIMER_TICK_PERIOD)

#define EAST               0
#define NORTH_EAST         1
#define NORTH              2
#define WEST               3
#define SOUTH_WEST         4
#define SOUTH              5

#define OUT_LINK           SOUTH

//event to packet mapper parameters
#define MPR_KEY            0xeeee0000
#define MPF_MASK_0         0x0000ffff

// packet dropping parameters
#define PKT_DROP_WAIT      512

#define SPIF_SNT_CNT       0x1
#define SPIF_DRP_CNT       0x2


// ------------------------------------------------------------------------
// macros
// ------------------------------------------------------------------------
#define ROUTE_TO_CORE(p)   (1 << (p + 6))
#define ROUTE_TO_LINK(l)   (1 << l)


//NOTE: this packet key checks lower 3 bits of the event x coordinate
#define COORD_SHIFT        3
#define PKT_KEY(p)         (MPR_KEY | ((p - 1) << COORD_SHIFT))
#define PKT_MSK            (0xffff0000 | (0x07 << COORD_SHIFT))


// ------------------------------------------------------------------------
// global variables
// ------------------------------------------------------------------------
uchar lead_0_0;
uint  rec_pkts = 0;
uint  spif_cnt_pkts;
uint  spif_cnt_drop;
uint  spif_cnt = 0;

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

  spif_set_mapper_field_mask (0, MPF_MASK_0);

  // ajust peripheral input wait-before-drop value
  spif_set_input_drop_wait (PKT_DROP_WAIT);
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


void rcv_replies (uint key, uint payload)
{
  // check key for counter ID
  if ((key & RPLY_MSK) == RPLY_KEY) {
    if ((key & ~RPLY_MSK) == RWR_CIP_CMD) {
      spif_cnt_pkts = payload;
      sark.vcpu->user2 = payload;
      spif_cnt |= SPIF_SNT_CNT;
      return;
    }

    if ((key & ~RPLY_MSK) == RWR_CDP_CMD) {
      spif_cnt_drop = payload;
      sark.vcpu->user3 = payload;
      spif_cnt |= SPIF_DRP_CNT;
      return;
    }
  }
}


void count_packets (uint key, uint payload)
{
  (void) key;
  (void) payload;

  // count peripheral packets
  rec_pkts++;
}


void test_control (uint ticks, uint null)
{
  (void) null;

  // interface management done by lead_0_0 only
  if (lead_0_0) {
    // allow peripheral input
    if (ticks == 1) {
      spif_start_input ();
    }
  
    // read spif packet counter
    //NOTE: must have callback in place to receive reply
    if (ticks == (TIMEOUT - 1)) {
      // register callbacks to service spif replies
      spin1_callback_on (MC_PACKET_RECEIVED, rcv_replies, 0);
      spin1_callback_on (MCPL_PACKET_RECEIVED, rcv_replies, 0);

      // send counter read requests
      spif_read_counter (RWR_CIP_CMD);
      spif_read_counter (RWR_CDP_CMD);
    }

    // stop peripheral input
    if (ticks >= TIMEOUT) {
      spif_stop_input ();
    }
  }

  // finish simulation
  if (ticks >= TIMEOUT)
  {
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
    spin1_callback_on (TIMER_TICK, test_control, -1);
    spin1_callback_on (MC_PACKET_RECEIVED, count_packets, 0);
    spin1_callback_on (MCPL_PACKET_RECEIVED, count_packets, 0);

    // go
    spin1_start (SYNC_NOWAIT);

    // report results
    sark.vcpu->user1 = rec_pkts;
    io_printf (IO_BUF, "received %u packets\n", rec_pkts);

    if (lead_0_0) {
      if (spif_cnt & SPIF_SNT_CNT) {
        io_printf (IO_BUF, "spif reports %d packets sent\n", spif_cnt_pkts);
      } else {
        io_printf (IO_BUF, "spif sent packet read failed\n", spif_cnt_pkts);
      }

      if (spif_cnt & SPIF_DRP_CNT) {
        io_printf (IO_BUF, "spif reports %d packets dropped\n", spif_cnt_drop);
      } else {
        io_printf (IO_BUF, "spif dropped packet read failed\n", spif_cnt_pkts);
      }
    }
  }
  else
  {
    io_printf (IO_BUF, "error: failed to initialise");
  }

  io_printf (IO_BUF, "<< count_events\n");
}
