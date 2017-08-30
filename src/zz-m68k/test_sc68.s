| sc68 header for zingzong test
|

	.equ	start,0
	.equ	AMIGA,4

	
	.macro	endian,val
	.byte	(\val)
	.byte	(\val)>>8
	.byte	(\val)>>16
	.byte	(\val)>>24
	.endm

	.macro	chtag tag,size
	.ascii	"\tag"
	endian	\size
	.endm
	
	.macro	chint tag,val
	.ascii	"\tag"
	endian	4
	endian	\val
	.endm

	.macro	begdat tag
	.ascii	"\tag"
	endian	3f-2f
2:	
	.endm

	.macro	enddat
	.align	2
3:
	.endm

| ======================================================================
	
sc68:
	.asciz	"SC68 Music-file / (c) (BeN)jamin Gerard / SasHipA-Dev  "
begin:	
	chtag	SC68,theend-begin

theend:	
	.end
	
	begdat	SCFN
	.asciz	"Zingzong m68k test"
	enddat

	begdat	SCAN
	.asciz	"Ben/OVR"
	enddat
	.end

	;;
	.macro track name,file,frames=0,addr=0,
	chtag	SCMU,0
	
	.if	\addr
	chint	SCAT,\addr
	.endif
	
	chint	SCFQ,200
	
	.if	\frames
	chint	SCFR,\frames
	.endif
	
	chint	SCTY,AMIGA
	
	begdat	SCMN
	dc.b	\,0
	enddat
	
	begdat	SCDA
	innbin	"\file"
	enddat
	.endm

	track	"Test AGA","test.dat",addr=start
	
	chtag	"SCEF",0
theend:
