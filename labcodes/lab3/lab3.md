# 实验3：中断与异常处理

## 本机编译、运行与评分

```sh
cd /Users/liujinhao/Desktop/oslab/lab3_new
./run.sh build
./run.sh run
./run.sh grade
```

`run.sh` 复用 `/Users/liujinhao/Documents/ChatGPT/os` 中已安装的 RISC-V
工具链、QEMU 和 OpenSBI v0.4 固件，不修改 shell 配置。当前汇编器使用
`stval` 名称，脚本通过 `-Dsbadaddr=stval` 兼容框架的旧名称，保留 `SAVE_ALL`
原文。评分脚本直接调用 QEMU 和 `timeout`，运行脚本为它们建立临时兼容入口，
结束后自动清理；原有评分脚本未修改。

课程环境已提供工具链和兼容旧 CSR 名称的汇编器时，可以直接执行：

```sh
make
make qemu
make grade
```

正常运行每 100 次时钟中断打印 `100 ticks`，打印 10 次后关机。
`TICK_NUM=100`，`SHUTDOWN_AFTER=10`，后者按本次确认的默认值设置。
正常启动不会主动插入异常；评分在临时副本中注入异常和寄存器测试。

## 代码实现

### Trap 上下文保存与恢复

`kern/trap/trapentry.S` 中的帧大小为 `36 * REGBYTES`，RV64 下为 288 字节，
保持 C 函数调用要求的 16 字节栈对齐。

| 槽位 | 内容 |
| --- | --- |
| 0–31 | x0–x31，其中槽位 2 是中断前的 sp |
| 32 | sstatus |
| 33 | sepc |
| 34 | sbadaddr/stval |
| 35 | scause |

`RESTORE_ALL` 先恢复 `sstatus` 和 `sepc`，随后恢复通用寄存器。
sp 是访问当前帧的基址，最后才从槽位 2 恢复，不能在其他帧内容读取前改变。
x0 固定为零；`stval` 和 `scause` 是诊断信息，不参与 `sret` 的返回控制。

`__alltraps` 在 `SAVE_ALL` 后将 sp 放入 a0，作为 `trap(struct trapframe *tf)`
的第一个参数，然后调用 `trap`。C 函数返回后落入 `__trapret`，恢复状态并执行
`sret`。`SAVE_ALL`、`struct trapframe`、`trap_dispatch`、`trap` 均保持原样。

### 中断与异常

`kern/trap/trap.c` 新增两个处理函数。中断处理屏蔽 `scause` 最高位，再按框架
中断号分类。S 模式时钟中断先设置下一次事件，再递增 ticks，每 100 次打印，
打印次数达到 10 后调用 `sbi_shutdown()`。其他已定义的软件、时钟和外部中断
输出类型，未知中断输出原因及完整 TrapFrame。

非法指令和断点分别输出 `Illegal instruction at 0x…`、`Breakpoint at 0x…`，
再检查故障指令的首个 16 位，低两位为 `11` 时跳过 4 字节，否则跳过 2 字节。
该规则用于本实验 RV64+C 的 16/32 位指令，包括 `c.ebreak`、`ebreak` 和测试中的
非法编码；不扩展到本实验未使用的更长指令编码。其他异常打印完整 TrapFrame。
指令长度规则参见 [RISC-V 官方规范](https://docs.riscv.org/reference/isa/v20240411/unpriv/intro.html)。

### 实验2依赖与启动修复

- `kern/mm/pmm.c`：移植已有实验2的 `page_init()`，把原实验2的物理地址边界常量
  换成本框架现有常量的等价表达式。保留本框架默认 First-Fit 及中断保护接口。
- `kern/mm/best_fit_pmm.c`：移植已有实验2的 Best-Fit 实现，保留本框架自检代码。
- `kern/init/entry.S`：沿用实验2已有修复，在根页表第 2 项补充物理地址的恒等映射。
  这样开启 `satp` 后仍能继续读取当前物理 PC 的指令，再跳转到高地址内核。
  原框架只有高地址映射，在当前 QEMU 中无法进入 C 初始化。

未引入实验2中本实验不需要的 Buddy、SLUB 或其他扩展。

## 可复现提示词

### Prompt 1：Trap 汇编

> 请先阅读 kern/trap/trapentry.S、kern/trap/trap.h、libs/riscv.h，逐项确认
> SAVE_ALL 的 36 个槽位与 struct trapframe 的对应关系。仅补全 RESTORE_ALL
> 和 __alltraps，不修改 SAVE_ALL、结构体或 trap 的签名。先恢复 sstatus/sepc，
> 再恢复各可写通用寄存器，最后从槽位 2 恢复中断前 sp。解释为什么不能提前
> 恢复 sp。按照 RISC-V 调用约定将当前帧地址作为第一个参数传给 trap，调用
> 返回后进入 __trapret 并通过 sret 返回。检查栈对齐和临时寄存器的恢复顺序。

### Prompt 2：中断与异常处理

> 请阅读 trap.c、clock.c/h、riscv.h、sbi.h 及 tools/grade.sh，只新增
> interrupt_handler、exception_handler 和必需的头文件/常量，不修改现有函数。
> 对中断屏蔽 cause 最高位再分类；IRQ_S_TIMER 必须重设下一次事件、ticks 加一，
> 每 100 次调用已有 print_ticks，累计输出 10 次后调用 sbi_shutdown。
> 其他已定义类型打印名称，未知类型打印原因并调用 print_trapframe。
> 非法指令输出 Illegal instruction at 加地址，断点输出 Breakpoint at 加地址。
> 对本实验 RV64+C 的 16/32 位故障指令，读取首半字判断长度并更新 tf->epc，
> 同时支持 c.ebreak 和 ebreak。其他异常只打印帧。保留接口及统一返回流程。
> 最后运行原有评分，并实际验证 16/32 位异常返回、持续时钟和自动关机。

## 验证结果

2026-11-04 在本机 QEMU 中验证：

- 原有 `make grade`：35/35 上下文恢复、25/25 非法指令及兜底、40/40 持续时钟，
  总分 **100/100**，见 `logs/grade.log`。
- 临时副本额外检查 16 位/32 位断点、16 位/32 位非法指令返回，均通过；同时
  切换到 Best-Fit，原有页分配自检通过，见 `logs/extra.log`。
- 正常内核启动后恰好打印 10 次 `100 ticks`，QEMU 正常退出，见 `logs/boot.log`。
- 对照原始压缩包确认 `SAVE_ALL`、`trap.h`、`trap_dispatch`、`trap` 未修改。

补充测试只修改临时副本，没有把测试注入正式提交的内核。
