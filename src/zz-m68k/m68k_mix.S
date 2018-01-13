/**
 * @file    m68k_mix.s
 * @author  Benjmain Gerard AKA Ben/OVR
 * @date    2017-09
 * @brief   m68K assembler routine for 4 voices mixing.			
 *		
 * 
 *
 */

        .cpu    68000
        .arch   68000

#define BLKLEN  12	// Mix that many PCM per pass (no loop check)
#define BLKSIZ1 22      // Size of the first mix routine (= A+B)
#define BLKSIZ2 42	// Size of the STf 2nd mix routine (+= C+B)
#define BLKSIZ3 26	// Size of the STe 2nd mix routine (+= C+B)
#define IRQSIZ  32      // Size of the STf replay chained timer routine

	
/// @see mix_stf.c:mix_fast_t
	.struct	0
A_end:	.struct	.+4		| 00
A_cur:	.struct	.+4		| 04
A_stp:	.struct	.+4		| 08
A_dec:	.struct	.+2		| 12 0c
A_lpl:	.struct	.+2		| 14 0d
A_siz:				| 16 10

	.equiv	B_end,A_siz+A_end
	.equiv	B_cur,A_siz+A_cur
	.equiv	B_stp,A_siz+A_stp
	.equiv	B_dec,A_siz+A_dec
	.equiv	B_lpl,A_siz+A_lpl

	.macro	break
	clr.l   -(a7)
	pea     0x5C68DB61
	trap    #0
	addq.w  #8,a7
	.endm
	
	.section .text
        .global fast_stf,fast_ste,fast_fal
	
///                a2      a3      a4       a5        d7
/// void fast_stf(Tpcm.l, routs.l, temp.l, voices.l, cnt.l)
///                4(a7)   8(a7)   12(a7)  16(a7)    20(a7)	
///      +11*4=44 48(a7)  52(a7)   56(a7)  60(a7)    64(a7)	
fast_stf:
        move.l  20(a7),d0	| d0: count
        beq.s	0f
	
	|| 11 regs (d0/d1/a0/a1 not saved by m68k ABI)
	movem.l	d2-d7/a2-a6,-(a7)

	|| Arguments		|     (48)      (52)      (56)     (60)
	movem.l	48(a7),a2-a5	| a2: Tpcm, a3: rout, a4: temp, a5:voices
	move.l	d0,d7		| d7: count
	add.w	d7,d7		| d7: count*2

	|| Mix A/B
	lea	fast_stf_AB(pc),a6
	bsr	mix_2chans

	|| Arguments
	move.l	56(a7),a4	| a4: temp
	move.w	66(a7),d7	| d7: count
	add.w	d7,d7		| d7: count*2
	lea	2*A_siz(a5),a5	| a5: voice#C

	|| Mix C/D
	lea	fast_stf_CD(pc),a6
	bsr	mix_2chans
	
	movem.l	(a7)+,d2-d7/a2-a6 | 11 regs
0:	rts

///                a2      a3      a4       a5        d7
/// void fast_ste(Tmix.l, dest.l, temp.l, voices.l, cnt.l)
///                4(a7)   8(a7)  12(a7)  16(a7)    20(a7)	
///      +11*4=44 48(a7)  52(a7)  56(a7)  60(a7)    64(a7)	
fast_ste:
        move.l  20(a7),d0	| d0: count
        beq	0f

	|| 11 regs (d0/d1/a0/a1 not saved by m68k ABI)
	movem.l	d2-d7/a2-a6,-(a7)

	|| Arguments		|     (48)      (52)      (56)     (60)
	movem.l	48(a7),a2-a5	| a2: Tmix, a3: dest, a4: temp, a5:voices
	move.l	d0,d7		| d7: count
	add.w	d7,d7		| d7: count*2

	|| Mix A/B
	lea	fast_stf_AB(pc),a6
	bsr	mix_2chans

	|| Arguments
	move.l	56(a7),a4	| a4: temp
	move.w	66(a7),d7	| d7: count
	add.w	d7,d7		| d7: count*2
	lea	2*A_siz(a5),a5	| a5: voice#C

	|| Mix C/D
	lea	fast_ste_CD(pc),a6
	bsr	mix_2chans
	
	movem.l	(a7)+,d2-d7/a2-a6 | 11 regs
0:	rts

;; copy_temp:
;; 	lsr	#1,d7
;; 	subq	#1,d7
;; 0:	move.w	(a4)+,d6
;; 	add.w	#256,d6
;; 	move.b	(a2,d6.w),(a3)+
;; 	dbf	d7,0b
;; 	rts
	
