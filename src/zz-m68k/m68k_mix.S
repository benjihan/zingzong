/**
 * @file    m68k_mix.S
 * @author  Benjmain Gerard AKA Ben/OVR
 * @date    2017-09
 * @brief   m68K assembler routines for 4 voices mixing.
 *
 */

	.cpu	68020
	.arch	68020

#define BLKLEN	12	// Mix this many PCMs without loop check
#define IRQSIZ	32	// Size of the STf replay chained timer routine

/// @see mix_ata.h:mix_fast_t
	.struct 0
A_end:	.struct .+4		| 00
A_cur:	.struct .+4		| 04
A_stp:	.struct .+4		| 08
A_dec:	.struct .+2		| 12 0c
A_lpl:	.struct .+2		| 14 0d
A_chn:	.struct .+4		| 16 10
A_usr:	.struct .+12		| 20 14
A_siz:	.struct .		| 32 20

	.if A_siz != 32
	.error
	.endif

	.equiv	B_end,	A_siz + A_end
	.equiv	B_cur,	A_siz + A_cur
	.equiv	B_stp,	A_siz + A_stp
	.equiv	B_dec,	A_siz + A_dec
	.equiv	B_lpl,	A_siz + A_lpl

#ifndef NDEBUG
	.macro	break
#ifdef SC68
	clr.l	-(a7)
	pea	0x5C68DB61
	trap	#0
	addq.w	#8,a7
#else
	illegal
#endif
	.endm
#else
# define break
#endif

	.section .text
	.global fast_stf,fast_dnr,fast_fal,fast_lrb,fast_s7s

/// --------------------------------------------------------
///                a2      a3      a4       a5        d7
/// void fast_stf(Tpcm.l, routs.l, temp.l, voices.l, cnt.l)
///                4(a7)   8(a7)   12(a7)  16(a7)    20(a7)
///      +11*4=44 48(a7)  52(a7)   56(a7)  60(a7)    64(a7)

fast_stf:
	move.l	20(a7),d0	| d0: count
	beq.s	0f

	|| 11 regs (d0/d1/a0/a1 not saved by m68k ABI)
	movem.l d2-d7/a2-a6,-(a7)

	|| Arguments		|     (48)	(52)	  (56)	   (60)
	movem.l 48(a7),a2-a5	| a2: Tpcm, a3: rout, a4: temp, a5: voices
	move.l	d0,d7		|
	add.w	d7,d7		| d7: count*2

	|| Mix A/B
	lea	mid_u8+2(pc),a6
	move.l	a6,d6
	lea	mixAB_mix(pc),a6
	bsr	mix_2chans

	|| Arguments
	move.l	56(a7),a4	| a4: temp
	move.w	66(a7),d7	|
	add.w	d7,d7		| d7: count*2
	lea	2*A_siz(a5),a5	| a5: voice#C

	|| Mix C/D
	lea	mid_u8+2(pc),a6
	move.l	a6,d6
	lea	stfCD_mix(pc),a6
	bsr	mix_2chans

	movem.l (a7)+,d2-d7/a2-a6
0:	rts


/// --------------------------------------------------------
///                a2      a3      a4       a5        d7
/// void fast_dnr(Tmix.l, dest.l, temp.l, voices.l, cnt.l)
///                4(a7)   8(a7)  12(a7)  16(a7)    20(a7)
///      +11*4=44 48(a7)  52(a7)  56(a7)  60(a7)    64(a7)

fast_dnr:
	move.l	20(a7),d0	| d0: count
	beq.s	0f

	|| 11 regs (d0/d1/a0/a1 not saved by m68k ABI)
	movem.l d2-d7/a2-a6,-(a7)

	|| Arguments		|     (48)	(52)	  (56)	   (60)
	movem.l 48(a7),a2-a5	| a2: Tmix, a3: dest, a4: temp, a5:voices
	move.l	d0,d7		|
	add.w	d7,d7		| d7: count*2

	|| Mix A/B
	lea	mid_u8+2(pc),a6
	move.l	a6,d6
	lea	mixAB_mix(pc),a6
	bsr	mix_2chans

	|| Arguments
	move.l	56(a7),a4	| a4: temp
	move.w	66(a7),d7	|
	add.w	d7,d7		| d7: count*2
	lea	2*A_siz(a5),a5	| a5: voice#C

	|| Mix C/D
	lea	mid_u8+2(pc),a6
	move.l	a6,d6
	lea	dnrCD_mix(pc),a6
	bsr	mix_2chans

	movem.l (a7)+,d2-d7/a2-a6 | 11 regs
