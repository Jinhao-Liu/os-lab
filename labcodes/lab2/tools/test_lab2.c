#include <defs.h>
#include <stdio.h>
#include <string.h>
#include <pmm.h>
#include <best_fit_pmm.h>
#include <memlayout.h>
#include <mmu.h>
#include <dtb.h>
#include <test_lab2.h>

void
test_page_init(void)
{
    extern char end[];
    uint64_t mem_base = get_memory_base();
    uint64_t mem_size = get_memory_size();
    uintptr_t end_pa;

    /* 1. va_pa_offset must be set to the virtual/physical mapping offset */
    if (va_pa_offset != PHYSICAL_MEMORY_OFFSET) {
        cprintf("[Test 1] Page Init: FAIL (va_pa_offset)\n");
        return;
    }

    /* 2. the device tree must report a valid physical memory range */
    if (mem_base == 0 || mem_size == 0) {
        cprintf("[Test 1] Page Init: FAIL (dtb mem 0)\n");
        return;
    }

    /* 3. npage must equal the page number of the reported memory end,
     *    and the upper bound must exceed the DRAM start page number */
    {
        uint64_t mem_end = mem_base + mem_size;
        size_t expect_npage = PPN(mem_end);
        if (npage != expect_npage || npage <= nbase) {
            cprintf("[Test 1] Page Init: FAIL (npage=%lu expect=%lu)\n",
                    (unsigned long)npage, (unsigned long)expect_npage);
            return;
        }
    }

    /* 4. pages must be non-null, page-aligned, and located after the kernel `end` */
    if (pages == NULL) {
        cprintf("[Test 1] Page Init: FAIL (pages NULL)\n");
        return;
    }
    if (((uintptr_t)pages % PGSIZE) != 0) {
        cprintf("[Test 1] Page Init: FAIL (pages not page-aligned)\n");
        return;
    }
    if ((uintptr_t)pages < (uintptr_t)ROUNDUP((uintptr_t)end, PGSIZE)) {
        cprintf("[Test 1] Page Init: FAIL (pages before kernel end)\n");
        return;
    }

    /* 5. the free area must have been built: some free pages, not more than total */
    {
        size_t free_now = nr_free_pages();
        if (free_now == 0 || free_now > (npage - nbase)) {
            cprintf("[Test 1] Page Init: FAIL (nr_free=%lu)\n",
                    (unsigned long)free_now);
            return;
        }
    }

    /* 6. the kernel-image tail page must not be handed to the allocator */
    end_pa = PADDR((uintptr_t)end);
    if (PPN(end_pa) >= npage) {
        cprintf("[Test 1] Page Init: FAIL (end pa out of range)\n");
        return;
    }
    if (!PageReserved(pa2page(end_pa))) {
        cprintf("[Test 1] Page Init: FAIL (kernel tail not reserved)\n");
        return;
    }

    /* 7. the pages occupied by the page-descriptor array must stay reserved too */
    {
        uintptr_t pages_pa = PADDR((uintptr_t)pages);
        if (PPN(pages_pa) >= npage) {
            cprintf("[Test 1] Page Init: FAIL (pages pa out of range)\n");
            return;
        }
        if (!PageReserved(pa2page(pages_pa))) {
            cprintf("[Test 1] Page Init: FAIL (pages array not reserved)\n");
            return;
        }
    }

    /* 8. page2pa / pa2page round-trip must be correct */
    {
        struct Page *q = pa2page(end_pa);
        if (pa2page(page2pa(q)) != q) {
            cprintf("[Test 1] Page Init: FAIL (roundtrip)\n");
            return;
        }
    }

    cprintf("[Test 1] Page Init: PASS\n");
}

void
test_best_fit(void)
{
    if (pmm_manager != &best_fit_pmm_manager) {
        cprintf("[Test 2] Best-Fit: FAIL (pmm_manager=%s)\n", pmm_manager->name);
        return;
    }
    cprintf("[Test 2] Best-Fit: PASS\n");
}