mix_2chans:
	move.l	a6,d6		| d6: mixer routine
	lea	zeros+2(pc),a6	| a6: midpoint sample loop
	exg	d6,a6		| d6: midpoint, a6: mixer

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
	movea.w	d0,a0		| a0: decimal stp
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
	movea.w	d1,a1		| a1: decimal stp
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

	
/// ----------------------------------------
/// Mix 2 voices (A&B) into the intermediate
/// buffer.
/// ----------------------------------------
///
/// a2    .l PCM table (unused)
/// a3    .l IRQ routines (unused)
/// a4    .l intermediate buffer
/// a5    .l voices	
/// a6    .l scratch
/// ----- -- -------------------------------
/// a0/a1 .w dec-stp
/// d2/d3 .w dec-acu
/// d0/d1 .l int-stp
/// d4/d5 .l int-acu (adr)
/// d6/d7 .w scratch
/// ----- -- -------------------------------
///

part1_all:
	clr.w	d6		| clear MSB
	swap	d7		| store count in msw
part1_beg:
	.rept   BLKLEN
        |                         16  16 nops, 22 bytes
        | ----------------------+---+---
        move.l  d4,a6           | 1 | 7 nops, 10 bytes
        add.w   a0,d2           | 1 |
        addx.l  d0,d4           | 2 | .w if segment contraint
	clr.w	d7              | 1 |
        move.b  (a6),d7         | 2 |
        | ----------------------+---+---
        move.l  d5,a6           | 1 | 6 nops, 8 bytes
        add.w   a1,d3           | 1 |
        addx.l  d1,d5           | 2 | GB: .w with segment contraints
        move.b  (a6),d6         | 2 |
        | ----------------------+---+---
        add.w   d6,d7           | 1 | 3 nops, 4 bytes
        move.w  d7,(a4)+        | 2 |
        |
        .endr
part1_end:
	.if	(part1_end-part1_beg)/BLKLEN != BLKSIZ1
	.err
	.endif

	|| ----------------------------------------
	|| check loop macro
	|| ----------------------------------------
	
	.macro check_loop cur end acu dec int lpl

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
	lea	zeros+2(pc),a6	| 
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
	.if 	1
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
	
	check_loop d4,A_end(a5),d2,a0,d0,A_lpl(a5)
	check_loop d5,B_end(a5),d3,a1,d1,B_lpl(a5)

	swap	d7		| d7:count in LSW
fast_stf_AB:	
        sub.w	#BLKLEN*2,d7
	bpl	part1_all

	bset	#0,d7		| d7#0: set to detect end
	bne.s	0f
	move.w	d7,d6
	swap	d7		| d7: count in MSW
	not.w	d6		| -2:0 -2:1 ...
	lea	part1_off(pc,d6.w),a6
	adda.w	(a6),a6
	jmp	(a6)
part1_off:	
	.set	i,0
	.rept	BLKLEN-1
	.set	i,i+BLKSIZ1
	.short	part1_beg+i-.
	.endr
	.short	2		| jump to next rts
0:	rts


/// ----------------------------------------
/// Add 2 voices (C&D) to the 2 voices (A&B)
/// already in the intermediate buffer.
/// Convert to 3 voices volume level and
/// store in the chained timer interrupt
/// routines.
/// ----------------------------------------
///
/// a2    .l PCM table
/// a3    .l IRQ routines
/// a4    .l intermediate buffer
/// a5    .l voices	
/// a6    .l scratch
/// ----- -- -------------------------------
/// a0/a1 .w dec-stp
/// d2/d3 .w dec-acu
/// d0/d1 .l int-stp
/// d4/d5 .l int-acu (adr)
/// d6/d7 .w scratch
/// ----- -- -------------------------------
///

	
part2_all:
	clr.w	d6		| clear MSB
	swap	d7		| store count in msw
part2_beg:
	.set	i,0
	.rept   BLKLEN
        |                         33  33 nops, 42 bytes
        | ----------------------+---+---
        move.w  (a4)+,d7        | 2 | 9 nops, 12 bytes 
        move.l  d4,a6           | 1 |
        move.b  (a6),d6         | 2 |
        add.w   d6,d7           | 1 |
        add.w   a0,d2           | 1 |
        addx.l  d0,d4           | 2 |
        | ----------------------+---+---
        move.l  d5,a6           | 1 | 7 nops, 10 bytes
        add.w   a1,d3           | 1 |
        addx.l  d1,d5           | 2 |
        move.b  (a6),d6         | 2 |
        add.w   d6,d7           | 1 |
        | ----------------------+---+---
        add.w   d7,d7           | 1 | 5 nops, 8 bytes
        add.w   d7,d7           | 1 |
        lea     0(a2,d7.w),a6   | 3 |
        | ----------------------+---+---
        move.b	(a6)+,i+4(a3)   | 4 | 12 nops, 12 bytes
        move.b	(a6)+,i+12(a3)  | 4 |
        move.b	(a6)+,i+20(a3)  | 4 |
	|
	.set	i,i+IRQSIZ
        .endr
