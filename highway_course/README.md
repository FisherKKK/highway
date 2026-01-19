# Highway SIMD 深度学习课程

## 📚 课程简介

欢迎来到 Highway SIMD 库的完整深度学习课程！本课程包含两个系列：

1. **14 天基础课程** - 循序渐进，从零基础到精通
2. **深度剖析系列** - 针对核心主题的深入技术文档

**课程目标：**
- 🎯 深入理解 SIMD（Single Instruction Multiple Data）编程原理
- 🎯 完全掌握 Highway 库的核心架构和设计模式
- 🎯 学习如何编写可移植的高性能向量化代码
- 🎯 精通动态/静态分派机制的底层实现
- 🎯 能够阅读、优化并贡献 Highway 源代码

**适合人群：**
- 有 C++17 基础的开发者
- 对高性能计算感兴趣的工程师
- 想要深入理解编译器和 CPU 指令集的学习者
- 需要优化计算密集型应用的开发者
- 希望为开源 SIMD 库做贡献的程序员

**前置知识：**
- C++17 基础（模板、命名空间、RAII、lambda）
- 基本的计算机体系结构知识
- 了解 CPU 缓存和内存层次结构（加分项）
- Git 基本操作（可选）

## 📖 课程大纲

### 第一周：基础篇（Day 1-7）

**Day 1: Highway 概述与 SIMD 基础**
- SIMD 编程基础概念
- Highway 项目架构总览
- 环境搭建与第一个程序
- 27 个支持的目标平台介绍

**Day 2: 类型系统与 Tag 机制**
- Highway 的核心设计：Tag-based Dispatch
- ScalableTag、CappedTag、FixedTag 详解
- 类型推导：TFromD、VFromD、MFromD
- Vec1 和 Mask1 实现剖析

**Day 3: 基本操作：加载、存储、算术运算**
- Load/Store 操作及其变体
- 算术运算：Add、Sub、Mul、Div
- 逻辑运算：And、Or、Xor、Not
- 比较运算与掩码生成

**Day 4: 标量实现深入（scalar-inl.h）**
- scalar-inl.h 完整源码分析
- 单元素向量的设计哲学
- 为什么需要 SCALAR 目标
- 操作符重载的实现技巧

**Day 5: x86 SSE2 实现剖析**
- x86_128-inl.h 架构分析
- SSE2 intrinsics 映射关系
- 向量类型定义：__m128i、__m128、__m128d
- 核心操作的汇编代码对照

**Day 6: ARM NEON 实现对比**
- arm_neon-inl.h 架构分析
- NEON intrinsics 与 SSE2 的差异
- AES 和 Crypto 扩展处理
- 跨平台抽象的设计智慧

**Day 7: 实战练习：实现简单向量算法**
- 向量求和算法实现
- 数组元素查找（Find）
- 条件过滤（Filter）
- 性能测试与分析

### 第二周：高级篇（Day 8-14）

**Day 8: 动态分派机制详解**
- foreach_target.h 工作原理
- 多目标编译的魔法：HWY_TARGET_TOGGLE
- targets.h 与 CPU 特性检测
- 函数指针表的构建与调用

**Day 9: 静态分派与编译时优化**
- HWY_STATIC_DISPATCH 实现机制
- 零开销抽象的证明
- 编译器优化技巧
- 何时选择静态 vs 动态分派

**Day 10: 高级算法：Transform 系列**
- hwy/contrib/algo/transform-inl.h 详解
- Generate、Transform、Transform1/2 实现
- 循环向量化的最佳实践
- 处理数组余项（remainder）的策略

**Day 11: 向量化排序：VQSort 深度剖析**
- VQSort 算法原理
- 排序网络（Sorting Networks）
- 分区（Partition）策略
- 性能分析：为什么比 std::sort 快 5-10 倍

**Day 12: 数学函数库实现**
- hwy/contrib/math/math-inl.h 概览
- 超越函数的多项式近似
- Sin/Cos/Exp/Log 实现细节
- SIMD 友好的算法设计

**Day 13: 测试框架与最佳实践**
- test_util-inl.h 工具函数
- Google Test 集成方式
- ForAllTypes 和 ForPartialVectors 模式
- 如何为新操作编写测试

**Day 14: 性能优化与实战项目**
- 性能分析工具和方法
- 常见性能陷阱
- Cache-friendly 编程
- 综合项目：图像处理滤波器实现

---

## 🎓 深度剖析系列（推荐！）

针对核心主题的深入技术文档，适合已有基础或想要快速掌握关键概念的学习者：

### **[第一部分：架构深度剖析](01_ARCHITECTURE_DEEP_DIVE.md)** ⭐ 核心必读

深入解析 Highway 的底层架构和实现细节：

- **动态调度机制**完整解析（重新包含机制、切换保护、目标选择）
- **向量类型系统**详解（`Simd<T,N,kPow2>`、分数编码、标签别名）
- **平台特定实现**对比（x86/ARM/RISC-V 的 Vec 包装器）
- **性能优化技术**（缓存控制、预取、对齐、FMA）
- 包含实战练习和真实代码示例

**推荐阅读时长：** 3-4 小时

### **[第二部分：高级主题与性能优化](02_ADVANCED_TOPICS.md)** ⭐ 进阶必读

掌握高级功能和真实世界的优化技术：

- **Contrib 模块**深度剖析（Transform、Find、VQSort、数学函数）
- **可扩展向量架构**（RISC-V RVV、ARM SVE 的深入理解）
- **掩码和混合操作**（Compress、掩码存储）
- **循环向量化策略**（4 种策略的优缺点对比）
- **性能优化模式**（循环展开、SoA vs AoS、智能预取）
- **真实世界案例研究**（图像处理、GEMM、N-body 模拟）

**推荐阅读时长：** 4-5 小时

### **[第三部分：实战练习与调试](03_HANDS_ON_EXERCISES.md)** 🛠️ 实践必备

通过动手实验巩固理解，掌握调试和分析技能：

- **3 个完整的动手实验**（向量加法、自定义归约、字符串搜索）
- **调试技术大全**（打印向量、断言、Sanitizers）
- **性能分析工具**（Google Benchmark、perf、VTune、汇编检查）
- **常见陷阱与解决方案**（5 个高频错误及修复方法）
- **真实项目构建**（图像卷积库、JSON 解析器、ML 推理）
- **贡献到 Highway**（开发环境、代码风格、PR 流程）

**推荐阅读时长：** 6-8 小时（含实践）

---

## 🛠 学习资源

### 📚 课程配套材料（必读！）

#### 基础课程材料
- **[学习指南](学习指南.md)** - 详细的每日学习计划、核心概念速查表、学习技巧
- **[快速参考](快速参考.md)** - Highway API 完整速查手册，编程时随时查阅
- **[实战练习](实战练习.md)** - 14天配套练习题，从基础到高级，含综合项目

#### 深度技术文档
- **[架构设计深度剖析](架构设计深度剖析.md)** - 系统架构和设计模式分析
- **[性能优化深度指南](性能优化深度指南.md)** - 全面的性能优化技巧
- **[源码阅读指南](源码阅读指南.md)** - 源代码导读和分析

#### 参考手册与工具指南
- **[FAQ - 常见问题解答](FAQ.md)** ⭐ 推荐！- 20+ 个高频问题的详细解答
- **[平台对比与基准测试](PLATFORM_COMPARISON.md)** - 27+ 平台特性对比表、性能数据、基准测试指南
- **[快速开始指南](QUICKSTART.md)** 🚀 - 5 分钟上手，第一个 Highway 程序
- **[学习完成清单](LEARNING_CHECKLIST.md)** 📋 - 自我评估、认证等级、学习路线图

### 代码仓库

- **Highway GitHub**: https://github.com/google/highway
- **本地路径**: `/home/dev/highway`

### 关键文件索引

- **核心头文件**: `hwy/highway.h`, `hwy/base.h`
- **平台实现**: `hwy/ops/*-inl.h`
- **示例代码**: `hwy/examples/skeleton.*`
- **算法库**: `hwy/contrib/`
- **测试用例**: `hwy/tests/`

### 官方文档

- `README.md` - 快速入门
- `g3doc/quick_reference.md` - API 速查
- `g3doc/design_philosophy.md` - 设计哲学
- `g3doc/impl_details.md` - 实现细节

## 📝 学习建议

1. **动手实践**：每天的课程都包含练习，请务必亲自编写代码
2. **阅读源码**：课程会引导你阅读具体的源代码行，建议配合 IDE 查看
3. **做笔记**：记录不理解的地方，在后续课程中寻找答案
4. **性能测试**：使用 perf、vtune 等工具验证性能提升
5. **循序渐进**：不要跳过章节，概念是逐步建立的

## 🎯 预期成果

完成本课程后，你将能够：
- ✅ 独立编写高性能的 Highway SIMD 代码
- ✅ 理解 Highway 如何在 27 个平台上保持一致的 API
- ✅ 阅读和调试 Highway 库的源代码
- ✅ 为 Highway 贡献代码或修复 bug
- ✅ 将现有的标量代码向量化，获得 2-10 倍性能提升
- ✅ 设计自己的跨平台 SIMD 算法库

## 📂 课程文件结构

```
highway_course/
├── README.md                                      # 本文件 - 课程总览
├── 学习指南.md                                     # 学习指南 - 每日计划、速查表、学习技巧
├── 快速参考.md                                     # API 速查手册
├── 实战练习.md                                     # 配套练习题集
│
├── Day01-项目概览和基本概念.md                     # 第1天课程
├── Day02-核心基础-base.h和工具函数.md              # 第2天课程
├── Day03-向量标签和类型系统.md                     # 第3天课程
├── Day04-基本向量操作.md                          # 第4天课程
├── Day05-高级向量操作.md                          # 第5天课程
├── Day06-静态分发机制.md                          # 第6天课程
├── Day07-动态分发机制.md                          # 第7天课程
├── Day08-平台特定实现-x86架构.md                  # 第8天课程
├── Day09-平台特定实现-ARM和RISC-V.md              # 第9天课程
├── Day10-内存管理和对齐.md                        # 第10天课程
├── Day11-循环向量化和变换.md                      # 第11天课程
├── Day12-Contrib模块-算法和排序.md                # 第12天课程
├── Day13-Contrib模块-数学函数和其他工具.md        # 第13天课程
└── Day14-测试框架和最佳实践.md                    # 第14天课程
```

## 📚 推荐学习路径

根据你的背景和学习目标，选择合适的路径：

### 🚀 快速路径（3-5 天）- 适合有经验的开发者

如果你已经熟悉 SIMD 编程或有其他 SIMD 库经验：

1. **第 1 天：** 阅读 [01_ARCHITECTURE_DEEP_DIVE.md](01_ARCHITECTURE_DEEP_DIVE.md) - 核心架构（3-4小时）
2. **第 2 天：** 阅读 [02_ADVANCED_TOPICS.md](02_ADVANCED_TOPICS.md) - 高级主题（4-5小时）
3. **第 3-4 天：** 完成 [03_HANDS_ON_EXERCISES.md](03_HANDS_ON_EXERCISES.md) 的所有实验（6-8小时）
4. **第 5 天：** 构建一个真实项目（见第三部分）

**总时长：** 20-25 小时

### 🎓 系统路径（2-3 周）- 适合零基础学习者

如果你是 SIMD 编程新手，推荐完整的 14 天课程：

**第 1 周（Day 1-7）：**
- 每天 1 课 + 配套练习（2-3小时/天）
- 重点：理解基础概念、平台差异、简单算法

**第 2 周（Day 8-14）：**
- 每天 1 课 + 配套练习（2-3小时/天）
- 重点：动态调度、高级算法、性能优化

**第 3 周：**
- 精读深度剖析系列（01-03）
- 完成综合项目
- 阅读 Highway 源代码

**总时长：** 35-45 小时

### 🔥 实战路径（1 周）- 适合有明确项目需求的开发者

如果你需要快速将 Highway 应用到项目中：

1. **Day 1：** 快速入门 - Day01 + [快速参考](快速参考.md)
2. **Day 2：** 核心机制 - [01_ARCHITECTURE_DEEP_DIVE.md](01_ARCHITECTURE_DEEP_DIVE.md) 的"动态调度"部分
3. **Day 3：** 循环向量化 - [02_ADVANCED_TOPICS.md](02_ADVANCED_TOPICS.md) 的"循环向量化策略"
4. **Day 4-5：** 相关案例研究（图像/GEMM/科学计算）+ 实验
5. **Day 6-7：** 实现你的项目 + 性能调优

**总时长：** 15-20 小时

### 💡 主题路径 - 按需学习特定主题

**只想理解架构设计？**
→ [01_ARCHITECTURE_DEEP_DIVE.md](01_ARCHITECTURE_DEEP_DIVE.md) + Day07

**只想学性能优化？**
→ [02_ADVANCED_TOPICS.md](02_ADVANCED_TOPICS.md) + [性能优化深度指南](性能优化深度指南.md)

**只想学调试技巧？**
→ [03_HANDS_ON_EXERCISES.md](03_HANDS_ON_EXERCISES.md) 的"调试技术"和"性能分析"章节

**只想实现具体算法？**
→ [02_ADVANCED_TOPICS.md](02_ADVANCED_TOPICS.md) 的 Contrib 模块 + 案例研究

---

## ⏰ 学习时间规划

- **每天学习时间**：2-3 小时
- **理论学习**：1-1.5 小时（阅读课程材料和源码）
- **实践编程**：1-1.5 小时（完成练习）
- **总学时**：28-42 小时

## 🚀 开始学习

### 🎯 最快开始（5 分钟）

**立即上手：** [快速开始指南 (QUICKSTART.md)](QUICKSTART.md)

在 5 分钟内编写并运行你的第一个 Highway 程序，看到实际的性能提升！

---

选择你的学习路径：

### 🎯 快速路径（推荐给有经验的开发者）

**直接开始深度剖析系列：**

1. **快速入门** → [QUICKSTART.md](QUICKSTART.md) - 5 分钟上手（可选）
2. **核心架构** → [01_ARCHITECTURE_DEEP_DIVE.md](01_ARCHITECTURE_DEEP_DIVE.md)
3. **高级优化** → [02_ADVANCED_TOPICS.md](02_ADVANCED_TOPICS.md)
4. **实战练习** → [03_HANDS_ON_EXERCISES.md](03_HANDS_ON_EXERCISES.md)
5. **随时查阅** → [快速参考](快速参考.md) + [FAQ](FAQ.md)

### 📚 系统路径（推荐给零基础学习者）

**从 14 天基础课程开始：**

1. **学习路线** → [学习指南](学习指南.md) - 了解学习路线图和核心概念
2. **第一天课程** → [Day 01: 项目概览和基本概念](Day01-项目概览和基本概念.md)
3. **API 速查** → [快速参考](快速参考.md) - 编程时随时查阅
4. **配套练习** → [实战练习](实战练习.md)
5. **深入理解** → 完成 Day 1-14 后，阅读深度剖析系列（01-03）

### 💡 主题路径（按需学习）

根据你的具体需求，直接跳到相关章节（见上文"推荐学习路径"）

---

## ✅ 学习成果检查清单

完成课程后，你应该能够：

- [ ] **基础能力**
  - [ ] 解释 SIMD 编程的基本原理和优势
  - [ ] 理解 Highway 的零开销抽象设计
  - [ ] 使用 `ScalableTag`、`CappedTag`、`FixedTag` 编写代码
  - [ ] 实现基本的向量化算法（求和、查找、过滤）

- [ ] **核心理解**
  - [ ] 完全理解动态调度机制（`HWY_TARGET_TOGGLE`、`foreach_target.h`）
  - [ ] 解释 `Simd<T,N,kPow2>` 的参数含义和分数编码
  - [ ] 区分 `Load` vs `LoadU`、何时使用对齐加载
  - [ ] 正确处理循环余数（掩码/重叠/填充策略）

- [ ] **高级技能**
  - [ ] 使用 `MulAdd` 优化数学计算
  - [ ] 实现循环展开和双累加器优化
  - [ ] 使用预取（Prefetch）优化大数组处理
  - [ ] 理解 SoA vs AoS 布局对性能的影响
  - [ ] 读懂并修改 `vqsort-inl.h`、`math-inl.h` 等复杂源码

- [ ] **实战能力**
  - [ ] 调试 SIMD 代码（打印向量、使用 Sanitizers）
  - [ ] 使用 perf、VTune、Compiler Explorer 分析性能
  - [ ] 实现一个真实项目（图像处理/JSON 解析/ML 推理）
  - [ ] 为 Highway 贡献代码或提交高质量 issue

- [ ] **平台知识**
  - [ ] 理解 x86、ARM、RISC-V 的 SIMD 指令差异
  - [ ] 解释 RVV 的 LMUL 和 SVE 的可扩展向量长度
  - [ ] 针对特定平台优化代码（`#if HWY_TARGET == ...`）

---

💡 **提示**:
- 📑 查看 **[完整资源索引 (INDEX.md)](INDEX.md)** - 所有文档的详细分类和查找指南
- 🔖 将 [快速参考](快速参考.md) 添加到浏览器书签，编程时随时查阅！
- 💬 加入 Highway GitHub Discussions 与社区交流：https://github.com/google/highway/discussions
- 💡 阅读真实项目中的 Highway 应用案例获得灵感

---

*课程版本：v2.0 | 基于 Highway v1.3.0 | 创建日期：2026-01-19 | 深度剖析系列已添加*
