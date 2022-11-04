//************************************************//
//*                                              *//
//* spif hardware definitions                    *//
//*                                              *//
//* lap - 21/06/2021                             *//
//*                                              *//
//************************************************//

#ifndef __SPIF_H__
#define __SPIF_H__


//--------------------------------------------------------------------
// spif constants
//--------------------------------------------------------------------
#define SPIF_HW_PIPES_NUM     2     //
#define SPIF_BUF_MAX_SIZE     4096  // KB reserved memory

#define SPIF_ROUTER_NUM       16    // number of ROUTER registers
#define SPIF_DCREGS_NUM       5     // number of DIAGNOSTIC COUNTER registers
#define SPIF_MPREGS_NUM       4     // number of MAPPER registers per pipe
#define SPIF_FLREGS_NUM       8     // number of FILTER registers per pipe
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// spif configuration registers
//--------------------------------------------------------------------
#define SPIF_CONTROL          0
#define SPIF_REPLY_KEY        2
#define SPIF_IN_DROP_WAIT     3
#define SPIF_OUT_DROP_WAIT    4
#define SPIF_OUT_TICK         5
#define SPIF_OUT_LEN          6
#define SPIF_STATUS           14
#define SPIF_VERSION          15
#define SPIF_ROUTER_KEY       16    // section of SPIF_ROUTER_NUM registers
#define SPIF_ROUTER_MASK      32    // section of SPIF_ROUTER_NUM registers
#define SPIF_ROUTER_ROUTE     48    // section of SPIF_ROUTER_NUM registers
#define SPIF_COUNT_OUT_DROP   64
#define SPIF_COUNT_OUT        65
#define SPIF_COUNT_IN_DROP    66
#define SPIF_COUNT_IN         67
#define SPIF_COUNT_CONFIG     68
#define SPIF_MAPPER_KEY       80    // section of 1 register/pipe
#define SPIF_MAPPER_MASK      96    // section of SPIF_MPREGS_NUM registers/pipe
#define SPIF_MAPPER_SHIFT     112   // section of SPIF_MPREGS_NUM registers/pipe
#define SPIF_MAPPER_LIMIT     128   // section of SPIF_MPREGS_NUM registers/pipe
#define SPIF_FILTER_VALUE     144   // section of SPIF_FLREGS_NUM registers/pipe
#define SPIF_FILTER_MASK      176   // section of SPIF_FLREGS_NUM registers/pipe
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// spif configuration register field masks and shifts
//--------------------------------------------------------------------
#define SPIF_SEC_CODE         0x5ec00000
#define SPIF_SEC_MSK          0xfff00000
#define SPIF_HS_MSK           0x00010000
#define SPIF_PAT_VER_MSK      0x000000ff
#define SPIF_MIN_VER_MSK      0x0000ff00
#define SPIF_MAJ_VER_MSK      0x00ff0000
#define SPIF_PIPES_MSK        0x0f000000
#define SPIF_OUTPS_MSK        0xf0000000
#define SPIF_PAT_VER_SHIFT    0
#define SPIF_MIN_VER_SHIFT    8
#define SPIF_MAJ_VER_SHIFT    16
#define SPIF_PIPES_SHIFT      24
#define SPIF_OUTPS_SHIFT      28
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// spif output control commands
//--------------------------------------------------------------------
#define SPIF_OUT_START        0x5ec00000
#define SPIF_OUT_STOP         0x5ec10000
#define SPIF_OUT_SET_TICK     0x5ec20000
#define SPIF_OUT_SET_LEN      0x5ec40000

#define SPIF_OUT_CMD_MASK     0xffff0000
#define SPIF_OUT_VAL_MASK     0x0000ffff
//--------------------------------------------------------------------


#endif /* __SPIF_H__ */
