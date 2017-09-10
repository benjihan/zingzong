/**
 * @file   sc68_debug.h
 * @brief  Debugging facilities.
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-09-09
 */

#ifndef SC68_DEBUG_H
#define SC68_DEBUG_H

#ifdef NDEBUG

#define BREAKP    zz_void
#define BRKMSG(M) zz_void
#define ASSERT(C) zz_void
#define DBGMSG(M) zz_void
#define zz_assert(C) zz_void

#else

#define _STRINGY(X) #X
#define _ASSERTY(X,F,L) X " in " F  ":" _STRINGY(L)

#define BREAKP                                               \
  asm volatile(                                              \
    "peal 0\n\t"                                             \
    "peal 0x5C68DB61\n\t"                                    \
    "trap #0\n\t"                                            \
    "addqw #8,%sp")

#define BRKMSG(M)                                             \
  asm volatile(                                               \
    "peal %0\n\t"                                             \
    "peal 0x5C68DB61\n\t"                                     \
    "trap #0\n\t"                                             \
    "addqw #8,%%sp" : : "m" (M) )

#define DBGMSG(M)                                             \
  asm volatile(                                               \
    "peal %0\n\t"                                             \
    "peal 0x5C68DB60\n\t"                                     \
    "trap #0\n\t"                                             \
    "addqw #8,%%sp" : : "m" (M) )

#define zz_assert(C) if (! (C) ) {                                      \
    static char str[] = _ASSERTY(#C, __BASE_FILE__, __LINE__);          \
    asm volatile(                                                       \
      "peal %0\n\t"                                                     \
      "peal 0x5C68DB63\n\t"                                             \
      "trap #0\n\t"                                                     \
      "addqw #8,%%sp"                                                   \
      : : "m" ( str ) ); } else

#endif /* NDEBUG */

#endif /* SC68_DEBUG_H */
