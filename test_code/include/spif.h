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
#define SPIF_DCREGS_NUM       4     // number of DIAGNOSTIC COUNTER registers
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
#define SPIF_STATUS           14
#define SPIF_VERSION          15
#define SPIF_ROUTER_KEY       16    // section of SPIF_ROUTER_NUM registers
#define SPIF_ROUTER_MASK      32    // section of SPIF_ROUTER_NUM registers
#define SPIF_ROUTER_ROUTE     48    // section of SPIF_ROUTER_NUM registers
#define SPIF_COUNT_OUT        64
#define SPIF_COUNT_CONFIG     65
#define SPIF_COUNT_IN_DROP    66
#define SPIF_COUNT_IN         67
#define SPIF_MAPPER_KEY       80    // section of 1 register/pipe
#define SPIF_MAPPER_MASK      96    // section of 4 registers/pipe
#define SPIF_MAPPER_SHIFT     112   // section of 4 registers/pipe
#define SPIF_MAPPER_LIMIT     128   // section of 4 registers/pipe
#define SPIF_FILTER_VALUE     144   // section of 8 registers/pipe
#define SPIF_FILTER_MASK      176   // section of 8 registers/pipe
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// spif DMA controller registers
//--------------------------------------------------------------------
#define DMACR                 0     // control
#define DMASR                 1     // status
#define DMASA                 6     // source 
#define DMALEN                10    // length
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// spif DMA controller command / status constants
//--------------------------------------------------------------------
#define DMA_RESET             0x04
#define DMA_RUN               0x01
#define DMA_IDLE_MSK          0x02
//--------------------------------------------------------------------


#endif /* __SPIF_H__ */
