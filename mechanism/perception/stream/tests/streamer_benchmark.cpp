#include <benchmark/benchmark.h>

#include <cstdint>
#include <cstring>
#include <vector>

// Define our known payload sizes
constexpr size_t NV12_360P_BYTES = 640 * 360 * 1.5;
constexpr size_t DEPTH_480P_BYTES = 640 * 480 * 2;

// Benchmark the exact memory copy happening in your RGB streamer
static void BM_MemCopy_NV12_360p(benchmark::State& state) {
  std::vector<uint8_t> src(NV12_360P_BYTES, 255);
  std::vector<uint8_t> dst(NV12_360P_BYTES, 0);

  for (auto _ : state) {
    std::memcpy(dst.data(), src.data(), NV12_360P_BYTES);
    // Force the compiler to not optimize away the copy
    benchmark::DoNotOptimize(dst.data());
    benchmark::ClobberMemory();
  }
  // Tell benchmark how many bytes we moved to calculate MB/s
  state.SetBytesProcessed(int64_t(state.iterations()) *
                          int64_t(NV12_360P_BYTES));
}
BENCHMARK(BM_MemCopy_NV12_360p);

static void BM_MemCopy_Depth_480p(benchmark::State& state) {
  std::vector<uint8_t> src(DEPTH_480P_BYTES, 128);
  std::vector<uint8_t> dst(DEPTH_480P_BYTES, 0);

  for (auto _ : state) {
    std::memcpy(dst.data(), src.data(), DEPTH_480P_BYTES);
    benchmark::DoNotOptimize(dst.data());
    benchmark::ClobberMemory();
  }
  state.SetBytesProcessed(int64_t(state.iterations()) *
                          int64_t(DEPTH_480P_BYTES));
}
BENCHMARK(BM_MemCopy_Depth_480p);

// You can add ZMQ send/recv benchmarks here as well

BENCHMARK_MAIN();
