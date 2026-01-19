# Highway SIMD 课程资源总览

本文档提供课程所有学习资源的完整索引，帮助你快速找到需要的内容。

## 📊 资源统计

- **总文档数：** 36 个
- **总大小：** ~650 KB
- **预计学习时间：** 50-80 小时（完整课程）
- **最后更新：** 2026-01-19

---

## 🎯 入门必读（按推荐顺序）

### 1. 快速开始（5-10 分钟）

| 文件 | 大小 | 说明 |
|------|------|------|
| [QUICKSTART.md](QUICKSTART.md) | 8.3K | 🚀 5分钟上手，第一个程序 |
| [快速参考.md](快速参考.md) | 15K | 📖 API速查手册（编程时随时查阅）|

### 2. 核心理解（3-5 天）

| 文件 | 大小 | 说明 |
|------|------|------|
| [01_ARCHITECTURE_DEEP_DIVE.md](01_ARCHITECTURE_DEEP_DIVE.md) | 23K | ⭐ 架构深度剖析（必读）|
| [02_ADVANCED_TOPICS.md](02_ADVANCED_TOPICS.md) | 29K | ⭐ 高级主题与优化（必读）|
| [03_HANDS_ON_EXERCISES.md](03_HANDS_ON_EXERCISES.md) | 23K | 🛠️ 实战练习与调试（必做）|

### 3. 问题解答（随时查阅）

| 文件 | 大小 | 说明 |
|------|------|------|
| [FAQ.md](FAQ.md) | 23K | ❓ 20+ 常见问题详解 |
| [COMMON_ERRORS.md](COMMON_ERRORS.md) | 21K | 🐛 17个常见错误及修复 |
| [TROUBLESHOOTING_GUIDE.md](TROUBLESHOOTING_GUIDE.md) | 26K | 🔧 结构化调试工作流 |
| [PLATFORM_COMPARISON.md](PLATFORM_COMPARISON.md) | 18K | 📊 平台对比与基准测试 |

### 4. 快速参考（编程时查阅）

| 文件 | 大小 | 说明 |
|------|------|------|
| [CHEATSHEET.md](CHEATSHEET.md) | 11K | 📄 一页纸速查卡片（可打印）|
| [快速参考.md](快速参考.md) | 15K | 📖 API速查手册 |
| [GLOSSARY.md](GLOSSARY.md) | 24K | 📚 100+ 术语词典 |

### 5. 实践资源（项目与学习）

| 文件 | 大小 | 说明 |
|------|------|------|
| [PROJECT_IDEAS.md](PROJECT_IDEAS.md) | 23K | 💡 24个实战项目想法 |
| [RESOURCES.md](RESOURCES.md) | 23K | 🔗 100+ 学习资源汇总 |

### 6. 学习规划（可选）

| 文件 | 大小 | 说明 |
|------|------|------|
| [README.md](README.md) | 16K | 📚 课程总览和路径选择 |
| [学习指南.md](学习指南.md) | 15K | 📅 每日学习计划和技巧 |
| [LEARNING_CHECKLIST.md](LEARNING_CHECKLIST.md) | 14K | ✅ 自我评估与认证体系 |

---

## 📚 14 天系统课程

### 第一周：基础篇（Day 1-7）

| Day | 文件 | 大小 | 核心主题 |
|-----|------|------|---------|
| 1 | [Day01-项目概览和基本概念.md](Day01-项目概览和基本概念.md) | 8.0K | SIMD基础、Highway架构 |
| 2 | [Day02-核心基础-base.h和工具函数.md](Day02-核心基础-base.h和工具函数.md) | 12K | 编译器检测、类型工具 |
| 3 | [Day03-向量标签和类型系统.md](Day03-向量标签和类型系统.md) | 12K | Tag系统、Simd模板 |
| 4 | [Day04-基本向量操作.md](Day04-基本向量操作.md) | 11K | Load/Store、算术运算 |
| 5 | [Day05-高级向量操作.md](Day05-高级向量操作.md) | 14K | 掩码、Shuffle、归约 |
| 6 | [Day06-静态分发机制.md](Day06-静态分发机制.md) | 11K | HWY_STATIC_DISPATCH |
| 7 | [Day07-动态分发机制.md](Day07-动态分发机制.md) | 16K | HWY_DYNAMIC_DISPATCH |

**第一周总计：** 84K | **预计学习时间：** 14-21 小时

### 第二周：高级篇（Day 8-14）

| Day | 文件 | 大小 | 核心主题 |
|-----|------|------|---------|
| 8 | [Day08-平台特定实现-x86架构.md](Day08-平台特定实现-x86架构.md) | 16K | SSE/AVX/AVX-512实现 |
| 9 | [Day09-平台特定实现-ARM和RISC-V.md](Day09-平台特定实现-ARM和RISC-V.md) | 18K | NEON/SVE/RVV实现 |
| 10 | [Day10-内存管理和对齐.md](Day10-内存管理和对齐.md) | 15K | 对齐、缓存控制 |
| 11 | [Day11-循环向量化和变换.md](Day11-循环向量化和变换.md) | 17K | Transform、余数处理 |
| 12 | [Day12-Contrib模块-算法和排序.md](Day12-Contrib模块-算法和排序.md) | 15K | VQSort、Find、Copy |
| 13 | [Day13-Contrib模块-数学函数和其他工具.md](Day13-Contrib模块-数学函数和其他工具.md) | 22K | 数学库、图像处理 |
| 14 | [Day14-测试框架和最佳实践.md](Day14-测试框架和最佳实践.md) | 22K | 测试、性能分析 |

**第二周总计：** 125K | **预计学习时间：** 21-28 小时

**14天课程总计：** 209K | **预计学习时间：** 35-49 小时

---

## 🔬 深度技术文档

### 架构与设计

| 文件 | 大小 | 说明 | 难度 |
|------|------|------|------|
| [架构设计深度剖析.md](架构设计深度剖析.md) | 29K | 系统架构、设计模式分析 | 🌟🌟🌟 高级 |
| [01_ARCHITECTURE_DEEP_DIVE.md](01_ARCHITECTURE_DEEP_DIVE.md) | 23K | 动态调度、向量类型系统（英文版）| 🌟🌟 中级 |

### 性能优化

| 文件 | 大小 | 说明 | 难度 |
|------|------|------|------|
| [性能优化深度指南.md](性能优化深度指南.md) | 28K | 全面的优化技术和案例 | 🌟🌟🌟 高级 |
| [02_ADVANCED_TOPICS.md](02_ADVANCED_TOPICS.md) | 29K | Contrib模块、优化模式（英文版）| 🌟🌟🌟 高级 |
| [PLATFORM_COMPARISON.md](PLATFORM_COMPARISON.md) | 18K | 平台对比、基准测试指南 | 🌟🌟 中级 |

### 源码阅读

| 文件 | 大小 | 说明 | 难度 |
|------|------|------|------|
| [源码阅读指南.md](源码阅读指南.md) | 24K | 源代码结构、阅读技巧 | 🌟🌟🌟 高级 |
| [03_HANDS_ON_EXERCISES.md](03_HANDS_ON_EXERCISES.md) | 23K | 实战项目、调试技巧（英文版）| 🌟🌟 中级 |

---

## 📖 参考手册

### 使用手册

| 文件 | 大小 | 用途 | 推荐查阅频率 |
|------|------|------|-------------|
| [快速参考.md](快速参考.md) | 15K | API完整速查 | ⭐⭐⭐⭐⭐ 每天 |
| [CHEATSHEET.md](CHEATSHEET.md) | 11K | 一页纸速查 | ⭐⭐⭐⭐⭐ 编程时 |
| [GLOSSARY.md](GLOSSARY.md) | 24K | 术语词典 | ⭐⭐⭐⭐ 遇到术语时 |
| [FAQ.md](FAQ.md) | 23K | 常见问题解答 | ⭐⭐⭐⭐ 遇到问题时 |
| [COMMON_ERRORS.md](COMMON_ERRORS.md) | 21K | 错误及修复 | ⭐⭐⭐⭐ 调试时 |
| [TROUBLESHOOTING_GUIDE.md](TROUBLESHOOTING_GUIDE.md) | 26K | 调试工作流 | ⭐⭐⭐⭐ 解决问题时 |
| [QUICKSTART.md](QUICKSTART.md) | 8.3K | 快速入门教程 | ⭐⭐⭐ 初次使用 |

### 实战指南

| 文件 | 大小 | 内容 | 适用场景 |
|------|------|------|---------|
| [实战练习.md](实战练习.md) | 16K | 14天配套练习题 | 每日练习 |
| [LEARNING_CHECKLIST.md](LEARNING_CHECKLIST.md) | 14K | 自我评估清单 | 定期检查 |

---

## 🗂️ 按主题分类

### 1. 基础概念（初学者）

- [QUICKSTART.md](QUICKSTART.md) - 快速开始
- [Day01-项目概览和基本概念.md](Day01-项目概览和基本概念.md)
- [Day02-核心基础-base.h和工具函数.md](Day02-核心基础-base.h和工具函数.md)
- [Day03-向量标签和类型系统.md](Day03-向量标签和类型系统.md)
- [快速参考.md](快速参考.md)

### 2. 向量操作（基础）

- [Day04-基本向量操作.md](Day04-基本向量操作.md)
- [Day05-高级向量操作.md](Day05-高级向量操作.md)
- [Day11-循环向量化和变换.md](Day11-循环向量化和变换.md)

### 3. 调度机制（核心）

- [Day06-静态分发机制.md](Day06-静态分发机制.md)
- [Day07-动态分发机制.md](Day07-动态分发机制.md)
- [01_ARCHITECTURE_DEEP_DIVE.md](01_ARCHITECTURE_DEEP_DIVE.md) ⭐ 深度解析

### 4. 平台实现（进阶）

- [Day08-平台特定实现-x86架构.md](Day08-平台特定实现-x86架构.md)
- [Day09-平台特定实现-ARM和RISC-V.md](Day09-平台特定实现-ARM和RISC-V.md)
- [PLATFORM_COMPARISON.md](PLATFORM_COMPARISON.md) - 全平台对比

### 5. 内存与性能（进阶）

- [Day10-内存管理和对齐.md](Day10-内存管理和对齐.md)
- [性能优化深度指南.md](性能优化深度指南.md) ⭐ 全面优化
- [02_ADVANCED_TOPICS.md](02_ADVANCED_TOPICS.md) - 优化模式

### 6. 高级算法（高级）

- [Day12-Contrib模块-算法和排序.md](Day12-Contrib模块-算法和排序.md)
- [Day13-Contrib模块-数学函数和其他工具.md](Day13-Contrib模块-数学函数和其他工具.md)
- [02_ADVANCED_TOPICS.md](02_ADVANCED_TOPICS.md) - Contrib深入

### 7. 测试与调试（实战）

- [Day14-测试框架和最佳实践.md](Day14-测试框架和最佳实践.md)
- [03_HANDS_ON_EXERCISES.md](03_HANDS_ON_EXERCISES.md) ⭐ 实战练习
- [FAQ.md](FAQ.md) - 调试问题专区

### 8. 架构与源码（专家）

- [架构设计深度剖析.md](架构设计深度剖析.md) ⭐ 系统设计
- [源码阅读指南.md](源码阅读指南.md) ⭐ 源码导读
- [01_ARCHITECTURE_DEEP_DIVE.md](01_ARCHITECTURE_DEEP_DIVE.md) - 核心机制

---

## 🎯 学习路径推荐

### 路径 1：快速路径（3-5 天，20-25 小时）

**适合：** 有 SIMD 经验的开发者

```
Day 1:
  ├─ QUICKSTART.md (5分钟)
  ├─ 01_ARCHITECTURE_DEEP_DIVE.md (3小时)
  └─ 快速参考.md (30分钟浏览)

Day 2:
  ├─ 02_ADVANCED_TOPICS.md (4小时)
  └─ PLATFORM_COMPARISON.md (1小时)

Day 3-4:
  ├─ 03_HANDS_ON_EXERCISES.md (6小时)
  └─ 实验 1-3 (2小时)

Day 5:
  ├─ FAQ.md (按需查阅)
  └─ 真实项目实践
```

### 路径 2：系统路径（2-3 周，35-45 小时）

**适合：** SIMD 新手

```
第1周（Day 1-7）：
  ├─ QUICKSTART.md
  ├─ Day01 → Day07 课程
  ├─ 快速参考.md（常看）
  └─ 实战练习.md（每日练习）

第2周（Day 8-14）：
  ├─ Day08 → Day14 课程
  ├─ FAQ.md（遇到问题查阅）
  └─ 实战练习.md（每日练习）

第3周：
  ├─ 01_ARCHITECTURE_DEEP_DIVE.md
  ├─ 02_ADVANCED_TOPICS.md
  ├─ 03_HANDS_ON_EXERCISES.md
  └─ 真实项目实践
```

### 路径 3：实战路径（1 周，15-20 小时）

**适合：** 有明确项目需求

```
Day 1:
  ├─ QUICKSTART.md
  ├─ Day01
  └─ 快速参考.md

Day 2:
  ├─ 01_ARCHITECTURE_DEEP_DIVE.md（动态调度部分）
  └─ Day07

Day 3:
  ├─ 02_ADVANCED_TOPICS.md（循环向量化策略）
  └─ Day11

Day 4-5:
  ├─ 相关案例研究（图像/GEMM/科学）
  └─ 03_HANDS_ON_EXERCISES.md（实验）

Day 6-7:
  ├─ 实现项目
  ├─ 性能调优（参考：性能优化深度指南.md）
  └─ FAQ.md（解决问题）
```

---

## 📐 文档尺寸对比

### 按大小排序（Top 10）

1. **02_ADVANCED_TOPICS.md** - 29K ⭐ 最全面
2. **架构设计深度剖析.md** - 29K ⭐ 最深入
3. **性能优化深度指南.md** - 28K ⭐ 最实用
4. **源码阅读指南.md** - 24K
5. **01_ARCHITECTURE_DEEP_DIVE.md** - 23K ⭐ 核心必读
6. **03_HANDS_ON_EXERCISES.md** - 23K ⭐ 实战必做
7. **FAQ.md** - 23K ⭐ 问题必查
8. **Day13-Contrib模块-数学函数和其他工具.md** - 22K
9. **Day14-测试框架和最佳实践.md** - 22K
10. **PLATFORM_COMPARISON.md** - 18K

### 按类型统计

| 类型 | 文档数 | 总大小 | 平均大小 |
|------|--------|--------|---------|
| 14天课程 | 14 | 209K | 15K |
| 深度剖析系列 | 3 | 75K | 25K |
| 参考手册 | 5 | 83K | 17K |
| 技术文档 | 3 | 81K | 27K |
| 其他 | 5 | 52K | 10K |
| **总计** | **30** | **~500K** | **17K** |

---

## 🔍 快速查找

### 我想学...

#### "如何开始？"
→ [QUICKSTART.md](QUICKSTART.md)

#### "动态调度如何工作？"
→ [01_ARCHITECTURE_DEEP_DIVE.md](01_ARCHITECTURE_DEEP_DIVE.md) - 动态调度部分
→ [Day07-动态分发机制.md](Day07-动态分发机制.md)

#### "如何优化性能？"
→ [性能优化深度指南.md](性能优化深度指南.md)
→ [02_ADVANCED_TOPICS.md](02_ADVANCED_TOPICS.md) - 性能优化模式

#### "不同平台有什么差异？"
→ [PLATFORM_COMPARISON.md](PLATFORM_COMPARISON.md)
→ [Day08](Day08-平台特定实现-x86架构.md) / [Day09](Day09-平台特定实现-ARM和RISC-V.md)

#### "如何调试 SIMD 代码？"
→ [03_HANDS_ON_EXERCISES.md](03_HANDS_ON_EXERCISES.md) - 调试技术部分
→ [FAQ.md](FAQ.md) - Q12-Q14

#### "有没有 API 速查？"
→ [快速参考.md](快速参考.md) ⭐ 最常用

#### "遇到错误怎么办？"
→ [FAQ.md](FAQ.md) - 按问题类型查找

#### "如何评估学习进度？"
→ [LEARNING_CHECKLIST.md](LEARNING_CHECKLIST.md)

---

## 📱 移动端友好

所有文档均为 Markdown 格式，在以下环境都能良好显示：

- ✅ **GitHub**（在线浏览，最佳体验）
- ✅ **VS Code**（本地编辑，推荐插件：Markdown All in One）
- ✅ **Typora / Mark Text**（所见即所得编辑器）
- ✅ **移动端**（GitHub Mobile App、Working Copy）

---

## 🔄 持续更新

本课程基于 **Highway v1.3.0**，将随 Highway 更新而更新。

**检查更新：**
```bash
cd /home/dev/highway
git pull
cd highway_course
# 查看最新更新日志
git log --oneline --all -- highway_course/
```

---

## 💡 使用技巧

1. **收藏常用文档：** 将 [快速参考.md](快速参考.md) 和 [FAQ.md](FAQ.md) 添加到浏览器书签
2. **打印学习清单：** [LEARNING_CHECKLIST.md](LEARNING_CHECKLIST.md) 可打印跟踪进度
3. **搜索关键词：** 使用 `grep -r "关键词" highway_course/` 快速查找
4. **对比学习：** 中文 Day 系列 vs 英文深度剖析系列，双语学习
5. **边学边练：** 每学一章，立即完成 [实战练习.md](实战练习.md) 中的对应练习

---

## 🎓 认证体系

根据 [LEARNING_CHECKLIST.md](LEARNING_CHECKLIST.md) 的评估标准：

- 🥉 **入门认证** - 完成 15 项基础检查点
- 🥈 **进阶认证** - 完成 40 项核心检查点
- 🥇 **高级认证** - 完成 75 项高级检查点
- 💎 **专家认证** - 完成全部 100 项检查点

---

## 📞 获取帮助

如果文档中没有找到答案：

1. **搜索文档：** 使用 Ctrl+F 或 grep
2. **查阅 FAQ：** [FAQ.md](FAQ.md) 涵盖 20+ 高频问题
3. **GitHub Discussions：** https://github.com/google/highway/discussions
4. **提交 Issue：** https://github.com/google/highway/issues

---

**祝学习愉快！** 🎉

记住：这 500KB 的知识，能让你的代码快 5-10 倍！

---

*最后更新：2026-01-19 | Highway v1.3.0 | 30 个文档*
