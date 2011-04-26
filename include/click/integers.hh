// -*- c-basic-offset: 4; related-file-name: "../../lib/integers.cc" -*-
#ifndef CLICK_INTEGERS_HH
#define CLICK_INTEGERS_HH
#include <click/glue.hh>
#if !HAVE___BUILTIN_FFS && HAVE_FFS && HAVE_STRINGS_H
# include <strings.h>
#endif
CLICK_DECLS

/** @file <click/integers.hh>
 * @brief Functions for manipulating integers.
 */

#if HAVE_INT64_TYPES

/** @brief Return @a x translated from host to network byte order. */
inline uint64_t htonq(uint64_t x) {
    uint32_t hi = x >> 32;
    uint32_t lo = x & 0xffffffff;
    return (((uint64_t)htonl(lo)) << 32) | htonl(hi);
}

/** @brief Return @a x translated from network to host byte order. */
inline uint64_t ntohq(uint64_t x) {
    uint32_t hi = x >> 32;
    uint32_t lo = x & 0xffffffff;
    return (((uint64_t)ntohl(lo)) << 32) | ntohl(hi);
}

#endif

/** @brief Translate @a x to network byte order.
 *
 * Compare htons/htonl/htonq.  host_to_net_order is particularly useful in
 * template functions, where the type to be translated to network byte order
 * is unknown. */
inline unsigned char host_to_net_order(unsigned char x) {
    return x;
}
/** @overload */
inline signed char host_to_net_order(signed char x) {
    return x;
}
/** @overload */
inline char host_to_net_order(char x) {
    return x;
}
/** @overload */
inline short host_to_net_order(short x) {
    return htons(x);
}
/** @overload */
inline unsigned short host_to_net_order(unsigned short x) {
    return htons(x);
}
/** @overload */
inline int host_to_net_order(int x) {
    return htonl(x);
}
/** @overload */
inline unsigned host_to_net_order(unsigned x) {
    return htonl(x);
}
#if SIZEOF_LONG == 4
/** @overload */
inline long host_to_net_order(long x) {
    return htonl(x);
}
/** @overload */
inline unsigned long host_to_net_order(unsigned long x) {
    return htonl(x);
}
#endif
#if HAVE_INT64_TYPES
# if SIZEOF_LONG == 8
/** @overload */
inline long host_to_net_order(long x) {
    return htonq(x);
}
/** @overload */
inline unsigned long host_to_net_order(unsigned long x) {
    return htonq(x);
}
# endif
# if HAVE_LONG_LONG && SIZEOF_LONG_LONG == 8
/** @overload */
inline long long host_to_net_order(long long x) {
    return htonq(x);
}
/** @overload */
inline unsigned long long host_to_net_order(unsigned long long x) {
    return htonq(x);
}
# endif
# if !HAVE_INT64_IS_LONG && !HAVE_INT64_IS_LONG_LONG
/** @overload */
inline int64_t host_to_net_order(int64_t x) {
    return htonq(x);
}
/** @overload */
inline uint64_t host_to_net_order(uint64_t x) {
    return htonq(x);
}
# endif
#endif

/** @brief Translate @a x to host byte order.
 *
 * Compare ntohs/ntohl/ntohq.  net_to_host_order is particularly useful in
 * template functions, where the type to be translated to network byte order
 * is unknown. */
inline unsigned char net_to_host_order(unsigned char x) {
    return x;
}
/** @overload */
inline signed char net_to_host_order(signed char x) {
    return x;
}
/** @overload */
inline char net_to_host_order(char x) {
    return x;
}
/** @overload */
inline short net_to_host_order(short x) {
    return ntohs(x);
}
/** @overload */
inline unsigned short net_to_host_order(unsigned short x) {
    return ntohs(x);
}
/** @overload */
inline int net_to_host_order(int x) {
    return ntohl(x);
}
/** @overload */
inline unsigned net_to_host_order(unsigned x) {
    return ntohl(x);
}
#if SIZEOF_LONG == 4
/** @overload */
inline long net_to_host_order(long x) {
    return ntohl(x);
}
/** @overload */
inline unsigned long net_to_host_order(unsigned long x) {
    return ntohl(x);
}
#endif
#if HAVE_INT64_TYPES
# if SIZEOF_LONG == 8
/** @overload */
inline long net_to_host_order(long x) {
    return ntohq(x);
}
/** @overload */
inline unsigned long net_to_host_order(unsigned long x) {
    return ntohq(x);
}
# endif
# if HAVE_LONG_LONG && SIZEOF_LONG_LONG == 8
/** @overload */
inline long long net_to_host_order(long long x) {
    return ntohq(x);
}
/** @overload */
inline unsigned long long net_to_host_order(unsigned long long x) {
    return ntohq(x);
}
# endif
# if !HAVE_INT64_IS_LONG && !HAVE_INT64_IS_LONG_LONG
/** @overload */
inline int64_t net_to_host_order(int64_t x) {
    return ntohq(x);
}
/** @overload */
inline uint64_t net_to_host_order(uint64_t x) {
    return ntohq(x);
}
# endif
#endif


// MSB is bit #1
#if HAVE___BUILTIN_CLZ && !HAVE_NO_INTEGER_BUILTINS
/** @brief Return the index of the most significant bit set in @a x.
 * @return 0 if @a x = 0; otherwise the index of first bit set, where the
 * most significant bit is numbered 1.
 */
inline int ffs_msb(unsigned x) {
    return (x ? __builtin_clz(x) + 1 : 0);
}
#else
# define NEED_FFS_MSB_UNSIGNED 1
/** @overload */
int ffs_msb(unsigned x);
#endif

#if HAVE___BUILTIN_CLZL && !HAVE_NO_INTEGER_BUILTINS
/** @overload */
inline int ffs_msb(unsigned long x) {
    return (x ? __builtin_clzl(x) + 1 : 0);
}
#elif SIZEOF_INT == SIZEOF_LONG
/** @overload */
inline int ffs_msb(unsigned long x) {
    return ffs_msb(static_cast<unsigned>(x));
}
#else
# define NEED_FFS_MSB_UNSIGNED_LONG 1
/** @overload */
int ffs_msb(unsigned long x);
#endif

#if HAVE_LONG_LONG && HAVE___BUILTIN_CLZLL && !HAVE_NO_INTEGER_BUILTINS
/** @overload */
inline int ffs_msb(unsigned long long x) {
    return (x ? __builtin_clzll(x) + 1 : 0);
}
#elif HAVE_LONG_LONG && SIZEOF_LONG == SIZEOF_LONG_LONG
/** @overload */
inline int ffs_msb(unsigned long long x) {
    return ffs_msb(static_cast<unsigned long>(x));
}
#elif HAVE_LONG_LONG
# define NEED_FFS_MSB_UNSIGNED_LONG_LONG 1
/** @overload */
int ffs_msb(unsigned long long x);
#endif

#if HAVE_INT64_TYPES && !HAVE_INT64_IS_LONG && !HAVE_INT64_IS_LONG_LONG
# if SIZEOF_LONG >= 8
/** @overload */
inline int ffs_msb(uint64_t x) {
    return ffs_msb(static_cast<unsigned long>(x));
}
# elif HAVE_LONG_LONG && SIZEOF_LONG_LONG >= 8
/** @overload */
inline int ffs_msb(uint64_t x) {
    return ffs_msb(static_cast<unsigned long long>(x));
}
# else
#  define NEED_FFS_MSB_UINT64_T 1
/** @overload */
int ffs_msb(uint64_t x);
# endif
#endif


// LSB is bit #1
#if HAVE___BUILTIN_FFS && !HAVE_NO_INTEGER_BUILTINS
/** @brief Return the index of the least significant bit set in @a x.
 * @return 0 if @a x = 0; otherwise the index of first bit set, where the
 * least significant bit is numbered 1.
 */
inline int ffs_lsb(unsigned x) {
    return __builtin_ffs(x);
}
#elif HAVE_FFS && !HAVE_NO_INTEGER_BUILTINS
/** overload */
inline int ffs_lsb(unsigned x) {
    return ffs(x);
}
#else
# define NEED_FFS_LSB_UNSIGNED 1
/** @overload */
int ffs_lsb(unsigned x);
#endif

#if HAVE___BUILTIN_FFSL && !HAVE_NO_INTEGER_BUILTINS
/** @overload */
inline int ffs_lsb(unsigned long x) {
    return __builtin_ffsl(x);
}
#elif SIZEOF_INT == SIZEOF_LONG
/** @overload */
inline int ffs_lsb(unsigned long x) {
    return ffs_lsb(static_cast<unsigned>(x));
}
#else
# define NEED_FFS_LSB_UNSIGNED_LONG 1
/** @overload */
int ffs_lsb(unsigned long x);
#endif

#if HAVE_LONG_LONG && HAVE___BUILTIN_FFSLL && !HAVE_NO_INTEGER_BUILTINS
/** @overload */
inline int ffs_lsb(unsigned long long x) {
    return __builtin_ffsll(x);
}
#elif HAVE_LONG_LONG && SIZEOF_LONG == SIZEOF_LONG_LONG
/** @overload */
inline int ffs_lsb(unsigned long long x) {
    return ffs_lsb(static_cast<unsigned long>(x));
}
#elif HAVE_LONG_LONG
# define NEED_FFS_LSB_UNSIGNED_LONG_LONG 1
/** @overload */
int ffs_lsb(unsigned long long x);
#endif

#if HAVE_INT64_TYPES && !HAVE_INT64_IS_LONG && !HAVE_INT64_IS_LONG_LONG
# if SIZEOF_LONG >= 8
/** @overload */
inline int ffs_lsb(uint64_t x) {
    return ffs_lsb(static_cast<unsigned long>(x));
}
# elif HAVE_LONG_LONG && SIZEOF_LONG_LONG >= 8
/** @overload */
inline int ffs_lsb(uint64_t x) {
    return ffs_lsb(static_cast<unsigned long long>(x));
}
# else
#  define NEED_FFS_LSB_UINT64_T 1
/** @overload */
int ffs_lsb(uint64_t x);
# endif
#endif


/** @brief Return the integer approximation of @a x's square root.
 * @return The integer @a y where @a y*@a y <= @a x, but
 * (@a y+1)*(@a y+1) > @a x.
 */
uint32_t int_sqrt(uint32_t x);

#if HAVE_INT64_TYPES && HAVE_INT64_DIVIDE
/** @overload */
uint64_t int_sqrt(uint64_t x);
#endif

CLICK_ENDDECLS
#endif
