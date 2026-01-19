# Day 10: 内存管理和对齐

## 课程目标

今天我们将学习 Highway 的内存管理系统，特别是 SIMD 内存对齐的需求和实现。

---

## 10.1 为什么需要对齐？

### SIMD 指令的对齐要求

```cpp
// 未对齐数据的问题
float* unaligned = malloc_array();

// SSE/AVX: 对齐加载更快
__m128 v = _mm_load_ps(unaligned);  // 未对齐，可能崩溃（旧 CPU）
__m128 v = _mm_loadu_ps(unaligned);  // 非对齐指令，安全但较慢

// ARM NEON: vld1q 不严格要求对齐，但对齐更好
float32x4_t v = vld1q_f32(unaligned);
```

### 性能影响

| 情况 | 对齐加载 | 未对齐加载 |
|------|----------|------------|
| SSE/AVX | 1 周期 | 2-3 周期 |
| AVX-512 | 1 周期 | 可能跨缓存行，4-6 周期 |
| ARM NEON | 1 周期 | 1-2 周期 |

---

## 10.2 HWY_ALIGNMENT 常量

```cpp
// aligned_allocator.h

// SIMD 操作的最小对齐
// 原因：
// 1. AVX-512: 512-bit = 64 字节，需要 64 字节对齐
// 2. 防止伪共享：L1 缓存行通常 64 字节
// 3. M1 L2 和 POWER8 L2 缓存行是 128 字节
#define HWY_ALIGNMENT 128
```

### 各平台的对齐需求

```cpp
// 向量大小和对齐需求
#if HWY_TARGET == HWY_SSE2
constexpr size_t kVectorBytes = 16;   // 128-bit
constexpr size_t kAlignment = 16;

#elif HWY_TARGET == HWY_AVX2
constexpr size_t kVectorBytes = 32;   // 256-bit
constexpr size_t kAlignment = 32;

#elif HWY_TARGET == HWY_AVX3
constexpr size_t kVectorBytes = 64;   // 512-bit
constexpr size_t kAlignment = 64;

#elif HWY_TARGET == HWY_NEON
constexpr size_t kVectorBytes = 16;   // 128-bit
constexpr size_t kAlignment = 16;

#elif HWY_TARGET == HWY_SVE
constexpr size_t kVectorBytes = 64;   // 最大 2048-bit，使用 64 字节对齐
constexpr size_t kAlignment = 64;
#endif

// Highway 使用 HWY_ALIGNMENT = 128 作为统一对齐
```

---

## 10.3 IsAligned 函数

```cpp
// aligned_allocator.h
template <typename T>
HWY_API constexpr bool IsAligned(T* ptr, size_t align = HWY_ALIGNMENT) {
  return reinterpret_cast<uintptr_t>(ptr) % align == 0;
}

// 使用示例
float* data = AllocateArray(100);

if (IsAligned(data)) {
  // 使用对齐加载
  auto v = Load(df, data);
} else {
  // 使用非对齐加载
  auto v = LoadU(df, data);
}
```

---

## 10.4 AllocateAlignedBytes / FreeAlignedBytes

### 基本分配函数

```cpp
// aligned_allocator.h

// 分配对齐内存
HWY_DLLEXPORT void* AllocateAlignedBytes(size_t payload_size,
                                         AllocPtr alloc_ptr,
                                         void* opaque_ptr);

// 释放对齐内存
HWY_DLLEXPORT void FreeAlignedBytes(const void* aligned_pointer,
                                    FreePtr free_ptr,
                                    void* opaque_ptr);

// AllocPtr 和 FreePtr 函数指针类型
using AllocPtr = void* (*)(void* opaque, size_t bytes);
using FreePtr = void (*)(void* opaque, void* memory);
```

### 默认使用

```cpp
// 使用默认的 malloc/free
float* data = static_cast<float*>(
    AllocateAlignedBytes(100 * sizeof(float)));

// ... 使用 ...

FreeAlignedBytes(data, nullptr, nullptr);
```

### 自定义分配器

```cpp
// 自定义内存分配器
void* MyAlloc(void* opaque, size_t bytes) {
  // 例如：使用 arena allocator
  Arena* arena = static_cast<Arena*>(opaque);
  return arena->Allocate(bytes);
}

void MyFree(void* opaque, void* memory) {
  // 例如：arena 不需要释放单个内存
}

// 使用
Arena my_arena;

float* data = static_cast<float*>(
    AllocateAlignedBytes(100 * sizeof(float), MyAlloc, &my_arena));

// ... 使用 ...

FreeAlignedBytes(data, MyFree, &my_arena);
```

