/**
 * @file    zz_tos.h
 * @author  Benjamin Gerard AKA Ben^OVR
 * @date    2018-01-24
 * @brief   Atari ST/TOS sound system calls
 */

#ifndef ZZ_TOS_H
#define ZZ_TOS_H

/*
  ----------------------------------------------------------------------

  Sound system calls

  ----------------------------------------------------------------------

  Because we are not using frame pointer it can be a bit tricky to use
  "g" or "m" constraints as it might generate addressing mode
  referring to a7.

  ----------------------------------------------------------------------
*/

static inline int32_t always_inline
xbios_na(int16_t func)
{
  int32_t ret;
  asm volatile (
    "  move.w  %[func],-(%%a7) \n"
    "  trap    #14             \n"
    "  addq.w  #2,%%a7         \n"
    "  move.l  %d0,%[ret]      \n\t"
    : [ret]  "=g" (ret)
    : [func] "i"  (func)
    : "cc","d0");
  return ret;
}

/*
  ----------------------------------------------------------------------
*/

static inline int32_t always_inline
xbios_w(int16_t func, int16_t par1)
{
  int32_t ret;
  asm volatile (
    "  move.w  %[par1],-(%%a7) \n"
    "  move.w  %[func],-(%%a7) \n"
    "  trap    #14             \n"
    "  addq.w  #4,%%a7         \n"
    "  move.l  %d0,%[ret]      \n\t"
    : [ret]  "=g" (ret)
    : [func] "i"  (func),
      [par1] "ig" (par1)
    : "cc","d0");
  return ret;
}

/*
  ----------------------------------------------------------------------
*/

static inline int32_t always_inline
xbios_ww(int16_t func, int16_t par1, int16_t par2)
{
  int32_t ret;
  asm volatile (
    "  move.w  %[par2],-(%%a7) \n"
    "  move.w  %[par1],-(%%a7) \n"
    "  move.w  %[func],-(%%a7) \n"
    "  trap    #14             \n"
    "  addq.w  #6,%%a7         \n"
    "  move.l  %d0,%[ret]      \n\t"
    : [ret]  "=g" (ret)
    : [func] "i"  (func),
      [par1] "ir" (par1),
      [par2] "ig" (par2)
    : "cc","d0");
  return ret;
}

/*
  ----------------------------------------------------------------------
*/

static inline int32_t
/**
 * Lock sound system for other applications.
 *
 * The xbios_locksnd() function locks the sound system to prevent it
 * being used by other applications at the same time.
 *
 * @retval   1  success
 * @retval  <0  already locked (-129)
 */
xbios_locksnd(void)
{
  return xbios_na(128);
}

/*
  ----------------------------------------------------------------------
*/

static inline int32_t
/**
 * Unlock sound system for use by other applications.
 *
 * The xbios_unlocksnd() function unlocks the sound system for use by
 * other applications, after it has been locked previously.
 *
 * @retval    0  success
 * @retval   <0  not locked (-128)
 */
xbios_unlocksnd(void)
{
  return xbios_na(129);
}

/*
  ----------------------------------------------------------------------
*/

static inline int32_t
/**
 * Set/get the parameters of the A/D and D/A converter.
 *
 * The xbios_soundcmd() function sets or obtains various parameters
 * of the A/D and D/A converter. The parameter mode determines the
 * command and data the setting to be performed.
 *
 * @param  mode determines the command:
 *  - `0`  L attenuation output channel in 1.5 dB increments
 *  - `1`  R attenuation output channel in 1.5 dB increments
 *  - `2`  L gain output channel in 1.5 dB increments
 *  - `3`  R gain output channel in 1.5 dB increments
 *  - `4`  Input source for the 16-bit hardware adder
 *  - `5`  Input source of the A/D converter
 *  - `6`  Compatibility to the STE sound system. (0:6.25khz .. 3:50khz)
 *  - `7`  Direct input of the sample frequency
 *  - `8`  Setting of the 8-bit sample format
 *  - `9`  Setting of the 16-bit sample format
 *  - `10` Setting of the 24-bit sample format
 *  - `11` Setting of the 32-bit sample format
 * @param  `-1` inquire / `Other` command data.
 * @return current settings
 */
xbios_soundcmd(int16_t mode, int16_t data)
{
  return xbios_ww(130,mode,data);
}
/*
  ----------------------------------------------------------------------
*/

static inline int32_t
/**
 * Set record/playback buffer.
 *
 * The xbios_setbuffer() sets the buffer address for the playback or
 * record data.
 *
 * @param  reg  0:playback 1:recording
 * @param  beg  start address
 * @param  end  end address (exclusive)
 * @retval 0 on success
 */
xbios_setbuffer(int16_t reg, void * beg, void *end)
{
  int32_t ret;
  asm volatile (
    "  move.l  %[end],-(%%a7) \n"
    "  move.l  %[beg],-(%%a7) \n"
    "  move.w  %[reg],-(%%a7) \n"
    "  move.w  #131,-(%%a7)   \n"
    "  trap    #14            \n"
    "  lea     12(%%a7),%%a7  \n"
    "  move.l  %%d0,%[ret]    \n\t"
    : [ret] "=g" (ret)
    : [reg] "i"	 (reg),
      [beg] "ir" (beg),
      [end] "ig" (end)
    : "cc","d0");
  return ret;
}

/*
  ----------------------------------------------------------------------
*/

static inline int32_t
/**
 * Select record/playback mode.
 *
 * @param  mode  0:2x8bit 1:2x16bit 2:1x8bit
 * @retval 0 on success
 */
xbios_setmode(int16_t mode)
{
  return xbios_w(132,mode);
}

/*
  ----------------------------------------------------------------------
*/

static inline int32_t
/**
 * Set number of record and playback tracks.
 *
 * The xbios_settracks() function sets the number of the recording and
 * playback tracks. A maximum of 4 stereo tracks is available at a
 * time.
 *
 * @param  playtracks  `-1`:skip 1-4:number of playback tracks
 * @param  rectracks   `-1`:skip 1-4:number of record tracks
 * @retval 0 on success
 */
xbios_settracks(int16_t playtracks, int16_t rectracks)
{
  return xbios_ww(133,playtracks,rectracks);
}

/*
  ----------------------------------------------------------------------
*/

static inline int32_t
/**
 * Set playback track for internal loudspeaker.
 *
 * The xbios_setmontracks() function specifies which track should be
 * output via the internal loudspeaker. This can only play back one
 * track at a time.
 *
 * @parma  montrack  track number to monitor [0-3].
 * @retval 0 on success
 */
xbios_setmontracks(int16_t montrack)
{
  return xbios_w(134,montrack);
}

/*
  ----------------------------------------------------------------------
*/

static inline int32_t
/**
 * Set interrupt at the end of recording/playback.
 *
 * The xbios_setinterrupt() function determines which interrupt is to
 * be generated by the sound system when the end of a recording or
 * playback buffer is reached.
 *
 * @param  src_inter  `0`:timer-A `1`:MFP-interrupt-7
 * @param  cause      `0`:No `1`:on-end-pb `2`:on-end-rec `4`:on-end-any
 * @retval 0 on success
 */
xbios_setinterrupt(int16_t src_inter, int16_t cause)
{
  return xbios_ww(135,src_inter,cause);
}

/*
  ----------------------------------------------------------------------
*/

static inline int32_t
/**
 * Set or read record/playback mode.
 *
 * @param  mode -1: Read status
 *              #0: Off/On DMA sound playback
 *              #1: Off/On Loop playback
 *              #2: Off/On DMA sound recording
 *              #3: Off/On Loop recording
 * @return 0 on success or current state
 */
xbios_buffoper(int16_t mode)
{
  return xbios_w(136,mode);
}

/*
  ----------------------------------------------------------------------
*/

static inline int32_t
/**
 * Coordinate external DSP hardware.
 *
 * The xbios_dsptristate() function uncouples connections from the
 * multiplexer connection matrix when external hardware is connected
 * to the SSI port of the DSP.
 *
 * @param  dspxmit  `0`:disconnect,  `1`:permits the connection
 * @param  dsprec   `0`:disconnect,  `1`:permits the connection
 * @return 0 on success or current state
 */
xbios_dsptristate(int16_t dspxmit, int16_t dsprec)
{
  return xbios_ww(137,dspxmit, dsprec);
}

/*
  ----------------------------------------------------------------------
*/

static inline int32_t
/**
 * Communication via the external DSP port.
 *
 * The xbios_gpio() function serves for communication via the general
 * purpose pins of the external DSP connector.
 *
 * The 3 pins are represented by the 3 lower bit in data or return
 * value.
 *
 * @param  mode  `0`:set I/O direction (0/1 <-> I/O)
 *               `1`:Get the three general purpose pins.
 *               `2`:Set the three general purpose pins.
 * @param  data  pins bitfield.
 * @retval 0 on success
 * @retval state if mode is was 1
 */
xbios_gpio(int16_t mode, int16_t data)
{
  return xbios_ww(138,mode,data);
}

/*
  ----------------------------------------------------------------------
*/

