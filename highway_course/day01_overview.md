# Day 1: Highway 概述与 SIMD 基础

## 📋 学习目标

- 理解 SIMD 编程的核心概念和价值
- 了解 Highway 库的设计哲学
- 掌握 Highway 的项目结构
- 编译并运行第一个 Highway 程序
- 认识 27 个支持的目标平台

## 1. SIMD 编程基础

### 1.1 什么是 SIMD？

**SIMD (Single Instruction, Multiple Data)** 是一种并行计算架构，允许一条指令同时处理多个数据。

**传统标量代码示例：**
```cpp
// 标量加法：一次处理一个元素
void add_arrays_scalar(const float* a, const float* b, float* result, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        result[i] = a[i] + b[i];  // 每次执行一个加法
    }
}
```

**SIMD 向量化代码概念：**
```cpp
// SIMD 加法：一次处理 4/8/16 个元素（取决于硬件）
void add_arrays_simd(const float* a, const float* b, float* result, size_t n) {
    for (size_t i = 0; i < n; i += VECTOR_SIZE) {
        // 一条指令加载 VECTOR_SIZE 个 float
        // 一条指令执行 VECTOR_SIZE 个加法
        // 一条指令存储 VECTOR_SIZE 个结果
    }
}
```

**性能对比：**
- **标量代码**：100 个元素需要 100 次加法指令
- **SIMD 代码**（假设向量宽度为 8）：100 个元素只需要 13 次加法指令（12次完整向量 + 1次余项处理）
- **理论加速比**：~8x（实际会略低，因为有加载/存储开销）

### 1.2 为什么需要 SIMD？

**能耗优势：**
- 执行更少的指令意味着更低的能耗
- Highway README 提到可以实现 **5 倍能耗降低**

**性能优势：**
- 常见性能提升：**5-10x**
- 某些算法（如排序）可以达到更高加速比

**适用场景：**
- 图像/视频处理：像素操作天然数据并行
- 音频处理：采样点批量处理
- 机器学习：矩阵运算、卷积
- 密码学：批量加密/哈希
- 科学计算：数值模拟

### 1.3 SIMD 编程的挑战

**传统 SIMD 编程的困境：**

1. **平台依赖性强**
   - x86: SSE, AVX, AVX-512
   - ARM: NEON, SVE
   - 不同 intrinsics，无法共享代码

2. **可移植性差**
   - 为每个平台写不同代码
   - 维护成本高

3. **编译器支持参差不齐**
   - 自动向量化不可靠
   - 性能难以预测

**Highway 的解决方案：**
- ✅ 统一的 API 适配 27 个目标
- ✅ 一次编写，处处运行
- ✅ 可预测的性能
- ✅ 零开销抽象

## 2. Highway 项目概览

### 2.1 Highway 是什么？

Highway 是 **Google 开源的 C++ SIMD 库**，提供：
- **可移植的向量 intrinsics**：编写一次代码，在多个平台运行
- **类型安全的 API**：基于 Tag 的设计防止类型错误
- **零成本抽象**：直接映射到 CPU 指令，无运行时开销
- **灵活的分派机制**：支持静态和动态分派

### 2.2 设计哲学

引自 Highway README：

**"Does what you expect" (符合直觉)**
- 函数设计直接映射到 CPU 指令
- 避免依赖编译器复杂变换
- 代码行为可预测

**"Works on widely-used platforms" (广泛兼容)**
- 支持 7 大架构
- 支持可伸缩向量（编译时大小未知）
- 仅需 C++17 语言特性

**"Flexible to deploy" (部署灵活)**
- 运行时选择最佳指令集（动态分派）
- 或编译时锁定单一目标（静态分派）
- 代码几乎完全相同

**"Suitable for variety of domains" (领域广泛)**
- 图像处理、压缩、视频分析
- 线性代数、密码学、排序、随机数生成

**"Rewards data-parallel design" (奖励数据并行设计)**
- 提供 Gather、MaskedLoad 等工具
- 鼓励 SIMD 友好的数据布局

### 2.3 支持的 27 个目标平台

Highway 支持的目标（按架构分类）：

**通用目标：**
- `SCALAR`: 单元素标量实现（任何平台的 fallback）
- `EMU128`: 模拟的 128 位向量

**x86/x86-64 (9 个目标)：**
- `SSE2`: ~2001, Pentium 4+，128 位
- `SSSE3`: ~2006, Intel Core，增强的 shuffle
- `SSE4`: ~2008, Nehalem，包含 AES + CLMUL
- `AVX2`: ~2013, Haswell，256 位，包含 BMI2/F16/FMA
- `AVX3`: ~2017, Skylake-X，512 位基础
- `AVX3_DL`: ~2019, Ice Lake，增加 VNNI/VBMI2 等
- `AVX3_ZEN4`: ~2022, AMD Zen 4，增加 BF16
- `AVX3_SPR`: ~2023, Sapphire Rapids，AVX-512FP16
- `AVX10_2`: 未来，Diamond Rapids

**ARM (7 个目标)：**
- `NEON_WITHOUT_AES`: Armv7+，基础 NEON
- `NEON`: Armv8+，NEON + AES
- `NEON_BF16`: BFloat16 支持
- `SVE`: 可伸缩向量扩展（128-2048 位）
- `SVE2`: SVE 第二代
- `SVE_256`: SVE 锁定 256 位
- `SVE2_128`: SVE2 锁定 128 位

**RISC-V (1 个目标)：**
- `RVV`: RISC-V Vector Extension 1.0

**POWER (3 个目标)：**
- `PPC8`: POWER8 (ISA v2.07)
- `PPC9`: POWER9 (ISA v3.0)
- `PPC10`: POWER10 (ISA v3.1B, 因编译器 bug 暂未启用)

**LoongArch (2 个目标)：**
- `LSX`: LoongArch SIMD Extension
- `LASX`: LoongArch Advanced SIMD Extension

**IBM Z (2 个目标)：**
- `Z14`: IBM z14
- `Z15`: IBM z15

**WebAssembly (2 个目标)：**
- `WASM`: 128 位 SIMD
- `WASM_EMU256`: 2x 展开的 WASM

**代码位置：** `/home/dev/highway/hwy/ops/` 目录下的 `-inl.h` 文件

### 2.4 项目结构

```
highway/
├── hwy/                          # 核心库
│   ├── highway.h                 # 主头文件
│   ├── base.h                    # 平台无关工具
│   ├── foreach_target.h          # 动态分派核心
│   ├── targets.h                 # 目标选择与 CPU 检测
│   ├── detect_targets.h          # 编译时目标检测
│   ├── aligned_allocator.h       # 对齐内存分配
│   │
│   ├── ops/                      # 平台特定实现
│   │   ├── shared-inl.h          # 所有目标共享定义
│   │   ├── set_macros-inl.h      # 每目标配置
│   │   ├── generic_ops-inl.h     # 通用操作（target-independent）
│   │   ├── scalar-inl.h          # 标量实现（2174 行）
│   │   ├── x86_128-inl.h         # SSE2/SSSE3/SSE4（14145 行）
│   │   ├── x86_256-inl.h         # AVX2（8996 行）
│   │   ├── x86_512-inl.h         # AVX-512（7669 行）
│   │   ├── arm_neon-inl.h        # NEON（10647 行）
│   │   ├── arm_sve-inl.h         # SVE/SVE2（7090 行）
│   │   ├── rvv-inl.h             # RISC-V Vector（6599 行）
│   │   └── ...                   # 其他平台
│   │
│   ├── contrib/                  # 高级算法
│   │   ├── algo/                 # 基础算法
│   │   │   ├── transform-inl.h   # Transform 系列
│   │   │   ├── copy-inl.h        # 内存拷贝
│   │   │   └── find-inl.h        # 查找算法
│   │   ├── sort/                 # VQSort 向量化快排
│   │   ├── math/                 # 数学函数（sin/cos/exp/log）
│   │   ├── dot/                  # 点积
│   │   ├── image/                # 图像处理工具
│   │   └── thread_pool/          # 线程池
│   │
│   ├── tests/                    # 测试
│   │   ├── test_util-inl.h       # 测试工具
│   │   ├── hwy_gtest.h           # Google Test 集成
│   │   ├── arithmetic_test.cc    # 算术运算测试
│   │   ├── memory_test.cc        # 内存操作测试
│   │   └── ...                   # 40+ 测试文件
│   │
│   └── examples/                 # 示例代码
│       ├── skeleton.h            # 公共接口
│       ├── skeleton-inl.h        # 可重用 SIMD 函数
│       ├── skeleton.cc           # 分派实现
│       └── skeleton_test.cc      # 测试
│
├── g3doc/                        # 文档
│   ├── quick_reference.md        # API 速查
│   ├── design_philosophy.md      # 设计哲学
│   ├── impl_details.md           # 实现细节
│   └── faq.md                    # 常见问题
│
├── CMakeLists.txt                # CMake 构建配置
├── BUILD                         # Bazel 构建配置
└── README.md                     # 项目说明
```

**关键文件大小（行数）：**
- `hwy/base.h`: 113,469 行（包含大量工具函数）
- `hwy/ops/x86_128-inl.h`: 14,145 行
- `hwy/contrib/math/math-inl.h`: 59,380 行
- `hwy/examples/skeleton_test.cc`: 4,819 行

## 3. 环境搭建与第一个程序

### 3.1 构建 Highway

Highway 已经在 `/home/dev/highway` 目录下，让我们编译它：

```bash
# 进入 Highway 目录
cd /home/dev/highway

# 创建构建目录
mkdir -p build && cd build

# 配置 CMake（Debug 模式以便调试）
cmake .. -DCMAKE_BUILD_TYPE=Debug

# 编译（使用所有 CPU 核心）
make -j$(nproc)

# 运行测试验证安装
make test
# 或者
ctest -j$(nproc)
```

**常用构建选项：**
```bash
# Release 模式（性能测试用）
cmake .. -DCMAKE_BUILD_TYPE=Release

# 使用特定编译器
CXX=clang++-17 CC=clang-17 cmake .. -DCMAKE_BUILD_TYPE=Debug

# 启用警告即错误（CI 模式）
cmake .. -DHWY_WARNINGS_ARE_ERRORS:BOOL=ON

# Armv7 目标（需要此选项因编译器限制）
cmake .. -DHWY_CMAKE_ARM7:BOOL=ON
```

### 3.2 第一个 Highway 程序

让我们编写一个简单的向量加法程序来感受 Highway。

**创建文件: `first_highway.cc`**

```cpp
#include <cstdio>
#include "hwy/highway.h"

// 必须在 hwy 命名空间内
namespace hwy {
// HWY_NAMESPACE 会根据编译目标展开为不同的命名空间
namespace HWY_NAMESPACE {

// 向量加法函数
void VectorAdd(const float* HWY_RESTRICT a, const float* HWY_RESTRICT b,
               float* HWY_RESTRICT result, size_t count) {
  // 创建一个 float 类型的标签（tag），用于确定向量大小
  const ScalableTag<float> d;

  // 获取每个向量的元素数量（lane count）
  const size_t N = Lanes(d);

  size_t i = 0;
  // 处理完整向量
  for (; i + N <= count; i += N) {
    // 加载向量
    const auto va = Load(d, a + i);
    const auto vb = Load(d, b + i);

    // 向量加法（一条指令处理 N 个元素）
    const auto sum = Add(va, vb);

    // 存储结果
    Store(sum, d, result + i);
  }

  // 处理剩余元素（标量方式）
  for (; i < count; ++i) {
    result[i] = a[i] + b[i];
  }
}

}  // namespace HWY_NAMESPACE
}  // namespace hwy

// 使用静态分派（单一目标，最简单）
int main() {
  constexpr size_t kCount = 16;
  float a[kCount], b[kCount], result[kCount];

  // 初始化数据
  for (size_t i = 0; i < kCount; ++i) {
    a[i] = static_cast<float>(i);
    b[i] = static_cast<float>(i * 2);
  }

  // 调用向量加法
  HWY_STATIC_DISPATCH(VectorAdd)(a, b, result, kCount);

  // 打印结果
  printf("Vector addition results:\\n");
  for (size_t i = 0; i < kCount; ++i) {
    printf("%.1f + %.1f = %.1f\\n", a[i], b[i], result[i]);
  }

  // 打印当前使用的目标
  printf("\\nUsing target: %s\\n",
         hwy::TargetName(HWY_STATIC_TARGET));

  return 0;
}
```

**编译并运行：**

```bash
# 编译（使用 Highway 构建系统）
cd /home/dev/highway/build
cat > ../first_highway.cc << 'EOF'
[粘贴上面的代码]
EOF

# 编译为可执行文件
g++ -std=c++17 -I.. -O2 ../first_highway.cc -o first_highway

# 运行
./first_highway
```

**预期输出：**
```
Vector addition results:
0.0 + 0.0 = 0.0
1.0 + 2.0 = 3.0
2.0 + 4.0 = 6.0
...
15.0 + 30.0 = 45.0

Using target: AVX2  (或者 SSE4、NEON 等，取决于你的硬件)
```

### 3.3 代码解析

让我们逐行理解这个程序：

**1. 头文件包含**
```cpp
#include "hwy/highway.h"
```
- 这是 Highway 的主头文件
- 会根据编译选项自动选择合适的目标实现
- 位置：`/home/dev/highway/hwy/highway.h:16`

**2. 命名空间声明**
```cpp
namespace hwy {
namespace HWY_NAMESPACE {
```
- 所有 Highway SIMD 代码必须在这两层命名空间内
- `HWY_NAMESPACE` 是一个宏，会展开为目标特定的命名空间
  - SSE2 目标: `N_SSE2`
  - AVX2 目标: `N_AVX2`
  - NEON 目标: `N_NEON`
- 这允许同一份代码为多个目标编译并共存

**3. Tag（标签）**
```cpp
const ScalableTag<float> d;
```
- `ScalableTag<T>` 是 Highway 的核心类型
- 不是向量类型本身，而是用于**选择**向量类型的"标签"
- `float` 表示元素类型
- "Scalable" 意味着向量大小适应硬件（SSE 是 4 个 float，AVX2 是 8 个）

**4. 获取向量宽度**
```cpp
const size_t N = Lanes(d);
```
- `Lanes(d)` 返回向量中的元素数量
- SSE/NEON (128-bit): 4 个 float
- AVX2 (256-bit): 8 个 float
- AVX-512 (512-bit): 16 个 float
- RVV/SVE: 运行时确定

**5. 向量操作**
```cpp
const auto va = Load(d, a + i);      // 加载
const auto vb = Load(d, b + i);      // 加载
const auto sum = Add(va, vb);        // 加法
Store(sum, d, result + i);           // 存储
```
- `Load()`: 从内存加载向量
- `Add()`: 向量加法，编译为单条指令（如 `addps`/`fadd`）
- `Store()`: 向量存储到内存
- `auto` 推导为向量类型，如 `Vec<ScalableTag<float>>`

**6. 静态分派调用**
```cpp
HWY_STATIC_DISPATCH(VectorAdd)(a, b, result, kCount);
```
- `HWY_STATIC_DISPATCH` 调用针对当前编译目标的版本
- 零运行时开销，直接函数调用
- 替代方案是 `HWY_DYNAMIC_DISPATCH`（运行时选择）

## 4. 深入理解：查看生成的汇编

让我们看看 Highway 代码实际编译成什么：

```bash
# 生成汇编代码
g++ -std=c++17 -I.. -O2 -S -masm=intel ../first_highway.cc -o first_highway.s

# 查看 VectorAdd 函数的汇编
grep -A 20 "VectorAdd" first_highway.s
```

**SSE2 目标的汇编示例：**
```asm
movaps  xmm0, [rdi + rax]    ; 加载 4 个 float 从 a
movaps  xmm1, [rsi + rax]    ; 加载 4 个 float 从 b
addps   xmm0, xmm1           ; 向量加法（4 个加法同时进行）
movaps  [rdx + rax], xmm0    ; 存储结果
```

**关键观察：**
- 原本需要 4 条标量 `addss` 指令
- Highway 生成单条 `addps` 指令（packed single-precision add）
- **4x 指令减少** = 理论 4x 加速

**AVX2 目标的汇编示例：**
```asm
vmovaps ymm0, [rdi + rax]    ; 加载 8 个 float
vmovaps ymm1, [rsi + rax]    ; 加载 8 个 float
vaddps  ymm0, ymm0, ymm1     ; 向量加法（8 个加法）
vmovaps [rdx + rax], ymm0    ; 存储结果
```
- **8x 指令减少** = 理论 8x 加速

## 5. 今日小结

### 核心概念回顾

1. **SIMD 是什么**
   - Single Instruction, Multiple Data
   - 一条指令处理多个数据
   - 5-10x 性能提升，5x 能耗降低

2. **Highway 解决什么问题**
   - 跨平台 SIMD 编程的可移植性
   - 27 个目标，一套 API
   - 零成本抽象，可预测性能

3. **Highway 支持哪些平台**
   - x86: SSE2 到 AVX10_2（9 个目标）
   - ARM: NEON 到 SVE2（7 个目标）
   - RISC-V, POWER, LoongArch, IBM Z, WASM

4. **第一个程序的要点**
   - 包含 `hwy/highway.h`
   - 代码在 `hwy::HWY_NAMESPACE` 内
   - 使用 `ScalableTag<T>` 创建标签
   - `Load/Store/Add` 等操作
   - `HWY_STATIC_DISPATCH` 调用

### 关键文件位置

- 主头文件: `/home/dev/highway/hwy/highway.h`
- 标量实现: `/home/dev/highway/hwy/ops/scalar-inl.h`
- 示例代码: `/home/dev/highway/hwy/examples/skeleton*`
- 文档: `/home/dev/highway/g3doc/`

## 📝 今日练习

### 练习 1: 编译并运行示例

1. 编译 Highway 库（如果还没做）
2. 运行第一个程序，观察输出
3. 尝试修改数组大小为 100，观察余项处理

### 练习 2: 向量乘法

修改 `first_highway.cc`，将加法改为乘法：
```cpp
const auto product = Mul(va, vb);  // 替代 Add
```

### 练习 3: 查看汇编

生成汇编代码，找到以下内容：
1. `Load` 对应的汇编指令（如 `movaps`）
2. `Mul` 对应的汇编指令（如 `mulps`）
3. 计算一个循环处理多少个元素

### 练习 4: 目标对比

编译三个版本，对比汇编代码：
```bash
# SSE2 版本
g++ -std=c++17 -I.. -O2 -msse2 -mno-avx first_highway.cc -o first_sse2
./first_sse2

# AVX2 版本
g++ -std=c++17 -I.. -O2 -mavx2 first_highway.cc -o first_avx2
./first_avx2

# 标量版本（禁用 SIMD）
g++ -std=c++17 -I.. -O2 -DHWY_COMPILE_ONLY_SCALAR first_highway.cc -o first_scalar
./first_scalar
```

观察三者的 Lanes(d) 输出和性能差异。

### 思考题

1. 为什么 Highway 需要 `HWY_RESTRICT` 关键字？（提示：指针别名）
2. 如果数组大小不是向量宽度的整数倍怎么办？
3. `ScalableTag` 的 "Scalable" 是什么意思？
4. 为什么要用 `auto` 而不是显式写出向量类型？

## 🔗 相关资源

- [Highway GitHub 仓库](https://github.com/google/highway)
- [SIMD for C++ Developers (PDF)](http://const.me/articles/simd/simd.pdf)
- [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/)
- [ARM NEON Intrinsics](https://developer.arm.com/architectures/instruction-sets/intrinsics/)

## 明天预告

**Day 2: 类型系统与 Tag 机制**

我们将深入探讨：
- Tag-based Dispatch 的设计原理
- `ScalableTag` vs `CappedTag` vs `FixedTag`
- 类型推导系统：`TFromD`、`VFromD`、`MFromD`
- `Vec1` 和 `Mask1` 的完整实现
- 为什么这种设计比传统模板更优秀

---

**继续前进！** 👉 [Day 2: 类型系统与 Tag 机制](day02_type_system.md)
