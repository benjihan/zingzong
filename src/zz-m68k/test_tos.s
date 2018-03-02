;;; @file   test_tos.s
;;; @author Benjamin Gerard AKA Ben/OVR
;;; @date   2017-12
;;; @brief  zingzong TOS test
;;;

        ifnd    MIXERID
MIXERID set     0               ; (0:Auto 1:Amiga 2:YM 3:DMA8 4:DMA16
        endc

llea:   macro
        lea     vars(pc),\2
        adda.l  #\1-vars,\2
        endm

        opt     a+,o+,p+

        ;; Welcome and initializing message
        pea     msgA(pc)        ; >> Zingzong ...
        move.w  #9,-(a7)        ; Cconws(msg.l)
        trap    #1
        addq    #6,a7

        ;; Fill stacks with a value to detect stack usage
        llea    mytop,a6
        move.l  #$55555555,d4
        move.l  d4,d5
        move.l  d4,d6
        move.l  d4,d7
        move.w  #(mytop-mybot)/16-1,d1
fill:   movem.l d4-d7,-(a6)
        dbf     d1,fill

        ;; Become superuser
        clr.l   -(a7)           ; null -> superuser
        move.w  #32,-(a7)       ; Super(ssp.l)
        trap    #1
        addq    #6,a7

        ;; Save stack pointers
        lea     vars(pc),a6
        move.l  d0,susp(a6)     ; returned by super()

        ;; Setup new stacks
        move.l  a6,a1
        adda.l  #myusp-vars,a1  ; a1: myusp
        move.l  a1,usp
        adda.l  #myssp-myusp,a1 ; a1: myssp
        move.l  a1,a7

        ;; Save stuff
        move.w  $ffff8240.w,s240(a6)
        move.b  $484.w,s484(a6)
        and.b   #~3,$484.w      ; Remove click and repeat

        ;; Init player
	bsr	zz_init

        ;; Wait for key message
        pea     msgB(pc)        ; >> Press ...
        move.w  #9,-(a7)        ; Cconws(msg.l)
        trap    #1
        addq    #6,a7

        move.l  $4ba.w,hz200(a6)
mainloop:
        ;; Test console input device (kbd)
        move.w  #2,-(a7)        ; CON: (kbd)
        move.w  #1,-(a7)        ; Bconstat(dev.w)
        trap    #13
        addq.w  #4,a7
        tst.w   d0
        beq.s   nokey

        ;; Read console input device (kbd)
        move.w  #2,-(a7)        ; CON: (kbd)
        move.w  #2,-(a7)        ; Bconin(dev.w)
        trap    #13             ; => d0.l: scan.w | code.w
        addq.w  #4,a7
        cmp.w   #27,d0          ; <ESC>?
        beq     exitloop
        cmp.w   #32,d0          ; <SPC>?
        beq.s   exitloop

nokey:
        move.l  $4ba.w,d0
        cmp.l   vars+hz200(pc),d0
        beq.s   mainloop
        lea     vars(pc),a6
        move.l  d0,hz200(a6)

        ;; Run music player
        move.w  #$F55,$ffff8240.w
        bsr     zz_play
        move.w  #$FFF,$ffff8240.w

        bra.s   mainloop

exitloop:
        pea     msgC(pc)        ; >> Restoring
        move.w  #$09,-(a7)
        trap    #1
        addq.w  #6,a7

        ;; Stop timers and musics
        bsr     zz_kill

        ;; Save vectors and MFP setup
        lea     vars(pc),a6
        move.b  s484(a6),$484.w
        move.w  s240(a6),$ffff8240.w

        stop    #$2300

        ;; Back to user mode
        move.l  susp(a6),-(a7)  ; previous stack
        move.w  #32,-(a7)       ; Super(ssp.l)
        trap    #1
        addq    #6,a7

        ;; Check for memory commando
        llea    prot1,a6
        movem.l (a6),d0-d1
        cmp.l   #"Ben/",d0
        sne     d0
        cmp.l   #"OVR!",d1
        sne     d1
        or.b    d0,d1
        beq.s   usage

        ;; CRITICAL error
        pea     msgE(pc)        ; >> CRITICAL ERROR ...
        move.w  #9,-(a7)        ; Cconws(msg.l)
        trap    #1
        addq    #6,a7
        bra     wait_message

;;; a0.l : bot
;;; a1,l : top
;;; d0.l : => usage
;;; d1.l : trashed
chkstack:
        moveq   #0,d0
        move.l  a1,d1
        sub.l   a0,d1
        ble.s   .wtf

        moveq   #$55,d0
        subq.w  #1,d1
.lp:
        cmp.b   (a0)+,d0
        dbne    d1,.lp
        sne     d0
        neg.b   d0
        add.l   a1,d0
        sub.l   a0,d0
.wtf:
        rts

xdigit: dc.b    "0123456789ABCDEF"

;;; a0.l : text
;;; d0.w : value
;;; d1.l,d2.l : trashed
hexa:   moveq   #3,d2
.lp:
        moveq   #15,d1
        rol.w   #4,d0
        and.b   d0,d1
        move.b  xdigit(pc,d1.w),(a0)+
        dbf     d2,.lp
        rts

usage:
        llea    mybot,a0
        llea    myssp,a1
        bsr.s   chkstack
        lea     sspV(pc),a0
        bsr.s   hexa

        llea    myssp,a0
        llea    myusp,a1
        bsr.s   chkstack
        lea     uspV(pc),a0
        bsr.s   hexa

stack_message:
        pea     msgF(pc)        ; >> stack usage
        move.w  #9,-(a7)        ; Cconws(msg.l)
        trap    #1
        addq    #6,a7

wait_message:
        pea     msgB(pc)        ; >> Press ...
        move.w  #9,-(a7)        ; Cconws(msg.l)
        trap    #1
        addq    #6,a7

wait:
        ;; Read console input device (kbd)
        move.w  #2,-(a7)        ; CON: (kbd)
        move.w  #2,-(a7)        ; Bconin(dev.w)
        trap    #13             ; => d0.l: scan.w | code.w
        addq.w  #4,a7
        cmp.w   #27,d0          ; <ESC>?
        beq.s   exit
        cmp.w   #32,d0          ; <SPC>?
        bne.s   wait

exit:
        ;; Exit message
        pea     msgD(pc)        ; >> Bye !
        move.w  #$09,-(a7)      ; Cconws()
        trap    #1
        addq    #6,a7

        ;; Return GEM/caller
        clr     -(a7)           ; Pterm()
        trap    #1
        illegal                 ; Safety net


zz_init:	
	movem.l	d0-d1/a0-a1,-(a7)

	;; Sampling/Quality (0:default)
	clr.l	-(a7)
	
	;; Driver (0:Auto 1:AGA 2:STf 3:STe 4:Falcon)
	pea	MIXERID.w

	;; vset bin_t address
	lea	tovset(pc),a0
	adda.l	(a0),a0
	pea	(a0)
	
	;; song bin_t address
	lea	tosong(pc),a0
	adda.l	(a0),a0
	pea	(a0)

	bsr	zingzong
	lea	16(a7),a7
	
	movem.l	(a7)+,d0-d1/a0-a1
	rts
	
zz_kill:
	movem.l	d0-d1/a0-a1,-(a7)
	bsr	zingzong+4
	movem.l	(a7)+,d0-d1/a0-a1
	rts

zz_play:
	movem.l	d0-d1/a0-a1,-(a7)
	bsr	zingzong+8
	movem.l	(a7)+,d0-d1/a0-a1
	rts
	
        ;; Variable definitions
        rsreset
hz200:  rs.l    1
susp:   rs.l    1
s240:   rs.w    1
s484:   rs.b    1
varsz:  rs.b    0

        ;; Variable allocations
vars:   ds.b    varsz

        ;; Text messages
msgA:   dc.b    27,"E"
        dc.b    "Zingzong quartet music player",13,10,10
        dc.b    "By Ben/OVR 2017",13,10,10
        dc.b    ">> Please wait initializing <<"
        dc.b    0

msgC:   dc.b    13
        dc.b    ">> Restoring <<",27,"K"
        dc.b    0

msgD:   dc.b    27,"E"
        dc.b    "Bye !"
        dc.b    0

msgE:   dc.b    27,"E"
        dc.b    ">> CRITICAL ERROR <<",13,10
        dc.b    "Stack or buffer overflow",10,10
        dc.b    0

msgB:   dc.b    13
        dc.b    ">> Press <ESC> or <SPC> to exit <<",27,"K"
        dc.b    0

msgF:   dc.b    13
        dc.b    "Stack usage: USP=$"
uspV:   dc.b    "0000 / SSP=$"
sspV:   dc.b    "0000",10,10
        dc.b    0

;;; ----------------------------------------
;;; Player and Music Files
	
	even

tosong:	dc.l	song-*
tovset:	dc.l	vset-*
	
zingzong:
	incbin	"zingzong.bin"
	even

	;; Song (.4v)
	
song:	dc.l	0
	dc.l	song_max
	dc.l	song_len
song_bin:
	incbin	"song.dat"	; .4v file
song_len: equ *-song_bin
	even
	ds.l	3		; for closing incomplete files
song_max: equ *-song_bin


	;; Voice set (.set)
	
vset:	dc.l	0
	dc.l	vset_max
	dc.l	vset_len
vset_bin:
	incbin	"vset.dat"	; .set file
vset_len: equ *-vset_bin
	even
	ds.b	2048		; loop unroll extra space
vset_max: equ *-vset_bin

;;; ----------------------------------------
;;; Stack and memory protection
	
        ;; Memory commando check
        even
prot1:  dc.b    "Ben/OVR!"

        ;; Stacks buffer
mybot:
        ;; Superuser stack
        ds.l    128
myssp:
        ;; User stack
        ds.l    32
myusp:
mytop:
