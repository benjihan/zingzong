;;; @file    test_sndh.s
;;; @author  Ben G. AKA Ben/OVR
;;; @date    2018-02-28
;;; @brief   zingzong quartet player SNDH test
;;;

	opt	p+
	
	bra.w	sndh_init
	bra.w	sndh_kill
	bra.w	sndh_play

	dc.b	"SNDH"
	dc.b	"TITL","Zingzong sndh test",0
	dc.b	"TC200",0
	dc.b	"HDNS",0
	even

sndh_init:
	movem.l	d0-d1/a0-a1,-(a7)

	;; Sampling/Quality (0:default)
	clr.l	-(a7)
	
	;; Driver (0:Auto 1:AGA 2:STf 3:STe 4:Falcon)

	clr.l	-(a7)
	;; pea	2.w

	;; vset bin_t address
	lea	tovset(pc),a0
	adda.l	(a0),a0
	pea	(a0)
	
	;; song bin_t address
	lea	tosong(pc),a0
	adda.l	(a0),a0
	pea	(a0)

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

tosong:	dc.l	song-*
tovset:	dc.l	vset-*
	
	;; zingzong replay (m68k "C" ABI)
zingzong:
	incbin	"zingzong.bin"
	even

	;; Structure used to describe binary buffers (loaded files)
	rsreset
binptr:	rs.l	1		; points to file data (0=file follow)-
binmax:	rs.l	1		; buffer size
binlen:	rs.l	1		; data size

	;; Song (.4v)
	
song:	dc.l	0
	dc.l	song_max
	dc.l	song_len
song_bin:
	incbin	"test.4v"
song_len: equ *-song_bin
	even
	ds.l	4		; for closing incomplete files
song_max: equ *-song_bin


	;; Voice set (.set)
	
vset:	dc.l	0
	dc.l	vset_max
	dc.l	vset_len
vset_bin:
	incbin	"test.set"
vset_len: equ *-vset_bin
	even
	ds.b	2048
vset_max: equ *-vset_bin
