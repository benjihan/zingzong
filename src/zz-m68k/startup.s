/**
 * @file   startup.s
 * @author Benjamin gerard AKA Ben/OVR
 * @date   2017-08-03
 * @brief  zingzong m68k startup.
 */

	.section .text
	.global _start
/*	.org	0x10000,0xC4C4B0D1*/
_start:	
        braw    player_init
        braw    player_kill
        braw    player_play
	
	.long	_start
