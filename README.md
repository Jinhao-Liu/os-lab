# Lab 1: 比麻雀更小的麻雀（最小可执行内核）

> 南开大学 2026 年操作系统课程实验 · riscv64-ucore
> 实验指导书：http://8.135.34.58/lab2026/_book/

## 小组信息

| 成员 | 分工 |
|------|------|
| 2510670 刘晋豪（组长） | 实验环境搭建与修复（QEMU 4.1.1 源码编译）、练习1 |
| 2510547 李佩霖 | 练习2（GDB 启动流程追踪）、AI 交互记录汇总 |
| 2512739 徐乙涵 | 实验报告整理与测试验证 |

## 目录结构

```
lab1 分支
├── code/               # lab1 实验源码（最小可执行内核）
│   ├── Makefile
│   ├── kern/           # 内核代码（入口 entry.S、初始化 init.c、SBI 控制台输出）
│   ├── libs/           # SBI 调用封装与基础库
│   └── tools/          # 链接脚本 kernel.ld（内核基址 0x80200000）
└── report/             # 实验交付物
    ├── report.md       # 实验报告（练习1/2 解答、环境修复记录、测试截图）
    ├── prompt.md       # AI 交互提示词汇总
    └── images/         # 测试与调试截图
```

## 实验环境

| 项目 | 版本 |
|------|------|
| 运行环境 | Windows 11 + WSL2（Ubuntu 24.04） |
| 交叉编译器 | riscv64-unknown-elf-gcc 13.2.0 |
| 模拟器 | **QEMU 4.1.1**（源码编译，见下方说明） |
| 固件 | OpenSBI v0.4（QEMU 4.1.1 内置，Runtime SBI 0.1） |
| 调试器 | gdb-multiarch 15.1 |
| AI 协作工具 | Kimi Code（Kimi / Moonshot AI） |

> ⚠️ **QEMU 版本注意事项**：apt 源的 QEMU 8.x 自带 OpenSBI v1.3，已移除旧版 SBI v0.1
> 控制台接口（本内核 `libs/sbi.c` 依赖该接口），会导致内核无输出。必须按指导书
> 从源码编译 QEMU 4.1.1：
>
> ```bash
> wget https://download.qemu.org/qemu-4.1.1.tar.xz
> tar xvJf qemu-4.1.1.tar.xz && cd qemu-4.1.1
> ./configure --target-list=riscv64-softmmu,riscv32-softmmu --disable-werror
> make -j$(nproc) && make install
> ```

## 构建与运行

```bash
cd code
make            # 交叉编译，生成 bin/kernel 与 bin/ucore.img
make qemu       # 运行：OpenSBI v0.4 启动后内核打印 (THU.CST) os is loading ...
                # 退出：先按 Ctrl+a，松开后再按 x
```

## GDB 调试（练习2）

```bash
# 终端 1：QEMU 开启 GDB stub（1234 端口）并停在复位地址
make debug

# 终端 2：接入调试
gdb-multiarch bin/kernel
(gdb) set arch riscv:rv64
(gdb) target remote localhost:1234
(gdb) x/8i 0x1000          # 加电后最初执行的指令（复位地址 0x1000）
(gdb) break *0x80200000    # 内核入口断点
(gdb) continue             # OpenSBI 初始化完成后命中 kern_entry
```

启动流程接力链：`0x1000`（QEMU 复位代码）→ `0x80000000`（OpenSBI 固件）→ `0x80200000`（内核 `kern_entry`）。

详见 [report/report.md](report/report.md)。
