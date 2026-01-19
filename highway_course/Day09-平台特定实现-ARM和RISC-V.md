# Day 9: 平台特定实现 - ARM 和 RISC-V

## 课程目标

今天我们将学习 ARM（NEON/SVE）和 RISC-V（RVV）平台的 Highway 实现，特别关注可变长度向量的处理。

---

## 9.1 ARM 实现文件结构

```
hwy/ops/
├── arm_neon-inl.h     # ARM NEON 实现（128-bit 固定）
└── arm_sve-inl.h      # ARM SVE 实现（可变长度）
```

### 文件选择

```cpp
// highway.h
#if HWY_TARGET & HWY_ALL_NEON
#include "hwy/ops/arm_neon-inl.h"
#elif HWY_TARGET & HWY_ALL_SVE
#include "hwy/ops/arm_sve-inl.h"
#endif
```

---

## 9.2 ARM NEON：固定 128-bit 向量

### 基本类型定义

```cpp
// arm_neon-inl.h
namespace detail {

// NEON 原始类型（128-bit，与架构无关）
using RawN = int8x16_t;      // 16 × 8-bit 整数
using RawN_16 = int16x8_t;   // 8 × 16-bit 整数
using RawN_32 = int32x4_t;   // 4 × 32-bit 整数
using RawN_64 = int64x2_t;   // 2 × 64-bit 整数

using RawF = float32x4_t;     // 4 × 32-bit 浮点
using RawF_64 = float64x2_t;  // 2 × 64-bit 浮点

}  // namespace detail

// Vec 和 Mask 包装类型
namespace hwy {
namespace HWY_NAMESPACE {

template <typename T, size_t N>
struct Vec128 {
  // 根据类型选择正确的 NEON 类型
  typename detail::Raw<T, N>::type raw;

  // 构造函数
  HWY_INLINE Vec128() = default;
  HWY_INLINE Vec128(typename detail::Raw<T, N>::type v) : raw(v) {}
};

template <typename T, size_t N>
using Mask128 = Vec128<T, N>;

}  // namespace HWY_NAMESPACE
}  // namespace hwy
```

### Load/Store 实现

```cpp
// 非对齐加载（NEON 没有严格的对齐要求）
template <typename T, size_t N>
Vec128<T, N> LoadU(Simd<T, N, 0> d, const T* p) {
#if HWY_TARGET == HWY_NEON
  // vld1q_u8/vld1q_s8/vld1q_s32 等
  if constexpr (sizeof(T) == 1) {
    return Vec128<T, N>{vld1q_u8(reinterpret_cast<const uint8_t*>(p))};
  } else if constexpr (sizeof(T) == 2) {
    return Vec128<T, N>{vld1q_u16(reinterpret_cast<const uint16_t*>(p))};
  } else if constexpr (sizeof(T) == 4) {
    if constexpr (IsFloat<T>()) {
      return Vec128<T, N>{vld1q_f32(reinterpret_cast<const float*>(p))};
    } else {
      return Vec128<T, N>{vld1q_u32(reinterpret_cast<const uint32_t*>(p))};
  }
  // ...
#endif
}

// 存储
template <typename T, size_t N>
void Store(const Vec128<T, N> v, Simd<T, N, 0> d, T* p) {
#if HWY_TARGET == HWY_NEON
  // vst1q_u8/vst1q_s32 等
  if constexpr (sizeof(T) == 1) {
    vst1q_u8(reinterpret_cast<uint8_t*>(p), v.raw);
  } else if constexpr (sizeof(T) == 4) {
    if constexpr (IsFloat<T>()) {
      vst1q_f32(reinterpret_cast<float*>(p), v.raw);
    } else {
      vst1q_u32(reinterpret_cast<uint32_t*>(p), v.raw);
    }
  }
  // ...
#endif
}
```

### 算术运算实现

```cpp
// Add
template <typename T, size_t N>
Vec128<T, N> Add(const Vec128<T, N> a, const Vec128<T, N> b) {
#if HWY_TARGET == HWY_NEON
  // vaddq_s8/vaddq_s16/vaddq_s32/vaddq_f32 等
  if constexpr (IsSigned<T>() && sizeof(T) == 1) {
    return Vec128<T, N>{vaddq_s8(a.raw, b.raw)};
  } else if constexpr (IsSigned<T>() && sizeof(T) == 2) {
    return Vec128<T, N>{vaddq_s16(a.raw, b.raw)};
  } else if constexpr (IsSigned<T>() && sizeof(T) == 4) {
    return Vec128<T, N>{vaddq_s32(a.raw, b.raw)};
  } else if constexpr (IsFloat<T>()) {
    return Vec128<T, N>{vaddq_f32(a.raw, b.raw)};
  }
  // ...
#endif
}

// Mul
template <typename T, size_t N>
Vec128<T, N> Mul(const Vec128<T, N> a, const Vec128<T, N> b) {
#if HWY_TARGET == HWY_NEON
  // vmulq_s16/vmulq_s32/vmulq_f32 等
  if constexpr (IsSigned<T>() && sizeof(T) == 2) {
    return Vec128<T, N>{vmulq_s16(a.raw, b.raw)};
  } else if constexpr (IsSigned<T>() && sizeof(T) == 4) {
    return Vec128<T, N>{vmulq_s32(a.raw, b.raw)};
  } else if constexpr (IsFloat<T>()) {
    return Vec128<T, N>{vmulq_f32(a.raw, b.raw)};
  }
  // ...
#endif
}

// FMA
template <typename T, size_t N>
Vec128<T, N> MulAdd(const Vec128<T, N> a,
                  const Vec128<T, N> b,
                  const Vec128<T, N> c) {
#if HWY_TARGET == HWY_NEON
  // vfmaq_f32: a * b + c（浮点）
  if constexpr (IsFloat<T>()) {
    return Vec128<T, N>{vfmaq_f32(c.raw, a.raw, b.raw)};
  }
  // 整数：使用 vmlaq 等或分解实现
#endif
}
```

### 比较操作

```cpp
// EQ
template <typename T, size_t N>
Mask128<T, N> EQ(const Vec128<T, N> a, const Vec128<T, N> b) {
#if HWY_TARGET == HWY_NEON
  // vceqq_s8/vceqq_s16/vceqq_s32/vceqq_f32 等
  // 结果为 0 或全 1
  if constexpr (IsSigned<T>() && sizeof(T) == 1) {
    return Mask128<T, N>{vreinterpretq_s8_u8(vceqq_s8(a.raw, b.raw))};
  } else if constexpr (IsSigned<T>() && sizeof(T) == 4) {
    return Mask128<T, N>{vreinterpretq_s32_u32(vceqq_s32(a.raw, b.raw))};
  } else if constexpr (IsFloat<T>()) {
    return Mask128<T, N>{vreinterpretq_s32_u32(vceqq_f32(a.raw, b.raw))};
  }
  // ...
#endif
}

// LT
template <typename T, size_t N>
Mask128<T, N> LT(const Vec128<T, N> a, const Vec128<T, N> b) {
#if HWY_TARGET == HWY_NEON
  // vcltq_s8/vcltq_s16/vcltq_s32/vcltq_f32 等
  if constexpr (IsSigned<T>() && sizeof(T) == 1) {
    return Mask128<T, N>{vreinterpretq_s8_u8(vcltq_s8(a.raw, b.raw))};
  } else if constexpr (IsFloat<T>()) {
    return Mask128<T, N>{vreinterpretq_s32_u32(vcltq_f32(a.raw, b.raw))};
  }
  // ...
#endif
}
```

---

## 9.3 ARM SVE：可变长度向量

### SVE 的特殊性

SVE (Scalable Vector Extension) 的向量长度在**运行时**确定，范围从 128-bit 到 2048-bit（通常 128-bit 的倍数）。

```cpp
// arm_sve-inl.h

// 基本类型：svbool_t（掩码）、svint8_t、svfloat32_t 等
namespace detail {

// 掩码类型
using RawMask = svbool_t;

// 原始向量类型
using RawI8 = svint8_t;
using RawI16 = svint16_t;
using RawI32 = svint32_t;
using RawI64 = svint64_t;

using RawU8 = svuint8_t;
using RawU16 = svuint16_t;
using RawU32 = svuint32_t;
using RawU64 = svuint64_t;

using RawF32 = svfloat32_t;
using RawF64 = svfloat64_t;

}  // namespace detail
```

