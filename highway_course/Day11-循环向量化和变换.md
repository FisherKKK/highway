# Day 11: 循环向量化变换

## 课程目标

今天我们将学习 Highway 的 `contrib/algo/` 模块，了解如何使用高级算法进行循环向量化变换，以及手动编写高效 SIMD 循环的模式。

---

## 11.1 什么是循环向量化？

循环向量化是将标量循环转换为 SIMD 向量循环的过程：

```cpp
// 标量循环
for (size_t i = 0; i < n; ++i) {
  c[i] = a[i] * b[i] + c[i];
}

// 向量化后
for (size_t i = 0; i < n; i += N) {
  // N = Lanes(d)
  va = Load(d, a + i);
  vb = Load(d, b + i);
  vc = Load(d, c + i);
  vc = MulAdd(va, vb, vc);
  Store(vc, d, c + i);
}
```

### Highway 的循环向量化策略

| 策略 | 优点 | 缺点 | 适用场景 |
|------|------|------|----------|
| **填充数组** | 简单，无尾部处理 | 内存消耗高 | 数据量大 |
| **处理完整向量，重叠尾部** | 无额外内存 | 复杂 | 实时处理 |
| **使用掩码** | 精确控制 | 平台依赖 | 精度要求高 |
| **Algorithm 模块** | 自动化 | 可能损失优化 | 通用场景 |

---

## 11.2 基本循环模式

### 手动向量化循环

```cpp
#include "hwy/highway.h"
#include "hwy/aligned_allocator.h"
namespace hn = hwy::HWY_NAMESPACE;

// 基本向量化循环
template <typename T>
void BasicLoop(const T* input, T* output, size_t count) {
  const ScalableTag<T> d;
  const size_t N = Lanes(d);

  // 处理完整向量
  for (size_t i = 0; i + N <= count; i += N) {
    auto v_input = Load(d, &input[i]);
    auto v_output = Load(d, &output[i]);

    // 标量操作：output[i] = input[i] * 2 + output[i]
    auto v_scaled = Mul(v_input, Set(d, T{2}));
    v_output = Add(v_output, v_scaled);

    Store(v_output, d, &output[i]);
  }

  // 处理剩余元素
  for (size_t i = (count / N) * N; i < count; ++i) {
    output[i] = input[i] * 2 + output[i];
  }
}
```

### 使用掩码处理尾部

```cpp
template <typename T>
void LoopWithMask(const T* input, T* output, size_t count) {
  const ScalableTag<T> d;
  const size_t N = Lanes(d);

  size_t i = 0;
  for (; i + N <= count; i += N) {
    auto v_input = Load(d, &input[i]);
    auto v_output = Load(d, &output[i]);

    v_output = MulAdd(v_input, Set(d, T{2}), v_output);
    Store(v_output, d, &output[i]);
  }

  // 处理剩余元素
  if (i < count) {
    auto mask = FirstN(d, count - i);
    auto v_input = Load(d, &input[i]);
    auto v_output = Load(d, &output[i]);

    v_output = MulAdd(v_input, Set(d, T{2}), v_output);
    BlendedStore(v_output, mask, d, &output[i]);
  }
}
```

---

## 11.3 contrib/algo 模块

`contrib/algo/` 提供了高层次的算法，简化循环向量化。

### Transform：变换数组

```cpp
#include "hwy/contrib/algo/transform-inl.h"

// Transform1：output = f(input)
template <typename T>
void TransformExample(const T* input, T* output, size_t count) {
  const ScalableTag<T> d;

  // Transform1 会自动处理所有边界情况
  Transform1(d, input, count, output, [](auto d, auto v) {
    return Mul(v, v);  // output[i] = input[i]^2
  });
}

// Transform2：output = f(a, b)
template <typename T>
void Transform2Example(const T* a, const T* b, T* output, size_t count) {
  const ScalableTag<T> d;

  Transform2(d, a, b, count, output, [](auto d, auto va, auto vb) {
    return MulAdd(va, vb, Zero(d));  // output[i] = a[i] * b[i]
  });
}
```

### Copy：内存拷贝

```cpp
#include "hwy/contrib/algo/copy-inl.h"

// 自动选择最佳拷贝方式
template <typename T>
void CopyExample(const T* src, T* dst, size_t count) {
  const ScalableTag<T> d;
  Copy1(d, src, count, dst);
}

// 带转换的拷贝
template <typename SrcT, typename DstT>
template <typename T>
void CopyConvertExample(const SrcT* src, DstT* dst, size_t count) {
  const ScalableTag<DstT> d;
  CopyConvert1(d, src, count, dst);
}
```

### Find：查找操作

```cpp
#include "hwy/contrib/algo/find-inl.h"

// 查找第一个大于阈值的元素
template <typename T>
size_t FindGreaterThan(const T* data, size_t count, T threshold) {
  const ScalableTag<T> d;

  // 返回第一个满足条件的索引
  auto result = FindIf(d, data, count, [threshold](auto d, auto v) {
    auto v_threshold = Set(d, threshold);
    return GT(v, v_threshold);  // v > threshold
  });

  return result;
}
```

---

## 11.4 Transform 详细使用

### Transform1 详解

```cpp
// transform-inl.h（简化）
template <class D, typename T, typename Func>
void Transform1(D d, const T* HWY_RESTRICT input, size_t count,
                T* HWY_RESTRICT output, Func func) {
  // 1. 处理前导对齐部分（如果需要）
  size_t offset = 0;

  // 2. 处理完整向量
  for (; offset + Lanes(d) <= count; offset += Lanes(d)) {
    auto v_input = Load(d, input + offset);
    auto v_output = func(d, v_input);
    Store(v_output, d, output + offset);
  }

  // 3. 处理剩余元素
  if (offset < count) {
    const CappedTag<T, 64> dc;
    const size_t remaining = count - offset;

    if (Lanes(dc) <= remaining) {
      // 处理剩余向量（可能跨过结束）
      auto mask = FirstN(dc, remaining);
      auto v_input = Load(dc, input + offset);
      auto v_output = func(dc, v_input);
      BlendedStore(v_output, mask, dc, output + offset);
    } else {
      // 处理最后的标量元素
      auto dc = RebindToSigned<decltype(dc)>();
      const size_t N = Lanes(dc);
      for (size_t i = 0; i < N; ++i) {
        output[offset + i] =
          GetLane(func(d, LoadU(d, &input[offset + i])), 0);
      }
    }
  }
}
```

### Transform 使用示例

```cpp
// 复杂变换：exp(x^2) 近似
template <typename T>
void ExpSqr(const T* input, T* output, size_t count) {
  const ScalableTag<T> d;

  Transform1(d, input, count, output, [](auto d, auto v) {
    // x^2
    auto v_sq = Mul(v, v);

    // exp(x) 近似：1 + x + x^2/2 + x^3/6 + x^4/24
    auto v_one = Set(d, T{1});
    auto v_two = Set(d, T{2});

    auto v_exp = v_one;  // 1
    v_exp = MulAdd(v, v_exp, v_one);        // 1 + x
    v_exp = MulAdd(v_sq, v_exp, v_two);    // 1 + x + x^2/2
    // ... 更高阶项

    return v_exp;
  });
}
```

---

## 11.5 Algorithm 模块高级用法

### 带状态的变换

```cpp
// 上下文状态
struct TransformContext {
  float scale;
  int iterations;
};

// 带状态的 Transform
template <typename T>
void TransformWithState(const T* input, T* output, size_t count,
                       TransformContext* ctx) {
  const ScalableTag<T> d;

  Transform1(d, input, count, output, [ctx](auto d, auto v) {
    // 使用上下文
    auto v_scale = Set(d, T(ctx->scale));
    v = Mul(v, v_scale);

    ctx->scale *= 0.99f;  // 更新状态
    return v;
  });
}
```

### 条件 Transform

