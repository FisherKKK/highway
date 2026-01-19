# Highway SIMD 术语表 (Glossary)

完整的 SIMD 编程和 Highway 库术语词典，按字母顺序排列。

---

## A

### ADL (Argument Dependent Lookup)
**中文：** 参数依赖查找
**定义：** C++ 编译器在查找函数时，会在参数类型所在的命名空间中搜索。
**Highway 中的应用：** Highway 操作必须在正确的命名空间中调用，以便 ADL 能找到正确的重载。

### Alignment (对齐)
**定义：** 数据在内存中的起始地址必须是某个值的倍数。
**Highway 常量：** `HWY_ALIGNMENT` (通常是 64 或 128 字节)
**重要性：** 对齐的加载/存储操作通常比未对齐的快 2-3 倍。
**示例：**
```cpp
// 对齐分配
AlignedUniquePtr<float[]> data = AllocateAligned<float>(1024);
auto v = Load(d, data.get());  // 快速对齐加载
```

### AoS (Array of Structures)
**中文：** 结构体数组
**定义：** 数据布局方式，将每个对象的所有字段连续存储。
**对比：** SoA (Structure of Arrays)
**缺点：** 对 SIMD 不友好，需要复杂的 shuffle 操作。
**示例：**
```cpp
// AoS 布局
struct Particle { float x, y, z; };
Particle particles[100];  // [xyz][xyz][xyz]...
```

### AVX (Advanced Vector Extensions)
**定义：** Intel/AMD 的 256 位 SIMD 指令集扩展。
**版本：**
- AVX (2011): 256-bit 浮点
- AVX2 (2013): 256-bit 整数
- AVX-512 (2016+): 512-bit + 掩码寄存器
**Highway 目标：** `HWY_AVX2`, `HWY_AVX3`, `HWY_AVX3_DL`, `HWY_AVX3_ZEN4`, `HWY_AVX3_SPR`

---

## B

### Blending (混合)
**定义：** 根据掩码从两个向量中选择通道。
**Highway 函数：** `IfThenElse(mask, true_vec, false_vec)`
**用途：** 条件赋值、掩码存储
**示例：**
```cpp
auto result = IfThenElse(Gt(v, threshold), v, Zero(d));
// 等价于: result[i] = v[i] > threshold ? v[i] : 0
```

### Broadcast (广播)
**定义：** 将单个值或单个通道复制到向量的所有通道。
**Highway 函数：** `Set(d, value)`, `Broadcast<N>(vec)`
**示例：**
```cpp
auto v = Set(d, 3.14f);        // [3.14, 3.14, 3.14, ...]
auto b = Broadcast<0>(vec);    // [vec[0], vec[0], ...]
```

---

## C

### Cache Line (缓存行)
**定义：** CPU 缓存的基本单位，通常是 64 字节。
**重要性：** 数据应该对齐到缓存行边界以避免 false sharing。
**Highway 考虑：** `HWY_ALIGNMENT` 设计考虑了缓存行大小。

### CappedTag
**定义：** Highway 标签类型，限制最大通道数。
**语法：** `CappedTag<T, N>`
**行为：** 最多 N 个通道，但会向下取整到 2 的幂。
**用途：** 处理小数组、嵌套向量。
**示例：**
```cpp
CappedTag<float, 8> d8;  // 最多 8 个 float
// 在 AVX2 上是 8 个，在 SSE2 上是 4 个
```

### Compress (压缩)
**定义：** 根据掩码压缩向量，将所有"真"的通道移到前面。
**Highway 函数：** `Compress(vec, mask)`
**性能：** AVX-512 有硬件指令（快），其他平台需要模拟（慢）。
**用途：** 过滤数据、稀疏操作。
**示例：**
```cpp
// 输入: [1, 2, 3, 4], mask: [T, F, T, F]
// 输出: [1, 3, ?, ?]  (压缩后前 2 个有效)
```

---

## D

### Dynamic Dispatch (动态调度)
**定义：** 运行时根据 CPU 特性选择最佳的 SIMD 实现。
**Highway 机制：** `HWY_EXPORT` + `HWY_DYNAMIC_DISPATCH`
**优点：** 一个二进制支持多种 CPU
**缺点：** 轻微的间接调用开销
**对比：** Static Dispatch

### DemoteTo (降级转换)
**定义：** 将向量转换为更窄的类型（减少位数）。
**示例：** `int32 → int16`, `float → bfloat16`
**Highway 函数：** `DemoteTo(narrow_tag, vec)`
**注意：** 可能发生截断或饱和。

---

## E

### EMUL / LMUL
**定义：** RISC-V Vector Extension 的向量长度倍增器。
**LMUL：** Length Multiplier (1, 2, 4, 8)
**EMUL：** Effective LMUL
**作用：** 控制每次操作使用几个向量寄存器。
**示例：** LMUL=2 表示使用 2 倍的向量寄存器宽度。

---

## F

### False Sharing
**中文：** 伪共享
**定义：** 多个线程访问同一缓存行的不同变量，导致缓存失效。
**避免方法：** 将数据对齐到缓存行边界，使用 `HWY_ALIGNMENT`。

### FixedTag
**定义：** Highway 标签类型，指定精确的通道数。
**语法：** `FixedTag<T, N>`
**要求：** N 必须是 2 的幂
**用途：** 需要固定大小的算法（如 FFT）。
**示例：**
```cpp
FixedTag<float, 4> d4;  // 恰好 4 个 float
```

### FMA (Fused Multiply-Add)
**中文：** 融合乘加
**定义：** 单条指令完成 `a*b + c`，精度更高，速度更快。
**Highway 函数：** `MulAdd(a, b, c)`
**性能：** 与单独的 `Mul` 相同延迟，但做更多工作。
**重要性：** 高性能计算的核心操作。

### foreach_target.h
**定义：** Highway 的核心头文件，实现动态调度的重新编译机制。
**工作原理：** 多次包含同一源文件，每次为不同的目标编译。
**使用前提：** 必须定义 `HWY_TARGET_INCLUDE`。

---

## G

### Gather (聚集)
**定义：** 根据索引向量从内存的不连续位置加载数据。
**Highway 函数：** `GatherIndex`, `GatherOffset`
**性能：** 非常慢（10-20x Load），应尽量避免。
**示例：**
```cpp
// indices = [0, 5, 2, 7]
// 从 data[0], data[5], data[2], data[7] 加载
auto v = GatherIndex(d, data, indices);
```

### GEMM (General Matrix Multiply)
**中文：** 通用矩阵乘法
**定义：** `C = α*A*B + β*C`
**重要性：** 线性代数和机器学习的核心操作。
**优化技巧：** 分块、SIMD、缓存优化。

---

## H

### Horizontal Operation (水平操作)
**定义：** 在向量内部的通道之间进行操作（如求和、最大值）。
**Highway 函数：** `ReduceSum`, `ReduceMin`, `ReduceMax`
**性能：** 比垂直操作慢 3-5 倍，应最小化使用。
**对比：** Vertical Operation

### HWY_ALIGNMENT
**定义：** Highway 要求的内存对齐值，通常是 64 或 128 字节。
**用途：** 用于 `AllocateAligned` 和对齐断言。

### HWY_ATTR
**定义：** 函数属性宏，标记函数可以使用目标特定的指令。
**用途：** 在不使用 `HWY_BEFORE_NAMESPACE()` 时标记函数。

### HWY_DYNAMIC_DISPATCH
**定义：** 宏，用于调用动态调度的函数。
**语法：** `HWY_DYNAMIC_DISPATCH(FunctionName)(args)`
**前提：** 必须先用 `HWY_EXPORT(FunctionName)` 导出。

### HWY_EXPORT
**定义：** 宏，导出函数以支持动态调度。
**用法：** 放在 `#if HWY_ONCE` 块中。
**作用：** 创建函数指针表和调度函数。

### HWY_NAMESPACE
**定义：** 当前编译目标的命名空间名称。
**示例：** `N_AVX2`, `N_NEON`, `N_SCALAR`
**用途：** 隔离不同目标的代码，避免符号冲突。

### HWY_STATIC_DISPATCH
**定义：** 宏，用于调用静态调度的函数（单一目标）。
**语法：** `HWY_STATIC_DISPATCH(FunctionName)(args)`
**优点：** 零开销，编译器可完全内联。

### HWY_TARGET
**定义：** 宏，表示当前正在编译的 SIMD 目标。
**示例：** `HWY_AVX2`, `HWY_NEON`, `HWY_SCALAR`
**用途：** 条件编译目标特定的代码。

### HWY_TARGET_TOGGLE
**定义：** 魔法宏，用于实现切换包含保护。
**用途：** 允许头文件被 `foreach_target.h` 多次包含。

---

## I

### Intrinsics (内在函数)
**定义：** 编译器提供的类似函数的接口，直接映射到特定的汇编指令。
**示例：** `_mm_add_ps` (SSE), `vaddq_f32` (NEON)
**Highway 优势：** 跨平台抽象，不需要直接使用 intrinsics。

### Iota
**定义：** 生成递增序列向量。
**Highway 函数：** `Iota(d, start)`
**结果：** `[start, start+1, start+2, ...]`
**用途：** 生成索引、测试数据。

---

## K

### kPow2
**定义：** `Simd<T, N, kPow2>` 的第三个参数，表示向量大小的指数。
**编码：** 负数表示分数（`kPow2=-1` → 1/2 向量）
**用途：** 递归算法、子向量操作。

---

## L

### Lane (通道)
**定义：** 向量中的单个数据元素。
**示例：** 8 个 float 的向量有 8 个通道。
**Highway 函数：** `Lanes(d)` 返回通道数。

### Latency (延迟)
**定义：** 指令从开始到结果可用的时钟周期数。
**示例：** `Add` 延迟通常是 1 周期，`Div` 可能是 10-15 周期。
**优化：** 使用多个累加器隐藏延迟。

### Load / LoadU
**定义：** 从内存加载数据到向量寄存器。
**Load：** 需要对齐到 `HWY_ALIGNMENT`
**LoadU：** 未对齐加载，稍慢但更灵活
**Highway 函数：** `Load(d, ptr)`, `LoadU(d, ptr)`

---

## M

### Mask (掩码)
**定义：** 表示布尔条件的向量，每个通道是真或假。
**Highway 类型：** `Mask<D>`
**生成：** 比较操作（`Eq`, `Lt`, `Gt` 等）
**用途：** 条件操作、掩码加载/存储。

### MaskedLoad / MaskedLoadOr
**定义：** 根据掩码有条件地加载数据。
**MaskedLoadOr：** 未加载的通道使用提供的值。
**用途：** 处理数组余项、避免越界。

### MSVC (Microsoft Visual C++)
**定义：** Microsoft 的 C++ 编译器。
**Highway 支持：** 需要 MSVC 2015+
**特殊要求：** 使用 `/Gv` 标志传递向量参数。

---

## N

### NEON
**定义：** ARM 的 128 位 SIMD 指令集。
**支持：** ARMv7+ (32-bit), ARMv8+ (64-bit)
**Highway 目标：** `HWY_NEON`
**特点：** 相比 SSE2 缺少一些操作，Highway 通过软件模拟弥补。

---

## O

### OOO (Out-of-Order Execution)
**中文：** 乱序执行
**定义：** CPU 动态重排指令以最大化吞吐量。
**优化启示：** 使用多个累加器可以让 CPU 并行执行更多指令。

---

## P

### Predication (谓词化)
**定义：** 使用掩码控制指令是否执行（ARM SVE、AVX-512）。
**优点：** 避免分支，提高性能。
**Highway：** 通过 `FirstN`, `IfThenElse` 等实现。

### Prefetch (预取)
**定义：** 提前将数据从主存加载到缓存。
**Highway 函数：** `Prefetch(ptr)`, `PrefetchNonTemporal(ptr)`
**用途：** 处理大数组时隐藏内存延迟。
**示例：**
```cpp
for (size_t i = 0; i < count; i += N) {
  Prefetch(&data[i + 64]);  // 提前预取
  auto v = Load(d, &data[i]);
  // 处理 v...
}
```

### PromoteTo (提升转换)
**定义：** 将向量转换为更宽的类型（增加位数）。
**示例：** `int16 → int32`, `bfloat16 → float`
**Highway 函数：** `PromoteTo(wide_tag, vec)`

---

## R

### RAII (Resource Acquisition Is Initialization)
**定义：** C++ 资源管理习惯用法。
**Highway 应用：** `AlignedUniquePtr` 自动释放对齐的内存。

### Reciprocal (倒数)
**定义：** 计算 1/x。
**Highway 函数：** `Div(Set(d, 1.0f), x)` 或近似版本
**优化：** 使用近似倒数 + 牛顿迭代法更快。

### Reduce / Reduction (归约)
**定义：** 将向量的所有通道合并为单个标量值。
**Highway 函数：** `ReduceSum`, `ReduceMin`, `ReduceMax`
**性能：** 水平操作，比垂直操作慢。

### RVV (RISC-V Vector Extension)
**定义：** RISC-V 的可扩展向量指令集。
**特点：** 向量长度可变（通过 LMUL）
**Highway 目标：** `HWY_RVV`
**优势：** 未来可扩展到任意宽度。

---

## S

### Scalar (标量)
**定义：** 单个数据元素（非向量）。
**Highway 目标：** `HWY_SCALAR` - 可移植的后备实现。

### ScalableTag
**定义：** Highway 推荐的标签类型，使用平台的最大向量宽度。
**语法：** `ScalableTag<T>`
**别名：** `HWY_FULL(T)` (旧版本)
**优点：** 自动适应平台，性能最佳。

### Scatter (分散)
**定义：** 根据索引向量将数据存储到内存的不连续位置。
**Highway 函数：** `ScatterIndex`, `ScatterOffset`
**性能：** 非常慢，应尽量避免。

### Shuffle (重排)
**定义：** 重新排列向量内的通道。
**Highway 函数：** `Shuffle01`, `Shuffle2301`, `TableLookupBytes`
**用途：** 数据重组、AoS ↔ SoA 转换。

### SIMD (Single Instruction, Multiple Data)
**中文：** 单指令多数据
**定义：** 并行计算范式，一条指令同时处理多个数据。
**示例：** 同时对 8 个 float 执行加法。

### SoA (Structure of Arrays)
**中文：** 数组的结构体
**定义：** 数据布局方式，将相同字段连续存储。
**优点：** SIMD 友好，易于向量化。
**示例：**
```cpp
// SoA 布局
struct Particles {
  float x[100];
  float y[100];
  float z[100];
};
```

### SSE (Streaming SIMD Extensions)
**定义：** Intel 的 128 位 SIMD 指令集系列。
**版本：** SSE, SSE2, SSE3, SSSE3, SSE4.1, SSE4.2
**Highway 目标：** `HWY_SSE2`, `HWY_SSSE3`, `HWY_SSE4`

### Static Dispatch (静态调度)
**定义：** 编译时选择单一的 SIMD 目标。
**Highway 机制：** `HWY_STATIC_DISPATCH`
**优点：** 零开销，完全内联
**缺点：** 二进制只能在支持该目标的 CPU 上运行。

### Store / StoreU
**定义：** 将向量寄存器的数据存储到内存。
**Store：** 需要对齐
**StoreU：** 未对齐存储
**Highway 函数：** `Store(v, d, ptr)`, `StoreU(v, d, ptr)`

### SVE (Scalable Vector Extension)
**定义：** ARM 的可扩展向量指令集（ARMv8.2+）。
**特点：** 向量长度在 128-2048 位可变
**Highway 目标：** `HWY_SVE`, `HWY_SVE2`
**谓词：** 使用谓词寄存器控制通道操作。

---

## T

### TableLookup (表查找)
**定义：** 使用索引向量重排向量。
**Highway 函数：** `TableLookupBytes`, `TableLookupLanes`
**用途：** 复杂的 shuffle、查找表操作。

### Target (目标)
**定义：** 特定的 SIMD 指令集（如 AVX2、NEON）。
**Highway 支持：** 27+ 个目标
**检测：** `HWY_TARGET` 宏

### Throughput (吞吐量)
**定义：** CPU 每个时钟周期可以执行多少条指令。
**示例：** `Add` 吞吐量可能是 2/cycle（每周期 2 条）
**优化：** 循环展开可以接近理论吞吐量。

### Toggle Guard (切换保护)
**定义：** 使用 `HWY_TARGET_TOGGLE` 实现的特殊包含保护。
**用途：** 允许头文件被多次包含而不重复定义。
**模式：**
```cpp
#if defined(MY_HEADER_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef MY_HEADER_H_
#undef MY_HEADER_H_
#else
#define MY_HEADER_H_
#endif
// 内容...
#endif
```

### Transform
**定义：** 将函数应用到数组的每个元素（向量化循环）。
**Highway 函数：** `Transform`, `Transform1`
**优点：** 自动处理余项、优化内存访问。

---

## V

### Vec (向量类型)
**定义：** Highway 的向量寄存器类型。
**类型推导：** `Vec<D>` 或 `decltype(Zero(d))`
**平台特定：** 在 x86 上是 `__m128i`, 在 ARM 上是 `float32x4_t`

### Vectorization (向量化)
**中文：** 向量化
**定义：** 将标量代码转换为 SIMD 代码的过程。
**方法：** 手动（Highway）、编译器自动向量化。

### Vertical Operation (垂直操作)
**定义：** 在向量之间对应通道进行操作（如逐元素加法）。
**示例：** `Add(a, b)` - 对应通道相加
**性能：** 通常非常快，是 SIMD 的主要用途。

### VLEN (Vector Length)
**定义：** 向量的位宽度（如 128, 256, 512 位）。
**可变：** SVE 和 RVV 支持可变 VLEN。

### VQSort
**定义：** Highway 提供的向量化快速排序算法。
**性能：** 比 `std::sort` 快 5-10 倍。
**原理：** 排序网络 + 向量化分区。

---

## W

### WebAssembly (Wasm)
**定义：** 在浏览器中运行的可移植二进制格式。
**SIMD 支持：** Wasm SIMD (128-bit)
**Highway 目标：** `HWY_WASM`, `HWY_WASM_EMU256`

---

## Z

### Zero-Overhead Abstraction (零开销抽象)
**定义：** 抽象不引入额外的运行时成本。
**Highway 实现：** 通过内联、模板和编译器优化。
**验证：** 查看生成的汇编代码。

---

## 符号和缩写

### `__m128` / `__m256` / `__m512`
**定义：** x86 intrinsics 的向量类型。
**Highway：** 不直接使用，而是通过 `Vec<D>` 抽象。

### `d` (常见变量名)
**定义：** Tag/Descriptor 变量，用于推导向量类型。
**示例：** `const ScalableTag<float> d;`

### `N` (通道数)
**定义：** 向量的通道数量，通过 `Lanes(d)` 获取。
**注意：** 不要硬编码，使用运行时查询。

### `T` (模板参数)
**定义：** 向量元素的类型（如 `float`, `int32_t`）。

---

## 性能相关术语

### Bandwidth (带宽)
**定义：** 内存系统每秒可以传输的字节数。
**瓶颈：** 许多 SIMD 程序受内存带宽限制。

### Cache Miss (缓存未命中)
**定义：** 数据不在缓存中，需要从主存加载。
**代价：** 可能延迟 100+ 周期。

### ILP (Instruction-Level Parallelism)
**中文：** 指令级并行
**定义：** CPU 同时执行多条独立指令的能力。
**优化：** 循环展开、多累加器。

### Roofline Model (屋顶线模型)
**定义：** 性能分析模型，显示程序是计算受限还是内存受限。
**用途：** 指导优化方向。

---

## 调试相关术语

### ASan (AddressSanitizer)
**定义：** 内存错误检测工具。
**用途：** 检测越界访问、use-after-free。
**编译选项：** `-fsanitize=address`

### MSan (MemorySanitizer)
**定义：** 未初始化内存读取检测工具。
**编译选项：** `-fsanitize=memory`

### UBSan (UndefinedBehaviorSanitizer)
**定义：** 未定义行为检测工具。
**用途：** 检测整数溢出、空指针解引用等。
**编译选项：** `-fsanitize=undefined`

---

## 快速查找索引

**最常用术语（新手必知）：**
- ScalableTag, CappedTag, FixedTag
- Load / LoadU / Store / StoreU
- Lanes(d) / HWY_ALIGNMENT
- Dynamic Dispatch / Static Dispatch
- HWY_TARGET / HWY_NAMESPACE
- Mask / IfThenElse
- Reduce / ReduceSum

**性能优化术语：**
- FMA / MulAdd
- Prefetch
- SoA vs AoS
- Latency / Throughput
- Cache Miss / Bandwidth

**平台相关术语：**
- AVX / AVX2 / AVX-512
- NEON / SVE
- RVV / LMUL
- SSE / SSSE3 / SSE4

**调试术语：**
- ASan / MSan / UBSan
- Sanitizer
- Alignment fault

---

**如何使用本术语表：**

1. **学习新概念时：** 先查找定义和示例
2. **遇到错误时：** 搜索错误相关术语（如 "Alignment"）
3. **优化性能时：** 查阅性能相关章节
4. **编写代码时：** 查找 API 函数名称

---

*最后更新：2026-01-19 | Highway v1.3.0 | 包含 100+ 术语*
