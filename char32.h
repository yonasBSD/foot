#pragma once

#include <stdbool.h>
#include <uchar.h>
#include <stddef.h>
#include <stdarg.h>

size_t c32len(const char32_t *s);
int c32cmp(const char32_t *s1, const char32_t *s2);

char32_t *c32ncpy(char32_t *dst, const char32_t *src, size_t n);
char32_t *c32cpy(char32_t *dst, const char32_t *src);
char32_t *c32ncat(char32_t *dst, const char32_t *src, size_t n);
char32_t *c32cat(char32_t *dst, const char32_t *src);
char32_t *c32dup(const char32_t *s);

char32_t *c32chr(const char32_t *s, char32_t c);

int c32casecmp(const char32_t *s1, const char32_t *s2);
int c32ncasecmp(const char32_t *s1, const char32_t *s2, size_t n);

size_t mbsntoc32(char32_t *dst, const char *src, size_t nms, size_t len);
size_t mbstoc32(char32_t *dst, const char *src, size_t len);
char32_t *ambstoc32(const char *src);
char *ac32tombs(const char32_t *src);

char32_t toc32lower(char32_t c);
char32_t toc32upper(char32_t c);

bool isc32space(char32_t c32);
bool isc32print(char32_t c32);
bool isc32graph(char32_t c32);

int c32width(char32_t c);
int c32swidth(const char32_t *s, size_t n);
