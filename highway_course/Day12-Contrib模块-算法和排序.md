# Day 12: Contrib 模块 - 算法和排序

## 课程目标

今天我们将学习 Highway 的 `contrib/` 模块，重点关注排序算法和其他高级功能。

---

## 12.1 contrib 模块结构

```
contrib/
├── algo/                    # 基础算法
│   ├── transform-inl.h      # 数组变换
│   ├── copy-inl.h          # 内存拷贝
│   ├── find-inl.h          # 查找
│   └── foreach-inl.h       # 遍历
├── sort/                   # 排序算法
│   ├── vqsort-inl.h        # VQSort
│   └── vqsort-emergency.h  # 紧急排序
├── math/                    # 数学函数
│   ├── math-inl.h          # 基础数学
│   ├── polynomial-inl.h    # 多式
│   └── sin-cos-inl.h       # sin/cos
├── stats/                   # 统计函数
│   └── stats-inl.h         # mean/var 等
└── thread_pool/             # 线程池
    ├── thread_pool.h
    └── thread_pool-inl.h
```

---

## 12.2 VQSort：向量化快速排序

### 什么是 VQSort？

VQSort 是 Highway 的**SIMD 优化的快速排序算法**，支持多种数据类型和比较器。

### 基本使用

```cpp
#include "hwy/contrib/sort/vqsort.h"

// 基本排序
template <typename T>
void SortExample(T* data, size_t count) {
  hwy::VQSort(data, count);
}

// 降序排序
template <typename T>
void SortDescendingExample(T* data, size_t count) {
  // 自定义比较器
  auto compare_less = [](const T& a, const T& b) { return a < b; };
  auto compare_greater = [](const T& a, const T& b) { return a > b; };

  hwy::VQSort(data, count, compare_greater);
}

// 使用示例
int main() {
  const size_t n = 1000000;
  AlignedVector<int32_t> data(n);

  // 初始化数据
  for (size_t i = 0; i < n; ++i) {
    data[i] = static_cast<int32_t>(n - i);  // 逆序
  }

  // 排序
  SortExample(data.data(), n);

  // 结果：1, 2, 3, ..., 1000000
  return 0;
}
```

### VQSort 的高级功能

```cpp
// 指定比较器
template <typename T>
struct Point2D {
  float x, y;
};

void SortPointsByX(Point2D* points, size_t count) {
  auto compare_by_x = [](const Point2D& a, const Point2D& b) {
    return a.x < b.x;
  };

  hwy::VQSort(points, count, compare_by_x);
}

// 稳定排序选项
void StableSortExample(float* data, size_t count) {
  // VQSort 间接支持稳定排序
  // 1. 添加索引
  AlignedVector<std::pair<float, size_t>> indexed_data(count);
  for (size_t i = 0; i < count; ++i) {
    indexed_data[i] = {data[i], i};
  }

  // 2. 排序：值相等时比较索引
  auto compare_with_index = [](const auto& a, const auto& b) {
    if (a.first != b.first) {
      return a.first < b.first;
    }
    return a.second < b.second;  // 稳定
  };

  hwy::VQSort(indexed_data.data(), count, compare_with_index);

  // 3. 提取排序后的值
  for (size_t i = 0; i < count; ++i) {
    data[i] = indexed_data[i].first;
  }
}
```

### VQSort 的性能优势

| 特性 | 传统 QuickSort | VQSort |
|------|---------------|--------|
| 比较次数 | O(n log n) | O(n log n) |
| 分区方式 | 交换元素 | SIMD 批量处理 |
| 缓存友好 | 中等 | 高度优化 |
| 小数组处理 | 递归插入 | 向量化插入 |

---

## 12.3 分区和合并

### VQSort 内部机制

```cpp
// VQSort 核心分区算法（简化版）
template <typename T, typename Compare>
size_t Partition(T* data, size_t left, size_t right, Compare compare) {
  // 选择基准值（通常中间元素）
  T pivot = data[(left + right) / 2];

  // 双指针分区
  size_t i = left - 1;
  size_t j = right + 1;

  while (true) {
    do { ++i; } while (compare(data[i], pivot));
    do { --j; } while (compare(pivot, data[j]));

    if (i >= j) return j;

    std::swap(data[i], data[j]);
  }
}

// SIMD 优化的分区
template <typename T, typename Compare>
size_t SIMDPartition(T* data, size_t left, size_t right, Compare compare) {
  // 1. 批量比较与基准值
  // 2. 使用掩码提取分区
  // 3. 移动分区到正确位置
}
```

