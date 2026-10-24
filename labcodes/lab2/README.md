# Lab2：物理内存管理

本目录包含物理页初始化、Best-Fit、Buddy 和简化 SLUB 的代码与测试。
当前内核选择 Buddy 作为页分配器，SLUB 在其上提供 kmalloc/kfree。

- [实验报告](实验报告.md)：整体逻辑、实现分析、spec/prompt、问题处理和验证结果。
- [思考题](思考题.md)：三个思考题的简答。
- Buddy 提示词：tools/buddy_prompts.md。
- SLUB 提示词：tools/slub_prompts.md；完整整理版本见实验报告第六节。

## 宿主机测试

在本目录执行，需 Python 3 和 Clang：

~~~sh
python3 tools/check_buddy_host.py
python3 tools/check_slub_host.py
~~~

## 内核编译与运行

准备 RISC-V 交叉工具链及兼容本实验 legacy SBI 接口的 QEMU/OpenSBI 环境，在本目录执行：

~~~sh
make
make qemu
~~~

默认工具链前缀为 riscv64-unknown-elf-；不同安装位置可通过 GCCPREFIX 和 QEMU 参数指定。
本机 macOS 的实际参数、Best-Fit 切换方法和成功输出见实验报告第八节。
内核自检结束后停在循环中，QEMU 可通过 Ctrl+A、再按小写 x 退出。

## 提交目录

本实验的源码、测试与两份报告统一保存在仓库的 labcodes/lab2 目录。
编译生成的 bin、obj 等文件不纳入版本控制。