0:	rts


/// --------------------------------------------------------
///                a4      a5       d7
/// void fast_s7s(dest.l, voices.l, cnt.l)
///                4(a7)   8(a7)    12(a7)
///      +11*4=44 48(a7)  52(a7)    56(a7)

fast_s7s:
	move.l	12(a7),d0	| d0: count
	beq.s	0f

	|| 10 regs (d0/d1/a0/a1 not saved by m68k ABI)
	movem.l d2-d7/a2-a6,-(a7)

	|| Arguments		|     (44)	  (48)
	movem.l 48(a7),a4-a5	| a4: dest, a5:voices
	move.l	d0,d7		|
	add.w	d7,d7		| d7: count*2

	|| Mix A/B
	lea	mid_s8+2(pc),a6
	move.l	a6,d6
	lea	s7sAB_mix(pc),a6
	bsr	mix_2chans

	|| Arguments
	move.l	48(a7),a4	| a4: dest
	move.w	58(a7),d7	|
	add.w	d7,d7		| d7: count*2
	lea	2*A_siz(a5),a5	| a5: voice#C

	|| Mix C/D
	lea	mid_s8+2(pc),a6
	move.l	a6,d6
	lea	s7sCD_mix(pc),a6
	bsr	mix_2chans

	movem.l (a7)+,d2-d7/a2-a6
0:	rts

/// --------------------------------------------------------
///                a2     a4       a5       d7
/// void fast_lrb(Tmix.l,dest.l, voices.l, cnt.l)
///                4(a7)  8(a7)   12(a7)   16(a7)
///      +11*4=44 48(a7) 52(a7)   56(a7)   60(a7)

fast_lrb:
	move.l	16(a7),d0	| d0: count
	beq.s	0f

	|| 10 regs (d0/d1/a0/a1 not saved by m68k ABI)
	movem.l d2-d7/a2-a6,-(a7)

	|| Arguments		|
	movem.l 48(a7),a2/a4-a5	| a2: Tmix, a4: dest, a5:voices
	move.l	d0,d7		|
	add.w	d7,d7		| d7: count*2

	|| Mix A/B
	lea	mid_u8+2(pc),a6
	move.l	a6,d6
	lea	mixAB_mix(pc),a6
	bsr.s	mix_2chans

	|| Arguments
	move.l	52(a7),a4	| a4: dest
	move.w	62(a7),d7	|
	add.w	d7,d7		| d7: count*2
	lea	1020(a2),a3	| a3: Tmix (reverse)
	lea	2*A_siz(a5),a5	| a5: voice#C

	|| Mix C/D
	lea	mid_u8+2(pc),a6
	move.l	a6,d6
	lea	lrbCD_mix(pc),a6
	bsr.s	mix_2chans

	movem.l (a7)+,d2-d7/a2-a6
0:	rts

/// --------------------------------------------------------
///                a2     a4       a5       d7
/// void fast_fal(Tmix.l,dest.l, voices.l, cnt.l)
///                4(a7)  8(a7)   12(a7)   16(a7)
///      +11*4=44 48(a7) 52(a7)   56(a7)   60(a7)

fast_fal:
	move.l	16(a7),d0	| d0: count
	beq.s	0f

	|| 10 regs (d0/d1/a0/a1 not saved by m68k ABI)
	movem.l d2-d7/a2-a6,-(a7)

	|| Arguments		|
	movem.l 48(a7),a2/a4-a5	| a2: Tmix, a4: dest, a5:voices
	move.l	d0,d7		|
	add.w	d7,d7		| d7: count*2
	add.w	d7,a4		| a4: intermediate

	|| Mix A/B
	lea	mid_u8+2(pc),a6
	move.l	a6,d6
	lea	mixAB_mix(pc),a6
	bsr.s	mix_2chans

	|| Arguments
	move.l	52(a7),a4	| a4: dest
	move.w	62(a7),d7	|
	add.w	d7,d7		| d7: count*2
	lea	0(a4,d7.w),a3	| a3: intermediate
	lea	2*A_siz(a5),a5	| a5: voice#C

	|| Mix C/D
	lea	mid_u8+2(pc),a6
	move.l	a6,d6
	lea	falCD_mix(pc),a6
	bsr.s	mix_2chans

	movem.l (a7)+,d2-d7/a2-a6
