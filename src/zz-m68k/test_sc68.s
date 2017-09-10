;;; sc68 header for zingzong test
;;;

start:	equ	0		; 0:PCR

;;; Hardware flags
PSG:	set	1		; YM-2149 flag
STE:	set	2		; STe flag
AGA:	set	4		; Amiga flag

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

;;; track(title,file,type,addr,frames)
;;; ------ /1 -- /2 - /3 - /4 -- /5 --
;;; declare a new track (song)
	;;
track:	macro
	;; 
	chtag	SCMU,0
	;; 
	ifne	\4
	chint	SCAT,\3
	endc
	;; 
	chint	SCFQ,200
	;; 
	ifne	\5
	chint	SCFR,\5
	endc
	;; 
	chint	SCTY,\3
	;; 
	begdat	SCMN
	asciz	\1
	enddat
	;;
	if	\?2
	begdat	SCDA
	incbin	\2
	enddat
	endc
	;; 
	endm

	
;;; ======================================================================
;;; 
	
sc68:
	asciz	"SC68 Music-file / (c) (BeN)jamin Gerard / SasHipA-Dev  "
begin:	
	chtag	SC68,theend-begin
	
	begdat	SCFN
	asciz	"Zingzong m68k test"
	enddat

	begdat	SCAN
	asciz	"Ben/OVR"
	enddat

	track	"Test AGA","test.dat",AGA,0,start
	track	"Test STf",,PSG,0,start
	track	"Test STe",,STE,0,start
	
	chtag	SCEF,0
theend:
