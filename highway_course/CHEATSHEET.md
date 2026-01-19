# Highway SIMD 速查卡片 (Cheatsheet)

这是一页纸的快速参考，可以打印出来放在桌面上。

---

## 🎯 核心概念

### 标签类型 (Tags)

```cpp
ScalableTag<T>         // 完整向量（推荐）
CappedTag<T, N>       // 最多 N 通道
FixedTag<T, N>        // 恰好 N 通道
```

### 查询向量大小

```cpp
Lanes(d)              // 运行时通道数
MaxLanes(d)           // 编译时上界
MaxBytes(d)           // 最大字节数
```

---

## 📥 加载与存储

| 操作 | 用途 | 对齐要求 |
|-----|------|---------|
| `Load(d, p)` | 对齐加载 | 需要 HWY_ALIGNMENT |
| `LoadU(d, p)` | 未对齐加载 | 无 |
| `MaskedLoad(d, mask, p)` | 掩码加载 | 无 |
| `Store(v, d, p)` | 对齐存储 | 需要 HWY_ALIGNMENT |
| `StoreU(v, d, p)` | 未对齐存储 | 无 |
| `BlendedStore(v, mask, d, p)` | 掩码存储 | 无 |

---

## 🧮 算术操作

### 基本运算

```cpp
Add(a, b)             // a + b
Sub(a, b)             // a - b
Mul(a, b)             // a * b
Div(a, b)             // a / b (慢！)
MulAdd(a, b, c)       // a*b + c (FMA，推荐！)
Neg(a)                // -a
Abs(a)                // |a|
```

### 数学函数

```cpp
Sqrt(v)               // √v
Min(a, b)             // min(a, b)
Max(a, b)             // max(a, b)
Clamp(v, lo, hi)      // clamp(v, lo, hi)
```

---

## 🎭 比较与掩码

### 比较操作

```cpp
Eq(a, b)              // a == b
Lt(a, b)              // a < b
Le(a, b)              // a <= b
Gt(a, b)              // a > b
Ge(a, b)              // a >= b
```

### 掩码操作

```cpp
IfThenElse(mask, a, b)     // mask ? a : b
IfThenElseZero(mask, v)    // mask ? v : 0
FirstN(d, n)               // 前 n 个通道的掩码
And(mask1, mask2)          // 掩码 AND
Or(mask1, mask2)           // 掩码 OR
Not(mask)                  // 掩码取反
AnyTrue(d, mask)           // 任意为真？
AllTrue(d, mask)           // 全部为真？
CountTrue(d, mask)         // 计数为真的数量
FindFirstTrue(d, mask)     // 第一个真的索引
```

---

## 🔄 重排与混合

```cpp
Broadcast<N>(v)            // 广播第 N 个通道
Set(d, value)              // 所有通道设为 value
Zero(d)                    // 所有通道设为 0
Iota(d, start)             // [start, start+1, ...]
Reverse(d, v)              // 反转通道顺序
Shuffle01(v)               // 交换相邻对
TableLookupBytes(v, idx)   // 字节表查找
```

---

## ⬇️ 归约操作

```cpp
ReduceSum(d, v)            // 水平求和
ReduceMin(d, v)            // 水平最小值
ReduceMax(d, v)            // 水平最大值
GetLane(v)                 // 提取第 0 通道
ExtractLane(v, i)          // 提取第 i 通道
```

---

## 🔧 类型转换

```cpp
ConvertTo(d, v)            // 转换类型
PromoteTo(d, v)            // 提升精度（int8→int16）
DemoteTo(d, v)             // 降低精度（int16→int8）
BitCast(d, v)              // 位转换（重新解释）
```

---

## 🔁 常见循环模式

### 模式 1：填充数组

```cpp
// 假设 count 是 N 的倍数
for (size_t i = 0; i < count; i += N) {
  auto v = Load(d, &data[i]);
  v = Process(v);
  Store(v, d, &data[i]);
}
```

### 模式 2：掩码余数

```cpp
size_t i = 0;
for (; i + N <= count; i += N) {
  // 完整向量
}
if (i < count) {
  auto mask = FirstN(d, count - i);
  auto v = MaskedLoadOr(Zero(d), mask, d, &data[i]);
  BlendedStore(Process(v), mask, d, &data[i]);
}
```

### 模式 3：重叠余数

```cpp
for (; i + N <= count; i += N) {
  // 完整向量
}
if (i < count) {
  auto v = LoadU(d, &data[count - N]);
  StoreU(Process(v), d, &data[count - N]);
}
```

### 模式 4：双累加器

```cpp
auto sum0 = Zero(d), sum1 = Zero(d);
for (size_t i = 0; i + 2*N <= count; i += 2*N) {
  sum0 = MulAdd(LoadU(d, &a[i]), LoadU(d, &b[i]), sum0);
  sum1 = MulAdd(LoadU(d, &a[i+N]), LoadU(d, &b[i+N]), sum1);
}
return ReduceSum(d, Add(sum0, sum1));
```

---

## 🚀 动态调度

### 头文件模式

```cpp
// my_code-inl.h
#if defined(MY_CODE_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef MY_CODE_INL_H_
#undef MY_CODE_INL_H_
#else
#define MY_CODE_INL_H_
#endif

#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace my_project {
namespace HWY_NAMESPACE {
  // 你的代码
}  // namespace HWY_NAMESPACE
}  // namespace my_project
HWY_AFTER_NAMESPACE();

#endif  // MY_CODE_INL_H_
```

### 源文件模式

```cpp
// my_code.cc
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "my_code.cc"
#include "hwy/foreach_target.h"
#include "hwy/highway.h"

namespace HWY_NAMESPACE {
  void MyFunc() { /* ... */ }
}

#if HWY_ONCE
namespace my_project {
  HWY_EXPORT(MyFunc);

  void CallMyFunc() {
    HWY_DYNAMIC_DISPATCH(MyFunc)();
  }
}
#endif
```

---

## 🐛 调试技巧

### 打印向量

```cpp
#include "hwy/print-inl.h"

#if HWY_TARGET != HWY_SCALAR
Print(d, "label", v);
#endif
```

### 断言

```cpp
#include "hwy/tests/test_util.h"

HWY_ASSERT_VEC_EQ(d, expected, actual);
HWY_ASSERT(condition);
```

### 编译选项

```bash
# Debug 构建
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Release 构建
cmake .. -DCMAKE_BUILD_TYPE=Release

# 使用 ASan
cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=address -g"

# 查看汇编
g++ -S -masm=intel -O3 -mavx2 file.cc
```

---

## ⚡ 性能优化清单

### ✅ 必做

- [ ] 使用 `-O3` 编译
- [ ] 使用 `MulAdd` 代替 `Mul + Add`
- [ ] 对齐数据到 `HWY_ALIGNMENT`
- [ ] 使用 `LoadU` 处理未对齐数据
- [ ] 避免水平操作（`ReduceSum` 等）在热循环中

### ⚡ 高级

- [ ] 循环展开 2-4x
- [ ] 双/四累加器隐藏延迟
- [ ] 预取数据（`Prefetch`）
- [ ] SoA 布局代替 AoS
- [ ] 使用 `Transform` / `contrib` 算法

### ❌ 避免

- ❌ Debug 模式运行基准测试
- ❌ 频繁的 `GetLane` / `ExtractLane`
- ❌ 在动态调度中使用静态向量变量
- ❌ 假设固定向量大小（`Lanes(d)` 可能变化）
- ❌ 使用 `Div` 和 `Sqrt` 在热路径

---

## 📊 性能预期

| 操作 | 相对成本 | 说明 |
|-----|---------|------|
| Add/Sub/Mul | 1x | 基准 |
| FMA (MulAdd) | 1x | 与 Mul 相同，但做更多 |
| Div | 10-15x | 很慢！ |
| Sqrt | 10-15x | 很慢！ |
| Load (对齐) | 1-2x | L1 缓存 |
| Load (未对齐) | 1-3x | 取决于平台 |
| Gather | 15-20x | 非常慢！ |
| ReduceSum | 3-5x | 水平操作 |
| Compress | 1-10x | AVX-512 快，其他慢 |

---

## 🔗 快速链接

- **GitHub:** https://github.com/google/highway
- **快速参考:** `g3doc/quick_reference.md`
- **FAQ:** [FAQ.md](FAQ.md)
- **深度教程:** [01_ARCHITECTURE_DEEP_DIVE.md](01_ARCHITECTURE_DEEP_DIVE.md)

---

## 💡 一行技巧

```cpp
// 向量化条件
auto result = IfThenElse(Gt(v, threshold), v, Zero(d));

// 饱和加法
auto saturated = Min(Add(a, b), Set(d, MAX_VALUE));

// 交换相邻元素
auto swapped = Shuffle01(v);

// 快速清零
auto zeros = Zero(d);

// 计算绝对差
auto abs_diff = AbsDiff(a, b);

// 符号位
auto sign = SignBit(d);

// 复制符号
auto with_sign = CopySign(mag, sign);
```

---

**打印提示：** 设置为 A4 横向，缩小到适合页面

*Highway v1.3.0 | 2026-01-19*
