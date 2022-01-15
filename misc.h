#pragma once

#include <stdbool.h>
#include <wchar.h>
#include <time.h>

bool isword(wchar_t wc, bool spaces_only, const wchar_t *delimiters);

void timespec_sub(const struct timespec *a, const struct timespec *b, struct timespec *res);