```cpp
// 条件变换：只处理正数
template <typename T>
void ProcessPositiveOnly(const T* input, T* output, size_t count) {
  const ScalableTag<T> d;

  Transform1(d, input, count, output, [](auto d, auto v) {
    auto mask = GT(v, Zero(d));  // v > 0
    auto v_result = v;

    // 只对正数进行操作
    if (!AllFalse(d, mask)) {
      v_result = Mul(v, v);  // 仅正数平方
    }

    return v_result;
  });
}
```

### ForEach：只读处理

```cpp
#include "hwy/contrib/algo/foreach-inl.h"

// 计算数组总和
template <typename T>
T SumArray(const T* data, size_t count) {
  const ScalableTag<T> d;
  Vec<decltype(d)> sum = Zero(d);

  ForEach1(d, data, count, [&sum](auto d, auto v) {
    sum = Add(sum, v);
  });

  return GetLane(SumOfLanes(d, sum), 0);
}

// 查找最大值
template <typename T>
T FindMax(const T* data, size_t count) {
  const ScalableTag<T> d;
  Vec<decltype(d)> max_val = Set(d, std::numeric_limits<T>::min());

  ForEach1(d, data, count, [&max_val](auto d, auto v) {
    max_val = Max(max_val, v);
  });

  return GetLane(MinOfLanes(d, max_val), 0);
}
```

---

## 11.6 完整示例：图像处理循环

### 标量实现

```cpp
// 标量卷积：3x3 高斯模糊
void GaussianBlurScalar(const uint8_t* input, uint8_t* output,
                       size_t width, size_t height,
                       const float* kernel, size_t ksize) {
  for (size_t y = 1; y < height - 1; ++y) {
    for (size_t x = 1; x < width - 1; ++x) {
      float sum = 0.0f;
      for (int ky = -1; ky <= 1; ++ky) {
        for (int kx = -1; kx <= 1; ++kx) {
          size_t idx = (y + ky) * width + (x + kx);
          sum += input[idx] * kernel[(ky + 1) * 3 + (kx + 1)];
        }
      }
      output[y * width + x] = static_cast<uint8_t>(sum);
    }
  }
}
```

### SIMD 实现版本 1：基础循环

```cpp
// 基础 SIMD 实现
void GaussianBlurBasic(const uint8_t* input, uint8_t* output,
                      size_t width, size_t height,
                      const float* kernel, size_t ksize) {
  const ScalableTag<uint8_t> du8;
  const ScalableTag<float> df;
  const size_t N = Lanes(du8);

  // 预转换 kernel 为定点数（可选优化）
  // ...

  for (size_t y = 1; y < height - 1; ++y) {
    for (size_t x = 1; x < width - 1; ++x) {
      float sum = 0.0f;

      // 内层循环向量化的潜力有限，但可以优化
      for (int ky = -1; ky <= 1; ++ky) {
        for (int kx = -1; kx <= 1; ++kx) {
          size_t idx = (y + ky) * width + (x + kx);
          sum += static_cast<float>(input[idx]) * kernel[(ky + 1) * 3 + (kx + 1)];
        }
      }
      output[y * width + x] = static_cast<uint8_t>(sum);
    }
  }
}
```

### SIMD 实现版本 2：使用 Algorithm 模块

```cpp
// 使用 Algorithm 的版本
void GaussianBlurAlgorithm(const uint8_t* input, uint8_t* output,
                         size_t width, size_t height,
                         const float* kernel, size_t ksize) {
  const ScalableTag<uint8_t> du8;
  const ScalableTag<float> df;
  const size_t N = Lanes(du8);

  // 使用 Algorithm 处理主要计算
  Transform2(du8, input, kernel, width * height, output,
    [width](auto d, auto v_input, auto v_kernel) {
      // 这是简化版本，实际 3x3 卷积更复杂
      return CastTo<uint8_t>(df, v_input);
    });
}
```

### SIMD 实现版本 3：手动优化循环

```cpp
// 手动优化的循环版本
void GaussianBlurOptimized(const uint8_t* input, uint8_t* output,
                         size_t width, size_t height,
                         const float* kernel, size_t ksize) {
  const ScalableTag<uint8_t> du8;
  const ScalableTag<float> df;
  const size_t N = Lanes(du8);

  // 创建对齐的临时缓冲区
  std::vector<float> temp_row(width, 0.0f);

  for (size_t y = 1; y < height - 1; ++y) {
    // 处理当前行
    for (size_t x = 1; x < width - 1; ++x) {
      float sum = 0.0f;

      // 向量化水平卷积
      auto v_sum = Zero(df);
      for (int ky = -1; ky <= 1; ++ky) {
        // 加载邻居行（向量化）
        size_t row_offset = (y + ky) * width;
        size_t start_x = x - 1;
        size_t end_x = x + 2;

        size_t x_vec = start_x;
        for (; x_vec + N <= end_x; x_vec += N) {
          auto v_row = LoadU(df, reinterpret_cast<const float*>(&input[row_offset + x_vec]));
          auto v_ker = LoadU(df, &kernel[(ky + 1) * 3 + (x_vec - (x - 1))]);
          v_sum = MulAdd(v_row, v_ker, v_sum);
        }

        // 处理剩余元素（如果任何）
        if (x_vec < end_x) {
          auto mask = FirstN(df, end_x - x_vec);
          auto v_row = LoadU(df, &input[row_offset + x_vec]);
          auto v_ker = LoadU(df, &kernel[(ky + 1) * 3 + (x_vec - (x - 1))]);
          auto v_partial = Mul(v_row, v_ker);
          v_sum = Add(v_sum, IfThenElseZero(mask, v_partial));
        }

        // 从 v_sum 提取标量结果
        if (ky == 0 && x_vec == x + 1) {  // 简化示例
          float partial_sum = GetLane(SumOfLanes(df, v_sum), 0);
          sum += partial_sum;
        }
      }

      output[y * width + x] = static_cast<uint8_t>(sum);
    }
  }
}
```

---

## 11.7 性能优化技巧

### 1. 内存预取

```cpp
template <typename T>
void PrefetchExample(const T* data, size_t count) {
  const ScalableTag<T> d;
  const size_t N = Lanes(d);
  const size_t prefetch_distance = 256;  // 提前 256 字节

  for (size_t i = 0; i + N <= count; i += N) {
    // 预取后续数据
    if (i + prefetch_distance < count) {
      __builtin_prefetch(&data[i + prefetch_distance], 0, 3);
    }

    // 处理当前数据
    auto v = Load(d, &data[i]);
    // ... 处理 ...
  }
}
```

### 2. 循环展开

```cpp
template <typename T>
void UnrolledLoopExample(const T* input, T* output, size_t count) {
  const ScalableTag<T> d;
  const size_t N = Lanes(d);

  // 处理完整的向量块
  for (size_t i = 0; i + 8 * N <= count; i += 8 * N) {
    // 4 重展开
    auto v0 = Load(d, &input[i + 0 * N]);
    auto v1 = Load(d, &input[i + 1 * N]);
    auto v2 = Load(d, &input[i + 2 * N]);
    auto v3 = Load(d, &input[i + 3 * N]);

    // 处理
    v0 = Mul(v0, v0);
    v1 = Mul(v1, v1);
    v2 = Mul(v2, v2);
    v3 = Mul(v3, v3);

    Store(v0, d, &output[i + 0 * N]);
    Store(v1, d, &output[i + 1 * N]);
    Store(v2, d, &output[i + 2 * N]);
    Store(v3, d, &output[i + 3 * N]);

    // 另 4 个
    auto v4 = Load(d, &input[i + 4 * N]);
    auto v5 = Load(d, &input[i + 5 * N]);
    auto v6 = Load(d, &input[i + 6 * N]);
    auto v7 = Load(d, &input[i + 7 * N]);

    v4 = Mul(v4, v4);
    v5 = Mul(v5, v5);
    v6 = Mul(v6, v6);
    v7 = Mul(v7, v7);

    Store(v4, d, &output[i + 4 * N]);
    Store(v5, d, &output[i + 5 * N]);
    Store(v6, d, &output[i + 6 * N]);
    Store(v7, d, &output[i + 7 * N]);
  }

  // 处理剩余
  for (size_t i = (count / N) * N; i < count; ++i) {
    output[i] = input[i] * input[i];
  }
}
```