### Lanes() 运行时实现

```cpp
// 对于 SVE，Lanes() 不是 constexpr
#if HWY_HAVE_SCALABLE
template <class D>
HWY_INLINE HWY_MAYBE_UNUSED size_t Lanes(D d) {
#if HWY_TARGET == HWY_SVE
  // svcntw_b：计算字向量中的元素数
  if constexpr (sizeof(TFromD<D>()) == 4) {
    return svcntw_pat_d(sv_pat_s32());
  } else if constexpr (sizeof(TFromD<D>()) == 2) {
    return svcnth_pat_d(sv_pat_s16());
  } else if constexpr (sizeof(TFromD<D>()) == 1) {
    return svcntb_pat_d(sv_pat_s8());
  }
#endif
}
#endif
```

### Load/Store 实现

```cpp
// SVE 非对齐加载
template <typename T, size_t N>
Vec<ScalableTag<T, /*kPow2*/>> LoadU(Simd<T, N, 0> d,
                                          const T* p) {
#if HWY_TARGET == HWY_SVE
  // svld1：加载向量（可以加载部分元素）
  if constexpr (IsSigned<T>()) {
    if constexpr (sizeof(T) == 1) {
      return Vec<ScalableTag<T>>{svld1_s8(sv_pat_s8(), p)};
    } else if constexpr (sizeof(T) == 2) {
      return Vec<ScalableTag<T>>{svld1_s16(sv_pat_s16(), p)};
    } else if constexpr (sizeof(T) == 4) {
      return Vec<ScalableTag<T>>{svld1_s32(sv_pat_s32(), p)};
    }
  } else if constexpr (IsUnsigned<T>()) {
    // svld1_u8/svld1_u16/svld1_u32 等
  } else if constexpr (IsFloat<T>()) {
    return Vec<ScalableTag<T>>{svld1_f32(sv_pat_s32(), p)};
  }
#endif
}

// SVE 存储
template <typename T, size_t N>
void Store(const Vec<ScalableTag<T>> v,
          Simd<T, N, 0> d,
          T* p) {
#if HWY_TARGET == HWY_SVE
  // svst1：存储向量
  if constexpr (IsSigned<T>()) {
    if constexpr (sizeof(T) == 1) {
      svst1_s8(p, sv_pat_s8(), v.raw);
    } else if constexpr (sizeof(T) == 4) {
      svst1_s32(p, sv_pat_s32(), v.raw);
    }
  } else if constexpr (IsFloat<T>()) {
    svst1_f32(p, sv_pat_s32(), v.raw);
  }
#endif
}
```

### 掩码操作（SVE 特色）

```cpp
// SVE 使用独立的掩码类型 svbool_t

// AllTrue
template <typename T, size_t N>
bool AllTrue(Simd<T, N, 0> d, const Mask<Simd<T, N, 0>> mask) {
#if HWY_TARGET == HWY_SVE
  // svptest_all: 测试掩码是否全为真
  return svptest_all(svptrue_b8(), mask.raw);
#endif
}

// CountTrue
template <typename T, size_t N>
size_t CountTrue(Simd<T, N, 0> d, const Mask<Simd<T, N, 0>> mask) {
#if HWY_TARGET == HWY_SVE
  // svcntp_pat_b8: 计算掩码中设置位的数量
  return svcntp_pat_b8(mask.raw, sv_pat_s8());
#endif
}

// IfThenElse（使用掩码选择）
template <typename T, size_t N>
Vec<Simd<T, N, 0>> IfThenElse(const Mask<Simd<T, N, 0>> mask,
                                const Vec<Simd<T, N, 0>> if_true,
                                const Vec<Simd<T, N, 0>> if_false) {
#if HWY_TARGET == HWY_SVE
  // svsel: 根据掩码选择元素
  return Vec<Simd<T, N, 0>>{svsel(mask.raw, if_true.raw, if_false.raw)};
#endif
}
```

### FirstN：处理可变长度尾部

