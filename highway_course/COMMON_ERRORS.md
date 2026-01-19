# Highway 常见错误及解决方案

本文档列出了使用 Highway 时最常见的错误，以及快速解决方案。

## 目录

1. [编译错误](#编译错误)
2. [链接错误](#链接错误)
3. [运行时错误](#运行时错误)
4. [性能问题](#性能问题)
5. [逻辑错误](#逻辑错误)

---

## 编译错误

### ❌ 错误 1: "HWY_TARGET_INCLUDE not defined"

```
error: #error ">1 target enabled => define HWY_TARGET_INCLUDE before foreach_target.h"
```

**原因：** 使用动态调度但未定义目标包含文件。

**解决方案：**

```cpp
// ❌ 错误
#include "hwy/foreach_target.h"

// ✅ 正确
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "my_file.cc"  // 当前文件路径
#include "hwy/foreach_target.h"
```

---

### ❌ 错误 2: "redefinition of 'struct MyClass'"

```
error: redefinition of 'struct MyClass'
note: previous definition of 'struct MyClass' was here
```

**原因：** 忘记使用切换包含保护。

**解决方案：**

```cpp
// ❌ 错误：标准包含保护
#ifndef MY_HEADER_H
#define MY_HEADER_H
// 内容
#endif

// ✅ 正确：切换保护
#if defined(MY_HEADER_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef MY_HEADER_H_
#undef MY_HEADER_H_
#else
#define MY_HEADER_H_
#endif
// 内容
#endif
```

---

### ❌ 错误 3: "no matching function for call to 'Load'"

```
error: no matching function for call to 'Load'
note: candidate template ignored: could not match 'Simd' against 'int'
```

**原因：** 忘记传递标签参数。

**解决方案：**

```cpp
// ❌ 错误
auto v = Load(ptr);

// ✅ 正确
const ScalableTag<float> d;
auto v = Load(d, ptr);
```

---

### ❌ 错误 4: "use of undeclared identifier 'HWY_NAMESPACE'"

```
error: use of undeclared identifier 'HWY_NAMESPACE'
```

**原因：** 未包含 `hwy/highway.h` 或在 `foreach_target.h` 之前包含。

**解决方案：**

```cpp
// ✅ 正确顺序
#define HWY_TARGET_INCLUDE "my_file.cc"
#include "hwy/foreach_target.h"  // 必须先包含
#include "hwy/highway.h"         // 然后包含
```

---

### ❌ 错误 5: "static assertion failed: Lane must not be const"

```
error: static assertion failed: Lane must not be a reference type, const-qualified type...
```

**原因：** 标签类型使用了 const 或引用。

**解决方案：**

```cpp
// ❌ 错误
const ScalableTag<const float> d;  // const float 不对

// ✅ 正确
const ScalableTag<float> d;
```

---

### ❌ 错误 6: "Forgot kPow2 recursion terminator?"

```
error: static assertion failed: Forgot kPow2 recursion terminator?
```

**原因：** 在递归创建标签时，kPow2 变得过小（< HWY_MIN_POW2）。

**解决方案：**

```cpp
// ❌ 错误：递归没有终止
template <int kPow2>
void RecursiveFunc(Simd<T, N, kPow2> d) {
  RecursiveFunc(Half(d));  // 无限递归！
}

// ✅ 正确：添加终止条件
template <int kPow2>
void RecursiveFunc(Simd<T, N, kPow2> d) {
  if constexpr (kPow2 >= HWY_MIN_POW2) {
    RecursiveFunc(Half(d));
  }
}
```

---

## 链接错误

### ❌ 错误 7: "undefined reference to 'HWY_DYNAMIC_DISPATCH'"

```
undefined reference to `MyFunc__HighwayDispatchTable'
```

**原因：** 忘记在 `#if HWY_ONCE` 块中使用 `HWY_EXPORT`。

**解决方案：**

```cpp
// my_file.cc
namespace HWY_NAMESPACE {
  void MyFunc() { /* ... */ }
}

// ❌ 错误：忘记 HWY_EXPORT
void CallMyFunc() {
  HWY_DYNAMIC_DISPATCH(MyFunc)();  // 链接错误！
}

// ✅ 正确：添加 HWY_EXPORT
#if HWY_ONCE
HWY_EXPORT(MyFunc);

void CallMyFunc() {
  HWY_DYNAMIC_DISPATCH(MyFunc)();
}
#endif
```

---

### ❌ 错误 8: "undefined reference to 'hwy::...'"

```
undefined reference to `hwy::AllocateAlignedBytes(...)'
```

**原因：** 未链接 Highway 库。

**解决方案：**

```cmake
# CMakeLists.txt
target_link_libraries(my_app PRIVATE hwy)
```

或手动链接：

```bash
g++ my_app.cc -o my_app -lhwy
```

---

## 运行时错误

### ❌ 错误 9: SIGILL (Illegal Instruction)

```
Illegal instruction (core dumped)
```

**常见原因和解决方案：**

#### 原因 1：CPU 不支持选择的指令集

```bash
# 检查 CPU 支持的特性
cat /proc/cpuinfo | grep flags  # Linux
sysctl machdep.cpu.features     # macOS

# 查看 Highway 检测到的目标
HWY_TARGETS=0x... ./my_program
```

**解决：** 禁用不支持的目标

```cpp
// 在包含 Highway 之前
#define HWY_DISABLED_TARGETS (HWY_AVX3|HWY_AVX3_DL)
#include "hwy/highway.h"
```

#### 原因 2：静态初始化错误（最常见！）

```cpp
// ❌ 错误：命名空间作用域的静态向量
namespace HWY_NAMESPACE {
  static const auto kConstant = Set(ScalableTag<float>(), 1.0f);
  // ^ 在目标选择前初始化 → SIGILL
}

// ✅ 正确：函数局部或延迟初始化
namespace HWY_NAMESPACE {
  HWY_INLINE auto GetConstant() {
    const ScalableTag<float> d;
    return Set(d, 1.0f);
  }
}
```

#### 原因 3：混合不同目标的代码

```cpp
// ❌ 错误
#include "hwy/highway.h"

Vec256<float> v1 = CreateVectorAVX2();  // AVX2
ProcessVectorSSE2(v1);                   // SSE2 → 类型不匹配 → SIGILL
```

---

### ❌ 错误 10: Segmentation Fault (SIGSEGV)

```
Segmentation fault (core dumped)
```

**常见原因和解决方案：**

#### 原因 1：未对齐的数据与 Load()

```cpp
// ❌ 错误
float* data = new float[100];  // 可能只对齐到 8/16 字节
auto v = Load(d, data);        // 需要 HWY_ALIGNMENT (128字节)

// ✅ 解决方案 1：使用 LoadU
auto v = LoadU(d, data);

// ✅ 解决方案 2：对齐分配
AlignedUniquePtr<float[]> data = AllocateAligned<float>(100);
auto v = Load(d, data.get());
```

#### 原因 2：越界访问

```cpp
// ❌ 错误：count 不是 N 的倍数
for (size_t i = 0; i < count; i += N) {
  auto v = Load(d, &data[i]);  // 最后一次越界！
}

// ✅ 正确：检查边界
for (size_t i = 0; i + N <= count; i += N) {
  auto v = Load(d, &data[i]);
}
```

---

### ❌ 错误 11: AddressSanitizer 报错

```
ERROR: AddressSanitizer: heap-buffer-overflow
READ of size 32 at ...
```

**解决方案：** 参见错误 10 的解决方案（越界访问）。

**调试技巧：**

```bash
# 启用 ASan
cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=address -g"
make
./my_program

# ASan 会精确指出错误位置
```

---

### ❌ 错误 12: MemorySanitizer 报未初始化读取

```
WARNING: MemorySanitizer: use-of-uninitialized-value
```

**原因：** 使用未初始化的向量。

**解决方案：**

```cpp
// ❌ 错误
Vec<D> v;  // 未初始化
Store(v, d, dest);

// ✅ 正确
auto v = Zero(d);  // 初始化为 0
// 或
auto v = Load(d, src);  // 从数据加载
```

---

## 性能问题

### ❌ 问题 13: SIMD 代码没有比标量快

**常见原因：**

#### 原因 1：Debug 模式

```bash
# ❌ Debug 模式 → 慢 10-100x
cmake .. -DCMAKE_BUILD_TYPE=Debug

# ✅ Release 模式
cmake .. -DCMAKE_BUILD_TYPE=Release
```

#### 原因 2：数据太小

```cpp
// ❌ 向量化 10 个元素 → 开销 > 收益
void ProcessTiny(float* data) {
  for (size_t i = 0; i < 10; i += Lanes(d)) { /* ... */ }
}

// ✅ 对小数据使用标量
if (count < 64) {
  // 标量代码
} else {
  // 向量化代码
}
```

#### 原因 3：内存带宽瓶颈

```cpp
// ❌ 几乎没有计算
for (size_t i = 0; i < count; i += N) {
  auto v = LoadU(d, &in[i]);
  StoreU(v, d, &out[i]);  // 只是拷贝
}

// ✅ 增加计算密度
for (size_t i = 0; i < count; i += N) {
  auto v = LoadU(d, &in[i]);
  v = Sqrt(Mul(v, v));  // 更多计算
  StoreU(v, d, &out[i]);
}
```

#### 原因 4：频繁的水平操作

```cpp
// ❌ 每次迭代都归约
float sum = 0.0f;
for (size_t i = 0; i < count; i += N) {
  sum += ReduceSum(d, LoadU(d, &data[i]));  // 慢！
}

// ✅ 最后归约
auto vsum = Zero(d);
for (size_t i = 0; i < count; i += N) {
  vsum = Add(vsum, LoadU(d, &data[i]));
}
float sum = ReduceSum(d, vsum);  // 只归约一次
```

---

### ❌ 问题 14: 性能不稳定（波动大）

**原因：** 频率调节、热量调节、Turbo Boost。

**解决方案：**

```bash
# Linux: 禁用频率调节
sudo cpupower frequency-set --governor performance

# 固定 CPU 频率
sudo cpupower frequency-set -d 3.0GHz -u 3.0GHz

# 禁用 Turbo Boost
echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo

# 绑定到特定 CPU
taskset -c 0 ./my_benchmark
```

---

## 逻辑错误

### ❌ 错误 15: 结果与标量版本不一致

**常见原因：**

#### 原因 1：浮点精度差异

```cpp
// 标量：(a + b) + c
float scalar = (a + b) + c;

// SIMD：可能是 ((a + b) + (c + d)) / 2（不同的关联性）
auto vec = ReduceSum(d, Add(Add(va, vb), vc));

// 解决：接受小误差或使用双精度
if (fabs(scalar - vec) < 1e-5) { /* OK */ }
```

#### 原因 2：未处理余数

```cpp
// ❌ 忘记余数
for (size_t i = 0; i < count; i += N) {
  // 处理 [0, count - count%N)
}
// 忘记处理 [count - count%N, count)

// ✅ 添加余数处理
for (size_t i = 0; i < count; i += N) { /* ... */ }
for (; i < count; ++i) { /* 标量 */ }
```

#### 原因 3：掩码操作错误

```cpp
// ❌ 忘记初始化未掩码的通道
auto v = MaskedLoad(mask, d, ptr);  // 未掩码通道是未定义的
auto result = Add(v, other);        // 未定义 + 已定义 = 错误

// ✅ 使用 MaskedLoadOr 初始化
auto v = MaskedLoadOr(Zero(d), mask, d, ptr);
```

---

### ❌ 错误 16: 在可扩展向量上假设固定大小

```cpp
// ❌ 假设 AVX2 的 8 个 float
for (size_t i = 0; i < count; i += 8) {  // RVV 上崩溃！
  auto v = Load(d, &data[i]);
}

// ✅ 使用 Lanes(d)
const size_t N = Lanes(d);
for (size_t i = 0; i < count; i += N) {
  auto v = Load(d, &data[i]);
}
```

---

### ❌ 错误 17: 有符号/无符号混淆

```cpp
// ❌ 错误：有符号与无符号比较
const ScalableTag<int32_t> di;
auto a = Set(di, -1);
auto b = Set(di, 1);

// 如果意外使用无符号比较：
const RebindToUnsigned<decltype(di)> du;
auto wrong = Gt(BitCast(du, a), BitCast(du, b));
// 0xFFFFFFFF > 0x00000001 在无符号中为真！

// ✅ 正确：明确类型
auto correct = Gt(a, b);  // 有符号：-1 < 1
```

---

## 🔧 调试工具箱

### 快速诊断清单

```bash
# 1. 检查编译选项
cmake .. -DCMAKE_VERBOSE_MAKEFILE=ON

# 2. 检查目标检测
ldd ./my_program  # 检查链接的库
./my_program 2>&1 | grep -i target  # 查看运行时目标

# 3. 使用 Sanitizers
cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"

# 4. 查看汇编
g++ -S -masm=intel -O3 file.cc
objdump -d my_program | grep -A 20 MyFunc

# 5. 性能分析
perf stat ./my_program
perf record -g ./my_program && perf report

# 6. GDB 调试
gdb ./my_program
(gdb) catch throw
(gdb) run
(gdb) bt  # 回溯
```

---

## 📚 更多帮助

如果上述方法都不能解决问题：

1. **查阅 FAQ：** [FAQ.md](FAQ.md)
2. **搜索 GitHub Issues：** https://github.com/google/highway/issues
3. **提问 Discussions：** https://github.com/google/highway/discussions
4. **Stack Overflow：** 标签 `highway-simd`

---

## 💡 预防建议

### 开发最佳实践

1. **✅ 始终从 QUICKSTART 开始**
   - 先确保基本示例能运行

2. **✅ 使用 Release 模式测试性能**
   - Debug 模式性能不准

3. **✅ 添加单元测试**
   - 验证标量和 SIMD 结果一致

4. **✅ 使用 Sanitizers 开发**
   - 尽早发现内存错误

5. **✅ 定期检查汇编**
   - 确保向量化成功

6. **✅ 小步迭代**
   - 不要一次性写大量 SIMD 代码

7. **✅ 查阅文档**
   - 不确定时查 [快速参考](快速参考.md)

---

*最后更新：2026-01-19 | Highway v1.3.0*
