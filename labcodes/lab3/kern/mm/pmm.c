#include <default_pmm.h>
#include <best_fit_pmm.h>
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <../sync/sync.h>
#include <riscv.h>
#include <dtb.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// the kernel image is mapped at VA=KERNBASE and PA=info.base
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
uintptr_t *satp_virtual = NULL;
// physical address of boot-time page directory
uintptr_t satp_physical;

// physical memory management
const struct pmm_manager *pmm_manager;


static void check_alloc_page(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
    pmm_manager = &default_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    struct Page *page = NULL;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        page = pmm_manager->alloc_pages(n);
    }
    local_intr_restore(intr_flag);
    return page;
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        pmm_manager->free_pages(base, n);
    }
    local_intr_restore(intr_flag);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    size_t ret;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        ret = pmm_manager->nr_free_pages();
    }
    local_intr_restore(intr_flag);
    return ret;
}

// LAB2: 沿用实验2的物理页初始化，适配本框架的地址常量。
static void page_init(void) {
    // 链接脚本给出的内核结束虚拟地址，描述符数组放在它之后。
    extern char end[];
    // dtb_init() 已解析设备树；这里读取结果，使用半开区间 [mem_base, mem_end)。
    uint64_t mem_base = get_memory_base();
    uint64_t mem_size = get_memory_size();
    uint64_t mem_end = mem_base + mem_size;

    // 当前实验用零值表示无效信息；mem_end < mem_base 表示无符号加法溢出。
    if (mem_base == 0 || mem_size == 0 || mem_end < mem_base) {
        panic("invalid physical memory range from DTB");
    }

    // 必须先设置偏移，后面的 PADDR(pages) 才能正确换算物理地址。
    va_pa_offset = PHYSICAL_MEMORY_OFFSET;
    // 输出设备树报告的 RAM 范围，此时还没有扣除保留空间或截断管理上界。
    cprintf("physical memory map:\n");
    cprintf("  memory: 0x%016lx, [0x%016lx, 0x%016lx).\n",
            (unsigned long)mem_size, (unsigned long)mem_base,
            (unsigned long)mem_end);

    // 取硬件范围与框架支持范围的交集上界，向下对齐以排除末尾不足一页的空间。
    uintptr_t maxpa = mem_end;
    if (maxpa > (KERNTOP - PHYSICAL_MEMORY_OFFSET)) {
        maxpa = (KERNTOP - PHYSICAL_MEMORY_OFFSET);
    }
    maxpa = ROUNDDOWN(maxpa, PGSIZE);
    // RAM 必须覆盖约定的内核加载起点；后面还会检查内核之后能否放下描述符。
    if (mem_base > (KERNBASE - PHYSICAL_MEMORY_OFFSET) || maxpa <= (KERNBASE - PHYSICAL_MEMORY_OFFSET)) {
        panic("physical memory does not cover the kernel");
    }

    // 物理页号从 DRAM_BASE / PGSIZE 开始，有效描述符下标为 [0, npage - nbase)。
    npage = PPN(maxpa);
    // end 是虚拟地址，向上对齐后得到描述符数组的虚拟起点。
    pages = (struct Page *)ROUNDUP((uintptr_t)end, PGSIZE);
    // PADDR(pages) 是数组自身的存放位置；page2pa(pages) 才是 pages[0] 描述的页。
    uintptr_t pages_pa = PADDR(pages);
    size_t page_count = npage - nbase;
    // 先检查起点，利用 || 短路避免减法下溢；用除法检查空间，避免先乘数量。
    if (pages_pa > maxpa ||
        page_count > (maxpa - pages_pa) / sizeof(struct Page)) {
        panic("not enough physical memory for page descriptors");
    }

    // 先全部标为保留，防止 OpenSBI、内核及描述符占用的页被分配出去。
    // 这里只初始化管理信息，不清零这些物理页中原有的数据。
    for (size_t i = 0; i < page_count; i ++) {
        struct Page *p = pages + i;
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
        SetPageReserved(p);
    }

    // 数组末尾向上对齐，跳过 OpenSBI、内核、整个描述符数组及对齐空隙。
    uintptr_t freemem = ROUNDUP(pages_pa + page_count * sizeof(struct Page),
                               PGSIZE);
    // 相等时没有完整空闲页，不调用 init_memmap，也避免 pa2page(maxpa) 越界。
    if (freemem < maxpa) {
        // 物理起点先转换为 Page 指针，具体空闲块组织方式由当前分配器决定。
        init_memmap(pa2page(freemem), (maxpa - freemem) / PGSIZE);
    }
}

/* pmm_init - initialize the physical memory management */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}
