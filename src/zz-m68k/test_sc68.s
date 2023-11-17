;;; @file   test_sc68.s
;;; @author Benjamin Gerard AKA Ben^OVR
;;; @date   2017-08
;;; @brief  Create a sc68 file.
;;;
;;; ----------------------------------------------------------------------

	ifnd	RATE
RATE:	set	200		; replay rate
	endc

	ifnd	START
START:	set	0		; 0:PCR $10000:sc68 default start address
	endc

;;; Hardware flags definition
PSG:	set	1		; YM-2149 flag
STE:	set	2		; STe flag
AGA:	set	4		; Amiga flag

;;; ----------------------------------------------------------------------
;;;  Low level macros
;;; ----------------------------------------------------------------------

;;; LEint(value)
;;; ------------
;;; encode little endian integer
LEint:	macro
	;;
	dc.b	(\1)&255
	dc.b	((\1)>>8)&255
	dc.b	((\1)>>16)&255
	dc.b	((\1)>>24)&255
	;;
	endm

;;; chtag(tag,size)
;;; ---------------
;;; Declare a genral purpose tag
chtag:	macro
	;;
	dc.b	"\1"
	LEint	\2
	;;
	endm

;;; chint(tag,value)
;;; ----------------
;;; declare an integer tag
chint:	macro
	;;
	dc.b	"\1"		; tag name
	LEint	4		; chunk length
	LEint	\2		; chunk data (LE integer)
	;;
	endm


;;; begdat(size)
;;; ------------
;;; open a data chunk (close it with enddat)
begdat:	macro
	;;
data\@:
	dc.b	"\1"		; tag name
	LEint	(.tad-.dat)	; chunk length
.dat:				; data begin here
	;;
	endm

;;; enddat()
;;; --------
;;; close a data chunk (previously open with begdat)
enddat:	macro
	;;
	even
.tad:				; data end here
	;;
	endm

;;; asciz(string)
;;; -------------
;;; simulate gnu/asm .asciz (zero terminated string)
asciz:	macro
	;;
	dc.b \1,0
	;;
	endm

;;; ----------------------------------------------------------------------
;;;  High level macros
;;; ----------------------------------------------------------------------

;;; track(title,file,type,addr,frames)
;;; ------ /1 -- /2 - /3 - /4 -- /5 --
;;; declare a new track (song)
	;;
track:	macro
	;;
	chtag	SCMU,0
	;;
	if	\?4
	ifne	\4
	chint	SCAT,\4
	endc
	endc
	;;
	chint	SCFQ,RATE
	;;
	if	\?5
	ifne	\5
	chint	SCFR,\5
	endc
	endc
	;;
	chint	SCTY,\3
	;;
	title	\1
	;;
	if	\?2
	begdat	SCDA
	incbin	\2
	enddat
	endc
	;;
	endm


;;; open(name)
;;;
open	macro
	asciz	"SC68 Music-file / (c) (BeN)jamin Gerard / SasHipA-Dev  "
\1_open:
	chtag	SC68,\1_close-\1_open
	endm

;;; close(name)
;;;
close:	macro
	chtag	SCEF,0
\1_close:
	endm

;;; album(string)
;;;
album:	macro
	if	\?1
	begdat	SCFN
	asciz	\1
	enddat
	endc
	endm

;;; title(string)
;;;
title:	macro
	if	\?1
	begdat	SCMN
	asciz	\1
	enddat
	endc
	endm

;;; artist(string)
;;;
artist:	macro
	if	\?1
	begdat	SCAN
	asciz	\1
	enddat
	endc
	endm


;;; ----------------------------------------------------------------------
;;;  Finally the sc68 file looks like this
;;; ----------------------------------------------------------------------

sc68:
	open	zingzong

	album	"Zingzong m68k test"
	artist	"Ben^OVR"
	track	"Test AGA","test.bin",AGA,START
	track	"Test STf",,PSG,START
	track	"Test STe",,STE,START
	track	"Test Falcon",,STE,START

	close	zingzong

;;; Local Variables:
;;; mode: asm
;;; indent-tabs-mode: t
;;; tab-width: 10
;;; comment-column: 40
;;; End:
