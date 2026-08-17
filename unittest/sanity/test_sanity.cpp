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

#include <gtest/gtest.h>
#include <sys/mman.h>
#include "sanity/sanity.h"

int n_aborts = 0;

extern "C"
{
void memory_sanity_abort()
{
  n_aborts++;
}
}

int64_t global_addr = 0;

int64_t align_up(int64_t v, int align)
{
  return (v + align - 1) & ~(align - 1);
}

void *alloc(int64_t size)
{
  int64_t all_size = align_up(size, 2<<20);
  int64_t addr = __sync_add_and_fetch(&global_addr, all_size);
  void *ptr = mmap((void*)addr, all_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  if (MAP_FAILED == ptr) return NULL;
  void *shadow_ptr = mmap((void*)sanity_to_shadow(ptr), sanity_to_shadow_size(all_size), PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  if (MAP_FAILED == shadow_ptr) {
    munmap(ptr, all_size);
    return NULL;
  }
  sanity_unpoison(ptr, size);
  char *left_ptr = (char*)align_up((int64_t)((char*)ptr + size), 8);
  sanity_poison(left_ptr, (char*)ptr + all_size - left_ptr);
  return ptr;
}

void free(void *ptr, int64_t size)
{
  //int64_t all_size = align_up(size, 2<<20);
  //munmap(ptr, all_size);
  //munmap((void*)sanity_to_shadow(ptr), sanity_to_shadow_size(all_size));
  sanity_poison(ptr, size);
}

class TestSanity : public ::testing::Test {
protected:
  static void SetUpTestCase()
  {
    sanity_min_addr = 0x400000000000;
    sanity_max_addr = 0x500000000000;
    global_addr = sanity_min_addr;
  }
  static void TearDownTestCase() {}
  void SetUp() override
  {
    n_aborts = 0;
  }
  void TearDown() override {}
};

TEST_F(TestSanity, use_after_free)
{
  int64_t size = 1;
  char *ptr = (char*)alloc(size);
  ASSERT_NE(ptr, MAP_FAILED);
  ASSERT_TRUE(sanity_addr_in_range(ptr, size));
  *ptr = 'a';
  ASSERT_EQ(n_aborts, 0);
  {
    auto v = *ptr;
    ASSERT_EQ(n_aborts, 0);
  }
  free(ptr, size);
  ASSERT_EQ(n_aborts, 0);
  *ptr = 'a';
  ASSERT_EQ(n_aborts, 1);
  *ptr = 'a';
  ASSERT_EQ(n_aborts, 2);
  {
    auto v = *ptr;
    ASSERT_EQ(n_aborts, 3);
  }
}

TEST_F(TestSanity, out_of_bounds)
{
  int64_t size = 1;
  char *ptr = (char*)alloc(size);
  ASSERT_NE(ptr, MAP_FAILED);
  ASSERT_TRUE(sanity_addr_in_range(ptr, size));
  *ptr = 'a';
  ASSERT_EQ(n_aborts, 0);
  *(ptr + 1) = 'a';
  ASSERT_EQ(n_aborts, 1);
  auto v = *(ptr + 1);
  ASSERT_EQ(n_aborts, 2);
}

TEST_F(TestSanity, sanity_check_range)
{
  int64_t size = 15;
  char *ptr = (char*)alloc(size);
  ASSERT_NE(ptr, MAP_FAILED);
  sanity_check_range(ptr, size);
  ASSERT_EQ(n_aborts, 0);
  sanity_check_range(ptr, size + 1);
  ASSERT_EQ(n_aborts, 1);
  free(ptr, size);
  ASSERT_EQ(n_aborts, 1);
  sanity_check_range(ptr, size);
  ASSERT_EQ(n_aborts, 2);
}

TEST_F(TestSanity, border)
{
  char *ptr = (char*)alloc(sizeof(int) + 1);
  ASSERT_EQ(n_aborts, 0);
  *(int*)ptr = 0;
  ASSERT_EQ(n_aborts, 0);
  *(int*)((char*)ptr + 1) = 0;
  ASSERT_EQ(n_aborts, 0);
  *(int*)((char*)ptr + 2) = 0;
  ASSERT_EQ(n_aborts, 1);
  auto v = *(int*)((char*)ptr + 2);
  ASSERT_EQ(n_aborts, 2);
}

TEST_F(TestSanity, false_negative)
{
  char *ptr = (char*)alloc(sizeof(int64_t));
  *(int64_t*)((char*)ptr) = 0;
  ASSERT_EQ(n_aborts, 0);
  *(int64_t*)((char*)ptr + 1) = 0;
  ASSERT_EQ(n_aborts, 0);
}

TEST_F(TestSanity, poison_and_unpoison)
{
  char *ptr = (char*)alloc(16);
  uint8_t *shadow_a = (uint8_t*)sanity_to_shadow(ptr);
  uint8_t *shadow_b = shadow_a + 1;
  ASSERT_EQ(0, *shadow_a);
  ASSERT_EQ(0, *shadow_b);
  sanity_poison(ptr, 16);
  ASSERT_EQ(0xF0, *shadow_a);
  ASSERT_EQ(0xF0, *shadow_b);
  sanity_unpoison(ptr + 1, 8);
  ASSERT_EQ(0xF0, *shadow_a);
  ASSERT_EQ(0x1, *shadow_b);
  sanity_poison(ptr, 10);
  ASSERT_EQ(0xF0, *shadow_a);
  ASSERT_EQ(0xF0, *shadow_b);
  sanity_unpoison(ptr, 8);
  ASSERT_EQ(0, *shadow_a);
  sanity_unpoison(ptr + 8, 1);
  ASSERT_EQ(1, *shadow_b);
  sanity_unpoison(ptr + 1, 2);
  ASSERT_EQ(0, *shadow_a);
  sanity_unpoison(ptr, 0);
  ASSERT_EQ(0, *shadow_a);
  sanity_unpoison(ptr, -1);
  ASSERT_EQ(0, *shadow_a);
  sanity_poison(ptr, 1);
  ASSERT_EQ(0xF0, *shadow_a);
  sanity_poison(ptr, 0);
  ASSERT_EQ(0xF0, *shadow_a);
  sanity_poison(ptr, -1);
  ASSERT_EQ(0xF0, *shadow_a);
}

int main(int argc, char **argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
