# Day 13: Contrib 模块 - 数学函数和其他工具

## 课程目标

今天我们将学习 Highway 的 `contrib/math/` 模块和其他高级工具，包括数学函数、统计函数和变换工具。

---

## 13.1 contrib/math 模块

### 基本数学函数

```cpp
#include "hwy/contrib/math/math-inl.h"

namespace hn = hwy::HWY_NAMESPACE;

// 基础数学函数：sin, cos, exp, log 等

// 计算 sin(x)
float Sin(float x) {
  const hn::ScalableTag<float> d;
  return GetLane(hn::Sin(d, Set(d, x)), 0);
}

// 计算 cos(x)
float Cos(float x) {
  const hn::ScalableTag<float> d;
  return GetLane(hn::Cos(d, Set(d, x)), 0);
}

// 计算 exp(x)
float Exp(float x) {
  const hn::ScalableTag<float> d;
  return GetLane(hn::Exp(d, Set(d, x)), 0);
}

// 计算 log(x)
float Log(float x) {
  const hn::ScalableTag<float> d;
  return GetLane(hn::Log(d, Set(d, x)), 0);
}

// 计算 sqrt(x)
float Sqrt(float x) {
  const hn::ScalableTag<float> d;
  return GetLane(hn::Sqrt(d, Set(d, x)), 0);
}
```

### 批量计算

```cpp
// 批量计算 sin
void BatchSin(const float* input, float* output, size_t count) {
  const hn::ScalableTag<float> d;

  // 使用 Transform 批量计算
  hn::Transform1(d, input, count, output,
    [](auto d, auto v) { return hn::Sin(d, v); });
}

// 同时计算 sin 和 cos（更高效）
void BatchSinCos(const float* input, float* sin_out, float* cos_out, size_t count) {
  const hn::ScalableTag<float> d;

  hn::Transform1(d, input, count, sin_out,
    [](auto d, auto v) { return hn::Sin(d, v); });

  hn::Transform1(d, input, count, cos_out,
    [](auto d, auto v) { return hn::Cos(d, v); });

  // 更好的实现：使用 math-inl.h 中内置的同时计算
}
```

### 向量化多项式评估

```cpp
// 使用 Horner 方法评估多项式
template <size_t Degree>
float EvaluatePolynomial(float x, const float* coefficients) {
  float result = 0.0f;
  for (size_t i = Degree; i > 0; --i) {
    result = result * x + coefficients[i];
  }
  result = result * x + coefficients[0];
  return result;
}

// 向量化版本
template <size_t Degree>
void BatchEvaluatePolynomial(const float* x_values, const float* coefficients,
                           float* output, size_t count) {
  const hn::ScalableTag<float> d;

  // 每次处理多项式的所有系数
  // 这里简化处理，实际实现需要考虑向量大小
  hn::Transform1(d, x_values, count, output,
    [coefficients](auto d, auto v_x) {
      auto result = Set(d, coefficients[Degree]);

      // Horner 方法的向量化版本
      // 注意：需要展开循环
      result = MulAdd(result, v_x, Set(d, coefficients[Degree-1]));
      result = MulAdd(result, v_x, Set(d, coefficients[Degree-2]));
      // ... 继续展开
      result = MulAdd(result, v_x, Set(d, coefficients[0]));

      return result;
    });
}
```

---

## 13.2 统计函数

### contrib/stats 模块

```cpp
#include "hwy/contrib/stats/stats-inl.h"

// 计算平均值
template <typename T>
T Mean(const T* data, size_t count) {
  const hn::ScalableTag<T> d;
  auto sum = hn::Zero(d);

  hn::ForEach1(d, data, count, [&sum](auto d, auto v) {
    sum = hn::Add(sum, v);
  });

  float result = GetLane(hn::SumOfLanes(d, sum), 0);
  return result / static_cast<T>(count);
}

// 计算方差
template <typename T>
T Variance(const T* data, size_t count) {
  const hn::ScalableTag<T> d;
  auto sum = hn::Zero(d);
  auto sum_sq = hn::Zero(d);

  hn::ForEach1(d, data, count, [&sum, &sum_sq](auto d, auto v) {
    sum = hn::Add(sum, v);
    sum_sq = hn::Add(sum_sq, Mul(v, v));
  });

  float mean_val = GetLane(sum, 0) / static_cast<T>(count);
  float mean_sq = GetLane(sum_sq, 0) / static_cast<T>(count);

  return mean_sq - mean_val * mean_val;
}

// 计算标准差
template <typename T>
T StdDev(const T* data, size_t count) {
  return hn::Sqrt(Variance(data, count));
}

// 计算最小值和最大值
template <typename T>
std::pair<T, T> MinMax(const T* data, size_t count) {
  const hn::ScalableTag<T> d;

  auto min_val = Set(d, std::numeric_limits<T>::max());
  auto max_val = Set(d, std::numeric_limits<T>::min());

  hn::ForEach1(d, data, count, [&min_val, &max_val](auto d, auto v) {
    min_val = hn::Min(min_val, v);
    max_val = hn::Max(max_val, v);
  });

  return {
    GetLane(min_val, 0),
    GetLane(max_val, 0)
  };
}

// 计算分位数
template <typename T>
T Quantile(const T* data, size_t count, float q) {
  // q 应该在 0 到 1 之间
  if (count == 0) return T{0};

  // 排序数据
  AlignedVector<T> sorted_data(data, data + count);
  hn::VQSort(sorted_data.data(), count);

  // 计算分位数位置
  float pos = q * (count - 1);

  if (pos >= count - 1) {
    return sorted_data[count - 1];
  }

  size_t index = static_cast<size_t>(pos);
  float fraction = pos - index;

  // 线性插值
  return sorted_data[index] + fraction * (sorted_data[index + 1] - sorted_data[index]);
}
```

### 统计函数的高级用法

```cpp
// 统计结构体
struct Statistics {
  float mean;
  float std_dev;
  float min;
  float max;
  float median;
  float q25;
  float q75;
};

// 计算完整统计信息
Statistics CalculateCompleteStats(const float* data, size_t count) {
  if (count == 0) {
    return {0, 0, 0, 0, 0, 0, 0};
  }

  Statistics stats;
  stats.mean = Mean(data, count);
  stats.std_dev = StdDev(data, count);
  auto minmax = MinMax(data, count);
  stats.min = minmax.first;
  stats.max = minmax.second;
  stats.median = Quantile(data, count, 0.5f);
  stats.q25 = Quantile(data, count, 0.25f);
  stats.q75 = Quantile(data, count, 0.75f);

  return stats;
}

// 向量化计算多个统计量
struct VectorStatistics {
  hn::Vec<decltype(d)> mean;
  hn::Vec<decltype(d)> std_dev;
  hn::Vec<decltype(d)> min;
  hn::Vec<decltype(d)> max;
};

VectorStatistics BatchCalculateStats(const float** data_arrays,
                                    const size_t* array_sizes,
                                    size_t num_arrays) {
  const hn::ScalableTag<float> d;

  VectorStatistics results;
  results.mean = Set(d, 0.0f);
  results.std_dev = Set(d, 0.0f);
  results.min = Set(d, std::numeric_limits<float>::max());
  results.max = Set(d, std::numeric_limits<float>::min());

  // 并行计算（简化版）
  for (size_t i = 0; i < num_arrays; ++i) {
    const float* data = data_arrays[i];
    size_t count = array_sizes[i];

    // 更新统计量
    auto current_mean = Mean(data, count);
    results.mean = Set(d, GetLane(results.mean, 0) + current_mean);

    auto current_minmax = MinMax(data, count);
    results.min = hn::Min(results.min, Set(d, current_minmax.first));
    results.max = hn::Max(results.max, Set(d, current_minmax.second));
    // ... 更新标准差等
  }

  return results;
}
```

---

## 13.3 变换和滤波

### 滤波器实现

```cpp
// FIR 滤波器
template <typename T>
class FIRFilter {
 public:
  FIRFilter(const T* coefficients, size_t num_coeffs)
      : coefficients_(coefficients),
        num_coeffs_(num_coeffs) {}

  void Process(const T* input, T* output, size_t count) {
    const hn::ScalableTag<T> d;
    const size_t N = hn::Lanes(d);

    // 处理每个输出样本
    for (size_t i = 0; i < count; ++i) {
      auto sum = hn::Zero(d);

      // 卷积计算
      for (size_t k = 0; k < num_coeffs_ && k <= i; ++k) {
        size_t input_idx = i - k;
        size_t coeff_idx = k;

        auto v_input = Set(d, input[input_idx]);
        auto v_coeff = Set(d, coefficients_[coeff_idx]);
        sum = hn::MulAdd(v_input, v_coeff, sum);
      }

      output[i] = GetLane(sum, 0);
    }
  }

 private:
  const T* coefficients_;
  size_t num_coeffs_;
};

// IIR 滤波器（使用递归）
template <typename T>
class IIRFilter {
 public:
  IIRFilter(const T* a_coeffs, const T* b_coeffs, size_t num_coeffs)
      : a_coeffs_(a_coeffs),
        b_coeffs_(b_coeffs),
        num_coeffs_(num_coeffs) {}

  void Process(const T* input, T* output, size_t count) {
    const hn::ScalableTag<T> d;
    const size_t N = hn::Lanes(d);

    // 初始化状态
    AlignedVector<T> state(num_coeffs_);
    for (size_t i = 0; i < num_coeffs_; ++i) {
      state[i] = 0;
    }

    for (size_t i = 0; i < count; ++i) {
      // 计算 FIR 部分（分子）
      auto y_fir = hn::Zero(d);
      for (size_t k = 0; k <= i && k < num_coeffs_; ++k) {
        size_t input_idx = i - k;
        auto v_input = Set(d, input[input_idx]);
        auto v_coeff = Set(d, b_coeffs_[k]);
        y_fir = hn::MulAdd(v_input, v_coeff, y_fir);
      }

      // 计算 IIR 部分（分母，减去反馈）
      auto y_iir = Set(d, 0.0f);
      for (size_t k = 1; k <= i && k < num_coeffs_; ++k) {
        auto v_state = Set(d, state[k-1]);
        auto v_coeff = Set(d, a_coeffs_[k]);
        y_iir = hn::Add(y_iir, Mul(v_state, v_coeff));
      }

      // 最终输出
      auto y = Sub(y_fir, y_iir);

      // 更新状态
      if (i < num_coeffs_ - 1) {
        state[i] = GetLane(y, 0);
      }

      output[i] = GetLane(y, 0);
    }
  }

 private:
  const T* a_coeffs_;  // 分母系数
  const T* b_coeffs_;  // 分子系数
  size_t num_coeffs_;
};
```

### FFT（快速傅里叶变换）

```cpp
#include <complex>

// 向量化 Cooley-Tukey FFT
template <size_t N>
class FFT {
 public:
  FFT() {
    // 初始化 twiddle 因子
    for (size_t k = 0; k < N/2; ++k) {
      float angle = -2.0f * M_PI * k / N;
      twiddle_[k] = std::exp(std::complex<float>(0.0f, angle));
    }
  }

  void ComputeFFT(const std::complex<float>* input,
                 std::complex<float>* output,
                 size_t size) {
    // 简化版：实现 N=2^m 的 FFT
    // 实际实现会更复杂，支持任意大小
  }

 private:
  std::complex<float> twiddle_[N/2];
};

// 批量 FFT
void BatchFFT(const std::complex<float>** input_arrays,
              std::complex<float>** output_arrays,
              size_t num_arrays, size_t fft_size) {
  const hn::ScalableTag<float> d;

  // 这里简化处理，实际实现需要复杂的复数运算
  for (size_t i = 0; i < num_arrays; ++i) {
    FFT<256> fft;  // 假设使用固定大小
    fft.ComputeFFT(input_arrays[i], output_arrays[i], fft_size);
  }
}
```

---

## 13.4 矩阵运算

### 矩阵乘法

```cpp
// 矩阵乘法：C = A × B
template <typename T>
void MatrixMultiply(const T* A, const T* B, T* C,
                    size_t rows_A, size_t cols_A, size_t cols_B) {
  const hn::ScalableTag<T> d;
  const size_t N = hn::Lanes(d);

  // C[i][j] = sum(A[i][k] * B[k][j])
  for (size_t i = 0; i < rows_A; ++i) {
    for (size_t j = 0; j < cols_B; j += N) {
      auto sum = hn::Zero(d);

      // 对 k 求和
      for (size_t k = 0; k < cols_A; ++k) {
        auto a_val = Set(d, A[i * cols_A + k]);

        // 加载 B 的第 k 行的 [j:j+N) 列
        for (size_t n = 0; n < N && j + n < cols_B; ++n) {
          auto b_val = Set(d, B[k * cols_B + j + n]);
          sum = hn::MulAdd(a_val, b_val, sum);
        }
      }

      // 存储结果
      for (size_t n = 0; n < N && j + n < cols_B; ++n) {
        C[i * cols_B + j + n] = GetLane(sum, n);
      }
    }
  }
}

// 分块矩阵乘法（缓存优化）
template <typename T>
void BlockedMatrixMultiply(const T* A, const T* B, T* C,
                          size_t rows_A, size_t cols_A, size_t cols_B,
                          size_t block_size) {
  for (size_t ii = 0; ii < rows_A; ii += block_size) {
    for (size_t jj = 0; jj < cols_B; jj += block_size) {
      for (size_t kk = 0; kk < cols_A; kk += block_size) {
        // 处理块：ii:ii+block_size, kk:kk+block_size, jj:jj+block_size
        for (size_t i = ii; i < ii + block_size && i < rows_A; ++i) {
          for (size_t k = kk; k < kk + block_size && k < cols_A; ++k) {
            auto a_val = A[i * cols_A + k];

            // 向量化处理 B 的行
            const hn::ScalableTag<T> d;
            const size_t N = hn::Lanes(d);

            size_t j = jj;
            for (; j + N <= jj + block_size && j < cols_B; j += N) {
              auto v_b = Load(d, &B[k * cols_B + j]);
              auto v_c = Load(d, &C[i * cols_B + j]);
              v_c = hn::MulAdd(Set(d, a_val), v_b, v_c);
              Store(v_c, d, &C[i * cols_B + j]);
            }

            // 处理剩余部分
            for (; j < jj + block_size && j < cols_B; ++j) {
              C[i * cols_B + j] += a_val * B[k * cols_B + j];
            }
          }
        }
      }
    }
  }
}
```

### 矩阵求和

```cpp
// 矩阵元素求和
template <typename T>
T SumMatrix(const T* matrix, size_t rows, size_t cols) {
  const hn::ScalableTag<T> d;
  auto total = hn::Zero(d);

  hn::ForEach1(d, matrix, rows * cols, [&total](auto d, auto v) {
    total = hn::Add(total, v);
  });

  return GetLane(hn::SumOfLanes(d, total), 0);
}

// 矩阵最大值
template <typename T>
T FindMatrixMax(const T* matrix, size_t rows, size_t cols) {
  const hn::ScalableTag<T> d;
  auto max_val = Set(d, std::numeric_limits<T>::min());

  hn::ForEach1(d, matrix, rows * cols, [&max_val](auto d, auto v) {
    max_val = hn::Max(max_val, v);
  });

  return GetLane(max_val, 0);
}
```

---

## 13.5 图像处理工具

### 灰度转换

```cpp
// RGB 转 Grayscale
void RGBToGrayscale(const uint8_t* rgb, uint8_t* gray, size_t width, size_t height) {
  const hn::ScalableTag<uint8_t> du8;
  const hn::ScalableTag<float> df;

  const float weight_r = 0.299f;
  const float weight_g = 0.587f;
  const float weight_b = 0.114f;

  auto vw_r = Set(df, weight_r);
  auto vw_g = Set(df, weight_g);
  auto vw_b = Set(df, weight_b);

  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < width; x += hn::Lanes(du8)) {
      // 加载 RGB 通道
      auto r = Load(du8, &rgb[(y * width + x) * 3 + 0]);
      auto g = Load(du8, &rgb[(y * width + x) * 3 + 1]);
      auto b = Load(du8, &rgb[(y * width + x) * 3 + 2]);

      // 转换为浮点
      auto rf = hn::ConvertTo(df, r);
      auto gf = hn::ConvertTo(df, g);
      auto bf = hn::ConvertTo(df, b);

      // 加权求和
      auto gray_f = hf::MulAdd(rf, vw_r, hf::Zero(df));
      gray_f = hf::MulAdd(gf, vw_g, gray_f);
      gray_f = hf::MulAdd(bf, vw_b, gray_f);

      // 转回 uint8_t
      auto gray_u8 = hn::DemoteTo(du8, hn::ConvertTo(hn::RebindToUnsigned<df>(), gray_f));
      Store(gray_u8, du8, &gray[y * width + x]);
    }
  }
}
```

### 卷积滤波

```cpp
// 图像卷积（使用 Sobel 算子）
void SobelFilter(const uint8_t* input, uint8_t* output,
                size_t width, size_t height) {
  const hn::ScalableTag<uint8_t> du8;
  const hn::ScalableTag<int32_t> di32;
  const hn::ScalableTag<float> df;

  // Sobel 算子
  const int32_t sobel_x[] = {-1, 0, 1, -2, 0, 2, -1, 0, 1};
  const int32_t sobel_y[] = {-1, -2, -1, 0, 0, 0, 1, 2, 1};

  for (size_t y = 1; y < height - 1; ++y) {
    for (size_t x = 1; x < width - 1; ++x) {
      // 计算梯度
      int32_t gx = 0, gy = 0;

      // 手动向量化简化版
      for (int ky = -1; ky <= 1; ++ky) {
        for (int kx = -1; kx <= 1; ++kx) {
          size_t idx = (y + ky) * width + (x + kx);
          int32_t val = static_cast<int32_t>(input[idx]);

          gx += val * sobel_x[(ky + 1) * 3 + (kx + 1)];
          gy += val * sobel_y[(ky + 1) * 3 + (kx + 1)];
        }
      }

      // 计算梯度幅值
      float magnitude = std::sqrt(static_cast<float>(gx * gx + gy * gy));
      magnitude = std::min(255.0f, magnitude);

      output[y * width + x] = static_cast<uint8_t>(magnitude);
    }
  }
}
```

---

## 13.6 高级：并行处理和任务分解

### 任务并行化

```cpp
#include <thread>
#include <atomic>

// 并行矩阵操作
template <typename T>
void ParallelMatrixOperation(const T* matrix, T* result, size_t rows, size_t cols,
                            std::function<void(const T*, T*, size_t, size_t)> operation) {
  const size_t num_threads = std::thread::hardware_concurrency();
  std::vector<std::thread> threads;
  std::atomic<size_t> current_row(0);

  auto worker = [&]() {
    while (true) {
      size_t row = current_row.fetch_add(1);
      if (row >= rows) break;

      operation(&matrix[row * cols], &result[row * cols], 1, cols);
    }
  };

  for (size_t i = 0; i < num_threads; ++i) {
    threads.emplace_back(worker);
  }

  for (auto& t : threads) {
    t.join();
  }
}

// 使用示例：并行矩阵求和
template <typename T>
void ParallelMatrixSum(const T* matrix, T* result, size_t rows, size_t cols) {
  const hn::ScalableTag<T> d;
  auto local_sum = hn::Zero(d);

  for (size_t i = 0; i < rows; ++i) {
    for (size_t j = 0; j < cols; ++j) {
      local_sum = hn::Add(local_sum, Set(d, matrix[i * cols + j]));
    }
  }

  *result = GetLane(hn::SumOfLanes(d, local_sum), 0);
}

int main() {
  // 示例使用
  const size_t matrix_size = 1000;
  const size_t rows = 1000, cols = 1000;

  AlignedVector<float> matrix(rows * cols);
  AlignedVector<float> result(rows);

  // ... 初始化 matrix ...

  ParallelMatrixOperation(matrix.data(), result.data(), rows, cols,
    [](const float* row_in, float* row_out, size_t n_rows, size_t n_cols) {
      // 对行进行操作
    });

  return 0;
}
```

---

## 13.7 性能优化技巧

### 缓存友好

```cpp
// 差的做法：非连续内存访问
template <typename T>
void BadCacheAccess(const T* matrix, size_t rows, size_t cols) {
  for (size_t j = 0; j < cols; ++j) {
    for (size_t i = 0; i < rows; ++i) {
      // 每次跳一整行，缓存不友好
      matrix[i * cols + j] *= 2;
    }
  }
}

// 好的做法：连续内存访问
template <typename T>
void GoodCacheAccess(const T* matrix, size_t rows, size_t cols) {
  for (size_t i = 0; i < rows; ++i) {
    for (size_t j = 0; j < cols; ++j) {
      // 连续访问，缓存友好
      matrix[i * cols + j] *= 2;
    }
  }
}
```

### 循环展开

```cpp
// 4 重展开的版本
template <typename T>
void UnrolledLoop(T* data, size_t count) {
  const hn::ScalableTag<T> d;
  const size_t N = hn::Lanes(d);

  for (size_t i = 0; i + 4 * N <= count; i += 4 * N) {
    // 处理 4 个向量
    auto v0 = Load(d, &data[i + 0 * N]);
    auto v1 = Load(d, &data[i + 1 * N]);
    auto v2 = Load(d, &data[i + 2 * N]);
    auto v3 = Load(d, &data[i + 3 * N]);

    v0 = Mul(v0, v0);
    v1 = Mul(v1, v1);
    v2 = Mul(v2, v2);
    v3 = Mul(v3, v3);

    Store(v0, d, &data[i + 0 * N]);
    Store(v1, d, &data[i + 1 * N]);
    Store(v2, d, &data[i + 2 * N]);
    Store(v3, d, &data[i + 3 * N]);
  }

  // 处理剩余
  for (size_t i = (count / N) * N; i < count; ++i) {
    data[i] = data[i] * data[i];
  }
}
```