static inline int32_t
/**
 * Connect audio subsystem components.
 *
 * The xbios_devconnect() function connects a source component in the
 * audio subsystem to one or more destination components using the
 * connection matrix.
 *
 * @param src       0:DMA out 1:DSP out, 2:xtern 3:ADC
 * @param dst       1:DMA in, 2:DSP in,  4:xtern, 8:DAC
 * @param srcclk    0:25.175MHz 1:xtern 2:32MHz
 * @param prescale  0:ste 1-11: SPR=CLK/256/(prescale+1)
 * @param protocol  0:handshake
 * @retval 0 on success
 */
xbios_devconnect(int16_t src, int16_t dst,
		 int16_t clk, int16_t prescale,
		 int16_t protocol)
{
  int32_t ret;
  asm volatile (
    "  move.w  %[ptc],-(%%a7) \n"
    "  move.w  %[sca],-(%%a7) \n"
    "  move.w  %[clk],-(%%a7) \n"
    "  move.w  %[dst],-(%%a7) \n"
    "  move.w  %[src],-(%%a7) \n"
    "  move.w  #139,-(%%a7)   \n"
    "  trap    #14            \n"
    "  lea     12(%%a7),%%a7  \n"
    "  move.l  %%d0,%[ret]    \n\t"
    : [ret] "=g" (ret)
    : [ptc] "i"	 (protocol),
      [sca] "ir" (prescale),
      [clk] "i"	 (clk),
      [dst] "i"	 (dst),
      [src] "i"	 (src)
    : "cc","d0");
  return ret;
}

/*
  ----------------------------------------------------------------------
*/

static inline int32_t
/**
 * Get status of the A/D and D/A converter.
 *
 * @param  reset
 *  - `1`  Reinitialized the A/D and D/A converter
 *  - `2`  Get bit-depth (#0:8 #1:16 #2:24 #3:32)
 *  - `3`  Get available inputs for MasterMix output
 *  - `4`  Get available inputs for A/D converter
 *  - `5`  Get duplex operation
 *  - `8`  Get 8-bits sample formats
 *  - `9`  Get 16-bits sample formats
 *  - `10` Get 24-bits sample formats
 *  - `11` Get 32-bits sample formats
 *  - `0x8902` Get Frame base address
 *  - `0x890e` Get Frame end address
 *  - `0x8920` Get mode
 */
xbios_sndstatus(int16_t reset)
{
  return xbios_w(140,reset);
}

/*
  ----------------------------------------------------------------------
*/

static inline int32_t
/**
 * Get current record/playback position.
 *
 * The xbios_buffptr() function ascertains the current playback and
 * record positions in the corresponding buffers.
 *
 * @param ptr points to a buffer at least 4 longwords in size. The
 *            position pointers will be stored in that. The first
 *            int32_t of the buffer contains a pointer to the current
 *            playback position and the second int32_t a pointer to
 *            the current record position.
 *
 * @retval 0 on success
 */
xbios_buffptr(int32_t *ptr)
{
  int32_t ret;
  asm volatile (
    "  move.l  %[ptr],-(%%a7)  \n"
    "  move.w  #141,-(%%a7)    \n"
    "  trap    #14             \n"
    "  addq.w  #6,%%a7         \n"
    "  move.l  %%d0,%[ret]     \n\t"
    "0:\n"
    : [ret] "=g" (ret)
    : [ptr] "g"	 (ptr)
    : "cc","d0");
  return ret;
}

static inline void *
xbios_playptr(void)
{
  void *pos, *ptr[4];
  asm volatile (
    "  pea     (%[ptr])        \n"
    "  move.w  #141,-(%%a7)    \n"
    "  trap    #14             \n"
    "  addq.w  #6,%%a7         \n"
    "  move.l  %%d0,%[ret]     \n"
    "  beq     0f              \n"
    "  move.l  (%[ptr]),%[ret] \n\t"
    "0:\n"
    : [ret] "=g" (pos)
    : [ptr] "a"	 (ptr)
    : "cc","d0");
  return pos;
}

static inline void *
xbios_recordptr(void)
{
  void *pos, *ptr[4];
  asm volatile (
    "  pea     (%[ptr])         \n"
    "  move.w  #141,-(%%a7)     \n"
    "  trap    #14              \n"
    "  addq.w  #6,%%a7          \n"
    "  move.l  %%d0,%[ret]      \n"
    "  beq     0f               \n"
    "  move.l  4(%[ptr]),%[ret] \n\t"
    "0:\n"
    : [ret] "=g" (pos)
    : [ptr] "a"	 (ptr)
    : "cc","d0");
  return pos;
}

#endif /* ZZ_TOS_H */
