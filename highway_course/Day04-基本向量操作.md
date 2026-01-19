# Day 4: 基本向量操作

## 课程目标

今天我们将学习 Highway 的基本向量操作，包括：
- 内存加载和存储（Load/Store）
- 向量初始化（Zero/Set/AllTrue）
- 算术运算（Add/Sub/Mul/Div）
- 类型转换（PromoteTo/DemoteTo/ConvertTo）

---

## 4.1 Load 和 Store

### Load：对齐加载

```cpp
// 从已对齐的内存加载向量
template <class D, typename T = TFromD<D>>
Vec<D> Load(D d, const T* p);

// 使用示例
const ScalableTag<float> df;
const size_t N = Lanes(df);

// 确保 p 对齐到 N * sizeof(float)
float* aligned_ptr = AlignedAlloc<float>(N);

auto v = Load(df, aligned_ptr);  // 对齐加载
```

### LoadU：非对齐加载

```cpp
// 从可能未对齐的内存加载向量
template <class D, typename T = TFromD<D>>
Vec<D> LoadU(D d, const T* p);

// 使用示例
float* unaligned_ptr = malloc_array();

auto v = LoadU(df, unaligned_ptr);  // 非对齐加载（稍慢但安全）
```

### Store / StoreU

```cpp
// 存储到对齐内存
template <class D, typename T = TFromD<D>>
void Store(const VecArg<Vec<D>> v, D d, T* p);

// 存储到非对齐内存
template <class D, typename T = TFromD<D>>
void Store(const VecArg<Vec<D>> v, D d, T* p);

// 使用示例
auto v = Load(df, input);
Store(v, df, output_aligned);
StoreU(v, df, output_unaligned);
```

### 完整示例：数组复制

```cpp
#include "hwy/highway.h"
namespace hn = hwy::HWY_NAMESPACE;

void CopyArray(const float* src, float* dst, size_t count) {
  const ScalableTag<float> df;
  const size_t N = Lanes(df);

  size_t i = 0;
  // 处理完整向量
  for (; i + N <= count; i += N) {
    auto v = LoadU(df, &src[i]);
    Store(v, df, &dst[i]);
  }
  // 处理剩余元素
  if (i < count) {
    const CappedTag<float, 4> d4;
    const size_t N4 = Lanes(d4);
    for (; i < count; i += N4) {
      size_t remaining = count - i;
      const size_t n = HWY_MIN(remaining, N4);
      // 使用掩码存储
      BlendedStore(Load(d4, &src[i]), FirstN(d4, n), d4, &dst[i]);
    }
  }
}
```

---

## 4.2 向量初始化

### Zero：零向量

```cpp
// 创建所有通道为零的向量
template <class D>
Vec<D> Zero(D d);

// 使用示例
auto v_zero = Zero(df);  // [0, 0, 0, 0, ...]
```

### Set：设置所有通道

```cpp
// 创建所有通道为相同值的向量
template <class D, typename T = TFromD<D>>
Vec<D> Set(D d, const T t);

// 使用示例
auto v = Set(df, 3.14f);  // [3.14, 3.14, 3.14, ...]
auto v_int = Set(du32, 42);  // [42, 42, 42, ...]
```

### Set/Iota：设置不同值

```cpp
// 设置每个通道为不同的值
template <class D, typename T = TFromD<D>>
Vec<D> Set(D d, const T* values);

// 创建递增序列 [0, 1, 2, ...]
template <class D, typename T = TFromD<D>>
Vec<D> Iota(D d, T first = 0);

// 使用示例
float values[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, ...};
auto v = Set(df, values);  // [1.0, 2.0, 3.0, ...]

auto v_iota = Iota(df, 10);  // [10, 11, 12, 13, ...]
```

### AllTrue：全为真

```cpp
// 创建全为 1（真）的掩码或向量
template <class D>
Vec<D> AllTrue(D d);

// 使用示例（整数类型）
auto v_true = AllTrue(du32);  // [0xFFFFFFFF, 0xFFFFFFFF, ...]
```

### Undefined：未定义值

```cpp
// 创建未定义的向量（用于初始化）
template <class D>
Vec<D> Undefined(D d);

// 使用示例
auto v = Undefined(df);  // 值未定义，稍后会被覆盖
```

---

## 4.3 算术运算

### Add / Sub

```cpp
// 向量加法
template <class D>
Vec<D> Add(const VecArg<Vec<D>> a, const VecArg<Vec<D>> b);

// 向量减法
template <class D>
Vec<D> Sub(const VecArg<Vec<D>> a, const VecArg<Vec<D>> b);

// 使用示例
auto a = Load(df, &array[0]);
auto b = Load(df, &array[N]);
auto sum = Add(a, b);
auto diff = Sub(a, b);
```

### Mul / Div

```cpp
// 向量乘法
template <class D>
Vec<D> Mul(const VecArg<Vec<D>> a, const VecArg<Vec<D>> b);

// 向量除法（仅浮点）
template <class D>
Vec<D> Div(const VecArg<Vec<D>> a, const VecArg<Vec<D>> b);

// 使用示例
auto product = Mul(a, b);      // a * b
auto quotient = Div(a, b);     // a / b
```

### MulAdd / MulSub

```cpp
// fused multiply-add: a * b + c
template <class D>
Vec<D> MulAdd(const VecArg<Vec<D>> a,
                const VecArg<Vec<D>> b,
                const VecArg<Vec<D>> c);

// fused multiply-subtract: a * b - c
template <class D>
Vec<D> MulSub(const VecArg<Vec<D>> a,
                const VecArg<Vec<D>> b,
                const VecArg<Vec<D>> c);

// negative multiply-add: c - a * b
template <class D>
Vec<D> NegMulAdd(const VecArg<Vec<D>> a,
                   const VecArg<Vec<D>> b,
                   const VecArg<Vec<D>> c);

// 使用示例
// 计算 y += a * b
auto y = Load(df, y_ptr);
auto a = Load(df, a_ptr);
auto b = Load(df, b_ptr);
y = MulAdd(a, b, y);  // 高效的 FMA 指令
Store(y, df, y_ptr);
```

### Neg / Abs

```cpp
// 取负：-v
template <class D>
Vec<D> Neg(const VecArg<Vec<D>> v);

// 绝对值：|v|
template <class D>
Vec<D> Abs(const VecArg<Vec<D>> v);

// 使用示例
auto neg_v = Neg(v);  // -v
auto abs_v = Abs(v);  // |v|
```

---

## 4.4 整数除法

Div 对于整数类型不直接支持，需要使用近似方法：

```cpp
// 整数除法近似（仅限有符号整数）
template <class D>
Vec<D> ApproximateReciprocal(const VecArg<Vec<D>> v);

// 使用 ApproximateReciprocal 实现整数除法
auto approx_recip = ApproximateReciprocal(divisor);
auto approx_quotient = Mul(dividend, approx_recip);
```

对于精确整数除法，建议使用通用实现或 ConvertTo 浮点。

---

## 4.5 类型转换

### PromoteTo：扩展

```cpp
// 将向量转换为更大的类型（相同通道数）
template <class D, typename T = TFromD<D>>
Vec<Rebind<MakeWide<T>, D>> PromoteTo(
    Rebind<MakeWide<T>, D> d,
    const VecArg<Vec<D>> v);

// 使用示例：uint8_t -> uint16_t
const ScalableTag<uint8_t> du8;
const ScalableTag<uint16_t> du16;  // Rebind<MakeWide<uint8_t>, decltype(du8)>

auto v_u8 = Load(du8, u8_ptr);
auto v_u16 = PromoteTo(du16, v_u8);  // [u8_0, u8_0] [u8_1, u8_1] ...
```

### DemoteTo：截断

```cpp
// 将向量转换为更小的类型（相同通道数）
template <class D, typename T = TFromD<D>>
Vec<Rebind<MakeNarrow<T>, D>> DemoteTo(
    Rebind<MakeNarrow<T>, D> d,
    const VecArg<Vec<D>> v);

// 使用示例：uint16_t -> uint8_t
auto v_u16 = Load(du16, u16_ptr);
auto v_u8 = DemoteTo(du8, v_u16);  // [low_u8_0, low_u8_1, ...]
```

### ConvertTo：数值转换

```cpp
// 数值转换（可能改变通道数）
template <class D, typename T = TFromD<D>>
Vec<D> ConvertTo(D d, const VecArg<Vec<D>> v);

// 使用示例：int32_t -> float32_t
const ScalableTag<int32_t> di32;
const ScalableTag<float> df;

auto v_i32 = Load(di32, i32_ptr);
auto v_f32 = ConvertTo(df, v_i32);  // [42.0, -17.0, ...]

// 使用示例：float32_t -> int32_t
auto v_f32 = Load(df, f32_ptr);
auto v_i32 = ConvertTo(di32, v_f32);  // [42, -17, ...]（截断）
```

---

## 4.6 BitCast：位转换

```cpp
// 无数值转换地重新解释位的类型
template <class D, typename T = TFromD<D>>
Vec<D> BitCast(D d, const VecArg<Vec<D>> v);

// 使用示例：float -> uint32_t（读取指数位）
const ScalableTag<float> df;
const ScalableTag<uint32_t> du32;

auto v_f32 = Load(df, f32_ptr);
auto v_bits = BitCast(du32, v_f32);  // 重新解释为整数
```

### 实际应用：提取浮点指数

```cpp
uint8_t* ExtractExponents(const float* values, size_t count,
                        uint8_t* exponents) {
  const ScalableTag<float> df;
  const ScalableTag<int32_t> di32;
  const ScalableTag<uint8_t> du8;
  const size_t N = Lanes(df);

  for (size_t i = 0; i < count; i += N) {
    auto v_f32 = Load(df, &values[i]);

    // 转换为 uint32_t
    auto v_bits = BitCast(du32, v_f32);

    // 提取指数（IEEE 754: bits [30:23]）
    auto v_exponent = ShiftRight<23>(v_bits);
    auto v_bias = Set(di32, 127);
    v_exponent = Sub(v_exponent, v_bias);

    // 转换回 uint8_t
    auto v_u8 = DemoteTo(du8, v_exponent);
    Store(v_u8, du8, &exponents[i]);
  }
  return exponents;
}
```

---

## 4.7 完整示例：数组平方

```cpp
#include "hwy/highway.h"
namespace hn = hwy::HWY_NAMESPACE;

// 计算每个元素的平方：output[i] = input[i]^2
template <typename T>
void SquareArray(const T* input, T* output, size_t count) {
  const ScalableTag<T> d;
  const size_t N = Lanes(d);

  size_t i = 0;
  // 处理完整向量
  for (; i + N <= count; i += N) {
    auto v = Load(d, &input[i]);
    v = Mul(v, v);
    Store(v, d, &output[i]);
  }
  // 处理剩余元素
  for (; i < count; ++i) {
    output[i] = input[i] * input[i];
  }
}

// 导出用于动态分发
#if HWY_ONCE
namespace hwy {
template <typename T>
HWY_EXPORT(SquareArray);
}  // namespace hwy
#endif
```

---

## 4.8 完整示例：数组加权和

```cpp
// 计算：output = a * weight_a + b * weight_b + bias
template <typename T>
void WeightedSum(const T* a, T weight_a,
                 const T* b, T weight_b,
                 T bias,
                 T* output, size_t count) {
  const ScalableTag<T> d;
  const size_t N = Lanes(d);

  // 创建权重向量
  auto v_weight_a = Set(d, weight_a);
  auto v_weight_b = Set(d, weight_b);
  auto v_bias = Set(d, bias);

  size_t i = 0;
  for (; i + N <= count; i += N) {
    auto v_a = Load(d, &a[i]);
    auto v_b = Load(d, &b[i]);

    // a * weight_a + b * weight_b + bias
    v_a = Mul(v_a, v_weight_a);
    v_b = Mul(v_b, v_weight_b);
    auto v_sum = Add(v_a, v_b);
    v_sum = Add(v_sum, v_bias);

    Store(v_sum, d, &output[i]);
  }

  // 处理剩余元素
  for (; i < count; ++i) {
    output[i] = a[i] * weight_a + b[i] * weight_b + bias;
  }
}
```

---

## 4.9 性能提示

### LoadU vs Load

| 情况 | 使用 | 原因 |
|------|------|------|
| 数据保证对齐 | `Load()` | 更快（使用对齐指令）|
| 数据可能未对齐 | `LoadU()` | 安全（正确性）|
| 循环第一次迭代 | `LoadU()` | 可能未对齐 |
| 循环后续迭代 | `Load()` | 对齐保证 |

### MulAdd 的优势

```cpp
// 低效方式
auto product = Mul(a, b);
auto result = Add(product, c);

// 高效方式（FMA 指令）
auto result = MulAdd(a, b, c);
```

MulAdd 在支持 FMA 的架构（AVX2、NEON）上使用单条指令。

---

## 本日总结

| 操作类别 | 函数 | 说明 |
|----------|------|------|
| **加载/存储** | `Load()`, `LoadU()`, `Store()`, `StoreU()` | 对齐/非对齐内存访问 |
| **初始化** | `Zero()`, `Set()`, `Iota()`, `AllTrue()` | 创建向量 |
| **算术** | `Add()`, `Sub()`, `Mul()`, `Div()` | 基本运算 |
| **融合运算** | `MulAdd()`, `MulSub()`, `NegMulAdd()` | FMA 指令 |
| **一元运算** | `Neg()`, `Abs()` | 负号、绝对值 |
| **类型转换** | `PromoteTo()`, `DemoteTo()`, `ConvertTo()` | 数值转换 |
| **位转换** | `BitCast()` | 无数值转换 |

---

## 练习

1. 实现数组元素乘法：`c[i] = a[i] * b[i]`
2. 使用 `MulAdd` 实现 `c[i] = a[i] * b[i] + c[i]`（就地更新）
3. 实现从 `int32_t` 到 `float` 的转换函数
4. 使用 `BitCast` 提取浮点数的符号位

---

## 下一步

明天我们将学习高级向量操作，包括比较、逻辑运算、移位和规约操作。
