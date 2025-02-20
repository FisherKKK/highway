// Copyright 2025 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "hwy/contrib/thread_pool/spin.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <atomic>

#include "hwy/aligned_allocator.h"          // HWY_ALIGNMENT
#include "hwy/contrib/thread_pool/futex.h"  // NanoSleep
#include "hwy/contrib/thread_pool/thread_pool.h"
#include "hwy/contrib/thread_pool/topology.h"
#include "hwy/tests/hwy_gtest.h"
#include "hwy/tests/test_util-inl.h"
#include "hwy/timer.h"

namespace hwy {
namespace {

// Simple mutex.
TEST(SpinTest, TestPingPong) {
  if (!HaveThreadingSupport()) {
    HWY_WARN("Threads not supported, skipping test\n");
    return;
  }

  const SpinMode mode = DetectSpinMode();
  fprintf(stderr, "Spin mode: %s\n", ToString(mode));

  alignas(HWY_ALIGNMENT) std::atomic<uint32_t> thread_active[HWY_ALIGNMENT / 4];
  alignas(HWY_ALIGNMENT) std::atomic<uint32_t> thread_done[HWY_ALIGNMENT / 4];

  thread_active[0].store(0, std::memory_order_release);
  thread_done[0].store(0, std::memory_order_release);
  hwy::ThreadPool pool(1);
  HWY_ASSERT(pool.NumWorkers() == 2);

  const double t0 = hwy::platform::Now();
  std::atomic_flag error = ATOMIC_FLAG_INIT;

  std::atomic<size_t> reps1;
  std::atomic<size_t> reps2;

  pool.Run(0, 2, [&](uint64_t task, size_t thread) {
    HWY_ASSERT(task == thread);
    if (task == 0) {  // new thread
      size_t my_reps = 0;
      (void)SpinUntilDifferent(mode, 0, thread_active[0], my_reps);
      reps1.store(my_reps);
      if (!NanoSleep(20 * 1000 * 1000)) {
        error.test_and_set();
      }
      thread_done[0].store(1, std::memory_order_release);
    } else {  // main thread
      if (!NanoSleep(30 * 1000 * 1000)) {
        error.test_and_set();
      }
      // Release the thread.
      thread_active[0].store(1, std::memory_order_release);
      // Wait for it to finish.
      size_t my_reps = 0;
      (void)SpinUntilDifferent(mode, 0, thread_done[0], my_reps);
      reps2.store(my_reps);
    }
  });

  const double t1 = hwy::platform::Now();
  const double elapsed = t1 - t0;
  fprintf(stderr, "Elapsed time: %f us; reps1=%zu, reps2=%zu\n", elapsed * 1E6,
          reps1.load(), reps2.load());
  // Unless NanoSleep failed to sleep, this should take 50ms+epsilon.
  HWY_ASSERT(error.test_and_set() || elapsed > 25E-3);
}

}  // namespace
}  // namespace hwy

HWY_TEST_MAIN();
