# Day 7: 动态分发机制

## 课程目标

今天我们将深入理解 Highway 的**动态分发机制**（Dynamic Dispatch），学习如何在运行时检测 CPU 功能并选择最佳实现。

---

## 7.1 什么是动态分发？

动态分发是指在**运行时**检测 CPU 支持的指令集，然后选择对应的实现。同一二进制可以支持多种 CPU。

```
运行时检测和选择
     │
     ▼
┌─────────────────────────────────────────────┐
│  同一个二进制                          │
│                                        │
│  ┌─────────┬─────────┬─────────┐      │
│  │  SSE2   │  AVX2   │  AVX512  │      │
│  │ 代码    │  代码    │  代码    │      │
│  └─────────┴─────────┴─────────┘      │
│                                        │
│  检测 CPU → 选择最佳实现              │
└─────────────────────────────────────────────┘
```

### 适用场景

| 场景 | 原因 |
|------|------|
| 桌面应用 | 用户硬件多样 |
| 服务器集群 | 混合 CPU 型号 |
| 库分发 | 单二进制支持多平台 |
| 向后兼容 | 新硬件用新指令，旧硬件用旧指令 |

---

## 7.2 foreach_target.h 核心

`foreach_target.h` 是动态分发的核心机制。它会**重新包含**实现文件多次，每次为不同的目标编译。

### 基本原理

```cpp
// 实现文件 my_code.cc
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "my_code.cc"  // 指定自身
#include "hwy/foreach_target.h"  // 重新编译多次

// foreach_target.h 的工作：
// 1. 设置 HWY_TARGET 为第一个目标（如 HWY_SSE2）
// 2. #include HWY_TARGET_INCLUDE（即 #include "my_code.cc"）
// 3. HWY_NAMESPACE 映射到 N_SSE2，编译 SSE2 版本
// 4. 重复 1-3 对所有目标（HWY_AVX2, HWY_AVX3, ...）
// 5. 最后设置 HWY_TARGET 为 HWY_STATIC_TARGET
```

### include guard 切换

由于文件被多次包含，需要特殊的 include guard：

```cpp
// my_code-inl.h（可选：header-only 实现）
#if defined(HIGHWAY_MY_CODE_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef HIGHWAY_MY_CODE_INL_H_
#undef HIGHWAY_MY_CODE_INL_H_
#else
#define HIGHWAY_MY_CODE_INL_H_
#endif

// Highway 代码
#include "hwy/highway.h"

namespace my_code {
namespace HWY_NAMESPACE {
  // SIMD 实现
}

}  // namespace my_code

#endif  // HIGHWAY_MY_CODE_INL_H_
```

**HWY_TARGET_TOGGLE** 在每次包含时切换，使得 guard 可以工作。

---

## 7.3 HWY_EXPORT

`HWY_EXPORT` 宏创建一个函数指针表，用于动态分发：

```cpp
// highway.h（简化）
#define HWY_EXPORT(FUNC_NAME) \
  static decltype(&HWY_STATIC_DISPATCH(FUNC_NAME)) const \
  HWY_DISPATCH_TABLE(FUNC_NAME)[HWY_MAX_DYNAMIC_TARGETS + 2] = { \
    /* 入口 0: 初始化缓存并分发的函数 */ \
    &FunctionCache<...>::ChooseAndCall<HWY_DISPATCH_TABLE(FUNC_NAME)>, \
    /* 入口 1-15: 各目标的函数指针 */ \
    HWY_CHOOSE_TARGET_LIST(FUNC_NAME), \
    /* 最后一个入口：回退目标 */ \
    HWY_CHOOSE_FALLBACK(FUNC_NAME) \
  }
```

### 函数指针表结构

```cpp
// 假设 x86，有 15 个动态目标 + 1 个回退
const void* MyFunction_Table[17] = {
  [0] = FunctionCache::ChooseAndCall,  // 首次调用初始化
  [1] = nullptr,                        // 保留
  [2] = nullptr,                        // 保留
  [3] = &N_AVX10_2::MyFunction,     // AVX10.2
  [4] = &N_AVX3_SPR::MyFunction,     // AVX3_SPR
  [5] = nullptr,                        // 保留
  [6] = &N_AVX3_ZEN4::MyFunction,   // AVX3_ZEN4
  [7] = &N_AVX3_DL::MyFunction,      // AVX3_DL
  [8] = &N_AVX3::MyFunction,        // AVX3
  [9] = &N_AVX2::MyFunction,        // AVX2
  [10] = nullptr,                       // AVX
  [11] = &N_SSE4::MyFunction,        // SSE4
  [12] = &N_SSSE3::MyFunction,       // SSSE3
  [13] = nullptr,                       // SSE3
  [14] = &N_SSE2::MyFunction,        // SSE2
  [15] = &N_SCALAR::MyFunction       // 回退
};
```

---

## 7.4 HWY_DYNAMIC_DISPATCH

`HWY_DYNAMIC_DISPATCH` 宏用于调用动态分发的函数：

```cpp
// highway.h（简化）
#define HWY_DYNAMIC_DISPATCH(FUNC_NAME) \
  (*(HWY_DISPATCH_TABLE(FUNC_NAME)[GetChosenTarget().GetIndex()]))

// 调用流程：
// 1. 首次调用：入口 0
//    a. FunctionCache 调用 SupportedTargets()
//    b. 更新 ChosenTarget（全局缓存）
//    c. 跳到正确入口并调用
// 2. 后续调用：直接跳到正确入口
```

### 运行时流程

```
首次调用：                     后续调用：
                              ┌──────────────────┐
                              │  ChosenTarget    │
                              │  已初始化       │
┌────────────┐                 └────────┬─────────┘
│  调用      │                          │
│  函数       │                          ▼
└─────┬──────┘              ┌──────────────────────┐
      │                     │  获取索引         │
      ▼                     │  GetIndex()      │
┌────────────────┐           └─────────┬──────────┘
│  入口 0：    │                     │
│  初始化缓存  │                     ▼
└─────────┬────┘          ┌──────────────────────────┐
          │                │  从表中选择实现       │
          ▼                │  table[index]         │
┌────────────────┐         └─────────┬────────────┘
│  调用        │                   │
│  Supported  │                   ▼
│  Targets()   │         ┌──────────────────────────┐
└─────────┬────┘         │  调用目标实现       │
          │              │  N_AVX2::Function()  │
          ▼              └──────────────────────────┘
┌────────────────┐
│  更新缓存    │
│  ChosenTarget │
└─────────┬────┘
          │
          ▼
┌────────────────┐
│  选择并调用  │
│  最佳实现    │
└────────────────┘
```

---

## 7.5 完整动态分发示例

### my_code.h - 头文件

```cpp
#ifndef MY_CODE_H_
#define MY_CODE_H_

#include "hwy/base.h"

namespace my_code {

HWY_DLLEXPORT void ProcessArray(float* data, size_t count);
HWY_DLLEXPORT float SumArray(const float* data, size_t count);

}  // namespace my_code

#endif  // MY_CODE_H_
```

### my_code.cc - 实现文件

```cpp
#include "my_code.h"

// ========== 动态分发开始 ==========
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "my_code.cc"
#include "hwy/foreach_target.h"
// ========== 动态分发结束 ==========

// 必须在 foreach_target.h 之后包含 highway.h
#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace my_code {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;

// 每个目标都会编译这个函数
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

// ========== 导出函数指针表 ==========
#if HWY_ONCE
namespace my_code {

// 创建函数指针表
HWY_EXPORT(ProcessArray);
HWY_EXPORT(SumArray);

// 导出供外部调用的包装函数
HWY_DLLEXPORT void ProcessArray(float* data, size_t count) {
  HWY_DYNAMIC_DISPATCH(ProcessArray)(data, count);
}

HWY_DLLEXPORT float SumArray(const float* data, size_t count) {
  return HWY_DYNAMIC_DISPATCH(SumArray)(data, count);
}

}  // namespace my_code
#endif  // HWY_ONCE
```

### main.cc - 使用示例

```cpp
#include "my_code.h"
#include <cstdio>

int main() {
  float data[1000];

  // 初始化数据
  for (int i = 0; i < 1000; ++i) {
    data[i] = static_cast<float>(i);
  }

  // 调用动态分发的函数
  // 首次调用会检测 CPU 并选择最佳实现
  my_code::ProcessArray(data, 1000);

  float sum = my_code::SumArray(data, 1000);

  printf("Sum: %f\n", sum);
  return 0;
}
```

---

## 7.6 SupportedTargets()

`SupportedTargets()` 函数返回当前 CPU 支持的目标：

```cpp
// targets.h
HWY_DLLEXPORT int64_t SupportedTargets();

// 返回值是一个位掩码
// 位 0 对应 HWY_SSE2
// 位 1 对应 HWY_SSSE3
// 位 2 对应 HWY_SSE4
// ...
```

### 使用示例

```cpp
#include "hwy/targets.h"

void PrintSupportedTargets() {
  int64_t supported = hwy::SupportedTargets();

  printf("Supported targets:\n");

  // 检查各个目标
  if (supported & HWY_AVX2) {
    printf("  AVX2\n");
  }
  if (supported & HWY_AVX3) {
    printf("  AVX-512\n");
  }
  if (supported & HWY_SSE4) {
    printf("  SSE4\n");
  }
  if (supported & HWY_NEON) {
    printf("  NEON\n");
  }
  if (supported & HWY_SVE) {
    printf("  SVE\n");
  }
}
```

---

## 7.7 DisableTargets()

`DisableTargets()` 允许禁用特定目标：

```cpp
// targets.h
HWY_DLLEXPORT void DisableTargets(int64_t disabled_targets);

// 使用示例：禁用有 bug 的目标
hwy::DisableTargets(HWY_AVX3);  // 禁用 AVX-512

// 后续的 SupportedTargets() 不会返回禁用的目标
// （除非只剩禁用的目标，则回退到 SCALAR）
```

---

## 7.8 SetSupportedTargetsForTest()

用于测试，强制指定支持的目标：

```cpp
// targets.h
HWY_DLLEXPORT void SetSupportedTargetsForTest(int64_t targets);

// 使用示例：测试所有目标
void TestAllTargets() {
  auto targets = hwy::SupportedAndGeneratedTargets();

  for (int64_t target : targets) {
    hwy::SetSupportedTargetsForTest(target);

    // 调用测试函数
    RunMyTest();

    printf("Tested: %s\n", hwy::TargetName(target));
  }

  // 恢复正常行为
  hwy::SetSupportedTargetsForTest(0);
}
```

---

## 7.9 TargetName()

获取目标的字符串名称：

```cpp
// targets.h
static inline const char* TargetName(int64_t target);

// 使用示例
printf("Current target: %s\n",
       hwy::TargetName(HWY_AVX2));  // "AVX2"
printf("Current target: %s\n",
       hwy::TargetName(HWY_NEON));  // "NEON"
```

---

## 7.10 性能考虑

### 首次调用开销

```
首次调用：
  - CPU 功能检测
  - 全局缓存初始化
  - 函数指针查找

典型开销：< 1 微秒

后续调用：
  - 仅函数指针调用
  - 完全可预测的分支

开销：几乎为零（分支预测）
```

### 优化提示

```cpp
// 避免频繁调用不同的动态分发函数
// 好的做法：批量处理
for (int iter = 0; iter < 100; ++iter) {
  HWY_DYNAMIC_DISPATCH(ProcessAll)(data);
}

// 不好的做法：分散调用
for (int iter = 0; iter < 100; ++iter) {
  HWY_DYNAMIC_DISPATCH(ProcessOne)(&data[i]);
  HWY_DYNAMIC_DISPATCH(ProcessOne)(&data[i+1]);
  // ...
}
```

---

## 7.11 模板函数的动态分发

对于模板函数，使用 `HWY_EXPORT_T` 和 `HWY_DYNAMIC_DISPATCH_T`：

```cpp
// 模板函数
template <typename T>
void ProcessTyped(T* data, size_t count) {
  const ScalableTag<T> d;
  const size_t N = Lanes(d);

  for (size_t i = 0; i < count; i += N) {
    auto v = Load(d, &data[i]);
    v = Mul(v, v);
    Store(v, d, &data[i]);
  }
}

// 导出和使用
HWY_EXPORT_T(MyFloatTable, ProcessTyped<float>);
HWY_EXPORT_T(MyIntTable, ProcessTyped<int32_t>);

void Caller() {
  float f32_data[100];
  int32_t i32_data[100];

  HWY_DYNAMIC_DISPATCH_T(MyFloatTable)(f32_data, 100);
  HWY_DYNAMIC_DISPATCH_T(MyIntTable)(i32_data, 100);
}
```

### 简化版本：HWY_EXPORT_AND_DYNAMIC_DISPATCH_T

```cpp
template <typename T>
void ProcessTyped(T* data, size_t count) {
  // 实现...
}

void Caller() {
  float f32_data[100];

  // 一行完成导出和调用
  HWY_EXPORT_AND_DYNAMIC_DISPATCH_T(ProcessTyped<float>)(f32_data, 100);
}
```

---

## 7.12 完整示例：图像处理库

```cpp
// image_processor.cc
#include "image_processor.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "image_processor.cc"
#include "hwy/foreach_target.h"

#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace img_proc {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;

// 灰度转换：RGB -> Grayscale
void ConvertToGrayscale(const uint8_t* rgb, uint8_t* gray,
                       size_t width, size_t height) {
  const ScalableTag<uint8_t> du8;
  const ScalableTag<float> df;
  const size_t N = Lanes(du8);

  const float weight_r = 0.299f;
  const float weight_g = 0.587f;
  const float weight_b = 0.114f;

  auto vw_r = Set(df, weight_r);
  auto vw_g = Set(df, weight_g);
  auto vw_b = Set(df, weight_b);

  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < width; x += N) {
      // 加载 RGB 通道
      auto v_r = LoadU(du8, &rgb[(y * width + x) * 3]);
      auto v_g = LoadU(du8, &rgb[(y * width + x) * 3 + 1]);
      auto v_b = LoadU(du8, &rgb[(y * width + x) * 3 + 2]);

      // 转换为浮点
      auto vf_r = ConvertTo(df, PromoteTo(RebindToSigned<decltype(df)>(), v_r));
      auto vf_g = ConvertTo(df, PromoteTo(RebindToSigned<decltype(df)>(), v_g));
      auto vf_b = ConvertTo(df, PromoteTo(RebindToSigned<decltype(df)>(), v_b));

      // 加权求和
      auto vf_gray = Mul(vf_r, vw_r);
      vf_gray = MulAdd(vf_g, vw_g, vf_gray);
      vf_gray = MulAdd(vf_b, vw_b, vf_gray);

      // 转回 uint8_t
      auto v_gray = DemoteTo(du8, ConvertTo(RebindToUnsigned<decltype(df)>(), vf_gray));
      Store(v_gray, du8, &gray[y * width + x]);
    }
  }
}

}  // namespace HWY_NAMESPACE
}  // namespace img_proc
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace img_proc {

HWY_EXPORT(ConvertToGrayscale);

HWY_DLLEXPORT void ConvertToGrayscale(const uint8_t* rgb, uint8_t* gray,
                                     size_t width, size_t height) {
  HWY_DYNAMIC_DISPATCH(ConvertToGrayscale)(rgb, gray, width, height);
}

}  // namespace img_proc
#endif
```

---

## 本日总结

| 概念 | 说明 |
|------|------|
| **foreach_target.h** | 重新包含文件多次，为每个目标编译 |
| **HWY_TARGET_TOGGLE** | 切换 include guard，允许多次包含 |
| **HWY_EXPORT** | 创建函数指针表 |
| **HWY_DYNAMIC_DISPATCH** | 运行时选择并调用目标函数 |
| **SupportedTargets()** | 返回 CPU 支持的目标位掩码 |
| **ChosenTarget** | 全局缓存，存储当前最佳目标 |
| **HWY_EXPORT_T** | 模板函数的导出 |
| **首次调用开销** | CPU 检测 + 缓存初始化（< 1μs）|

---

## 练习

1. 实现一个动态分发的函数，计算数组的标准差
2. 使用 `SupportedTargets()` 打印当前 CPU 支持的所有目标
3. 使用 `SetSupportedTargetsForTest()` 编写测试所有目标的代码

---

## 下一步

明天我们将深入分析平台特定实现，从 x86 架构开始。