0:	rts


/// --------------------------------------------------------
/// a2.l: mixer specific (usually convert table)
/// a3.l: mixer specific (usually destination)
/// a4.l: mixer specific (usually temp)
/// a5.l: voice-info (either A or C)
/// d6.l: end of mid point PCM (empty voice)
/// a6.l: mixer routine
/// d7.w: number of pcm * 2 (bit #0 used to detect end)

mix_2chans:
	|| move.l  a6,d6	   | d6: mixer routine
	|| lea	   zero0+2(pc),a6  | a6: midpoint sample loop
	|| exg	   d6,a6	   | d6: midpoint, a6: mixer

	||----------------------
	|| Voice A/C

	move.l	A_cur(a5),d4	| d4: pcm
	bne.s	1f		| running ?

	|| Voice A/C is stopped
	moveq	#0,d0		| d0: stp
	move.l	d0,a0		| a0: decimal stp (=0)
	move.l	d0,d2		| d2: decimal acu (=0)
	move.l	d6,A_end(a5)
	move.l	d6,d4
	subq.l	#2,d4		| d4: pcm (zeros)
	bra.s	2f

1:	|| Voice A/C is running
	move.l	A_stp(a5),d0	| d0: stp
	movea.w d0,a0		| a0: decimal stp
	clr.w	d0
	swap	d0		| d0: integer stp
	move.w	A_dec(a5),d2	| d2: decimal acu

	||----------------------
	|| Voice B/D

2:	move.l	B_cur(a5),d5	| d5: pcm
	bne.s	1f		| running ?

	|| Voice B/D is stopped
	moveq	#0,d1		| d1: stp
	move.l	d1,a1		| a1: decimal stp (=0)
	move.l	d1,d3		| d3: decimal acu (=0)
	move.l	d6,B_end(a5)
	move.l	d6,d5
	subq.l	#2,d5		| d5: pcm (zeros)
	bra.s	2f

1:	|| Voice B/D is running
	move.l	B_stp(a5),d1	| d1: stp
	movea.w d1,a1		| a1: decimal stp
	clr.w	d1
	swap	d1		| d1: integer stp
	move.w	B_dec(a5),d3	| d3: decimal acu

	||----------------------
	|| Call 2 channels mixer
2:	jsr	(a6)

	|| Save voice A/C
	swap	d0
	move.w	a0,d0
	tst.l	d0
	bne.s	1f
	moveq	#0,d2
	moveq	#0,d4
1:	move.l	d4,A_cur(a5)
	move.w	d2,A_dec(a5)

	|| Save voice B/D
	swap	d1
	move.w	a1,d1
	tst.l	d1
	bne.s	1f
	moveq	#0,d3
	moveq	#0,d5
1:	move.l	d5,B_cur(a5)
	move.w	d3,B_dec(a5)

	rts


/// --------------------------------------------------------
/// Mix 2 voices (A&B) into the intermediate buffer.
/// --------------------------------------------------------
/// a2         .l Unused
/// a3         .l unused
/// a4         .l intermediate buffer
/// a5         .l voices
/// a6         .l scratch
/// ----- -- -----------------------------------------------
/// a0/a1 .w dec-stp
/// d2/d3 .w dec-acu
/// d0/d1 .l int-stp
/// d4/d5 .l int-acu (adr)
/// d6/d7 .w scratch
///

mixAB_all:
	swap	d7		| d7: count in MSW
	clr.w	d6		| d6: clear d6.w MSB
mixAB_beg:
	.rept	BLKLEN
	|			 16/22
	| ----------------------+-----+---
	move.l	d4,a6		| 1/2 | 7/10
	add.w	a0,d2		| 1/2 |
	addx.l	d0,d4		| 2/2 |
	clr.w	d7		| 1/2 |
	move.b	(a6),d7		| 2/2 |
	| ----------------------+-----+---
	move.l	d5,a6		| 1/2 | 6/8
	add.w	a1,d3		| 1/2 |
	addx.l	d1,d5		| 2/2 |
	move.b	(a6),d6		| 2/2 |
	| ----------------------+-----+---
	add.w	d6,d7		| 1/2 | 3/4
	move.w	d7,(a4)+	| 2/2 |
	|
	.endr
