/**
 *  @file    zingzong.x
 *  @author  Benjamin Gerard AKA Ben/OVR
 *  @date    2017-09
 *  @brief   Linker script for building zingzong player
 */

SECTIONS
{
  .text 0x10000 :
  {
    player_start.o(.text)
    player.o(.text)
    zz_*.o(.text)
    dri_*.elf(.text)
  }
  text_section_adr = ADDR(.text);
  text_section_len = SIZEOF(.text);
  text_section_end = text_section_adr + text_section_len;

  .data ALIGN(16):
  {
    *(.data*) *(.rodata*);
  }
  data_section_adr = ADDR(.data);
  data_section_len = SIZEOF(.data);
  data_section_end = data_section_adr + data_section_len;

  /* GB: Create an overlay for drivers BSS. We don't need them all
   *     at the same time so basically we only allocate the largest
   *     driver BSS.
   */

  OVERLAY ALIGN(16) :
  {
    .dri.any { }
    .dri.aga { dri_aga.elf(.bss); }
    .dri.stf { dri_stf.elf(.bss); }
    .dri.ste { dri_ste.elf(.bss); }
    .dri.fal { dri_fal.elf(.bss); }
  }
  dribss_section_adr = ADDR(.dri.any);
  dribss_section_end = ABSOLUTE(ALIGN(16));
  dribss_section_len = dribss_section_end - dribss_section_adr;

  .bss dribss_section_adr : AT(dribss_section_adr)
  {
    . += dribss_section_len;
    *(.bss) *(COMMON);
  }
}
