/* #include(LICENSE) */
#ifndef _COMMON_H
#define _COMMON_H

#ifdef __GNUC__
#define deprecated __attribute__ ((deprecated))
#else
#define deprecated
#endif

#define xmalloc malloc
#define xstrdup strdup
#define xrealloc realloc
#define xfree free

#define D(x,y...) fwprintf(stderr, L"%s:%d %s() " x "\n", \
	__FILE__, __LINE__, __func__, ## y)

#endif /* _COMMON_H */