mixAB_end:
	.equiv	mixAB_siz, (mixAB_end-mixAB_beg) / BLKLEN

	|| ----------------------------------------
	|| check loop macro
	|| ----------------------------------------

	.macro check_loop cur end acu dec int lpl nul

	| A loop check
	| ----------------------+------

	move.l	\cur,d6		|
	move.l	\end,a6		| a6.l: end
	sub.l	a6,d6		| d6.l: overflow amount
	bmi.s	2f		|

	move.w	d6,d7		| d7.w: overflow amount
	move.w	\lpl,d6		| d6.l: loop length (uint16)
	bne.s	0f		| loop ?

	| Sample has no loop
	| ----------------------+------
	clr.w	\int		| clear integer step
	move.w	\int,\dec
	lea	\nul+2(pc),a6
	move.l	a6,\end		| set end pointer
	subq.w	#2,a6		|
	move.l	a6,\cur		| set cur pointer
	bra.s	2f

0:
	| Sample need loop
	| ----------------------+------

	|| break

	suba.l	d6,a6		| a6.l: loop address

	|| GB: d7 (overflow) must be lower than d6 (length).
	||     this can be avoid or diminish by changing the loop
	||     length when it is too short (loop = N*loop)
	.if	1
	sub.w	d6,d7
	bcs.s	3f
	break
1:
	sub.w	d6,d7
	bcc.s	1b
3:
	add	d6,d7
	.endif
	adda.w	d7,a6		| a6.l: cur address
	move.l	a6,\cur
2:
	.endm

	check_loop d4,A_end(a5),d2,a0,d0,A_lpl(a5),mid_u8
	check_loop d5,B_end(a5),d3,a1,d1,B_lpl(a5),mid_u8

	swap	d7		| d7: count in LSW
mixAB_mix:
	sub.w	#BLKLEN*2,d7
	bpl	mixAB_all

	bset	#0,d7		| d7#0: set to detect end
	bne.s	0f
	move.w	d7,d6		| d6: -1,-3,-5 ...
	swap	d7		| d7: count in MSW
	not.w	d6		| d6: 0,2,4,6 ... (clear d6.w MSB)
	lea	mixAB_off(pc,d6.w),a6
	adda.w	(a6),a6
	jmp	(a6)
mixAB_off:
	.set	i,0
	.rept	BLKLEN-1
	.set	i,i+mixAB_siz
	.short	mixAB_beg+i-.
	.endr
	.short	2		| jump to next instruction
0:	rts


/// --------------------------------------------------------
/// Mix 2 voices (A&B) 8 bit stereo buffer
/// --------------------------------------------------------
/// a2    .l scratch
/// a3    .l continue (s7sAB_mix or s7sCD_mix)
/// a4    .l 2x8 PCM buffer
/// a5    .l voices
/// a6    .l scratch
/// ----- -- -----------------------------------------------
/// a0/a1 .w dec-stp
/// d2/d3 .w dec-acu
/// d0/d1 .l int-stp
/// d4/d5 .l int-acu (adr)
/// d6/d7 .w scratch
///

s7sAB_all:
	moveq	#0,d6		| d6: clear d6.w MSB
s7sAB_beg:
	.rept	BLKLEN
	|			 14/18
	| ----------------------+-----+---
	move.l	d4,a6		| 1/2 | 6/8
	add.w	a0,d2		| 1/2 |
	addx.l	d0,d4		| 2/2 |
	move.b	(a6),d6		| 2/2 |
	| ----------------------+-----+---
	move.l	d5,a6		| 1/2 | 6/8
	add.w	a1,d3		| 1/2 |
	addx.l	d1,d5		| 2/2 |
	add.b	(a6),d6		| 2/2 |
	| ----------------------+-----+---
	move.w	d6,(a4)+	| 2/2 | 2/2
	|
	.endr
