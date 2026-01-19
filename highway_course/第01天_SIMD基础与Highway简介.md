# 第01天：SIMD基础与Highway简介

## 课程目标
- 理解SIMD的基本概念和重要性
- 了解Highway库的设计哲学
- 掌握向量化编程的基本思想
- 搭建Highway开发环境

---

## 1. 什么是SIMD？

### 1.1 SIMD的定义

**SIMD** (Single Instruction, Multiple Data) 是一种并行计算架构：
- **单条指令**：CPU执行一条机器指令
- **多个数据**：这条指令同时处理多个数据元素

### 1.2 为什么需要SIMD？

```cpp
// 传统标量代码
void add_scalar(float* a, float* b, float* c, int n) {
    for (int i = 0; i < n; i++) {
        c[i] = a[i] + b[i];  // 一次处理1个元素
    }
}

// SIMD向量代码（概念）
void add_vector(float* a, float* b, float* c, int n) {
    for (int i = 0; i < n; i += 8) {  // 一次处理8个元素
        // 单条指令同时加载8个float
        // 单条指令同时相加8对数字
        // 单条指令同时存储8个结果
    }
}
```

**性能提升：**
- **速度提升**：5-10倍甚至更高
- **能耗降低**：减少5倍能耗（因为执行的指令更少）
- **吞吐量提升**：现代CPU可以同时处理32-64个字节的数据

### 1.3 主流SIMD指令集

| 架构 | 指令集 | 向量宽度 | 备注 |
|------|--------|----------|------|
| x86 | SSE2 | 128位 (4个float) | 2001年引入 |
| x86 | AVX2 | 256位 (8个float) | 2013年引入 |
| x86 | AVX-512 | 512位 (16个float) | 2017年引入 |
| ARM | NEON | 128位 (4个float) | 移动设备标配 |
| ARM | SVE/SVE2 | 可扩展 (128-2048位) | 服务器级ARM |
| RISC-V | RVV | 可扩展 | 新兴架构 |

---

## 2. SIMD编程的挑战

### 2.1 传统方法的问题

**1. 平台特定的内联函数 (Intrinsics)**

```cpp
// x86 SSE代码 - 只能在x86上运行
#include <emmintrin.h>
__m128 a = _mm_load_ps(array);
__m128 b = _mm_add_ps(a, a);

// ARM NEON代码 - 只能在ARM上运行
#include <arm_neon.h>
float32x4_t a = vld1q_f32(array);
float32x4_t b = vaddq_f32(a, a);
```

**问题：**
- 代码不可移植
- 需要为每个平台编写不同代码
- 难以维护

**2. 编译器自动向量化**

```cpp
// 期望编译器自动向量化
void add(float* a, float* b, float* c, int n) {
    for (int i = 0; i < n; i++) {
        c[i] = a[i] + b[i];
    }
}
```

**问题：**
- 不可预测（编译器可能无法向量化）
- 不可控（无法指定使用哪种SIMD指令）
- 对代码修改敏感（小改动可能破坏向量化）

---

## 3. Highway的解决方案

### 3.1 设计理念

Highway提供了一个**统一的、可移植的、零成本抽象**的SIMD API：

```cpp
#include "hwy/highway.h"
namespace hn = hwy::HWY_NAMESPACE;

// 这段代码可以在x86、ARM、RISC-V等平台上运行！
template <class D>
void AddVectors(D d, const float* a, const float* b, float* c, size_t n) {
    for (size_t i = 0; i < n; i += hn::Lanes(d)) {
        auto va = hn::Load(d, a + i);      // 加载向量
        auto vb = hn::Load(d, b + i);      // 加载向量
        auto vc = hn::Add(va, vb);         // 向量加法
        hn::Store(vc, d, c + i);           // 存储结果
    }
}
```

### 3.2 核心优势

**1. 可移植性**
- 同一份代码在所有平台上工作
- 支持27个不同的SIMD目标

**2. 性能**
- 零成本抽象（编译后直接变成SIMD指令）
- 性能与手写内联函数相当

**3. 可预测性**
- 你写的每个操作都会变成SIMD指令
- 不依赖编译器优化

**4. 灵活的分发**
- **静态分发**：编译时选择单个目标
- **动态分发**：运行时选择最佳目标

---

## 4. Highway架构概览

### 4.1 核心概念

```
┌─────────────────────────────────────────┐
│         你的应用代码                      │
│  (使用Highway API编写SIMD算法)           │
└─────────────────┬───────────────────────┘
                  │
    ┌─────────────┴─────────────┐
    │   Highway抽象层            │
    │   (hwy/highway.h)          │
    │   统一的操作接口            │
    └─────────────┬─────────────┘
                  │
    ┌─────────────┴─────────────────────────────┐
    │         分发机制                            │
    │  静态分发 或 动态分发                       │
    └─────┬───────┬────────┬──────────┬─────────┘
          │       │        │          │
    ┌─────┴─┐ ┌──┴──┐ ┌───┴───┐ ┌───┴────┐
    │  SSE  │ │ AVX │ │ NEON  │ │  RVV   │
    │ 实现  │ │实现 │ │ 实现  │ │  实现  │
    └───────┘ └─────┘ └───────┘ └────────┘
```

### 4.2 文件组织

```
hwy/
├── highway.h          # 主头文件
├── base.h             # 基础类型和宏
├── detect_targets.h   # 目标检测
├── foreach_target.h   # 动态分发机制
├── ops/
│   ├── shared-inl.h       # 共享类型定义
│   ├── generic_ops-inl.h  # 通用操作
│   ├── x86_128-inl.h      # x86 SSE/AVX实现
│   ├── arm_neon-inl.h     # ARM NEON实现
│   ├── rvv-inl.h          # RISC-V实现
│   └── ...
└── contrib/           # 高级算法
    ├── sort/          # 向量化排序
    ├── math/          # 数学函数
    └── algo/          # 算法工具
```

---

## 5. 环境搭建

### 5.1 检查当前环境

```bash
# 在highway目录中
cd /home/dev/highway

# 查看README
cat README.md | head -50

# 查看构建系统
ls BUILD CMakeLists.txt
```

### 5.2 编译Highway

```bash
# 创建构建目录
mkdir -p build && cd build

# 配置CMake
cmake ..

# 编译
make -j

# 运行测试
make test
```

### 5.3 编译单个示例

```bash
# 查看示例代码
ls ../hwy/examples/

# 编译benchmark示例
make benchmark -j

# 运行
./benchmark
```

---

## 6. 第一个Highway程序

### 6.1 查看skeleton示例

```bash
# 查看skeleton示例的结构
cat ../hwy/examples/skeleton.h
cat ../hwy/examples/skeleton-inl.h
cat ../hwy/examples/skeleton.cc
```

**文件说明：**
- **skeleton.h** - 公共接口声明
- **skeleton-inl.h** - SIMD实现（可被多个目标重用）
- **skeleton.cc** - 分发代码（包含foreach_target.h）

### 6.2 分析skeleton-inl.h

让我们详细看看核心代码：

```cpp
// 特殊的包含守卫（用于动态分发）
#if defined(HIGHWAY_HWY_EXAMPLES_SKELETON_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef HIGHWAY_HWY_EXAMPLES_SKELETON_INL_H_
#undef HIGHWAY_HWY_EXAMPLES_SKELETON_INL_H_
#else
#define HIGHWAY_HWY_EXAMPLES_SKELETON_INL_H_
#endif

#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace skeleton {
namespace HWY_NAMESPACE {  // 这个命名空间会随目标变化

namespace hn = hwy::HWY_NAMESPACE;  // 别名，方便使用

// SIMD函数实现
template <class D, typename T>
HWY_MAYBE_UNUSED void MulAddLoop(
    const D d,                            // 标签（描述向量类型）
    const T* HWY_RESTRICT mul_array,
    const T* HWY_RESTRICT add_array,
    const size_t size,
    T* HWY_RESTRICT x_array) {

    for (size_t i = 0; i < size; i += hn::Lanes(d)) {
        // 1. 加载向量
        const auto mul = hn::Load(d, mul_array + i);
        const auto add = hn::Load(d, add_array + i);
        auto x = hn::Load(d, x_array + i);

        // 2. 执行SIMD操作：x = mul * x + add
        x = hn::MulAdd(mul, x, add);

        // 3. 存储结果
        hn::Store(x, d, x_array + i);
    }
}

}  // namespace HWY_NAMESPACE
}  // namespace skeleton
HWY_AFTER_NAMESPACE();

#endif
```

**关键点解析：**

1. **标签 (Tag)**：`D d`
   - 描述向量的类型和大小
   - 编译时参数，零运行时开销

2. **Lanes(d)**：返回向量中的元素个数
   - SSE: 4个float
   - AVX2: 8个float
   - 可扩展向量：运行时确定

3. **Load/Store**：内存操作
   - 自动处理对齐
   - 转换为平台特定指令

4. **MulAdd**：融合乘加操作
   - 一条指令完成乘法和加法
   - 利用FMA硬件指令（如果可用）

---

## 7. 关键概念总结

### 7.1 Highway的三大支柱

**1. 标签系统 (Tag System)**
```cpp
ScalableTag<float> d;  // 全向量
// d 描述了向量的所有信息，但大小为0
```

**2. 向量类型 (Vector Types)**
```cpp
auto v = Load(d, ptr);  // v的类型由d决定
// v 可能是 __m128, __m256, float32x4_t等
```

**3. 操作重载 (Overloaded Operations)**
```cpp
auto sum = Add(a, b);    // 根据a和b的类型选择正确的实现
```

### 7.2 Highway vs 传统内联函数

| 特性 | Highway | 传统内联函数 |
|------|---------|--------------|
| 可移植性 | ✅ 一份代码多平台 | ❌ 每个平台单独写 |
| 类型安全 | ✅ 编译时检查 | ⚠️ 部分检查 |
| 代码可读性 | ✅ 清晰的操作名 | ⚠️ 晦涩的指令名 |
| 性能 | ✅ 零成本抽象 | ✅ 直接映射硬件 |
| 学习曲线 | ⚠️ 需要理解概念 | ❌ 需要学习多套API |

---

## 8. 实践练习

### 练习1：探索skeleton示例

```bash
cd /home/dev/highway/build

# 查看skeleton测试
cat ../hwy/examples/skeleton_test.cc

# 运行测试
./skeleton_test
```

### 练习2：理解构建输出

```bash
# 查看编译选项
cmake .. -DCMAKE_VERBOSE_MAKEFILE=ON
make skeleton VERBOSE=1 | grep -i "sse\|avx\|neon"
```

观察编译器为不同目标生成的代码。

### 练习3：修改skeleton

尝试在`skeleton-inl.h`中添加一个新函数：

```cpp
// 计算向量的平方和: sum(x[i]^2)
template <class D, typename T>
T SquareSum(const D d, const T* array, size_t size) {
    auto sum = hn::Zero(d);
    for (size_t i = 0; i < size; i += hn::Lanes(d)) {
        auto v = hn::Load(d, array + i);
        sum = hn::MulAdd(v, v, sum);  // sum += v * v
    }
    return hn::ReduceSum(d, sum);
}
```

---

## 9. 下一步

在第2天，我们将深入学习：
- **标签系统**的详细设计
- **Simd<T, N, kPow2>**的奥秘
- 如何选择正确的标签
- 向量类型的内部表示

---

## 10. 参考资料

**代码位置：**
- `/home/dev/highway/hwy/highway.h` - 主头文件
- `/home/dev/highway/hwy/examples/` - 示例代码
- `/home/dev/highway/README.md` - 项目文档

**重要链接：**
- [SIMD for C++ Developers](http://const.me/articles/simd/simd.pdf)
- Highway Quick Reference: `g3doc/quick_reference.md`
- Highway Design Philosophy: `g3doc/design_philosophy.md`

---

## 作业

1. 编译并运行Highway的所有示例程序
2. 阅读`hwy/examples/skeleton-inl.h`，理解每一行代码
3. 查看`hwy/highway.h`的前100行，了解基本结构
4. 思考：为什么Highway使用"标签"而不是模板参数来指定向量类型？

---

**第01天课程完成！明天我们将深入探索Highway的类型系统。**
