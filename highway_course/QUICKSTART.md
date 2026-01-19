# 快速开始 - 5 分钟上手 Highway

本指南帮助你在 5 分钟内编写并运行你的第一个 Highway SIMD 程序。

## 🎯 学习目标

- ✅ 编译并运行第一个 Highway 程序
- ✅ 理解基本的向量化循环结构
- ✅ 查看不同 SIMD 目标的性能差异

## 📋 前提条件

- C++17 编译器（GCC 7+, Clang 6+, MSVC 2019+）
- CMake 3.10+
- Highway 库（已克隆到 `/home/dev/highway`）

## 🚀 步骤 1：创建项目目录

```bash
mkdir -p ~/highway_quickstart
cd ~/highway_quickstart
```

## 📝 步骤 2：编写第一个程序

创建文件 `quickstart.cc`：

```cpp
#include <stdio.h>
#include <vector>
#include <chrono>
#include "hwy/highway.h"

// 使用 Highway 命名空间
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;

// 向量化求和函数
float VectorSum(const float* data, size_t count) {
  const hn::ScalableTag<float> d;
  const size_t N = hn::Lanes(d);

  auto sum = hn::Zero(d);

  // 主循环：处理完整的向量
  size_t i = 0;
  for (; i + N <= count; i += N) {
    auto v = hn::LoadU(d, &data[i]);
    sum = hn::Add(sum, v);
  }

  // 余数处理（标量）
  float scalar_sum = hn::ReduceSum(d, sum);
  for (; i < count; ++i) {
    scalar_sum += data[i];
  }

  return scalar_sum;
}

// 标量求和（用于对比）
float ScalarSum(const float* data, size_t count) {
  float sum = 0.0f;
  for (size_t i = 0; i < count; ++i) {
    sum += data[i];
  }
  return sum;
}

}  // namespace HWY_NAMESPACE

// 主函数
int main() {
  const size_t count = 10000000;  // 10M 元素
  std::vector<float> data(count);

  // 初始化数据
  for (size_t i = 0; i < count; ++i) {
    data[i] = static_cast<float>(i % 100);
  }

  // 打印目标信息
  printf("Running on target: %s\n", hwy::TargetName(HWY_TARGET));
  printf("Vector lanes: %zu\n", hwy::HWY_NAMESPACE::Lanes(hwy::ScalableTag<float>()));
  printf("Processing %zu elements...\n\n", count);

  // 标量版本
  auto start = std::chrono::high_resolution_clock::now();
  float scalar_result = HWY_NAMESPACE::ScalarSum(data.data(), count);
  auto end = std::chrono::high_resolution_clock::now();
  auto scalar_time = std::chrono::duration<double, std::milli>(end - start).count();

  printf("Scalar sum: %.2f (took %.3f ms)\n", scalar_result, scalar_time);

  // SIMD 版本
  start = std::chrono::high_resolution_clock::now();
  float vector_result = HWY_NAMESPACE::VectorSum(data.data(), count);
  end = std::chrono::high_resolution_clock::now();
  auto vector_time = std::chrono::duration<double, std::milli>(end - start).count();

  printf("Vector sum: %.2f (took %.3f ms)\n", vector_result, vector_time);

  // 验证结果
  if (fabs(scalar_result - vector_result) < 1e-3) {
    printf("✅ Results match!\n");
  } else {
    printf("❌ Results differ: %.2f vs %.2f\n", scalar_result, vector_result);
  }

  // 加速比
  printf("\n🚀 Speedup: %.2fx\n", scalar_time / vector_time);

  return 0;
}
```

## 🔧 步骤 3：创建 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.10)
project(HighwayQuickstart)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找或添加 Highway
if(EXISTS "/home/dev/highway")
  add_subdirectory(/home/dev/highway highway_build)
else()
  find_package(HWY REQUIRED)
endif()

# 创建可执行文件
add_executable(quickstart quickstart.cc)

# 链接 Highway
target_link_libraries(quickstart PRIVATE hwy)

# Release 构建优化
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR NOT CMAKE_BUILD_TYPE)
  target_compile_options(quickstart PRIVATE -O3)
endif()
```

## 🏗️ 步骤 4：编译

```bash
# 创建构建目录
mkdir build && cd build

# 配置（Release 模式）
cmake .. -DCMAKE_BUILD_TYPE=Release

# 编译
make -j

# 或使用 Ninja（更快）
# cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
# ninja
```

## ▶️ 步骤 5：运行

```bash
./quickstart
```

**预期输出（x86 AVX2）：**

```
Running on target: AVX2
Vector lanes: 8
Processing 10000000 elements...

Scalar sum: 49500000.00 (took 8.234 ms)
Vector sum: 49500000.00 (took 1.243 ms)
✅ Results match!

🚀 Speedup: 6.62x
```

**预期输出（ARM NEON）：**

```
Running on target: NEON
Vector lanes: 4
Processing 10000000 elements...

Scalar sum: 49500000.00 (took 10.123 ms)
Vector sum: 49500000.00 (took 2.456 ms)
✅ Results match!

🚀 Speedup: 4.12x
```

## 🎓 代码解析

### 1. 包含 Highway 头文件

```cpp
#include "hwy/highway.h"
```

这是使用 Highway 的唯一必需头文件。

### 2. 使用 HWY_NAMESPACE

```cpp
namespace HWY_NAMESPACE {
  // 你的 SIMD 代码
}
```

`HWY_NAMESPACE` 是一个宏，展开为特定目标的命名空间（例如 `N_AVX2`、`N_NEON`）。

### 3. 创建向量标签

```cpp
const hn::ScalableTag<float> d;
```

标签（Tag）指定向量的元素类型和大小。`ScalableTag` 自动适应硬件的向量宽度。

### 4. 查询向量大小

```cpp
const size_t N = hn::Lanes(d);
```

`Lanes(d)` 返回向量的通道数（例如，AVX2 上的 float 是 8）。

### 5. 向量操作

```cpp
auto v = hn::LoadU(d, &data[i]);  // 未对齐加载
sum = hn::Add(sum, v);             // 向量加法
```

Highway 的操作函数接受标签作为第一个参数，用于类型推导。

### 6. 水平归约

```cpp
float scalar_sum = hn::ReduceSum(d, sum);
```

`ReduceSum` 将向量的所有通道求和为单个标量。

## 🔍 探索更多

### 实验 1：查看其他向量操作

修改 `VectorSum` 来计算平方和：

```cpp
float VectorSumOfSquares(const float* data, size_t count) {
  const hn::ScalableTag<float> d;
  const size_t N = hn::Lanes(d);
  auto sum = hn::Zero(d);

  for (size_t i = 0; i + N <= count; i += N) {
    auto v = hn::LoadU(d, &data[i]);
    sum = hn::MulAdd(v, v, sum);  // sum += v * v (使用 FMA)
  }

  return hn::ReduceSum(d, sum);
}
```

### 实验 2：尝试不同的数据大小

修改 `count` 变量：

```cpp
const size_t count = 100;        // 小数据：可能无加速
const size_t count = 10000;      // 中等：开始看到加速
const size_t count = 10000000;   // 大数据：最大加速
```

观察加速比如何随数据大小变化。

### 实验 3：检查生成的汇编

```bash
# 生成汇编代码
g++ -O3 -S -masm=intel -mavx2 quickstart.cc -I/home/dev/highway

# 查看 quickstart.s
less quickstart.s
```

查找 `vmovups`, `vaddps` 等 AVX2 指令。

### 实验 4：对比不同编译选项

```bash
# 无优化
g++ -O0 quickstart.cc -o quickstart_O0 -I/home/dev/highway -lhwy

# 优化级别 2
g++ -O2 quickstart.cc -o quickstart_O2 -I/home/dev/highway -lhwy

# 优化级别 3 + 快速数学
g++ -O3 -ffast-math quickstart.cc -o quickstart_O3 -I/home/dev/highway -lhwy

# 对比性能
time ./quickstart_O0
time ./quickstart_O2
time ./quickstart_O3
```

## 📚 下一步

现在你已经运行了第一个 Highway 程序！接下来：

1. **深入学习架构：** 阅读 [01_ARCHITECTURE_DEEP_DIVE.md](01_ARCHITECTURE_DEEP_DIVE.md)
2. **探索更多操作：** 查看 [快速参考](快速参考.md) 中的 API 列表
3. **完成实战练习：** 尝试 [03_HANDS_ON_EXERCISES.md](03_HANDS_ON_EXERCISES.md) 中的实验
4. **阅读 FAQ：** 解答常见问题 [FAQ.md](FAQ.md)

## ❓ 故障排除

### 问题：找不到 hwy/highway.h

**解决方案：**

```bash
# 确保 Highway 路径正确
export HWY_PATH=/home/dev/highway

# 编译时指定包含路径
g++ -I${HWY_PATH} quickstart.cc -o quickstart -L${HWY_PATH}/build -lhwy
```

### 问题：链接错误（undefined reference）

**解决方案：**

```bash
# 先构建 Highway 库
cd /home/dev/highway
mkdir -p build && cd build
cmake .. && make -j

# 然后链接你的程序
g++ quickstart.cc -o quickstart \
  -I/home/dev/highway \
  -L/home/dev/highway/build \
  -lhwy
```

### 问题：性能没有提升

**可能原因：**

1. **Debug 模式：** 使用 `-DCMAKE_BUILD_TYPE=Release`
2. **数据太小：** 增加 `count` 到至少 10,000
3. **未对齐数据：** 使用 `AllocateAligned` 代替 `std::vector`

```cpp
// 使用对齐内存
hwy::AlignedFreeUniquePtr<float[]> data =
  hwy::AllocateAligned<float>(count);
```

## 🎉 完成！

恭喜！你已经成功：
- ✅ 编译并运行了 Highway 程序
- ✅ 看到了 SIMD 向量化的性能提升
- ✅ 理解了基本的 Highway API 使用

现在你可以继续深入学习 Highway 的高级特性了！

---

*本指南基于 Highway v1.3.0 | 最后更新：2026-01-19*
