/**
 * @file    m68k_mix.s
 * @author  Benjmain Gerard AKA Ben/OVR
 * @date    2017-09
 * @brief   m68K assembler routines for 4 voices mixing.
 *
 */

        .cpu    68000
        .arch   68000

#define BLKLEN  12      // Mix this many PCMs without loop check
#define IRQSIZ  32      // Size of the STf replay chained timer routine


/// @see mix_ata.h:mix_fast_t
        .struct 0
A_end:  .struct .+4             | 00
A_cur:  .struct .+4             | 04
A_stp:  .struct .+4             | 08
A_dec:  .struct .+2             | 12 0c
A_lpl:  .struct .+2             | 14 0d
A_usr:  .struct .+16            | 16 10
A_siz:  .struct .               | 32 20

        .if A_siz != 32
        .error
        .endif

        .equiv  B_end,  A_siz + A_end
        .equiv  B_cur,  A_siz + A_cur
        .equiv  B_stp,  A_siz + A_stp
        .equiv  B_dec,  A_siz + A_dec
        .equiv  B_lpl,  A_siz + A_lpl

#ifndef NDEBUG
        .macro  break
        clr.l   -(a7)
        pea     0x5C68DB61
        trap    #0
        addq.w  #8,a7
        .endm
#else
# define break	
#endif
	
        .section .text
        .global fast_stf,fast_ste,fast_fal // ,fast_4x8

/// --------------------------------------------------------
///                a2      a3      a4       a5        d7
/// void fast_stf(Tpcm.l, routs.l, temp.l, voices.l, cnt.l)
///                4(a7)   8(a7)   12(a7)  16(a7)    20(a7)
///      +11*4=44 48(a7)  52(a7)   56(a7)  60(a7)    64(a7)

fast_stf:
        move.l  20(a7),d0       | d0: count
        beq.s   0f

        || 11 regs (d0/d1/a0/a1 not saved by m68k ABI)
        movem.l d2-d7/a2-a6,-(a7)

        || Arguments            |     (48)      (52)      (56)     (60)
        movem.l 48(a7),a2-a5    | a2: Tpcm, a3: rout, a4: temp, a5: voices
        move.l  d0,d7           |
        add.w   d7,d7           | d7: count*2

        || Mix A/B
	lea	mid_u8+2(pc),a6
	move.l	a6,d6
        lea     mixAB_mix(pc),a6
        bsr     mix_2chans

        || Arguments
        move.l  56(a7),a4       | a4: temp
        move.w  66(a7),d7       |
        add.w   d7,d7           | d7: count*2
        lea     2*A_siz(a5),a5  | a5: voice#C

        || Mix C/D
	lea	mid_u8+2(pc),a6
	move.l	a6,d6
        lea     stfCD_mix(pc),a6
        bsr     mix_2chans

        movem.l (a7)+,d2-d7/a2-a6
0:      rts


/// --------------------------------------------------------
///                a2      a3      a4       a5        d7
/// void fast_ste(Tmix.l, dest.l, temp.l, voices.l, cnt.l)
///                4(a7)   8(a7)  12(a7)  16(a7)    20(a7)
///      +11*4=44 48(a7)  52(a7)  56(a7)  60(a7)    64(a7)

fast_ste:
        move.l  20(a7),d0       | d0: count
        beq.s   0f

        || 11 regs (d0/d1/a0/a1 not saved by m68k ABI)
        movem.l d2-d7/a2-a6,-(a7)

        || Arguments            |     (48)      (52)      (56)     (60)
        movem.l 48(a7),a2-a5    | a2: Tmix, a3: dest, a4: temp, a5:voices
        move.l  d0,d7           |
        add.w   d7,d7           | d7: count*2

        || Mix A/B
	lea	mid_u8+2(pc),a6
	move.l	a6,d6
        lea     mixAB_mix(pc),a6
        bsr     mix_2chans

        || Arguments
        move.l  56(a7),a4       | a4: temp
        move.w  66(a7),d7       |
        add.w   d7,d7           | d7: count*2
        lea     2*A_siz(a5),a5  | a5: voice#C

        || Mix C/D
	lea	mid_u8+2(pc),a6
	move.l	a6,d6
        lea     steCD_mix(pc),a6
        bsr     mix_2chans

        movem.l (a7)+,d2-d7/a2-a6 | 11 regs
0:      rts


/// --------------------------------------------------------
///                a4      a5       d7
/// void fast_fal(dest.l, voices.l, cnt.l)
///                4(a7)   8(a7)    12(a7)
///      +11*4=44 48(a7)  52(a7)    56(a7)

fast_fal:
        move.l  12(a7),d0       | d0: count
        beq.s   0f

        || 10 regs (d0/d1/a0/a1 not saved by m68k ABI)
        movem.l d2-d7/a2-a6,-(a7)

        || Arguments            |     (44)     (48)
        movem.l 48(a7),a4-a5    | a4: dest, a5:voices
        move.l  d0,d7           |
        add.w   d7,d7           | d7: count*2
	add.w	d7,a4		| a4: intermediat

        || Mix A/B
	lea	mid_u8+2(pc),a6
	move.l	a6,d6
        lea     mixAB_mix(pc),a6
        bsr.s   mix_2chans

        || Arguments
        lea     0x8000.w,a2     | a3: sign change
        move.l  48(a7),a4       | a4: dest
        move.w  58(a7),d7       |
        add.w   d7,d7           | d7: count*2
	lea	0(a4,d7.w),a3	| a3: intermediat
        lea     2*A_siz(a5),a5  | a5: voice#C
	
        || Mix C/D
	lea	mid_u8+2(pc),a6
	move.l	a6,d6
        lea     falCD_mix(pc),a6
        bsr.s   mix_2chans

        movem.l (a7)+,d2-d7/a2-a6
0:      rts


/// --------------------------------------------------------
///                a4      a5       d7
/// void fast_4x8(dest.l, voices.l, cnt.l)
///                4(a7)   8(a7)    12(a7)
///      +9*4=36  40(a7)  44(a7)    48(a7)

	.if 0
fast_4x8:
        move.l  12(a7),d0       | d0: count
        beq.s   0f

        || 9 regs (d0/d1/a0/a1 not saved by m68k ABI)
        movem.l d2-d7/a4-a6,-(a7)

        || Arguments            |     (44)     (48)
        movem.l 40(a7),a4-a5    | a4: dest, a5:voices
        move.l  d0,d7           |
        add.w   d7,d7           | d7: count*2

        || Mix A/B
	lea	mid_s8+2(pc),a6
	move.l	a6,d6
        lea     ilac2_mix(pc),a6
        bsr.s   mix_2chans

        || Arguments
        move.l  40(a7),a4       | a4: dest
        addq.w  #2,a4           |
        move.w  50(a7),d7       |
        add.w   d7,d7           | d7: count*2
        lea     2*A_siz(a5),a5  | a5: voice#C

        || Mix C/D
	lea	mid_s8+2(pc),a6
	move.l	a6,d6
        lea     ilac2_mix(pc),a6
        bsr.s   mix_2chans

        movem.l (a7)+,d2-d7/a4-a6
0:      rts

	.endif
	
/// --------------------------------------------------------
/// a2.l: mixer specific (usually convert table)
/// a3.l: mixer specific (usually destination)
/// a4.l: mixer specific (usually temp)
/// a5.l: voice-info (either A or C)
/// d6.l: end of mid point PCM (empty voice)
/// a6.l: mixer routine
/// d7.w: number of pcm * 2 (bit #0 used to detect end)

mix_2chans:
        || move.l  a6,d6           | d6: mixer routine
        || lea     zero0+2(pc),a6  | a6: midpoint sample loop
        || exg     d6,a6           | d6: midpoint, a6: mixer

        ||----------------------
        || Voice A/C

        move.l  A_cur(a5),d4    | d4: pcm
        bne.s   1f              | running ?

        || Voice A/C is stopped
        moveq   #0,d0           | d0: stp
        move.l  d0,a0           | a0: decimal stp (=0)
        move.l  d0,d2           | d2: decimal acu (=0)
        move.l  d6,A_end(a5)
        move.l  d6,d4
        subq.l  #2,d4           | d4: pcm (zeros)
        bra.s   2f

1:      || Voice A/C is running
        move.l  A_stp(a5),d0    | d0: stp
        movea.w d0,a0           | a0: decimal stp
        clr.w   d0
        swap    d0              | d0: integer stp
        move.w  A_dec(a5),d2    | d2: decimal acu

        ||----------------------
        || Voice B/D

2:      move.l  B_cur(a5),d5    | d5: pcm
        bne.s   1f              | running ?

        || Voice B/D is stopped
        moveq   #0,d1           | d1: stp
        move.l  d1,a1           | a1: decimal stp (=0)
        move.l  d1,d3           | d3: decimal acu (=0)
        move.l  d6,B_end(a5)
        move.l  d6,d5
        subq.l  #2,d5           | d5: pcm (zeros)
        bra.s   2f

