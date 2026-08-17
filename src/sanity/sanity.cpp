/*
 * Copyright (c) 2026 OceanBase.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "sanity/sanity.h"
#include <stdarg.h>

int64_t sanity_min_addr = 0;
int64_t sanity_max_addr = 0;

#define LIKELY(x) __builtin_expect(!!(x),!!1)

template <class Tp>
inline void DoNotOptimize(Tp const& value)
{
  asm volatile("" : : "g"(value) : "memory");
}

extern "C"
{
void *memset(void *s, int c, size_t n)
{
  static void *(*real_func)(void *, int, size_t)
    = (__typeof__(real_func)) dlsym(RTLD_NEXT, "memset");
  sanity_check_range(s, n);
  return real_func(s, c, n);
}

void bzero(void *s, size_t n)
{
  static void (*real_func)(void *, size_t)
    = (__typeof__(real_func)) dlsym(RTLD_NEXT, "bzero");
  sanity_check_range(s, n);
  return real_func(s, n);
}

void *memcpy(void *dest, const void *src, size_t n)
{
  static void *(*real_func)(void *, const void *, size_t)
    = (__typeof__(real_func)) dlsym(RTLD_NEXT, "memcpy");
  sanity_check_range(dest, n);
  sanity_check_range(src, n);
  return real_func(dest, src, n);
}

void *memmove(void *dest, const void *src, size_t n)
{
  static void *(*real_func)(void *, const void *, size_t)
    = (__typeof__(real_func)) dlsym(RTLD_NEXT, "memmove");
  sanity_check_range(dest, n);
  sanity_check_range(src, n);
  return real_func(dest, src, n);
}

int memcmp(const void *s1, const void *s2, size_t n)
{
  static int (*real_func)(const void *, const void *, size_t)
    = (__typeof__(real_func)) dlsym(RTLD_NEXT, "memcmp");
  sanity_check_range(s1, n);
  sanity_check_range(s2, n);
  return real_func(s1, s2, n);
}

size_t strlen(const char *s)
{
  static size_t (*real_func)(const char *)
    = (__typeof__(real_func)) dlsym(RTLD_NEXT, "strlen");
  size_t len = real_func(s);
  sanity_check_range(s, len + 1); // include the terminating null byte
  return len;
}

size_t strnlen(const char *s, size_t maxlen)
{
  static size_t (*real_func)(const char *, size_t)
    = (__typeof__(real_func)) dlsym(RTLD_NEXT, "strnlen");
  size_t len = real_func(s, maxlen);
  sanity_check_range(s, len);
  return len;
}

char *strcpy(char *dest, const char *src)
{
  static char *(*real_func)(char *, const char *)
    = (__typeof__(real_func)) dlsym(RTLD_NEXT, "strcpy");
  sanity_check_range(dest, strlen(src) + 1); // invoke strlen directly, utilize checker within strlen
  return real_func(dest, src);
}

char *strncpy(char *dest, const char *src, size_t n)
{
  static char *(*real_func)(char *, const char *, size_t)
    = (__typeof__(real_func)) dlsym(RTLD_NEXT, "strncpy");
  sanity_check_range(dest, strnlen(src, n));
  return real_func(dest, src, n);
}

int strcmp(const char *s1, const char *s2)
{
  static int (*real_func)(const char *, const char *)
    = (__typeof__(real_func)) dlsym(RTLD_NEXT, "strcmp");
  DoNotOptimize(strlen(s1));
  DoNotOptimize(strlen(s2));
  return real_func(s1, s2);
}

int strncmp(const char *s1, const char *s2, size_t n)
{
  static int (*real_func)(const char *, const char *, size_t)
    = (__typeof__(real_func)) dlsym(RTLD_NEXT, "strncmp");
  DoNotOptimize(strnlen(s1, n));
  DoNotOptimize(strnlen(s2, n));
  return real_func(s1, s2, n);
}

int strcasecmp(const char *s1, const char *s2)
{
  static int (*real_func)(const char *, const char *)
    = (__typeof__(real_func)) dlsym(RTLD_NEXT, "strcasecmp");
  DoNotOptimize(strlen(s1));
  DoNotOptimize(strlen(s2));
  return real_func(s1, s2);
}

int strncasecmp(const char *s1, const char *s2, size_t n)
{
  static int (*real_func)(const char *, const char *, size_t)
    = (__typeof__(real_func)) dlsym(RTLD_NEXT, "strncasecmp");
  DoNotOptimize(strnlen(s1, n));
  DoNotOptimize(strnlen(s2, n));
  return real_func(s1, s2, n);
}

int vsprintf(char *str, const char *format, va_list ap)
{
  static int (*real_func)(char *, const char *, va_list)
    = (__typeof__(real_func)) dlsym(RTLD_NEXT, "vsprintf");
  int n = real_func(str, format, ap);
  sanity_check_range(str, n + 1);
  return n;
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
  static int (*real_func)(char *, size_t, const char *, va_list)
    = (__typeof__(real_func)) dlsym(RTLD_NEXT, "vsnprintf");
  int n = real_func(str, size, format, ap);
  sanity_check_range(str, LIKELY((n + 1) < size) ? (n + 1) : size);
  return n;
}

int sprintf(char *str, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  int n = vsprintf(str, format, ap);
  va_end(ap);
  return n;
}

int snprintf(char *str, size_t size, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  int n = vsnprintf(str, size, format, ap);
  va_end(ap);
  return n;
}
}