### 并行排序

```cpp
#include <thread>
#include "hwy/contrib/sort/vqsort.h"

template <typename T>
void ParallelSort(T* data, size_t count, size_t min_size = 10000) {
  if (count <= min_size) {
    // 小数组直接排序
    hwy::VQSort(data, count);
    return;
  }

  // 自动检测线程数
  const size_t num_threads = std::thread::hardware_concurrency();

  if (num_threads <= 1) {
    hwy::VQSort(data, count);
    return;
  }

  // 分割数据并并行排序
  std::vector<std::thread> threads;
  size_t chunk_size = count / num_threads;

  for (size_t i = 0; i < num_threads; ++i) {
    size_t start = i * chunk_size;
    size_t end = (i == num_threads - 1) ? count : (i + 1) * chunk_size;

    threads.emplace_back([start, end, data]() {
      hwy::VQSort(&data[start], end - start);
    });
  }

  // 等待所有线程完成
  for (auto& t : threads) {
    t.join();
  }

  // 合并已排序的块
  std::inplace_merge(data, data + chunk_size, data + count);
}
```

---

## 12.4 算法模式匹配

### 查找模式

```cpp
#include "hwy/contrib/algo/find-inl.h"

// 查找所有满足条件的元素索引
template <typename T>
std::vector<size_t> FindAll(const T* data, size_t count,
                            std::function<bool(const T&)> predicate) {
  const ScalableTag<T> d;

  // 使用 FindIf 查找第一个匹配的
  auto result = FindIf(d, data, count, [predicate](auto d, auto v) {
    // 在 SIMD 实现中，需要将标量谓词转换为向量谓词
    // 这里简化处理
    return Set(d, T(predicate(GetLane(v, 0)) ? 1 : 0));
  });

  std::vector<size_t> indices;
  // 收集所有匹配的索引（简化版）
  return indices;
}

// 查找最小/最大值
template <typename T>
std::pair<T, T> FindMinMax(const T* data, size_t count) {
  const ScalableTag<T> d;

  T min_val = std::numeric_limits<T>::max();
  T max_val = std::numeric_limits<T>::min();

  ForEach1(d, data, count, [&min_val, &max_val](auto d, auto v) {
    // 提取所有通道的最小/最大值
    v = Min(v, Set(d, min_val));
    min_val = GetLane(v, 0);

    v = Max(v, Set(d, max_val));
    max_val = GetLane(v, 0);
  });

  return {min_val, max_val};
}
```

### 排序算法比较

```cpp
#include <algorithm>
#include <chrono>

template <typename T, typename SortFunc>
void BenchmarkSort(const char* name, SortFunc sort_func,
                   size_t data_size, size_t iterations) {
  std::vector<T> data(data_size);
  for (size_t i = 0; i < data_size; ++i) {
    data[i] = static_cast<T>(data_size - i);  // 逆序
  }

  // 预热
  sort_func(data.data(), data_size);

  auto start = std::chrono::high_resolution_clock::now();

  for (size_t iter = 0; iter < iterations; ++iter) {
    for (size_t i = 0; i < data_size; ++i) {
      data[i] = static_cast<T>(data_size - i);
    }
    sort_func(data.data(), data_size);
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  printf("%s: %zu ms (data_size=%zu, iterations=%zu)\n",
         name, duration.count(), data_size, iterations);
}

int main() {
  const size_t size = 1000000;
  size_t iterations = 10;

  BenchmarkSort<int32_t>("std::sort",
      [](int32_t* data, size_t n) { std::sort(data, data + n); },
      size, iterations);

  BenchmarkSort<int32_t>("VQSort",
      [](int32_t* data, size_t n) { hwy::VQSort(data, n); },
      size, iterations);

  return 0;
}
```

---

## 12.5 字符串和复杂类型排序

### 字符串排序

