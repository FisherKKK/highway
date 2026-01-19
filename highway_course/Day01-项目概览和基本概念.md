# Day 1: Highway SIMD 库概览和基本概念

## 课程目标

今天我们将了解 Highway SIMD 库的整体架构、设计理念以及它解决的问题。

---

## 1.1 什么是 Highway？

Highway 是一个 C++ 库，提供**跨平台的 SIMD/vector 指令抽象层**。它允许你编写一次代码，然后在多种 CPU 架构上运行（x86、ARM、RISC-V、POWER、WebAssembly 等）。

### 核心价值

```cpp
// 一次编写，到处运行
#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

void MySIMDFunction(float* data, size_t count) {
  const ScalableTag<float> df;  // 全向量宽度
  const size_t N = Lanes(df);

  for (size_t i = 0; i < count; i += N) {
    auto v = Load(df, &data[i]);  // 自动适配最佳指令集
    v = Mul(v, v);
    Store(v, df, &data[i]);
  }
}

}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();
```

---

## 1.2 为什么需要 Highway？

### 传统 SIMD 开发的痛点

| 问题 | 描述 |
|------|------|
| **平台碎片化** | SSE、AVX、NEON、SVE、RVV... 每个平台有不同指令集 |
| **代码重复** | 为每个平台写相同的算法逻辑 |
| **维护困难** | 平台更新需要同步修改多个实现 |
| **可移植性差** | ARM 设备无法使用 x86 代码 |

### Highway 的解决方案

```
┌─────────────────────────────────────────────────────────────┐
│                  你的代码                              │
│            (平台无关的 SIMD 操作)                      │
└────────────────────┬────────────────────────────────────┘
                     │ Highway 抽象层
                     ▼
┌─────────────────────────────────────────────────────────────┐
│  Highway 核心                                     │
│  - Load/Store                                      │
│  - 算术运算                                        │
│  - 比较操作                                        │
│  - 位操作                                           │
└──────┬────────────────────────────┬───────────────────┘
       │                        │
       ▼                        ▼
┌──────────────┐        ┌──────────────────┐
│  x86 后端    │        │  ARM 后端       │
│ SSE/AVX/AVX2  │        │ NEON/SVE/SVE2  │
└──────────────┘        └──────────────────┘
```

---

## 1.3 项目目录结构

```
hwy/
├── highway.h              ← 主入口，用户只需包含这个
├── base.h                 ← 平台无关的工具函数
├── targets.h              ← 目标检测和选择
├── detect_targets.h       ← 编译时目标检测
├── foreach_target.h       ← 动态分发核心机制
├── aligned_allocator.h    ← SIMD 对齐内存分配
│
├── ops/                  ← 平台特定实现
│   ├── x86_128-inl.h     ← SSE2/SSSE3/SSE4 实现
│   ├── x86_256-inl.h     ← AVX2 实现
│   ├── x86_512-inl.h     ← AVX-512 实现
│   ├── arm_neon-inl.h     ← ARM NEON 实现
│   ├── arm_sve-inl.h      ← ARM SVE 实现
│   ├── rvv-inl.h          ← RISC-V RVV 实现
│   └── ...               ← 其他架构
│
├── contrib/               ← 高级算法库
│   ├── algo/             ← 变换、拷贝、查找
│   ├── sort/             ← VQSort 向量化排序
│   ├── math/             ← 数学函数 (sin/cos/exp)
│   ├── thread_pool/      ← 线程池
│   └── ...               ← 其他模块
│
├── tests/                ← 单元测试
└── examples/             ← 示例代码
```

---

## 1.4 支持的架构

### x86 系列

```cpp
HWY_SSE2     // SSE2 (128-bit)
HWY_SSSE3    // SSSE3
HWY_SSE4     // SSE4.1/4.2
HWY_AVX2     // AVX2 (256-bit)
HWY_AVX3     // AVX-512 (512-bit)
HWY_AVX3_DL  // AVX-512 VNNI/VL
HWY_AVX3_ZEN4 // AVX-512 ZEN4
HWY_AVX3_SPR // AVX-512 SPR
HWY_AVX10_2  // AVX10.2
```

### ARM 系列

```cpp
HWY_NEON             // ARM NEON (128-bit)
HWY_NEON_WITHOUT_AES // NEON without AES
HWY_NEON_BF16        // NEON with bf16/dot
HWY_SVE              // SVE (可变长度)
HWY_SVE2             // SVE2
HWY_SVE_256          // SVE with 256-bit minimum
HWY_SVE2_128         // SVE2 with 128-bit maximum
```

### 其他架构

| 架构 | 目标 | 描述 |
|------|------|------|
| RISC-V | `HWY_RVV` | RISC-V Vector Extension |
| POWER | `HWY_PPC8/PPC9/PPC10` | POWER VSX |
| LoongArch | `HWY_LSX/LASX` | 龙芯架构 |
| WebAssembly | `HWY_WASM` | WASM SIMD (128-bit) |
| Scalar | `HWY_SCALAR` | 可移植的标量回退 |

---

## 1.5 分发机制

Highway 支持两种分发模式：

### 静态分发（零运行时开销）

```cpp
// 编译时确定目标
HWY_STATIC_DISPATCH(MyFunction)(args);
```

- 优点：零开销，直接调用
- 缺点：需要编译时知道目标
- 适用场景：嵌入式、已知硬件

### 动态分发（运行时选择）

```cpp
// 运行时检测 CPU 功能并选择最佳目标
HWY_DYNAMIC_DISPATCH(MyFunction)(args);
```

- 优点：同一二进制支持多种 CPU
- 缺点：首次调用有少量检测开销
- 适用场景：桌面应用、服务器

---

## 1.6 Hello Highway 示例

让我们看一个完整的例子（来自 `examples/skeleton.cc`）：

```cpp
// skeleton.h - 公共接口
HWY_DLLEXPORT void CallFloorLog2(const uint8_t* HWY_RESTRICT in,
                               size_t count,
                               uint8_t* HWY_RESTRICT out);
```

```cpp
// skeleton.cc - 实现文件
#define HWY_TARGET_INCLUDE "hwy/examples/skeleton.cc"
#include "hwy/foreach_target.h"  // 为每个目标重新编译

#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace skeleton {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;  // 命名空间别名

void FloorLog2(const uint8_t* values, size_t count,
               uint8_t* log2) {
  const hn::ScalableTag<float> df;
  const size_t N = hn::Lanes(df);

  for (size_t i = 0; i < count; i += N) {
    // 使用浮点指令计算整数对数
    const auto vi32 = hn::PromoteTo(hn::RebindToSigned<decltype(df)>(),
                                  hn::Load(df, values + i));
    const auto bits = hn::BitCast(df, hn::ConvertTo(df, vi32));
    const auto exponent = hn::Sub(hn::ShiftRight<23>(bits),
                                hn::Set(df, 127));
    hn::Store(hn::DemoteTo(df, exponent), df, log2 + i);
  }
}

}  // namespace HWY_NAMESPACE
}  // namespace skeleton
HWY_AFTER_NAMESPACE();

// 导出函数指针表
#if HWY_ONCE
namespace skeleton {
HWY_EXPORT(FloorLog2);

// 包装函数供外部调用
HWY_DLLEXPORT void CallFloorLog2(const uint8_t* in,
                                 size_t count,
                                 uint8_t* out) {
  return HWY_DYNAMIC_DISPATCH(FloorLog2)(in, count, out);
}
}
#endif
```

---

## 1.7 编译和构建

### CMake 构建

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
```

### 运行测试

```bash
make test
# 或使用完整测试脚本
./run_tests.sh
```

---

## 本日总结

| 概念 | 要点 |
|------|------|
| **Highway** | 跨平台 SIMD 抽象库 |
| **统一 API** | 一套代码，多平台运行 |
| **分发机制** | 静态（零开销）和动态（运行时选择） |
| **支持架构** | x86、ARM、RISC-V、POWER、WASM 等 15+ 架构 |
| **核心文件** | `highway.h` 是主入口 |

---

## 下一步

明天我们将深入探索 `base.h` 和核心工具函数，理解 Highway 的基础构建模块。