```cpp
// 创建前 n 个元素为真的掩码
template <typename T, size_t N>
Mask<Simd<T, N, 0>> FirstN(Simd<T, N, 0> d, size_t n) {
#if HWY_TARGET == HWY_SVE
  // svwhilelt_b8: 比较小于，生成掩码
  auto i = Iota(d, static_cast<T>(0));
  auto limit = Set(d, static_cast<T>(n));
  return Mask<Simd<T, N, 0>>{svwhilelt_b8(i.raw, limit.raw)};
#endif
}

// 使用示例：处理数组尾部
template <typename T>
void ProcessTail(const T* data, T* output, size_t count) {
  const ScalableTag<T> d;
  const size_t N = Lanes(d);

  // 处理完整向量
  for (size_t i = 0; i + N <= count; i += N) {
    auto v = Load(d, &data[i]);
    // 处理...
    Store(v, d, &output[i]);
  }

  // 处理剩余元素
  if (count % N != 0) {
    size_t remaining = count % N;
    auto mask = FirstN(d, remaining);  // 创建掩码
    auto v = Load(d, &data[count - N]);  // 加载（可能越界）
    // BlendedStore 只存储掩码为真的部分
    BlendedStore(v, mask, d, &output[count - N]);
  }
}
```

---

## 9.4 RISC-V RVV：可变长度向量

### 基本类型定义

```cpp
// rvv-inl.h

namespace detail {

// RVV 使用 ELEN（元素长度）和 VLEN（向量长度）
// LMUL 控制向量分组（1/2/4/8 个寄存器）

// 基本类型：vint8m1_t, vint16m1_t, vfloat32m1_t 等
// m1 表示 LMUL=1（单寄存器）

using RawI8 = vint8m1_t;
using RawI16 = vint16m1_t;
using RawI32 = vint32m1_t;
using RawI64 = vint64m1_t;

using RawU8 = vuint8m1_t;
using RawU16 = vuint16m1_t;
using RawU32 = vuint32m1_t;
using RawU64 = vuint64m1_t;

using RawF32 = vfloat32m1_t;
using RawF64 = vfloat64m1_t;

// LMUL 参数：Simd 的 kPow2 用于选择 LMUL
// kPow2 = 0: LMUL=1
// kPow2 = 1: LMUL=2
// kPow2 = 2: LMUL=4

}  // namespace detail
```

### Load/Store 实现

```cpp
// RVV 向量加载
template <typename T, size_t N, int kPow2>
Vec<Simd<T, N, kPow2>> LoadU(Simd<T, N, kPow2> d, const T* p) {
#if HWY_TARGET == HWY_RVV
  // vle8_v/vle16_v/vle32_f/vle64_f 等
  // 自动处理向量长度和对齐
  if constexpr (IsSigned<T>()) {
    if constexpr (sizeof(T) == 1) {
      return Vec<Simd<T, N, kPow2>>{vle8_v(p)};
    } else if constexpr (sizeof(T) == 2) {
      return Vec<Simd<T, N, kPow2>>{vle16_v(p)};
    } else if constexpr (sizeof(T) == 4) {
      return Vec<Simd<T, N, kPow2>>{vle32_v(p)};
    }
  } else if constexpr (IsUnsigned<T>()) {
    // vle8u_v/vle16u_v/vle32u_v 等
  } else if constexpr (IsFloat<T>()) {
    if constexpr (sizeof(T) == 4) {
      return Vec<Simd<T, N, kPow2>>{vle32_f(p)};
    }
  }
#endif
}

// RVV 存储
template <typename T, size_t N, int kPow2>
void Store(const Vec<Simd<T, N, kPow2>> v,
          Simd<T, N, kPow2> d,
          T* p) {
#if HWY_TARGET == HWY_RVV
  if constexpr (IsSigned<T>()) {
    if constexpr (sizeof(T) == 1) {
      vse8_v(p, v.raw);
    } else if constexpr (sizeof(T) == 2) {
      vse16_v(p, v.raw);
    } else if constexpr (sizeof(T) == 4) {
      vse32_v(p, v.raw);
    }
  } else if constexpr (IsFloat<T>()) {
    if constexpr (sizeof(T) == 4) {
      vse32_f(p, v.raw);
    }
  }
#endif
}
```

