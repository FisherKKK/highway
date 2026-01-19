# Day 3: 向量标签和类型系统

## 课程目标

今天我们将深入理解 Highway 的**向量标签系统**（Tag System），这是 Highway 类型抽象的核心机制。我们将学习 ScalableTag、CappedTag、FixedTag 以及 Simd<> 模板的工作原理。

---

## 3.1 为什么需要标签系统？

### 问题：不同架构的向量大小不同

| 架构 | 指令集 | 向量大小 | float32 通道数 |
|------|--------|----------|---------------|
| x86 | SSE2 | 128-bit | 4 |
| x86 | AVX2 | 256-bit | 8 |
| x86 | AVX-512 | 512-bit | 16 |
| ARM | NEON | 128-bit | 4 |
| ARM | SVE | 可变 | 运行时确定 |
| RISC-V | RVV | 可变 | 运行时确定 |

### Highway 的解决方案：编译时类型

```cpp
// 不使用模板参数指定大小，而是使用"标签"
const ScalableTag<float> df;  // 编译时类型，运行时确定大小
auto v = Load(df, ptr);       // Load 根据标签选择实现

// 标签包含：
// 1. 通道类型 T (float)
// 2. 最大通道数 N (编译时常量)
// 3. 缩放因子 kPow2 (用于分数/倍数)
```

---

## 3.2 Simd<> 模板核心

Simd<> 是所有向量标签的底层模板：

```cpp
// hwy/ops/shared-inl.h
template <typename Lane, size_t N, int kPow2>
struct Simd {
  using T = Lane;  // 通道类型

  // kPow2 = 0: 全向量
  // kPow2 = -1: 半向量
  // kPow2 = 1: 双向量（两向量组合）
  // kPow2 = 2: 四向量组合

  // 最大通道数（上界）
  static constexpr size_t kPrivateLanes = /* 计算得出的值 */;
  static constexpr int kPrivatePow2 = kPow2;

  constexpr size_t MaxLanes() const { return kPrivateLanes; }
  constexpr size_t MaxBytes() const { return kPrivateLanes * sizeof(T); }

  // 类型转换
  template <typename NewT>
  using Rebind = Simd<NewT, /* 新N */, /* 新Pow2 */>;

  template <typename NewT>
  using Repartition = Simd<NewT, /* 新N */, kPow2>;

  using Half = Simd<T, N, kPow2 - 1>;
  using Twice = Simd<T, N, kPow2 + 1>;
};
```

### kPow2 的含义

```
kPow2 = 0:  全向量（1 × V）
kPow2 = 1:  双向量（2 × V）
kPow2 = 2:  四向量（4 × V）
kPow2 = -1: 半向量（1/2 × V）
kPow2 = -2: 四分之一（1/4 × V）
```

### N 编码的特殊性

```cpp
// N 使用固定小数编码支持分数
static constexpr size_t kWhole = N & 0xFFFFF;        // 整数部分（低20位）
static constexpr int kFrac = static_cast<int>(N >> 20);  // 分数部分（高位）

// 例如：Rebind<uint32_t, Simd<uint8_t, 1, 0>>
// 结果：Simd<uint32_t, 0x200001, 2>
// 解释：kWhole = 1, kFrac = 2, kPow2 = 2
// 实际通道数 = 1 × 2^(2-2) = 1（保持相同通道数）
```

---

## 3.3 ScalableTag：可缩放标签

### 定义

```cpp
template <typename T, int kPow2 = 0>
using ScalableTag = Simd<T, HWY_LANES(T), kPow2>;
```

### 使用场景

```cpp
#include "hwy/highway.h"
namespace hn = hwy::HWY_NAMESPACE;

// 全向量 - 最常用
const ScalableTag<float> df;       // 全宽度浮点向量
const ScalableTag<uint8_t> du8;   // 全宽度 uint8 向量

// 双向量（两向量组合）
const ScalableTag<float, 1> df_x2;

// 四向量组合
const ScalableTag<uint32_t, 2> du32_x4;

// 半向量（用于递归）
template <class D>
void RecursiveProcess(D d) {
  constexpr size_t N = MaxLanes(d);
  if (N > 1) {
    RecursiveProcess(typename D::Half{});  // kPow2 - 1
  }
}
```

### 实际运行时行为

```cpp
void Example() {
  const ScalableTag<float> df;

  // 在 SSE2 上：N = 4 (128-bit / 32-bit)
  // 在 AVX2 上：N = 8 (256-bit / 32-bit)
  // 在 AVX-512 上：N = 16 (512-bit / 32-bit)
  // 在 NEON 上：N = 4 (128-bit / 32-bit)
  // 在 SVE 上：N 运行时确定（通常是 2、4、8、16...）

  size_t N = Lanes(df);  // 运行时值
  size_t max = MaxLanes(df);  // 编译时常量
}
```

---

## 3.4 CappedTag：上限标签

### 定义

```cpp
template <typename T, size_t kLimit, int kPow2 = 0>
using CappedTag = Simd<T, /* 限制后的N */, kPow2>;
```