1:      || Voice B/D is running
        move.l  B_stp(a5),d1    | d1: stp
        movea.w d1,a1           | a1: decimal stp
        clr.w   d1
        swap    d1              | d1: integer stp
        move.w  B_dec(a5),d3    | d3: decimal acu

        ||----------------------
        || Call 2 channels mixer
2:      jsr     (a6)

        || Save voice A/C
        swap    d0
        move.w  a0,d0
        tst.l   d0
        bne.s   1f
        moveq   #0,d2
        moveq   #0,d4
1:      move.l  d4,A_cur(a5)
        move.w  d2,A_dec(a5)

        || Save voice B/D
        swap    d1
        move.w  a1,d1
        tst.l   d1
        bne.s   1f
        moveq   #0,d3
        moveq   #0,d5
1:      move.l  d5,B_cur(a5)
        move.w  d3,B_dec(a5)

        rts


/// --------------------------------------------------------
/// Mix 2 voices (A&B) into the intermediate buffer.
/// --------------------------------------------------------
/// a2    .l Unused
/// a3    .l unused
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

mixAB_all:
        clr.w   d6              | clear MSB
        swap    d7              | store count in msw
mixAB_beg:
        .rept   BLKLEN
        |                         16  16 nops, 22 bytes
        | ----------------------+---+---
        move.l  d4,a6           | 1 | 7 nops, 10 bytes
        add.w   a0,d2           | 1 |
        addx.l  d0,d4           | 2 | .w if segment contraint
        clr.w   d7              | 1 |
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
mixAB_end:
        .equiv  MIXAB_SIZE,(mixAB_end-mixAB_beg)/BLKLEN


        || ----------------------------------------
        || check loop macro
        || ----------------------------------------

        .macro check_loop cur end acu dec int lpl nul

        | A loop check
        | ----------------------+------

        move.l  \cur,d6         |
        move.l  \end,a6         | a6.l: end
        sub.l   a6,d6           | d6.l: overflow amount
        bmi.s   2f              |

        move.w  d6,d7           | d7.w: overflow amount
        move.w  \lpl,d6         | d6.l: loop length (uint16)
        bne.s   0f              | loop ?

        | Sample has no loop
        | ----------------------+------
        clr.w   \int            | clear integer step
        move.w  \int,\dec
        lea     \nul+2(pc),a6  |
        move.l  a6,\end         | set end pointer
        subq.w  #2,a6           |
        move.l  a6,\cur         | set cur pointer
        bra.s   2f

0:
        | Sample need loop
        | ----------------------+------

        || break

        suba.l  d6,a6           | a6.l: loop address

        || GB: d7 (overflow) must be lower than d6 (length).
        ||     this can be avoid or diminish by changing the loop
        ||     length when it is too short (loop = N*loop)
        .if     1
        sub.w   d6,d7
        bcs.s   3f
        break
1:
        sub.w   d6,d7
        bcc.s   1b
3:
        add     d6,d7
        .endif
        adda.w  d7,a6           | a6.l: cur address
        move.l  a6,\cur
2:
        .endm

        check_loop d4,A_end(a5),d2,a0,d0,A_lpl(a5),mid_u8
        check_loop d5,B_end(a5),d3,a1,d1,B_lpl(a5),mid_u8

        swap    d7              | d7:count in LSW
mixAB_mix:
        sub.w   #BLKLEN*2,d7
        bpl     mixAB_all

        bset    #0,d7           | d7#0: set to detect end
        bne.s   0f
        move.w  d7,d6           | d6: -1,-3,-5 ...
        swap    d7              | d7: count in MSW
        not.w   d6              | d6: 0,2,4,6 ...
        lea     mixAB_off(pc,d6.w),a6
        adda.w  (a6),a6
        jmp     (a6)
mixAB_off:
        .set    i,0
        .rept   BLKLEN-1
        .set    i,i+MIXAB_SIZE
        .short  mixAB_beg+i-.
        .endr
        .short  2               | jump to next rts
0:      rts


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
        clr.w   d6              | clear MSB
        swap    d7              | store count in msw
stfCD_beg:
        .set    i,0
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
        move.b  (a6)+,i+4(a3)   | 4 | 12 nops, 12 bytes
        move.b  (a6)+,i+12(a3)  | 4 |
        move.b  (a6)+,i+20(a3)  | 4 |
        |
        .set    i,i+IRQSIZ
        .endr
stfCD_end:
        lea     IRQSIZ*BLKLEN(a3),a3

        .equiv  STFCD_SIZE,(stfCD_end-stfCD_beg)/BLKLEN

        || adr,end,acu,dec,int,lpl
        check_loop d4,A_end(a5),d2,a0,d0,A_lpl(a5),mid_u8
        check_loop d5,B_end(a5),d3,a1,d1,B_lpl(a5),mid_u8

        swap    d7              | d7: count in LSW
stfCD_mix:
        sub.w   #BLKLEN*2,d7
        bpl     stfCD_all

        move.w  d7,d6
        lsl.w   #4,d6
        adda.w  d6,a3           | correct rout offset

        bset    #0,d7           | d7#0: set to detect end
        bne.s   0f
        move.w  d7,d6
        swap    d7              | d7: count in MSW
        not.w   d6              | -2:0 -2:1 ...
        lea     stfCD_off(pc,d6.w),a6
        adda.w  (a6),a6
        jmp     (a6)
stfCD_off:
        .set    i,0
        .rept   BLKLEN-1
        .set    i,i+STFCD_SIZE
        .short  stfCD_beg+i-.
        .endr
        .short  2               | jump to next rts
0:      rts


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

steCD_all:
        clr.w   d6              | clear MSB
        swap    d7              | store count in msw
steCD_beg:
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
steCD_end:

        .equiv STECD_SIZE,(steCD_end-steCD_beg)/BLKLEN

        || adr,end,acu,dec,int,lpl
        check_loop d4,A_end(a5),d2,a0,d0,A_lpl(a5),mid_u8
        check_loop d5,B_end(a5),d3,a1,d1,B_lpl(a5),mid_u8

        swap    d7              | d7: count in LSW
steCD_mix:
        sub.w   #BLKLEN*2,d7
        bpl     steCD_all

        bset    #0,d7           | d7#0: set to detect end
        bne.s   0f
        move.w  d7,d6
        swap    d7              | d7: count in MSW
        not.w   d6              | -2:0 -2:1 ...
        lea     steCD_off(pc,d6.w),a6
        adda.w  (a6),a6
        jmp     (a6)
steCD_off:
        .set    i,0
        .rept   BLKLEN-1
        .set    i,i+STECD_SIZE
        .short  steCD_beg+i-.
        .endr
        .short  2               | jump to next rts
0:      rts


/// --------------------------------------------------------
/// Falcon 16bit DMA
/// --------------------------------------------------------
/// Add 2 voices (C&D) to the 2 voices (A&B)
/// already in the intermediate buffer.
/// Convert to signed Stereo 16bit PCM
/// --------------------------------------------------------
/// a2    .w $8000
/// a3    .w intermediat (inp)
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
/// GB: timings are wrong for the Falcon 68030

falCD_all:
        clr.w   d6              | clear MSB
        swap    d7              | store count in msw
falCD_beg:
        .rept   BLKLEN
        |                         27  27 nops, 28 bytes
        | ----------------------+---+---
        move.w  (a3)+,d7        | 2 | 9 nops, 12 bytes
        | ----------------------+---+---
        move.l  d4,a6           | 1 |
        add.w   a0,d2           | 1 |
        addx.l  d0,d4           | 2 |
        move.b  (a6),d6         | 2 |
        add.w   d6,d7           | 1 |
        | ----------------------+---+---
        move.l  d5,a6           | 1 | 7 nops, 10 bytes
        add.w   a1,d3           | 1 |
        addx.l  d1,d5           | 2 |
        move.b  (a6),d6         | 2 |
        add.w   d6,d7           | 1 |
        | ----------------------+---+---
        lsl.w   #6,d7           | 5 |
        sub.w   a2,d7		| 1 |
        move.w  d7,(a4)+        | 5 |
        move.w  d7,(a4)+        | 5 |
        |
        .endr
falCD_end:

        .equiv FALCD_SIZE,(falCD_end-falCD_beg)/BLKLEN

        || adr,end,acu,dec,int,lpl
        check_loop d4,A_end(a5),d2,a0,d0,A_lpl(a5),mid_u8
        check_loop d5,B_end(a5),d3,a1,d1,B_lpl(a5),mid_u8

        swap    d7              | d7: count in LSW
