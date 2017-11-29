/**
 * @file    m68k_stf.s
 * @author  Benjmain Gerard AKA Ben/OVR
 * @date    2017-09
 * @brief   Assembler routine for STf quartet mixer
 */

        .cpu    68000
        .arch   68000
        .global stf_timer_gen

#define SPRMIN 4000             // Minimal sampling rate
#define SPRMAX 32000		// Maximum sampling rate
#define BLKLEN 16		// # of sample mixed per block
#define SPFMAX SPRMAX/200	// Maximum sample per frame (160)
	
/// stf_timer_gen(mem.l,vec.w,count.w,loop.w)
///
/// @param  mem    Pointer to the buffer where timer routines are being
///                generated. Must be 36*count+2 bytes long.
/// @param  vec    Vector address {0x110,0x114,0x120,0x134}.
/// @param  cnt    Number of routine (at least 2).
/// @param  loop   If non zero last routine is chained to the first.

stf_timer_gen:
        movem.l d1-d7/a1-a4,-(a7)       // 11 regs

        // ----------------------------------------
        // Get parameter from the stack

        move.l  4+11*4(a7),a0           // a0: buffer (*)
        move.w  8+11*4(a7),d0           // d0: vector
        addq    #2,d0                   // d0: vector+2
        swap    d0                      // d0: VEC2:0000
        move.w  #0x4e73,d0              // d0: VEC2:RTE
        move.l  d0,a3                   // a3: VECT:RTE (*)
        move.w  12+11*4(a7),d0          // d0: loop ?
        seq     d0
        ext.w   d0
        swap    d0
        move.w  10+11*4(a7),d0          // d0: loop:count (*)

        // ----------------------------------------
        // Prepare registers to speedup the loop

        move.l  #0x31fc0000,d6
        move.w  a0,d6
        movem.l tpl_beg(pc),d1-d5/a2
        movea.w #tpl_len,a4

        // tst.w        d0
        // beq  empty
        // setup
        subq    #2,d0                   // d0: dbf count
loop:
        movem.l d1-d5/a2,(a0)           // (3x) move.l #Imm,Addr.W ]
        lea     24(a0),a1
        adda.l  a4,a0                   // a0= next routine
        add.w   a4,d6
        bcs.s   newseg
oldseg:
        move.l  d6,(a1)+                // 31fc-ROUT
        move.l  a3,(a1)+                // 31fc-ROUT-VECT RTE
cont:
        dbf     d0,loop

        move.w  a3,(a0)                 // RTE to finish

        // ----------------------------------------

        bset    #31,d0                  // Unless it's looping
        bne.s   over

        clr.w   d0
empty:
        //
        movem.l d1-d5/a2,(a0)           // (3x) move.l #Imm,Addr.W ]
        lea     24(a0),a1
        move.l  a0,d1                   // d1.l: last
        swap    d1                      // d1.w: last-seg
        move.l  4+4*11(a7),a0           // a0.l: first
        move.l  a0,d2                   // d2.l: first
        move.w  d2,d6
        swap    d2                      // d2.w: first-seg
        cmp.w   d1,d2                   // same segment ?
        beq.s   oldseg

newseg:
        move.w  #0x21fc,(a1)+           // 21fc
        move.l  a0,(a1)+                // 21fc-ROUTINE
        move.l  a3,(a1)                 // 21fc-ROUTINE-VECT RTE
        subq    #2,(a1)
        bra.s   cont

        // ----------------------------------------

over:
        movem.l (a7)+,d1-d7/a1-a4
        rts

tpl_beg:
        move.l  #0x08000000,0xFFFF8800.w // 21fc-08000X00-8800 | 8
        move.l  #0x09000000,0xFFFF8800.w // 21fc-09000Y00-8800 | 8
        move.l  #0x0A000000,0xFFFF8800.w // 21fc-0A000Z00-8800 | 8
        move.w  #0x00001234,0x0000ABCD.w // 31fc-1234-ABCD     | 6
        rte                              // 4e73               | 2
tpl_end:
        .set	tpl_len,tpl_end-tpl_beg
	.if	tpl_len != 32
	.err
	.endif
	
	
///
///

stf_mix:
        move.w  8(a7),d0
        bne.s   0f
        move.l  4(a7),a4
        rts
of:				
	|| TODO
	rts

/// d7.w number of sample to mix {1..TMPLEN}	
stf_mix_parts:
	
	

/// ----------------------------------------
/// Mix 2 first voices into temporary block
/// ----------------------------------------
/// d0.w: (#A) decimal acu
/// a0.w: (#A) decimal stp
/// d1.l: (#A) integer stp
/// d2.l: (#A) sample pointer
/// a5.l: (#A) sample end address
/// ----
/// d3.w: (#B) decimal acu
/// a1.w: (#B) decimal stp
/// d4.l: (#B) integer stp
/// d5.l: (#B) sample pointer
/// a6.l: (#B) sample end address
/// ----
/// d6.l: scratch
/// d7.w: number of pcm to mix
///
/// a3.l: temporary mix buffer
/// a4.l: scratch


