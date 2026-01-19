# Day 2: 核心基础 - base.h 和工具函数

## 课程目标

今天我们将深入分析 `base.h` 的实现，理解 Highway 的基础工具函数、类型特性和编译器抽象。

---

## 2.1 base.h 的角色

`base.h` 是 Highway 的**核心基础层**，它提供：
- 平台无关的实用函数
- 编译器抽象和特性检测
- 类型特性和工具
- 对齐常量和检查

**关键特性**：`base.h` **不包含任何 SIMD 操作**，这使得它可以被任何代码使用，即使不涉及 SIMD。

---

## 2.2 版本信息

```cpp
// base.h 中的版本定义
#define HWY_MAJOR 1
#define HWY_MINOR 3
#define HWY_PATCH 0

// 可以在运行时检查
inline int Version() { return 10300; }
```

版本必须与 `CMakeLists.txt` 中的版本号同步。

---

## 2.3 编译器抽象

### 检测编译器

```cpp
// 检测编译器类型
#define HWY_COMPILER_GCC     1
#define HWY_COMPILER_CLANG    1
#define HWY_COMPILER_MSVC     1
#define HWY_COMPILER_ICC      1

// 检测编译器版本
#define HWY_COMPILER_GCC_ACTUAL  __GNUC__
#define HWY_COMPILER_CLANG_ACTUAL __clang_major__
```

### 检测操作系统

```cpp
#define HWY_OS_LINUX   1
#define HWY_OS_MAC     1
#define HWY_OS_WIN     1
#define HWY_OS_ANDROID 1
```

### 检测架构

```cpp
#define HWY_ARCH_X86       1
#define HWY_ARCH_ARM        1
#define HWY_ARCH_ARM_A64   1
#define HWY_ARCH_RISCV     1
#define HWY_ARCH_PPC       1
```

### 架构判断的技巧

```cpp
// 判断是否为 64 位
#define HWY_ARCH_X86_64  (HWY_ARCH_X86 && defined(__x86_64__))
#define HWY_ARCH_ARM_A64  (HWY_ARCH_ARM && defined(__aarch64__))

// 判断是否为小端
#define HWY_IS_LITTLE_ENDIAN 1  // 所有现代架构都是小端
```

---

## 2.4 类型特性和工具

### 基本类型特性

```cpp
// 移除 const 和引用
template <class T>
struct RemoveConst { using type = T; };
template <class T>
struct RemoveConst<const T> { using type = T; };

// 判断是否为有符号类型
template <class T>
struct IsSignedT {
  static constexpr bool value = T(-1) < T(0);
};

// 判断是否为浮点类型
template <class T>
struct IsFloatT : std::is_floating_point<T> {};

// 特殊浮点类型（float16_t, bfloat16_t）
template <class T>
struct IsSpecialFloat {
  static constexpr bool value =
      IsSame<T, float16_t>() || IsSame<T, bfloat16_t>();
};
```

### MakeSigned / MakeUnsigned

```cpp
// 将类型转换为有符号版本
template <class T>
using MakeSigned = typename detail::MakeSignedT<T>::type;

// 将类型转换为无符号版本
template <class T>
using MakeUnsigned = typename detail::MakeUnsignedT<T>::type;

// 使用示例
static_assert(IsSame<MakeSigned<uint32_t>, int32_t>());
static_assert(IsSame<MakeUnsigned<int16_t>, uint16_t>());
```

### IsSame SFINAE 工具

```cpp
// 类型相等检查（用于 SFINAE）
template <class T, class U>
struct IsSame {
  static constexpr bool value = false;
};

template <class T>
struct IsSame<T, T> {
  static constexpr bool value = true;
};

// EnableIf - SFINAE 条件
template <bool B, class T = void>
struct EnableIf {};

template <class T>
struct EnableIf<true, T> {
  using type = T;
};

// 使用示例
template <class T, typename EnableIf<IsFloat<T>()>* = nullptr>
void OnlyFloats(T value) { /* ... */ }
```

---

## 2.5 对齐操作

### 基本对齐检查

```cpp
// 对齐常量 - 至少为缓存行大小
#define HWY_ALIGNMENT 128

// 检查指针对齐
template <typename T>
HWY_API constexpr bool IsAligned(T* ptr,
                                size_t align = HWY_ALIGNMENT) {
  return reinterpret_cast<uintptr_t>(ptr) % align == 0;
}

// 向上对齐到 2 的幂
template <typename T>
constexpr T AlignTo(T x, T align) {
  static_assert((align & (align - 1)) == 0, "align must be power of 2");
  return (x + align - 1) & ~(align - 1);
}

// 使用示例
float* aligned_data = /* ... */;
if (IsAligned(aligned_data)) {
  // 数据已对齐，可以使用非对齐加载
}
```

### 指针类型转换

```cpp
// 安全地将指针转换为uintptr_t（兼容 MSVC）
template <typename T>
HWY_INLINE uintptr_t PointerToSignedInt(T* p) {
  static_assert(sizeof(T*) <= sizeof(uintptr_t), "T* too large");
  return reinterpret_cast<uintptr_t>(p);
}

// 安全地转换回来
template <typename T>
HWY_INLINE T* SignedIntToPointer(uintptr_t p) {
  return reinterpret_cast<T*>(static_cast<intptr_t>(p));
}
```

---

## 2.6 位操作

### PopCount（人口计数）

```cpp
// 计算设置位的数量
template <typename T>
HWY_INLINE size_t PopCount(T x) {
  static_assert(IsUnsigned<T>() || IsSame<T, intptr_t>(),
                "PopCount requires unsigned or intptr_t");

#if HWY_COMPILER_GCC || HWY_COMPILER_CLANG
  // GCC/Clang 内置函数
  return static_cast<size_t>(__builtin_popcountll(x));
#else
  // 通用实现
  size_t count = 0;
  while (x) {
    count += x & 1;
    x >>= 1;
  }
  return count;
#endif
}

// 使用示例
uint64_t mask = 0b10110100;
size_t ones = PopCount(mask);  // ones = 4
```

### 查找最低设置位

```cpp
// 计算从最低位到第一个设置位的位数（ctz = count trailing zeros）
template <typename T>
HWY_INLINE size_t Num0BitsBelowLS1Bit_Nonzero64(T x) {
  static_assert(IsUnsigned<T>(), "requires unsigned");

#if HWY_COMPILER_GCC || HWY_COMPILER_CLANG
  return static_cast<size_t>(__builtin_ctzll(x));
#else
  size_t n = 0;
  while ((x & 1) == 0) {
    x >>= 1;
    ++n;
  }
  return n;
#endif
}
```

---

## 2.7 对数操作

```cpp
// 计算以 2 为底的对数的向上取整
template <typename T>
HWY_INLINE constexpr T CeilLog2(T n) {
  return n <= 1 ? 0 : 1 + CeilLog2(n >> 1);
}

// 计算以 2 为底的对数的向下取整
template <typename T>
HWY_INLINE constexpr T FloorLog2(T n) {
  return n <= 1 ? 0 : 1 + FloorLog2(n >> 1);
}

// 使用示例
constexpr size_t pow2 = CeilLog2(17);  // pow2 = 5 (2^5 = 32 >= 17)
constexpr size_t lg = FloorLog2(16);     // lg = 4 (2^4 = 16)
```

---

## 2.8 限幅函数

```cpp
// 将值限制在 [a, b] 范围内
template <typename T>
HWY_INLINE constexpr T Clamp(T value, T a, T b) {
  return value < a ? a : (value > b ? b : value);
}

// 使用示例
int x = Clamp(x, 0, 100);  // 限制在 0-100 之间
```

---

## 2.9 特殊浮点类型

Highway 支持半精度浮点：

```cpp
// float16_t - IEEE 754 binary16
struct float16_t {
  uint16_t bits;
  // ... 构造函数和运算符
};

// bfloat16_t - Brain floating point
struct bfloat16_t {
  uint16_t bits;
  // ... 构造函数和运算符
};

// 检测类型
#define HWY_HAVE_SCALAR_F16_TYPE  0  // 如果编译器支持原生 __fp16
#define HWY_HAVE_SCALAR_BF16_TYPE 0  // 如果编译器支持原生 __bf16
```

### MakeFloat 特性

```cpp
// 将整数类型转换为对应的浮点类型
template <class T>
struct MakeFloatT;

template <>
struct MakeFloatT<uint8_t>  { using type = float; };
template <>
struct MakeFloatT<int16_t>  { using type = float; };
template <>
struct MakeFloatT<int32_t>  { using type = float; };
template <>
struct MakeFloatT<int64_t>  { using type = double; };

template <class T>
using MakeFloat = typename MakeFloatT<T>::type;
```

### MakeWide / MakeNarrow

```cpp
// 将 8/16 位类型转换为更宽的类型
template <class T>
struct MakeWideT;

template <>
struct MakeWideT<uint8_t>  { using type = uint16_t; };
template <>
struct MakeWideT<int8_t>   { using type = int16_t; };
template <>
struct MakeWideT<uint16_t> { using type = uint32_t; };
template <>
struct MakeWideT<int16_t>  { using type = int32_t; };

// 反向转换
template <class T>
struct MakeNarrowT {
  using type = T;  // 默认不转换
};
```

---

## 2.10 内存操作

### CopyBytes

```cpp
// 安全的字节拷贝（可能优化为 memcpy）
template <typename T>
HWY_INLINE void CopyBytes(const T* src, T* dst, size_t count) {
  memcpy(dst, src, count * sizeof(T));
}
```

### ZeroBytes

```cpp
// 将内存清零
template <typename T>
HWY_INLINE void ZeroBytes(T* dst, size_t bytes) {
  memset(dst, 0, bytes);
}

// 使用示例
float* buffer = /* ... */;
ZeroBytes(buffer, sizeof(float) * N);
```

### MemcpyBytes / MemmoveBytes

```cpp
// 包装 memcpy/memmove，防止参数顺序错误
HWY_INLINE void MemcpyBytes(void* dst, const void* src, size_t bytes) {
  memcpy(dst, src, bytes);
}

HWY_INLINE void MemmoveBytes(void* dst, const void* src, size_t bytes) {
  memmove(dst, src, bytes);  // 处理重叠区域
}
```

---

## 2.11 函数属性

### HWY_INLINE

```cpp
// 强制内联（但可能被编译器忽略）
#if HWY_COMPILER_MSVC
#define HWY_INLINE __forceinline
#else
#define HWY_INLINE __attribute__((always_inline)) inline
#endif
```

### HWY_NOINLINE

```cpp
// 防止内联
#if HWY_COMPILER_MSVC
#define HWY_NOINLINE __declspec(noinline)
#else
#define HWY_NOINLINE __attribute__((noinline))
#endif
```

### HWY_RESTRICT

```cpp
// 指针不别名提示
#define HWY_RESTRICT __restrict__
```

---

## 2.12 向量大小常量

```cpp
// 不同架构的向量大小的编译时常量
#if HWY_ARCH_X86
  // AVX-512: 512 位
  // AVX2: 256 位
  // SSE: 128 位
  #define HWY_MAX_BYTES 64
#elif HWY_ARCH_ARM && HWY_HAVE_SVE
  // SVE 可变长度，但有限制
  #define HWY_MAX_BYTES 2048
#elif HWY_ARCH_ARM
  // NEON: 128 位
  #define HWY_MAX_BYTES 128
#endif

// 计算类型的最大通道数
#define HWY_LANES(T) (HWY_MAX_BYTES / sizeof(T))

// 示例
// HWY_LANES(float32_t) = 64 / 4 = 16 (AVX-512)
// HWY_LANES(uint8_t)  = 64 / 1 = 64 (AVX-512)
```

---

## 2.13 调试和断言

```cpp
// Debug/Release 模式检测
#define HWY_IS_DEBUG_BUILD \
  (!defined(NDEBUG) && defined(_DEBUG))

// 断言
#if HWY_IS_DEBUG_BUILD
#define HWY_ASSERT(expr) assert(expr)
#define HWY_DASSERT(expr) assert(expr)
#else
#define HWY_ASSERT(expr) ((void)0)
#define HWY_DASSERT(expr) ((void)0)
#endif

// 终止程序
#define HWY_ABORT(msg) \
  do { \
    fprintf(stderr, "Error: %s\n", msg); \
    std::abort(); \
  } while (0)
```

---

## 本日总结

| 类别 | 重要函数/宏 |
|------|------------|
| **类型工具** | `IsSame<>`, `MakeSigned<>`, `MakeUnsigned<>`, `EnableIf<>` |
| **浮点工具** | `MakeFloat<>`, `MakeWide<>`, `float16_t`, `bfloat16_t` |
| **对齐操作** | `IsAligned()`, `AlignTo()`, `HWY_ALIGNMENT` |
| **位操作** | `PopCount()`, `CeilLog2()`, `FloorLog2()` |
| **内存操作** | `CopyBytes()`, `ZeroBytes()`, `MemcpyBytes()` |
| **编译器抽象** | `HWY_COMPILER_*`, `HWY_ARCH_*`, `HWY_OS_*` |
| **函数属性** | `HWY_INLINE`, `HWY_NOINLINE`, `HWY_RESTRICT` |

---

## 代码示例：使用 base.h 工具

```cpp
#include "hwy/base.h"

void ProcessArray(int32_t* data, size_t size) {
  // 检查对齐
  if (!hwy::IsAligned(data, hwy::HWY_ALIGNMENT)) {
    // 数据未对齐，可能需要处理
  }

  // 限幅值
  for (size_t i = 0; i < size; ++i) {
    data[i] = hwy::Clamp(data[i], 0, 1000);
  }

  // 计算需要分配的大小（向上对齐）
  size_t aligned_size = hwy::AlignTo(size, 64 / sizeof(int32_t));

  // 检查是否有符号类型
  constexpr bool is_signed = hwy::IsSigned<int32_t>::value;

  // 类型转换
  using UnsignedT = hwy::MakeUnsigned<int32_t>;  // uint32_t
  using FloatT = hwy::MakeFloat<int32_t>;      // float
}
```

---

## 下一步

明天我们将学习 Highway 的向量标签系统（ScalableTag、CappedTag、FixedTag），这是 Highway 类型系统的核心。