falCD_mix:
        sub.w   #BLKLEN*2,d7
        bpl     falCD_all

        bset    #0,d7           | d7#0: set to detect end
        bne.s   0f
        move.w  d7,d6
        swap    d7              | d7: count in MSW
        not.w   d6              | -2:0 -2:1 ...
        lea     falCD_off(pc,d6.w),a6
        adda.w  (a6),a6
        jmp     (a6)
falCD_off:
        .set    i,0
        .rept   BLKLEN-1
        .set    i,i+FALCD_SIZE
        .short  falCD_beg+i-.
        .endr
        .short  2               | jump to next rts
0:      rts



/// --------------------------------------------------------
///
/// --------------------------------------------------------
/// Interleave 2 voices A/B or C/D
/// Input is 8-bit, Output is 8 bit 4 interleaved channels
/// --------------------------------------------------------
/// a2    .l unused
/// a3    .l unused
/// a4    .l s8[4] pcm buffer (inp/out)
/// a5    .l voices
/// a6    .l scratch
/// ----- -- -----------------------------------------------
/// a0/a1 .w dec-stp
/// d2/d3 .w dec-acu
/// d0/d1 .l int-stp
/// d4/d5 .l int-acu (adr)
/// d6/d7 .w scratch
///
/// GB: timings are wrong for the Falcon 68030

	.if 0

ilac2_all:
        | No special init at block start
ilac2_beg:
        .rept   BLKLEN
        |                         18  18 nops, 18 bytes
        | ----------------------+---+---
        move.l  d4,a6           | 1 | 8 nops, 8 bytes
        move.b  (a6),(a4)+      | 4 |
        add.w   a0,d2           | 1 |
        addx.l  d0,d4           | 2 |
        | ----------------------+---+---
        move.l  d5,a6           | 1 | 8 nops, 8 byes
        move.b  (a6),(a4)+      | 4 |
        add.w   a1,d3           | 1 |
        addx.l  d1,d5           | 2 |
        | ----------------------+---+---
        addq.w  #2,a4           | 2 | 2 nops, 2 bytes

        .endr
ilac2_end:

        .equiv ILAC2_SIZE,(ilac2_end-ilac2_beg)/BLKLEN

        || adr,end,acu,dec,int,lpl
        check_loop d4,A_end(a5),d2,a0,d0,A_lpl(a5),mid_s8
        check_loop d5,B_end(a5),d3,a1,d1,B_lpl(a5),mid_s8

ilac2_mix:
        sub.w   #BLKLEN*2,d7
        bpl     ilac2_all

        bset    #0,d7           | d7#0: set to detect end
        bne.s   0f
        move.w  d7,d6
        not.w   d6              | -2:0 -2:1 ...
        lea     ilac2_off(pc,d6.w),a6
        adda.w  (a6),a6
        jmp     (a6)
ilac2_off:
        .set    i,0
        .rept   BLKLEN-1
        .set    i,i+ILAC2_SIZE
        .short  ilac2_beg+i-.
        .endr
        .short  2               | jump to next rts
0:      rts

	.endif

|| #define BIS_FIX 3
|| #define BIS_LEN (1<<BIS_FIX)

|| ||| Inp:
|| |||     d0: ste-fix
|| |||     a0: SMC base
|| ||| Out:
|| |||     d0: stp-fix * BIS_LEN
|| ||| Use:
|| |||     d1,d2,d3
|| smc_offsets:
||         move.l  d0,d1
||         swap    d0
||         move.w  d0,d2
||         move.w  d1,d3

||         .set    X,0
||         .set    Y,18

||         .rept   BIS_LEN-1       | C | B |
||         move.w  d0,X(a0)        | 3 | 4 |
||         add.w   d3,d1           | 1 | 2 |
||         addx.w  d2,d0           | 1 | 2 |
||         .set    X,X+Y           |---+---+
||         .endr                   | 5 | 8 |
||         ||
||         swap    d0
||         move.w  d1,d0
||         rts

|| ///                 a5        a6     d7
|| /// void fast_ste(dest.l, voices.l, cnt.l)
|| ///                4(a7)    8(a7)   12(a7)
|| ///      +11*4=44 48(a7)   52(a7)   56(a7)
|| fast_ste_6bit:
||         move.l  12(a7),d0       | d0: count
||         beq     0f

||         || 11 regs (d0/d1/a0/a1 not saved by m68k ABI)
||         movem.l d2-d7/a2-a6,-(a7)

