#include <default_pmm.h>
#include <defs.h>
#include <error.h>
#include <kmalloc.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <sync.h>
#include <vmm.h>
#include <riscv.h>
#include <dtb.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// The kernel image is mapped at VA=KERNBASE and PA=info.base
uint_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
pde_t *boot_pgdir_va = NULL;
// physical address of boot-time page directory
uintptr_t boot_pgdir_pa;

// physical memory management
const struct pmm_manager *pmm_manager;

static void check_alloc_page(void);
static void check_pgdir(void);
static void check_boot_pgdir(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void)
{
    pmm_manager = &default_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n)
{
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n)
{
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
void free_pages(struct Page *base, size_t n)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        pmm_manager->free_pages(base, n);
    }
    local_intr_restore(intr_flag);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void)
{
    size_t ret;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        ret = pmm_manager->nr_free_pages();
    }
    local_intr_restore(intr_flag);
    return ret;
}

// LAB2: DTB discovery and page descriptor initialization from lab3.
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

// pmm_init - setup a pmm to manage physical memory, build PDT&PT to setup
// paging mechanism
//         - check the correctness of pmm & paging mechanism, print PDT&PT
void pmm_init(void)
{
    // We need to alloc/free the physical memory (granularity is 4KB or other
    // size).
    // So a framework of physical memory manager (struct pmm_manager)is defined
    // in pmm.h
    // First we should init a physical memory manager(pmm) based on the
    // framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a
    // pmm
    check_alloc_page();
    // create boot_pgdir, an initial page directory(Page Directory Table, PDT)
    extern char boot_page_table_sv39[];
    boot_pgdir_va = (pte_t *)boot_page_table_sv39;
    boot_pgdir_pa = PADDR(boot_pgdir_va);

    // C now runs at its high virtual address, so drop the boot identity map.
    boot_pgdir_va[PDX1(DRAM_BASE)] = 0;
    flush_tlb();

    check_pgdir();

    static_assert(KERNBASE % PTSIZE == 0 && KERNTOP % PTSIZE == 0);

    // now the basic virtual memory map(see memalyout.h) is established.
    // check the correctness of the basic virtual memory map.
    check_boot_pgdir();

    kmalloc_init();
}

// get_pte - get pte and return the kernel virtual address of this pte for la
//        - if the PT contians this pte didn't exist, alloc a page for PT
// parameter:
//  pgdir:  the kernel virtual base address of PDT
//  la:     the linear address need to map
//  create: a logical value to decide if alloc a page for PT
// return vaule: the kernel virtual address of this pte
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create)
{
    const pte_t leaf_bits = PTE_R | PTE_W | PTE_X;
    pde_t *root_entry = &pgdir[PDX1(la)];
    struct Page *new_directory = NULL;
    pde_t *directory;

    if (*root_entry & PTE_V)
    {
        if (*root_entry & leaf_bits)
            return NULL;
        directory = KADDR(PDE_ADDR(*root_entry));
    }
    else
    {
        if (!create || (new_directory = alloc_page()) == NULL)
            return NULL;
        directory = page2kva(new_directory);
        memset(directory, 0, PGSIZE);
    }

    pde_t *entry = &directory[PDX0(la)];
    if (!(*entry & PTE_V))
    {
        struct Page *page = create ? alloc_page() : NULL;
        if (page == NULL)
        {
            if (new_directory != NULL)
                free_page(new_directory);
            return NULL;
        }
        memset(page2kva(page), 0, PGSIZE);
        set_page_ref(page, 1);
        *entry = ptd_create(page2ppn(page));
    }
    else if (*entry & leaf_bits)
    {
        return NULL;
    }

    // Publish a new directory only after its leaf table is ready.
    if (new_directory != NULL)
    {
        set_page_ref(new_directory, 1);
        *root_entry = ptd_create(page2ppn(new_directory));
    }

    return &((pte_t *)KADDR(PDE_ADDR(*entry)))[PTX(la)];
}