s7sAB_end:
	.equiv	s7sAB_siz, (s7sAB_end-s7sAB_beg) / BLKLEN

	swap	d7		| d7: count in MSW
	check_loop d4,A_end(a5),d2,a0,d0,A_lpl(a5),mid_s8
	check_loop d5,B_end(a5),d3,a1,d1,B_lpl(a5),mid_s8
	swap	d7		| d7:count in LSW
s7sAB_mix:
	sub.w	#BLKLEN*2,d7
	bpl	s7sAB_all

	bset	#0,d7		| d7#0: set to detect end
	bne.s	0f
	move.w	d7,d6		| d6: -1,-3,-5 ...
	not.w	d6		| d6: 0,2,4,6 ... (clear d6.w MSB)
	lea	s7sAB_off(pc,d6.w),a6
	adda.w	(a6),a6
	jmp	(a6)
s7sAB_off:
	.set	i,0
	.rept	BLKLEN-1
	.set	i,i+s7sAB_siz
	.short	s7sAB_beg+i-.
	.endr
	.short	2		| jump to next instruction
0:	rts


||| GB: This can be optimize with automatic stack alignment using a7
|||	as destination pointer while running in usermode.
|||
||| 2*BLKLEN offset is a cheap trick to prevent gas to optimize 0(an)
||| breaking the code tower.
s7sCD_all:
	lea	2*BLKLEN(a4),a4
s7sCD_beg:
	.set	i,-2*BLKLEN
	.rept	BLKLEN
	|			 15/20
	| ----------------------+-----+---
	move.l	d4,a6		| 1/2 | 6/8
	add.w	a0,d2		| 1/2 |
	addx.l	d0,d4		| 2/2 |
	move.b	(a6),d6		| 2/2 |
	| ----------------------+-----+---
	move.l	d5,a6		| 1/2 | 6/8
	add.w	a1,d3		| 1/2 |
	addx.l	d1,d5		| 2/2 |
	add.b	(a6),d6		| 2/2 |
	| ----------------------+-----+---
	move.b	d6,i(a4)	| 3/4 | 3/4
	.set	i,i+2
	|
	.endr
s7sCD_end:
	.equiv	s7sCD_siz, (s7sCD_end-s7sCD_beg) / BLKLEN

	swap	d7		| d7:count in MSW
	check_loop d4,A_end(a5),d2,a0,d0,A_lpl(a5),mid_s8
	check_loop d5,B_end(a5),d3,a1,d1,B_lpl(a5),mid_s8
	swap	d7		| d7:count in LSW

s7sCD_mix:
	sub.w	#BLKLEN*2,d7
	bpl	s7sCD_all

	lea	2*BLKLEN(a4),a4
	adda.w	d7,a4		| correct output offset

	bset	#0,d7		| d7#0: set to detect end
	bne.s	0f
	move.w	d7,d6		| d6: -1,-3,-5 ...
	not.w	d6		| d6: 0,2,4,6 ...
	lea	s7sCD_off(pc,d6.w),a6
	adda.w	(a6),a6
	jmp	(a6)
s7sCD_off:
	.set	i,0
	.rept	BLKLEN-1
	.set	i,i+s7sCD_siz
	.short	s7sCD_beg+i-.
	.endr
	.short	2		| jump to next instruction
0:	rts


/// --------------------------------------------------------
/// STf/YM PART 2
/// --------------------------------------------------------
/// Add 2 voices (C&D) to the 2 voices (A&B)
/// already in the intermediate buffer.
/// Convert to 3 voices volume level and
/// store in the chained timer interrupt
/// routines.
/// --------------------------------------------------------
/// a2    .l PCM table (10bit)
/// a3    .l IRQ routines (output)
/// a4    .l intermediate buffer
/// a5    .l voices
/// a6    .l scratch
/// ----- -- -----------------------------------------------
/// a0/a1 .w dec-stp
/// d2/d3 .w dec-acu
/// d0/d1 .l int-stp
/// d4/d5 .l int-acu (adr)
/// d6/d7 .w scratch
///

stfCD_all:
	swap	d7		| d7: count in MSW
	clr.w	d6		| d6: clear d6.w MSB
