/**
 * @file   stub.S
 * @author Benjamin gerard AKA Ben/OVR
 * @date   2017-08-03
 * @brief  zingzong player stub.
 *
 * +$00 long init(bin_t * song, bin_t * vset, long driver, long sampling)
 * +$04 void kill(void)
 * +$08 void play(void)
 * +$0C	long player_status(void)
 * +$10 void *player_internal(void)
 *
 */
	.section .text
	.global zingzong

zingzong:
        bra.w   player_init	// -> d0.l: 0:error 1:ready
        bra.w   player_kill	// -> (void)
        bra.w   player_play	// -> (void)
	bra.w	player_status	// -> d0.l: 0:not ready 1:ready
	bra.w	player_internal	// -> d0.l: internal zz_play_t
