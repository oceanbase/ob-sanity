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

#ifndef _SANITY_H_
#define _SANITY_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

extern "C"
{
  void memory_sanity_abort();
}

extern int64_t sanity_min_addr;
extern int64_t sanity_max_addr;

class SanityDisableCheckRangeGuard
{
public:
  static bool &tl_check()
  {
    static __thread bool check = true;
    return check;
  }
  SanityDisableCheckRangeGuard()
    : bak_(tl_check())
  {
    tl_check() = false;
  }
  ~SanityDisableCheckRangeGuard()
  {
    tl_check() = bak_;
  }
private:
  const bool bak_;
};

static inline uint64_t sanity_align_down(const void *x, uint64_t align)
{
  return (uint64_t)x & ~(align - 1);
}

static inline uint64_t sanity_align_up(const void *x, uint64_t align)
{
  return ((uint64_t)x + (align - 1)) & ~(align - 1);
}

static inline bool sanity_addr_in_range(const void *ptr, int64_t size)
{
  return (int64_t)ptr + size < sanity_max_addr && (int64_t)ptr >= sanity_min_addr;
}

static inline void* sanity_to_shadow(const void *ptr)
{
  return (void*)((uint64_t)ptr >> 3);
}

static inline int64_t sanity_to_shadow_size(int64_t size)
{
  return size >> 3;
}

static inline void sanity_poison(const void *ptr, ssize_t len)
{
  if (!sanity_addr_in_range(ptr, len)) return;
  const void *orig_ptr = ptr;
  ptr = (void*)sanity_align_up(ptr, 8);
  len -= (uint64_t)ptr - (uint64_t)orig_ptr;
  if (len <= 0) return;
  int8_t *shadow = (int8_t*)sanity_to_shadow(ptr);
  int32_t n_bytes = static_cast<int32_t>(sanity_to_shadow_size(len));
  if (n_bytes > 0) {
    static void *(*real_memset)(void *, int, size_t)
      = (__typeof__(real_memset)) dlsym(RTLD_NEXT, "memset");
    real_memset(shadow, 0xF0, n_bytes);
  }
  int8_t n_bits =  len & 0x7;
  if (n_bits > 0) {
    shadow[n_bytes] = 0xF0;
  }
}

static inline void sanity_unpoison(const void *ptr, ssize_t len)
{
  if (!sanity_addr_in_range(ptr, len)) return;
  const void *orig_ptr = ptr;
  ptr = (void*)sanity_align_up(ptr, 8);
  len -= (uint64_t)ptr - (uint64_t)orig_ptr;
  if (len <= 0) return;
  int8_t *shadow = (int8_t*)sanity_to_shadow(ptr);
  int32_t n_bytes = static_cast<int32_t>(sanity_to_shadow_size(len));
  if (n_bytes > 0) {
    static void *(*real_memset)(void *, int, size_t)
      = (__typeof__(real_memset)) dlsym(RTLD_NEXT, "memset");
    real_memset(shadow, 0x0, n_bytes);
  }
  int8_t n_bits =  len & 0x7;
  if (n_bits > 0) {
    shadow[n_bytes] = n_bits;
  }
}

static inline void sanity_check_range(const void *ptr, ssize_t len)
{
  if (!SanityDisableCheckRangeGuard::tl_check()) return;
  if (len <= 0) return;
  if (!sanity_addr_in_range(ptr, len)) return;
  char *start = (char*)ptr;
  char *end = start + len;
  if (end <= start) return memory_sanity_abort();
  char *start_align = (char*)sanity_align_up(start, 8);
  char *end_align = (char*)sanity_align_down(end, 8);
  if (start_align > start &&
      (*(int8_t*)sanity_to_shadow(start_align - 8) != 0x0 &&
       *(int8_t*)sanity_to_shadow(start_align - 8) < (len + start - (start_align - 8)))) {
    return memory_sanity_abort();
  }
  if (end_align >= start_align + 8) {
    if (*(int8_t*)sanity_to_shadow(start_align) != 0x0) {
      return memory_sanity_abort();
    }
    if (end_align > start_align + 8) {
      static void *(*real_memcmp)(const void *, const void *, size_t)
        = (__typeof__(real_memcmp)) dlsym(RTLD_NEXT, "memcmp");
      if (real_memcmp(sanity_to_shadow(start_align), sanity_to_shadow(start_align + 8),
                      sanity_to_shadow_size(end_align - start_align - 8)) != 0) {
        return memory_sanity_abort();
      }
    }
  }
  if (end_align < end &&
      (*(int8_t*)sanity_to_shadow(end_align) != 0x0 &&
       *(int8_t*)sanity_to_shadow(end_align) < (end - end_align))) {
    return memory_sanity_abort();
  }
}

#endif /* _SANITY_H_ */
