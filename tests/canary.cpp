// Deliberately broken: proves the build tree is really instrumented before a green ctest is trusted.
// Not registered with ctest — it is run on its own and is expected to die with a sanitizer report.
#include <cstdio>

#if defined(CORE_CANARY_ASAN)

int main() {
  volatile int* freed = new int[4];
  delete[] freed;
  std::printf("%d\n", freed[3]);
  return 0;
}

#elif defined(CORE_CANARY_UBSAN)

#include <climits>

int main() {
  volatile int max = INT_MAX;
  const int overflowed = max + 1;
  std::printf("%d\n", overflowed);
  return 0;
}

#elif defined(CORE_CANARY_TSAN)

#include <atomic>
#include <thread>

static std::atomic<bool> start{false};
static int shared = 0;

int main() {
  // Many accesses on both sides, released together: a single write pair was missed by TSan in 3 of
  // 5 runs, which made the canary useless as a proof of instrumentation.
  std::thread worker([] {
    while (!start.load(std::memory_order_acquire)) {}
    for (int i = 0; i < 200000; ++i) shared = i;
  });
  start.store(true, std::memory_order_release);
  for (int i = 0; i < 200000; ++i) shared = i + 1;
  worker.join();
  std::printf("%d\n", shared);
  return 0;
}

#else
#error "canary.cpp: define CORE_CANARY_ASAN, CORE_CANARY_UBSAN or CORE_CANARY_TSAN"
#endif
