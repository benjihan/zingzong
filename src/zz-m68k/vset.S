/// @file   vset.S
/// @author Benjamin Gerard AKA Ben/OVR
/// @date   2017-08

	.global	vset_bin
	.section .vset.bin

vset_bin:
	.long	0
	.long	vset_max
	.long	vset_len