stfCD_beg:
	.set	i,0
	.rept	BLKLEN
	|			 33/42
	| ----------------------+-----+---
	move.w	(a4)+,d7	| 2/2 | 9/12
	move.l	d4,a6		| 1/2 |
	move.b	(a6),d6		| 2/2 |
	add.w	d6,d7		| 1/2 |
	add.w	a0,d2		| 1/2 |
	addx.l	d0,d4		| 2/2 |
	| ----------------------+-----+---
	move.l	d5,a6		| 1/2 | 7/10
	add.w	a1,d3		| 1/2 |
	addx.l	d1,d5		| 2/2 |
	move.b	(a6),d6		| 2/2 |
	add.w	d6,d7		| 1/2 |
	| ----------------------+-----+---
	add.w	d7,d7		| 1/2 | 5/8
	add.w	d7,d7		| 1/2 |
	lea	0(a2,d7.w),a6	| 3/2 |
	| ----------------------+-----+---
	move.b	(a6)+,i+4(a3)	| 4/4 | 12/12
	move.b	(a6)+,i+12(a3)	| 4/4 |
	move.b	(a6)+,i+20(a3)	| 4/4 |
	|
	.set	i,i+IRQSIZ
	.endr
stfCD_end:
	.equiv	stfCD_siz, (stfCD_end-stfCD_beg) / BLKLEN

	lea	IRQSIZ*BLKLEN(a3),a3

	|| adr,end,acu,dec,int,lpl
	check_loop d4,A_end(a5),d2,a0,d0,A_lpl(a5),mid_u8
	check_loop d5,B_end(a5),d3,a1,d1,B_lpl(a5),mid_u8

	swap	d7		| d7: count in LSW
stfCD_mix:
	sub.w	#BLKLEN*2,d7
	bpl	stfCD_all

	move.w	d7,d6
	lsl.w	#4,d6
	adda.w	d6,a3		| correct rout offset

	bset	#0,d7		| d7#0: set to detect end
	bne.s	0f
	move.w	d7,d6		| d6: -1,-3,-5 ...
	swap	d7		| d7: count in MSW
	not.w	d6		| d6: 0,2,4 ... and clear d6.w MSB
	lea	stfCD_off(pc,d6.w),a6
	adda.w	(a6),a6
	jmp	(a6)
stfCD_off:
	.set	i,0
	.rept	BLKLEN-1
	.set	i,i+stfCD_siz
	.short	stfCD_beg+i-.
	.endr
	.short	2		| jump to next instruction
0:	rts


/// --------------------------------------------------------
/// STe/DMA8 PART 2
/// --------------------------------------------------------
/// Add 2 voices (C&D) to the 2 voices (A&B)
/// already in the intermediate buffer.
/// Convert to signed 8bit PCM using the mix
/// dynamic range mix table.
/// --------------------------------------------------------
/// a2    .l Mix table (byte[1024])
/// a3    .l s8 pcm buffer (output)
/// a4    .l intermediate buffer
/// a5    .l voices
/// a6    .l scratch
/// ----- -- -----------------------------------------------
/// a0/a1 .w dec-stp
/// d2/d3 .w dec-acu
/// d0/d1 .l int-stp
/// d4/d5 .l int-acu (adr)
/// d6/d7 .w scratch

dnrCD_all:
	swap	d7		| d7: count in MSW
	clr.w	d6		| d6: clear MSB
dnrCD_beg:
	.rept	BLKLEN
	|			 21/26
	| ----------------------+-----+---
	move.w	(a4)+,d7	| 2/2 | 9/12
	move.l	d4,a6		| 1/2 |
	move.b	(a6),d6		| 2/2 |
	add.w	d6,d7		| 1/2 |
	add.w	a0,d2		| 1/2 |
	addx.l	d0,d4		| 2/2 |
	| ----------------------+-----+---
	move.l	d5,a6		| 1/2 | 7/10
	add.w	a1,d3		| 1/2 |
	addx.l	d1,d5		| 2/2 |
	move.b	(a6),d6		| 2/2 |
	add.w	d6,d7		| 1/2 |
	| ----------------------+-----+---
	move.b	(a2,d7.w),(a3)+ | 5/4 | 5/4
	|
	.endr
