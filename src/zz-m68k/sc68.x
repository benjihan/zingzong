/**
 *  @file    sc68.x
 *  @author  Benjamin Gerard AKA Ben^OVR
 *  @date    2017-09
 *  @brief   Linker script for building zingzong test file.
 */

SECTIONS
{
  .text :
  {
    sc68_start.o(.text);
    . = ALIGN(4);
    LONG(0xAD235535);
    LONG(song_bin - .);
    LONG(vset_bin - .);
    m68k_*.o(.text)
    player.o(.text)
    zz_*.o(.text)
    dri_*.elf(.text)
  }
  text_section_end = .;
  text_section_adr = ADDR(.text);
  text_section_len = SIZEOF(.text);

  .data ALIGN(4):
  {
    *(.data*) *(.rodata*);
  }
  data_section_end = .;
  data_section_adr = ADDR(.data);
  data_section_len = SIZEOF(.data);

  /* GB: Create an overlay for drivers BSS. We don't need them all
   *     at the same time so basically we only allocate the largest
   *     driver BSS.
   */

  OVERLAY ALIGN(4):
  {
    .dri.any { }
    .dri.aga { dri_aga.elf(.bss); }
    .dri.stf { dri_stf.elf(.bss); }
    .dri.hub { dri_hub.elf(.bss); } /* should be empty ! */
    .dri.lrb { dri_lrb.elf(.bss); }
    .dri.dnr { dri_dnr.elf(.bss); }
    .dri.s7s { dri_s7s.elf(.bss); }
    .dri.fal { dri_fal.elf(.bss); }
  }
  dribss_section_len = ABSOLUTE(ALIGN(4) - ADDR(.dri.any));

  .bss ADDR(.dri.any) : AT(ADDR(.dri.any))
  {
    . += dribss_section_len;
    *(.bss) *(COMMON);
  }
  bss_section_end = .;
  bss_section_adr = ADDR(.bss);
  bss_section_len = SIZEOF(.bss);

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
    . += 2048;
    vset_max = ABSOLUTE(.) - vset_adr;
  }
}
