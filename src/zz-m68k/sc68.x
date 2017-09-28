/**
 *  @file    sc68.x
 *  @author  Benjamin Gerard AKA Ben/OVR
 *  @date    2017-09
 *  @brief   Linker script for building zingzong test file.
 */

SECTIONS
{
  .text 0x10000 :
  {
    sc68_start.o(.text);
    . = ALIGN(4);
    LONG(song_bin - .);
    LONG(vset_bin - .);
    player.o(.text) zz_*.o(.text) dri_*.elf(.text)
  }
  text_section_adr = ADDR(.text);
  text_section_len = SIZEOF(.text);
  text_section_end = text_section_adr + text_section_len;

  .data ALIGN(4):
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

  OVERLAY ALIGN(4) :
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

  .data.song ALIGN(4) :
  { 
    song.elf(.song.bin);
    song_adr = ABSOLUTE(.);
    song.elf(.song.dat);
    song_len = ABSOLUTE(.) - song_adr;
    . = ALIGN(16);
    . += 16;
    song_max = ABSOLUTE(.) - song_adr;
  }

  .data.vset ALIGN(16) :
  { 
    vset.elf(.vset.bin);
    vset_adr = ABSOLUTE(.);
    vset.elf(.vset.dat);
    vset_len = ABSOLUTE(.) - vset_adr;
    . = ALIGN(16);
    . += 1024;
    vset_max = ABSOLUTE(.) - vset_adr;
  }
}
