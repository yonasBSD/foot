#include "misc.h"

#include <wctype.h>

bool
isword(wchar_t wc, bool spaces_only, const wchar_t *delimiters)
{
    if (spaces_only)
        return iswgraph(wc);

    if (wcschr(delimiters, wc) != NULL)
        return false;

    return iswgraph(wc);
}

void
timespec_sub(const struct timespec *a, const struct timespec *b,
             struct timespec *res)
{
    res->tv_sec = a->tv_sec - b->tv_sec;
    res->tv_nsec = a->tv_nsec - b->tv_nsec;
    /* tv_nsec may be negative */
    if (res->tv_nsec < 0) {
        res->tv_sec--;
        res->tv_nsec += 1000 * 1000 * 1000;
    }
}
