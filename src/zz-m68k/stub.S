/**
 * @file   stub.S
 * @author Benjamin gerard AKA Ben/OVR
 * @date   2017-08-03
 * @brief  zingzong player stub.
 *
 * This interface is still in progress however the 3 major functions
 * should not change.
 *
 * +$00 long init(bin_t * song, bin_t * vset, long driver, long sampling)
 * +$04 void kill(void)
 * +$08 void play(void)
 * +$0C long version(void)
 * ------------------------------
 * GB: Disabled for now (program has reach critical mass)
 * ------------------------------	
 * +$10	long player_status(void)
 * +$14 long player_sampling(void)
 * +$18	long player_driver()
 * +$1C long player_internal(void)
 *
 */
	.section .text
	.global zingzong
zingzong:
        bra.w   player_init	// -> d0.l: 0:error 1:ready
        bra.w   player_kill	// -> void
        bra.w   player_play	// -> void
	bra.w	zz_version	// -> d0.l: version string

	//bra.w	player_status	// -> d0.l: 0:not ready 1:ready
	//bra.w	player_sampling // -> d0.l: sampling rate in hz
	//bra.w	player_driver	// -> d0.l: driver
	//bra.w	player_internal	// -> d0.l: internal zz_play_t
