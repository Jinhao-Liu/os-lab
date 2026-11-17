#include <defs.h>
#include <error.h>
#include <assert.h>
#include <pmm.h>
#include <proc.h>
#include <sched.h>
#include <stdio.h>

static void lab4_pass(const char *name)
{
    cprintf("[Test %s] PASS\n", name);
}

static void hold_free_pages(list_entry_t *held)
{
    list_init(held);
    while (nr_free_pages() > 1)
    {
        struct Page *page = alloc_page();
        assert(page != NULL);
        list_add(held, &page->page_link);
    }
    assert(nr_free_pages() == 1);
}

static void release_held_pages(list_entry_t *held)
{
    while (!list_empty(held))
    {
        list_entry_t *entry = list_next(held);
        list_del(entry);
        free_page(le2page(entry, page_link));
    }
}

static void test_page_table_rollback(void)
{
    const uintptr_t va = (uintptr_t)1 << PDX1SHIFT;
    size_t free_before = nr_free_pages();
    struct Page *target = alloc_page();
    assert(target != NULL && boot_pgdir_va[PDX1(va)] == 0);

    list_entry_t held;
    hold_free_pages(&held);
    assert(page_insert(boot_pgdir_va, target, va, PTE_R) == -E_NO_MEM);
    assert(boot_pgdir_va[PDX1(va)] == 0);
    assert(nr_free_pages() == 1 && page_ref(target) == 0);

    struct Page *last = alloc_page();
    assert(last != NULL && nr_free_pages() == 0);
    assert(page_insert(boot_pgdir_va, target, va, PTE_R) == -E_NO_MEM);
    page_remove(boot_pgdir_va, va);
    assert(boot_pgdir_va[PDX1(va)] == 0 && page_ref(target) == 0);

    free_page(last);
    release_held_pages(&held);
    free_page(target);
    assert(nr_free_pages() == free_before);
    lab4_pass("Page table allocation rollback");
}

void test_lab4_pmm_basic(void)
{
    struct Page *p1 = alloc_page();
    struct Page *p2 = alloc_page();
    struct Page *p3 = alloc_page();
    pte_t *ptep;
    size_t free_before;

    assert(p1 != NULL && p2 != NULL && p3 != NULL);

    assert(page_insert(boot_pgdir_va, p1, 0, 0) == 0);
    assert((ptep = get_pte(boot_pgdir_va, 0, 0)) != NULL);
    assert((*ptep & PTE_V) && pte2page(*ptep) == p1 && page_ref(p1) == 1);

    assert(page_insert(boot_pgdir_va, p2, PGSIZE, PTE_U | PTE_W) == 0);
    assert(page_insert(boot_pgdir_va, p1, PGSIZE, 0) == 0);
    assert(page_ref(p1) == 2 && page_ref(p2) == 0);
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
    assert(pte2page(*ptep) == p1 && (*ptep & (PTE_U | PTE_W)) == 0);

    assert(page_insert(boot_pgdir_va, p3, 2 * PGSIZE, 0) == 0);
    free_before = nr_free_pages();
    assert(page_insert(boot_pgdir_va, p3, 2 * PGSIZE, PTE_W) == 0);
    assert(page_ref(p3) == 1 && nr_free_pages() == free_before);

    page_remove(boot_pgdir_va, 0);
    page_remove(boot_pgdir_va, PGSIZE);
    page_remove(boot_pgdir_va, 2 * PGSIZE);
    page_remove(boot_pgdir_va, 2 * PGSIZE);
    assert(page_ref(p1) == 0 && page_ref(p2) == 0 && page_ref(p3) == 0);

    test_page_table_rollback();
    lab4_pass("PMM");
}

void test_lab4_process_create(void)
{
    assert(current == idleproc);
    assert(idleproc != NULL && idleproc->pid == 0);
    assert(idleproc->state == PROC_RUNNABLE);
    assert(idleproc->kstack == (uintptr_t)bootstack);
    assert(idleproc->parent == NULL && idleproc->mm == NULL);
    assert(idleproc->tf == NULL && idleproc->runs == 0);
    assert(idleproc->flags == 0 && idleproc->context.ra == 0);

    assert(initproc != NULL && initproc->pid == 1);
    assert(initproc->state == PROC_RUNNABLE);
    assert(initproc->parent == idleproc && initproc->mm == NULL);
    assert(initproc->context.ra != 0 && initproc->context.sp != 0);

    size_t free_before = nr_free_pages();
    list_entry_t held;
    hold_free_pages(&held);
    struct trapframe unused = {0};
    assert(do_fork(0, 0, &unused) == -E_NO_MEM);
    assert(nr_free_pages() == 1 && find_proc(2) == NULL);
    assert(list_next(&proc_list) == &initproc->list_link);
    assert(list_next(&initproc->list_link) == &proc_list);
    release_held_pages(&held);
    assert(nr_free_pages() == free_before);
    lab4_pass("Fork allocation rollback");
    lab4_pass("Process Create");
}

static volatile int test_thread1_ran;
static volatile int test_thread2_ran;

static int __attribute__((noreturn)) test_lab4_thread(void *arg)
{
    int id = (int)(intptr_t)arg;
    int i;
    for (i = 0; i < 3; i++)
    {
        if (id == 1) test_thread1_ran = 1;
        else test_thread2_ran = 1;
        current->need_resched = 1;
        schedule();
    }
    while (1)
    {
        current->need_resched = 1;
        schedule();
    }
}

void test_lab4_schedule(void)
{
    int i;
    test_thread1_ran = test_thread2_ran = 0;
    int pid1 = kernel_thread(test_lab4_thread, (void *)(intptr_t)1, 0);
    int pid2 = kernel_thread(test_lab4_thread, (void *)(intptr_t)2, 0);
    assert(pid1 > 0 && pid2 > 0 && pid1 != pid2);
    struct proc_struct *first = find_proc(pid1), *second = find_proc(pid2);
    assert(first != NULL && second != NULL);
    assert(first->kstack + KSTACKSIZE <= second->kstack ||
           second->kstack + KSTACKSIZE <= first->kstack);

    for (i = 0; i < 8; i++)
    {
        current->need_resched = 1;
        schedule();
    }
    assert(test_thread1_ran && test_thread2_ran);
    lab4_pass("Schedule");
}