### 3. 分离偶/奇通道（适用于广播操作）

```cpp
template <typename T>
void SeparateEvenOdd(const T* input, T* even, T* odd, size_t count) {
  const ScalableTag<T> d;
  const size_t N = Lanes(d);

  for (size_t i = 0; i + N <= count; i += N) {
    auto v = Load(d, &input[i]);

    // 分离偶数索引和奇数索引
    auto mask_even = Set(df, 0xAAAAAAAA);
    auto v_even = And(v, mask_even);
    auto v_odd = Xor(v, v_even);

    Store(v_even, d, &even[i]);
    Store(v_odd, d, &odd[i]);
  }

  // 处理剩余元素
  if (count % N != 0) {
    // ...
  }
}
```

---

## 11.8 性能测试和对比

### 简单的性能测试

```cpp
#include <chrono>

template <typename Func>
void Benchmark(const char* name, Func func, size_t iterations) {
  auto start = std::chrono::high_resolution_clock::now();

  for (size_t iter = 0; iter < iterations; ++iter) {
    func();
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  printf("%s: %zu ms\n", name, duration.count());
}

int main() {
  const size_t size = 10000000;
  AlignedVector<float> input(size);
  AlignedVector<float> output(size);

  // 初始化
  for (size_t i = 0; i < size; ++i) {
    input[i] = static_cast<float>(i);
  }

  // 测试不同实现
  size_t iterations = 100;

  Benchmark("Scalar", [input = input.data(), output = output.data(), size]() {
    for (size_t iter = 0; iter < iterations; ++iter) {
      // 标量实现
      for (size_t i = 0; i < size; ++i) {
        output[i] = input[i] * 2.0f;
      }
    }
  }, 1);

  Benchmark("Manual SIMD", [input = input.data(), output = output.data(), size]() {
    const ScalableTag<float> df;
    const size_t N = Lanes(df);
    for (size_t iter = 0; iter < iterations; ++iter) {
      // SIMD 实现
      for (size_t i = 0; i + N <= size; i += N) {
        auto v = Load(df, &input[i]);
        v = Mul(v, Set(df, 2.0f));
        Store(v, df, &output[i]);
      }
      // 处理尾部...
    }
  }, 1);

  Benchmark("Algorithm", [input = input.data(), output = output.data(), size]() {
    const ScalableTag<float> df;
    for (size_t iter = 0; iter < iterations; ++iter) {
      Transform1(df, input, size, output, [](auto d, auto v) {
        return Mul(v, Set(d, 2.0f));
      });
    }
  }, 1);

  return 0;
}
```

---

## 本日总结

| 方法 | 优点 | 缺点 | 适用场景 |
|------|------|------|----------|
| **手动循环** | 完全控制性能 | 编写复杂，调试困难 | 核心算法，性能关键 |
| **Algorithm 模块** | 简洁，自动边界处理 | 可能损失优化 | 通用场景，快速开发 |
| **掩码尾部** | 精确控制 | 平台依赖 | 精度要求高 |
| **填充数组** | 无尾部处理 | 内存消耗高 | 数据量大 |

| 功能 | 模块 | 说明 |
|------|------|------|
| **Transform** | `algo/transform-inl.h` | output = f(input) 或 output = f(a, b) |
| **Copy** | `algo/copy-inl.h` | 内存拷贝和转换 |
| **Find** | `algo/find-inl.h` | 查找条件元素 |
| **ForEach** | `algo/foreach-inl.h` | 只读处理 |
| **Sort** | `contrib/sort/` | 排序算法 |

---

## 练习

1. 使用 `Transform1` 实现数组元素平方
2. 使用 `Transform2` 实现两个数组的逐元素加法
3. 手动实现一个标量到向量的循环，处理数组倒数
4. 使用 `ForEach` 计算数组平均值

---

## 下一步

明天我们将学习 contrib 模块的高级功能，特别是 VQSort 排序算法。
