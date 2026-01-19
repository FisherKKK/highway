# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Highway is a C++ library providing portable SIMD/vector intrinsics that work across multiple architectures (x86, ARM, RISC-V, POWER, WebAssembly, etc.). The same application code can target various instruction sets with either static or dynamic dispatch.

## Build System

Highway supports **CMake** (primary) and **Bazel** (less commonly used).

### CMake Build Commands

**Basic build and test:**
```bash
mkdir -p build && cd build
cmake ..
make -j
make test  # or: ctest -j
```

**Build with specific options:**
```bash
# Debug build with Clang 17
CXX=clang++-17 CC=clang-17 cmake .. -DCMAKE_BUILD_TYPE=Debug

# Release build with warnings as errors (for CI)
cmake .. -DHWY_WARNINGS_ARE_ERRORS:BOOL=ON -DCMAKE_BUILD_TYPE=Release

# For Armv7 targets (required due to compiler limitations)
cmake .. -DHWY_CMAKE_ARM7:BOOL=ON

# For 32-bit x86 with SSE2 baseline
cmake .. -DHWY_CMAKE_SSE2:BOOL=ON
```

**Comprehensive test script:**
```bash
./run_tests.sh  # Runs full test suite across multiple configurations
```

### Bazel Build Commands

```bash
bazel build -c opt :all
bazel test -c opt :all
```

### Running a Single Test

```bash
# CMake: build specific test
cd build
make arithmetic_test -j
ctest -R arithmetic_test

# Or run directly
./tests/arithmetic_test

# Bazel
bazel test :arithmetic_test
```

## Code Architecture

### Directory Structure

- **`hwy/`** - Core library
  - **`ops/`** - Platform-specific SIMD implementations (arm_neon-inl.h, x86_128-inl.h, rvv-inl.h, etc.)
  - **`contrib/`** - Higher-level SIMD utilities
    - **`algo/`** - Algorithms (copy, find, transform)
    - **`math/`** - Math functions (trigonometry, etc.)
    - **`sort/`** - Vectorized quicksort (VQSort)
    - **`dot/`** - Dot product implementations
    - **`thread_pool/`** - Thread pool for parallel SIMD operations
    - **`image/`** - Image class with aligned rows
    - **`bit_pack/`** - Bit packing utilities
    - **`random/`** - Random number generation
    - **`matvec/`** - Matrix-vector operations
    - **`unroller/`** - Loop unrolling utilities
  - **`tests/`** - Unit tests for all SIMD operations
  - **`examples/`** - Example code (skeleton, benchmark)

### Core Headers

- **`hwy/highway.h`** - Main header, must be included for vector operations
- **`hwy/base.h`** - Platform-independent utilities (no SIMD), includes PopCount, type traits
- **`hwy/foreach_target.h`** - Enables dynamic dispatch by re-including the translation unit for each target
- **`hwy/aligned_allocator.h`** - Memory allocation with SIMD alignment requirements
- **`hwy/targets.h`** - Target detection and selection
- **`hwy/detect_targets.h`** - Compile-time target detection
- **`hwy/ops/*-inl.h`** - Platform-specific operation implementations (textual headers)

### Dispatch Mechanisms

Highway supports two dispatch models:

**Static Dispatch** - Single target, zero runtime overhead:
```cpp
// Call function compiled for HWY_STATIC_TARGET
HWY_STATIC_DISPATCH(MyFunction)(args);
```

**Dynamic Dispatch** - Multiple targets, runtime selection:
```cpp
// Requires: #define HWY_TARGET_INCLUDE before #include "hwy/foreach_target.h"
// foreach_target.h re-compiles the same source for each enabled target
HWY_DYNAMIC_DISPATCH(MyFunction)(args);
```

The dispatch system works by:
1. `foreach_target.h` re-includes your translation unit multiple times
2. Each inclusion defines a different `HWY_TARGET` (e.g., HWY_AVX2, HWY_NEON)
3. Each target gets its own namespace (N_AVX2, N_NEON, etc.)
4. `HWY_DYNAMIC_DISPATCH` selects the best target at runtime based on CPU features

### Header Include Pattern for Dynamic Dispatch

Vector code headers (typically named `*-inl.h`) use a special include guard that toggles with `HWY_TARGET_TOGGLE`:

```cpp
#if defined(HIGHWAY_HWY_MYCODE_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef HIGHWAY_HWY_MYCODE_INL_H_
#undef HIGHWAY_HWY_MYCODE_INL_H_
#else
#define HIGHWAY_HWY_MYCODE_INL_H_
#endif

#include "hwy/highway.h"
// Your vector code here
namespace hwy {
namespace HWY_NAMESPACE {
// Operations...
}  // namespace HWY_NAMESPACE
}  // namespace hwy
#endif
```

This allows the header to be re-included for each target compilation.

### Namespacing Requirements

Due to ADL (Argument Dependent Lookup) restrictions, Highway operations must be called in one of these ways:

1. **Inside Highway namespace:**
```cpp
namespace hwy {
namespace HWY_NAMESPACE {
  auto result = Add(a, b);  // Direct call
}
}
```

2. **With namespace alias:**
```cpp
namespace hn = hwy::HWY_NAMESPACE;
auto result = hn::Add(a, b);
```

3. **With using declarations:**
```cpp
using hwy::HWY_NAMESPACE::Add;
auto result = Add(a, b);
```

### Function Attributes

Functions calling Highway ops must either:
- Be prefixed with `HWY_ATTR`, OR
- Reside between `HWY_BEFORE_NAMESPACE()` and `HWY_AFTER_NAMESPACE()`

Lambda functions require `HWY_ATTR` before the opening brace.

### Target Detection at Compile Time

`HWY_TARGET` macro indicates which instruction set is being compiled:
- `HWY_SCALAR` - Portable scalar fallback
- `HWY_SSE2`, `HWY_SSSE3`, `HWY_SSE4` - x86 SSE variants
- `HWY_AVX2`, `HWY_AVX3`, `HWY_AVX3_DL`, `HWY_AVX3_ZEN4`, `HWY_AVX3_SPR` - x86 AVX variants
- `HWY_NEON`, `HWY_SVE`, `HWY_SVE2` - ARM variants
- `HWY_RVV` - RISC-V Vector Extension
- `HWY_PPC8`, `HWY_PPC9`, `HWY_PPC10` - POWER variants
- And many more (see README.md for full list)

Use `#if HWY_TARGET == HWY_NEON` for platform-specific code.

## Common Development Patterns

### Creating a New SIMD Function

1. **For header-only code with dynamic dispatch:**
   - Create `my_code-inl.h` with the toggle pattern
   - Place implementation in `namespace hwy::HWY_NAMESPACE`
   - Create `my_code.cc` that defines `HWY_TARGET_INCLUDE` and includes `foreach_target.h`
   - Use `HWY_EXPORT` macro to export function pointers
   - Call with `HWY_DYNAMIC_DISPATCH(MyFunction)(args)`

2. **For static dispatch:**
   - Include `hwy/highway.h`
   - Write code in `namespace hwy::HWY_NAMESPACE`
   - Call with `HWY_STATIC_DISPATCH(MyFunction)(args)`

See `hwy/examples/skeleton.{h,cc,-inl.h}` for a complete working example.

### Vector Tags and Types

Use **tags** to select vector types, not template parameters:
- `ScalableTag<T>` or `HWY_FULL(T)` - Full vector width for type T
- `CappedTag<T, N>` or `HWY_CAPPED(T, N)` - Up to N lanes (rounded down to power of 2)
- `FixedTag<T, N>` - Exactly N lanes (N must be power of 2)

Example:
```cpp
const ScalableTag<float> df;  // Full vector of floats
auto v = Zero(df);            // Create zero vector
```

### Writing Tests

Tests use googletest framework (or HWY_TEST_STANDALONE mode):
```cpp
#include "hwy/highway.h"
#include "hwy/tests/test_util.h"

namespace hwy {
namespace HWY_NAMESPACE {

struct TestMyOp {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v = Set(d, T(42));
    HWY_ASSERT_VEC_EQ(d, v, MyOp(d, v));
  }
};

HWY_NOINLINE void TestAllMyOp() {
  ForAllTypes(ForPartialVectors<TestMyOp>());
}

}  // namespace HWY_NAMESPACE
}  // namespace hwy

#if HWY_ONCE
namespace hwy {
HWY_BEFORE_TEST(MyOpTest);
HWY_EXPORT_AND_TEST_P(MyOpTest, TestAllMyOp);
HWY_AFTER_TEST();
}  // namespace hwy
#endif
```

### Loop Vectorization Strategies

1. **Padded arrays (preferred):**
```cpp
for (size_t i = 0; i < count; i += Lanes(d)) {
  auto v = Load(d, &array[i]);
  // ... process v
}
```

2. **Process whole vectors, overlap at end:**
```cpp
for (size_t i = 0; i < count; i += N) {
  auto v = Load(d, &array[HWY_MIN(i, count - N)]);
}
```

3. **Use Transform functions from hwy/contrib/algo/transform-inl.h:**
```cpp
Transform1(d, in, n, out, [](auto d, auto v) { return Mul(v, v); });
```

4. **Masking for remainder:**
```cpp
size_t i = 0;
for (; i + N <= count; i += N) { /* full vectors */ }
if (i < count) {
  BlendedStore(v, FirstN(d, count - i), d, &out[i]);
}
```

## Compiler Requirements

- **C++17** minimum (language features, not necessarily the full library)
- **GCC 4.9+**, **Clang 3.9+**, **MSVC 2015+**
- Function-specific target attributes required (supported by all modern compilers)

### Compiler-Specific Flags

**MSVC:**
- Compile with `/Gv` to pass vector arguments in registers
- Use `/arch:AVX2` when using AVX2 with half-width vectors to ensure VEX encoding

**GCC/Clang:**
- `-O2` or higher for reasonable performance (inlining is critical)
- Avoid `-march` flags if using dynamic dispatch (can conflict with target pragmas)
- If `-march` is necessary, define `HWY_COMPILE_ONLY_STATIC` or `HWY_SKIP_NON_BEST_BASELINE`

## Important Notes

### Safety and Best Practices

- **Never** use namespace-scope or `static` initializers for SIMD vectors (can cause SIGILL with runtime dispatch)
- **Always** read files before modifying them
- Prefer **`ScalableTag<T>`** over `HWY_FULL(T)` in new code
- Prefer **`CappedTag<T, N>`** over `HWY_CAPPED(T, N)` in new code
- **Pad arrays** when possible rather than handling remainders
- Use `#if !HWY_MEM_OPS_MIGHT_FAULT` before using MaskedLoad/BlendedStore for remainder handling

### Performance Considerations

- Operations are designed to map efficiently to all supported platforms
- Some operations are expensive on certain platforms (see `g3doc/instruction_matrix.pdf`)
- Horizontal operations (lane reduction) are generally slower than vertical operations
- Full vectors (`ScalableTag`) are most efficient and future-proof

### Cross-Compilation and Testing

Tests can run under QEMU for cross-platform testing. The `run_tests.sh` script includes examples for ARM, POWER, RISC-V, and IBM Z architectures using cross-compilers and QEMU emulation.

## Version Information

Project version defined in CMakeLists.txt line 39 (currently 1.3.0). Keep in sync with `hwy/base.h` version constants.
