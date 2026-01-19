# Highway SIMD 故障排除指南 (Troubleshooting Guide)

结构化的问题诊断流程、决策树和调试工作流，帮助快速定位和解决 Highway 开发中的常见问题。

---

## 📋 目录

1. [快速诊断决策树](#快速诊断决策树)
2. [编译问题排查流程](#编译问题排查流程)
3. [运行时崩溃调试](#运行时崩溃调试)
4. [性能问题诊断](#性能问题诊断)
5. [逻辑错误排查](#逻辑错误排查)
6. [平台特定问题](#平台特定问题)
7. [完整调试工作流](#完整调试工作流)

---

## 🌳 快速诊断决策树

```
你遇到了什么问题？
│
├─ 编译失败
│  │
│  ├─ 错误包含 "HWY_TARGET_INCLUDE"
│  │  └─→ 检查是否定义了 HWY_TARGET_INCLUDE
│  │      → 见：编译问题 #1
│  │
│  ├─ 错误包含 "redefinition"
│  │  └─→ 检查是否使用了切换保护
│  │      → 见：编译问题 #2
│  │
│  ├─ 错误包含 "no matching function"
│  │  └─→ 检查是否传递了标签参数
│  │      → 见：编译问题 #3
│  │
│  └─ 其他编译错误
│     └─→ 转到：编译问题排查流程
│
├─ 链接失败
│  │
│  ├─ "undefined reference to HWY_DYNAMIC_DISPATCH"
│  │  └─→ 检查是否使用了 HWY_EXPORT
│  │      → 见：链接问题 #1
│  │
│  └─ "undefined reference to hwy::..."
│     └─→ 检查是否链接了 Highway 库
│         → 见：链接问题 #2
│
├─ 运行时崩溃
│  │
│  ├─ SIGILL (Illegal Instruction)
│  │  └─→ CPU 不支持 / 静态初始化 / 混合目标
│  │      → 转到：运行时崩溃调试
│  │
│  ├─ SIGSEGV (Segmentation Fault)
│  │  └─→ 对齐问题 / 越界访问
│  │      → 转到：运行时崩溃调试
│  │
│  └─ Sanitizer 报错
│     └─→ 查看 Sanitizer 输出，定位具体问题
│
├─ 性能不理想
│  │
│  ├─ SIMD 代码比标量慢
│  │  └─→ Debug 模式 / 数据太小 / 内存瓶颈
│  │      → 转到：性能问题诊断
│  │
│  └─ 性能不稳定
│     └─→ CPU 频率调节 / 热量调节
│         → 转到：性能问题诊断
│
└─ 结果不正确
   │
   ├─ 与标量版本不一致
   │  └─→ 浮点精度 / 余数处理 / 掩码错误
   │      → 转到：逻辑错误排查
   │
   └─ 特定平台失败
      └─→ 平台差异 / 指令集限制
          → 转到：平台特定问题
```

---

## 🔧 编译问题排查流程

### 流程图

```
编译失败
│
├─ 第 1 步：确认基本设置
│  ├─ [ ] 使用 C++17 编译器？
│  ├─ [ ] 包含了 hwy/highway.h？
│  └─ [ ] CMake/编译命令正确？
│
├─ 第 2 步：检查动态调度设置
│  ├─ [ ] 是否使用动态调度？
│  │    └─ 是 → 检查以下项
│  │         ├─ [ ] 定义了 HWY_TARGET_INCLUDE？
│  │         ├─ [ ] 包含了 foreach_target.h？
│  │         └─ [ ] 使用了切换保护？
│  │
│  └─ [ ] 是否使用静态调度？
│       └─ 是 → 确认只包含 hwy/highway.h
│
├─ 第 3 步：检查代码结构
│  ├─ [ ] 代码在 HWY_NAMESPACE 中？
│  ├─ [ ] 函数有 HWY_ATTR 或在 HWY_BEFORE/AFTER_NAMESPACE 之间？
│  └─ [ ] 标签类型正确（ScalableTag<T>）？
│
└─ 第 4 步：查看具体错误
   └─→ 转到：常见编译错误解决方案
```

### 常见编译错误解决方案

#### 错误 1: "HWY_TARGET_INCLUDE not defined"

**检查清单：**
```cpp
// ❌ 错误
#include "hwy/foreach_target.h"

// ✅ 正确
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "my_file.cc"  // 当前文件名
#include "hwy/foreach_target.h"
```

**调试命令：**
```bash
# 检查是否定义
g++ -E my_file.cc | grep HWY_TARGET_INCLUDE

# 应该看到：
# #define HWY_TARGET_INCLUDE "my_file.cc"
```

#### 错误 2: "redefinition of struct/class"

**检查清单：**
```cpp
// ❌ 错误：标准包含保护
#ifndef MY_HEADER_H
#define MY_HEADER_H
// ...
#endif

// ✅ 正确：切换保护
#if defined(MY_HEADER_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef MY_HEADER_H_
#undef MY_HEADER_H_
#else
#define MY_HEADER_H_
#endif
// ...
#endif
```

**验证方法：**
```bash
# 预处理并查看宏定义
g++ -E -dM my_header.h | grep -i toggle
```

#### 错误 3: "no matching function for call"

**诊断步骤：**

1. **检查是否传递了标签：**
```cpp
// ❌ 忘记标签
auto v = Load(ptr);

// ✅ 传递标签
const ScalableTag<float> d;
auto v = Load(d, ptr);
```

2. **检查参数类型：**
```cpp
// 确保标签类型匹配数据类型
const ScalableTag<float> d;
float* data = ...;
auto v = Load(d, data);  // ✅ 类型匹配
```

3. **检查命名空间：**
```cpp
// ❌ 不在正确的命名空间
auto v = Load(d, ptr);  // 找不到 Load

// ✅ 在 Highway 命名空间中
namespace hwy {
namespace HWY_NAMESPACE {
  auto v = Load(d, ptr);
}
}
```

---

## 💥 运行时崩溃调试

### SIGILL (Illegal Instruction) 调试流程

```
SIGILL 崩溃
│
├─ 第 1 步：检查 CPU 支持
│  │
│  ├─ Linux: cat /proc/cpuinfo | grep flags
│  ├─ macOS: sysctl machdep.cpu.features
│  └─ Windows: wmic cpu get caption
│  │
│  └─ CPU 不支持？
│     └─→ 禁用该目标
│         #define HWY_DISABLED_TARGETS (HWY_AVX3)
│
├─ 第 2 步：检查静态初始化
│  │
│  ├─ [ ] 是否有命名空间级别的静态向量？
│  │    └─→ ❌ 错误！移到函数内部
│  │
│  └─ [ ] 是否有全局的 SIMD 常量？
│       └─→ ❌ 错误！使用函数返回
│  │
│  └─ 示例修复：
│      // ❌ 静态初始化
│      static const auto kConstant = Set(d, 1.0f);
│
│      // ✅ 函数局部
│      auto GetConstant(D d) { return Set(d, 1.0f); }
│
├─ 第 3 步：检查目标混合
│  │
│  └─ [ ] 是否在不同目标之间传递向量？
│       └─→ ❌ Vec<AVX2> 传给 SSE2 函数会崩溃
│           ✅ 确保函数在同一 HWY_NAMESPACE 中
│
└─ 第 4 步：使用 GDB 定位
   │
   └─ 调试命令：
       gdb ./my_program
       (gdb) run
       (gdb) bt        # 查看崩溃栈
       (gdb) info reg  # 查看寄存器
       (gdb) disas     # 查看汇编

       # 找到非法指令（如 AVX2 但 CPU 不支持）
```

### SIGSEGV (Segmentation Fault) 调试流程

```
SIGSEGV 崩溃
│
├─ 第 1 步：确定崩溃位置
│  │
│  └─ 使用 GDB/LLDB：
│      gdb ./my_program
│      (gdb) run
│      (gdb) bt        # 查看调用栈
│      (gdb) frame 0   # 进入崩溃的栈帧
│      (gdb) info locals  # 查看局部变量
│
├─ 第 2 步：检查对齐问题
│  │
│  ├─ 症状：Load() 崩溃，但 LoadU() 正常
│  │  └─→ 数据未对齐
│  │
│  ├─ 检查方法：
│  │  printf("地址: %p, 对齐: %zu\n",
│  │         ptr, (size_t)ptr % HWY_ALIGNMENT);
│  │  // 对齐地址应该能被 HWY_ALIGNMENT 整除
│  │
│  └─ 解决方案：
│      // ❌ 普通分配（可能未对齐）
│      float* data = new float[100];
│
│      // ✅ 对齐分配
│      AlignedUniquePtr<float[]> data = AllocateAligned<float>(100);
│
│      // 或使用 LoadU
│      auto v = LoadU(d, unaligned_ptr);
│
├─ 第 3 步：检查越界访问
│  │
│  ├─ 症状：崩溃在数组末尾附近
│  │
│  ├─ 常见错误：
│  │  for (size_t i = 0; i < count; i += N) {
│  │    auto v = Load(d, &data[i]);  // 最后可能越界！
│  │  }
│  │
│  └─ 解决方案（3 种）：
│
│      // 方案 1：确保边界
│      for (size_t i = 0; i + N <= count; i += N) { ... }
│
│      // 方案 2：填充数组
│      AllocateAligned<float>(RoundUp(count, N));
│
│      // 方案 3：掩码加载
│      if (i < count) {
│        auto mask = FirstN(d, count - i);
│        auto v = MaskedLoadOr(Zero(d), mask, d, &data[i]);
│      }
│
└─ 第 4 步：使用 AddressSanitizer
   │
   └─ 编译并运行：
       cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=address -g"
       make
       ./my_program

       # ASan 会精确报告：
       # - 越界访问的位置
       # - 访问的大小
       # - 调用栈
```

### Sanitizer 报错处理

```
Sanitizer 报错
│
├─ AddressSanitizer (ASan)
│  │
│  ├─ heap-buffer-overflow
│  │  └─→ 数组越界
│  │      → 检查循环边界
│  │      → 使用掩码或填充
│  │
│  ├─ stack-buffer-overflow
│  │  └─→ 栈数组太小
│  │      → 增大数组或使用堆分配
│  │
│  └─ use-after-free
│     └─→ 访问已释放的内存
│         → 检查对象生命周期
│
├─ MemorySanitizer (MSan)
│  │
│  └─ use-of-uninitialized-value
│     └─→ 使用未初始化的向量
│
│         // ❌ 未初始化
│         Vec<D> v;
│         Store(v, d, dest);
│
│         // ✅ 初始化
│         auto v = Zero(d);  // 或 Load(d, src)
│
└─ UndefinedBehaviorSanitizer (UBSan)
   │
   ├─ signed-integer-overflow
   │  └─→ 有符号整数溢出
   │      → 使用无符号类型或饱和运算
   │
   └─ shift-exponent-overflow
      └─→ 移位量超出范围
          → 检查移位值是否合法
```

---

## ⚡ 性能问题诊断

### 性能不理想决策树

```
SIMD 代码性能不佳
│
├─ 与标量代码对比？
│  │
│  ├─ SIMD 更慢
│  │  │
│  │  ├─ 检查 1：编译模式
│  │  │  └─ Debug 模式？
│  │  │     └─→ ❌ 改为 Release (-O3)
│  │  │
│  │  ├─ 检查 2：数据大小
│  │  │  └─ 数据少于 64 元素？
│  │  │     └─→ ⚠️ SIMD 开销 > 收益
│  │  │         对小数据使用标量
│  │  │
│  │  ├─ 检查 3：计算强度
│  │  │  └─ 只是简单的拷贝？
│  │  │     └─→ ⚠️ 内存带宽瓶颈
│  │  │         增加计算量
│  │  │
│  │  └─ 检查 4：水平操作
│  │     └─ 频繁使用 ReduceSum？
│  │        └─→ ❌ 移到循环外
│  │
│  ├─ SIMD 快，但不够快
│  │  │
│  │  ├─ 检查 1：FMA 使用
│  │  │  └─ 使用 Mul + Add？
│  │  │     └─→ 改用 MulAdd (FMA)
│  │  │
│  │  ├─ 检查 2：循环展开
│  │  │  └─ 单累加器？
│  │  │     └─→ 使用 2-4 个累加器
│  │  │
│  │  ├─ 检查 3：内存访问
│  │  │  └─ 随机访问？
│  │  │     └─→ 改为顺序访问
│  │  │         考虑预取 (Prefetch)
│  │  │
│  │  └─ 检查 4：数据布局
│  │     └─ 使用 AoS？
│  │        └─→ 改为 SoA 布局
│  │
│  └─ 性能不稳定
│     │
│     ├─ 波动 > 10%？
│     │  └─→ CPU 频率调节
│     │
│     └─ 解决方案：
│         # Linux
│         sudo cpupower frequency-set --governor performance
│
│         # 固定 CPU
│         taskset -c 0 ./benchmark
│
└─ 分析性能瓶颈
   └─→ 转到：性能分析工作流
```

### 性能分析工作流

```
性能分析
│
├─ 第 1 步：微基准测试
│  │
│  └─ 使用 Google Benchmark：
│
│      #include <benchmark/benchmark.h>
│
│      static void BM_VectorSum(benchmark::State& state) {
│        const size_t N = state.range(0);
│        auto data = AllocateAligned<float>(N);
│
│        for (auto _ : state) {
│          float result = VectorSum(data.get(), N);
│          benchmark::DoNotOptimize(result);
│        }
│
│        state.SetBytesProcessed(state.iterations() * N * sizeof(float));
│      }
│      BENCHMARK(BM_VectorSum)->Range(1<<10, 1<<20);
│
├─ 第 2 步：CPU 性能计数器
│  │
│  └─ 使用 perf (Linux)：
│
│      # 基本性能统计
│      perf stat ./my_benchmark
│
│      # 查看详细事件
│      perf stat -e cycles,instructions,cache-misses,cache-references ./my_benchmark
│
│      # 计算 IPC (Instructions Per Cycle)
│      # IPC > 2.0 表示良好
│      # IPC < 1.0 表示有瓶颈
│
├─ 第 3 步：热点分析
│  │
│  └─ 使用 perf record/report：
│
│      # 记录性能数据
│      perf record -g ./my_program
│
│      # 查看报告
│      perf report
│
│      # 查找花费最多时间的函数
│      # → 优化热点函数
│
├─ 第 4 步：汇编检查
│  │
│  ├─ Compiler Explorer (godbolt.org)
│  │  └─ 在线查看生成的汇编
│  │      → 确认向量化成功
│  │      → 检查是否使用了预期的指令
│  │
│  └─ 本地检查：
│
│      # 生成汇编
│      g++ -S -O3 -mavx2 -masm=intel my_file.cc
│
│      # 查看特定函数
│      objdump -d my_program | grep -A 50 MyFunction
│
│      # 检查项：
│      # - 是否有向量指令 (vaddps, vmulps 等)
│      # - 是否有不必要的标量代码
│      # - 循环是否展开
│
└─ 第 5 步：微架构分析
   │
   └─ 使用 Intel VTune 或 AMD uProf：

       # VTune 基本分析
       vtune -collect hotspots ./my_program

       # 微架构分析
       vtune -collect uarch-exploration ./my_program

       # 查看报告
       vtune-gui

       # 关注指标：
       # - Backend Bound (> 50%: 执行瓶颈)
       # - Frontend Bound (指令获取问题)
       # - Memory Bound (内存访问瓶颈)
```

### 性能优化检查清单

```
□ 编译优化
  ├─ [ ] 使用 -O3 或 /O2
  ├─ [ ] 启用目标特定标志 (-mavx2, -march=native)
  └─ [ ] 链接时优化 (LTO)

□ 算法优化
  ├─ [ ] 使用 MulAdd 代替 Mul + Add
  ├─ [ ] 循环展开 (2-4x)
  ├─ [ ] 多累加器隐藏延迟
  └─ [ ] 减少水平操作 (ReduceSum)

□ 内存优化
  ├─ [ ] 数据对齐到 HWY_ALIGNMENT
  ├─ [ ] 使用 SoA 布局代替 AoS
  ├─ [ ] 顺序访问，避免 Gather/Scatter
  ├─ [ ] 添加预取 (Prefetch)
  └─ [ ] 缓存分块 (Tiling)

□ 平台优化
  ├─ [ ] 为目标平台选择合适的标签
  ├─ [ ] 使用平台特定优化 (#if HWY_TARGET == ...)
  └─ [ ] 避免不支持的操作

□ 验证
  ├─ [ ] 查看汇编代码
  ├─ [ ] 性能计数器显示高 IPC
  ├─ [ ] 与理论峰值对比
  └─ [ ] 跨平台测试
```

---

## 🐛 逻辑错误排查

### 结果不正确决策树

```
输出结果不正确
│
├─ 与标量版本对比
│  │
│  ├─ 完全不同
│  │  │
│  │  ├─ 检查 1：余数处理
│  │  │  └─ 是否处理了非向量大小的余项？
│  │  │
│  │  │     // ✅ 正确处理余数
│  │  │     size_t i = 0;
│  │  │     for (; i + N <= count; i += N) { /* SIMD */ }
│  │  │     for (; i < count; ++i) { /* 标量 */ }
│  │  │
│  │  ├─ 检查 2：掩码操作
│  │  │  └─ 掩码是否初始化未使用的通道？
│  │  │
│  │  │     // ❌ 错误
│  │  │     auto v = MaskedLoad(mask, d, ptr);  // 未掩码通道未定义
│  │  │     auto result = Add(v, other);  // 未定义值参与运算
│  │  │
│  │  │     // ✅ 正确
│  │  │     auto v = MaskedLoadOr(Zero(d), mask, d, ptr);
│  │  │
│  │  └─ 检查 3：向量大小假设
│  │     └─ 是否硬编码了向量大小？
│  │
│  │        // ❌ 假设 8 个 float (AVX2)
│  │        for (size_t i = 0; i < count; i += 8) { ... }
│  │        // RVV 上会出错！
│  │
│  │        // ✅ 使用 Lanes(d)
│  │        const size_t N = Lanes(d);
│  │        for (size_t i = 0; i < count; i += N) { ... }
│  │
│  └─ 略有差异
│     │
│     ├─ 浮点精度差异
│     │  └─ SIMD 改变了运算顺序？
│     │
│     │     // 标量: ((a + b) + c) + d
│     │     // SIMD:  (a + b) + (c + d)  ← 不同的关联性
│     │
│     │     解决方案：
│     │     - 接受小误差 (< 1e-5)
│     │     - 使用双精度
│     │     - 使用 Kahan 求和算法
│     │
│     └─ 有符号/无符号混淆
│        └─ 是否错误地使用了无符号比较？
│
│           // ❌ 有符号数据，无符号比较
│           auto mask = Gt(BitCast(du, signed_vec), threshold);
│           // -1 会被当作很大的正数！
│
│           // ✅ 使用正确的类型
│           auto mask = Gt(signed_vec, threshold);
│
└─ 特定输入失败
   │
   ├─ 边界值
   │  └─ 测试：0, -0, NaN, ±Inf, MIN, MAX
   │
   │     // 特别注意 NaN 的处理
   │     // NaN 与任何值比较都是 false
   │
   └─ 对齐问题
      └─ 数据未对齐时行为不同？
         → 统一使用 LoadU 和 StoreU
```

### 调试技术

#### 打印向量内容

```cpp
#include "hwy/print-inl.h"

namespace hwy {
namespace HWY_NAMESPACE {

void MyFunction() {
  const ScalableTag<float> d;
  auto v = Set(d, 3.14f);

  // 打印向量
  Print(d, "my_vector", v);
  // 输出: my_vector: [3.14, 3.14, 3.14, ...]
}

}  // namespace HWY_NAMESPACE
}  // namespace hwy
```

#### 使用断言

```cpp
#include "hwy/tests/test_util.h"

// 比较向量是否相等
HWY_ASSERT_VEC_EQ(d, expected, actual);

// 检查条件
HWY_ASSERT(AllTrue(d, Gt(v, Zero(d))));

// 比较标量
HWY_ASSERT_EQ(42, GetLane(v));
```

#### 单步调试 SIMD 代码

```bash
# GDB 查看向量寄存器 (x86)
gdb ./my_program
(gdb) break MyFunction
(gdb) run
(gdb) info registers xmm0  # SSE
(gdb) info registers ymm0  # AVX
(gdb) info registers zmm0  # AVX-512

# LLDB (macOS/Linux)
lldb ./my_program
(lldb) b MyFunction
(lldb) run
(lldb) register read xmm0

# 以数组形式查看
(lldb) memory read -f f -c 8 &vec
```

---

## 🌐 平台特定问题

### 跨平台兼容性检查

```
代码在某平台失败
│
├─ 编译失败
│  │
│  ├─ ARM 平台
│  │  └─ 缺少某些 intrinsics？
│  │     → Highway 可能已经模拟
│  │     → 检查 HWY_NATIVE_* 宏
│  │
│  ├─ RISC-V 平台
│  │  └─ RVV 支持？
│  │     → 需要 GCC 13+ 或 Clang 17+
│  │     → 确认 -march=rv64gcv
│  │
│  └─ WebAssembly
│     └─ SIMD 支持？
│        → 需要启用 -msimd128
│        → 检查浏览器支持
│
├─ 运行时失败
│  │
│  ├─ 功能测试失败
│  │  └─ 某个操作不支持？
│  │
│  │     // 检查是否原生支持
│  │     #if HWY_NATIVE_DOT_BF16
│  │       // 使用硬件指令
│  │     #else
│  │       // 软件模拟
│  │     #endif
│  │
│  └─ 性能差异
│     └─ 某平台特别慢？
│        → 检查该平台的指令成本
│        → 可能需要不同的算法
│
└─ 数值差异
   └─ 浮点行为不同？
      → IEEE 754 合规性
      → Flush-to-zero 模式
      → 舍入模式
```

### 平台特定优化

```cpp
// 根据平台选择策略
#if HWY_TARGET == HWY_AVX3
  // AVX-512 有 Compress 硬件指令
  auto filtered = Compress(vec, mask);

#elif HWY_TARGET == HWY_NEON
  // NEON 需要模拟 Compress，使用表查找可能更快
  auto filtered = TableLookupLanes(vec, indices);

#else
  // 通用实现
  auto filtered = Compress(vec, mask);
#endif
```

---

## 📊 完整调试工作流

### 新问题调试标准流程

```
遇到新问题
│
├─ 第 1 阶段：信息收集 (5-10 分钟)
│  │
│  ├─ [ ] 记录完整错误信息
│  ├─ [ ] 记录复现步骤
│  ├─ [ ] 记录环境信息
│  │     - OS 和版本
│  │     - 编译器和版本
│  │     - Highway 版本
│  │     - CPU 型号
│  └─ [ ] 创建最小复现示例
│
├─ 第 2 阶段：初步诊断 (10-15 分钟)
│  │
│  ├─ [ ] 查阅文档
│  │     - FAQ.md
│  │     - COMMON_ERRORS.md
│  │     - 本故障排除指南
│  │
│  ├─ [ ] 搜索类似问题
│  │     - GitHub Issues
│  │     - Stack Overflow
│  │     - Highway Discussions
│  │
│  └─ [ ] 使用快速诊断决策树
│        → 定位问题类别
│
├─ 第 3 阶段：深入调查 (30-60 分钟)
│  │
│  ├─ 编译问题
│  │  └─ [ ] 检查编译命令
│  │     [ ] 验证宏定义
│  │     [ ] 简化代码到最小示例
│  │
│  ├─ 运行时问题
│  │  └─ [ ] 使用 Sanitizers
│  │     [ ] GDB/LLDB 调试
│  │     [ ] 添加打印语句
│  │
│  ├─ 性能问题
│  │  └─ [ ] 基准测试
│  │     [ ] 性能分析工具
│  │     [ ] 汇编检查
│  │
│  └─ 逻辑问题
│     └─ [ ] 单元测试
│        [ ] 对比标量实现
│        [ ] 边界条件测试
│
├─ 第 4 阶段：解决方案 (变动)
│  │
│  ├─ 找到解决方案
│  │  └─ [ ] 验证修复
│  │     [ ] 添加测试防止回归
│  │     [ ] 文档化（如果是常见问题）
│  │
│  └─ 未找到解决方案
│     └─ [ ] 准备详细报告
│        [ ] 在 GitHub 提问
│        [ ] 提供最小复现示例
│
└─ 第 5 阶段：预防 (可选)
   │
   └─ [ ] 分析根因
      [ ] 更新开发流程
      [ ] 添加静态检查
      [ ] 改进测试覆盖
```

### 提问模板

当你需要寻求帮助时，使用此模板：

```markdown
## 问题描述
[简洁描述问题]

## 环境信息
- OS: [Linux/macOS/Windows] [版本]
- 编译器: [GCC/Clang/MSVC] [版本]
- Highway: [版本或 commit]
- CPU: [型号]
- 目标: [HWY_AVX2 / HWY_NEON / 等]

## 最小复现示例
```cpp
#include "hwy/highway.h"

namespace hwy {
namespace HWY_NAMESPACE {

void ReproduceIssue() {
  const ScalableTag<float> d;
  // ... 最少的代码重现问题
}

}  // namespace HWY_NAMESPACE
}  // namespace hwy
```

## 编译命令
```bash
g++ -std=c++17 -O2 -mavx2 test.cc -o test -lhwy
```

## 实际行为
[描述发生了什么]

## 预期行为
[描述应该发生什么]

## 已尝试的解决方案
- [x] 查阅 FAQ
- [x] 使用 Sanitizers
- [ ] ...

## 额外信息
[日志、错误信息、汇编代码等]
```

---

## 📚 相关资源

**快速参考：**
- [常见错误](COMMON_ERRORS.md) - 17 个常见错误及修复
- [FAQ](FAQ.md) - 20+ 个常见问题
- [速查卡](CHEATSHEET.md) - 一页纸参考

**调试工具：**
- [快速参考](快速参考.md) - API 完整列表
- [术语表](GLOSSARY.md) - 100+ 术语定义

**性能优化：**
- [高级主题](02_ADVANCED_TOPICS.md) - 性能优化模式
- [平台对比](PLATFORM_COMPARISON.md) - 性能特性对比

---

## 💡 调试最佳实践

### 黄金法则

1. **始终从最小示例开始** - 隔离问题比在大代码库中调试快 10 倍
2. **先读文档，再调试** - 90% 的问题已经有文档记录
3. **使用正确的工具** - Sanitizers 比 printf 调试快 100 倍
4. **记录你的发现** - 帮助他人，也帮助未来的自己
5. **不要假设** - 验证每一个假设

### 调试心态

```
遇到问题时：
✅ "这是学习的机会"
❌ "这个库有 bug"

✅ "我可能哪里理解错了"
❌ "编译器一定有问题"

✅ "让我查一下文档"
❌ "我试试瞎改能不能工作"

✅ "我创建一个最小示例"
❌ "我把整个项目贴上去求助"
```

---

**快速提醒：**

- 🔍 **遇到错误？** → 使用"快速诊断决策树"
- ⚡ **性能不佳？** → 使用"性能问题诊断"
- 🐛 **结果错误？** → 使用"逻辑错误排查"
- 💬 **需要帮助？** → 使用"提问模板"

---

*最后更新：2026-01-19 | Highway v1.3.0*
