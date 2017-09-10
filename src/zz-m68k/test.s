;;; @file   test.s
;;; @author Benjamin Gerard AKA Ben/OVR
;;; @date   2017/08/12
;;; @brief  sc68 stub for zingzong test

	opt p+,o-

	bra.w	init
	bra.w	player+4
	bra.w	player+8

parm:	
	dc.l	gnos-song	; d0.l song size
	dc.l	tesv-vset	; d1.l vset size
	dc.l	song-parm	; a0.l song offset
	dc.l	vset-parm	; a1.l vset offset
	
init:
	lea	parm(pc),a2
	movem.l	(a2),d0/d1/a0/a1
	adda.l	a2,a0
	adda.l	a2,a1
player:
	incbin	"zingzong.bin"
	even
	
song:	incbin	"test.4v"
	even
	ds.b	12
gnos:	
	
vset:	incbin	"test.set"
	even
	ds.b	1024
tesv:
