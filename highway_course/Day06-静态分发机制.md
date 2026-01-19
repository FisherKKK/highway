# Day 6: 静态分发机制

## 课程目标

今天我们将深入理解 Highway 的**静态分发机制**（Static Dispatch），学习如何为特定的 CPU 目标编译代码，以及零开销的函数调用。

---

## 6.1 什么是静态分发？

静态分发是指在**编译时**确定 CPU 目标，直接调用对应的函数实现，**零运行时开销**。

```
编译时目标选择              运行时直接调用
     │                        │
     ▼                        ▼
┌────────────────┐        ┌──────────────────┐
│   源代码     │  ──►  │  AVX2 代码     │
│              │        │  (已编译好的机器码）
└────────────────┘        └──────────────────┘
```

### 适用场景

| 场景 | 原因 |
|------|------|
| 嵌入式系统 | 硬件固定，无需动态检测 |
| 高性能内核 | 零开销调用 |
| 静态库 | 分发由调用方负责 |

---

## 6.2 HWY_STATIC_TARGET

`HWY_STATIC_TARGET` 是编译时的目标宏：

```cpp
// 根据编译器参数，自动设置为最佳目标
// 例如：编译时使用 -march=haswell，则 HWY_STATIC_TARGET = HWY_AVX2

// 手动指定目标（CMake）
cmake .. -DHWY_BASELINE_TARGETS=HWY_AVX2
```

### 目标宏映射

```cpp
// highway.h 中的静态目标命名空间映射
#if HWY_STATIC_TARGET == HWY_SCALAR
#define HWY_STATIC_NAMESPACE N_SCALAR
#elif HWY_STATIC_TARGET == HWY_NEON
#define HWY_STATIC_NAMESPACE N_NEON
#elif HWY_STATIC_TARGET == HWY_AVX2
#define HWY_STATIC_NAMESPACE N_AVX2
#elif HWY_STATIC_TARGET == HWY_AVX3
#define HWY_STATIC_NAMESPACE N_AVX3
// ... 其他目标
#endif
```

---

## 6.3 HWY_STATIC_DISPATCH

`HWY_STATIC_DISPATCH` 宏用于直接调用静态目标函数：

```cpp
// highway.h
#define HWY_STATIC_DISPATCH(FUNC_NAME) HWY_STATIC_NAMESPACE::FUNC_NAME
```

### 基本用法

```cpp
#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

void MyFunction(float* data, size_t count) {
  const ScalableTag<float> df;
  const size_t N = Lanes(df);

  for (size_t i = 0; i < count; i += N) {
    auto v = Load(df, &data[i]);
    v = Mul(v, v);
    Store(v, df, &data[i]);
  }
}

}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

// 调用静态分发的函数
void CallMyFunction(float* data, size_t count) {
  // 直接调用，零开销
  HWY_STATIC_DISPATCH(MyFunction)(data, count);
}
```

### 实际展开

假设 `HWY_STATIC_TARGET = HWY_AVX2`：

```cpp
// HWY_STATIC_DISPATCH(MyFunction)
// 展开为：
N_AVX2::MyFunction(data, count);
```

---

## 6.4 HWY_BEFORE_NAMESPACE / HWY_AFTER_NAMESPACE

这两个宏用于包装 SIMD 函数：

```cpp
// highway.h
#define HWY_BEFORE_NAMESPACE() HWY_ATTR

// 高速属性：强制内联，传递向量参数
// （根据编译器不同，定义不同）
#if HWY_COMPILER_GCC || HWY_COMPILER_CLANG
#define HWY_ATTR __attribute__((always_inline))
#else
#define HWY_ATTR __forceinline
#endif

#define HWY_AFTER_NAMESPACE()
```

### 为什么需要它们？

```cpp
// 正确写法
HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

void MyVectorFunction(Vec<decltype(df)> v) {
  // HWY_ATTR 确保函数被内联
  // 避免非内联函数传递向量参数的问题
}

}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();
```

如果不使用这些宏：
- 在某些平台上（如 GCC + Windows），传递向量参数到非内联函数可能导致崩溃
- 函数可能不被内联，影响性能

---

## 6.5 完整静态分发示例

### my_code.h - 头文件

