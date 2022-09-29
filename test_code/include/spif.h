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
#define SPIF_OUT_FRM_PERIOD   5
#define SPIF_OUT_FRM_SIZE     6
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


#endif /* __SPIF_H__ */