```cpp
// 按字典序排序字符串数组
void SortStrings(char** strings, size_t count) {
  auto compare_strings = [](const char* a, const char* b) {
    return strcmp(a, b) < 0;
  };

  hwy::VQSort(strings, count, compare_strings);
}

// 自定义字符串排序（前置字符）
bool CompareFirstChar(const char* a, const char* b) {
  return *a < *b;
}

void SortStringsByFirstChar(char** strings, size_t count) {
  hwy::VQSort(strings, count, CompareFirstChar);
}
```

### 结构体排序

```cpp
struct Person {
  std::string name;
  int age;
  double salary;
};

// 按年龄排序
bool CompareByAge(const Person& a, const Person& b) {
  return a.age < b.age;
}

void SortPeopleByAge(Person* people, size_t count) {
  // VQSort 需要随机访问迭代器
  // 这里简化处理
  std::sort(people, people + count, CompareByAge);
}

// 多字段排序
bool ComparePeople(const Person& a, const Person& b) {
  if (a.name != b.name) {
    return a.name < b.name;
  }
  if (a.age != b.age) {
    return a.age < b.age;
  }
  return a.salary < b.salary;
}
```

---

## 12.6 高级排序选项

### 自定义分配器

```cpp
#include "hwy/aligned_allocator.h"

// 使用自定义分配器
template <typename T>
void SortWithCustomAlloc(T* data, size_t count) {
  // 使用 aligned_allocator
  AlignedVector<T> temp(count);

  hwy::VQSort(data, count, std::less<T>(), &temp[0]);
}
```

### 部分排序

```cpp
// 查找第 k 小的元素（快速选择）
template <typename T>
T QuickSelect(T* data, size_t count, size_t k) {
  // 使用 std::nth_element 或实现快速选择
  std::nth_element(data, data + k, data + count);
  return data[k];
}

// 查找前 k 个最小值
template <typename T>
std::vector<T> FindKSmallest(T* data, size_t count, size_t k) {
  std::partial_sort(data, data + k, data + count);
  std::vector<T> result(data, data + k);
  return result;
}
```

---

## 12.7 完整示例：统计数据的排序

```cpp
#include "hwy/contrib/sort/vqsort.h"
#include "hwy/contrib/algo/foreach-inl.h"
#include "hwy/aligned_allocator.h"

// 统计数据
struct DataPoint {
  float value;
  std::string category;
  size_t timestamp;
};

// 按值排序
void SortByValue(DataPoint* data, size_t count) {
  auto compare = [](const DataPoint& a, const DataPoint& b) {
    return a.value < b.value;
  };

  hwy::VQSort(data, count, compare);
}

// 计算分位数
std::vector<float> CalculateQuantiles(const DataPoint* data, size_t count, size_t num_bins) {
  std::vector<float> quantiles;

  if (count == 0) return quantiles;

  // 复制数据并排序
  AlignedVector<DataPoint> sorted_data(data, data + count);
  SortByValue(sorted_data.data(), sorted_data.size());

  // 计算分位数
  quantiles.reserve(num_bins + 1);
  quantiles.push_back(sorted_data[0].value);  // 最小值

  for (size_t i = 1; i < num_bins; ++i) {
    size_t idx = static_cast<size_t>((i * count) / num_bins);
    quantiles.push_back(sorted_data[idx].value);
  }

  quantiles.push_back(sorted_data[count - 1].value);  // 最大值
  return quantiles;
}

// 计算统计量
struct Statistics {
  float min;
  float max;
  float mean;
  float median;
  float stddev;
};

Statistics CalculateStatistics(const DataPoint* data, size_t count) {
  Statistics stats = {0};

  if (count == 0) return stats;

  // 最小值和最大值
  const ScalableTag<float> df;
  auto v_min = Set(df, std::numeric_limits<float>::max());
  auto v_max = Set(df, std::numeric_limits<float>::min());
  auto v_sum = Set(df, 0.0f);

  ForEach1(df, &data[0].value, count, [&v_min, &v_max, &v_sum](auto d, auto v) {
    v_min = Min(v_min, v);
    v_max = Max(v_max, v);
    v_sum = Add(v_sum, v);
  });

  stats.min = GetLane(v_min, 0);
  stats.max = GetLane(v_max, 0);

  // 平均值
  stats.mean = GetLane(v_sum, 0) / static_cast<float>(count);

  // 中位数（需要排序）
  AlignedVector<float> values(count);
  for (size_t i = 0; i < count; ++i) {
    values[i] = data[i].value;
  }

  hwy::VQSort(values.data(), count);

  if (count % 2 == 0) {
    stats.median = (values[count/2 - 1] + values[count/2]) / 2.0f;
  } else {
    stats.median = values[count/2];
  }

  // 标准差
  auto v_sum_sq = Set(df, 0.0f);
  ForEach1(df, &data[0].value, count, [&v_sum_sq, mean = stats.mean](auto d, auto v) {
    auto diff = Sub(v, Set(d, mean));
    v_sum_sq = Add(v_sum_sq, Mul(diff, diff));
  });

  float variance = GetLane(v_sum_sq, 0) / static_cast<float>(count);
  stats.stddev = std::sqrt(variance);

  return stats;
}

int main() {
  // 生成测试数据
  const size_t data_size = 1000000;
  AlignedVector<DataPoint> data(data_size);

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> dis(0.0, 100.0);

  for (size_t i = 0; i < data_size; ++i) {
    data[i].value = static_cast<float>(dis(gen));
    data[i].category = "Category_" + std::to_string(i % 10);
    data[i].timestamp = i;
  }

  // 计算统计量
  auto stats = CalculateStatistics(data.data(), data.size());
  printf("Statistics:\n");
  printf("  Min: %.2f\n", stats.min);
  printf("  Max: %.2f\n", stats.max);
  printf("  Mean: %.2f\n", stats.mean);
  printf("  Median: %.2f\n", stats.median);
  printf("  StdDev: %.2f\n", stats.stddev);

  // 计算分位数
  auto quantiles = CalculateQuantiles(data.data(), data.size(), 10);
  printf("\nQuantiles:\n");
  for (float q : quantiles) {
    printf("  %.2f\n", q);
  }

  return 0;
}
```