||         || Arguments            |     (48)      (52)
||         movem.l 48(a7),a5-a6    | a5: dest, a6:voices
||         moveq   #BIS_LEN-1,d7
||         and.w   d0,d7
||         swap    d7
||         move    d0,d7
||         lsr     #BIS_FIX,d7

||         || Update voices offset (self modified code)
||         .macro  smc_voice n,stp
||         || ----------------------------------------
||         move.l  A_stp+\n*A_siz(a6),d0
||         tst.l   A_cur+\n*A_cur(a6)
||         bne.s   l\@
||         moveq   #0,d0
|| l\@:
||         lea     smc_mixer+12+\n*4(pc),a0
||         bsr     smc_offsets
||         move.l  d0,\stp
||         || ----------------------------------------
||         .endm

||         smc_voice 0,d4
||         smc_voice 1,d5
||         smc_voice 2,a4
||         smc_voice 3,-(a7)

||         || Load voices
||         .macro  load_voice n
||         ||
||         move.l  A_cur+\n*A_siz(a6),d\n
||         bne.s   l\@
||         move.l  a3,d\n
|| l\@:
||         move.l  d\n,a\n
||         moveq   #0,d\n
||         move.w  A_dec+\n*A_siz(a6),d\n
||         ||
||         .endm

||         lea     zero6(pc),a3
||         load_voice 0
||         load_voice 1
||         load_voice 2
||         load_voice 3

||         moveq   #0,d6
||         subq    #1,d7
||         bmi     1f

|| loop_ste6:

||         || 6-bit source
||         .set    X,0
|| smc_mixer:
||         .rept   BIS_LEN         | C | B |
||         ||                      |---+---|
||         move.b  X(a0),d6        | 3 | 4 | 2
||         add.b   X(a1),d6        | 3 | 4 | 2
||         add.b   X(a2),d6        | 3 | 4 | 2
||         add.b   X(a3),d6        | 3 | 4 | 2
||         move.b  d6,(a5)+        | 3 | 2 | 2
||         .set    X,0x1234        |---+---|
||         ||                      |15 |18 |
||         .endr
||         || 15*BIS_LEN-4 -> 116 nops
||         || 18*BIS_LEN-8 -> 136 nops

||         .macro step_voice n,stp
||         || ----------------------------------------
||         add.l   \stp,d\n
||         swap    d\n
||         adda.w  d\n,a\n
||         clr.w   d\n
||         swap    d\n

||         cmpa.l  A_end+\n*A_siz(a6),a\n
||         bmi.s   l\@
||         move.w  A_lpl+\n*A_siz(a6),d6
||         suba.l  d6,a\n
|| l\@:
||         || ----------------------------------------
||         .endm

||         step_voice  0,  d4
||         step_voice  1,  d5
||         step_voice  2,  a4
||         step_voice  3, (a7)

||         dbf     d7,loop_ste6


|| 1:
||         addq    #4,sp

||         || Save voices
||         .macro  save_voice n
||         || ----------------------------------------
||         move.w  d\n,A_dec+\n*A_siz(a6)
||         cmp.l   a5,a\n
||         bne.s   l\@
||         move.l  d5,a\n
|| l\@:
||         move.l  a\n,A_cur+\n*A_siz(a6)
||         || ----------------------------------------
||         .endm

||         lea     zero6(pc),a5
||         moveq   #0,d5
||         save_voice 0
||         save_voice 1
||         save_voice 2
||         save_voice 3

||         movem.l (a7)+,d2-d7/a2-a6 | 11 regs
|| 0:      rts


|| |||
|| |||
|| |||
||         .if     0
|| smc_mixer10:
||         || 10-bit dynamic range table
||         .set    X,0
||         .rept   BIS_LEN         | C | B |
||         ||                      |---+---|
||         move.l  d5,d7           | 1 | 2 |
||         move.b  X(a0),d7        | 3 | 4 |
||         move.b  X(a1),d6        | 3 | 4 |
||         add.w   d6,d7           | 1 | 2 |
||         move.b  X(a2),d6        | 3 | 4 |
||         add.w   d6,d7           | 1 | 2 |
||         move.b  X(a3),d6        | 3 | 4 |
||         add.w   d6,d7           | 1 | 2 |
||         move.l  d7,a4           | 1 | 2 |
||         move.b  (a4),(a5)+      | 3 | 4 |
||         .set    X,0x1234        |---+---|
||         ||                      |21 |24 |
||         .endr

|| 0:      rts
||         .endif

mid_s8:	.word	0x0000
mid_u8: .word   0x8080
