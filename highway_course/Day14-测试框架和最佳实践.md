# Day 14: 测试框架和最佳实践

## 课程目标

这是课程的最后一天！我们将学习 Highway 的测试框架、调试 SIMD 代码的技巧，以及使用 Highway 的最佳实践。

---

## 14.1 Highway 测试框架

### 基本测试结构

```cpp
#include "hwy/highway.h"
#include "hwy/tests/test_util.h"

namespace hwy {
namespace HWY_NAMESPACE {

// 测试结构体
struct TestMyOperation {
  // 模板函数：对每种类型和向量大小测试
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);

    // 分配测试数据
    AlignedUniquePtr<T[]> input = AllocateAligned<T>(N);
    AlignedUniquePtr<T[]> output = AllocateAligned<T>(N);

    // 初始化数据
    for (size_t i = 0; i < N; ++i) {
      input[i] = static_cast<T>(i + 1);
      output[i] = 0;
    }

    // 测试操作
    auto v_input = Load(d, input.get());
    auto v_output = MyOperation(d, v_input);
    Store(v_output, d, output.get());

    // 验证结果
    for (size_t i = 0; i < N; ++i) {
      T expected = ComputeExpected<T>(i + 1);
      HWY_ASSERT_EQ(expected, output[i]);
    }
  }
};

// 测试所有类型和向量大小
HWY_NOINLINE void TestAllMyOperation() {
  // ForAllTypes：测试所有支持的数据类型
  // ForPartialVectors：测试所有向量大小（包括部分大小）
  ForAllTypes(ForPartialVectors<TestMyOperation>());
}

}  // namespace HWY_NAMESPACE
}  // namespace hwy

// 导出测试
#if HWY_ONCE
namespace hwy {
HWY_BEFORE_TEST(MyOperationTest);
HWY_EXPORT_AND_TEST_P(MyOperationTest, TestAllMyOperation);
HWY_AFTER_TEST();
}  // namespace hwy
#endif
```

### 测试工具函数

```cpp
// test_util.h

// 随机数生成器（Xorshift128+）
class RandomState {
 public:
  explicit RandomState(uint64_t s = 0x123456789ABCDEF) : s0_(s), s1_(s) {}

  uint32_t Rand32() {
    uint64_t s0 = s0_;
    uint64_t s1 = s1_;
    uint64_t result = s0 + s1;

    s1 ^= s0;
    s0 = RotateLeft(s0, 23);
    s0 = s0 + s1;
    s1 = RotateLeft(s1, 17);

    s0_ = s0;
    s1_ = s1;

    return static_cast<uint32_t>(result);
  }

 private:
  uint64_t s0_, s1_;
};

// 填充随机数组
template <typename T>
void RandomFill(RandomState& rng, T* data, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    data[i] = static_cast<T>(rng.Rand32());
  }
}
```

---

## 14.2 编写单元测试

### 简单操作测试

```cpp
// 测试 Add 操作
struct TestAdd {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);

    AlignedUniquePtr<T[]> a = AllocateAligned<T>(N);
    AlignedUniquePtr<T[]> b = AllocateAligned<T>(N);
    AlignedUniquePtr<T[]> expected = AllocateAligned<T>(N);

    RandomState rng;
    for (size_t i = 0; i < N; ++i) {
      a[i] = static_cast<T>(rng.Rand32());
      b[i] = static_cast<T>(rng.Rand32());
      expected[i] = AddWrapAround(a[i], b[i]);
    }

    auto v_a = Load(d, a.get());
    auto v_b = Load(d, b.get());
    auto v_sum = Add(v_a, v_b);

    AlignedUniquePtr<T[]> result = AllocateAligned<T>(N);
    Store(v_sum, d, result.get());

    HWY_ASSERT_VEC_EQ(d, expected.get(), result.get());
  }
};

HWY_NOINLINE void TestAllAdd() {
  ForAllTypes(ForPartialVectors<TestAdd>());
}

#if HWY_ONCE
namespace hwy {
HWY_BEFORE_TEST(AddTest);
HWY_EXPORT_AND_TEST_P(AddTest, TestAllAdd);
HWY_AFTER_TEST();
}
#endif
```

### 比较操作测试

```cpp
// 测试 LT（小于）操作
struct TestLessThan {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);

    AlignedUniquePtr<T[]> a = AllocateAligned<T>(N);
    AlignedUniquePtr<T[]> b = AllocateAligned<T>(N);

    RandomState rng;
    for (size_t i = 0; i < N; ++i) {
      a[i] = static_cast<T>(rng.Rand32());
      b[i] = static_cast<T>(rng.Rand32());
    }

    auto v_a = Load(d, a.get());
    auto v_b = Load(d, b.get());
    auto v_mask = LT(v_a, v_b);

    // 验证掩码
    for (size_t i = 0; i < N; ++i) {
      bool expected = a[i] < b[i];
      bool actual = GetLane(v_mask, i) != 0;
      HWY_ASSERT_EQ(expected, actual);
    }
  }
};

HWY_NOINLINE void TestAllLessThan() {
  ForSignedTypes(ForPartialVectors<TestLessThan>());
  ForUnsignedTypes(ForPartialVectors<TestLessThan>());
  ForFloatTypes(ForPartialVectors<TestLessThan>());
}

#if HWY_ONCE
namespace hwy {
HWY_BEFORE_TEST(LessThanTest);
HWY_EXPORT_AND_TEST_P(LessThanTest, TestAllLessThan);
HWY_AFTER_TEST();
}
#endif
```

### 边界情况测试

```cpp
// 测试极端值和边界条件
struct TestEdgeCases {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);

    // 测试零值
    {
      auto v_zero = Zero(d);
      HWY_ASSERT_VEC_EQ(d, v_zero, v_zero);
    }

    // 测试全一值
    {
      auto v_one = AllTrue(d);
      HWY_ASSERT(AllTrue(d, v_one));
    }

    // 测试最小/最大值
    {
      auto v_min = Set(d, std::numeric_limits<T>::min());
      auto v_max = Set(d, std::numeric_limits<T>::max());

      auto v_result = Min(v_min, v_max);
      HWY_ASSERT_VEC_EQ(d, v_result, v_min);
    }

    // 测试溢出（整数）
    if (!IsFloat<T>()) {
      auto v_max = Set(d, std::numeric_limits<T>::max());
      auto v_overflow = Add(v_max, v_max);
      // 溢出行为取决于平台
      // 可以添加平台特定检查
    }
  }
};

HWY_NOINLINE void TestAllEdgeCases() {
  ForAllTypes(ForPartialVectors<TestEdgeCases>());
}

#if HWY_ONCE
namespace hwy {
HWY_BEFORE_TEST(EdgeCasesTest);
HWY_EXPORT_AND_TEST_P(EdgeCasesTest, TestAllEdgeCases);
HWY_AFTER_TEST();
}
#endif
```

---

## 14.3 调试 SIMD 代码

### 打印向量值

```cpp
// 辅助函数：打印向量内容
template <typename T, typename D>
void PrintVector(D d, Vec<Simd<T, /*...>>> v, const char* name) {
  const size_t N = Lanes(d);

  printf("%s [", name);

  AlignedUniquePtr<T[]> buffer = AllocateAligned<T>(N);
  Store(v, d, buffer.get());

  for (size_t i = 0; i < N; ++i) {
    if constexpr (IsFloat<T>()) {
      printf("%.2f", buffer[i]);
    } else if constexpr (IsSigned<T>()) {
      printf("%d", static_cast<int>(buffer[i]));
    } else {
      printf("%u", static_cast<unsigned>(buffer[i]));
    }

    if (i < N - 1) {
      printf(", ");
    }
  }

  printf("]\n");
}

// 使用示例
void DebugExample() {
  const ScalableTag<float> df;
  const size_t N = Lanes(df);

  AlignedUniquePtr<float[]> data = AllocateAligned<float>(N);
  for (size_t i = 0; i < N; ++i) {
    data[i] = static_cast<float>(i);
  }

  auto v = Load(df, data.get());
  PrintVector(df, v, "Original");

  v = Mul(v, v);
  PrintVector(df, v, "Squared");
}
```

### 打印掩码

```cpp
// 打印掩码
template <typename D>
void PrintMask(D d, Mask<Simd<TFromD<D>, /*...>>> mask, const char* name) {
  const size_t N = Lanes(d);

  printf("%s [", name);

  for (size_t i = 0; i < N; ++i) {
    if (GetLane(mask, i)) {
      printf("T");
    } else {
      printf("F");
    }

    if (i < N - 1) {
      printf(", ");
    }
  }

  printf("]\n");
}

// 使用示例
void DebugMaskExample() {
  const ScalableTag<int32_t> di32;
  const size_t N = Lanes(di32);

  AlignedUniquePtr<int32_t[]> a = AllocateAligned<int32_t>(N);
  AlignedUniquePtr<int32_t[]> b = AllocateAligned<int32_t>(N);

  for (size_t i = 0; i < N; ++i) {
    a[i] = static_cast<int32_t>(i);
    b[i] = static_cast<int32_t>(N - i);
  }

  auto v_a = Load(di32, a.get());
  auto v_b = Load(di32, b.get());
  auto v_mask = LT(v_a, v_b);

  PrintMask(di32, v_mask, "LT(a, b)");
}
```

### 比较实现差异

```cpp
// 比较标量和 SIMD 实现
void CompareImplementations(float* data, size_t count) {
  // 标量实现
  std::vector<float> scalar_result(count);
  for (size_t i = 0; i < count; ++i) {
    scalar_result[i] = data[i] * 2.0f + 1.0f;
  }

  // SIMD 实现
  AlignedVector<float> simd_result(count);
  const ScalableTag<float> df;
  const size_t N = Lanes(df);

  for (size_t i = 0; i + N <= count; i += N) {
    auto v = Load(df, &data[i]);
    v = MulAdd(v, Set(df, 2.0f), Set(df, 1.0f));
    Store(v, df, &simd_result[i]);
  }

  // 处理尾部
  for (size_t i = (count / N) * N; i < count; ++i) {
    simd_result[i] = data[i] * 2.0f + 1.0f;
  }

  // 比较结果
  for (size_t i = 0; i < count; ++i) {
    float diff = std::abs(scalar_result[i] - simd_result[i]);

    if (diff > 0.001f) {  // 允许浮点误差
      printf("Mismatch at index %zu: scalar=%.6f, simd=%.6f, diff=%.6f\n",
             i, scalar_result[i], simd_result[i], diff);
    }
  }
}
```

---

## 14.4 性能测试

### 基本基准测试

```cpp
#include <chrono>
#include <vector>

// 计时工具类
class Timer {
 public:
  void Start() {
    start_ = std::chrono::high_resolution_clock::now();
  }

  double Stop() {
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_);
    return duration.count() / 1e9;  // 转换为秒
  }

 private:
  std::chrono::high_resolution_clock::time_point start_;
};

// 基准测试函数
template <typename T, typename Func>
double Benchmark(Func func, size_t iterations, size_t warmup = 10) {
  // 预热
  for (size_t i = 0; i < warmup; ++i) {
    func();
  }

  // 正式测试
  Timer timer;
  timer.Start();

  for (size_t i = 0; i < iterations; ++i) {
    func();
  }

  double elapsed = timer.Stop();
  return elapsed / iterations;
}

// 使用示例
int main() {
  const size_t data_size = 1000000;
  AlignedVector<float> data(data_size);

  // 初始化数据
  for (size_t i = 0; i < data_size; ++i) {
    data[i] = static_cast<float>(i);
  }

  // 标量实现
  auto scalar_func = [&]() {
    for (size_t i = 0; i < data_size; ++i) {
      data[i] = data[i] * 2.0f + 1.0f;
    }
  };

  // SIMD 实现
  const ScalableTag<float> df;
  const size_t N = Lanes(df);

  auto simd_func = [N, df, &data]() {
    for (size_t i = 0; i + N <= data_size; i += N) {
      auto v = Load(df, &data[i]);
      v = MulAdd(v, Set(df, 2.0f), Set(df, 1.0f));
      Store(v, df, &data[i]);
    }

    // 处理尾部
    for (size_t i = (data_size / N) * N; i < data_size; ++i) {
      data[i] = data[i] * 2.0f + 1.0f;
    }
  };

  const size_t iterations = 1000;

  double scalar_time = Benchmark<float>(scalar_func, iterations);
  double simd_time = Benchmark<float>(simd_func, iterations);

  printf("Scalar: %.6f seconds/iteration\n", scalar_time);
  printf("SIMD:   %.6f seconds/iteration\n", simd_time);
  printf("Speedup: %.2fx\n", scalar_time / simd_time);

  return 0;
}
```

---

## 14.5 最佳实践

### 1. 选择正确的分发方式

| 场景 | 推荐的分发 | 原因 |
|------|-------------|------|
| 单目标（如嵌入式） | 静态分发 | 零开销，编译时优化 |
| 多目标桌面应用 | 动态分发 | 单二进制，自动选择 |
| 高性能内核 | 静态分发 | 完全内联 |
| 库代码 | 动态分发 | 用户硬件多样 |

### 2. 内存对齐

```cpp
// 好的做法：使用 AlignedAllocator
AlignedVector<float> data(1000);
// 自动对齐，可以使用 Load()

// 不好的做法：使用未对齐的内存
std::vector<float> data(1000);
// 必须使用 LoadU()
```

### 3. 尾部处理策略

```cpp
// 策略 1：填充数组（最简单）
template <typename T>
void ProcessPadded(T* data, size_t count) {
  const ScalableTag<T> d;
  const size_t N = Lanes(d);

  // 分配填充后的数组
  size_t padded_count = RoundUpTo(count, N);
  AlignedUniquePtr<T[]> padded = AllocateAligned<T>(padded_count);

  for (size_t i = 0; i < padded_count; ++i) {
    padded[i] = (i < count) ? data[i] : T{0};
  }

  // 处理（无需尾部处理）
  for (size_t i = 0; i < padded_count; i += N) {
    auto v = Load(d, &padded[i]);
    v = Mul(v, v);
    Store(v, d, &padded[i]);
  }

  // 拷贝结果
  for (size_t i = 0; i < count; ++i) {
    data[i] = padded[i];
  }
}

// 策略 2：手动处理尾部（更精确）
template <typename T>
void ProcessManualTail(T* data, size_t count) {
  const ScalableTag<T> d;
  const size_t N = Lanes(d);

  size_t i = 0;
  for (; i + N <= count; i += N) {
    auto v = Load(d, &data[i]);
    v = Mul(v, v);
    Store(v, d, &data[i]);
  }

  // 处理尾部
  if (i < count) {
    auto mask = FirstN(d, count - i);
    auto v = Load(d, &data[i]);
    v = Mul(v, v);
    BlendedStore(v, mask, d, &data[i]);
  }
}
```

### 4. 避免常见陷阱

```cpp
// 陷阱 1：命名空间作用域问题
// 好的做法
namespace my_lib {
namespace HWY_NAMESPACE {
  // 使用 hn 命名空间别名
  namespace hn = hwy::HWY_NAMESPACE;

  void MyFunction(float* data) {
    const ScalableTag<float> df;
    auto v = Load(df, data);
    v = hn::Mul(v, v);
    hn::Store(v, df, data);
  }
}  // namespace HWY_NAMESPACE
}  // namespace my_lib

// 不好的做法：省略命名空间
namespace my_lib {
  void MyFunctionBad(float* data) {
    const ScalableTag<float> df;
    auto v = Load(df, data);
    v = Mul(v, v);  // 可能在 ADL 下找不到
    Store(v, df, data);  // 同样的问题
  }
}

// 陷阱 2：静态初始化
// 好的做法
class GoodClass {
 public:
  GoodClass() = default;

  void Initialize() {
    const ScalableTag<float> df;
    zero_vec_ = Zero(df);
  }

  void Process(float* data) {
    // 使用成员向量
    Store(zero_vec_, df, data);
  }

 private:
  Vec<ScalableTag<float>> zero_vec_;
};

// 不好的做法：静态初始化可能导致 SIGILL
class BadClass {
 public:
  BadClass() : zero_vec_(Zero(ScalableTag<float>())) {
    // 如果使用动态分发，可能崩溃
  }

 private:
  Vec<ScalableTag<float>> zero_vec_;
};
```

### 5. 使用 contrib 模块

```cpp
// 好的做法：使用 Algorithm 模块
#include "hwy/contrib/algo/transform-inl.h"

void ProcessWithAlgorithm(const float* input, float* output, size_t count) {
  const ScalableTag<float> d;
  Transform1(d, input, count, output, [](auto d, auto v) {
    return Mul(v, v);
  });
}

// 不好的做法：手动处理边界（容易出错）
void ProcessManually(const float* input, float* output, size_t count) {
  const ScalableTag<float> d;
  const size_t N = Lanes(d);

  size_t i = 0;
  for (; i + N <= count; i += N) {
    auto v = Load(d, &input[i]);
    v = Mul(v, v);
    Store(v, d, &output[i]);
  }

  // 处理尾部（容易遗漏或出错）
  for (; i < count; ++i) {
    output[i] = input[i] * input[i];
  }
}
```

---

## 14.6 完整示例：从零到部署

### 1. 定义接口（my_filter.h）

```cpp
#ifndef MY_FILTER_H_
#define MY_FILTER_H_

#include "hwy/base.h"

namespace my_filter {

// 低通滤波器
class LowPassFilter {
 public:
  LowPassFilter(float cutoff_frequency, float sample_rate);
  ~LowPassFilter();

  void Process(const float* input, float* output, size_t count);
  void Reset();

 private:
  float* coefficients_;
  size_t num_coeffs_;
};

}  // namespace my_filter

#endif  // MY_FILTER_H_
```

### 2. 实现动态分发（my_filter.cc）

```cpp
#include "my_filter.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "my_filter.cc"
#include "hwy/foreach_target.h"

#include "hwy/highway.h"
#include "hwy/aligned_allocator.h"

HWY_BEFORE_NAMESPACE();
namespace my_filter {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;

void ApplyFIRFilter(const float* input, float* output, size_t count,
                   const float* coeffs, size_t num_coeffs) {
  const ScalableTag<float> df;
  const size_t N = Lanes(df);

  for (size_t i = 0; i < count; ++i) {
    auto sum = hn::Zero(df);

    for (size_t k = 0; k < num_coeffs && k <= i; ++k) {
      auto v_input = hn::Set(df, input[i - k]);
      auto v_coeff = hn::Set(df, coeffs[k]);
      sum = hn::MulAdd(v_input, v_coeff, sum);
    }

    output[i] = hn::GetLane(sum, 0);
  }
}

}  // namespace HWY_NAMESPACE
}  // namespace my_filter
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace my_filter {

HWY_EXPORT(ApplyFIRFilter);

void LowPassFilter::Process(const float* input, float* output, size_t count) {
  HWY_DYNAMIC_DISPATCH(ApplyFIRFilter)(input, output, count,
                                    coefficients_, num_coeffs_);
}

LowPassFilter::~LowPassFilter() {
  if (coefficients_) {
    FreeAlignedBytes(coefficients_, nullptr, nullptr);
  }
}

}  // namespace my_filter
#endif
```

### 3. 编写测试（my_filter_test.cc）

```cpp
#include "my_filter.h"
#include "hwy/tests/test_util.h"
#include <cmath>
#include <random>

namespace my_filter {
namespace HWY_NAMESPACE {

struct TestFIRFilter {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);

    // 创建测试数据
    AlignedUniquePtr<float[]> input = AllocateAligned<float>(N);
    AlignedUniquePtr<float[]> output = AllocateAligned<float>(N);

    // 生成正弦波信号
    for (size_t i = 0; i < N; ++i) {
      input[i] = std::sin(2.0f * M_PI * i / N);
    }

    // 创建简单的滤波器系数
    std::vector<float> coeffs = {0.1f, 0.2f, 0.4f, 0.2f, 0.1f};

    // 应用滤波器
    ApplyFIRFilter(input.get(), output.get(), N, coeffs.data(), coeffs.size());

    // 验证结果（简化版）
    // 实际测试会更复杂
    for (size_t i = 2; i < N - 2; ++i) {
      float expected = 0.1f * input[i-2] + 0.2f * input[i-1] +
                     0.4f * input[i]   + 0.2f * input[i+1] +
                     0.1f * input[i+2];
      HWY_ASSERT(std::abs(output[i] - expected) < 0.0001f);
    }
  }
};

HWY_NOINLINE void TestAllFIRFilter() {
  ForFloatTypes(ForPartialVectors<TestFIRFilter>());
}

}  // namespace HWY_NAMESPACE
}  // namespace my_filter

#if HWY_ONCE
namespace my_filter {
HWY_BEFORE_TEST(FIRFilterTest);
HWY_EXPORT_AND_TEST_P(FIRFilterTest, TestAllFIRFilter);
HWY_AFTER_TEST();
}
#endif
```

### 4. 基准测试（benchmark.cc）

```cpp
#include "my_filter.h"
#include <chrono>
#include <random>

void BenchmarkFilter() {
  const size_t signal_length = 1000000;
  const float sample_rate = 1000.0f;
  const float cutoff_freq = 50.0f;

  // 创建测试信号
  AlignedVector<float> signal(signal_length);
  std::mt19937 rng(42);
  std::uniform_real_distribution<> dist(-1.0f, 1.0f);

  for (size_t i = 0; i < signal_length; ++i) {
    signal[i] = static_cast<float>(dist(rng)) +
                 std::sin(2.0f * M_PI * cutoff_freq * i / sample_rate);
  }

  // 创建滤波器
  my_filter::LowPassFilter filter(cutoff_freq, sample_rate);

  // 基准测试
  AlignedVector<float> output(signal_length);

  const size_t iterations = 10;
  std::vector<double> times(iterations);

  for (size_t iter = 0; iter < iterations; ++iter) {
    auto start = std::chrono::high_resolution_clock::now();

    filter.Process(signal.data(), output.data(), signal_length);

    auto end = std::chrono::high_resolution_clock::now();
    times[iter] = std::chrono::duration<double, std::milli>(end - start).count();
  }

  // 计算统计
  double mean = 0.0;
  for (double t : times) {
    mean += t;
  }
  mean /= iterations;

  printf("Filter benchmark:\n");
  printf("  Signal length: %zu\n", signal_length);
  printf("  Iterations: %zu\n", iterations);
  printf("  Mean time: %.3f ms\n", mean);
  printf("  Throughput: %.2f MB/s\n",
         signal_length * sizeof(float) / 1024.0 / 1024.0 / (mean / 1000.0));
}

int main() {
  printf("Testing with %s\n", hwy::TargetName(hwy::HWY_SUPPORTED_TARGETS));
  BenchmarkFilter();
  return 0;
}
```

---

## 14.7 学习路线总结

### 14 天课程回顾

| 天数 | 主题 | 关键内容 |
|------|------|----------|
| Day 1 | 项目概览和基本概念 | Highway 架构、分发机制 |
| Day 2 | 核心基础 - base.h | 类型工具、对齐、位操作 |
| Day 3 | 向量标签和类型系统 | ScalableTag、CappedTag、FixedTag |
| Day 4 | 基本向量操作 | Load/Store、算术运算、类型转换 |
| Day 5 | 高级向量操作 | 比较、逻辑、移位、规约 |
| Day 6 | 静态分发机制 | HWY_STATIC_DISPATCH、编译时优化 |
| Day 7 | 动态分发机制 | foreach_target、HWY_EXPORT |
| Day 8 | 平台实现 - x86 | SSE/AVX2/AVX-512 内置函数 |
| Day 9 | 平台实现 - ARM/RISC-V | NEON、SVE、RVV 可变向量 |
| Day 10 | 内存管理和对齐 | AlignedAllocator、对齐操作 |
| Day 11 | 循环向量化 | Algorithm 模块、循环模式 |
| Day 12 | Contrib 算法和排序 | VQSort、Find、统计函数 |
| Day 13 | Contrib 数学和工具 | sin/cos/exp、矩阵运算 |
| Day 14 | 测试框架和最佳实践 | 测试编写、调试、性能测试 |

### 下一步学习建议

1. **实践项目**
   - 选择一个简单的算法（如卷积、FFT）使用 Highway 向量化
   - 编写单元测试和性能基准测试

2. **深入特定平台**
   - 研究你主要使用的平台（x86、ARM 等）
   - 了解平台特定的优化技巧

3. **阅读源码**
   - 深入研究 `ops/*.h` 的实现
   - 学习 Highway 如何处理不同平台

4. **贡献代码**
   - 如果发现 bug 或有改进建议，提交 PR
   - Highway 是开源项目，欢迎贡献

---

## 本日总结

| 类别 | 关键要点 |
|------|----------|
| **测试框架** | 使用 `ForAllTypes(ForPartialVectors<TestStruct>())` 模式 |
| **调试工具** | PrintVector、PrintMask、比较标量和 SIMD 实现 |
| **性能测试** | 使用 Timer 类，预热迭代，计算加速比 |
| **最佳实践** | 选择正确分发、内存对齐、正确处理尾部 |
| **避免陷阱** | 命名空间作用域、静态初始化、ADL 问题 |

| 推荐库/工具 | 用途 |
|---------------|------|
| **test_util.h** | Highway 测试工具 |
| **contrib/algo/** | 高层次算法，简化开发 |
| **contrib/math/** | 向量化数学函数 |
| **contrib/sort/** | VQSort 高性能排序 |
| **aligned_allocator.h** | SIMD 内存管理 |

---

## 课程总结

恭喜你完成了 Highway SIMD 库的 14 天学习课程！

你已经掌握了：
- Highway 的核心架构和设计理念
- 向量标签系统和类型抽象
- 静态和动态分发机制
- 各平台的实现细节
- 高级算法和优化技巧
- 测试和调试 SIMD 代码

现在你可以：
1. 在自己的项目中使用 Highway
2. 编写高性能的 SIMD 代码
3. 理解和优化现有 SIMD 实现

祝你编码愉快！