// get_page - get related Page struct for linear address la using PDT pgdir
struct Page *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store)
{
    pte_t *ptep = get_pte(pgdir, la, 0);
    if (ptep_store != NULL)
    {
        *ptep_store = ptep;
    }
    if (ptep != NULL && *ptep & PTE_V)
    {
        return pte2page(*ptep);
    }
    return NULL;
}

//LAB4 function1 2510670
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    pte_t *ptep = get_pte(pgdir, la, 1);
    if (ptep == NULL)
    {
        local_intr_restore(intr_flag);
        return -E_NO_MEM;
    }

    struct Page *old = (*ptep & PTE_V) ? pte2page(*ptep) : NULL;
    if (old != page)
        page_ref_inc(page);

    // Match the lab's default read access and the Sv39 W-implies-R rule.
    if ((perm & PTE_W) || !(perm & (PTE_R | PTE_X)))
        perm |= PTE_R;
    *ptep = pte_create(page2ppn(page), perm);
    tlb_invalidate(pgdir, la);

    if (old != NULL && old != page && page_ref_dec(old) == 0)
        free_page(old);
    local_intr_restore(intr_flag);
    return 0;
}

void page_remove(pde_t *pgdir, uintptr_t la)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    pte_t *ptep = get_pte(pgdir, la, 0);
    if (ptep != NULL && (*ptep & PTE_V))
    {
        struct Page *page = pte2page(*ptep);
        *ptep = 0;
        tlb_invalidate(pgdir, la);
        if (page_ref_dec(page) == 0)
            free_page(page);
    }
    local_intr_restore(intr_flag);
}

void tlb_invalidate(pde_t *pgdir, uintptr_t la)
{
    // Lab4 uses one hart and ASID zero. A full fence also covers changes
    // to non-leaf entries when get_pte creates a page-table path.
    flush_tlb();
}

static void check_alloc_page(void)
{
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}

static void check_pgdir(void)
{
    // assert(npage <= KMEMSIZE / PGSIZE);
    // The memory starts at 2GB in RISC-V
    // so npage is always larger than KMEMSIZE / PGSIZE
    size_t nr_free_store;

    nr_free_store = nr_free_pages();

    assert(npage <= KERNTOP / PGSIZE);
    assert(boot_pgdir_va != NULL && (uint32_t)PGOFF(boot_pgdir_va) == 0);
    assert(get_page(boot_pgdir_va, 0x0, NULL) == NULL);

    struct Page *p1, *p2;
    p1 = alloc_page();
    assert(page_insert(boot_pgdir_va, p1, 0x0, 0) == 0);

    pte_t *ptep;
    assert((ptep = get_pte(boot_pgdir_va, 0x0, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert(page_ref(p1) == 1);

    ptep = (pte_t *)KADDR(PDE_ADDR(boot_pgdir_va[0]));
    ptep = (pte_t *)KADDR(PDE_ADDR(ptep[0])) + 1;
    assert(get_pte(boot_pgdir_va, PGSIZE, 0) == ptep);

    p2 = alloc_page();
    assert(page_insert(boot_pgdir_va, p2, PGSIZE, PTE_U | PTE_W) == 0);
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
    assert(*ptep & PTE_U);
    assert(*ptep & PTE_W);
    assert((boot_pgdir_va[0] & (PTE_V | PTE_U)) == PTE_V);
    assert(page_ref(p2) == 1);

    assert(page_insert(boot_pgdir_va, p1, PGSIZE, 0) == 0);
    assert(page_ref(p1) == 2);
    assert(page_ref(p2) == 0);
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert((*ptep & PTE_U) == 0);

    page_remove(boot_pgdir_va, 0x0);
    assert(page_ref(p1) == 1);
    assert(page_ref(p2) == 0);

    page_remove(boot_pgdir_va, PGSIZE);
    assert(page_ref(p1) == 0);
    assert(page_ref(p2) == 0);

    assert(page_ref(pde2page(boot_pgdir_va[0])) == 1);

    pde_t *pd1 = boot_pgdir_va, *pd0 = page2kva(pde2page(boot_pgdir_va[0]));
    free_page(pde2page(pd0[0]));
    free_page(pde2page(pd1[0]));
    boot_pgdir_va[0] = 0;
    flush_tlb();

    assert(nr_free_store == nr_free_pages());

    cprintf("check_pgdir() succeeded!\n");
}

static void check_boot_pgdir(void)
{
    size_t nr_free_store;
    nr_free_store = nr_free_pages();

    // The kernel direct map is an Sv39 level-2 leaf (1 GiB).
    const uintptr_t span = (uintptr_t)1 << PDX1SHIFT;
    for (uintptr_t pa = PADDR(KERNBASE); pa < npage * PGSIZE; pa += PGSIZE)
    {
        uintptr_t va = (uintptr_t)KADDR(pa);
        pte_t entry = boot_pgdir_va[PDX1(va)];
        assert((entry & (PTE_V | PTE_R | PTE_W | PTE_X)) == READ_WRITE_EXEC);
        assert(PTE_ADDR(entry) + (va & (span - 1)) == pa);
    }

    assert(boot_pgdir_va[0] == 0);

    struct Page *p;
    p = alloc_page();
    assert(page_insert(boot_pgdir_va, p, 0x100, PTE_W | PTE_R) == 0);
    assert(page_ref(p) == 1);
    assert(page_insert(boot_pgdir_va, p, 0x100 + PGSIZE, PTE_W | PTE_R) == 0);
    assert(page_ref(p) == 2);

    const char *str = "ucore: Hello world!!";
    strcpy((void *)0x100, str);
    assert(strcmp((void *)0x100, (void *)(0x100 + PGSIZE)) == 0);

    *(char *)(page2kva(p) + 0x100) = '\0';
    assert(strlen((const char *)0x100) == 0);

    pde_t *pd1 = boot_pgdir_va, *pd0 = page2kva(pde2page(boot_pgdir_va[0]));
    page_remove(boot_pgdir_va, 0x100);
    page_remove(boot_pgdir_va, 0x100 + PGSIZE);
    free_page(pde2page(pd0[0]));
    free_page(pde2page(pd1[0]));
    boot_pgdir_va[0] = 0;
    flush_tlb();

    assert(nr_free_store == nr_free_pages());

    cprintf("check_boot_pgdir() succeeded!\n");
}

// perm2str - use string 'u,r,w,-' to present the permission
static const char *perm2str(int perm)
{
    static char str[4];
    str[0] = (perm & PTE_U) ? 'u' : '-';
    str[1] = 'r';
    str[2] = (perm & PTE_W) ? 'w' : '-';
    str[3] = '\0';
    return str;
}

// get_pgtable_items - In [left, right] range of PDT or PT, find a continuous
// linear addr space
//                  - (left_store*X_SIZE~right_store*X_SIZE) for PDT or PT
//                  - X_SIZE=PTSIZE=4M, if PDT; X_SIZE=PGSIZE=4K, if PT
// paramemters:
//  left:        no use ???
//  right:       the high side of table's range
//  start:       the low side of table's range
//  table:       the beginning addr of table
//  left_store:  the pointer of the high side of table's next range
//  right_store: the pointer of the low side of table's next range
//  return value: 0 - not a invalid item range, perm - a valid item range with
//  perm permission
static int get_pgtable_items(size_t left, size_t right, size_t start,
                             uintptr_t *table, size_t *left_store,
                             size_t *right_store)
{
    if (start >= right)
    {
        return 0;
    }
    while (start < right && !(table[start] & PTE_V))
    {
        start++;
    }
    if (start < right)
    {
        if (left_store != NULL)
        {
            *left_store = start;
        }
        int perm = (table[start++] & PTE_USER);
        while (start < right && (table[start] & PTE_USER) == perm)
        {
            start++;
        }
        if (right_store != NULL)
        {
            *right_store = start;
        }
        return perm;
    }
    return 0;
}
