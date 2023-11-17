/// @file   sc68_start.s
/// @author Benjamin Gerard AKA Ben^OVR
/// @date   2017-08
/// @brief  sc68 startup for zingzong
///
/// Because of the 64k limitation of PC relative offset we have to
/// generate long offset to reach the song and voiceset data.


	.global	sc68_start

	.section .text

sc68_start:
	bra.w	0f
	bra.w	player_kill
	bra.w	player_play

0:
	lea	sc68_start(pc),a0
	lea	sc68_start.l,a0

	move.l	#0xAD235535,d7
	move.l	address(pc),d6
	cmp.l	d6,d7
	beq.s	1f
	illegal
1:
	clr.l	-(a7)	// Sampling/Quality (0:default)

	// illegal
	move.l	d0,-(a7)	   // Auto:0, AGA:1 STf:2 STe:3 Falcon:4

	lea	to_set(pc),a0
	adda.l	(a0),a0
	pea	(a0)		// set.l

	lea	to_sng(pc),a0
	adda.l	(a0),a0
	pea	(a0)		// sng.l

	bsr	player_init
	lea	16(a7),a7

	rts

	// To be allocated and set by the linker script (sc68.x)
	.align	4
	.set	address,.+0
	.set	to_sng,.+4
	.set	to_set,.+8

// Local Variables:
// mode: asm
// indent-tabs-mode: t
// tab-width: 10
// comment-column: 40
// End:
