# Highway SIMD 深度学习指南 - 第一部分：架构深度剖析

## 目录

1. [核心架构概览](#核心架构概览)
2. [动态调度机制深度解析](#动态调度机制深度解析)
3. [向量类型系统](#向量类型系统)
4. [平台特定实现](#平台特定实现)
5. [性能优化技术](#性能优化技术)
6. [实战练习](#实战练习)

---

## 核心架构概览

### Highway 的设计哲学

Highway 的核心设计原则：

1. **零开销抽象**：标签（tags）是空结构体，编译时完全消失
2. **可移植优先**：Scalar fallback 保证代码在任何平台都能编译
3. **函数级别的目标特性**：使用 `__attribute__((target("sse2")))` 而非全局编译选项
4. **避免 ADL 陷阱**：显式使用命名空间别名 `namespace hn = hwy::HWY_NAMESPACE`
5. **大量使用宏**：为每个目标生成代码而不重复编写

### 关键文件架构图

```
hwy/
├── highway.h              # 主调度头文件，HWY_DYNAMIC_DISPATCH 宏
├── foreach_target.h       # 重复包含协调器（核心！）
├── targets.h             # 运行时目标选择，ChosenTarget 原子掩码
├── detect_targets.h      # 编译时目标检测（985 行！）
├── base.h                # 编译器/架构检测基础设施
├── cache_control.h       # 内存层次结构优化
├── aligned_allocator.h   # 缓存感知内存分配
│
├── ops/                  # 平台特定 SIMD 实现
│   ├── shared-inl.h      # 向量类型系统和标签（Simd<T,N,kPow2>）
│   ├── set_macros-inl.h  # 每目标宏定义
│   ├── generic_ops-inl.h # 目标无关的通用实现（8245 行）
│   ├── x86_128-inl.h     # SSE2/SSSE3/SSE4（14145 行）
│   ├── x86_256-inl.h     # AVX2（8996 行）
│   ├── x86_512-inl.h     # AVX-512（7669 行）
│   ├── arm_neon-inl.h    # ARM NEON（10647 行）
│   ├── arm_sve-inl.h     # ARM SVE（7090 行）
│   ├── rvv-inl.h         # RISC-V Vector（6599 行）
│   ├── ppc_vsx-inl.h     # PowerPC VSX（7490 行）
│   ├── wasm_128-inl.h    # WebAssembly SIMD（5994 行）
│   ├── emu128-inl.h      # 位操作模拟（2979 行）
│   └── scalar-inl.h      # 单通道回退（2174 行）
│
└── contrib/              # 高级 SIMD 实用工具
    ├── algo/             # 算法（copy, find, transform）
    ├── math/             # 数学函数（三角函数等）
    ├── sort/             # VQSort 向量化快速排序
    ├── dot/              # 点积实现
    └── ...
```

---

## 动态调度机制深度解析

### 1. 重新包含（Re-inclusion）机制

这是 Highway 最精妙的设计。让我们逐步解析：

#### 步骤 1：`foreach_target.h` 的工作原理

**文件：** `hwy/foreach_target.h:58-100`

```cpp
// foreach_target.h 迭代每个启用的目标
#if (HWY_TARGETS & HWY_SSE2) && (HWY_STATIC_TARGET != HWY_SSE2)
#undef HWY_TARGET
#define HWY_TARGET HWY_SSE2
#include HWY_TARGET_INCLUDE  // 重新包含你的 .cc 文件
#ifdef HWY_TARGET_TOGGLE
#undef HWY_TARGET_TOGGLE
#else
#define HWY_TARGET_TOGGLE
#endif
#endif

#if (HWY_TARGETS & HWY_AVX2) && (HWY_STATIC_TARGET != HWY_AVX2)
#undef HWY_TARGET
#define HWY_TARGET HWY_AVX2
#include HWY_TARGET_INCLUDE  // 再次重新包含！
#ifdef HWY_TARGET_TOGGLE
#undef HWY_TARGET_TOGGLE
#else
#define HWY_TARGET_TOGGLE
#endif
#endif
```

**关键洞察：**

- `HWY_TARGET_INCLUDE` 被定义为当前源文件路径（例如 `"hwy/examples/skeleton.cc"`）
- `foreach_target.h` 多次包含同一个 `.cc` 文件
- 每次包含时，`HWY_TARGET` 被设置为不同的值（SSE2, AVX2, NEON等）
- `HWY_TARGET_TOGGLE` 在每次迭代后翻转，允许 `-inl.h` 头文件重新可见

#### 步骤 2：切换包含保护（Toggle Include Guard）

**标准包含保护：**
```cpp
#ifndef MY_HEADER_H
#define MY_HEADER_H
// 内容
#endif
```
→ 第二次包含时被跳过

**Highway 的切换保护：**

**文件：** `hwy/examples/skeleton-inl.h:25-30`

```cpp
#if defined(HIGHWAY_HWY_EXAMPLES_SKELETON_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef HIGHWAY_HWY_EXAMPLES_SKELETON_INL_H_
#undef HIGHWAY_HWY_EXAMPLES_SKELETON_INL_H_  // 取消定义
#else
#define HIGHWAY_HWY_EXAMPLES_SKELETON_INL_H_  // 定义
#endif
// 头文件内容在这里
#endif
```

**工作原理：**

| 迭代 | `GUARD` 状态 | `TOGGLE` 状态 | 条件评估 | 结果 |
|------|-------------|--------------|----------|------|
| 1    | 未定义 (0)   | 未定义 (0)    | 0 == 0   | ✓ 包含内容 |
| 2    | 已定义 (1)   | 已定义 (1)    | 1 == 1   | ✓ 包含内容 |
| 3    | 未定义 (0)   | 未定义 (0)    | 0 == 0   | ✓ 包含内容 |

→ **每次迭代都重新可见！**

#### 步骤 3：每目标命名空间

**文件：** `hwy/highway.h:215-235`

```cpp
// highway.h 根据 HWY_TARGET 定义不同的命名空间
#if HWY_TARGET == HWY_SSE2
  #define HWY_NAMESPACE N_SSE2
#elif HWY_TARGET == HWY_AVX2
  #define HWY_NAMESPACE N_AVX2
#elif HWY_TARGET == HWY_NEON
  #define HWY_NAMESPACE N_NEON
// ... 其他目标
#endif
```

**你的代码：**

**文件：** `hwy/examples/skeleton.cc:42-93`

```cpp
namespace skeleton {
namespace HWY_NAMESPACE {  // 展开为 N_SSE2, N_AVX2, N_NEON 等

void FloorLog2(const uint8_t* values, size_t count, uint8_t* log2) {
  const hn::ScalableTag<float> df;
  // ... SIMD 代码
}

}  // namespace HWY_NAMESPACE
}  // namespace skeleton
```

**编译后得到：**
```cpp
namespace skeleton {
  namespace N_SSE2 { void FloorLog2(...) { /* SSE2 代码 */ } }
  namespace N_AVX2 { void FloorLog2(...) { /* AVX2 代码 */ } }
  namespace N_NEON { void FloorLog2(...) { /* NEON 代码 */ } }
}
```

→ **同一翻译单元中共存多个目标的代码！**

### 2. 导出和调度表

#### HWY_EXPORT 宏

**文件：** `hwy/highway.h:439-505`

```cpp
// 简化版本
#define HWY_EXPORT(FUNC_NAME)                                      \
  static void* HWY_CONCAT(FUNC_NAME, HighwayDispatchTable)[] = {  \
    &N_SSE2::FUNC_NAME,     /* 索引 0 */                           \
    &N_SSSE3::FUNC_NAME,    /* 索引 1 */                           \
    &N_SSE4::FUNC_NAME,     /* 索引 2 */                           \
    &N_AVX2::FUNC_NAME,     /* 索引 3 */                           \
    &N_AVX3::FUNC_NAME,     /* 索引 4 */                           \
    /* ... */                                                      \
  }
```

**实际使用：**

**文件：** `hwy/examples/skeleton.cc:106`

```cpp
#if HWY_ONCE  // 只编译一次（在最后一次迭代）
namespace skeleton {
  HWY_EXPORT(FloorLog2);  // 创建函数指针数组
}
#endif
```

#### HWY_DYNAMIC_DISPATCH 宏

**文件：** `hwy/highway.h:607-657`

```cpp
// 简化版本
#define HWY_DYNAMIC_DISPATCH(FUNC_NAME) (                          \
  HWY_CONCAT(FUNC_NAME, HighwayDispatchTable)[                    \
    hwy::GetChosenTargetIndex()  // 运行时查找最佳目标             \
  ]                                                                \
)
```

**实际使用：**

**文件：** `hwy/examples/skeleton.cc:117`

```cpp
void CallFloorLog2(const uint8_t* in, size_t count, uint8_t* out) {
  return HWY_DYNAMIC_DISPATCH(FloorLog2)(in, count, out);
  // 展开为：
  // FloorLog2HighwayDispatchTable[GetChosenTargetIndex()](in, count, out);
}
```

### 3. 运行时目标选择

#### ChosenTarget 结构

**文件：** `hwy/targets.h:341-381`

```cpp
struct ChosenTarget {
  using Mask = uint64_t;

  static HWY_INLINE Mask Get() {
    return mask_.load(std::memory_order_relaxed);
  }

  static HWY_INLINE size_t GetIndex() {
    const Mask mask = Get();
    // 计算最低设置位下方的零位数
    return hwy::Num0BitsBelowLS1Bit_Nonzero64(mask);
  }

 private:
  static std::atomic<Mask> mask_;
};
```

#### 目标位布局（62 位掩码）

**文件：** `hwy/targets.h:195-334`

```
位 0-14:  x86 (SSE2=14, AVX2=9, AVX3=8, AVX3_SPR=4)
位 15-29: ARM (NEON=28, SVE=24, SVE2=23, SVE2_128=18)
位 37:    RISC-V (RVV)
位 40-41: LoongArch (LASX, LSX)
位 47-51: PPC/S390X
位 58-59: WASM
位 61-62: 模拟 (EMU128, SCALAR)
```

**索引计算示例：**

```cpp
// 假设 CPU 支持 AVX2 (位 9) 和 SSE4 (位 11)
Mask chosen = 0b...10100000000;  // 位 9 和 11 设置
//                      ^
//                      最低设置位

GetIndex() → Num0BitsBelowLS1Bit_Nonzero64(chosen) → 9

// 索引 9 → 调度表[9] → &N_AVX2::FloorLog2
```

**关键优化：** 使用位操作（`__builtin_ctzll`）的 O(1) 查找，而非线性搜索。

---

## 向量类型系统

### Simd<T, N, kPow2> 模板

**文件：** `hwy/ops/shared-inl.h:213-270`

这是 Highway 类型系统的核心：

```cpp
template <typename Lane, size_t N, int kPow2>
struct Simd {
  using T = Lane;

 private:
  // N 的 20 位是"整数"部分（2 的幂）
  static constexpr size_t kWhole = N & 0xFFFFF;

  // N 的高位是"分数"部分（用于 Rebind）
  static constexpr int kFrac = static_cast<int>(N >> 20);

  // 静态断言验证
  static_assert(kFrac == 0 || kWhole == 1, "If frac, whole must be 1");
  static_assert((kWhole & (kWhole - 1)) == 0, "kWhole 必须是 2 的幂");
  static_assert(kPow2 >= HWY_MIN_POW2, "忘记 kPow2 递归终止符了吗？");

 public:
  // 通道数的上界（对于 !HWY_HAVE_SCALABLE 是精确的）
  static constexpr size_t kPrivateLanes =
      HWY_MAX(1, detail::ScaleByPower(kWhole, kPow2 - kFrac));

  constexpr size_t MaxLanes() const { return kPrivateLanes; }
  constexpr size_t MaxBytes() const { return kPrivateLanes * sizeof(Lane); }
  constexpr int Pow2() const { return kPow2; }
};
```

### 参数详解

#### `Lane` - 通道类型

支持的类型：
- **整数：** `uint8_t`, `int16_t`, `uint32_t`, `int64_t` 等
- **浮点：** `float`, `double`
- **特殊浮点：** `float16_t`, `bfloat16_t`

#### `N` - 最大通道数（带分数编码）

**正常情况（kFrac == 0）：**
```cpp
Simd<float, 4, 0>   // 4 个浮点通道
Simd<int32_t, 8, 0> // 8 个 int32 通道
```

**分数情况（kFrac != 0）：**

当你 Rebind 到更大的类型时出现：

```cpp
// 开始：1 个 uint8_t 通道
Simd<uint8_t, 1, 0>

// Rebind 到 uint32_t（4 倍大小）
// RVV 需要 kPow2 = 2（LMUL 比率）
// 但通道数应该是 1/4
Simd<uint32_t, 0x200001, 2>
//             ^^^^^^^^
//             kFrac=2（位 21-22）
//             kWhole=1（位 0-19）
//
// MaxLanes = ScaleByPower(1, 2 - 2) = ScaleByPower(1, 0) = 1 ✓
```

**为什么需要这个？** RVV 的 LMUL（寄存器组）必须在类型编码中，但我们想要保持相同的 `MaxLanes()`。

#### `kPow2` - 缩放因子

```cpp
kPow2 = -2  → 1/4 向量（MaxLanes / 4）
kPow2 = -1  → 1/2 向量（MaxLanes / 2）
kPow2 =  0  → 完整向量（MaxLanes）      ← 最常见
kPow2 = +1  → 2x 组合向量（MaxLanes × 2）
kPow2 = +2  → 4x 组合向量（MaxLanes × 4）
```

**范围：**
- `HWY_MIN_POW2` 到 `HWY_MAX_POW2`
- 典型：`-3` 到 `+3`（RVV/SVE）或 `-8` 到 `+3`（x86）

### 标签别名

**文件：** `hwy/ops/shared-inl.h:383-423`

#### ScalableTag<T, kPow2=0>

```cpp
template <typename T, int kPow2 = 0>
using ScalableTag = Simd<T, HWY_LANES(T), HWY_MIN(kPow2, HWY_MAX_POW2)>;

// 使用：
const ScalableTag<float> df;        // 完整的 float 向量
const ScalableTag<int32_t, -1> d32; // 半个 int32 向量
```

**何时使用：** 向量长度不可知的代码（RVV, SVE, 未来可扩展架构）

#### CappedTag<T, kLimit, kPow2=0>

```cpp
template <typename T, size_t kLimit, int kPow2 = 0>
using CappedTag = Simd<T, HWY_MIN(kLimit, HWY_LANES(T)), HWY_MIN(kPow2, HWY_MAX_POW2)>;

// 使用：
const CappedTag<uint8_t, 16> d8;  // 最多 16 个 uint8 通道
```

**何时使用：** 你想要限制向量大小（例如，固定大小的缓冲区）

#### FixedTag<T, kNumLanes>

```cpp
template <typename T, size_t kNumLanes>
using FixedTag = Simd<T, kNumLanes, 0>;

// 使用：
const FixedTag<float, 4> df4;  // 恰好 4 个 float（128 位）
```

**何时使用：** 需要精确通道数（危险：在小向量上失败！）

### 运行时 vs 编译时通道查询

```cpp
const ScalableTag<float> df;

// 编译时上界（constexpr）
constexpr size_t kMaxLanes = df.MaxLanes();  // 例如 16

// 运行时计数（RVV/SVE 上动态）
const size_t N = Lanes(df);  // 例如 4（实际硬件）

// 循环展开使用编译时值
for (size_t i = 0; i < count; i += Lanes(df)) {
  auto v = Load(df, &array[i]);
  // ...
}
```

**关键洞察：** `Lanes(df)` 在可扩展目标上调用内在函数（`vsetvli` 用于 RVV），在固定目标上返回 constexpr。

---

## 平台特定实现

### Vec<T> 包装器

每个目标定义自己的 `Vec` 模板来包装内在类型：

#### x86 (SSE/AVX)

**文件：** `hwy/ops/x86_128-inl.h:136-169`

```cpp
template <typename T, size_t N = 16 / sizeof(T)>
class Vec128 {
  using Raw = typename detail::Raw128<T>::type;
  // Raw = __m128i  (整数)
  //     = __m128   (float)
  //     = __m128d  (double)

  Raw raw;

 public:
  HWY_INLINE Vec128() = default;
  HWY_INLINE Vec128(const Vec128& other) = default;
  HWY_INLINE Vec128& operator=(const Vec128& other) = default;

  // 复合运算符
  HWY_INLINE Vec128& operator*=(const Vec128 other) {
    return *this = (*this * other);
  }
  HWY_INLINE Vec128& operator/=(const Vec128 other) {
    return *this = (*this / other);
  }
  // ... 其他
};

// 部分向量的别名
template <typename T> using Vec64  = Vec128<T, 8 / sizeof(T)>;
template <typename T> using Vec32  = Vec128<T, 4 / sizeof(T)>;
template <typename T> using Vec16  = Vec128<T, 2 / sizeof(T)>;
```

#### ARM NEON

**文件：** `hwy/ops/arm_neon-inl.h`

```cpp
template <typename T, size_t N = 16 / sizeof(T)>
struct Vec128 {
  using Raw = typename detail::Raw128<T>::type;
  // Raw = uint8x16_t, int32x4_t, float32x4_t 等

  Raw raw;
};
```

#### Scalar（回退）

**文件：** `hwy/ops/scalar-inl.h`

```cpp
template <typename T>
struct Vec1 {
  T raw;  // 只有一个通道！
};

template <typename T>
struct Mask1 {
  MakeUnsigned<T> bits;  // 0x00...00 或 0xFF...FF
};
```

### 操作示例：Add

每个目标实现相同的操作：

#### x86 SSE2

```cpp
HWY_API Vec128<float> Add(Vec128<float> a, Vec128<float> b) {
  return Vec128<float>{_mm_add_ps(a.raw, b.raw)};
}

HWY_API Vec128<int32_t> Add(Vec128<int32_t> a, Vec128<int32_t> b) {
  return Vec128<int32_t>{_mm_add_epi32(a.raw, b.raw)};
}
```

#### ARM NEON

```cpp
HWY_API Vec128<float> Add(Vec128<float> a, Vec128<float> b) {
  return Vec128<float>{vaddq_f32(a.raw, b.raw)};
}

HWY_API Vec128<int32_t> Add(Vec128<int32_t> a, Vec128<int32_t> b) {
  return Vec128<int32_t>{vaddq_s32(a.raw, b.raw)};
}
```

#### Scalar

```cpp
template <typename T>
HWY_API Vec1<T> Add(Vec1<T> a, Vec1<T> b) {
  return Vec1<T>{static_cast<T>(a.raw + b.raw)};
}
```

**关键洞察：** 相同的 API (`Add`)，不同的实现，通过命名空间隔离（`N_SSE2::Add`, `N_NEON::Add`）。

---

## 性能优化技术

### 1. 缓存控制

**文件：** `hwy/cache_control.h`

#### Prefetch（预取）

```cpp
template <typename T>
HWY_INLINE HWY_ATTR_CACHE void Prefetch(const T* p) {
#if HWY_ARCH_X86
  _mm_prefetch(reinterpret_cast<const char*>(p), _MM_HINT_T0);
#elif HWY_COMPILER_GCC || HWY_COMPILER_CLANGCL
  __builtin_prefetch(p, /*write=*/0, /*hint=*/3);
#endif
}
```

**使用场景：**
```cpp
for (size_t i = 0; i < count; i += N) {
  Prefetch(&array[i + N * 4]);  // 提前 4 次迭代预取
  auto v = Load(d, &array[i]);
  // 处理 v ...
}
```

**为什么有效：** 隐藏内存延迟（~100 周期）通过提前启动缓存行获取。

#### StreamCacheLine（流式写入）

**文件：** `hwy/cache_control.h:72-83`

```cpp
static HWY_INLINE void StreamCacheLine(const uint64_t* HWY_RESTRICT from,
                                       uint64_t* HWY_RESTRICT to) {
  HWY_DASSERT(IsAligned(from));
  HWY_DASSERT(IsAligned(to));
#if HWY_COMPILER_CLANG
  for (size_t i = 0; i < HWY_ALIGNMENT / sizeof(uint64_t); ++i) {
    __builtin_nontemporal_store(from[i], to + i);  // 绕过缓存
  }
#else
  hwy::CopyBytes(from, to, HWY_ALIGNMENT);
#endif
}
```

**何时使用：** 写入不会再读取的大数据（避免缓存污染）。

**示例：** 内存拷贝大缓冲区到磁盘。

#### Pause（暂停自旋等待）

**文件：** `hwy/cache_control.h:125-139`

```cpp
HWY_INLINE HWY_ATTR_CACHE void Pause() {
#if HWY_ARCH_X86
  _mm_pause();                    // x86: 减少功耗 + 内存序违规惩罚
#elif HWY_ARCH_ARM_A64
  __yield();                      // ARM: 让出 SMT 线程
#elif HWY_ARCH_PPC
  __asm__ volatile("or 27,27,27" ::: "memory");  // PPC: 提示处理器
#endif
}
```

**使用场景：**
```cpp
while (!ready.load(std::memory_order_acquire)) {
  Pause();  // 在自旋锁中减少功耗
}
```

### 2. 内存对齐

**文件：** `hwy/aligned_allocator.h`

```cpp
// Highway 使用 128 字节对齐（缓存行大小）
#define HWY_ALIGNMENT 128

AllocateAlignedBytes(size_t bytes, size_t* offset_ptr) {
  // 返回对齐到 128 字节的指针
}

// 使用：
std::vector<float, AlignedAllocator<float>> aligned_data(1000);

// 或者：
AlignedUniquePtr<float[]> ptr = AllocateAligned<float>(1000);
```

**为什么 128 字节？**
- 防止伪共享（false sharing）
- 容纳 AVX-512（64 字节）+ 未来
- 匹配 Apple M1 L2 和 POWER8 需求

**性能影响：** 对齐加载可以快 2-3 倍（避免跨缓存行访问）。

### 3. 内联优化

每个操作都标记为 `HWY_INLINE`：

```cpp
#if HWY_COMPILER_MSVC
#define HWY_INLINE __forceinline
#elif HWY_COMPILER_GCC || HWY_COMPILER_CLANG
#define HWY_INLINE inline __attribute__((always_inline))
#else
#define HWY_INLINE inline
#endif
```

**为什么关键：** SIMD 代码依赖激进的内联来消除函数调用开销和允许跨函数的寄存器分配。

### 4. 假设对齐（Assume Aligned）

**文件：** `hwy/base.h`

```cpp
#define HWY_ASSUME_ALIGNED(ptr, align) \
  __builtin_assume_aligned(ptr, align)

// 使用：
void Process(float* HWY_RESTRICT data) {
  data = HWY_ASSUME_ALIGNED(data, HWY_ALIGNMENT);
  // 编译器现在可以使用对齐加载！
}
```

**效果：** 启用更快的加载/存储指令（例如，`vmovaps` 而非 `vmovups`）。

### 5. 目标特定属性

**文件：** `hwy/base.h`

```cpp
#if HWY_COMPILER_GCC || HWY_COMPILER_CLANG
#define HWY_ATTR_TARGET_SSE2 __attribute__((target("sse2")))
#define HWY_ATTR_TARGET_AVX2 __attribute__((target("avx2")))
#define HWY_ATTR_TARGET_AVX512 __attribute__((target("avx512f,avx512bw")))
// ...
#endif
```

**为什么不使用全局 `-march`？**

- 全局标志可能与目标编译指示冲突
- 函数级别属性允许混合代码路径
- 支持动态调度，无需单独的编译单元

### 6. 通用操作的编译时特化

**文件：** `hwy/ops/generic_ops-inl.h:8245`

```cpp
// 示例：Clamp 的通用实现
template <class V>
HWY_API V Clamp(const V v, const V lo, const V hi) {
#if HWY_TARGET == HWY_AVX3  // AVX-512 有原生 min/max 掩码
  // 使用快速 AVX-512 路径
  return _mm512_min_epu32(_mm512_max_epu32(v, lo), hi);
#else
  // 回退到通用实现
  return Min(Max(lo, v), hi);
#endif
}
```

**效果：** 每个平台的最优代码路径，自动选择。

---

## 实战练习

### 练习 1：理解调度机制

**任务：** 追踪 `HWY_DYNAMIC_DISPATCH(FloorLog2)` 如何在 AVX2 系统上解析。

**步骤：**

1. 阅读 `hwy/examples/skeleton.cc`
2. 找到 `HWY_EXPORT(FloorLog2)` 扩展到什么
3. 追踪 `GetChosenTargetIndex()` 如何返回 9（AVX2）
4. 验证 `FloorLog2HighwayDispatchTable[9]` 指向 `N_AVX2::FloorLog2`

**提示：** 使用 `printf("Target: %s\n", hwy::TargetName(HWY_TARGET));` 在每个目标中。

### 练习 2：实现自定义向量函数

**任务：** 编写一个函数来计算 `result[i] = sqrt(a[i]^2 + b[i]^2)`（向量幅度）。

**模板：**

```cpp
// magnitude-inl.h
#if defined(HIGHWAY_MAGNITUDE_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef HIGHWAY_MAGNITUDE_INL_H_
#undef HIGHWAY_MAGNITUDE_INL_H_
#else
#define HIGHWAY_MAGNITUDE_INL_H_
#endif

#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace magnitude {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

template <class D>
void Magnitude(D d, const float* HWY_RESTRICT a, const float* HWY_RESTRICT b,
               float* HWY_RESTRICT result, size_t count) {
  for (size_t i = 0; i < count; i += hn::Lanes(d)) {
    // 你的实现在这里！
    // 提示：使用 hn::Sqrt(hn::MulAdd(a_vec, a_vec, hn::Mul(b_vec, b_vec)))
  }
}

}  // namespace HWY_NAMESPACE
}  // namespace magnitude
HWY_AFTER_NAMESPACE();

#endif
```

**挑战：** 处理不是向量大小倍数的 `count` 的余数。

### 练习 3：分析性能

**任务：** 比较有/无预取的点积性能。

**代码：**

```cpp
float DotProductWithPrefetch(const float* a, const float* b, size_t count) {
  const ScalableTag<float> d;
  const size_t N = Lanes(d);
  auto sum = Zero(d);

  for (size_t i = 0; i < count; i += N) {
    Prefetch(&a[i + N * 4]);  // 提前 4 次迭代预取
    Prefetch(&b[i + N * 4]);

    auto va = Load(d, &a[i]);
    auto vb = Load(d, &b[i]);
    sum = MulAdd(va, vb, sum);
  }

  return ReduceSum(d, sum);
}
```

**测量：** 对 1M 元素数组进行基准测试。在大型（> L3 缓存）vs 小型（< L1 缓存）数组上比较。

### 练习 4：探索 VQSort

**任务：** 理解 Highway 的向量化快速排序如何使用排序网络。

**文件阅读列表：**

1. `hwy/contrib/sort/vqsort-inl.h` - 主算法
2. `hwy/contrib/sort/sorting_networks-inl.h` - 固定大小的网络
3. `hwy/contrib/sort/shared-inl.h` - 辅助函数

**问题回答：**

1. `SortingNetwork(d, v)` 对 16 个元素做什么？
2. VQSort 如何选择主元？
3. 为什么分区是向量化的最难部分？

**提示：** 查找 `Partition()` 和 `CompressStore()`。

---

## 下一步

在 **第二部分**，我们将涵盖：

1. **高级 contrib 模块**（algo, math, sort, dot）
2. **可扩展向量深度探讨**（RVV, SVE）
3. **掩码和混合操作**
4. **编写高性能算法**
5. **调试和分析工具**
6. **真实世界案例研究**

继续到 `02_ADVANCED_TOPICS.md`！

---

## 参考文献

- **Highway 主文档：** `README.md`
- **快速参考：** `g3doc/quick_reference.md`
- **指令矩阵：** `g3doc/instruction_matrix.pdf`
- **目标架构：** `hwy/detect_targets.h` 注释
- **示例代码：** `hwy/examples/`, `hwy/contrib/`

---

**作者：** 基于 Highway 1.3.0 探索
**日期：** 2026-01-19
