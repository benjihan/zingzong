/**
 * @file   zz_tos.h
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2018-01-24
 * @brief  Atari ST/TOS system calls
 */

#ifndef ZZ_TOS_H
#define ZZ_TOS_H

static inline
/* @retval     1 success
 * @retval  -129 already locked
 */
int32_t Locksnd(void)
{
  int32_t ret;
  asm volatile(
    "move.w  #0x80,-(%%a7) \n\t"
    "trap    #14           \n\t"
    "addq.w  #2,%%a7       \n\t"
    : [ret] "=d" (ret));
  return ret;
}

static inline
/* @retval     0 success
 * @retval  -128 not locked
 */
int32_t Unlocksnd(void)
{
  int32_t ret;
  asm volatile(
    "move.w  #0x81,-(%%a7) \n\t"
    "trap    #14           \n\t"
    "addq.w  #2,%%a7       \n\t"
    : [ret] "=d" (ret));
  return ret;
}

static inline
/* @param  mode  0:L-atten 1:R-atten 2:L-gain 3:R-gain 4:adder-in 5:adcinput
 * @param  data -1:inquire
 * @retval prior value ?
 */
int32_t Soundcmd(int16_t mode, int16_t data)
{
  int32_t ret;
  asm volatile(
    "move.w %[data],-(%%a7) \n\t"
    "move.w %[mode],-(%%a7) \n\t"
    "move.w #0x82,-(%%a7)   \n\t"
    "trap   #14             \n\t"
    "addq.w #6,%%a7         \n\t"
    : [ret] "=g" (ret)
    : [mode] "g" (mode), [data] "g" (data)
    );
  return ret;
}

static inline
/* @retval  0  on success */
int32_t Setbuffer(int16_t mode, void * beg, void *end)
{
  int32_t ret;

  asm volatile(
    "move.l %[end],-(%%a7) \n\t"
    "move.l %[beg],-(%%a7) \n\t"
    "move.w %[mod],-(%%a7) \n\t"
    "move.w #0x83,-(%%a7)  \n\t"
    "trap   #14            \n\t"
    "lea   12(%%a7),%%a7   \n\t"
    : [ret] "=g" (ret)
    : [beg] "g" (beg), [end] "g" (end), [mod] "g" (mode)
    );
  return ret;
}

static inline
/* @param  mode  0:2x8 1:2x16 2:1x8
 * @retval 0 on success
 */
int32_t Setmode(int16_t mode)
{
  int32_t ret;
  asm volatile(
    "move.w %[mode],-(%%a7) \n\t"
    "move.w #0x84,-(%%a7)   \n\t"
    "trap   #14             \n\t"
    "addq.w #4,%%a7         \n\t"
    : [ret] "=g" (ret)
    : [mode] "g" (mode)
    );
  return ret;
}

static inline
int32_t Settracks(int16_t play, int16_t record)
{
  int32_t ret;
  asm volatile(
    "move.w %[rec],-(%%a7) \n\t"
    "move.w %[pla],-(%%a7) \n\t"
    "move.w #0x83,-(%%a7)  \n\t"
    "trap   #14            \n\t"
    "addq.w #6,%%a7        \n\t"
    : [ret] "=g" (ret)
    : [pla] "g" (play), [rec] "g" (record)
    );
  return ret;
}

static inline
/* @param  reset  1:reset the sound system
 * @retval #0-3   0:success
 *         #4     set if left channel has clip
 *         #5     set if right channel has clip
 *         #6-31  unused
 */
int32_t Sndstatus(int16_t reset)
{
  int32_t ret;
  reset = !!reset;
  asm volatile(
    "move.w %[rs],-(%%a7)  \n\t"
    "move.w #0x8c,-(%%a7)  \n\t"
    "trap   #14            \n\t"
    "addq.w #4,%%a7        \n\t"
    : [ret] "=g" (ret)
    : [rs] "g" (reset)
    );
  return ret;
}

static inline
int32_t Devconnect(int16_t src, int16_t dst,
                   int16_t clk, int16_t prescale, int16_t protocol)
{
  int32_t ret;

  asm volatile(
    "move.w  %[ptc],-(%%a7) \n\t"
    "move.w  %[sca],-(%%a7) \n\t"
    "move.w  %[clk],-(%%a7) \n\t"
    "move.w  %[dst],-(%%a7) \n\t"
    "move.w  %[src],-(%%a7) \n\t"
    "move.w  #0x8B,-(%%a7)  \n\t"
    "trap    #14            \n\t"
    "lea     12(%%a7),%%a7  \n\t"
    : [ret] "=g" (ret)
    : [src] "g" (src), [dst] "g" (dst), [clk] "g" (clk),
      [sca] "g" (prescale), [ptc] "g" (protocol)
    );

  return ret;
}

#endif
