;;; @file    test_sndh.s
;;; @author  Ben G. AKA Ben^OVR
;;; @date    2018-02-28
;;; @brief   zingzong quartet player SNDH test
;;;

	opt	a+,p+

	ifnd	MIXERID
MIXERID	set	0	    ; 0:Auto 1:Amiga 2:YM 3:DMA8 4:DMA16
	endc

	bra.w	sndh_init
	bra.w	sndh_kill
	bra.w	sndh_play

	dc.b	"SNDH"
	dc.b	"TITL","Zingzong sndh test",0
	dc.b	"CONV","Ben/OVR",0
	dc.b	"TC200",0
	dc.b	"##02",0
	dc.b	"#!02",0
	dc.b	"HDNS",0
	even

sndh_init:
	movem.l	d0-d1/a0-a1,-(a7)

	;; Sampling/Quality (0:default)
	clr.l	-(a7)

	;; Driver (0:Auto 1:AGA 2:STf 3:STe 4:Falcon)
	pea	MIXERID.w

	lea	nbsng(pc),a0
	subq.b	#1,d0
	and.l	#255,d0
	divu	(a0)+,d0
	swap	d0
	lsl	#3,d0
	adda.w	d0,a0

	lea	4(a0),a1
	add.l	(a0),a0		; vset bin_t address
	pea	(a0)
	add.l	(a1),a1		; song bin_t address
	pea	(a1)

	bsr.s	zingzong
	lea	16(a7),a7

	movem.l	(a7)+,d0-d1/a0-a1
	rts

sndh_kill:
	movem.l	d0-d1/a0-a1,-(a7)
	bsr.s	zingzong+4
	movem.l	(a7)+,d0-d1/a0-a1
	rts

sndh_play:
	movem.l	d0-d1/a0-a1,-(a7)
	bsr.s	zingzong+8
	movem.l	(a7)+,d0-d1/a0-a1
	rts

	;; Sub song table
nbsng:	dc.w	(sgnos-songs)/8
songs:	dc.l	vset-*,sng1-*
	dc.l	vset-*,sng2-*
sgnos:

	;; zingzong replay (m68k "C" ABI)
zingzong:
	ifd	ZZSYMB
	include	"zingzong.sym"
	endc
	incbin	"zingzong.bin"
	even

	;; Structure used to describe binary buffers (loaded files)
	rsreset
binptr:	rs.l	1		; points to file data (0=file follow)-
binmax:	rs.l	1		; buffer size
binlen:	rs.l	1		; data size

	;; ---------------------
	;; Sng1 (.4v)
sng1:	dc.l	0
	dc.l	sng1_max
	dc.l	sng1_len
sng1_bin:
	incbin	"sng1.4v"	; .4v file
sng1_len: equ *-sng1_bin
	even
	ds.l	3		; for closing incomplete files
sng1_max: equ *-sng1_bin

	;; ---------------------
	;; Sng2 (.4v)
sng2:	dc.l	0
	dc.l	sng2_max
	dc.l	sng2_len
sng2_bin:
	incbin	"sng2.4v"	; .4v file
sng2_len: equ *-sng2_bin
	even
	ds.l	3		; for closing incomplete files
sng2_max: equ *-sng2_bin

	;; ---------------------
	;; Voice set (.set)
vset:	dc.l	0
	dc.l	vset_max
	dc.l	vset_len
vset_bin:
	incbin	"sndh.set"	; .set file
vset_len: equ *-vset_bin
	even
	ds.b	2048		; loop unroll extra space
vset_max: equ *-vset_bin

;;; Local Variables:
;;; mode: asm
;;; indent-tabs-mode: t
;;; tab-width: 10
;;; comment-column: 40
;;; End:
