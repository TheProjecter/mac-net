/****************************************************************
 * file:	frame_defines.h
 * date:	2009-03-13
 * description:	some frame definitions accord to 802.15.4
 ***************************************************************/
#ifndef FRAME_DEFINES_H
#define FRAME_DEFINES_H

#define LRWPAN_BCAST_SADDR       0xFFFF
#define LRWPAN_BCAST_PANID       0xFFFF
#define LRWPAN_SADDR_USE_LADDR   0xFFFE

#define LRWPAN_ACKFRAME_LENGTH 5

#define LRWPAN_MAX_MACHDR_LENGTH 23
#define LRWPAN_MAX_NETHDR_LENGTH 8
#define LRWPAN_MAX_APSHDR_LENGTH 5

#define LRWPAN_MAXHDR_LENGTH (LRWPAN_MAX_MACHDR_LENGTH+LRWPAN_MAX_NETHDR_LENGTH+LRWPAN_MAX_APSHDR_LENGTH)

#define LRWPAN_MAX_FRAME_SIZE 127
#define LRWPAN_RXBUFF_SIZE 11

#define LRWPAN_FRAME_TYPE_BEACON 0
#define LRWPAN_FRAME_TYPE_DATA 1
#define LRWPAN_FRAME_TYPE_ACK 2
#define LRWPAN_FRAME_TYPE_MAC 3

//BYTE masks
#define LRWPAN_FCF_SECURITY_MASK 0x8
#define LRWPAN_FCF_FRAMEPEND_MASK 0x10
#define LRWPAN_FCF_ACKREQ_MASK 0x20
#define LRWPAN_FCF_INTRAPAN_MASK 0x40

#define LRWPAN_SET_FRAME_TYPE(x,f)     (x=x|f)
#define LRWPAN_GET_FRAME_TYPE(x)     (x&0x03)

//common macros
#define BITSET(var,bitno) ((var) |= (1 << (bitno)))
#define BITCLR(var,bitno) ((var) &= ~(1 << (bitno)))
#define BITTST(var,bitno) (var & (1 << (bitno)))

#define LRWPAN_SET_SECURITY_ENABLED(x) BITSET(x,3)
#define LRWPAN_SET_FRAME_PENDING(x)    BITSET(x,4)
#define LRWPAN_SET_ACK_REQUEST(x)      BITSET(x,5)
#define LRWPAN_SET_INTRAPAN(x)         BITSET(x,6)

#define LRWPAN_CLR_SECURITY_ENABLED(x) BITCLR(x,3)
#define LRWPAN_CLR_FRAME_PENDING(x)    BITCLR(x,4)
#define LRWPAN_CLR_ACK_REQUEST(x)      BITCLR(x,5)
#define LRWPAN_CLR_INTRAPAN(x)         BITCLR(x,6)

#define LRWPAN_GET_SECURITY_ENABLED(x) BITTST(x,3)
#define LRWPAN_GET_FRAME_PENDING(x)    BITTST(x,4)
#define LRWPAN_GET_ACK_REQUEST(x)      BITTST(x,5)
#define LRWPAN_GET_INTRAPAN(x)         BITTST(x,6)

#define LRWPAN_ADDRMODE_NOADDR 0
#define LRWPAN_ADDRMODE_SADDR  2
#define LRWPAN_ADDRMODE_LADDR  3

#define LRWPAN_GET_DST_ADDR(x) ((x>>2)&0x3)
#define LRWPAN_GET_SRC_ADDR(x) ((x>>6)&0x3)
#define LRWPAN_SET_DST_ADDR(x,f) (x=x|(f<<2))
#define LRWPAN_SET_SRC_ADDR(x,f) (x=x|(f<<6))

#define LRWPAN_FCF_DSTMODE_MASK   (0x03<<2)
#define LRWPAN_FCF_DSTMODE_NOADDR (LRWPAN_ADDRMODE_NOADDR<<2)
#define LRWPAN_FCF_DSTMODE_SADDR (LRWPAN_ADDRMODE_SADDR<<2)
#define LRWPAN_FCF_DSTMODE_LADDR (LRWPAN_ADDRMODE_LADDR<<2)

#define LRWPAN_FCF_SRCMODE_MASK   (0x03<<6)
#define LRWPAN_FCF_SRCMODE_NOADDR (LRWPAN_ADDRMODE_NOADDR<<6)
#define LRWPAN_FCF_SRCMODE_SADDR (LRWPAN_ADDRMODE_SADDR<<6)
#define LRWPAN_FCF_SRCMODE_LADDR (LRWPAN_ADDRMODE_LADDR<<6)

#define LRWPAN_IS_ACK(x) (LRWPAN_GET_FRAME_TYPE(x) == LRWPAN_FRAME_TYPE_ACK)
#define LRWPAN_IS_BCN(x) (LRWPAN_GET_FRAME_TYPE(x) == LRWPAN_FRAME_TYPE_BEACON)
#define LRWPAN_IS_MAC(x) (LRWPAN_GET_FRAME_TYPE(x) == LRWPAN_FRAME_TYPE_MAC)
#define LRWPAN_IS_DATA(x) (LRWPAN_GET_FRAME_TYPE(x) == LRWPAN_FRAME_TYPE_DATA)

#endif