### 算术运算

```cpp
// Add
template <typename T, size_t N, int kPow2>
Vec<Simd<T, N, kPow2>> Add(const Vec<Simd<T, N, kPow2>> a,
                              const Vec<Simd<T, N, kPow2>> b) {
#if HWY_TARGET == HWY_RVV
  // vadd/vv: 向量-向量加法
  // vadd/vx: 向量-标量加法
  if constexpr (IsSigned<T>()) {
    if constexpr (sizeof(T) == 1) {
      return Vec<Simd<T, N, kPow2>>{vadd_vv_i8m1(a.raw, b.raw)};
    } else if constexpr (sizeof(T) == 2) {
      return Vec<Simd<T, N, kPow2>>{vadd_vv_i16m1(a.raw, b.raw)};
    } else if constexpr (sizeof(T) == 4) {
      return Vec<Simd<T, N, kPow2>>{vadd_vv_i32m1(a.raw, b.raw)};
    }
  } else if constexpr (IsFloat<T>()) {
    if constexpr (sizeof(T) == 4) {
      return Vec<Simd<T, N, kPow2>>{vfadd_vv_f32m1(a.raw, b.raw)};
    }
  }
#endif
}

// Mul
template <typename T, size_t N, int kPow2>
Vec<Simd<T, N, kPow2>> Mul(const Vec<Simd<T, N, kPow2>> a,
                              const Vec<Simd<T, N, kPow2>> b) {
#if HWY_TARGET == HWY_RVV
  if constexpr (IsSigned<T>()) {
    if constexpr (sizeof(T) == 2) {
      return Vec<Simd<T, N, kPow2>>{vmul_vv_i16m1(a.raw, b.raw)};
    } else if constexpr (sizeof(T) == 4) {
      return Vec<Simd<T, N, kPow2>>{vmul_vv_i32m1(a.raw, b.raw)};
    }
  } else if constexpr (IsFloat<T>()) {
    if constexpr (sizeof(T) == 4) {
      return Vec<Simd<T, N, kPow2>>{vfmul_vv_f32m1(a.raw, b.raw)};
    }
  }
#endif
}

// FMA
template <typename T, size_t N, int kPow2>
Vec<Simd<T, N, kPow2>> MulAdd(const Vec<Simd<T, N, kPow2>> a,
                              const Vec<Simd<T, N, kPow2>> b,
                              const Vec<Simd<T, N, kPow2>> c) {
#if HWY_TARGET == HWY_RVV
  if constexpr (IsFloat<T>()) {
    if constexpr (sizeof(T) == 4) {
      // vfmacc: c = a * b + c
      return Vec<Simd<T, N, kPow2>>{vfmacc_f32m1(a.raw, b.raw, c.raw)};
    }
  }
#endif
}
```

### 掩码操作

```cpp
// RVV 使用位掩码（vbool32_t 等）

// AllTrue
template <typename T, size_t N, int kPow2>
bool AllTrue(Simd<T, N, kPow2> d, const Mask<Simd<T, N, kPow2>> mask) {
#if HWY_TARGET == HWY_RVV
  // vmfirst_m: 检查掩码是否全为真
  return vmfirst_m_b1(mask.raw) >= 0;  // >= 0 表示全为真
#endif
}

// CountTrue
template <typename T, size_t N, int kPow2>
size_t CountTrue(Simd<T, N, kPow2> d, const Mask<Simd<T, N, kPow2>> mask) {
#if HWY_TARGET == HWY_RVV
  // vcpop_m: 计算掩码中设置位的数量
  // RVV 1.0+: vpopcnt_m
  if constexpr (sizeof(T) == 1) {
    return vpopcnt_m_b8(mask.raw);
  } else if constexpr (sizeof(T) == 2) {
    return vpopcnt_m_b16(mask.raw);
  } else if constexpr (sizeof(T) == 4) {
    return vpopcnt_m_b32(mask.raw);
  }
#endif
}
```

---

## 9.5 可变长度向量的编程模式

### 通用循环模式

```cpp
// 适用于所有可变长度向量（SVE/RVV）
template <typename T>
void ProcessArray(const T* input, T* output, size_t count) {
  const ScalableTag<T> d;
  const size_t N = Lanes(d);  // 运行时值

  size_t i = 0;
  // 处理完整向量
  for (; i + N <= count; i += N) {
    auto v = Load(d, &input[i]);
    v = Mul(v, v);
    Store(v, d, &output[i]);
  }

  // 处理剩余元素（使用掩码）
  if (i < count) {
    size_t remaining = count - i;
    auto mask = FirstN(d, remaining);
    auto v = Load(d, &input[i]);
    v = Mul(v, v);
    BlendedStore(v, mask, d, &output[i]);
  }
}
```

### MaxLanes() vs Lanes()

```cpp
const ScalableTag<float> df;

// MaxLanes(): 编译时常量上界
constexpr size_t max = MaxLanes(df);  // 对于 SVE: 2048/4 = 512

// Lanes(): 运行时实际值
size_t n = Lanes(df);  // 运行时，可能是 4, 8, 16, ..., 512

// 使用 MaxLanes() 静态分配内存
float buffer_static[MaxLanes(df)];  // 编译时分配足够空间

// 使用 Lanes() 运行时循环
for (size_t i = 0; i < Lanes(df); ++i) {
  // ...
}
```

---

## 9.6 平台特定优化

### ARM NEON Dot Product

```cpp
#if HWY_TARGET == HWY_NEON
// vdotq_s32: 点积（ARMv8.2 AArch64）
template <size_t N>
Vec128<int32_t, N/4> DotProduct(const Vec128<int8_t, N> a,
                                 const Vec128<int8_t, N> b) {
  return Vec128<int32_t, N/4>{vdotq_s32(vdupq_n_s32(0), a.raw, b.raw)};
}
#endif
```

### ARM SVE2 字符串操作

```cpp
#if HWY_TARGET == HWY_SVE2
// svmatch: 字符串匹配
template <size_t N>
Vec<uint8_t, N> StringMatch(const Vec<uint8_t, N> str,
                           const Vec<uint8_t, N> pattern) {
  return Vec<uint8_t, N>{svmatch(p0, str.raw, pattern.raw)};
}
#endif
```

### RISC-V 向量扩展

```cpp
#if HWY_TARGET == HWY_RVV
// vrgather: 向量聚集（根据索引加载）
template <size_t N>
Vec<ScalableTag<int32_t>> Gather(const int32_t* base,
                               const Vec<ScalableTag<uint32_t>> indices) {
  return Vec<ScalableTag<int32_t>>{vluxei32_v_i32m1(base, indices.raw)};
}
#endif
```

---

## 本日总结

| 架构 | 指令集 | 向量特性 | 文件 |
|------|--------|----------|------|
| ARM | NEON | 128-bit 固定 | `arm_neon-inl.h` |
| ARM | SVE/SVE2 | 可变长度 | `arm_sve-inl.h` |
| RISC-V | RVV | 可变长度，LMUL | `rvv-inl.h` |

| 概念 | NEON | SVE | RVV |
|------|------|-----|-----|
| **基本类型** | `int8x16_t`, `float32x4_t` | `svint8_t`, `svfloat32_t` | `vint8m1_t`, `vfloat32m1_t` |
| **Lanes()** | constexpr | 运行时 | 运行时 |
| **加载** | `vld1q_u8` | `svld1_s8` | `vle8_v` |
| **存储** | `vst1q_u8` | `svst1_s8` | `vse8_v` |
| **掩码** | 使用向量类型 | `svbool_t` 独立类型 | 位掩码 |
| **尾部处理** | 手动处理 | `svwhilelt`, `svsel` | `vmslt`, `vmerge` |

---

## 练习

1. 查看 `arm_neon-inl.h` 找到 `Add` 的实现
2. 查看 `arm_sve-inl.h` 找到 `FirstN` 的实现
3. 查看 `rvv-inl.h` 找到 `Mul` 的实现

---

## 下一步

明天我们将学习内存管理和对齐，理解 SIMD 内存分配的特殊需求。