```cpp
#ifndef MY_CODE_H_
#define MY_CODE_H_

#include "hwy/base.h"

namespace my_code {

// 声明接口（独立于 SIMD 实现）
HWY_DLLEXPORT void ProcessArray(float* data, size_t count);
HWY_DLLEXPORT float SumArray(const float* data, size_t count);

}  // namespace my_code

#endif  // MY_CODE_H_
```

### my_code.cc - 实现文件

```cpp
#include "my_code.h"

// 包含 Highway 主头文件
#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace my_code {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;  // 命名空间别名

void ProcessArray(float* data, size_t count) {
  const ScalableTag<float> df;
  const size_t N = Lanes(df);

  for (size_t i = 0; i < count; i += N) {
    auto v = Load(df, &data[i]);
    v = Mul(v, v);  // 平方
    Store(v, df, &data[i]);
  }
}

float SumArray(const float* data, size_t count) {
  const ScalableTag<float> df;
  const size_t N = Lanes(df);

  Vec<decltype(df)> sum = Zero(df);

  for (size_t i = 0; i + N <= count; i += N) {
    auto v = Load(df, &data[i]);
    sum = Add(sum, v);
  }

  // 处理剩余元素
  if (count % N != 0) {
    auto v = LoadU(df, &data[count - N]);
    sum = Add(sum, v);
  }

  sum = SumOfLanes(df, sum);
  return GetLane(sum, 0);
}

}  // namespace HWY_NAMESPACE
}  // namespace my_code
HWY_AFTER_NAMESPACE();

// 导出包装函数（直接调用静态目标）
namespace my_code {

HWY_DLLEXPORT void ProcessArray(float* data, size_t count) {
  // 直接调用，零开销
  HWY_STATIC_DISPATCH(ProcessArray)(data, count);
}

HWY_DLLEXPORT float SumArray(const float* data, size_t count) {
  return HWY_STATIC_DISPATCH(SumArray)(data, count);
}

}  // namespace my_code
```

---

## 6.6 编译静态分发代码

### CMake 配置

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(MyStaticLib)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)

# 查找 Highway
find_path(highway REQUIRED)

# 可选：指定静态目标
# 默认：编译器自动选择最佳目标
# 可选选项：
# -DHWY_BASELINE_TARGETS=HWY_AVX2       # AVX2
# -DHWY_BASELINE_TARGETS=HWY_AVX3       # AVX-512
# -DHWY_BASELINE_TARGETS=HWY_NEON       # ARM NEON
# -DHWY_BASELINE_TARGETS=HWY_SSE2       # SSE2
# -DHWY_COMPILE_ONLY_SCALAR=ON          # 仅标量

# 创建库
add_library(my_static_lib SHARED
    my_code.cc
)

target_link_libraries(my_static_lib
    highway
)
```

### 编译命令

```bash
# 编译（自动检测最佳目标）
mkdir build && cd build
cmake ..
make -j

# 指定目标
cmake .. -DHWY_BASELINE_TARGETS=HWY_AVX2
make -j

# 仅标量（用于测试）
cmake .. -DHWY_COMPILE_ONLY_SCALAR=ON
make -j
```

---

## 6.7 静态分发的命名空间

每个目标都有独立的命名空间：

```cpp
namespace hwy {
// 静态目标命名空间
#if HWY_STATIC_TARGET == HWY_SCALAR
namespace N_SCALAR { /* ... */ }
#elif HWY_STATIC_TARGET == HWY_SSE2
namespace N_SSE2 { /* ... */ }
#elif HWY_STATIC_TARGET == HWY_AVX2
namespace N_AVX2 { /* ... */ }
#elif HWY_STATIC_TARGET == HWY_NEON
namespace N_NEON { /* ... */ }
// ...
#endif
}  // namespace hwy
```

### 命名空间隔离的好处

```cpp
// 同一个编译单元可以有多个目标的代码
// 但静态分发只使用 HWY_STATIC_TARGET

namespace hwy {
namespace HWY_NAMESPACE {  // 映射到 HWY_STATIC_TARGET
  // 当前编译目标的代码
}

#if HWY_TARGET != HWY_STATIC_TARGET
// 其他目标的代码（当使用 foreach_target.h）
namespace N_SSE2 { /* SSE2 代码 */ }
namespace N_AVX2 { /* AVX2 代码 */ }
// ...
#endif
}  // namespace hwy
```

---

## 6.8 性能优势

### 零开销

```cpp
// 静态分发：编译时确定
HWY_STATIC_DISPATCH(MyFunction)(args);
// 等价于直接调用
N_AVX2::MyFunction(args);

// 无运行时检测，无函数指针查找
// 编译器可以完全内联
```

### 优势对比

| 特性 | 静态分发 | 动态分发 |
|------|----------|----------|
| 运行时开销 | 0 | 首次调用检测 + 指针查找 |
| 二进制大小 | 1 个实现 | 多个实现 |
| 硬件兼容性 | 单目标 | 多目标 |
| 内联可能性 | 完全内联 | 受限 |
| 适用场景 | 嵌入式、内核 | 桌面应用、服务器 |

---

## 6.9 完整示例：数学库静态分发

```cpp
// math_lib.cc
#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace math_lib {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;

// 向量化 exp(x) 近似
Vec<decltype(df)> Exp(Vec<decltype(df)> x) {
  const ScalableTag<float> df;

  // 简化版 exp：exp(x) ≈ 2^(x / ln(2))
  const float ln2_inv = 1.44269504f;
  const float one = 1.0f;

  auto v_ln2_inv = Set(df, ln2_inv);
  auto v_one = Set(df, one);

  // x / ln(2)
  auto v_exp_input = Mul(x, v_ln2_inv);

  // 四舍五入到整数
  auto v_exp_int = ConvertTo(RebindToSigned<decltype(df)>(),
                          v_exp_input);

  // 2^n（使用移位）
  auto v_exp_bits = ShiftRight<23>(v_exp_int);
  auto v_exp_value = BitCast(df, Add(v_exp_bits,
                                     ShiftRight<23>(Set(df, 127.0f)));

  // 线性插值校正
  // ...（完整实现更复杂）

  return v_exp_value;
}

// 数组 exp
void ExpArray(const float* input, float* output, size_t count) {
  const ScalableTag<float> df;
  const size_t N = Lanes(df);

  for (size_t i = 0; i + N <= count; i += N) {
    auto v = Load(df, &input[i]);
    v = Exp(v);
    Store(v, df, &output[i]);
  }
}

}  // namespace HWY_NAMESPACE
}  // namespace math_lib
HWY_AFTER_NAMESPACE();

// 导出
namespace math_lib {

HWY_DLLEXPORT void ExpArray(const float* input, float* output,
                          size_t count) {
  HWY_STATIC_DISPATCH(ExpArray)(input, output, count);
}

}  // namespace math_lib
```

---

## 6.10 使用静态分发的最佳实践

### 1. 头文件保持独立

```cpp
// 好的做法
// my_header.h
namespace my_lib {
HWY_DLLEXPORT void Process(float* data, size_t count);
}

// 实现在 .cc 中
// my_implementation.cc
HWY_STATIC_DISPATCH(Process)(data, count);
```

### 2. 编译器标志

```bash
# 确保启用优化
cmake .. -DCMAKE_BUILD_TYPE=Release

# 对于 GCC/Clang
g++ -O2 -march=native ...

# 对于 MSVC
cl /O2 /arch:AVX2 ...
```

### 3. 调试时使用标量

```cmake
# Debug 构建：使用标量（便于调试）
cmake .. -DCMAKE_BUILD_TYPE=Debug -DHWY_COMPILE_ONLY_SCALAR=ON

# Release 构建：使用最佳目标
cmake .. -DCMAKE_BUILD_TYPE=Release
```

---

## 本日总结

| 概念 | 说明 |
|------|------|
| **HWY_STATIC_TARGET** | 编译时确定的目标（如 HWY_AVX2）|
| **HWY_STATIC_DISPATCH** | 静态分发宏，零开销调用 |
| **HWY_BEFORE/AFTER_NAMESPACE** | 包装 SIMD 函数的属性 |
| **HWY_STATIC_NAMESPACE** | 静态目标的命名空间（如 N_AVX2）|
| **编译时决定** | 无运行时检测，完全内联 |
| **适用场景** | 嵌入式、高性能内核、静态库 |

---

## 练习

1. 编写一个静态分发的函数，计算数组的标准差
2. 使用 CMake 配置项目，支持选择不同的静态目标
3. 比较静态分发与动态分发的性能差异

---

## 下一步

明天我们将学习动态分发机制，了解如何在运行时检测 CPU 并选择最佳实现。