---

## 10.5 AlignedAllocator：STL 兼容分配器

```cpp
// aligned_allocator.h

template <class T>
struct AlignedAllocator {
  using value_type = T;

  AlignedAllocator() = default;

  // 从其他类型构造
  template <class V>
  explicit AlignedAllocator(const AlignedAllocator<V>&) noexcept {}

  // 分配
  template <class V>
  value_type* allocate(V n) {
    static_assert(std::is_integral<V>::value);
    return static_cast<value_type*>(
        AllocateAlignedBytes(static_cast<size_t>(n) * sizeof(value_type)));
  }

  // 释放
  template <class V>
  void deallocate(value_type* p, HWY_MAYBE_UNUSED V n) {
    FreeAlignedBytes(p, nullptr, nullptr);
  }
};

// 比较相等
template <class T, class V>
constexpr bool operator==(const AlignedAllocator<T>&,
                          const AlignedAllocator<V>&) noexcept {
  return true;
}
```

### 与 STL 容器配合使用

```cpp
#include "hwy/aligned_allocator.h"
#include <vector>

// 使用 AlignedAllocator 的 std::vector
using AlignedVectorFloat = std::vector<float, hwy::AlignedAllocator<float>>;

AlignedVectorFloat data(1000);

// 数据自动对齐
// 可以安全地使用 Load()
const ScalableTag<float> df;
auto v = Load(df, data.data());  // 对齐加载
```

### AlignedVector 别名

```cpp
// aligned_allocator.h
template <class T>
using AlignedVector = std::vector<T, AlignedAllocator<T>>;

// 使用
AlignedVector<float> f32_vec(1000);
AlignedVector<int32_t> i32_vec(500);
```

---

## 10.6 AlignedUniquePtr：RAII 包装

```cpp
// aligned_allocator.h

// AlignedDeleter：自定义删除器
class AlignedDeleter {
 public:
  AlignedDeleter() : free_(nullptr), opaque_ptr_(nullptr) {}
  AlignedDeleter(FreePtr free_ptr, void* opaque_ptr)
      : free_(free_ptr), opaque_ptr_(opaque_ptr) {}

  template <typename T>
  void operator()(T* aligned_pointer) const {
    return DeleteAlignedArray(aligned_pointer, free_, opaque_ptr_,
                              TypedArrayDeleter<T>);
  }

 private:
  template <typename T>
  static void TypedArrayDeleter(void* ptr, size_t size_in_bytes) {
    size_t elems = size_in_bytes / sizeof(T);
    for (size_t i = 0; i < elems; i++) {
      (static_cast<T*>(ptr) + i)->~T();
    }
  }

  HWY_DLLEXPORT static void DeleteAlignedArray(void* aligned_pointer,
                                               FreePtr free_ptr,
                                               void* opaque_ptr,
                                               ArrayDeleter deleter);

  FreePtr free_;
  void* opaque_ptr_;
};

// AlignedUniquePtr：与标准 unique_ptr 兼容
template <typename T>
using AlignedUniquePtr = std::unique_ptr<T, AlignedDeleter>;
```

### MakeUniqueAligned

```cpp
// 创建单个对齐对象
template <typename T, typename... Args>
AlignedUniquePtr<T> MakeUniqueAligned(Args&&... args) {
  T* ptr = static_cast<T*>(AllocateAlignedBytes(sizeof(T)));
  return AlignedUniquePtr<T>(new (ptr) T(std::forward<Args>(args)...),
                             AlignedDeleter());
}

// 使用
struct SIMDData {
  float values[4];
  // ...
};

auto data = MakeUniqueAligned<SIMDData>();
// data 自动对齐，并在作用域结束时释放
```

### MakeUniqueAlignedArray

```cpp
// 创建对齐数组
template <typename T, typename... Args>
AlignedUniquePtr<T[]> MakeUniqueAlignedArray(size_t items, Args&&... args) {
  return MakeUniqueAlignedArrayWithAlloc<T, Args...>(
      items, nullptr, nullptr, nullptr, std::forward<Args>(args)...);
}

// 使用
auto arr = MakeUniqueAlignedArray<float>(100);
// arr[0] = 1.0f;
// arr[99] = 99.0f;
```

---

## 10.7 AlignedFreeUniquePtr：POD 类型

```cpp
// AlignedFreer：只调用 free（不调用析构函数）
class AlignedFreer {
 public:
  static void DoNothing(void* /*opaque*/, void* /*aligned_pointer*/) {}

  AlignedFreer() : free_(nullptr), opaque_ptr_(nullptr) {}
  AlignedFreer(FreePtr free_ptr, void* opaque_ptr)
      : free_(free_ptr), opaque_ptr_(opaque_ptr) {}

  template <typename T>
  void operator()(T* aligned_pointer) const {
    FreeAlignedBytes(aligned_pointer, free_, opaque_ptr_);
  }

 private:
  FreePtr free_;
  void* opaque_ptr_;
};

template <typename T>
using AlignedFreeUniquePtr = std::unique_ptr<T, AlignedFreer>;
```

### AllocateAligned

```cpp
// 分配未初始化的 POD 数组
template <typename T>
AlignedFreeUniquePtr<T[]> AllocateAligned(const size_t items) {
  static_assert(std::is_trivially_copyable<T>::value);
  static_assert(std::is_trivially_destructible<T>::value);
  return AllocateAligned<T>(items, nullptr, nullptr, nullptr);
}

// 使用
auto buffer = AllocateAligned<float>(1000);
// buffer[0] = 1.0f;
// 自动释放
```

---

## 10.8 Span 类：轻量级视图

```cpp
// aligned_allocator.h

template <typename T>
class Span {
 public:
  Span() = default;
  Span(T* data, size_t size) : size_(size), data_(data) {}
  template <typename U>
  Span(U u) : Span(u.data(), u.size()) {}

  // 大小
  size_t size() const { return size_; }

  // 指针
  T* data() { return data_; }
  T* data() const { return data_; }

  // 索引访问
  T& operator[](size_t index) const { return data_[index]; }

  // 迭代器
  T* begin() { return data_; }
  T* end() { return data_[size_]; }

 private:
  size_t size_ = 0;
  T* data_ = nullptr;
};
```

### Span 使用示例

```cpp
void ProcessArray(Span<const float> input, Span<float> output) {
  const ScalableTag<float> df;
  const size_t N = Lanes(df);

  for (size_t i = 0; i + N <= input.size(); i += N) {
    auto v = Load(df, &input[i]);
    // ... 处理 ...
    Store(v, df, &output[i]);
  }
}

// 使用
AlignedVector<float> input(1000);
AlignedVector<float> output(1000);

ProcessArray(Span<const float>(input), Span<float>(output));
```

---

## 10.9 AlignedNDArray：多维数组

```cpp
// aligned_allocator.h

template <typename T, size_t axes>
class AlignedNDArray {
  static_assert(std::is_trivial<T>::value);

 public:
  AlignedNDArray(AlignedNDArray&& other) = default;
  AlignedNDArray& operator=(AlignedNDArray&& other) = default;

  // 构造：指定形状
  explicit AlignedNDArray(std::array<size_t, axes> shape) : shape_(shape) {
    sizes_ = ComputeSizes(shape_);
    memory_shape_ = shape_;
    // 内维度填充到对齐
    memory_shape_[axes - 1] = RoundUpTo(memory_shape_[axes - 1], VectorBytes());
    memory_sizes_ = ComputeSizes(memory_shape_);
    buffer_ = hwy::AllocateAligned<T>(memory_size());
    hwy::ZeroBytes(buffer_.get(), memory_size() * sizeof(T));
  }

  // 访问内维度（返回 Span）
  Span<T> operator[](std::array<const size_t, axes - 1> indices) {
    return Span<T>(buffer_.get() + Offset(indices), sizes_[indices.size()]);
  }

  Span<const T> operator[](std::array<const size_t, axes - 1> indices) const {
    return Span<const T>(buffer_.get() + Offset(indices),
                         sizes_[indices.size()]);
  }

  // 形状
  const std::array<size_t, axes>& shape() const { return shape_; }
  const std::array<size_t, axes>& memory_shape() const { return memory_shape_; }

  size_t size() const { return sizes_[0]; }
  size_t memory_size() const { return memory_sizes_[0]; }

  T* data() { return buffer_.get(); }
  const T* data() const { return buffer_.get(); }

 private:
  std::array<size_t, axes> shape_;
  std::array<size_t, axes> memory_shape_;
  std::array<size_t, axes + 1> sizes_;
  std::array<size_t, axes + 1> memory_sizes_;
  hwy::AlignedFreeUniquePtr<T[]> buffer_;
};
```

### AlignedNDArray 使用示例

```cpp
// 2D 数组：100 行，每行 128 个 float
AlignedNDArray<float, 2> array({100, 128});

// 访问第 0 行
Span<float> row0 = array[{0}];

// 处理一行
for (size_t i = 0; i < row0.size(); ++i) {
  row0[i] = i;
}

// 访问第 5 行
Span<float> row5 = array[{5}];
```

---

## 10.10 完整示例：SIMD 图像缓冲区

```cpp
#include "hwy/aligned_allocator.h"
#include "hwy/highway.h"

namespace hn = hwy::HWY_NAMESPACE;

// 对齐的图像缓冲区
class ImageBuffer {
 public:
  ImageBuffer(size_t width, size_t height, size_t channels)
      : width_(width), height_(height), channels_(channels) {
    // 分配对齐内存
    data_ = hwy::AllocateAligned<float>(width_ * height_ * channels_);
  }

  ~ImageBuffer() {
    if (data_) {
      hwy::FreeAlignedBytes(data_, nullptr, nullptr);
    }
  }

  // 获取行指针（对齐）
  float* RowPtr(size_t y) {
    return &data_[y * width_ * channels_];
  }

  const float* RowPtr(size_t y) const {
    return &data_[y * width_ * channels_];
  }

  // 检查行是否对齐
  bool IsRowAligned(size_t y) const {
    return hwy::IsAligned(RowPtr(y),
                         hwy::HWY_ALIGNMENT / sizeof(float));
  }

  size_t width() const { return width_; }
  size_t height() const { return height_; }
  size_t channels() const { return channels_; }

 private:
  size_t width_;
  size_t height_;
  size_t channels_;
  float* data_;
};

// 使用图像缓冲区
void ProcessImage(const ImageBuffer& input, ImageBuffer& output) {
  const ScalableTag<float> df;
  const size_t N = Lanes(df);

  for (size_t y = 0; y < input.height(); ++y) {
    const float* in_row = input.RowPtr(y);
    float* out_row = output.RowPtr(y);

    size_t x = 0;
    // 使用对齐加载
    for (; x + N <= input.width(); x += N) {
      auto v = Load(df, &in_row[x * input.channels()]);
      // ... 处理 ...
      Store(v, df, &out_row[x * input.channels()]);
    }
    // 处理剩余元素
    for (; x < input.width(); ++x) {
      // 标量处理
    }
  }
}
```

---

## 10.11 填充数组的最佳实践

```cpp
// 好的做法：填充数组到向量大小
template <typename T>
void ProcessPadded(T* data, size_t count) {
  const ScalableTag<T> d;
  const size_t N = Lanes(d);

  // 分配填充后的数组
  size_t padded_count = RoundUpTo(count, N);
  AlignedFreeUniquePtr<T[]> padded = AllocateAligned<T>(padded_count);

  // 拷贝数据并填充零
  for (size_t i = 0; i < padded_count; ++i) {
    padded[i] = (i < count) ? data[i] : T{0};
  }

  // 处理（无需尾部处理）
  for (size_t i = 0; i < padded_count; i += N) {
    auto v = Load(d, &padded[i]);
    // ... 处理 ...
  }
}

// 不好的做法：手动处理尾部
template <typename T>
void ProcessUnpadded(T* data, size_t count) {
  const ScalableTag<T> d;
  const size_t N = Lanes(d);

  size_t i = 0;
  for (; i + N <= count; i += N) {
    auto v = LoadU(d, &data[i]);
    // ... 处理 ...
  }

  // 复杂的尾部处理
  if (i < count) {
    // 使用掩码、标量处理等
  }
}
```

---

## 本日总结

| 功能 | 类/函数 | 用途 |
|------|----------|------|
| **基本分配** | `AllocateAlignedBytes()` / `FreeAlignedBytes()` | 底层分配/释放 |
| **STL 分配器** | `AlignedAllocator<T>` | 与 STL 容器配合 |
| **RAII 包装** | `AlignedUniquePtr<T>` | 自动内存管理 |
| **POD 包装** | `AlignedFreeUniquePtr<T[]>` | 无析构函数的类型 |
| **视图** | `Span<T>` | 轻量级数组视图 |
| **多维数组** | `AlignedNDArray<T, axes>` | 多维对齐数组 |
| **辅助函数** | `IsAligned()`, `AlignTo()` | 对齐检查和计算 |
| **对齐常量** | `HWY_ALIGNMENT` | 统一对齐要求（128 字节）|

---

## 练习

1. 使用 `AlignedVector` 创建一个存储 10000 个 float 的向量
2. 使用 `AlignedUniquePtr` 创建一个自动管理的对齐结构体
3. 使用 `AlignedNDArray` 创建一个 10×20 的 2D 数组

---

## 下一步

明天我们将学习循环向量化和变换，了解 `contrib/algo/` 模块提供的功能。
