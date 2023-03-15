// SpiNNaker API
#include "spin1_api.h"
#include "spif_local.h"


// ------------------------------------------------------------------------
// constants
// ------------------------------------------------------------------------
#define TIMER_TICK_PERIOD  1000       // (in useconds)
#define RUN_TIME           1          // (in seconds)
#define TIMEOUT            (RUN_TIME * 1000000 / TIMER_TICK_PERIOD)

#define EAST               0
#define NORTH_EAST         1
#define NORTH              2
#define WEST               3
#define SOUTH_WEST         4
#define SOUTH              5

#define OUT_LINK           SOUTH

// throttle packet injection
#define THROTTLE           7
//#define THROTTLE           0

// keep track of spif counters read
#define SENT_CTR           1
#define DROP_CTR           2


// ------------------------------------------------------------------------
// macros
// ------------------------------------------------------------------------
#define ROUTE_TO_LINK(l)   (1 << l)


// ------------------------------------------------------------------------
// global variables
// ------------------------------------------------------------------------
uint sent_pkts = 0;
uint cnt_spif_pkts;
uint cnt_spif_drop;
uint cnt_spif = 0;

volatile uchar stop = 0;


// ------------------------------------------------------------------------
// code
// ------------------------------------------------------------------------
void start_spif (uint a, uint b)
{
  (void) a;
  (void) b;

  // clear all spif packet counters
  spif_reset_counters ();

  // configure peripheral routing
  spif_set_peripheral_key (PER_KEY);
  spif_set_peripheral_mask (PER_MSK);
}


uint app_init ()
{
  // initialise MC routing table entries
  // -------------------------------------------------------------------
  uint entry = rtr_alloc (1);
  if (entry == 0) {
    return (FAILURE);
  }

  // setup peripheral packet route
  rtr_mc_set (entry,
               PER_KEY,
               PER_MSK,
               ROUTE_TO_LINK (OUT_LINK)
             );

  if (leadAp) {
    // initialise spif configuration MC routing table entries
    // -----------------------------------------------------------------
    if (spif_init () == FAILURE) {
      return (FAILURE);
    }

    // kick-start spif configuration
    // -----------------------------------------------------------------
    spin1_schedule_callback (start_spif, 0, 0, 1);

    // initialise router
    // -------------------------------------------------------------------
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


void send_pkts (uint a, uint b)
{
  (void) a;
  (void) b;

  uint * pca = (uint *) 0x70000000;
  uint * pva = (uint *) 0x70000004;

  uint pc = *pca;
  uint pv = *pva;

  for (uint i = 0; i < pc; i++) {
    while (!(cc[CC_TCR] & 0x10000000));  // tx not full
    cc[CC_TXKEY] = PER_KEY | (pv++ & ~PER_MSK);
    sent_pkts++;

    for (uint i = 0; i < THROTTLE; i++);
  }
}


void rcv_replies (uint key, uint payload)
{
  // check key for counter ID
  if ((key & RPLY_MSK) == RPLY_KEY) {
    if ((key & ~RPLY_MSK) == SPIF_COUNT_OUT) {
      cnt_spif_pkts = payload;
      sark.vcpu->user2 = payload;
      cnt_spif |= SENT_CTR;
      return;
    }

    if ((key & ~RPLY_MSK) == SPIF_COUNT_OUT_DROP) {
      cnt_spif_drop = payload;
      sark.vcpu->user3 = payload;
      cnt_spif |= DROP_CTR;
      return;
    }
  }
}


void test_control (uint ticks, uint null)
{
  (void) null;

  // interface management done by leadAp only
  if (leadAp) {
    // allow peripheral input - spif replies
    if (ticks == 1) {
      spif_start_input ();
    }

    // read spif packet counter
    //NOTE: must have callback in place to receive reply
    if (ticks == (TIMEOUT + 9)) {
      spif_read_counter (SPIF_COUNT_OUT);
      spif_read_counter (SPIF_COUNT_OUT_DROP);
    }

    // make sure that all packets are received!
    if (ticks > (TIMEOUT + 10)) {
      // stop peripheral input
      spif_stop_input ();

      // turn off peripheral routing
      spif_set_peripheral_key (0xffffffff);
      spif_set_peripheral_mask (0);
    }
  }

  // kick-start the sending of packets
  if (ticks == 1) {
    spin1_schedule_callback (send_pkts, 0, 0, 3);
  }
  
  // stop sending packets
  if (ticks == (TIMEOUT + 1)) {
    stop = 1;
  }

  // make sure that all packets are received!
  if (ticks > (TIMEOUT + 10)) {
    // finish simulation
    spin1_exit (0);
  }
}


void c_main ()
{
  io_printf (IO_BUF, ">> fire_out\n");

  // initialise application
  uint res = app_init ();

  if (res == SUCCESS) {
    // set timer tick value (in microseconds)
    spin1_set_timer_tick (TIMER_TICK_PERIOD);

    // register callbacks
    spin1_callback_on (TIMER_TICK, test_control, -1);

    //NOTE: PACKET_RECEIVED callbacks needed for spif replies! 
    spin1_callback_on (MC_PACKET_RECEIVED, rcv_replies, 0);
    spin1_callback_on (MCPL_PACKET_RECEIVED, rcv_replies, 0);

    // go
    spin1_start (SYNC_WAIT);

    // report results
    sark.vcpu->user0 = sent_pkts;
    io_printf (IO_BUF, "sent %d packets\n", sent_pkts);

    if (leadAp) {
      if (cnt_spif & SENT_CTR) {
        io_printf (IO_BUF, "spif reports %d packets received\n", cnt_spif_pkts);
      } else {
        io_printf (IO_BUF, "spif received packet read failed\n", cnt_spif_pkts);
      }

      if (cnt_spif & DROP_CTR) {
        io_printf (IO_BUF, "spif reports %d packets dropped\n", cnt_spif_drop);
      } else {
        io_printf (IO_BUF, "spif dropped packet read failed\n", cnt_spif_pkts);
      }
    }
  } else {
    io_printf (IO_BUF, "error: failed to initialise");
  }

  io_printf (IO_BUF, "<< fire_out\n");
}
