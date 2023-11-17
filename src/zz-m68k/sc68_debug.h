/**
 * @file    sc68_debug.h
 * @author  Benjamin Gerard AKA Ben^OVR
 * @date    2017-09-09
 * @brief   sc68 debugging facilities.
 */

#ifndef SC68_DEBUG_H
#define SC68_DEBUG_H

#define ILLEGAL asm volatile("illegal\n\t")

#define _STRINGY(X) #X
#define _ASSERTY(X,F,L) X " in " F  ":" _STRINGY(L)

/* Break-on-debug break-point */
#define _BREAKP					\
  asm volatile(					\
    "clr.l   -(%a7)      \n\t"			\
    "pea     0x5C68DB61  \n\t"			\
    "trap    #0          \n\t"			\
    "addq.w  #8,%a7      \n\t"			\
    )						\

/* Break-on-debug break-point with message. */
#define _BRKMSG(M)				\
  do {						\
    const char * str = (M);			\
    asm volatile (				\
      "move.l  %0,-(%%a7)  \n\t"		\
      "pea     0x5C68DB61  \n\t"		\
      "trap    #0          \n\t"		\
      "addq.w  #8,%%a7     \n\t"		\
      :						\
      : "m" (str)				\
      : "cc");					\
  } while (0)

/* Print a debug message. */
#define _DBGMSG(M)				\
  do {						\
    const char * str = (M);			\
    asm volatile (				\
      "move.l  %0,-(%%a7)  \n\t"		\
      "pea     0x5C68DB60  \n\t"		\
      "trap    #0          \n\t"		\
      "addq.w  #8,%%a7     \n\t"		\
      :						\
      : "m" (str)				\
      : "cc");					\
  } while (0)

/* Break-on-debug-only if condition is false. */
#define _zz_assert(C) if (! (C) ) {				\
    static char str[] = _ASSERTY(#C, __BASE_FILE__, __LINE__);	\
    asm volatile(						\
      "pea     %0          \n\t"				\
      "pea     0x5C68DB63  \n\t"				\
      "trap    #0          \n\t"				\
      "addq.w  #8,%%a7     \n\t"				\
      : : "m" ( str ) );					\
  }								\
  else

#ifdef NDEBUG

#define BREAKP	  zz_void
#define BRKMSG(M) zz_void
#define ASSERT(C) zz_void
#define DBGMSG(M) zz_void
#define zz_assert(C) zz_void

#else

#define BREAKP	  _BREAKP
#define BRKMSG(M) _BRKMSG(M)
#define ASSERT(C) _ASSERT(C)
#define DBGMSG(M) _DBGMSG(M)
#define zz_assert(C) _zz_assert(C)

#endif /* NDEBUG */

#endif /* SC68_DEBUG_H */