0:
        move.l  d7,d6           | d7#31 is set for the last loop [...]
        bmi.s	2f
1:	
        ext.l   d7              | [...] this is where it happens

        | d6 = {-1=7 .. -7=1}
	neg.w	d6
        lsl.w   #3,d6           | x8
        movea.w	d6,a4
        add.w   d6,d6           | x16
        add.w   a4,d6           | x24
        jmp     full_part1(pc,d6.w)
2:
	rts
	
stf_mix_part1:
        sub.w	#BLKLEN,d7
        bmi     0b

full_part1:
	.rept   BLKLEN
        | 24 bytes per loop       17
        | ----------------------+---+---
        add.w   a0,d0           | 1 | 7
        addx.l  d1,d2           | 2 | GB: .w with segment contraints
        move.l  d2,a4           | 1 |
        move.b  (a4),d7         | 2 |
        ext.w   d7              | 1 |
        | ----------------------+---+---
        add.w   a1,d3           | 1 | 7
        addx.l  d4,d5           | 2 | GB: .w with segment contraints
        move.l  d5,a4           | 1 |
        move.b  (a4),d6         | 2 |
        ext.w   d6              | 1 |
        | ----------------------+---+---
        add.w   d6,d7           | 1 | 3
        move.w  d7,(a3)+        | 2 |
        |
        .endr
	
	
	| A loop check
	| ----------------------+------
	move.l	d2,d6
	sub.l	a5,d6
	bmi.s	Aok		| d6.l: overflow amount

	move.l	d0,d7		|
	bpl.s	Aloop		| d0.l: bit#31 set => no loop

	| Sample has no loop
	| ----------------------+------
	clr.w	d1		| d1.w: integer step to 0
	lea	nulPcm(pc),a0	|
	move.l	a0,d2		| d2.l: null sample
	addq	#1,a0		|
	move.l	a0,a5		|
	movea.w	d1,a0		| a0.w: decimal step to 0
	bra.s	Aok

Aloop:
	| Sample must loop
	| ----------------------+------
	clr.w	d7		|
	swap	d7		| d7.w: loop length
	moveq	#4,d6		| d6.w: hopefully never got to the limit
Arwd:	
	sub.l	d7,d2
	cmp.l	a5,d2
	dbcs	d6,Arwd		| Gas do not understand BLO 
Aok:


	
	cmp.l	a6,d5
	blo.s	Bok

	nop
	
Bok:	
        sub.w	#BLKLEN,d7
	bpl	full_part1
        move.l  d7,d6           | d7#31 is set for the last loop [...]
        bpl	1b
        rts
	

stf_mix_part2:
	

full_part2:
	.if	0
	.rept   8
        | 35 nops / 48 bytes per loop

        move.w  (a3)+,d7        | 2 | 10
        add.w   a0,d0           | 1 |
        addx.l  d1,d2           | 2 |
        move.l  d2,a4           | 1 |
        moveq   #0,d6           | 1 |
        move.b  (a4),d6         | 2 |
        add.w   d6,d7           | 1 |

        add.w   a1,d3           | 1 | 8
        addx.l  d4,d5           | 2 |
        move.l  d5,a4           | 1 |
        moveq   #0,d6           | 1 |
        move.b  (a4),d6         | 2 |
        add.w   d6,d7           | 1 |

        add     d7,d7           | 1 | 5
        add     d7,d7           | 1 |
        lea     0(a2,d7.w),a4   | 3 |

        | GB: We have 2 alternatives methods both are equals in cycles but
        |     not in size. We can use whichever is the best for the
        |     multiplication.

        move.w  (a4)+,d7        | 2 | 12 (14 bytes)
        move.w  d7,N+4(a3)      | 3 |
        move.b  d7,N+12(a3)     | 3 |
        move.b  (a4)+,N+20(a3)  | 4 |

        |move.b (a4)+,N+4(a3)   | 4 | 12 (12 bytes)
        |move.b (a4)+,N+12(a3)  | 4 |
        |move.b (a4)+,N+20(a3)  | 4 |
	|
        .endr

	.endif


/// ----------------------------------------------------------------------
///  SECTION DATA
/// ----------------------------------------------------------------------
	
	.section .data
nulPcm:	.word 0x0000		| Mid point for signed 8bit sample

	
/// ----------------------------------------------------------------------
///  SECTION BSS
/// ----------------------------------------------------------------------
		
	.section .bss

// Buffer used to generate the chained interrupted routines.
//	
// Each routines is tpl_len (32 bytes) long. The routines must be in
// the same 64k segment. As we don't know where in memory this is
// going to be loaded we have to allocate at least twice the
// minimal room. We need 2 frames/ticks worth of sample (SPFMAX)
// so we allocate 2 times that plus a few bytes for the last
// routine of the ring buffer.
//
// We will use some of the lost room as temporary mix buffer. We need
// MIXMAX words.
	
stf_irq_routines:
// 	.space	tpl_len*4*MIXMAX
	
	
// Local Variables:
// mode: asm
// asm-comment-char: ?|
// End:
