# Lab 4 内核线程管理

沿用原仓库小组信息。

| 姓名 | 学号 |
| --- | --- |
| 刘晋豪 | 2510670 |
| 李佩霖 | 2510547 |
| 徐乙涵 | 2512739 |

在课程发布的 Lab 4 骨架中接入前序实验实现。源码中各处 `LAB4` 标记使用组长学号 `2510670`。[课程源码下载页](http://oslab.mobisys.top/lab2026/labcode.html) · [Lab 4 实验指导书](http://oslab.mobisys.top/lab2026/_book/lab4/lab4.html)

## 文件

- `kern/mm/` 包含物理页分配、Sv39 页表、SLOB 和 VMA。
- `kern/process/`、`kern/schedule/` 包含线程创建、调度和上下文切换。
- `kern/init/`、`kern/trap/` 包含分页启动、Trap 处理和新线程的首次恢复入口。
- `tools/` 包含课程评测、测试注入、回归检查和链接脚本。
- [实验报告](实验报告.md) 涵盖实现、各练习的 spec 和 prompt、调试、知识点及 Challenge。
- [logs](logs/) 下记录交叉编译、普通启动、评分结果和注入评测后的串口输出。

## 编译和验收

在 Linux 或 WSL 中进入 `labcodes/lab4`。实测使用 Ubuntu 26.04、RISC-V GCC 15.2.0、QEMU 4.1.1（内置 OpenSBI v0.4）和 GDB 17.1。评分还需要 GNU Make、Python 3 和 `timeout`。

```sh
make -j4 GCCPREFIX=riscv64-linux-gnu-
make qemu GCCPREFIX=riscv64-linux-gnu-
make grade GCCPREFIX=riscv64-linux-gnu- GDB=gdb-multiarch
```

使用 `riscv64-unknown-elf-` 工具链时可省略 `GCCPREFIX`。可以用 `QEMU=/path/to/qemu-system-riscv64` 和 `BIOS=/path/to/firmware.bin` 指定模拟器与固件。固件需支持源码使用的 legacy SBI 控制台和计时器接口。

普通启动会输出页表和 VMA 自检结果，随后 `initproc` 打印问候语。它返回后进入实验骨架保留的 `do_exit` 调试终点，此处打印 `process exit!!.` 并进入内核监视器。退出 QEMU 时先按 Ctrl+A，再按 x。评测检查 PMM、进程创建及调度，每项 10 分。
