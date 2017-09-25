/**
 * @file   player_start.S
 * @author Benjamin gerard AKA Ben/OVR
 * @date   2017-08-03
 * @brief  zingzong player stub.
 *
 * Implements sc68 player calling convention:
 *
 * +0 init (a0: a1: d0:)
 * +4 kill
 * +8 play
 */

	.section .text
	.global player_start

/**
 * player_start()
 * 	
 * @param  a0.l: 'QUA!' file or offset to bin_t 
 * @param  d0.w: song number
 */
player_start:	
        bra.w   0f
        bra.w   player_kill
        bra.w   player_play
0:	
	// TODO

	// { a0=song_bin, a1=vset_bin }
	pea	(a0)		// push vset_bin
	pea	(a1)		// push song_bin
	bsr	player_init	// run init
	addq.w	#8,a7		// adjust stack
	rts