### 预取优化

```cpp
template <typename T>
void OptimizedWithPrefetch(const T* data, T* output, size_t count) {
  const hn::ScalableTag<T> d;
  const size_t N = hn::Lanes(d);
  const size_t prefetch_distance = 8 * N;  // 预取距离

  size_t i = 0;
  for (; i + prefetch_distance < count; i += N) {
    // 预取后续数据
    if (i + prefetch_distance < count) {
      __builtin_prefetch(&data[i + prefetch_distance], 0, 3);
    }

    // 处理当前数据
    auto v = Load(d, &data[i]);
    v = Mul(v, Set(d, 2.0f));  // 乘 2
    Store(v, d, &output[i]);
  }

  // 处理剩余
  for (; i < count; ++i) {
    output[i] = data[i] * 2;
  }
}
```

---

## 13.8 完整示例：信号处理管道

```cpp
#include <iostream>
#include <vector>
#include "hwy/contrib/math/math-inl.h"
#include "hwy/contrib/stats/stats-inl.h"
#include "hwy/contrib/algo/foreach-inl.h"
#include "hwy/aligned_allocator.h"

namespace hn = hwy::HWY_NAMESPACE;

// 生成测试信号
void GenerateSignal(float* signal, size_t length, float frequency, float sample_rate) {
  for (size_t i = 0; i < length; ++i) {
    signal[i] = std::sin(2.0f * M_PI * frequency * i / sample_rate);
  }
}

// 信号处理管道
void ProcessSignalPipeline(const float* input, float* output, size_t length) {
  const hn::ScalableTag<float> df;

  // 1. 应用低通滤波器
  ApplyLowPassFilter(input, output, length, 0.1f);

  // 2. 计算并输出统计信息
  auto stats = CalculateCompleteStats(output, length);
  std::cout << "Signal Statistics:\n";
  std::cout << "  Mean: " << stats.mean << "\n";
  std::cout << "  StdDev: " << stats.std_dev << "\n";
  std::cout << "  Min: " << stats.min << ", Max: " << stats.max << "\n";
  std::cout << "  Median: " << stats.median << "\n";
  std::cout << "  Q1: " << stats.q25 << ", Q3: " << stats.q75 << "\n";

  // 3. 信号幅度归一化
  float max_val = FindMatrixMax(output, 1, length);
  if (max_val > 0) {
    hn::Transform1(df, output, length, output,
      [max_val](auto df, auto v) {
        return Mul(v, Set(df, 1.0f / max_val));
      });
  }
}

// 主函数
int main() {
  const size_t signal_length = 1000000;
  const float sample_rate = 1000.0f;
  const float frequency = 50.0f;

  // 分配对齐内存
  AlignedVector<float> input_signal(signal_length);
  AlignedVector<float> processed_signal(signal_length);

  // 生成测试信号
  GenerateSignal(input_signal.data(), signal_length, frequency, sample_rate);

  // 添加噪声
  const hn::ScalableTag<float> df;
  hn::Transform1(df, input_signal.data(), signal_length, input_signal.data(),
    [](auto df, auto v) {
      float noise = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.1f;
      return Add(v, Set(df, noise));
    });

  // 处理信号
  ProcessSignalPipeline(input_signal.data(), processed_signal.data(), signal_length);

  return 0;
}
```

---

## 本日总结

| 功能 | 文件 | 说明 |
|------|------|------|
| **数学函数** | `contrib/math/math-inl.h` | sin, cos, exp, log 等 |
| **多项式** | `contrib/math/polynomial-inl.h` | Horner 方法评估 |
| **三角函数** | `contrib/math/sin-cos-inl.h` | 同时计算 sin/cos |
| **统计函数** | `contrib/stats/stats-inl.h` | 均值、方差、分位数 |
| **滤波器** | 实现示例 | FIR、IIR、卷积 |
| **矩阵运算** | 实现示例 | 矩阵乘法、转置 |

| 优化技巧 | 说明 |
|----------|------|
| **缓存友好** | 连续内存访问 |
| **循环展开** | 4/8 重展开减少循环开销 |
| **预取** | 提前加载数据 |
| **并行处理** | 多线程分解任务 |

---

## 练习

1. 实现 FIR 低通滤波器并测试
2. 计算大型矩阵的统计信息
3. 使用 FFT 计算信号的频率谱
4. 实现图像的高斯模糊

---

## 下一步

明天我们将学习 Highway 的测试框架和最佳实践，包括如何编写测试、调试 SIMD 代码。
