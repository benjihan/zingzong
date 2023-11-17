/**
 * @file   stub.S
 * @author Benjamin Gerard AKA Ben^OVR
 * @date   2017-08-03
 * @brief  zingzong player stub.
 *
 */

	.section .text

zingzong:
zingzong_init: bra.w player_init	// -> d0.l: 0:error 1:ready
zingzong_kill: bra.w player_kill	// -> void
zingzong_play: bra.w player_play	// -> void
zingzong_mute: bra.w player_mute	// -> d0.b: old value
zingzong_cmap: bra.w player_blend	// -> d0.l: old values (MSW/LSW)
zingzong_vers: bra.w zz_core_version	// -> d0.l: version string
zingzong_stat: bra.w player_status	// -> d0.l: error-code (0=OK)
zingzong_samp: bra.w player_sampling	// -> d0.l: sampling rate (in hz)
zingzong_driv: bra.w player_driver	// -> d0.l: driver (mixer_t *)
zingzong_rate: bra.w player_rate	// -> d0.l: song tick rate
zingzong_core: bra.w player_core	// -> d0.l: internal zz_core_t

	.global zingzong
	.global zingzong_init
	.global zingzong_kill
	.global zingzong_play
	.global zingzong_mute
	.global zingzong_cmap
	.global zingzong_vers
	.global zingzong_stat
	.global zingzong_samp
	.global zingzong_driv
	.global zingzong_rate
	.global zingzong_core

/*
Local Variables:
mode: asm
indent-tabs-mode: t
tab-width: 10
comment-column: 40
End:
*/
