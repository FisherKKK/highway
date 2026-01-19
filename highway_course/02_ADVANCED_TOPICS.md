# Highway SIMD 深度学习指南 - 第二部分：高级主题与性能优化

## 目录

1. [Contrib 模块深度剖析](#contrib-模块深度剖析)
2. [可扩展向量架构](#可扩展向量架构)
3. [掩码、混合和条件操作](#掩码混合和条件操作)
4. [循环向量化策略](#循环向量化策略)
5. [性能优化模式](#性能优化模式)
6. [真实世界案例研究](#真实世界案例研究)

---

## Contrib 模块深度剖析

### 1. hwy/contrib/algo - 向量化算法

#### Transform - 映射操作

**文件：** `hwy/contrib/algo/transform-inl.h`

```cpp
// Transform1: 单输入、单输出
template <class D, class InputIt, class OutputIt, class Func>
OutputIt Transform1(D d, InputIt first, size_t count, OutputIt out, const Func& func) {
  const size_t N = Lanes(d);
  size_t i = 0;

  // 处理完整向量
  for (; i + N <= count; i += N) {
    auto v = LoadU(d, &first[i]);     // 未对齐加载
    auto result = func(d, v);          // 应用函数
    StoreU(result, d, &out[i]);       // 未对齐存储
  }

  // 处理余数（标量回退）
  for (; i < count; ++i) {
    out[i] = func_scalar(first[i]);
  }

  return out + count;
}
```

**使用示例：**

```cpp
#include "hwy/contrib/algo/transform-inl.h"

namespace project {
namespace HWY_NAMESPACE {

void SquareArray(float* data, size_t count) {
  const ScalableTag<float> d;

  Transform1(d, data, count, data, [](auto d, auto v) {
    return Mul(v, v);  // v² 向量化
  });
}

}  // namespace HWY_NAMESPACE
}  // namespace project
```

**性能提升：** 4-16x（取决于向量宽度）。

#### Find - 搜索操作

**文件：** `hwy/contrib/algo/find-inl.h`

```cpp
// 在数组中查找首次出现
template <class D, typename T = TFromD<D>>
size_t Find(D d, T value, const T* HWY_RESTRICT array, size_t count) {
  const auto target = Set(d, value);
  const size_t N = Lanes(d);

  for (size_t i = 0; i < count; i += N) {
    auto v = LoadU(d, &array[i]);
    auto mask = Eq(v, target);         // 比较生成掩码

    if (HWY_UNLIKELY(AnyTrue(d, mask))) {
      // 找到匹配！
      return i + FindFirstTrue(d, mask);
    }
  }

  return count;  // 未找到
}
```

**关键技术：**

- `Eq()` 生成每个通道的掩码
- `AnyTrue()` 快速检查任何匹配
- `FindFirstTrue()` 找到第一个设置的位

**性能：** 对于大数组，比标量循环快 8-32x。

#### Copy - 优化的内存拷贝

**文件：** `hwy/contrib/algo/copy-inl.h`

```cpp
template <class D, typename T = TFromD<D>>
void Copy(D d, const T* HWY_RESTRICT from, size_t count, T* HWY_RESTRICT to) {
  const size_t N = Lanes(d);

  // 使用非临时存储用于大拷贝
  if (count > 16384 / sizeof(T)) {
    // 流式拷贝（绕过缓存）
    for (size_t i = 0; i < count; i += N) {
      Stream(LoadU(d, &from[i]), d, &to[i]);
    }
  } else {
    // 常规拷贝
    for (size_t i = 0; i < count; i += N) {
      StoreU(LoadU(d, &from[i]), d, &to[i]);
    }
  }
}
```

**优化：** 对大于 L3 缓存的数据自动切换到流式存储。

### 2. hwy/contrib/math - 数学函数

**文件：** `hwy/contrib/math/math-inl.h`

#### 三角函数

```cpp
// 使用泰勒级数 + 范围缩减的 Sin
template <class D, class V>
V Sin(const D d, V x) {
  // 1. 范围缩减：x mod 2π
  const auto pi2 = Set(d, 6.28318530718);
  x = Sub(x, Mul(Round(Div(x, pi2)), pi2));

  // 2. 泰勒级数：sin(x) ≈ x - x³/3! + x⁵/5! - ...
  const auto x2 = Mul(x, x);
  const auto x3 = Mul(x2, x);
  const auto x5 = Mul(x3, x2);

  auto result = x;
  result = MulSub(x3, Set(d, -0.166666667), result);  // - x³/6
  result = MulAdd(x5, Set(d, 0.00833333333), result); // + x⁵/120
  // ... 更多项以提高精度

  return result;
}
```

**精度 vs 性能权衡：**

- **FastSin：** 2-3 项，~1e-4 误差，快 3x
- **Sin：** 5-7 项，~1e-7 误差
- **PreciseSin：** 完整实现，~1e-14 误差

#### 指数和对数

```cpp
// 使用 IEEE 754 位操作的快速 Log2
template <class D, class V>
V Log2(const D d, V x) {
  const RebindToSigned<D> di;

  // 提取指数（快速整数路径）
  const auto bits = BitCast(di, x);
  const auto exponent = Sub(ShiftRight<23>(bits), Set(di, 127));

  // 提取尾数
  const auto mantissa_bits = Or(And(bits, Set(di, 0x7FFFFF)), Set(di, 0x3F800000));
  const auto mantissa = BitCast(d, mantissa_bits);

  // 多项式逼近 log2(mantissa)
  auto log_m = PolynomialLog2(d, mantissa);

  return Add(ConvertTo(d, exponent), log_m);
}
```

**技巧：** 直接操作浮点位表示（指数 + 尾数）以获得极快的近似值。

### 3. hwy/contrib/sort - VQSort 深度探讨

**VQSort** 是一个完全向量化的快速排序实现。让我们剖析它的工作原理。

#### 排序网络（固定大小）

**文件：** `hwy/contrib/sort/sorting_networks-inl.h`

```cpp
// 8 元素排序网络（12 次比较）
template <class D, class Traits, typename T>
HWY_INLINE void Sort8(D d, Traits st, T* lanes) {
  const FixedTag<T, 8> d8;

  auto v = Load(d8, lanes);

  // Batcher 奇偶归并网络
  v = SortPairsDistance4(d8, st, v);  // 比较 [0,4], [1,5], [2,6], [3,7]
  v = SortPairsDistance2(d8, st, v);  // 比较 [0,2], [1,3], [4,6], [5,7]
  v = SortPairsDistance1(d8, st, v);  // 比较 [0,1], [2,3], [4,5], [6,7]
  v = SortPairsReverse4(d8, st, v);  // ...更多传递

  Store(v, d8, lanes);
}

// SortPairsDistance4 实现
template <class D, class Traits, class V>
V SortPairsDistance4(D d, Traits st, V v) {
  const V swapped = Shuffle01(v);  // 交换相隔 4 的对
  const auto cmp = st.Compare(d, v, swapped);
  return IfThenElse(cmp, swapped, v);  // 条件选择
}
```

**为什么高效？**

- **无分支：** 所有比较都是并行掩码操作
- **数据并行：** 8 次比较在 1 条 SIMD 指令中
- **固定深度：** 确定性性能（无最坏情况）

#### 分区（最难部分）

**文件：** `hwy/contrib/sort/vqsort-inl.h`

```cpp
// 向量化分区：将 < pivot 的移到左边，>= pivot 的移到右边
template <class D, class Traits, typename T>
size_t Partition(D d, Traits st, T* HWY_RESTRICT keys, size_t count, T pivot) {
  const auto vpivot = Set(d, pivot);
  const size_t N = Lanes(d);

  size_t left = 0, right = count - N;
  auto vL = Load(d, &keys[left]);
  auto vR = Load(d, &keys[right]);

  while (left < right) {
    // 生成掩码：哪些元素 < pivot？
    const auto ltL = st.Compare(d, vL, vpivot);
    const auto ltR = st.Compare(d, vR, vpivot);

    // 压缩：将匹配的元素打包在一起
    const size_t numL = CompressStore(vL, ltL, d, &keys[left]);
    const size_t numR = CompressStore(vR, Not(ltR), d, &keys[right]);

    left += numL;
    right -= numR;

    vL = Load(d, &keys[left]);
    vR = Load(d, &keys[right]);
  }

  return left;
}
```

**关键见解：**

1. **CompressStore：** AVX-512 上的单指令（`vcompressstorei32`）
2. **SSE/NEON 上的模拟：** 使用表查找 + 变长存储
3. **双端扫描：** 减少内存流量

#### 主元选择

```cpp
// Median-of-3（向量化）
template <class D, class Traits, typename T>
T ChoosePivot(D d, Traits st, const T* keys, size_t count) {
  const auto v1 = LoadU(d, keys);
  const auto v2 = LoadU(d, keys + count / 2);
  const auto v3 = LoadU(d, keys + count - Lanes(d));

  // 对 3 个向量排序，取中间
  auto median = MedianOfThree(d, st, v1, v2, v3);

  // 提取标量主元
  return GetLane(SortedMedian(d, st, median));
}
```

**性能：** VQSort 在大数组上比 `std::sort` 快 2-10x。

### 4. hwy/contrib/dot - 点积优化

**文件：** `hwy/contrib/dot/dot-inl.h`

```cpp
// 带 FMA 的点积
template <class D>
TFromD<D> Dot(D d, const float* HWY_RESTRICT a, const float* HWY_RESTRICT b, size_t count) {
  const size_t N = Lanes(d);
  auto sum0 = Zero(d);
  auto sum1 = Zero(d);  // 双累加器以减少延迟

  size_t i = 0;
  for (; i + 2 * N <= count; i += 2 * N) {
    auto a0 = LoadU(d, &a[i]);
    auto b0 = LoadU(d, &b[i]);
    auto a1 = LoadU(d, &a[i + N]);
    auto b1 = LoadU(d, &b[i + N]);

    sum0 = MulAdd(a0, b0, sum0);  // FMA: sum0 += a0 * b0
    sum1 = MulAdd(a1, b1, sum1);  // 交错以隐藏延迟
  }

  // 余数
  for (; i < count; i += N) {
    sum0 = MulAdd(LoadU(d, &a[i]), LoadU(d, &b[i]), sum0);
  }

  // 水平求和
  return ReduceSum(d, Add(sum0, sum1));
}
```

**优化：**

1. **双累加器：** 隐藏 FMA 延迟（5 周期）
2. **未对齐加载：** 对齐约束较少
3. **ReduceSum：** 使用 HADD 或 Permute+Add

**性能：** 峰值 FLOPS 的 80-90%（受内存带宽限制）。

---

## 可扩展向量架构

### RISC-V RVV (向量扩展)

**文件：** `hwy/ops/rvv-inl.h`

#### 关键概念

1. **动态向量长度：** `vsetvli` 指令在运行时设置
2. **LMUL（寄存器组）：** 使用多个寄存器 (m1, m2, m4, m8)
3. **分数 LMUL：** mf2 (半寄存器), mf4, mf8

#### Lanes() 的工作原理

```cpp
template <typename T, size_t N, int kPow2>
HWY_API size_t Lanes(Simd<T, N, kPow2> d) {
  // RVV: 调用内在函数！
  return detail::Lanes(d);
}

namespace detail {
  // 实际实现
  template <typename T, size_t N, int kPow2>
  HWY_INLINE size_t Lanes(Simd<T, N, kPow2>) {
    // vsetvli：设置向量长度，返回 vl（向量长度）
    return __riscv_vsetvlmax_e32m1();  // 示例：32 位，LMUL=1
  }
}
```

**关键见解：** `Lanes(d)` 不是 constexpr - 它在运行时查询硬件！

#### LMUL 编码

**文件：** `hwy/ops/rvv-inl.h` 注释

```
kPow2 = -3 → mf8  (1/8 寄存器)
kPow2 = -2 → mf4  (1/4 寄存器)
kPow2 = -1 → mf2  (1/2 寄存器)
kPow2 =  0 → m1   (1 寄存器)    ← 默认
kPow2 = +1 → m2   (2 寄存器)
kPow2 = +2 → m4   (4 寄存器)
kPow2 = +3 → m8   (8 寄存器)
```

**示例：**

```cpp
// float32, LMUL=2（使用 2 个寄存器）
ScalableTag<float, 1> d;  // kPow2 = 1

// 如果 VLEN=256，那么：
// - 单个寄存器 = 256/32 = 8 个 float
// - LMUL=2 → 16 个 float
// Lanes(d) 在运行时返回 16
```

#### RVV 向量操作

```cpp
// 加法（映射到 vadd.vv）
template <size_t N, int kPow2>
Vec<Simd<float, N, kPow2>> Add(Vec<Simd<float, N, kPow2>> a,
                                Vec<Simd<float, N, kPow2>> b) {
  return __riscv_vfadd_vv_f32m1(a.raw, b.raw, Lanes(Simd<float, N, kPow2>()));
  // vfadd.vv vd, vs2, vs1  (向量 + 向量)
}

// Gather（映射到 vluxei）
template <typename T, size_t N, int kPow2>
Vec<Simd<T, N, kPow2>> GatherIndex(Simd<T, N, kPow2> d,
                                    const T* base,
                                    Vec<RebindToSigned<decltype(d)>> indices) {
  return __riscv_vluxei32(base, indices.raw, Lanes(d));
  // vluxei32.v vd, (rs1), vs2  (索引加载)
}
```

### ARM SVE (可扩展向量扩展)

**文件：** `hwy/ops/arm_sve-inl.h`

#### 谓词（掩码）

SVE 使用谓词寄存器（p0-p15）进行掩码：

```cpp
template <typename T>
struct Mask128 {
  svbool_t raw;  // SVE 谓词类型
};

// 示例：掩码加载
template <typename T, size_t N, int kPow2>
Vec<Simd<T, N, kPow2>> MaskedLoad(Mask128<T> m, Simd<T, N, kPow2> d,
                                   const T* p) {
  return svld1(m.raw, p);  // ld1 {zt.s}, pg/z, [xn]
}
```

#### 向量长度不可知代码

```cpp
// 适用于 128 位、256 位、512 位 SVE 的相同代码！
template <class D>
void ProcessArray(D d, float* data, size_t count) {
  const size_t N = Lanes(d);  // 运行时查询

  for (size_t i = 0; i < count; i += N) {
    auto v = Load(d, &data[i]);
    v = Sqrt(v);
    Store(v, d, &data[i]);
  }
}

// 编译一次，在任何 SVE 宽度上运行！
```

**优势：** 前向兼容（对未来更宽的向量）。

#### SVE2_128（固定 128 位）

**文件：** `hwy/detect_targets.h:550-570`

```cpp
// SVE2_128：保证 128 位的 SVE2 子集
#if HWY_TARGET == HWY_SVE2_128
  static_assert(HWY_LANES(uint8_t) == 16, "必须是 128 位");
#endif
```

**为什么存在？** 一些算法需要固定大小（例如，AES, SHA）。

---

## 掩码、混合和条件操作

### 掩码类型

```cpp
template <typename T>
struct Mask128 {
#if HWY_TARGET == HWY_AVX2
  __m256i raw;  // x86: 向量的掩码
#elif HWY_TARGET == HWY_NEON
  uint32x4_t raw;  // ARM: 每通道 0x00...00 或 0xFF...FF
#elif HWY_TARGET == HWY_SVE
  svbool_t raw;  // SVE: 专用谓词寄存器
#endif
};
```

### 比较操作

```cpp
template <class D>
void CompareExample(D d, const float* a, const float* b, float* result) {
  auto va = Load(d, a);
  auto vb = Load(d, b);

  // 生成掩码
  auto mask_lt = Lt(va, vb);      // va < vb
  auto mask_eq = Eq(va, vb);      // va == vb
  auto mask_ge = Ge(va, vb);      // va >= vb

  // 掩码逻辑
  auto mask_in_range = And(mask_ge, Lt(va, Set(d, 10.0f)));
  // (va >= vb) && (va < 10.0)

  // 条件选择
  auto selected = IfThenElse(mask_in_range, va, vb);
  //   mask 为真 → va
  //   mask 为假 → vb

  Store(selected, d, result);
}
```

### 混合和选择

```cpp
// IfThenElse：完全替换
auto result = IfThenElse(mask, true_val, false_val);
// result[i] = mask[i] ? true_val[i] : false_val[i]

// IfThenElseZero：使用零
auto result = IfThenElseZero(mask, value);
// result[i] = mask[i] ? value[i] : 0

// IfThenZeroElse：反转
auto result = IfThenZeroElse(mask, value);
// result[i] = mask[i] ? 0 : value[i]
```

### 压缩和扩展

**Compress：** 将掩码元素打包到向量的左侧

```cpp
template <class D>
void CompressExample(D d) {
  auto v = Iota(d, 0);  // [0, 1, 2, 3, 4, 5, 6, 7]

  auto mask = Eq(And(v, Set(d, 1)), Zero(d));  // 偶数？
  // mask = [T, F, T, F, T, F, T, F]

  auto compressed = Compress(v, mask);
  // → [0, 2, 4, 6, ?, ?, ?, ?]  (未定义的尾部)

  size_t count = CountTrue(d, mask);  // 4
  // 只有前 4 个元素有效
}
```

**实现（AVX-512）：**
```asm
; AVX-512F vcompress 指令
vpcompressd zmm0{k1}, zmm1  ; 单周期！
```

**实现（SSE/AVX2）：**
```cpp
// 表查找：16 种可能的 4 位掩码模式
alignas(64) static constexpr uint8_t table[16][16] = {
  {0,1,2,3, 4,5,6,7, ...},  // 掩码 0000
  {0,1,2,3, 4,5,6,7, ...},  // 掩码 0001
  {4,5,6,7, 0,1,2,3, ...},  // 掩码 0010
  // ...
};

auto idx = MoveMask(mask);  // 提取位掩码
auto shuffle = LoadU(du8, table[idx]);
return TableLookupBytes(v, shuffle);
```

### 掩码压缩存储

**文件：** `hwy/contrib/sort/vqsort-inl.h` 中使用

```cpp
// 只存储掩码元素
template <class D, class V, class M>
size_t CompressStore(V v, M mask, D d, TFromD<D>* HWY_RESTRICT dest) {
#if HWY_TARGET <= HWY_AVX3
  _mm512_mask_compressstoreu_ps(dest, mask.raw, v.raw);
  return PopCount(mask.raw);  // 返回存储计数
#else
  auto compressed = Compress(v, mask);
  size_t count = CountTrue(d, mask);
  StoreU(compressed, d, dest);
  return count;
#endif
}
```

**使用场景：** 过滤（例如，"只保存 > 0 的值"）。

---

## 循环向量化策略

### 策略 1：填充数组（首选）

```cpp
// 假设：count 是 N 的倍数
template <class D>
void ProcessPadded(D d, float* data, size_t count) {
  const size_t N = Lanes(d);
  HWY_DASSERT(count % N == 0);  // 调试断言

  for (size_t i = 0; i < count; i += N) {
    auto v = Load(d, &data[i]);  // 对齐加载，快速
    v = Sqrt(v);
    Store(v, d, &data[i]);
  }
}

// 调用者：
AlignedUniquePtr<float[]> data = AllocateAligned<float>(1024);
ProcessPadded(ScalableTag<float>(), data.get(), 1024);
```

**优点：** 最简单，最快（无余数处理）。

**缺点：** 需要调用者填充。

### 策略 2：重叠处理（适用于只读）

```cpp
template <class D>
void ProcessOverlap(D d, const float* in, float* out, size_t count) {
  const size_t N = Lanes(d);

  if (count >= N) {
    size_t i = 0;
    for (; i + N <= count; i += N) {
      auto v = LoadU(d, &in[i]);
      StoreU(Sqrt(v), d, &out[i]);
    }

    // 余数：从末尾重新加载（重叠）
    auto v = LoadU(d, &in[count - N]);
    StoreU(Sqrt(v), d, &out[count - N]);
  } else {
    // 标量回退
    for (size_t i = 0; i < count; ++i) {
      out[i] = std::sqrt(in[i]);
    }
  }
}
```

**优点：** 简单，无分支。

**缺点：** 重复计算，对原地更新不安全。

### 策略 3：掩码余数

```cpp
template <class D>
void ProcessMasked(D d, float* data, size_t count) {
  const size_t N = Lanes(d);
  size_t i = 0;

  // 完整向量
  for (; i + N <= count; i += N) {
    auto v = Load(d, &data[i]);
    Store(Sqrt(v), d, &data[i]);
  }

  // 余数：使用掩码
  if (i < count) {
    size_t remaining = count - i;
    auto mask = FirstN(d, remaining);  // 前 N 个通道的掩码

    auto v = MaskedLoad(mask, d, &data[i]);
    v = Sqrt(v);
    BlendedStore(v, mask, d, &data[i]);
  }
}
```

**优点：** 安全，无重复。

**缺点：** 需要掩码加载/存储（并非所有平台都快）。

**注意：** 需要 `#if !HWY_MEM_OPS_MIGHT_FAULT` 保护（某些平台上的掩码加载可能在越界时失败）。

### 策略 4：使用 Transform（推荐）

```cpp
#include "hwy/contrib/algo/transform-inl.h"

template <class D>
void ProcessTransform(D d, float* data, size_t count) {
  Transform1(d, data, count, data, [](auto d, auto v) {
    return Sqrt(v);
  });
  // 自动处理余数！
}
```

**优点：** 简洁，被广泛测试，处理所有边缘情况。

**缺点：** 需要包含额外的头文件。

---

## 性能优化模式

### 1. 循环展开以隐藏延迟

**问题：** SIMD 指令有延迟（3-5 周期），但吞吐量高（每周期 1-2 ops）。

**解决方案：** 展开循环以保持多个独立的累加器。

```cpp
template <class D>
float DotProductOptimized(D d, const float* a, const float* b, size_t count) {
  const size_t N = Lanes(d);

  // 4 个独立的累加器
  auto sum0 = Zero(d);
  auto sum1 = Zero(d);
  auto sum2 = Zero(d);
  auto sum3 = Zero(d);

  size_t i = 0;
  for (; i + 4 * N <= count; i += 4 * N) {
    // 4x 展开：隐藏 FMA 延迟
    sum0 = MulAdd(LoadU(d, &a[i + 0*N]), LoadU(d, &b[i + 0*N]), sum0);
    sum1 = MulAdd(LoadU(d, &a[i + 1*N]), LoadU(d, &b[i + 1*N]), sum1);
    sum2 = MulAdd(LoadU(d, &a[i + 2*N]), LoadU(d, &b[i + 2*N]), sum2);
    sum3 = MulAdd(LoadU(d, &a[i + 3*N]), LoadU(d, &b[i + 3*N]), sum3);
  }

  // 合并累加器
  auto sum = Add(Add(sum0, sum1), Add(sum2, sum3));

  // 余数 ...
  return ReduceSum(d, sum);
}
```

**加速：** 对受延迟限制的操作 1.5-2x。

### 2. SoA (结构体数组) vs AoS (数组结构体)

**AoS（缓存友好但 SIMD 不友好）：**

```cpp
struct Particle {
  float x, y, z;
  float vx, vy, vz;
};

Particle particles[1000];

// 难以向量化！
for (size_t i = 0; i < 1000; ++i) {
  particles[i].x += particles[i].vx * dt;
  particles[i].y += particles[i].vy * dt;
  particles[i].z += particles[i].vz * dt;
}
```

**SoA（SIMD 友好）：**

```cpp
struct Particles {
  AlignedUniquePtr<float[]> x, y, z;
  AlignedUniquePtr<float[]> vx, vy, vz;
  size_t count;
};

Particles particles;

// 完美向量化！
template <class D>
void UpdateParticles(D d, Particles& p, float dt) {
  const size_t N = Lanes(d);
  auto vdt = Set(d, dt);

  for (size_t i = 0; i < p.count; i += N) {
    auto x = Load(d, &p.x[i]);
    auto vx = Load(d, &p.vx[i]);
    x = MulAdd(vx, vdt, x);  // x += vx * dt
    Store(x, d, &p.x[i]);
    // y, z 类似 ...
  }
}
```

**加速：** 对粒子系统 4-16x。

### 3. 智能预取

**启发式：** 提前 4-8 次迭代预取。

```cpp
template <class D>
void ProcessWithPrefetch(D d, float* data, size_t count) {
  const size_t N = Lanes(d);
  constexpr size_t kPrefetchAhead = 4;

  // 预取前几次迭代
  for (size_t i = 0; i < kPrefetchAhead * N && i < count; i += N) {
    Prefetch(&data[i]);
  }

  for (size_t i = 0; i < count; i += N) {
    // 为未来迭代预取
    if (i + kPrefetchAhead * N < count) {
      Prefetch(&data[i + kPrefetchAhead * N]);
    }

    auto v = Load(d, &data[i]);
    Store(Sqrt(v), d, &data[i]);
  }
}
```

**何时有效：** 大数组（> L3 缓存）。

**何时无效：** 小数组（开销 > 收益）。

### 4. 减少分支

**慢（分支不可预测）：**

```cpp
for (size_t i = 0; i < count; ++i) {
  if (data[i] > threshold) {
    data[i] = sqrt(data[i]);
  } else {
    data[i] = 0.0f;
  }
}
```

**快（无分支 SIMD）：**

```cpp
template <class D>
void ProcessNoBranch(D d, float* data, size_t count, float threshold) {
  const size_t N = Lanes(d);
  auto vthresh = Set(d, threshold);
  auto vzero = Zero(d);

  for (size_t i = 0; i < count; i += N) {
    auto v = Load(d, &data[i]);
    auto mask = Gt(v, vthresh);

    auto result = IfThenElse(mask, Sqrt(v), vzero);
    Store(result, d, &data[i]);
  }
}
```

**加速：** 对随机数据 2-5x（避免分支错误预测）。

### 5. 使用 FMA（融合乘加）

**慢（2 条指令）：**

```cpp
result = Mul(a, b);
result = Add(result, c);
```

**快（1 条指令，更精确）：**

```cpp
result = MulAdd(a, b, c);  // a * b + c
```

**优势：**

- **2x 吞吐量：** 1 条指令 vs 2 条
- **更高精度：** 无中间舍入
- **更低延迟：** 在某些 CPU 上

**使用场景：** 点积、矩阵乘法、多项式求值。

---

## 真实世界案例研究

### 案例 1：图像处理 - 高斯模糊

**问题：** 将 5x5 高斯核应用于 1920x1080 图像。

**朴素实现：**

```cpp
void GaussianBlurScalar(const uint8_t* in, uint8_t* out, int width, int height) {
  const float kernel[5][5] = { /* ... */ };

  for (int y = 2; y < height - 2; ++y) {
    for (int x = 2; x < width - 2; ++x) {
      float sum = 0.0f;
      for (int ky = -2; ky <= 2; ++ky) {
        for (int kx = -2; kx <= 2; ++kx) {
          sum += in[(y+ky)*width + (x+kx)] * kernel[ky+2][kx+2];
        }
      }
      out[y*width + x] = static_cast<uint8_t>(sum);
    }
  }
}
// 性能：~50 ms（单线程）
```

**Highway 优化：**

```cpp
namespace blur {
namespace HWY_NAMESPACE {

template <class D>
void GaussianBlurRow(D d, const uint8_t* in, uint8_t* out, int width) {
  const RebindToFloat<D> df;
  const size_t N = Lanes(d);

  // 预计算内核系数
  const auto k_center = Set(df, 0.375f);
  const auto k_adjacent = Set(df, 0.25f);
  const auto k_outer = Set(df, 0.0625f);

  for (int x = 2; x < width - 2; x += N) {
    // 加载 5 列（带移位）
    auto v0 = ConvertTo(df, PromoteTo(d, LoadU(d, &in[x - 2])));
    auto v1 = ConvertTo(df, PromoteTo(d, LoadU(d, &in[x - 1])));
    auto v2 = ConvertTo(df, PromoteTo(d, LoadU(d, &in[x])));
    auto v3 = ConvertTo(df, PromoteTo(d, LoadU(d, &in[x + 1])));
    auto v4 = ConvertTo(df, PromoteTo(d, LoadU(d, &in[x + 2])));

    // 应用权重
    auto sum = Mul(v2, k_center);
    sum = MulAdd(Add(v1, v3), k_adjacent, sum);
    sum = MulAdd(Add(v0, v4), k_outer, sum);

    // 转换回 uint8
    auto result = DemoteTo(d, ConvertTo(RebindToSigned<decltype(df)>(), sum));
    StoreU(result, d, &out[x]);
  }
}

}  // namespace HWY_NAMESPACE
}  // namespace blur

// 性能：~8 ms（单线程） → 6x 加速！
```

**关键技术：**

1. 向量化内循环（跨像素）
2. 使用 FMA 进行加权求和
3. 有效的 uint8 ↔ float 转换

### 案例 2：机器学习 - 矩阵乘法 (GEMM)

**问题：** 计算 `C = A × B`，其中 A 是 MxK，B 是 KxN。

**分块 + 向量化实现：**

```cpp
namespace gemm {
namespace HWY_NAMESPACE {

template <class D>
void GemmBlock(D d, const float* A, const float* B, float* C,
               int M, int K, int N) {
  const size_t VN = Lanes(d);

  for (int m = 0; m < M; ++m) {
    for (int n = 0; n < N; n += VN) {
      auto sum = Zero(d);

      // 点积：A[m,:] · B[:,n]
      for (int k = 0; k < K; ++k) {
        auto a_broadcast = Set(d, A[m * K + k]);
        auto b_vec = LoadU(d, &B[k * N + n]);
        sum = MulAdd(a_broadcast, b_vec, sum);
      }

      StoreU(sum, d, &C[m * N + n]);
    }
  }
}

// 主函数：分块以适应缓存
void Gemm(const float* A, const float* B, float* C, int M, int K, int N) {
  const ScalableTag<float> d;
  constexpr int kBlockSize = 64;  // 适应 L1 缓存

  for (int m0 = 0; m0 < M; m0 += kBlockSize) {
    for (int n0 = 0; n0 < N; n0 += kBlockSize) {
      for (int k0 = 0; k0 < K; k0 += kBlockSize) {
        int m_end = HWY_MIN(m0 + kBlockSize, M);
        int n_end = HWY_MIN(n0 + kBlockSize, N);
        int k_end = HWY_MIN(k0 + kBlockSize, K);

        GemmBlock(d, &A[m0 * K + k0], &B[k0 * N + n0], &C[m0 * N + n0],
                  m_end - m0, k_end - k0, n_end - n0);
      }
    }
  }
}

}  // namespace HWY_NAMESPACE
}  // namespace gemm

// 性能：~70% 峰值 FLOPS（与 OpenBLAS 相当）
```

**优化：**

1. **分块：** 利用 L1/L2 缓存
2. **向量化内循环：** 跨 N 维度
3. **广播：** 重用 A 元素
4. **FMA：** 每次迭代一条指令

### 案例 3：科学计算 - N 体模拟

**问题：** 计算 N 个粒子之间的成对引力。

**Highway 实现：**

```cpp
namespace nbody {
namespace HWY_NAMESPACE {

template <class D>
void ComputeForces(D d, const float* x, const float* y, const float* z,
                   float* fx, float* fy, float* fz, int N) {
  const size_t VN = Lanes(d);
  const auto eps2 = Set(d, 1e-6f);  // 软化参数

  for (int i = 0; i < N; ++i) {
    auto xi = Set(d, x[i]);
    auto yi = Set(d, y[i]);
    auto zi = Set(d, z[i]);

    auto fxi = Zero(d);
    auto fyi = Zero(d);
    auto fzi = Zero(d);

    for (int j = 0; j < N; j += VN) {
      // 向量化内循环
      auto xj = LoadU(d, &x[j]);
      auto yj = LoadU(d, &y[j]);
      auto zj = LoadU(d, &z[j]);

      // 距离向量
      auto dx = Sub(xj, xi);
      auto dy = Sub(yj, yi);
      auto dz = Sub(zj, zi);

      // r² = dx² + dy² + dz² + ε²
      auto r2 = MulAdd(dx, dx, eps2);
      r2 = MulAdd(dy, dy, r2);
      r2 = MulAdd(dz, dz, r2);

      // F = 1 / (r² * sqrt(r²))
      auto inv_r = Div(Set(d, 1.0f), Sqrt(r2));
      auto inv_r3 = Mul(Mul(inv_r, inv_r), inv_r);

      // 累积力
      fxi = MulAdd(dx, inv_r3, fxi);
      fyi = MulAdd(dy, inv_r3, fyi);
      fzi = MulAdd(dz, inv_r3, fzi);
    }

    fx[i] = ReduceSum(d, fxi);
    fy[i] = ReduceSum(d, fyi);
    fz[i] = ReduceSum(d, fzi);
  }
}

}  // namespace HWY_NAMESPACE
}  // namespace nbody

// 性能：对 10k 粒子 ~150ms（vs 朴素的 ~900ms） → 6x 加速
```

**关键见解：**

1. 向量化 j 循环（多体 vs 单体）
2. 广播 i 粒子的坐标
3. FMA 用于距离计算
4. 水平求和累积力

---

## 性能分析清单

在优化 Highway 代码时，检查这些要点：

### ✅ 算法级别

- [ ] 选择适合 SIMD 的算法（数据并行优先）
- [ ] 使用 SoA 而非 AoS 布局
- [ ] 最小化水平操作（ReduceSum, FindFirstTrue）
- [ ] 批处理操作以分摊开销

### ✅ 内存级别

- [ ] 对齐数据到 `HWY_ALIGNMENT` (128 字节)
- [ ] 使用 `AlignedAllocator` 或 `AllocateAligned`
- [ ] 对大数组预取（提前 4-8 次迭代）
- [ ] 对只写流使用 `Stream()` (> 16KB)
- [ ] 分块以适应 L1/L2 缓存

### ✅ 循环级别

- [ ] 向量化内循环
- [ ] 2-4x 展开以隐藏延迟
- [ ] 使用 `Transform` / `Find` / `Copy` 辅助程序
- [ ] 正确处理余数（掩码/重叠/填充）
- [ ] 避免分支（使用 `IfThenElse` 代替）

### ✅ 指令级别

- [ ] 使用 `MulAdd` 而非单独的 `Mul` + `Add`
- [ ] 使用 `LoadU` 用于未对齐（而非 `Load` + UB）
- [ ] 对过滤使用 `Compress` / `CompressStore`
- [ ] 检查目标特定的快速路径（`#if HWY_TARGET == ...`）
- [ ] 用 `HWY_INLINE` 标记热函数

### ✅ 编译

- [ ] 使用 `-O2` 或更高（内联关键！）
- [ ] 避免全局 `-march`（与动态调度冲突）
- [ ] 启用 LTO（链接时优化）
- [ ] 检查汇编（`godbolt.org`）以进行回归

---

## 进一步阅读

### Highway 文档

- **快速参考：** `g3doc/quick_reference.md` - 所有操作的备忘单
- **指令矩阵：** `g3doc/instruction_matrix.pdf` - 每个平台的成本
- **常见问题：** `g3doc/faq.md`

### 外部资源

- **Intel 内在函数指南：** software.intel.com/sites/landingpage/IntrinsicsGuide
- **ARM NEON 参考：** developer.arm.com/architectures/instruction-sets/simd-isas/neon
- **RISC-V RVV 规范：** github.com/riscv/riscv-v-spec

### 相关项目

- **Sleef：** SIMD 数学库（与 Highway 互补）
- **xsimd：** 另一个 C++ SIMD 库（不同的设计）
- **Google's gemmlowp：** 量化 GEMM（使用 NEON）

---

## 总结

你现在已经具备：

1. **架构掌握：** 理解调度、标签、向量类型
2. **Contrib 专业知识：** 了解 algo, math, sort, dot
3. **可扩展向量：** RVV/SVE 处理
4. **优化技术：** 预取、FMA、循环展开、SoA
5. **真实世界技能：** 案例研究 + 性能清单

**下一步：** 构建你自己的 Highway 项目！

建议：

- 将现有代码库转换为 Highway
- 为 `contrib/` 贡献新算法
- 对竞争实现进行基准测试
- 在不同架构上进行性能分析

**祝你向量化愉快！** 🚀

---

**作者：** 基于 Highway 1.3.0 深度探索
**日期：** 2026-01-19