dnrCD_end:

	.equiv	dnrCD_siz, (dnrCD_end-dnrCD_beg) / BLKLEN

	|| adr,end,acu,dec,int,lpl
	check_loop d4,A_end(a5),d2,a0,d0,A_lpl(a5),mid_u8
	check_loop d5,B_end(a5),d3,a1,d1,B_lpl(a5),mid_u8

	swap	d7		| d7: count in LSW
dnrCD_mix:
	sub.w	#BLKLEN*2,d7
	bpl	dnrCD_all

	bset	#0,d7		| d7#0: set to detect end
	bne.s	0f
	move.w	d7,d6		| d6: -1,-3,-5 ...
	swap	d7		| d7: count in MSW
	not.w	d6		| d6: 0,2,4 ... and clear d6.w MSB
	lea	dnrCD_off(pc,d6.w),a6
	adda.w	(a6),a6
	jmp	(a6)
dnrCD_off:
	.set	i,0
	.rept	BLKLEN-1
	.set	i,i+dnrCD_siz
	.short	dnrCD_beg+i-.
	.endr
	.short	2		| jump to next instruction
0:	rts

/// --------------------------------------------------------
/// 8bit DMA blended stereo
/// --------------------------------------------------------
/// Add 2 voices (C&D) to the 2 voices (A&B)
/// already in the intermediate buffer using a blending table
/// between left and right pairs.
/// --------------------------------------------------------
/// a2    .l L-pair blending table WORD[510]
/// a3    .l R-pair blending table WORD[510]
/// a4    .l 2xs16 pcm buffer (out)
/// a5    .l voices
/// a6    .l scratch
/// ----- -- -----------------------------------------------
/// a0/a1 .w dec-stp
/// d2/d3 .w dec-acu
/// d0/d1 .l int-stp
/// d4/d5 .l int-acu (adr)
/// d6/d7 .w scratch
///

lrbCD_all:
	swap	d7		| d7: count in MSW
lrbCD_beg:
	.rept	BLKLEN
	||			 29/38
	|| ---------------------+-----+---
	move.l	d4,a6		| 1/2 | 7/10
	add.w	a0,d2		| 1/2 |
	addx.l	d0,d4		| 2/2 |
	clr.w	d7		| 1/2 | d7= cnt | 0000
	move.b	(a6),d7		| 2/2 | d7= cnt | c
	|| ---------------------+-----+---
	move.l	d5,a6		| 1/2 | 7/10
	add.w	a1,d3		| 1/2 |
	addx.l	d1,d5		| 2/2 |
	clr.w	d6		| 1/2 |
	move.b	(a6),d6		| 2/2 | d6= ???? | d
	|| ---------------------+-----+---
	add.w	d7,d6		| 1/2 | 6/8
	add.w	d6,d6		| 1/2 | d6= ???? | 2(d+c)
	move.w	(a3,d6.w),d6	| 4/4 | d6= ???? | (c+d)*L : (c+d)*R
	|| ---------------------+-----+---
	move.w	(a4),d7		| 2/2 | 9/10
	add.w	d7,d7		| 1/2 | d7= ???? | 4(a+b)
	add.w	(a2,d7.w),d6	| 4/4 | d6= (a+b)*L+(c+d)*R : (a+b)*R+(c+d)*L
	move.w	d6,(a4)+	| 2/2 |
	||
	.endr
lrbCD_end:

	.equiv	lrbCD_siz, (lrbCD_end-lrbCD_beg) / BLKLEN

	|| adr,end,acu,dec,int,lpl
	check_loop d4,A_end(a5),d2,a0,d0,A_lpl(a5),mid_u8
	check_loop d5,B_end(a5),d3,a1,d1,B_lpl(a5),mid_u8

	swap	d7		| d7: count in LSW
lrbCD_mix:
	sub.w	#BLKLEN*2,d7
	bpl	lrbCD_all

	bset	#0,d7		| d7#0: set to detect end
	bne.s	0f

	move.w	d7,d6		| d6: -1,-3,-5 ...
	swap	d7		| d7: count in MSW
	not.w	d6		| d6: 0,2,4 ... and clear d6.w MSB
	lea	lrbCD_off(pc,d6.w),a6
	adda.w	(a6),a6
	jmp	(a6)
lrbCD_off:
	.set	i,0
	.rept	BLKLEN-1
	.set	i,i+lrbCD_siz
	.short	lrbCD_beg+i-.
	.endr
	.short	2		| jump to next instruction
