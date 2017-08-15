;;; @file   test_sc68.s
;;; @author Benjamin Gerard AKA Ben/OVR
;;; @date   2017/08/12
;;; @brief  sc68 header for zingzong test

start:	set	0


endian:	macro
	dc.b	(\1)&255
	dc.b	((\1)>>8)&255
	dc.b	((\1)>>16)&255
	dc.b	((\1)>>24)&255
	endm

chtag:	macro
	dc.l	\1
	endian	\2
	endm
	
chint:	macro
	dc.l	\1
	endian	4
	endian	\2
	endm


begdat:	macro
data\@:	
	dc.l	\1
	endian	(.tad-.dat)
.dat:
	endm

enddat:	macro
	even
.tad:
	endm


;;; ======================================================================
;;; 
	
sc68:
	dc.b	"SC68 Music-file / (c) (BeN)jamin Gerard / SasHipA-Dev  ",0
begin:	
	chtag	"SC68",theend-begin
	
	begdat	"SCFN"
	dc.b	"Zingzong m68k test",0
	enddat

	begdat	"SCAN"
	dc.b	"Ben/OVR",0
	enddat

	;;
track:	macro
	chtag	"SCMU",0
	ifne	\3
	chint	"SCAT",\3
	endc
	chint	"SCFQ",200
	chint	"SCFR",98305
	chint	"SCTY",4
	begdat	"SCMN"
	dc.b	\1,0
	enddat
	begdat	"SCDA"
	incbin	\2
	enddat
	endm

	track	"Test AGA","test.dat",0
	
	chtag	"SCEF",0
theend:
