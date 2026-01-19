# Highway SIMD 学习资源汇总

精选的 SIMD 编程和 Highway 学习资源，包括官方文档、教程、工具、库和社区资源。

---

## 📖 官方文档

### Highway 官方

| 资源 | 链接 | 说明 |
|------|------|------|
| **GitHub 仓库** | https://github.com/google/highway | 官方源代码 |
| **README** | https://github.com/google/highway/blob/master/README.md | 快速入门 |
| **Quick Reference** | `g3doc/quick_reference.md` | API 完整参考 |
| **Design Philosophy** | `g3doc/design_philosophy.md` | 设计理念 |
| **Discussions** | https://github.com/google/highway/discussions | 社区讨论区 |
| **Issues** | https://github.com/google/highway/issues | Bug 报告和功能请求 |
| **Releases** | https://github.com/google/highway/releases | 版本发布历史 |

---

## 🎓 学习教程

### 本课程资源

所有文档位于 `/home/dev/highway/highway_course/`：

| 文档 | 用途 | 推荐度 |
|------|------|--------|
| [QUICKSTART.md](QUICKSTART.md) | 5分钟上手 | ⭐⭐⭐⭐⭐ |
| [CHEATSHEET.md](CHEATSHEET.md) | 速查卡片（可打印）| ⭐⭐⭐⭐⭐ |
| [01_ARCHITECTURE_DEEP_DIVE.md](01_ARCHITECTURE_DEEP_DIVE.md) | 架构深度剖析 | ⭐⭐⭐⭐⭐ |
| [02_ADVANCED_TOPICS.md](02_ADVANCED_TOPICS.md) | 高级主题与优化 | ⭐⭐⭐⭐⭐ |
| [03_HANDS_ON_EXERCISES.md](03_HANDS_ON_EXERCISES.md) | 实战练习 | ⭐⭐⭐⭐⭐ |
| [FAQ.md](FAQ.md) | 常见问题解答 | ⭐⭐⭐⭐⭐ |
| [COMMON_ERRORS.md](COMMON_ERRORS.md) | 错误解决方案 | ⭐⭐⭐⭐ |
| [PROJECT_IDEAS.md](PROJECT_IDEAS.md) | 项目灵感 | ⭐⭐⭐⭐ |
| [快速参考.md](快速参考.md) | API 中文速查 | ⭐⭐⭐⭐ |

### 外部教程

| 资源 | 链接 | 语言 | 说明 |
|------|------|------|------|
| **Highway Introduction** | [Google Research Blog](https://ai.googleblog.com/) | EN | 官方介绍（搜索 Highway）|
| **SIMD for C++ Developers** | [Modernes C++](https://www.modernescpp.com/) | EN | C++ SIMD 教程 |
| **Intel Intrinsics Guide** | https://www.intel.com/intrinsics | EN | x86 内在函数参考 |
| **ARM NEON Guide** | https://developer.arm.com/neon | EN | ARM NEON 编程指南 |

---

## 🛠️ 开发工具

### 编译器

| 工具 | 版本要求 | 下载 | 说明 |
|------|---------|------|------|
| **GCC** | 7+ | https://gcc.gnu.org/ | 推荐 11+ |
| **Clang** | 6+ | https://clang.llvm.org/ | 推荐 14+ |
| **MSVC** | 2019+ | https://visualstudio.microsoft.com/ | Windows |
| **Intel ICC** | 2021+ | https://www.intel.com/icc | 高性能 |

### 构建系统

| 工具 | 用途 | 链接 |
|------|------|------|
| **CMake** | 跨平台构建 | https://cmake.org/ |
| **Ninja** | 快速构建后端 | https://ninja-build.org/ |
| **Bazel** | Google 构建系统 | https://bazel.build/ |

### IDE 和编辑器

| 工具 | 平台 | Highway 支持 | 推荐插件 |
|------|------|-------------|---------|
| **VS Code** | All | ✅ 优秀 | C/C++, CMake Tools |
| **CLion** | All | ✅ 优秀 | 内置 CMake |
| **Visual Studio** | Windows | ✅ 良好 | - |
| **Vim/NeoVim** | All | ✅ 良好 | YouCompleteMe, coc.nvim |

### 调试工具

| 工具 | 用途 | 链接 |
|------|------|------|
| **GDB** | 源码调试 | https://www.gnu.org/gdb/ |
| **LLDB** | LLVM 调试器 | https://lldb.llvm.org/ |
| **AddressSanitizer** | 内存错误检测 | 编译器内置 |
| **Valgrind** | 内存分析 | https://valgrind.org/ |
| **rr** | 记录和重放 | https://rr-project.org/ |

### 性能分析工具

| 工具 | 平台 | 用途 | 链接 |
|------|------|------|------|
| **perf** | Linux | CPU 分析 | 内核工具 |
| **Intel VTune** | x86 | 微架构分析 | https://www.intel.com/vtune |
| **AMD uProf** | AMD | CPU/GPU 分析 | https://developer.amd.com/uprof |
| **Apple Instruments** | macOS | 系统分析 | Xcode 自带 |
| **Arm Streamline** | ARM | 嵌入式分析 | https://developer.arm.com/streamline |
| **Google Benchmark** | All | 微基准测试 | https://github.com/google/benchmark |

### 汇编查看器

| 工具 | 类型 | 链接 | 说明 |
|------|------|------|------|
| **Compiler Explorer** | 在线 | https://godbolt.org/ | ⭐ 最佳选择 |
| **objdump** | 命令行 | GNU Binutils | 反汇编工具 |
| **llvm-objdump** | 命令行 | LLVM | LLVM 版本 |
| **LLVM-MCA** | 命令行 | LLVM | 机器码分析器 |

---

## 📚 参考手册

### SIMD 指令集参考

| 架构 | 资源 | 链接 |
|------|------|------|
| **Intel x86** | Intrinsics Guide | https://software.intel.com/intrinsics |
| **Intel x86** | 优化手册 | https://www.intel.com/sdm |
| **AMD x86** | 优化手册 | https://developer.amd.com/resources/epyc-resources/ |
| **ARM NEON** | Intrinsics 参考 | https://developer.arm.com/neon |
| **ARM SVE** | 程序员指南 | https://developer.arm.com/sve |
| **RISC-V RVV** | 规范 | https://github.com/riscv/riscv-v-spec |
| **WebAssembly** | SIMD 提案 | https://github.com/WebAssembly/simd |

### 性能优化手册

| 作者/组织 | 资源 | 链接 |
|-----------|------|------|
| **Agner Fog** | 优化系列手册 | https://agner.org/optimize/ |
| **Intel** | 优化参考手册 | https://www.intel.com/optimization-manual |
| **Brendan Gregg** | 性能工具书 | http://www.brendangregg.com/perf.html |
| **Denis Bakhvalov** | 性能分析书籍 | https://github.com/dendibakh/perf-book |

---

## 🔗 相关项目与库

### SIMD 库

| 项目 | 语言 | 特点 | GitHub |
|------|------|------|--------|
| **Highway** | C++ | ⭐ 本课程主题 | https://github.com/google/highway |
| **xsimd** | C++ | Header-only | https://github.com/xtensor-stack/xsimd |
| **Vc** | C++ | 模板元编程 | https://github.com/VcDevel/Vc |
| **libsimdpp** | C++ | 跨平台抽象 | https://github.com/p12tic/libsimdpp |
| **simde** | C | 模拟 intrinsics | https://github.com/simd-everywhere/simde |
| **std::simd** | C++26 | 标准库 | 编译器支持中 |

### 使用 SIMD 的高性能库

| 项目 | 领域 | 说明 | GitHub |
|------|------|------|--------|
| **simdjson** | JSON 解析 | 极快的 JSON 解析器 | https://github.com/simdjson/simdjson |
| **Folly** | 通用库 | Facebook C++ 库 | https://github.com/facebook/folly |
| **Eigen** | 线性代数 | C++ 矩阵库 | https://eigen.tuxfamily.org/ |
| **libjpeg-turbo** | 图像 | JPEG 编解码 | https://github.com/libjpeg-turbo/libjpeg-turbo |
| **OpenCV** | 计算机视觉 | 使用 Highway 和其他 SIMD | https://opencv.org/ |
| **DuckDB** | 数据库 | 列式数据库 | https://github.com/duckdb/duckdb |
| **Sleef** | 数学 | SIMD 数学函数库 | https://sleef.org/ |
| **VQSort** | 排序 | Highway 的向量化排序 | Highway contrib |

---

## 📺 视频教程

### YouTube 频道

| 频道 | 内容 | 语言 | 推荐度 |
|------|------|------|--------|
| **CppCon** | C++ 会议录像 | EN | ⭐⭐⭐⭐⭐ |
| **Meeting C++** | C++ 会议 | EN | ⭐⭐⭐⭐ |
| **Two's Complement** | 底层编程 | EN | ⭐⭐⭐ |

### 推荐演讲

| 标题 | 演讲者 | 会议 | 链接 |
|------|--------|------|------|
| "Highway: Portable SIMD" | Jan Wassenberg | CppCon | 搜索 YouTube |
| "SIMD Algorithms" | Various | CppCon | 搜索 YouTube |
| "Performance Matters" | Emery Berger | StrangeLoop | 搜索 YouTube |

---

## 📝 博客与文章

### 优质博客

| 博客 | 作者 | 主题 | 链接 |
|------|------|------|------|
| **Easyperf** | Denis Bakhvalov | 性能优化 | https://easyperf.net/ |
| **Travisdowns** | Travis Downs | 微架构 | https://travisdowns.github.io/ |
| **Daniel Lemire** | Daniel Lemire | SIMD 算法 | https://lemire.me/blog/ |
| **Agner Fog** | Agner Fog | 优化技术 | https://agner.org/ |
| **Fedor Pikus** | Fedor Pikus | 高性能 C++ | CppCon 视频 |

### 精选文章

| 标题 | 链接 | 说明 |
|------|------|------|
| "SIMD < SIMT < SMT" | easyperf | 并行模型对比 |
| "Highway Introduction" | Google Blog | Highway 介绍 |
| "Branch-Free Code" | Various | 无分支优化 |
| "Data-Oriented Design" | Various | 面向数据设计 |

---

## 🎓 在线课程

| 平台 | 课程 | 讲师 | 链接 |
|------|------|------|------|
| **Coursera** | Computer Architecture | Princeton | coursera.org |
| **edX** | Performance Engineering | MIT | edx.org |
| **YouTube** | 免费教程 | Various | - |

---

## 📖 书籍推荐

### SIMD 和优化

| 书名 | 作者 | 出版年 | 难度 |
|------|------|--------|------|
| **Computer Systems: A Programmer's Perspective** | Bryant & O'Hallaron | 2015 | 🌟🌟 |
| **Performance Analysis and Tuning** | Bakhvalov | 2020 | 🌟🌟🌟 |
| **Optimizing C++** | Agner Fog | 在线 | 🌟🌟🌟 |
| **Data-Oriented Design** | Richard Fabian | 2018 | 🌟🌟 |

### C++ 编程

| 书名 | 作者 | 说明 |
|------|------|------|
| **Effective Modern C++** | Scott Meyers | C++11/14 最佳实践 |
| **C++ Concurrency in Action** | Anthony Williams | 并发编程 |
| **The C++ Programming Language** | Bjarne Stroustrup | C++ 之父著作 |

---

## 💬 社区与论坛

### 在线社区

| 平台 | 链接 | 说明 |
|------|------|------|
| **Highway Discussions** | https://github.com/google/highway/discussions | 官方讨论区 |
| **Stack Overflow** | https://stackoverflow.com/questions/tagged/highway-simd | Q&A |
| **Reddit r/cpp** | https://reddit.com/r/cpp | C++ 社区 |
| **CppLang Slack** | https://cpplang.slack.com/ | C++ Slack |
| **Discord C++** | 搜索 C++ Discord | 实时聊天 |

### 会议

| 会议 | 频率 | 地点 | 网站 |
|------|------|------|------|
| **CppCon** | 年度 | 美国 | https://cppcon.org/ |
| **Meeting C++** | 年度 | 欧洲 | https://meetingcpp.com/ |
| **C++ Now** | 年度 | 美国 | https://cppnow.org/ |
| **ACCU** | 年度 | 英国 | https://accu.org/ |

---

## 🔬 学术资源

### 论文数据库

| 资源 | 链接 | 说明 |
|------|------|------|
| **Google Scholar** | https://scholar.google.com/ | 搜索 SIMD, vectorization |
| **arXiv** | https://arxiv.org/ | 预印本 |
| **ACM Digital Library** | https://dl.acm.org/ | 计算机科学论文 |
| **IEEE Xplore** | https://ieeexplore.ieee.org/ | 工程论文 |

### 推荐会议

| 会议 | 领域 | 说明 |
|------|------|------|
| **ISCA** | 计算机架构 | 顶会 |
| **MICRO** | 微架构 | 顶会 |
| **ASPLOS** | 系统和架构 | 顶会 |
| **PPoPP** | 并行编程 | 重要会议 |
| **CGO** | 编译器优化 | 重要会议 |

---

## 🛡️ 最佳实践资源

### 代码风格

| 资源 | 链接 | 说明 |
|------|------|------|
| **Google C++ Style** | https://google.github.io/styleguide/cppguide.html | Google 风格 |
| **CppCoreGuidelines** | https://isocpp.github.io/CppCoreGuidelines/ | 官方指南 |
| **clang-format** | https://clang.llvm.org/docs/ClangFormat.html | 自动格式化 |

### 测试框架

| 框架 | 链接 | 说明 |
|------|------|------|
| **Google Test** | https://github.com/google/googletest | C++ 测试框架 |
| **Catch2** | https://github.com/catchorg/Catch2 | 现代 C++ 测试 |
| **doctest** | https://github.com/doctest/doctest | 轻量级测试 |

---

## 🔧 实用脚本和工具

### 本课程提供的工具

位于 `/home/dev/highway/highway_course/`：

- [CHEATSHEET.md](CHEATSHEET.md) - 速查卡片
- [COMMON_ERRORS.md](COMMON_ERRORS.md) - 错误解决指南
- [QUICKSTART.md](QUICKSTART.md) - 快速开始模板

### GitHub Gists（示例）

搜索关键词：`highway simd`, `simd performance`, `vectorization examples`

---

## 📊 基准测试资源

### 基准测试套件

| 项目 | 用途 | 链接 |
|------|------|------|
| **Google Benchmark** | 微基准 | https://github.com/google/benchmark |
| **nanobench** | C++ 基准 | https://github.com/martinus/nanobench |
| **Celero** | C++ 基准 | https://github.com/DigitalInBlue/Celero |

### 性能数据库

| 资源 | 链接 | 说明 |
|------|------|------|
| **uops.info** | https://uops.info/ | x86 指令延迟/吞吐量 |
| **InstLatx64** | http://instlatx64.atw.hu/ | x86 指令数据库 |
| **WikiChip** | https://en.wikichip.org/ | CPU 规格 |

---

## 🌟 推荐学习路径

### 对于初学者

1. **基础知识：**
   - 阅读 [QUICKSTART.md](QUICKSTART.md)
   - 学习 Intel Intrinsics Guide
   - 观看 CppCon SIMD 演讲

2. **深入学习：**
   - 完成本课程 14 天系列
   - 阅读 Agner Fog 优化手册
   - 实践 [PROJECT_IDEAS.md](PROJECT_IDEAS.md) 中的项目

3. **进阶：**
   - 研究 simdjson 源码
   - 参与 Highway 社区
   - 阅读相关学术论文

### 对于有经验的开发者

1. **快速掌握：**
   - [01_ARCHITECTURE_DEEP_DIVE.md](01_ARCHITECTURE_DEEP_DIVE.md)
   - [02_ADVANCED_TOPICS.md](02_ADVANCED_TOPICS.md)
   - [03_HANDS_ON_EXERCISES.md](03_HANDS_ON_EXERCISES.md)

2. **性能优化：**
   - 学习 perf 和 VTune
   - 研究微架构文档
   - 实战性能调优

3. **贡献：**
   - 为 Highway 提交 PR
   - 分享博客文章
   - 参加 CppCon 演讲

---

## 📬 联系和反馈

### Highway 官方

- **GitHub Issues：** 报告 bug 或请求功能
- **Discussions：** 技术讨论和问答
- **Email：** 查看 GitHub 仓库联系信息

### 本课程

- **GitHub：** `/home/dev/highway/highway_course/`
- **反馈：** 欢迎提交改进建议

---

## 🔄 保持更新

### 订阅更新

- **Watch Highway 仓库：** 接收更新通知
- **RSS 订阅博客：** 关注 SIMD 领域动态
- **Twitter/X：** 关注 @cpplang, @CppCon

### 定期检查

- Highway Releases（每月）
- Compiler Explorer 新特性（每季度）
- CppCon 新视频（每年）

---

## 💎 精华资源（必看）

如果时间有限，优先查看：

1. ⭐⭐⭐⭐⭐ **[Highway GitHub](https://github.com/google/highway)**
2. ⭐⭐⭐⭐⭐ **[Intel Intrinsics Guide](https://www.intel.com/intrinsics)**
3. ⭐⭐⭐⭐⭐ **[Compiler Explorer](https://godbolt.org/)**
4. ⭐⭐⭐⭐⭐ **[Agner Fog 手册](https://agner.org/optimize/)**
5. ⭐⭐⭐⭐⭐ **本课程深度剖析系列**

---

**持续学习，不断进步！** 🚀

*最后更新：2026-01-19 | 包含 100+ 资源链接*