0:	rts



/// --------------------------------------------------------
/// Falcon 16bit DMA blended stereo
/// --------------------------------------------------------
/// Add 2 voices (C&D) to the 2 voices (A&B)
/// already in the intermediate buffer using a blending table
/// between left and right pairs.
/// --------------------------------------------------------
/// a2    .l L-pair blending table LONG[510]
/// a3    .w intermediate (inp)
/// a4    .l 2xs16 pcm buffer (out)
/// a5    .l voices
/// a6    .l scratch
/// ----- -- -----------------------------------------------
/// a0/a1 .w dec-stp
/// d2/d3 .w dec-acu
/// d0/d1 .l int-stp
/// d4/d5 .l int-acu (adr)
/// d6/d7 .w scratch
///

falCD_all:
	swap	d7		| d7: count in MSW
falCD_beg:
	.rept	BLKLEN

	.if	1

	||			  x/36
	|| ---------------------+-----+---
	move.l	d4,a6		| x/2 | x/10
	add.w	a0,d2		| x/2 |
	addx.l	d0,d4		| x/2 |
	clr.w	d7		| x/2 | d7= cnt | 0000
	move.b	(a6),d7		| x/2 | d7= cnt | c
	| ----------------------+-----+---
	move.l	d5,a6		| x/2 | x/8
	add.w	a1,d3		| x/2 |
	addx.l	d1,d5		| x/2 |
	clr.w	d6		| x/2 |
	move.b	(a6),d6		| x/2 | d6= ???? | d
	| ----------------------+-----+---
	add.w	d7,d6		| x/2 | d6= cnt | d+c
	move.l	0(a2,d6.w*4),d6	| x/4 | d6= L(c+d) | R(c+d)
	swap	d6		| x/2 | d6= R(c+d) | L(c+d)
	| ----------------------+-----+---
	move.w	(a3)+,d7	| x/2 | d7= ???? | a+b
	add.l	0(a2,d7.w*4),d6	| x/4 | d6= L(a+b)+R(c+d) | R(a+b)+L(c+d)
	move.l	d6,(a4)+	| x/2 |

	.else

	clr.w	d7		|     | *** CAN BE DONE OUTSIDE THE LOOP
	move.w	(a3)+,d6	|     |
	swap	d6		|     |

	move.l	d4,a6		| x/2 | x/10
	add.w	a0,d2		| x/2 |
	addx.l	d0,d4		| x/2 |
	move.b	(a6),d7		| x/2 | d7= cnt | c

	move.l	d5,a6		| x/2 | x/8
	add.w	a1,d3		| x/2 |
	addx.l	d1,d5		| x/2 |
	clr.w	d6		| x/2 |
	move.b	(a6),d6		| x/2 | d6= ???? | d

	add.w	d7,d6

	lsl.l	#7,d6
	eor.l	#0x80008000,d6
	move.l	d6,(a4)+	| x/2 |

	.endif

	||
	.endr
falCD_end:

	.equiv	falCD_siz, (falCD_end-falCD_beg) / BLKLEN

	|| adr,end,acu,dec,int,lpl
	check_loop d4,A_end(a5),d2,a0,d0,A_lpl(a5),mid_u8
	check_loop d5,B_end(a5),d3,a1,d1,B_lpl(a5),mid_u8

	swap	d7		| d7: count in LSW
falCD_mix:
	sub.w	#BLKLEN*2,d7
	bpl	falCD_all

	bset	#0,d7		| d7#0: set to detect end
	bne.s	0f

	move.w	d7,d6		| d6: -1,-3,-5 ...
	swap	d7		| d7: count in MSW
	not.w	d6		| d6: 0,2,4 ... and clear d6.w MSB
	lea	falCD_off(pc,d6.w),a6
	adda.w	(a6),a6
	jmp	(a6)
falCD_off:
	.set	i,0
	.rept	BLKLEN-1
	.set	i,i+falCD_siz
	.short	falCD_beg+i-.
	.endr
	.short	2		| jump to next instruction
0:	rts

mid_s8: .word 0x0000
mid_u8: .word 0x8080

/*
Local Variables:
mode: asm
indent-tabs-mode: t
tab-width: 8
comment-column: 32
End:
*/