### 使用场景

```cpp
// DCT 变换 - 8x8 块
const CappedTag<float, 8> df8;    // 最多 8 个 float 通道
const CappedTag<int16_t, 16> di16; // 最多 16 个 int16 通道

// 即使在 AVX-512 上（支持 16 个 float），df8 仍限制为 8
// 在 SVE 上，Lanes(df8) 会返回实际可用的通道数（<= 8）

void ProcessDCT8x8(float* block) {
  const CappedTag<float, 8> df;
  const size_t N = Lanes(df);  // 运行时值，可能是 4 或 8

  // 处理 8x8 DCT 的一行
  for (size_t i = 0; i < 8; i += N) {
    auto v = Load(df, &block[i]);
    // ... 处理 ...
  }
}
```

### 与 FixedTag 的区别

| 标签 | 保证 | 用途 |
|------|------|------|
| `CappedTag<T, N>` | 最多 N 个通道，可能更少 | 可变向量架构 |
| `FixedTag<T, N>` | 恰好 N 个通道 | 固定向量大小需求 |

---

## 3.5 FixedTag：固定标签

### 定义

```cpp
template <typename T, size_t kNumLanes>
using FixedTag = Simd<T, kNumLanes, 0>;

// 要求 kNumLanes 是 2 的幂
static_assert(kNumLanes <= HWY_LANES(T));
```

### 使用场景

```cpp
// 128-bit 固定块（用于特定数据结构）
const FixedTag<uint8_t, 16> du8_128bit;  // 16 × 8-bit = 128-bit
const FixedTag<float, 4> df_128bit;      // 4 × 32-bit = 128-bit

void Process128BitBlocks(uint8_t* data) {
  const FixedTag<uint8_t, 16> du8;
  const size_t N = Lanes(du8);  // 总是 16

  // 即使在 AVX-512 上，仍使用 128-bit 向量
  for (size_t i = 0; i < count; i += 16) {
    auto v = Load(du8, &data[i]);
    // ...
  }
}
```

### 警告

```cpp
// FixedTag 不推荐用于通用代码
// 原因：无法利用更宽的向量

// 好的做法：使用 ScalableTag
const ScalableTag<float> df;  // 在 AVX-512 上自动使用 512-bit

// 坏的做法：使用 FixedTag
const FixedTag<float, 4> df4;  // 总是使用 128-bit
```

---

## 3.6 Lanes() 和 MaxLanes()

### 区别

```cpp
const ScalableTag<float> df;

// MaxLanes(): 编译时常量，返回上界
constexpr size_t max = MaxLanes(df);  // AVX-512: 16, NEON: 4

// Lanes(): 运行时值，返回实际值
size_t n = Lanes(df);  // 运行时确定
```

### 编译时常量 vs 运行时值

```cpp
template <class D>
void Example(D d) {
  // 可用于数组大小（静态分配）
  static float buffer[MaxLanes(d)];  // 编译时常量

  // 用于循环条件（运行时）
  size_t N = Lanes(d);
  for (size_t i = 0; i < N; ++i) {
    // ...
  }
}
```

### 条件编译

```cpp
#if HWY_HAVE_SCALABLE
  // SVE/RVV - Lanes() 不是 constexpr
  size_t N = Lanes(df);
#else
  // SSE/AVX/NEON - Lanes() 是 constexpr
  constexpr size_t N = Lanes(df);
#endif
```

---

## 3.7 类型转换工具

### Rebind：相同通道数，不同类型

```cpp
const ScalableTag<float> df;

// Rebind 到相同通道数的其他类型
using DI32 = Rebind<int32_t, decltype(df)>;  // Simd<int32_t, ..., 0>
using DU8 = Rebind<uint8_t, decltype(df)>;   // Simd<uint8_t, ..., 0>

// 辅助别名
using DI32 = RebindToSigned<decltype(df)>;   // int32_t（如果 float）
using DU32 = RebindToUnsigned<decltype(df)>; // uint32_t

// 使用示例
void ConvertAndProcess(const ScalableTag<float> df,
                    const float* f32_input,
                    int32_t* i32_output) {
  const DI32 di32;
  auto v_f32 = Load(df, f32_input);
  auto v_i32 = ConvertTo(di32, v_f32);  // float -> int32
  Store(v_i32, di32, i32_output);
}
```

### Repartition：相同总大小，不同类型

```cpp
const ScalableTag<uint8_t> du8;

// Repartition 到更大类型（相同总字节数）
using DU16 = Repartition<uint16_t, decltype(du8)>;  // 通道数减半
using DU32 = Repartition<uint32_t, decltype(du8)>;  // 通道数除以 4
using DF = Repartition<float, decltype(du8)>;       // uint8_t -> float

// 使用示例：uint8_t 向量转为 uint16_t 向量
auto v_u8 = Load(du8, u8_ptr);
const DU16 du16;
auto v_u16 = RepartitionToWide(du16, v_u8);  // 每两个 u8 合并为 u16

// 辅助别名
using DU16 = RepartitionToWide<decltype(du8)>;     // uint8_t -> uint16_t
using DU32 = RepartitionToWideX2<decltype(du8)>;  // uint8_t -> uint32_t
using DU64 = RepartitionToWideX3<decltype(du8)>;  // uint8_t -> uint64_t
```

