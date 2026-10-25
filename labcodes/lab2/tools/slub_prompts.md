# 简化 SLUB 实验提示词与验证说明

采用两层架构：底层继续使用 Buddy 的连续页分配接口，上层提供
`slub_init()`、`kmalloc(size)` 和 `kfree(ptr)`。小对象按 16、32、64、128、
256、512、1024、2048 字节分档；每个 slab 使用一页，在页首存放元数据，
在空闲对象自身存放下一空闲对象指针。cache 只维护尚有空闲对象的 slab 链表。
全部对象释放后，立即将整页归还 Buddy。

大于 2048 字节的请求直接申请连续页；页首保存原申请页数，释放时按该页数
调用 Buddy，由 Buddy 处理 2 的幂取整。当前 64 位布局的页首元数据按 16 字节
对齐后占 64 字节，例如申请 8193 字节需要 3 页，Buddy 实际提供 4 页。

返回指针保证 16 字节对齐。零字节请求、溢出或内存不足返回 NULL；
`kfree(NULL)` 无副作用。其余释放参数必须是仍存活的 kmalloc 原始返回指针。
初始化在 `pmm_init()` 后执行一次；实现面向当前单 hart 实验环境，体现
对象 cache、页内空闲链表、slab 状态切换与底层页回收，不包含多核并发扩展。

## Prompt 1：从零生成 SLUB 模块并接入内核

```text
请阅读当前 ucore 的 kern/mm/pmm.h、pmm.c、memlayout.h、buddy_pmm.c、
libs/list.h 以及 kern/init/init.c，在现有 Buddy 页分配器之上实现简化 SLUB，
新增 kern/mm/slub.c 和 slub.h。无需完整复现 Linux SLUB，主体思想清晰即可。

第一层复用 alloc_pages/free_pages/page2pa 和 va_pa_offset，不替换当前
pmm_manager，不改变 Page 布局和此前各分配器。第二层提供 slub_init()、
kmalloc(size_t size)、kfree(void *ptr)，在 kern_init 的 pmm_init 后初始化。
Makefile 已自动编译 kern/mm/*.c，不要引入新的构建系统。

按 16、32、64、128、256、512、1024、2048 字节建立固定大小对象 cache。
每个 slab 占一页，页首记录所属 cache、Page 描述符、空闲对象链表、在用数
等元数据，元数据和对象起始位置均按 16 字节对齐。将下一空闲对象指针存储
在空闲对象自身。每个 cache 维护 partial slab 链表，优先复用已有 slab；
页满时移出 partial，释放对象使满页变为 partial 时重新插入，整页空闲时
移出并归还 Buddy。不要为每次小对象申请单独分配一页。

大于 2048 字节的对象直接申请连续页，先把页首元数据计入总大小，再向上按页
取整。页首记录传给 Buddy 的原请求页数，释放时使用同一个请求页数，正确处理
Buddy 实际占用页数向上取整到 2 的幂的语义。保证用户可写区域不会覆盖元数据。

kmalloc(0)、大小加法溢出或内存不足时返回 NULL，kfree(NULL) 无操作；
其余释放参数必须是尚未释放的 kmalloc 原始返回指针。面向当前单 hart 环境，
slub_init() 只在 pmm_init() 后执行一次，不引入多核并发扩展。

声明 void slub_check(void)，由第二份提示词生成独立测试模块，并在初始化后
调用。保持旧分配器及其测试不变，说明接口与状态约定，执行真实 RISC-V 编译
和 QEMU 启动，报告实际结果。
```

## Prompt 2：生成不同大小对象的正确性测试

```text
请阅读刚生成的 SLUB 模块及其 kmalloc/kfree ，新增
kern/mm/slub_test.c，实现 slub_check()，在 kern_init 的 slub_init() 后调用。
测试通过公开接口观察结果，使用 assert 和 cprintf，不得改动先前 Buddy、
Best-Fit、First-Fit 的测试，也不能放宽断言迁就实现。

覆盖以下情况：
1. 零字节、NULL 释放、极大或溢出请求，失败前后页计数不变。
2. 每个 cache 档位两侧边界，以及 2048、2049、4032、4033、4096、4097、
   8193 字节等大对象和跨页对象；地址按 16 字节对齐且有效区间不重叠。
3. 同时分配不同大小的多个对象，填入不同数据，逐字节检查数据隔离，
   按不同顺序释放，确认所有底层页最终归还。
4. 同一 cache 的对象共享 slab；在其他对象仍存活时释放、重新分配一个
   对象，验证空闲链表复用且不额外申请页，不破坏其他对象数据。
5. 分配足够多的小对象填满 slab 并扩展到第二页；释放满页中的对象，
   检查 full-to-partial 切换与复用，最终回收所有 slab。
6. 持有一个小对象后用 Buddy 耗尽其余页：原 slab 尚有空闲对象时，同档
   分配仍可成功；需要新 slab 的另一档位及大对象请求应失败。释放对象
   归还一页后应重新分配成功。测试结束归还所有测试占用的对象和页。

再新增 tools/slub_host_test.c 和 tools/check_slub_host.py，用按页对齐的
真实宿主机字节缓冲区模拟物理 RAM，并运行原 Buddy 和 SLUB 实现。保留
部分页为保留区，检查大对象实际页数、元数据边界及近整数上界请求；对
多个存活对象执行确定种子的随机分配释放，每次核对对齐、不重叠、地址
范围与全部存活对象的字节模式，最后确认无页泄漏。
只在测试夹具中用等价 builtin offsetof 替代框架旧宏以便运行检测器，
不得改动正式算法。用 ASan/UBSan 检查宿主机执行，并完成真实 RISC-V
编译和正常 QEMU 启动，报告实际验证结果。不要在内核自检中故意触发 panic。
```

