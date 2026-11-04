#include <pmm.h>
#include <list.h>
#include <string.h>
#include <best_fit_pmm.h>
#include <stdio.h>

// LAB2: 链表按物理地址排序，每个空闲块仅将首描述符的 page_link 挂入链表。
// 分配时遍历比较块大小；nr_free 统计空闲页总数，不是链表节点数。
static free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

static void best_fit_check(void);

static void
best_fit_init(void) {
    // 循环双向链表的哨兵指向自身；此时仅有空管理结构，尚未登记可用页面。
    list_init(&free_list);
    nr_free = 0;
}

//2510670 刘晋豪 bestfit算法

// 归还 n 个连续物理页，base 是首描述符；调用者必须保证释放范围合法。
static void
best_fit_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    // 不接受保留页或已有空闲块首；这些断言并不是完整的非法释放检测器。
    for (struct Page *p = base; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    // 只有块首设置 PageProperty 并记录页数，内部页不单独入链。
    base->property = n;
    SetPageProperty(base);
    // 本次新归还 n 页；后面的合并只改变组织方式，不再增加页数。
    nr_free += n;

    // 找到第一个地址更大的块；同一 Page 数组的先后顺序对应物理页号顺序。
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        if (base < le2page(le, page_link)) {
            break;
        }
    }
    // 遍历回到哨兵时就是尾插；空链表也可使用同一个插入操作。
    list_add_before(le, &(base->page_link));

    // 检查前块的排他末尾是否等于当前起点，链表相邻不等于物理相邻。
    le = list_prev(&(base->page_link));
    if (le != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p + p->property == base) {
            p->property += base->property;
            // 旧块首成为内部页，其 property 可残留旧值，不能脱离标记解释。
            ClearPageProperty(base);
            list_del(&(base->page_link));
            // 后续与后块比较时，必须使用合并后的新块首。
            base = p;
        }
    }

    // 再检查后块，可将“前空闲块 + 本次归还块 + 后空闲块”合为一个大块。
    le = list_next(&(base->page_link));
    if (le != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
    }
}

static void
best_fit_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    // page_init() 先把描述符全部设为保留；初次登记时先解除这些页的保留状态。
    for (struct Page *p = base; p != base + n; p ++) {
        assert(PageReserved(p));
        p->flags = 0;
    }
    // 将初次交付页面视为一次回收，复用元数据清理、有序插入和相邻合并逻辑。
    best_fit_free_pages(base, n);
}

// 返回连续 n 页的首描述符；访问实际内存还需要 page2pa() 和虚实地址换算。
static struct Page *
best_fit_alloc_pages(size_t n) {
    assert(n > 0);
    // 总量不足立即失败；总量足够时仍可能因外部碎片而找不到连续 n 页。
    if (n > nr_free) {
        return NULL;
    }

    // page 保存目前最小的合适块；链表按地址排序，不能遇到第一个合适块就停。
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        // 链表节点嵌在 Page 中，用 le2page 找回描述符以读取该空闲块的页数。
        struct Page *p = le2page(le, page_link);
        if (p->property >= n &&
            (page == NULL || p->property < page->property)) {
            page = p;
            // 恰好匹配时剩余为零，不可能再找到更小的合适块。
            if (p->property == n) {
                break;
            }
        }
    }

    if (page != NULL) {
        // 摘链前保存前驱，剩余块稍后放回原位置，维持地址顺序。
        list_entry_t *prev = list_prev(&(page->page_link));
        // list_del 只修改链表关系，描述符和原块长度仍可继续使用。
        list_del(&(page->page_link));
        if (page->property > n) {
            // 前 n 页交给调用者；page + n 前进 n 个描述符，代表物理地址前进 n 页。
            struct Page *p = page + n;
            p->property = page->property - n;
            SetPageProperty(p);
            list_add(prev, &(p->page_link));
        }
        // Best-Fit 精确占用 n 页；引用计数由上层用途管理，这里不自动设为 1。
        nr_free -= n;
        page->property = 0;
        ClearPageProperty(page);
    }
    return page;
}

static size_t
best_fit_nr_free_pages(void) {
    return nr_free;
}

// 将策略接入统一页分配接口；只有 pmm.c 选中此管理器时，公共接口才会调用它。
const struct pmm_manager best_fit_pmm_manager = {
    .name = "best_fit_pmm_manager",
    .init = best_fit_init,
    .init_memmap = best_fit_init_memmap,
    .alloc_pages = best_fit_alloc_pages,
    .free_pages = best_fit_free_pages,
    .nr_free_pages = best_fit_nr_free_pages,
    .check = best_fit_check,
};

static void
basic_check(void) {
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    assert(!list_empty(&free_list));

    struct Page *p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

// LAB2: below code is used to check the best fit allocation algorithm (your EXERCISE 1) 
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
static void
best_fit_check(void) {
    int score = 0 ,sumscore = 6;
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count ++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n",score, sumscore);
    #endif
    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n",score, sumscore);
    #endif
    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n",score, sumscore);
    #endif
    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    // * - - * -
    free_pages(p0 + 1, 2);
    free_pages(p0 + 4, 1);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 1) && p0[1].property == 2);
    // * - - * *
    assert((p1 = alloc_pages(1)) != NULL);
    assert(alloc_pages(2) != NULL);      // best fit feature
    assert(p0 + 4 == p1);

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n",score, sumscore);
    #endif
    p2 = p0 + 1;
    free_pages(p0, 5);
    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n",score, sumscore);
    #endif
    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count --, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n",score, sumscore);
    #endif
}