part2_end:
	lea	IRQSIZ*BLKLEN(a3),a3
	.if	(part2_end-part2_beg)/BLKLEN != BLKSIZ2
	.err
	.endif

	|| adr,end,acu,dec,int,lpl
	check_loop d4,A_end(a5),d2,a0,d0,A_lpl(a5)
	check_loop d5,B_end(a5),d3,a1,d1,B_lpl(a5)
	
	swap	d7		| d7: count in LSW
fast_stf_CD:	
        sub.w	#BLKLEN*2,d7
	bpl	part2_all

	move.w	d7,d6
	lsl.w	#4,d6
	adda.w	d6,a3		| correct rout offset

	bset	#0,d7		| d7#0: set to detect end
	bne.s	0f
	move.w	d7,d6
	swap	d7		| d7: count in MSW
	not.w	d6		| -2:0 -2:1 ...
	lea	part2_off(pc,d6.w),a6
	adda.w	(a6),a6
	jmp	(a6)
part2_off:	
	.set	i,0
	.rept	BLKLEN-1
	.set	i,i+BLKSIZ2
	.short	part2_beg+i-.
	.endr
	.short	2		| jump to next rts
0:	rts

/// ----------------------------------------
/// Add 2 voices (C&D) to the 2 voices (A&B)
/// already in the intermediate buffer.
/// Convert to signed 8bit PCM using the mix
/// dynamic range mix table.
/// ----------------------------------------
///
/// a2    .l Mix table (byte[1024])
/// a3    .l s8 pcm buffer (output)
/// a4    .l intermediate buffer
/// a5    .l voices	
/// a6    .l scratch
/// ----- -- -------------------------------
/// a0/a1 .w dec-stp
/// d2/d3 .w dec-acu
/// d0/d1 .l int-stp
/// d4/d5 .l int-acu (adr)
/// d6/d7 .w scratch
/// ----- -- -------------------------------
///
	
part3_all:
	clr.w	d6		| clear MSB
	swap	d7		| store count in msw
part3_beg:
	.rept   BLKLEN
        |                         21  21 nops, 26 bytes
        | ----------------------+---+---
        move.w  (a4)+,d7        | 2 | 9 nops, 12 bytes 
        move.l  d4,a6           | 1 |
        move.b  (a6),d6         | 2 |
        add.w   d6,d7           | 1 |
        add.w   a0,d2           | 1 |
        addx.l  d0,d4           | 2 |
        | ----------------------+---+---
        move.l  d5,a6           | 1 | 7 nops, 10 bytes
        add.w   a1,d3           | 1 |
        addx.l  d1,d5           | 2 |
        move.b  (a6),d6         | 2 |
        add.w   d6,d7           | 1 |
        | ----------------------+---+---
        move.b  (a2,d7.w),(a3)+ | 5 | 5 nops, 4 bytes
	|
        .endr
part3_end:
	

	
	.if	(part3_end-part3_beg)/BLKLEN != BLKSIZ3
	.err
	.endif

	|| adr,end,acu,dec,int,lpl
	check_loop d4,A_end(a5),d2,a0,d0,A_lpl(a5)
	check_loop d5,B_end(a5),d3,a1,d1,B_lpl(a5)
	
	swap	d7		| d7: count in LSW
fast_ste_CD:	
        sub.w	#BLKLEN*2,d7
	bpl	part3_all

	bset	#0,d7		| d7#0: set to detect end
	bne.s	0f
	move.w	d7,d6
	swap	d7		| d7: count in MSW
	not.w	d6		| -2:0 -2:1 ...
	lea	part3_off(pc,d6.w),a6
	adda.w	(a6),a6
	jmp	(a6)
part3_off:	
	.set	i,0
	.rept	BLKLEN-1
	.set	i,i+BLKSIZ3
	.short	part3_beg+i-.
	.endr
	.short	2		| jump to next rts
0:	rts
	
zeros:
	.word	0x8080
