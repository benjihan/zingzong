;;; @file   test_tos.s
;;; @author Benjamin Gerard AKA Ben/OVR
;;; @date   2017-12
;;; @brief  zingzong TOS test
;;;

	opt	a+,o+,p+
	
	;; Super()
	clr.l	-(a7)
	move.w	#32,-(a7)
	trap	#1
	addq	#6,a7
	move.l	d0,-(a7)	; store user stack for later

	move.w	#$2700,sr

	;; No key clicks
	lea	vars(pc),a0
	move.b	$484.w,s484(a0)
	move.b	#$E,$484.w

	;; Save vectors and MFP setup
	move.l	$070.w,s070(a0)	; VBL
	move.l	$114.w,s114(a0)	; timer-c
	move.l	$134.w,s134(a0)	; timer-a
	
	move.b	$fffffa07.w,sa07(a0)
	move.b	$fffffa09.w,sa09(a0)
	move.b	$fffffa13.w,sa13(a0)
	move.b	$fffffa15.w,sa15(a0)
	move.b	$fffffa17.w,sa17(a0)

	;; Cconws()
	pea	msgA(pc)
	move.w	#$09,-(a7)
	trap	#1
	addq	#6,a7
	
	;; Init player
	moveq	#MIXERID,d0
	bsr	music

	;; Cconws()
	pea	msgB(pc)
	move.w	#$09,-(a7)
	trap	#1
	addq	#6,a7
	
	;; ----------------------------------------
	move.w	#$2700,sr
	
 	move.b	#$12,d0		; Disable mouse
	bsr	put_ikbd

 	move.b	#$15,d0		; Disable joystick
	bsr	put_ikbd
	
	clr.b	$fffffa07.w	; IERA
	clr.b	$fffffa09.w	; IERB
	clr.b	$fffffa13.w	; IMRA
	clr.b	$fffffa15.w	; IMRB
	bclr.b	#3,$fffffa17.w	; AEI
	
	clr.b	$fffffa19.w	; TA/CR
	clr.b	$fffffa1d.w	; TCD/CR

	lea	myvbl(pc),a1	; VBL
	move.l	a1,$70.w	;
	
	lea	timerc(pc),a1
	move.l	a1,$114.w
	moveq	#$20,d0		 ; set #5
	move.b	d0,$fffffa07.w	 ; IERA
	move.b	d0,$fffffa13.w	 ; IMRA
	move.b	d0,$fffffa09.w	 ; IERB
	move.b	d0,$fffffa15.w	 ; IMRB
	
	move.b	#$c0,$fffffa23.w ; for 200hz timer-c
	move.b  #$50,$fffffa1d.w ; 2457600/64/192 -> 200hz
	not.b	d0		 ; clr #5
	move.b	d0,$fffffa11.w	 ; ISRB
	
	;; ----------------------------------------

	stop	#$2300
mainloop:
	btst	#1,$fffffc00.w
	beq	mainloop
	
	move.b	$fffffc02.w,d0
	cmp.b	#$39,d0
	bne	mainloop
	
exitloop:
	
	;; Stop timers and musics
	move	#$2700,sr
	bsr	music+4

	;; Save vectors and MFP setup
	move	#$2700,sr
	lea	vars(pc),a0
	move.l	s070(a0),$070.w	; VBL
	move.l	s114(a0),$114.w	; timer-c
	move.l	s134(a0),$134.w	; timer-a
	move.b	sa07(a0),$fffffa07.w
	move.b	sa09(a0),$fffffa09.w
	move.b	sa13(a0),$fffffa13.w
	move.b	sa15(a0),$fffffa15.w
	move.b	sa17(a0),$fffffa17.w
	move.b	s484(a0),$484.w
	
	stop	#$2300
	bsr	clear_acias

 	move.b	#$8,d0		; Enable mouse
	bsr	put_ikbd

	move.w	#$fff,$ffff8240.w
		
	;; User()
	move.w	#32,-(a7)
	trap	#1
	addq	#6,a7

	;; Cconws()
	pea	msgD(pc)
	move.w	#$09,-(a7)
	trap	#1
	addq	#6,a7
	
	;; Pterm()
	clr	-(a7)
	trap	#1
	illegal

myvbl:	move.w	#$755,$ffff8240.w
	rte

;;; *******************************************************
;;; 200hz music player
;;; 
timerc:
	move.w	#$2300,sr
	movem.l	d0-d1/a0-a1,-(a7)
	move.w	#$566,$ffff8240.w
	bsr	music+8
	move.w	#$fff,$ffff8240.w
	movem.l	(a7)+,d0-d1/a0-a1
	rte

;;; *******************************************************
;;; Write a command to ikbd
;;;
;;;  Inp: d0.b command to write
;;;
put_ikbd:
	btst	#1,$fffffc00.w
	beq.s	put_ikbd
	move.b	d0,$fffffc02.w
	rts

;;; *******************************************************
;;; Reset keyboard
;;;
clear_acias:
	moveq	#$13,d0		; disable transfert
	bsr	put_ikbd
	
.flushing:
	moveq.l	#-95,d0		; 1010 0001
	and.b	$fffffc00.w,d0
	beq.s	.flushed
	move.b	$fffffc02.w,d0
	bra.s	.flushing
.flushed:
	moveq	#$11,d0		; enable transfert
	bsr	put_ikbd
	rts
	
	rsreset
s070:	rs.l	1
s114:	rs.l	1
s134:	rs.l	1
sa07:	rs.b	1
sa09:	rs.b	1
sa13:	rs.b	1
sa15:	rs.b	1
sa17:	rs.b	1
s484:	rs.b	1
varsz:	rs.b	0

vars:	ds.b	varsz
	
msgA:	dc.b	27,"E"
	dc.b	"Zingzong quartet music player",13,10,10
	dc.b	"By Ben/OVR 2017",13,10,10
	dc.b	">> Please wait initializing <<",0
msgB:	dc.b	13
	dc.b	">> Press <SPC> to exit <<",27,"K",0
msgC:	dc.b	13
	dc.b	">> Restoring <<",27,"K",0
msgD:	dc.b	27,"E","Bye !",0
	
	even

music:	incbin	"test.bin"
