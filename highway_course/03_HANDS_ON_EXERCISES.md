# Highway SIMD 深度学习指南 - 第三部分：实战练习与调试

## 目录

1. [动手实验](#动手实验)
2. [调试技术](#调试技术)
3. [性能分析](#性能分析)
4. [常见陷阱与解决方案](#常见陷阱与解决方案)
5. [构建真实项目](#构建真实项目)
6. [贡献到 Highway](#贡献到-highway)

---

## 动手实验

### 实验 1：你的第一个 Highway 程序

**目标：** 实现一个简单的向量加法，理解完整的动态调度流程。

**文件：** `vec_add.h`

```cpp
#ifndef VEC_ADD_H_
#define VEC_ADD_H_

#include "hwy/base.h"

namespace vec_add {

// 公共 API
void VectorAdd(const float* HWY_RESTRICT a, const float* HWY_RESTRICT b,
               float* HWY_RESTRICT result, size_t count);

}  // namespace vec_add

#endif  // VEC_ADD_H_
```

**文件：** `vec_add-inl.h`

```cpp
#if defined(VEC_ADD_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef VEC_ADD_INL_H_
#undef VEC_ADD_INL_H_
#else
#define VEC_ADD_INL_H_
#endif

#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace vec_add {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

template <class D>
void VectorAddImpl(D d, const float* HWY_RESTRICT a,
                   const float* HWY_RESTRICT b,
                   float* HWY_RESTRICT result, size_t count) {
  const size_t N = hn::Lanes(d);

  // 添加调试打印
  printf("Target: %s, Lanes: %zu\n", hwy::TargetName(HWY_TARGET), N);

  // 主循环
  size_t i = 0;
  for (; i + N <= count; i += N) {
    auto va = hn::LoadU(d, &a[i]);
    auto vb = hn::LoadU(d, &b[i]);
    auto vresult = hn::Add(va, vb);
    hn::StoreU(vresult, d, &result[i]);
  }

  // 余数处理
  if (i < count) {
    size_t remaining = count - i;
    auto mask = hn::FirstN(d, remaining);
    auto va = hn::MaskedLoadOr(hn::Zero(d), mask, d, &a[i]);
    auto vb = hn::MaskedLoadOr(hn::Zero(d), mask, d, &b[i]);
    auto vresult = hn::Add(va, vb);
    hn::BlendedStore(vresult, mask, d, &result[i]);
  }
}

void VectorAdd(const float* HWY_RESTRICT a, const float* HWY_RESTRICT b,
               float* HWY_RESTRICT result, size_t count) {
  const hn::ScalableTag<float> d;
  VectorAddImpl(d, a, b, result, count);
}

}  // namespace HWY_NAMESPACE
}  // namespace vec_add
HWY_AFTER_NAMESPACE();

#endif  // VEC_ADD_INL_H_
```

**文件：** `vec_add.cc`

```cpp
#include "vec_add.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "vec_add.cc"
#include "hwy/foreach_target.h"
#include "hwy/highway.h"
#include "vec_add-inl.h"

HWY_BEFORE_NAMESPACE();
namespace vec_add {
namespace HWY_NAMESPACE {
}  // namespace HWY_NAMESPACE
}  // namespace vec_add
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace vec_add {

HWY_EXPORT(VectorAdd);

void VectorAdd(const float* HWY_RESTRICT a, const float* HWY_RESTRICT b,
               float* HWY_RESTRICT result, size_t count) {
  return HWY_DYNAMIC_DISPATCH(VectorAdd)(a, b, result, count);
}

}  // namespace vec_add
#endif  // HWY_ONCE
```

**文件：** `main.cc`

```cpp
#include <stdio.h>
#include <vector>
#include "vec_add.h"

int main() {
  const size_t count = 1000;
  std::vector<float> a(count), b(count), result(count);

  // 初始化
  for (size_t i = 0; i < count; ++i) {
    a[i] = static_cast<float>(i);
    b[i] = static_cast<float>(i * 2);
  }

  // 执行
  vec_add::VectorAdd(a.data(), b.data(), result.data(), count);

  // 验证
  for (size_t i = 0; i < 10; ++i) {
    printf("result[%zu] = %.1f (expected %.1f)\n",
           i, result[i], a[i] + b[i]);
  }

  return 0;
}
```

**CMakeLists.txt：**

```cmake
cmake_minimum_required(VERSION 3.10)
project(VecAddExample)

set(CMAKE_CXX_STANDARD 17)

# 添加 Highway
add_subdirectory(../.. highway)  # 假设在 Highway 根目录中

add_executable(vec_add_example
  vec_add.cc
  main.cc
)

target_link_libraries(vec_add_example hwy)
target_include_directories(vec_add_example PRIVATE ../..)
```

**构建并运行：**

```bash
mkdir build && cd build
cmake ..
make
./vec_add_example
```

**预期输出：**

```
Target: AVX2, Lanes: 8
result[0] = 0.0 (expected 0.0)
result[1] = 3.0 (expected 3.0)
result[2] = 6.0 (expected 6.0)
...
```

**练习问题：**

1. 修改代码以打印每个编译的目标（提示：在循环中添加 `printf`）
2. 将 `count` 更改为非 8 的倍数（例如 1003），验证余数处理
3. 使用 `HWY_STATIC_DISPATCH` 而非动态调度

---

### 实验 2：实现自定义归约操作

**任务：** 实现 `SumOfSquares(data, count)` 来计算 `Σ(data[i]²)`。

**模板：**

```cpp
// sum_of_squares-inl.h
#if defined(SUM_OF_SQUARES_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef SUM_OF_SQUARES_INL_H_
#undef SUM_OF_SQUARES_INL_H_
#else
#define SUM_OF_SQUARES_INL_H_
#endif

#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace sos {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

float SumOfSquares(const float* data, size_t count) {
  const hn::ScalableTag<float> d;
  const size_t N = hn::Lanes(d);

  // TODO: 实现你的逻辑
  // 提示：
  // 1. 使用 2 个累加器来隐藏延迟
  // 2. sum = MulAdd(v, v, sum)  // sum += v * v
  // 3. return ReduceSum(d, sum0) + ReduceSum(d, sum1)

  auto sum0 = hn::Zero(d);
  auto sum1 = hn::Zero(d);

  size_t i = 0;
  for (; i + 2 * N <= count; i += 2 * N) {
    // 你的代码在这里
  }

  // 余数处理
  for (; i < count; i += N) {
    // 你的代码在这里
  }

  return hn::ReduceSum(d, hn::Add(sum0, sum1));
}

}  // namespace HWY_NAMESPACE
}  // namespace sos
HWY_AFTER_NAMESPACE();

#endif
```

**验证：**

```cpp
// 测试
std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
float result = sos::SumOfSquares(data.data(), data.size());
// 预期：1 + 4 + 9 + 16 + 25 = 55.0
assert(fabs(result - 55.0f) < 1e-5);
```

---

### 实验 3：向量化字符串搜索

**任务：** 在字符串中查找字符的首次出现（向量化 `strchr`）。

**实现：**

```cpp
#if defined(FIND_CHAR_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef FIND_CHAR_INL_H_
#undef FIND_CHAR_INL_H_
#else
#define FIND_CHAR_INL_H_
#endif

#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace find_char {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

size_t FindChar(const char* str, char target, size_t max_len) {
  const hn::ScalableTag<uint8_t> d;
  const size_t N = hn::Lanes(d);

  auto vtarget = hn::Set(d, static_cast<uint8_t>(target));

  for (size_t i = 0; i < max_len; i += N) {
    auto vstr = hn::LoadU(d, reinterpret_cast<const uint8_t*>(&str[i]));

    // 检查 null 终止符
    auto is_null = hn::Eq(vstr, hn::Zero(d));
    if (hn::AnyTrue(d, is_null)) {
      size_t null_pos = i + hn::FindFirstTrue(d, is_null);
      max_len = hn::HWY_MIN(max_len, null_pos);
      if (i >= max_len) break;
    }

    // 检查目标字符
    auto is_match = hn::Eq(vstr, vtarget);
    if (hn::AnyTrue(d, is_match)) {
      return i + hn::FindFirstTrue(d, is_match);
    }
  }

  return max_len;  // 未找到
}

}  // namespace HWY_NAMESPACE
}  // namespace find_char
HWY_AFTER_NAMESPACE();

#endif
```

**测试：**

```cpp
const char* str = "Hello, Highway SIMD!";
size_t pos = find_char::FindChar(str, 'H', 100);
assert(pos == 0);

pos = find_char::FindChar(str, 'S', 100);
assert(pos == 15);

pos = find_char::FindChar(str, 'Z', 100);
assert(pos == strlen(str));  // 未找到
```

**性能：** 对长字符串比标量 `strchr` 快 4-16x。

---

## 调试技术

### 1. 打印向量内容

**使用 Print() 工具：**

```cpp
#include "hwy/print-inl.h"

template <class D>
void DebugVector(D d) {
  auto v = hn::Iota(d, 0);  // [0, 1, 2, 3, ...]

#if HWY_TARGET != HWY_SCALAR
  Print(d, "Iota vector", v);
  // 输出：Iota vector: [0, 1, 2, 3, 4, 5, 6, 7]
#endif
}
```

**手动打印（所有目标）：**

```cpp
template <class D, class V>
void PrintVec(D d, const char* label, V v) {
  HWY_ALIGN T lanes[hn::MaxLanes(d)];
  hn::Store(v, d, lanes);

  printf("%s: [", label);
  for (size_t i = 0; i < hn::Lanes(d); ++i) {
    printf("%g%s", static_cast<double>(lanes[i]),
           (i + 1 < hn::Lanes(d)) ? ", " : "");
  }
  printf("]\n");
}
```

### 2. 断言向量相等

```cpp
#include "hwy/tests/test_util.h"  // HWY_ASSERT_VEC_EQ

template <class D>
void TestAdd(D d) {
  auto a = hn::Set(d, 2.0f);
  auto b = hn::Set(d, 3.0f);
  auto result = hn::Add(a, b);
  auto expected = hn::Set(d, 5.0f);

  HWY_ASSERT_VEC_EQ(d, expected, result);
  // 如果失败，打印实际 vs 预期
}
```

### 3. 检测目标不匹配

**问题：** 混合来自不同目标的代码。

**解决方案：** 使用 `HWY_TARGET` 保护。

```cpp
#if HWY_TARGET == HWY_AVX2
  // AVX2 特定代码
  auto v = _mm256_set1_ps(1.0f);  // 原始内在函数
#else
  auto v = hn::Set(d, 1.0f);      // 可移植
#endif
```

**陷阱：**

```cpp
// 错误：在动态调度中混合目标！
void Bad() {
  const hn::ScalableTag<float> d;
  auto v1 = hn::Set(d, 1.0f);  // 来自 N_AVX2

  // 稍后，在不同的目标中...
  auto v2 = hn::Add(v1, hn::Set(d, 2.0f));  // 来自 N_SSE4
  // ❌ 未定义行为！不同的 Vec 类型
}
```

### 4. Sanitizers

**Address Sanitizer (ASan)：**

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DCMAKE_CXX_FLAGS="-fsanitize=address -g"
make
./my_test

# 检测：
# - 缓冲区溢出
# - 使用后释放
# - 未对齐访问
```

**Memory Sanitizer (MSan)：**

```bash
cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=memory -g"
make
./my_test

# 检测：未初始化的读取
```

**UndefinedBehavior Sanitizer (UBSan)：**

```bash
cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=undefined -g"
make
./my_test

# 检测：
# - 整数溢出
# - 空指针解引用
# - 未对齐访问
```

### 5. 禁用特定目标进行测试

```cpp
#include "hwy/targets.h"

int main() {
  // 仅测试 SSE4
  hwy::SetSupportedTargetsForTest(HWY_SSE4);

  RunTests();

  // 重置
  hwy::SetSupportedTargetsForTest(0);
  return 0;
}
```

---

## 性能分析

### 1. 微基准测试

**使用 Google Benchmark：**

```cpp
#include <benchmark/benchmark.h>
#include "hwy/highway.h"

namespace {
namespace HWY_NAMESPACE {

void BM_VectorAdd(benchmark::State& state) {
  const size_t count = state.range(0);
  std::vector<float> a(count, 1.0f);
  std::vector<float> b(count, 2.0f);
  std::vector<float> result(count);

  for (auto _ : state) {
    vec_add::VectorAdd(a.data(), b.data(), result.data(), count);
    benchmark::DoNotOptimize(result.data());
  }

  state.SetBytesProcessed(state.iterations() * count * sizeof(float) * 3);
  state.SetItemsProcessed(state.iterations() * count);
}

BENCHMARK(BM_VectorAdd)
    ->Range(64, 1 << 20)  // 64 到 1M 元素
    ->Unit(benchmark::kMicrosecond);

}  // namespace HWY_NAMESPACE
}  // namespace

HWY_EXPORT_AND_BENCH(BM_VectorAdd);

int main(int argc, char** argv) {
  ::benchmark::Initialize(&argc, argv);
  ::benchmark::RunSpecifiedBenchmarks();
  return 0;
}
```

**运行：**

```bash
./benchmark --benchmark_filter=BM_VectorAdd
```

**输出：**

```
BM_VectorAdd/64       0.15 us  12.8 GB/s
BM_VectorAdd/256      0.45 us  17.2 GB/s
BM_VectorAdd/1024     1.80 us  17.0 GB/s
BM_VectorAdd/1048576  2100 us  15.0 GB/s
```

### 2. 使用 perf 进行分析

**Linux perf 工具：**

```bash
# 记录性能计数器
perf stat -e cycles,instructions,L1-dcache-loads,L1-dcache-load-misses \
  ./my_benchmark

# 示例输出：
# 1,234,567,890  cycles
# 2,345,678,901  instructions      # 1.9 IPC
#   500,000,000  L1-dcache-loads
#    10,000,000  L1-dcache-misses  # 2% 未命中率
```

**热点分析：**

```bash
perf record -g ./my_benchmark
perf report

# 显示：
# 45.2%  [.] vec_add::VectorAddImpl
# 30.1%  [.] __memcpy_avx_unaligned
# 15.7%  [.] hn::ReduceSum
```

### 3. Vtune / LLVM-MCA

**Intel VTune：**

```bash
vtune -collect hotspots ./my_benchmark
vtune -report hotspots

# 分析：
# - CPI (每指令周期)
# - 前端/后端停顿
# - 缓存未命中
```

**LLVM Machine Code Analyzer：**

```bash
clang++ -O2 -march=native -mllvm -x86-asm-syntax=intel -S vec_add.cc
llvm-mca -timeline vec_add.s

# 输出：吞吐量、延迟、资源压力
```

### 4. 汇编检查

**Compiler Explorer (godbolt.org)：**

1. 粘贴你的 Highway 代码
2. 选择编译器（例如，Clang 17）
3. 添加标志：`-O2 -mavx2 -I/opt/compiler-explorer/libs/highway/include`
4. 检查生成的汇编

**寻找的内容：**

- ✅ **好：** `vmulps`, `vfmadd231ps`（AVX2/FMA）
- ❌ **坏：** 标量 `mulss`（向量化失败）
- ❌ **坏：** 过多的 `vmovaps`（不必要的移动）
- ✅ **好：** 循环展开（重复的指令块）

**示例（良好的 AVX2）：**

```asm
.LBB0_2:
  vmovups ymm0, ymmword ptr [rdi + 4*rsi]
  vmovups ymm1, ymmword ptr [rdx + 4*rsi]
  vaddps  ymm0, ymm0, ymm1
  vmovups ymmword ptr [rcx + 4*rsi], ymm0
  add     rsi, 8
  cmp     rsi, rax
  jb      .LBB0_2
```

---

## 常见陷阱与解决方案

### 陷阱 1：忘记包含保护切换

**症状：** 编译错误："重新定义"或"缺少符号"。

**原因：**

```cpp
// 错误：标准包含保护
#ifndef MY_HEADER_H
#define MY_HEADER_H
// ... 代码 ...
#endif
```

**修复：**

```cpp
// 正确：切换保护
#if defined(MY_HEADER_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef MY_HEADER_H_
#undef MY_HEADER_H_
#else
#define MY_HEADER_H_
#endif
// ... 代码 ...
#endif
```

### 陷阱 2：在动态调度中使用静态变量

**症状：** 运行时崩溃（SIGILL）或错误结果。

**原因：**

```cpp
// 危险：静态 SIMD 向量
namespace HWY_NAMESPACE {
  static auto kConstant = hn::Set(ScalableTag<float>(), 1.0f);
  // ❌ 在目标选择之前初始化！
}
```

**修复：**

```cpp
// 安全：函数局部或动态初始化
namespace HWY_NAMESPACE {
  HWY_INLINE auto GetConstant() {
    const ScalableTag<float> d;
    return hn::Set(d, 1.0f);
  }
}
```

### 陷阱 3：未对齐访问与 Load()

**症状：** 分段错误或性能不佳。

**原因：**

```cpp
float* unaligned = new float[100];  // malloc 对齐（8/16 字节）
auto v = hn::Load(d, unaligned);    // ❌ 需要 HWY_ALIGNMENT
```

**修复：**

```cpp
// 选项 1：使用 LoadU（未对齐）
auto v = hn::LoadU(d, unaligned);  // ✅ 适用于任何对齐

// 选项 2：对齐分配
AlignedUniquePtr<float[]> aligned = AllocateAligned<float>(100);
auto v = hn::Load(d, aligned.get());  // ✅ 快速对齐加载
```

### 陷阱 4：忘记余数处理

**症状：** 结果的最后几个元素不正确。

**原因：**

```cpp
for (size_t i = 0; i < count; i += N) {
  auto v = hn::Load(d, &data[i]);  // ❌ 越界，如果 count % N != 0
  // ...
}
```

**修复：**

```cpp
// 选项 1：掩码余数
size_t i = 0;
for (; i + N <= count; i += N) { /* 完整向量 */ }
if (i < count) {
  auto mask = hn::FirstN(d, count - i);
  // ... 掩码操作
}

// 选项 2：重叠余数
for (; i + N <= count; i += N) { /* 完整 */ }
if (i < count) {
  auto v = hn::LoadU(d, &data[count - N]);  // 重新加载
}

// 选项 3：使用 Transform
Transform1(d, data, count, data, [](auto d, auto v) { return Sqrt(v); });
```

### 陷阱 5：混合有符号/无符号比较

**症状：** 意外的比较结果。

**原因：**

```cpp
const ScalableTag<int32_t> di;
auto a = hn::Set(di, -1);
auto b = hn::Set(di, 1);

auto mask = hn::Gt(a, b);  // 有符号 GT：假
// 但如果意外使用无符号：
const RebindToUnsigned<decltype(di)> du;
auto mask_u = hn::Gt(BitCast(du, a), BitCast(du, b));  // 无符号：真！
// (0xFFFFFFFF > 0x00000001 在无符号中)
```

**修复：** 始终明确类型。

```cpp
// 明确使用
const ScalableTag<int32_t> di;   // 有符号比较
const ScalableTag<uint32_t> du;  // 无符号比较

// 或使用 RebindToSigned / RebindToUnsigned 转换
```

---

## 构建真实项目

### 项目 1：图像卷积库

**目标：** 为 RGB 图像实现 2D 卷积（模糊、锐化、边缘检测）。

**结构：**

```
image_conv/
├── include/
│   └── image_conv.h          # 公共 API
├── src/
│   ├── image_conv-inl.h      # Highway 实现
│   ├── image_conv.cc         # 调度包装器
│   └── kernels.h             # 卷积核定义
├── tests/
│   └── image_conv_test.cc
└── CMakeLists.txt
```

**API 设计：**

```cpp
// image_conv.h
namespace image_conv {

struct Image {
  uint8_t* data;  // RGB 交错：RGBRGBRGB...
  int width, height;
};

enum class Kernel {
  kBlur3x3,
  kBlur5x5,
  kSharpen,
  kEdgeDetect,
  kGaussian
};

void Convolve(const Image& input, Image* output, Kernel kernel);

}  // namespace image_conv
```

**关键优化：**

1. **分离通道：** 处理 R, G, B 独立以提高 SIMD 效率
2. **边界处理：** 用边缘像素填充或钳位坐标
3. **缓存分块：** 处理小瓦片以适应 L1
4. **预取：** 为下一行提前加载

**挑战问题：**

1. 实现 Sobel 边缘检测（x 和 y 梯度）
2. 添加用于大核的可分离卷积（N² → 2N）
3. 与 OpenCV 进行基准测试

### 项目 2：高性能 JSON 解析器

**目标：** 使用 SIMD 向量化 JSON 标记化。

**关键见解：**

- **字符分类：** 向量化 `is_whitespace`, `is_digit`, `is_quote`
- **引号扫描：** 一次查找 16 个引号
- **转义处理：** 使用掩码跳过 `\"`

**伪代码：**

```cpp
size_t FindNextQuote(const char* json, size_t start, size_t len) {
  const ScalableTag<uint8_t> d;
  const size_t N = Lanes(d);

  auto vquote = Set(d, '"');
  auto vbackslash = Set(d, '\\');

  for (size_t i = start; i < len; i += N) {
    auto vchars = LoadU(d, reinterpret_cast<const uint8_t*>(&json[i]));

    auto is_quote = Eq(vchars, vquote);
    auto is_escape = Eq(vchars, vbackslash);

    // 转义序列后跳过字符
    is_quote = AndNot(is_escape, is_quote);  // 清除 \" 中的引号

    if (AnyTrue(d, is_quote)) {
      return i + FindFirstTrue(d, is_quote);
    }
  }
  return len;
}
```

**参考：** simdjson (github.com/simdjson/simdjson) 用于高级技术。

### 项目 3：SIMD 加速的机器学习推理

**目标：** 为神经网络实现 `MatMul` 和 `ReLU`。

**MatMul（简化的 GEMM）：**

```cpp
// C = A * B，其中 A 是 MxK，B 是 KxN
void MatMul(const float* A, const float* B, float* C,
            int M, int K, int N) {
  const ScalableTag<float> d;
  const size_t VN = Lanes(d);

  for (int m = 0; m < M; ++m) {
    for (int n = 0; n < N; n += VN) {
      auto sum = Zero(d);

      for (int k = 0; k < K; ++k) {
        auto a_val = Set(d, A[m * K + k]);
        auto b_vec = LoadU(d, &B[k * N + n]);
        sum = MulAdd(a_val, b_vec, sum);
      }

      StoreU(sum, d, &C[m * N + n]);
    }
  }
}
```

**ReLU（最大值为零）：**

```cpp
void ReLU(float* data, size_t count) {
  const ScalableTag<float> d;
  const size_t N = Lanes(d);
  auto zero = Zero(d);

  for (size_t i = 0; i < count; i += N) {
    auto v = LoadU(d, &data[i]);
    v = Max(v, zero);  // v = max(v, 0)
    StoreU(v, d, &data[i]);
  }
}
```

**扩展：**

- Softmax（使用 Exp 和 ReduceSum）
- Batch Normalization
- 卷积层（使用 im2col + GEMM）

---

## 贡献到 Highway

### 1. 设置开发环境

```bash
# Fork GitHub 仓库
git clone https://github.com/YOUR_USERNAME/highway.git
cd highway

# 添加上游
git remote add upstream https://github.com/google/highway.git

# 安装工具
sudo apt install clang-format cmake ninja-build

# 构建
mkdir build && cd build
cmake .. -GNinja
ninja
ninja test
```

### 2. 代码风格

**运行 clang-format：**

```bash
# 格式化所有已更改的文件
git diff --name-only | grep -E '\.(cc|h)$' | xargs clang-format -i

# 或使用 pre-commit hook
cp .git/hooks/pre-commit.sample .git/hooks/pre-commit
# 编辑以添加 clang-format 检查
```

**风格指南：**

- 2 空格缩进
- 80 列限制
- `PascalCase` 用于类型，`snake_case` 用于函数
- `k` 前缀用于常量（`kBlockSize`）
- Doxygen 注释用于公共 API

### 3. 添加新操作

**示例：** 添加 `SaturatingAdd`（饱和加法）。

**步骤：**

1. **在 `hwy/ops/generic_ops-inl.h` 中添加默认实现：**

```cpp
// 通用实现（慢）
template <class V>
HWY_API V SaturatingAdd(V a, V b) {
  using D = DFromV<V>;
  const D d;
  using T = TFromV<V>;

  const auto sum = Add(a, b);
  const auto max_val = Set(d, LimitsMax<T>());
  return Min(sum, max_val);
}
```

2. **在 `hwy/ops/x86_128-inl.h` 中添加 x86 优化：**

```cpp
#if HWY_TARGET <= HWY_SSSE3
HWY_API Vec128<uint8_t> SaturatingAdd(Vec128<uint8_t> a, Vec128<uint8_t> b) {
  return Vec128<uint8_t>{_mm_adds_epu8(a.raw, b.raw)};
}
#endif
```

3. **在 `hwy/tests/arithmetic_test.cc` 中添加测试：**

```cpp
struct TestSaturatingAdd {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v1 = Set(d, LimitsMax<T>() - 5);
    const auto v2 = Set(d, T(10));
    const auto result = SaturatingAdd(v1, v2);
    const auto expected = Set(d, LimitsMax<T>());

    HWY_ASSERT_VEC_EQ(d, expected, result);
  }
};

HWY_NOINLINE void TestAllSaturatingAdd() {
  ForUnsignedTypes(ForPartialVectors<TestSaturatingAdd>());
}
```

4. **提交 PR：**

```bash
git checkout -b add-saturating-add
git add hwy/ops/generic_ops-inl.h hwy/ops/x86_128-inl.h hwy/tests/arithmetic_test.cc
git commit -m "Add SaturatingAdd operation"
git push origin add-saturating-add
# 在 GitHub 上创建 PR
```

### 4. 报告 Bug

**好的 bug 报告包括：**

1. **最小重现示例：** 隔离问题的小程序
2. **环境：** 操作系统、编译器、Highway 版本
3. **预期 vs 实际：** 清楚地说明错误
4. **回溯（如果崩溃）：** 从 `gdb` 或 `lldb`

**模板：**

```markdown
## Bug 描述
在 ARM NEON 上调用 `Compress()` 时分段错误。

## 重现
```cpp
const ScalableTag<float> d;
auto v = Iota(d, 0);
auto mask = Eq(v, Set(d, 2.0f));
auto compressed = Compress(v, mask);  // <-- 崩溃在这里
```

## 环境
- OS: Ubuntu 22.04 (aarch64)
- Compiler: GCC 11.3
- Highway: commit abc1234

## 回溯
```
#0  Compress (v=..., mask=...) at hwy/ops/arm_neon-inl.h:1234
#1  main () at test.cc:10
```
```

---

## 总结与下一步

### 你已经掌握

✅ **第 1 部分：** 架构（调度、标签、向量类型）
✅ **第 2 部分：** 高级主题（contrib、可扩展、优化）
✅ **第 3 部分：** 实践（实验、调试、真实项目）

### 继续学习

1. **阅读源代码：** 深入研究 `hwy/contrib/sort/vqsort-inl.h` 以获得高级模式
2. **研究论文：** Google "SIMD quicksort"、"vectorized algorithms"
3. **加入社区：** Highway GitHub 讨论、Stack Overflow
4. **贡献：** 添加新操作、优化、测试

### 推荐项目

- 🎨 **图像处理：** 过滤器、调整大小、颜色转换
- 🧮 **数值计算：** 线性代数、FFT、积分
- 🔍 **数据处理：** CSV 解析、压缩、编码
- 🤖 **ML 推理：** 神经网络运算符、量化

### 资源

- **Highway 仓库：** github.com/google/highway
- **快速参考：** `g3doc/quick_reference.md`
- **性能技巧：** `g3doc/quick_reference.md#performance-tips`
- **指令集参考：** software.intel.com/intrinsics, developer.arm.com

---

**祝你 SIMD 编程愉快！🚀**

如果你构建了有趣的东西，请与社区分享！

---

**作者：** 基于 Highway 1.3.0 实践经验
**日期：** 2026-01-19
