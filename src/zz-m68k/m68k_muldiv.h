/**
 * @file    m68k_muldiv.h
 * @author  Benjamin Gerard AKA Ben^OVR
 * @date    2017-07-04
 * @brief   m68k multiplications and divisions.
 */

#ifndef M68K_MULDIV_H
#define M68K_MULDIV_H

#define muls(a,b)   m68k_muls((a),(b))
#define mulu(a,b)   m68k_mulu((a),(b))
#define divu(a,b)   m68k_divu((a),(b))
#define modu(a,b)   m68k_modu((a),(b))
#define mulu32(a,b) m68k_mulu32((a),(b))
#define divu32(a,b) m68k_divu32((a),(b))

static inline int32_t always_inline
m68k_muls(int16_t a, int16_t b)
{
  int32_t c;
  asm (
    "muls.w %1,%0\n\t"
    : "=d" (c)
    : "iSd" (a), "0" (b)
    : "cc"
    );
  return c;
}

static inline uint32_t always_inline
m68k_mulu(uint16_t a, uint16_t b)
{
  uint32_t c;
  asm (
    "mulu.w %1,%0\n\t"
    : "=d" (c)
    : "iSd" (a), "0" (b)
    : "cc"
    );
  return c;
}

static inline uint32_t always_inline
m68k_divu(uint32_t v, uint16_t d)
{
  asm(
    "divu.w %1,%0\n\t"
    : "+d" (v)
    : "iSd" (d)
    : "cc"
    );
  return (uint16_t) v;
}

static inline uint32_t always_inline
m68k_modu(uint32_t v, uint16_t d)
{
  asm(
    "divu.w %1,%0  \n\t"
    "clr.w %0      \n\t"
    "swap %0       \n\t"
    : "+d" (v)
    : "iSd" (d)
    : "cc"
    );
  return v;
}

static inline void always_inline
xdivu(uint32_t n, uint16_t d, zz_u16_t *q, zz_u16_t *r)
{
  union {
    uint32_t l;
    uint16_t w[2];
  } reg;
  reg.l = n;
  asm(
    "divu.w %[div],%[val]  \n\t"
    : [val] "+d" (reg.l)
    : [div] "idS" (d)
    : "cc"
    );
  *r = reg.w[0];
  *q = reg.w[1];
}

static inline uint32_t always_inline
m68k_mulu32(uint32_t a, uint16_t b)
{
  uint32_t tp1;

  asm(
    "move.l %[val],%[tp1]  \n\t" /* tp1 = XX:YY */
    "swap   %[tp1]         \n\t" /* tp1 = YY:XX */
    "mulu.w %[mul],%[tp1]  \n\t" /* tp1 = XX*ZZ */
    "mulu.w %[mul],%[val]  \n\t" /* val = YY*ZZ */
    "swap   %[tp1]         \n\t" /* tp1 = XX:00*ZZ */
    /* We could clear tp1 LSW but if it is set it means the
     * multiplication has already overflow, so it would not help that
     * much.
     *
     "clr.w %[tp1] \n\t"
    */
    "add.l  %[tp1],%[val]  \n\t" /* val = XX:YY*ZZ */
    : [val] "+d" (a), [tp1] "=d" (tp1)
    : [mul] "iSd" (b)
    : "cc"
    );
  return a;
}

static inline uint32_t always_inline
m68k_divu32(uint32_t v, uint16_t d)
{
  uint32_t tp1, tp2;

  asm(
    "move.l %[val],%[tp1]  \n\t" /* tp1 = Xx:Yy */
    "clr.w %[val]          \n\t" /* val = Xx:00 */
    "sub.l %[val],%[tp1]   \n\t" /* tp1 = 00:Yy */
    "swap %[val]           \n\t" /* val = 00:Xx */
    "divu.w %[div],%[val]  \n\t" /* val = Rx:Qx */
    "move.l %[val],%[tp2]  \n\t" /* tp2 = Rx:Qx */
    "clr.w %[tp2]          \n\t" /* tp2 = Rx:00 */
    "move.w %[tp1],%[tp2]  \n\t" /* tp2 = Rx:Yy */
    "divu.w %[div],%[tp2]  \n\t" /* tp2 = Ri:Qi */
    "swap %[val]           \n\t" /* val = Qx:Rx */
    "move.w %[tp2],%[val]  \n\t" /* val = Qx:Qi */
    : [val] "+d" (v), [tp1] "=d" (tp1), [tp2] "=d" (tp2)
    : [div] "iSd" (d)
    : "cc"
    );
  return v;
}

#endif /* M68K_MULDIV_H */
