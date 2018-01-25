/**
 * @file    m68k_mem.s
 * @author  Benjamin Gerard AKA Ben/OVR
 * @date    2017-09
 * @brief   m68K assembler routines mem family functions
 *
 */

        .cpu    68000
        .arch   68000

        .section .text
        .global m68k_memcmp,m68k_memset,m68k_memcpy

/// --------------------------------------------------------
///                   a0      a1     d1/d0
/// int m68k_memcmp(astr.l, bstr.l, cnt.l)
///                 4(a7)   8(a7)   12(a7)

m68k_memcmp:
        move.l  12(a7),d0
        beq.s   0f

        movem.l 4(a7),a0-a1
        cmpa.l  a0,a1
        beq.s   0f

        subq.w  #1,d0
        swap    d0
        addq.w  #1,d0

4:      swap    d0
1:      cmpm.b  (a1)+,(a0)+
        dbne    d0,1b
        beq.s   3f
        bhi.s   1f

2:      || a < b
        moveq   #-1,d0
        rts

3:      || continue
        swap    d0
        subq.w  #1,d0
        bne.s   4b

0:      || a == b
        moveq   #0,d0
        rts

1:      || a > b
        moveq   #1,d0
        rts

/// --------------------------------------------------------
///                   a0     d1     d0
/// void * m68k_memset(dst.l, val.b, cnt.l)
///                  4(a7)  9(a7)  10(a7)

m68k_memset:
        move.l  10(a7),d0       | d0.l: cnt
        beq.s   0f

        move.l  4(a7),a0
        moveq   #0,d1
        move.b  9(a7),d1
        move.l  d1,a1

        move.l  a0,d1
        btst    #0,d1
        beq.s   1f

        move.w  a1,d1
        move.b  d1,(a0)+

        subq.l  #1,d0
        beq.s   0f

1:      || Aligned
        move.l  a1,d1
        move.b  d1,-(a7)
        move.w  (a7),d1
        move.b  (a7)+,d1
        move.w  d1,a1
        swap    d1
        move.w  a1,d1

        move.l  d0,a1
        lsr.l   #4,d0
        beq.s   5f

        subq    #1,d0
        swap    d0
        addq    #1,d0
2:      swap    d0
1:      move.l  d1,(a0)+
        move.l  d1,(a0)+
        move.l  d1,(a0)+
        move.l  d1,(a0)+
        dbf     d0,1b
        swap    d0
        subq.w  #1,d0
        bne.s   2b

5:      move.w  a1,d0
        btst    #3,d0
        beq.s   5f
        move.l  d1,(a0)+
        move.l  d1,(a0)+
5:      btst    #2,d0
        beq.s   5f
        move.l  d1,(a0)+
5:      btst    #1,d0
        beq.s   5f
        move.w  d1,(a0)+
5:      btst    #0,d0
        beq.s   0f
        move.b  d1,(a0)+

0:      move.l  4(a7),d0
        rts


/// --------------------------------------------------------
///                     a0     a1     d0
/// void * m68k_memcpy(dst.l, val.l, cnt.l)
///                    4(a7)  8(a7)  12(a7)

m68k_memcpy:
        move.l  12(a7),d0       | d0.l: cnt
        beq     0f

        movem.l 4(a7),a0-a1
        move.l  a1,d1
        sub.l   a0,d1
        beq     0f
        bhi     m68k_fcpy

        || ----------------------------------------
        || Backward copy
        || ----------------------------------------