### Half / Twice：相同类型，不同通道数

```cpp
const ScalableTag<float> df;

// Half: 半向量（kPow2 - 1）
using DHalf = typename decltype(df)::Half;

// Twice: 双向量（kPow2 + 1）
using DTwice = typename decltype(df)::Twice;

// 使用示例
void SplitAndCombine(const ScalableTag<float> df) {
  const DHalf dhalf;
  const DTwice dtwice;

  // 从一个全向量创建两个半向量
  auto v_full = Load(df, data);
  auto v_low = LowerHalf(dhalf, v_full);
  auto v_high = UpperHalf(dhalf, v_full);

  // 组合两个半向量
  auto v_combined = Combine(dtwise, v_low, v_high);
}
```

---

## 3.8 IsAligned() 检查

```cpp
// 检查指针是否对齐到向量边界
template <class D, typename T>
HWY_API bool IsAligned(D d, T* ptr) {
  const size_t N = Lanes(d);
  return reinterpret_cast<uintptr_t>(ptr) % (N * sizeof(T)) == 0;
}

// 使用示例
void Process(const ScalableTag<float> df, float* data) {
  if (IsAligned(df, data)) {
    // 数据对齐，可以使用对齐加载
    auto v = Load(df, data);
  } else {
    // 数据未对齐，必须使用非对齐加载
    auto v = LoadU(df, data);
  }
}
```

---

## 3.9 高级：固定大小别名

```cpp
// 特定大小的别名
template <typename T>
using Full16 = Simd<T, 2 / sizeof(T), 0>;   // 16-bit 向量
template <typename T>
using Full32 = Simd<T, 4 / sizeof(T), 0>;   // 32-bit 向量
template <typename T>
using Full64 = Simd<T, 8 / sizeof(T), 0>;   // 64-bit 向量
template <typename T>
using Full128 = Simd<T, 16 / sizeof(T), 0>;  // 128-bit 向量

// 使用示例
const Full128<uint8_t> du8_128;  // 16 个 uint8_t = 128-bit
const Full64<float> df_64;       // 16 个 float = 64-bit
```

---

## 3.10 完整示例：矩阵乘法中的标签使用

```cpp
#include "hwy/highway.h"
namespace hn = hwy::HWY_NAMESPACE;

// 通用矩阵向量乘法：y = A × x
// A: rows × cols
// x: cols
// y: rows
template <typename T>
void MatrixVectorMul(const T* A, const T* x, T* y,
                    size_t rows, size_t cols) {
  const ScalableTag<T> d;
  const size_t N = Lanes(d);

  // 清零结果向量 y
  ZeroBytes(y, rows * sizeof(T));

  // 遍历列
  for (size_t j = 0; j < cols; j += N) {
    // 使用 CappedTag 确保不越界
    size_t remaining = cols - j;
    size_t cols_to_process = HWY_MIN(remaining, N);
    const CappedTag<T, 64> dj(cols_to_process);
    size_t Nj = Lanes(dj);

    // 加载 x 的当前块
    auto x_vec = Load(dj, x + j);

    // 遍历行
    for (size_t i = 0; i < rows; ++i) {
      // 加载 A 的当前行块
      auto a_vec = Load(dj, A + i * cols + j);

      // 乘加并累加
      auto y_vec = Load(dj, y + i);
      y_vec = MulAdd(a_vec, x_vec, y_vec);
      Store(y_vec, dj, y + i);
    }
  }
}
```

---

## 本日总结

| 标签 | 用途 | 特性 |
|------|------|------|
| `ScalableTag<T>` | 全向量，最常用 | 自动使用最佳宽度 |
| `ScalableTag<T, kPow2>` | 分数/倍数向量 | kPow2 = ±1, ±2... |
| `CappedTag<T, N>` | 限制通道数 | 可变向量架构友好 |
| `FixedTag<T, N>` | 固定通道数 | 不推荐（无法利用宽向量） |

| 工具 | 功能 |
|------|------|
| `Rebind<T, D>` | 相同通道数，不同类型 |
| `Repartition<T, D>` | 相同总大小，不同类型 |
| `D::Half` | 半向量 |
| `D::Twice` | 双向量 |
| `Lanes(d)` | 运行时通道数 |
| `MaxLanes(d)` | 编译时上界 |

---

## 练习

1. 编写一个函数，使用 `ScalableTag<float>` 将两个数组相加
2. 修改上述函数，使用 `CappedTag<float, 8>` 限制每次处理 8 个元素
3. 使用 `Rebind` 和 `Repartition` 转换不同类型的向量

---

## 下一步

明天我们将学习 Highway 的基本向量操作，包括 Load/Store、算术运算和初始化函数。
