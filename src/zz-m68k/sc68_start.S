/// @file   sc68_start.s
/// @author Benjamin Gerard AKA Ben/OVR
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
	move.l	d0,-(a7)	// mixer-id.l

	lea	to_set(pc),a0
	adda.l	(a0),a0
	pea	(a0)		// set.l

	lea	to_sng(pc),a0
	adda.l	(a0),a0
	pea	(a0)		// sng.l

	bsr	player_init
	lea	12(a7),a7

	rts

	.align	4
	// To be allocated and set by the linker script (sc68.x)
	.set	to_sng,.+0
	.set	to_set,.+4