---

## 12.8 性能优化建议

### 1. 避免频繁创建分配器

```cpp
// 低效的做法
void LowEfficientSort(int* data, size_t count) {
  for (int i = 0; i < 10; ++i) {
    // 每次创建临时缓冲区
    AlignedVector<int> temp(count);
    hwy::VQSort(data, count);
  }
}

// 高效的做法
void EfficientSort(int* data, size_t count) {
  AlignedVector<int> temp;
  temp.reserve(count);  // 只分配一次

  for (int i = 0; i < 10; ++i) {
    // 重用临时缓冲区
    hwy::VQSort(data, count);
  }
}
```

### 2. 批量排序

```cpp
template <typename T>
void SortBatches(T* data, size_t* sizes, size_t num_batches) {
  // 预分配临时空间
  size_t total_size = 0;
  for (size_t i = 0; i < num_batches; ++i) {
    total_size += sizes[i];
  }

  AlignedVector<T> temp(total_size);
  T* temp_ptr = temp.data();

  for (size_t i = 0; i < num_batches; ++i) {
    hwy::VQSort(&data[i], sizes[i], std::less<T>(), temp_ptr);
    temp_ptr += sizes[i];
  }
}
```

---

## 本日总结

| 功能 | 文件 | 说明 |
|------|------|------|
| **VQSort** | `contrib/sort/vqsort-inl.h` | SIMD 优化的快速排序 |
| **算法模式** | `contrib/algo/` | 查找、变换、拷贝 |
| **数学函数** | `contrib/math/` | sin、cos、exp 等 |
| **统计函数** | `contrib/stats/` | 平均值、标准差等 |
| **线程池** | `contrib/thread_pool/` | 并行计算 |

| VQSort 特性 | 说明 |
|-------------|------|
| **多数据类型** | int32_t, float, double, 自定义类型 |
| **自定义比较器** | 支持任意排序逻辑 |
| **并行支持** | 多线程排序和合并 |
| **内存效率** | 最小化临时内存使用 |

---

## 练习

1. 使用 VQSort 对 100 万个 float 排序并与 std::sort 比较性能
2. 实现自定义结构体的排序（包含多种字段）
3. 使用 FindIf 查找数组中的第一个素数
4. 实现并行 VQSort 算法

---

## 下一步

明天我们将学习 contrib 模块的其他高级功能，包括数学函数和其他工具。
