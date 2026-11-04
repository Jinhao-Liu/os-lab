# Lab3：Spec 与提示词

以下内容整理自本次提交的实验报告第三节。

## 1. 前置工作：补回实验2依赖

**Spec。** 输入是已有实验2的 `page_init()`、Best-Fit 代码及实验3框架。输出是在实验3两个 `lab2` 标记处补齐的必要实现。移植应使用当前框架已有的地址常量，保持 `pmm_manager` 接口、默认分配策略和带中断保护的分配接口。

**Prompt。**

> 请比较已有实验2与当前实验3的 pmm.c、best_fit_pmm.c 及相关头文件。只提取当前框架缺少的 page_init 和 Best-Fit 实现，填入 lab2 标记处。把旧框架中的物理地址边界常量转换成当前常量的等价表达式，保持当前默认分配器、alloc_pages/free_pages 中的中断保护和已有自检函数。不要引入本次不需要的 Buddy、SLUB 模块。最后编译并验证默认分配器与 Best-Fit 自检。



## 2. 练习一：`RESTORE_ALL` 与 `__alltraps`

**输入与前置条件。** `SAVE_ALL`、`struct trapframe`、寄存器宽度宏以及 `trap()` 接口已经存在。进入恢复宏时，sp 指向完整且可访问的 TrapFrame；进入汇编入口时，当前实验内核栈有效，并满足标准对齐约束。

**输出与后置条件。** `__alltraps` 将实际帧地址作为 `trap()` 的第一个参数传入，C 函数返回后能够进入 `__trapret`。`RESTORE_ALL` 按实际槽位恢复通用寄存器及返回所需 CSR，并采用槽位 2 中的原 sp。恢复后由已有 `sret` 完成返回，帧中的 `epc` 可以被 C 处理函数更新。

**不变量与修改范围。** 保留 `SAVE_ALL`、结构体布局、`trap()` 签名；不假设另外一套帧格式；在所有依赖当前 sp 的读取完成前不改变它。恢复 CSR 的临时寄存器必须在使用后恢复。当前任务不扩展用户栈切换或浮点、向量上下文。

**验收条件。** 编译通过，原有寄存器上下文和断点返回测试通过；GDB 中帧大小为 288 字节，保存的原 sp 正确，处理返回后能回到帧指定的执行位置。

**Prompt。**

> 请先阅读 kern/trap/trapentry.S、kern/trap/trap.h 和 libs/riscv.h，列出 SAVE_ALL 的 36 个槽位与 C 字段的对应关系，再补全 RESTORE_ALL 与 __alltraps。不得修改 SAVE_ALL、struct trapframe 和 trap 的签名。RESTORE_ALL 先恢复 sstatus、sepc，再恢复通用寄存器，最后从槽位 2 恢复中断前 sp。解释临时寄存器与 sp 的恢复顺序。__alltraps 应按 RISC-V 调用约定传递当前帧地址，调用 trap 后进入已有 __trapret。检查栈对齐，并通过原有寄存器和断点返回测试。



## 3. 练习二 A：`interrupt_handler()`

**输入。** 一个有效的 `struct trapframe *tf`。正常中断分发传入的 `cause` 包含中断标记位；分类时应去掉该位。框架评分还会直接构造原因编号来测试兜底分支。

**行为与输出。** 对 `IRQ_S_TIMER` 每次重设下一次事件，并且仅把 `ticks` 增加一次；当 `ticks` 为 `TICK_NUM` 的整数倍时调用已有输出函数，累计输出达到 `SHUTDOWN_AFTER` 后调用关机接口。对其他已定义中断打印名称，对未知中断输出原因与完整帧。

**参数与边界。** 本次采用 `TICK_NUM=100`、`SHUTDOWN_AFTER=10`，后者是框架未给出数值后确认采用的默认值。普通中断处理不跳过帧中 `epc` 指向的指令。只补充指定处理函数、必需的头文件和常量，保留已有输出函数及统一分发逻辑。

**验收条件。** 原有持续时钟测试至少观察到 `ticks=200`；正常运行打印十次 `100 ticks` 后退出；软件中断和未知原因诊断符合评分脚本要求。

**Prompt。**

> 请阅读 trap.c、clock.c/h、riscv.h、sbi.h 和 tools/grade.sh，新增 interrupt_handler，保留现有函数。屏蔽 RV64 cause 最高位后分类。对 IRQ_S_TIMER 先设置下一次事件，再将 ticks 加一；每到 100 的整数倍调用 print_ticks，累计输出十次后调用 sbi_shutdown。其他框架已定义类型输出对应名称；未知类型打印 Unknown interrupt: cause 加原因编号，并调用 print_trapframe。不要将 ticks 与输出次数混为一谈，不修改中断帧中的 epc。验证持续中断、统计输出和关机。

## 4. 练习二 B：`exception_handler()`

**输入。** 当前异常的有效 TrapFrame。对于本实验要求跳过的非法指令和断点，`epc` 指向可读取的内核指令，编码长度限定为本次测试涉及的 16/32 位。

**行为与输出。** 按 `tf->cause` 区分非法指令、断点及其他异常。前两类输出规定前缀和原故障地址，然后按实际编码长度增加 `epc`。其他异常调用 `print_trapframe()`，不擅自增加恢复策略。

**约束。** 保持函数签名与统一返回机制，不把两种异常都固定处理为 `epc += 4`，也不改变其他通用寄存器。这里的 16/32 位判断不宣称支持更长编码。

**验收条件。** 原有非法指令和断点测试返回成功；补充的 2/4 字节断点、2/4 字节非法指令均能执行到异常指令后的检查输出，不形成异常循环。

**Prompt。**

> 请新增 exception_handler(struct trapframe *tf)，保留 trap.c 中已有函数。对 CAUSE_ILLEGAL_INSTRUCTION 输出 Illegal instruction at 加故障地址，对 CAUSE_BREAKPOINT 输出 Breakpoint at 加故障地址。结合本实验 RV64+C 的 16/32 位指令编码，读取首半字判断长度并更新 tf->epc，同时支持 c.ebreak 与 ebreak。其他异常仅调用 print_trapframe。不扩展任务范围。请说明跳过指令的适用边界，并实际验证四种长度与异常组合均能返回。
