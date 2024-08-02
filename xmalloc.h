#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>
#include <uchar.h>

#include "char32.h"
#include "macros.h"

void *xmalloc(size_t size) XMALLOC;
void *xcalloc(size_t nmemb, size_t size) XMALLOC;
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *str) XSTRDUP;
char *xstrndup(const char *str, size_t n) XSTRDUP;
char *xasprintf(const char *format, ...) PRINTF(1) XMALLOC;
char *xvasprintf(const char *format, va_list va) VPRINTF(1) XMALLOC;
char32_t *xc32dup(const char32_t *str) XSTRDUP;

static inline void *
xmemdup(const void *ptr, size_t size)
{
    return memcpy(xmalloc(size), ptr, size);
}

static inline char *
xstrjoin(const char *s1, const char *s2, char delim)
{
    size_t n1 = strlen(s1);
    size_t n2 = delim > 0 ? 1 : 0;
    size_t n3 = strlen(s2);

    char *joined = xmalloc(n1 + n2 + n3 + 1);
    memcpy(joined, s1, n1);
    if (delim > 0)
        joined[n1] = delim;
    memcpy(joined + n1 + n2, s2, n3 + 1);
    return joined;
}
