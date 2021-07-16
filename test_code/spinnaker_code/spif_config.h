#ifndef __SPIF_CFG_H__
#define __SPIF_CFG_H__

// configuration routing keys
#define PER_KEY            0xfffe0000
#define PER_MSK            0xffff0000
#define LCMD_KEY           0xfffffe00
#define LCMD_MSK           0xffffff00
#define RCMD_KEY           0xffffff00
#define RCMD_MSK           0xffffff00
#define RPLY_KEY           0xfffffd00
#define RPLY_MSK           0xffffff00

// local configuration commands
//NOTE: in most cases payload carries the value
#define WR_PKEY_CMD        2
#define WR_PMSK_CMD        3
#define WR_LKEY_CMD        12
#define WR_LMSK_CMD        13
#define WR_RKEY_CMD        14
#define WR_RMSK_CMD        15
#define STOP_CMD           16
#define START_CMD          17

// remote configuration commands
//NOTE: in most cases payload carries the value
#define NUM_RMT_CNT        4
#define RWR_MPK_CMD        1
#define RWR_RYK_CMD        2
#define RWR_IDW_CMD        3
#define RWR_ODW_CMD        4
#define RWR_KEY_CMD        16
#define RWR_MSK_CMD        32
#define RWR_RTE_CMD        48
#define RWR_CPP_CMD        64
#define RWR_CCP_CMD        65
#define RWR_CDP_CMD        66
#define RWR_CIP_CMD        67
#define RWR_MPM_CMD        80
#define RWR_MPS_CMD        96

// spif is always connected to the SOUTH link of chip (0, 0)
#define SPIF_ROUTE         (1 << 5)
#define CORE_ROUTE(core)   (1 << (core + 6))

uint spif_init ()
{
  // initialise spif configuration MC routing table entries
  // -----------------------------------------------------------------
  uint entry = rtr_alloc (3);
  if (entry == 0) {
    return (FAILURE);
  }

  // setup local configuration route
  rtr_mc_set (entry,
               LCMD_KEY,
               LCMD_MSK,
               SPIF_ROUTE
             );

  // setup remote configuration route
  rtr_mc_set (entry + 1,
               RCMD_KEY,
               RCMD_MSK,
               SPIF_ROUTE
             );

  // identify this core for reply messages
  uint core = spin1_get_core_id ();

  // setup remote reply configuration route
  rtr_mc_set (entry + 2,
               RPLY_KEY,
               RPLY_MSK,
               CORE_ROUTE(core)
             );

  return (SUCCESS);
}


void spif_set_peripheral_key (uint key)
{
  while (!spin1_send_mc_packet (
          LCMD_KEY | WR_PKEY_CMD,
          key,
          WITH_PAYLOAD)
        );
}


void spif_set_peripheral_mask (uint mask)
{
  while (!spin1_send_mc_packet (
          LCMD_KEY | WR_PMSK_CMD,
          mask,
          WITH_PAYLOAD)
        );
}


void spif_set_routing_key (uint entry, uint key)
{
  while (!spin1_send_mc_packet (
          RCMD_KEY | RWR_KEY_CMD + entry,
          key,
          WITH_PAYLOAD)
        );
}


void spif_set_routing_mask (uint entry, uint mask)
{
  while (!spin1_send_mc_packet (
          RCMD_KEY | RWR_MSK_CMD + entry,
          mask,
          WITH_PAYLOAD)
        );
}


void spif_set_routing_route (uint entry, uint route)
{
  while (!spin1_send_mc_packet (
          RCMD_KEY | RWR_RTE_CMD + entry,
          route,
          WITH_PAYLOAD)
        );
}


void spif_set_input_drop_wait (uint wait)
{
  while (!spin1_send_mc_packet (
          RCMD_KEY | RWR_IDW_CMD,
          wait,
          WITH_PAYLOAD)
        );
}


void spif_set_mapper_key (uint key)
{
  while (!spin1_send_mc_packet (
          RCMD_KEY | RWR_MPK_CMD,
          key,
          WITH_PAYLOAD)
        );
}


void spif_set_mapper_field_mask (uint field, uint mask)
{
  while (!spin1_send_mc_packet (
          RCMD_KEY | RWR_MPM_CMD + field,
          mask,
          WITH_PAYLOAD)
        );
}


void spif_set_mapper_field_shift (uint field, uint shift)
{
  while (!spin1_send_mc_packet (
          RCMD_KEY | RWR_MPS_CMD + field,
          shift,
          WITH_PAYLOAD)
        );
}


void spif_start_input (void)
{
  while (!spin1_send_mc_packet (
          LCMD_KEY | START_CMD,
          0,
          NO_PAYLOAD)
        );
}


void spif_stop_input (void)
{
  while (!spin1_send_mc_packet (
          LCMD_KEY | STOP_CMD,
          0,
          NO_PAYLOAD)
        );
}


void spif_read_counter (uint counter)
{
  // send counter read request
  while (!spin1_send_mc_packet (
          RCMD_KEY | counter,
          0,
          NO_PAYLOAD)
        );
}


void spif_reset_counters ()
{
  // clear spif packet counters
  for (uint i = 0; i < NUM_RMT_CNT; i++) {
     while (!spin1_send_mc_packet (
             RCMD_KEY | RWR_CPP_CMD + i,
             0,
             WITH_PAYLOAD)
           );
  }
}


#endif /* __SPIF_CFG_H__ */