m68k_bcpy:
        add.l   d0,a0
        add.l   d0,a1

        btst    #0,d1           | Same alignment ?
        bne     slow_bcpy       | no ? slow mode :(

        move.l  a0,d1
        btst    #0,d1
        beq.s   1f

        move.b  -(a1),-(a0)
        subq.l  #1,d0
        beq     0f

1:      || aligned
        move.l  d0,d1           | d1.l: cnt
        lsr.l   #5,d0
        beq.s   cont_bcpy

        cmp.l   #15,d0
        bpl     fast_bcpy

        subq.w  #1,d0
1:
        .rept   8
        move.l  -(a1),-(a0)
        .endr
        dbf     d0,1b

cont_bcpy:
        btst    #4,d1
        beq.s   5f
        move.l  -(a1),-(a0)
        move.l  -(a1),-(a0)
        move.l  -(a1),-(a0)
        move.l  -(a1),-(a0)
5:      btst    #3,d1
        beq.s   5f
        move.l  -(a1),-(a0)
        move.l  -(a1),-(a0)
5:      btst    #2,d1
        beq.s   5f
        move.l  -(a1),-(a0)
5:      btst    #1,d1
        beq.s   5f
        move.w  -(a1),-(a0)
5:      btst    #0,d1
        beq.s   5f
        move.b  -(a1),-(a0)
5:      move.l  a0,d0
        rts
        || ----------------------------------------

fast_bcpy:
        movem.l d2-d7/a2-a3,-(a7)
        subq.w  #1,d0
        swap    d0
        addq.w  #1,d0
2:      swap    d0
1:      lea     -32(a1),a1
        movem.l (a1),d2-d7/a2-a3
        movem.l d2-d7/a2-a3,-(a0)
        dbf     d0,1b
        swap    d0
        subq.w  #1,d0
        bne.s   2b
        movem.l (a7)+,d2-d7/a2-a3
        bra.s   cont_bcpy
        || ----------------------------------------

slow_bcpy:
        subq.w  #1,d0
        swap    d0
        addq.w  #1,d0
2:      swap    d0
1:      move.b  -(a1),-(a0)
        dbf     d0,1b
        swap    d0
        subq.w  #1,d0
        bne.s   2b
        move.l  a0,d0
        rts


        || ----------------------------------------
        || Forward copy
        || ----------------------------------------
m68k_fcpy:
        btst    #0,d1           | Same alignment ?
        bne     slow_fcpy       | no ? slow mode :(

        move.l  a0,d1
        btst    #0,d1
        beq.s   1f

        move.b  (a1)+,(a0)+
        subq.l  #1,d0
        beq     0f

1:      || aligned
        move.l  d0,d1           | d1.l: cnt
        lsr.l   #5,d0
        beq.s   cont_fcpy

        cmp.l   #15,d0
        bpl     fast_fcpy

        subq.w  #1,d0
1:
        .rept   8
        move.l  (a1)+,(a0)+
        .endr
        dbf     d0,1b


cont_fcpy:
        btst    #4,d1
        beq.s   5f
        move.l  (a1)+,(a0)+
        move.l  (a1)+,(a0)+
        move.l  (a1)+,(a0)+
        move.l  (a1)+,(a0)+
5:      btst    #3,d1
        beq.s   5f
        move.l  (a1)+,(a0)+
        move.l  (a1)+,(a0)+
5:      btst    #2,d1
        beq.s   5f
        move.l  (a1)+,(a0)+
5:      btst    #1,d1
        beq.s   5f
        move.w  (a1)+,(a0)+
5:      btst    #0,d1
        beq.s   5f
        move.b  (a1)+,(a0)+
5:      move.l  4(a7),d0
        rts
        || ----------------------------------------

fast_fcpy:
        movem.l d2-d7/a2-a3,-(a7)
        subq.w  #1,d0
        swap    d0
        addq.w  #1,d0
2:      swap    d0
1:      movem.l (a1)+,d2-d7/a2-a3
        movem.l d2-d7/a2-a3,(a0)
        lea     32(a0),a0
        dbf     d0,1b
        swap    d0
        subq.w  #1,d0
        bne.s   2b
        movem.l (a7)+,d2-d7/a2-a3
        bra.s   cont_fcpy
        || ----------------------------------------

slow_fcpy:
        subq    #1,d0
        swap    d0
        addq.w  #1,d0
2:      swap    d0
1:      move.b  (a1)+,(a0)+
        dbf     d0,1b
        swap    d0
        subq.w  #1,d0
        bne.s   2b
0:      move.l  4(a7),d0
        rts
