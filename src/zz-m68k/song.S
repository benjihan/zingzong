/// @file   song.S
/// @author Benjamin Gerard AKA Ben^OVR
/// @date   2017-08

	.global	song_bin
	.section .song.bin

song_bin:
	.long	0
	.long	song_max
	.long	song_len

// Local Variables:
// mode: asm
// indent-tabs-mode: t
// tab-width: 10
// comment-column: 40
// End:
