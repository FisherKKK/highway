#!/bin/bash

# 这个脚本创建 Day 2-14 的课程大纲
# 实际详细内容需要逐步完善

cd /home/dev/highway/highway_course

# Day 2-14 的课程标题和核心内容提纲
declare -A courses

courses[day02]="类型系统与 Tag 机制"
courses[day03]="基本操作：加载、存储、算术运算"
courses[day04]="标量实现深入（scalar-inl.h）"
courses[day05]="x86 SSE2 实现剖析"
courses[day06]="ARM NEON 实现对比"
courses[day07]="实战练习：实现简单向量算法"
courses[day08]="动态分派机制详解"
courses[day09]="静态分派与编译时优化"
courses[day10]="高级算法：Transform 系列"
courses[day11]="向量化排序：VQSort 深度剖析"
courses[day12]="数学函数库实现"
courses[day13]="测试框架与最佳实践"
courses[day14]="性能优化与实战项目"

echo "课程大纲文件已在 README.md 中"
echo "现在创建各天的详细课程..."

