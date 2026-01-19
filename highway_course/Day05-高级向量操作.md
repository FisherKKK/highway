# Day 5: 高级向量操作

## 课程目标

今天我们将学习 Highway 的高级向量操作，包括：
- 比较操作和掩码
- 逻辑运算和位操作
- 移位操作
- 最小/最大和规约操作
- 排列和其他高级操作

---

## 5.1 比较操作

### EQ / NE / LT / LE / GT / GE

```cpp
// 比较结果为掩码（每个通道一个比特）
template <class D>
Mask<D> EQ(const VecArg<Vec<D>> a, const VecArg<Vec<D>> b);  // ==
template <class D>
Mask<D> NE(const VecArg<Vec<D>> a, const VecArg<Vec<D>> b);  // !=
template <class D>
Mask<D> LT(const VecArg<Vec<D>> a, const VecArg<Vec<D>> b);  // <
template <class D>
Mask<D> LE(const VecArg<Vec<D>> a, const VecArg<Vec<D>> b);  // <=
template <class D>
Mask<D> GT(const VecArg<Vec<D>> a, const VecArg<Vec<D>> b);  // >
template <class D>
Mask<D> GE(const VecArg<Vec<D>> a, const VecArg<Vec<D>> b);  // >=

// 使用示例
const ScalableTag<float> df;
auto a = Load(df, &array[0]);
auto b = Load(df, &array[N]);
auto mask = LT(a, b);  // [a[0]<b[0], a[1]<b[1], ...]
```

### 掩码类型和操作

```cpp
// 掩码类型（Vec<D> 的包装）
template <class D>
using Mask = /* 平台特定类型 */;

// 检查掩码是否全为真
template <class D>
bool AllTrue(D d, const MaskArg<Mask<D>> mask);

// 检查掩码是否有真值
template <class D>
bool AnyTrue(D d, const MaskArg<Mask<D>> mask);

// 检查掩码是否全为假
template <class D>
bool AllFalse(D d, const MaskArg<Mask<D>> mask);

// 计算掩码中真值的数量
template <class D>
size_t CountTrue(D d, const MaskArg<Mask<D>> mask);
```

### 掩码与向量的转换

```cpp
// 从掩码创建向量（真值为全 1，假值为全 0）
template <class D>
Vec<D> VecFromMask(D d, const MaskArg<Mask<D>> mask);

// 从向量创建掩码（非零为真，零为假）
template <class D>
Mask<D> MaskFromVec(D d, const VecArg<Vec<D>> v);
```

---

## 5.2 条件选择

### IfThenElse

```cpp
// 如果 mask 为真，选择 if_true，否则选择 if_false
template <class D>
Vec<D> IfThenElse(const MaskArg<Mask<D>> mask,
                  const VecArg<Vec<D>> if_true,
                  const VecArg<Vec<D>> if_false);

// 使用示例：条件赋值
auto a = Load(df, &array[0]);
auto b = Load(df, &array[N]);
auto mask = LT(a, b);
auto result = IfThenElse(mask, a, b);  // min(a, b)
```

### IfThenElseZero

```cpp
// 如果 mask 为真，选择 v，否则选择零
template <class D>
Vec<D> IfThenElseZero(const MaskArg<Mask<D>> mask,
                       const VecArg<Vec<D>> v);

// 使用示例：条件置零
auto mask = GT(v, Set(df, 0.0f));  // v > 0
auto v_pos = IfThenElseZero(mask, v);  // 只保留正数
```

### IfNegativeThenElse

```cpp
// 如果 v 为负，选择 neg_val，否则选择 pos_val
template <class D>
Vec<D> IfNegativeThenElse(const VecArg<Vec<D>> v,
                         const VecArg<Vec<D>> neg_val,
                         const VecArg<Vec<D>> pos_val);

// 使用示例：根据符号选择
auto sign_mask = LT(v, Zero(df));
auto result = IfNegativeThenElse(v, neg_value, pos_value);
```

---

## 5.3 逻辑运算

### And / Or / Xor / Not

```cpp
// 按位与（掩码）
template <class D>
Mask<D> And(const MaskArg<Mask<D>> a, const MaskArg<Mask<D>> b);

// 按位或（掩码）
template <class D>
Mask<D> Or(const MaskArg<Mask<D>> a, const MaskArg<Mask<D>> b);

// 按位异或（掩码）
template <class D>
Mask<D> Xor(const MaskArg<Mask<D>> a, const MaskArg<Mask<D>> b);

// 按位非（掩码）
template <class D>
Mask<D> Not(const MaskArg<Mask<D>> a);

// 向量版本的按位运算
template <class D>
Vec<D> And(const VecArg<Vec<D>> a, const VecArg<Vec<D>> b);
template <class D>
Vec<D> Or(const VecArg<Vec<D>> a, const VecArg<Vec<D>> b);
template <class D>
Vec<D> Xor(const VecArg<Vec<D>> a, const VecArg<Vec<D>> b);
template <class D>
Vec<D> Not(const VecArg<Vec<D>> a);

// 使用示例
auto mask_a = LT(a, threshold_a);
auto mask_b = GT(b, threshold_b);
auto mask_combined = And(mask_a, mask_b);  // a < t1 && b > t2
auto mask_either = Or(mask_a, mask_b);   // a < t1 || b > t2
```

---

## 5.4 移位操作

### ShiftLeft / ShiftRight

```cpp
// 逻辑左移（仅整数）
template <int k, class D>
Vec<D> ShiftLeft(const VecArg<Vec<D>> v);

// 逻辑右移（仅整数）
template <int k, class D>
Vec<D> ShiftRight(const VecArg<Vec<D>> v);

// 算术右移（仅整数，保留符号位）
template <int k, class D>
Vec<D> ShiftRightArithmetic(const VecArg<Vec<D>> v);

// 使用示例
const ScalableTag<uint8_t> du8;
const ScalableTag<int32_t> di32;

auto v_u8 = Load(du8, data);
auto v_shifted = ShiftLeft<2>(v_u8);  // 每通道左移 2 位

auto v_i32 = Load(di32, data);
auto v_div4 = ShiftRightArithmetic<2>(v_i32);  // 有符号除以 4
```

### RotateRight

```cpp
// 循环右移（仅整数）
template <int k, class D>
Vec<D> RotateRight(const VecArg<Vec<D>> v);

// 使用示例
auto v_rotated = RotateRight<8>(v_u8);  // 循环右移 8 位
```

### Shl（运行时移位）

```cpp
// 按每个通道的值进行移位
template <class D>
Vec<D> Shl(const VecArg<Vec<D>> v, const VecArg<Vec<D>> count);
template <class D>
Vec<D> Shr(const VecArg<Vec<D>> v, const VecArg<Vec<D>> count);

// 使用示例
auto shift_counts = Load(du8, shifts);
auto v_shifted = Shl(v, shift_counts);  // 每通道按各自 count 移位
```

---

## 5.5 最小和最大

### Min / Max

```cpp
// 逐通道最小值
template <class D>
Vec<D> Min(const VecArg<Vec<D>> a, const VecArg<Vec<D>> b);

// 逐通道最大值
template <class D>
Vec<D> Max(const VecArg<Vec<D>> a, const VecArg<Vec<D>> b);

// 使用示例：clamp 实现
auto v = Load(df, data);
auto clamped = Max(Min(v, upper_bound), lower_bound);
Store(clamped, df, output);
```

### Min128 / Max128

```cpp
// 128位块的最小/最大（用于跨通道操作）
template <class D>
Vec<D> Min128(const VecArg<Vec<D>> a, const VecArg<Vec<D>> b);
template <class D>
Vec<D> Max128(const VecArg<Vec<D>> a, const VecArg<Vec<D>> b);
```

---

## 5.6 规约操作

规约操作将多个通道值合并为单个值。

### SumOfLanes

```cpp
// 所有通道求和
template <class D>
Vec<D> SumOfLanes(D d, const VecArg<Vec<D>> v);

// 使用示例：计算数组总和
template <typename T>
T SumArray(const T* data, size_t count) {
  const ScalableTag<T> d;
  const size_t N = Lanes(d);

  Vec<decltype(d)> sum = Zero(d);

  for (size_t i = 0; i + N <= count; i += N) {
    auto v = Load(d, &data[i]);
    sum = Add(sum, v);
  }

  sum = SumOfLanes(d, sum);  // 规约到单个值
  return GetLane(sum, 0);  // 提取第 0 个通道
}
```

### MinOfLanes / MaxOfLanes

```cpp
// 所有通道的最小值
template <class D>
Vec<D> MinOfLanes(D d, const VecArg<Vec<D>> v);

// 所有通道的最大值
template <class D>
Vec<D> MaxOfLanes(D d, const VecArg<Vec<D>> v);
```

### Horizontal Add

```cpp
// 水平求和：相邻通道相加
template <class D>
Vec<D> AddAdjacent(D d, const VecArg<Vec<D>> v);

// 使用示例：[a,b,c,d,e,f,g,h] -> [a+b, c+d, e+f, g+h]
auto v = Load(df, data);
auto hadd = AddAdjacent(df, v);
```

---

## 5.7 提取通道

### GetLane

```cpp
// 提取单个通道的值
template <class D>
TFromD<D> GetLane(const VecArg<Vec<D>> v, size_t i);

// 使用示例
auto v = Load(df, data);
float first = GetLane(v, 0);
float second = GetLane(v, 1);
```

### LowerHalf / UpperHalf

```cpp
// 提取向量的下半部分
template <class DHalf>
Vec<DHalf> LowerHalf(DHalf d, const VecArg<Vec<Rebind<typename DHalf::T, decltype(d)::Twice>>> v);

// 提取向量的上半部分
template <class DHalf>
Vec<DHalf> UpperHalf(DHalf d, const VecArg<Vec<Rebind<typename DHalf::T, decltype(d)::Twice>>> v);

// 使用示例
const ScalableTag<float> df;
const ScalableTag<float, -1> dhalf;  // 半向量

auto v_full = Load(df, data);
auto v_low = LowerHalf(dhalf, v_full);  // 下半部分
auto v_high = UpperHalf(dhalf, v_full); // 上半部分
```

---

## 5.8 Combine / Concat

### Combine

```cpp
// 组合两个半向量为一个全向量
template <class D>
Vec<D> Combine(D d, const VecArg<Vec<typename D::Half>> low,
              const VecArg<Vec<typename D::Half>> high);

// 使用示例
auto v_combined = Combine(df, v_low, v_high);
Store(v_combined, df, output);
```

### Concat

```cpp
// 拼接向量（适用于可变长度向量）
template <class D>
Vec<D> Concat(D d,
             const VecArg<Vec<Rebind<TFromD<D>, typename D::Half>>> hi,
             const VecArg<Vec<Rebind<TFromD<D>, typename D::Half>>> lo);

// 使用示例
auto v_concat = Concat(df, v_high, v_low);
```

---

## 5.9 Shuffle

### CombineShiftRightBytes

```cpp
// 按字节平移并组合（用于跨通道对齐）
template <class D>
Vec<D> CombineShiftRightBytes(D d,
                                  const VecArg<Vec<D>> hi,
                                  const VecArg<Vec<D>> lo,
                                  size_t bytes);

// 使用示例：从两个相邻向量创建跨越边界的数据
auto v = CombineShiftRightBytes(df, data[i], data[i+N], offset);
```

### TableLookupBytes

```cpp
// 字节查表（SSE4.1 的 _mm_shuffle_epi8）
template <class D>
Vec<D> TableLookupBytes(const VecArg<Vec<D>> indices,
                       const VecArg<Vec<D>> table);

template <class D>
Vec<D> TableLookupBytesOr0(const VecArg<Vec<D>> indices,
                            const VecArg<Vec<D>> table);

// 使用示例：使用查表转换
auto v_indices = Load(du8, indices);
auto v_table = Load(du8, table);
auto v_result = TableLookupBytes(v_indices, v_table);
```

---

## 5.10 填充操作

### DupEven / DupOdd

```cpp
// 复制偶数通道：[a,b,c,d] -> [a,a,c,c]
template <class D>
Vec<D> DupEven(D d, const VecArg<Vec<D>> v);

// 复制奇数通道：[a,b,c,d] -> [b,b,d,d]
template <class D>
Vec<D> DupOdd(D d, const VecArg<Vec<D>> v);

// 使用示例
auto v = Load(df, data);
auto even_duplicated = DupEven(df, v);
auto odd_duplicated = DupOdd(df, v);
```

### InterleaveLower / InterleaveUpper

```cpp
// 交叉下半部分：[a0,a1,a2,a3] [b0,b1,b2,b3] -> [a0,b0,a1,b1]
template <class D>
Vec<D> InterleaveLower(D d, const VecArg<Vec<D>> a,
                       const VecArg<Vec<D>> b);

// 交叉上半部分：[a0,a1,a2,a3] [b0,b1,b2,b3] -> [a2,b2,a3,b3]
template <class D>
Vec<D> InterleaveUpper(D d, const VecArg<Vec<D>> a,
                       const VecArg<Vec<D>> b);

// 使用示例（矩阵转置准备）
auto v_low = InterleaveLower(df, a, b);
auto v_high = InterleaveUpper(df, a, b);
```

---

## 5.11 完整示例：查找最小元素

```cpp
#include "hwy/highway.h"
namespace hn = hwy::HWY_NAMESPACE;

// 在数组中查找最小值
template <typename T>
T FindMinimum(const T* data, size_t count) {
  const ScalableTag<T> d;
  const size_t N = Lanes(d);

  Vec<decltype(d)> min_vec = LoadU(d, &data[0]);

  for (size_t i = N; i + N <= count; i += N) {
    auto v = Load(d, &data[i]);
    min_vec = Min(min_vec, v);  // 逐通道比较
  }

  // 处理剩余元素
  if (count % N != 0) {
    auto v = LoadU(d, &data[count - N]);
    min_vec = Min(min_vec, v);
  }

  // 规约到单个值
  min_vec = MinOfLanes(d, min_vec);
  return GetLane(min_vec, 0);
}

// 导出
#if HWY_ONCE
namespace hwy {
template <typename T>
HWY_EXPORT(FindMinimum);
}  // namespace hwy
#endif
```

---

## 5.12 完整示例：条件操作

```cpp
// ReLU 激活函数：max(0, x)
template <typename T>
void ReLU(const T* input, T* output, size_t count) {
  const ScalableTag<T> d;
  const size_t N = Lanes(d);
  const T zero_val = T{0};
  auto zero_vec = Set(d, zero_val);

  for (size_t i = 0; i + N <= count; i += N) {
    auto v = Load(d, &input[i]);
    auto mask = GT(v, zero_vec);  // v > 0
    v = IfThenElseZero(mask, v);  // 负值置零
    Store(v, d, &output[i]);
  }

  // 处理剩余元素
  for (size_t i = (count / N) * N; i < count; ++i) {
    output[i] = input[i] > zero_val ? input[i] : zero_val;
  }
}

// 使用 Max 的替代实现（更简洁）
template <typename T>
void ReLU_V2(const T* input, T* output, size_t count) {
  const ScalableTag<T> d;
  const size_t N = Lanes(d);
  auto zero_vec = Zero(d);

  for (size_t i = 0; i + N <= count; i += N) {
    auto v = Load(d, &input[i]);
    v = Max(v, zero_vec);  // max(0, x)
    Store(v, d, &output[i]);
  }
}
```

---

## 本日总结

| 操作类别 | 函数 | 说明 |
|----------|------|------|
| **比较** | `EQ()`, `NE()`, `LT()`, `LE()`, `GT()`, `GE()` | 返回掩码 |
| **掩码操作** | `AllTrue()`, `AnyTrue()`, `CountTrue()` | 检查/统计掩码 |
| **条件选择** | `IfThenElse()`, `IfThenElseZero()`, `IfNegativeThenElse()` | 条件赋值 |
| **逻辑运算** | `And()`, `Or()`, `Xor()`, `Not()` | 按位运算 |
| **移位** | `ShiftLeft()`, `ShiftRight()`, `RotateRight()`, `Shl()` | 位移操作 |
| **最小/最大** | `Min()`, `Max()`, `Min128()`, `Max128()` | 逐通道极值 |
| **规约** | `SumOfLanes()`, `MinOfLanes()`, `MaxOfLanes()` | 合并通道 |
| **提取/组合** | `GetLane()`, `LowerHalf()`, `UpperHalf()`, `Combine()` | 向量拆分组合 |
| **重排** | `DupEven()`, `DupOdd()`, `InterleaveLower()` | 通道重排 |

---

## 练习

1. 实现查找数组最大值的函数
2. 实现 ReLU 的反向传播导数：如果 x > 0，导数为 1，否则为 0
3. 使用 `IfThenElse` 实现三元运算符：`result = a ? b : c`（逐通道）
4. 使用 `ShiftRight` 和 `And` 提取 float 的符号位

---

## 下一步

明天我们将学习静态分发机制，理解如何为特定目标编译代码。
