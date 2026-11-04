# Lab3：RISC-V 中断与异常处理

本目录包含 Trap 上下文恢复、汇编入口到 C 处理函数的调用、中断与异常分类处理，以及本实验依赖的实验2内存初始化代码。

- [实验报告](实验报告.md)：整体逻辑、核心模块、spec/prompt、问题处理、验证结果和 OS 原理对应关系。
- [Spec 与提示词](prompt.md)：从本次报告第三节整理的各项约束和提示词。
- [运行与实现说明](lab3.md)：本机运行方法与实现细节。
- [评分记录](logs/grade.log)、[正常启动记录](logs/boot.log)、[补充测试记录](logs/extra.log)。

## 内核编译与运行

准备 RISC-V 交叉工具链，以及兼容本实验 legacy SBI 接口的 QEMU/OpenSBI 环境，在本目录执行：

```sh
make
make qemu
make grade
```

默认工具链前缀为 `riscv64-unknown-elf-`，可通过 `GCCPREFIX` 指定其他前缀，通过 `QEMU` 指定运行包装器。若当前汇编器提示 `unknown CSR sbadaddr`，可在构建命令中传入 `CC="riscv64-unknown-elf-gcc -Dsbadaddr=stval"`。原评分脚本直接调用 QEMU，还需要运行环境中具备 `timeout`。

仓库保留了本次 Mac 环境的运行脚本；原工具和固件目录仍位于 `/Users/liujinhao/Documents/ChatGPT/os` 时，可以执行：

```sh
./run.sh build
./run.sh run
./run.sh grade
```

`run.sh` 依赖该本机工具目录，不包含工具链或固件安装包；其他机器按前面的课程环境方式构建。正常运行每 100 次时钟中断打印 `100 ticks`，打印十次后自动关机。提前退出 QEMU 可按 Ctrl+A，松开后再按小写 x。

## 验证范围

原有评分覆盖 Trap 入口、寄存器恢复、断点和非法指令返回、分类兜底及持续时钟中断。补充记录验证了 16/32 位断点、16/32 位非法指令返回和 Best-Fit 自检。正式内核仍采用框架默认分配器，异常测试通过临时副本注入。

## 提交目录

本实验使用独立的 `lab3` 分支，源码、报告与提示词保存在 `labcodes/lab3`，沿用 lab2 的目录格式。源码中原有的三处 `lab3` 填空注释及当前学号标记均保留，编译产生的 `bin`、`obj` 文件不纳入版本控制。
