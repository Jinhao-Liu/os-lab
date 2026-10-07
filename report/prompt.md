# Lab1 提示词汇总（prompt.md）

本文件汇总 Lab1 实验过程中与 AI（Kimi Code）交互的全部主要提示词，按时间顺序排列。Lab1 无需编写代码，AI 主要用于环境搭建、问题定位与 GDB 调试验证。

---

## 1. 环境检查与课程要求梳理

**提示词：**

> （附上两次实验课飞书妙记录像链接）先看视频，检查本地进度

**AI 行为**：提炼两次课程录像的要求（智能体配置、RISC-V 工具链、指定版本 QEMU、Lab1 任务与 10 月 13 日截止日期），逐项检查本机 WSL、NodeJS、Git、交叉编译器、QEMU、GDB 的安装情况，给出进度清单。

---

## 2. 澄清工具链前提

**提示词：**

> 我没用 Claude code，用的 Kimi code

**AI 行为**：修正进度结论——智能体环境已由 Kimi Code 直接承担（可读文件、改代码、执行命令），无需另装 Claude Code / CCSwitch；真正的缺口只剩课程 lab1 代码与指定版本 QEMU。

---

## 3. 获取实验材料并制定下一步计划

**提示词：**

> 这是我们 lab1 的实验文档和实验代码：http://8.135.34.58/lab2026/_book/ ，看看我们下一步需要做点什么（附 lab1.zip、实验报告模板.md、群公告截图）

**AI 行为**：解压并通读 lab1 代码（Makefile / entry.S / init.c / sbi.c / kernel.ld），阅读指导书 lab0 环境搭建页与 lab1 全部章节，确认提交规范（小组公开 git 仓库、`lab1` 分支、`code/` + `report/` 目录、report.md / prompt.md / images/），在 WSL 中完成首次编译。

---

## 4. 定位"内核无输出"问题（本实验最关键的调试迭代）

**现象**：`make qemu` 后 OpenSBI 正常启动，但内核不打印 `(THU.CST) os is loading ...`。

**AI 的推理与验证过程**：

1. 检查 `libs/sbi.c` → 发现使用旧版 SBI v0.1 控制台调用（EID=1）；
2. 核对环境 → 当前 QEMU 8.2.2 自带 OpenSBI v1.3，已移除 legacy SBI v0.1 扩展，判定为接口契约失效；
3. 按指导书源码编译 QEMU 4.1.1：

```
wget https://download.qemu.org/qemu-4.1.1.tar.xz
tar xvJf qemu-4.1.1.tar.xz && cd qemu-4.1.1
./configure --target-list=riscv64-softmmu,riscv32-softmmu --disable-werror
make -j$(nproc) && make install
```

4. 重跑 `make qemu` → 输出 `Runtime SBI Version : 0.1` 与内核打印，问题解决。

---

## 5. 完成练习1

**提示词：**

> 现在开始做（练习1：分析 entry.S 中 la sp, bootstacktop 与 tail kern_init 的操作与目的）

**AI 行为**：结合 `memlayout.h`（KSTACKSIZE = 2×4KB）、`mmu.h`（PGSHIFT = 12）、`kernel.ld`（BASE_ADDRESS = 0x80200000）分析两条指令，并用 GDB 观察 `la sp, bootstacktop` 执行前后 sp 从 `0x8001bd80`（OpenSBI 的栈）变为 `0x80203000`（bootstacktop）作为证据。

---

## 6. 完成练习2（GDB 追踪启动流程）

**提示词：**

> 现在开始做（练习2：用 GDB 追踪 QEMU 模拟的 RISC-V 从加电到内核第一条指令的完整过程，回答最初指令位于什么地址、完成什么功能）

**AI 行为**：

1. `make debug` 启动 QEMU（`-s -S`），`gdb-multiarch` + `target remote localhost:1234` 接入；
2. `x/8i 0x1000` 反汇编复位代码并单步执行，确认 PC 从 `0x1000` 经 `jr t0` 跳入 OpenSBI 基址 `0x80000000`；
3. 按指导书提示 `watch *0x80200000` 观察内核加载——观察点**从未触发**；改为在 CPU 尚未执行任何指令时检查 `0x80200000`，发现内核指令已在内存中 → 得出"QEMU 在复位时由 `-device loader` 直接装入内核，OpenSBI 只负责跳转"的结论；
4. `b *0x80200000` + `continue` 命中内核入口，记录交接现场（pc / sp / a0 / a1）；
5. 全程调试命令与输出保存为原始日志，并整理为报告截图。

---

## 7. 整理交付物

**AI 行为**：按群公告要求组织 `code/`（lab1 源码）与 `report/`（report.md + prompt.md + images/ 三张测试截图）目录结构，等待小组 git 仓库地址后创建 `lab1` 分支推送。

---

**使用说明**：以上提示词为实验过程中真实交互的汇总整理。Lab1 的重心是"用 AI 加速定位与验证"，而非代码生成；验收时建议能现场复述每个结论的验证手段（断点、观察点、内存检查）。
