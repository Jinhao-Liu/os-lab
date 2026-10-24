#include <buddy_pmm.h>
#include <list.h>

static list_entry_t free_lists[BUDDY_MAX_ORDER + 1];
static size_t nr_free;

static unsigned int
buddy_order(size_t n) {
    unsigned int order = 0;
    while (((size_t)1 << order) < n) {
        order ++;
    }
    return order;
}

static void
buddy_init(void) {
    for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; order ++) {
        list_init(&free_lists[order]);
    }
    nr_free = 0;
}

// LAB2: 2510547 李佩霖 Buddy System 算法

static void
buddy_insert_free_block(struct Page *base, unsigned int order) {
    while (order < BUDDY_MAX_ORDER) {
        size_t count = (size_t)1 << order;
        ppn_t buddy_ppn = page2ppn(base) ^ count;
        if (buddy_ppn < nbase || buddy_ppn + count > npage) {
            break;
        }
        struct Page *buddy = pages + (buddy_ppn - nbase);
        if (PageReserved(buddy) || !PageProperty(buddy) ||
            buddy->property != count) {
            break;
        }
        list_del(&(buddy->page_link));
        ClearPageProperty(buddy);
        buddy->property = 0;
        if (buddy < base) {
            base = buddy;
        }
        order ++;
    }
    base->property = (size_t)1 << order;
    SetPageProperty(base);
    list_add(&free_lists[order], &(base->page_link));
}

static void
buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0 && base >= pages && base < pages + (npage - nbase));
    assert(n <= npage - page2ppn(base));
    for (struct Page *p = base; p != base + n; p ++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    nr_free += n;

    // Partition arbitrary ranges into physically aligned power-of-two blocks.
    while (n > 0) {
        unsigned int order = 0;
        while (order < BUDDY_MAX_ORDER) {
            size_t next_size = (size_t)1 << (order + 1);
            if (next_size > n || (page2ppn(base) & (next_size - 1)) != 0) {
                break;
            }
            order ++;
        }
        size_t count = (size_t)1 << order;
        buddy_insert_free_block(base, order);
        base += count;
        n -= count;
    }
}

static struct Page *
buddy_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free || n > ((size_t)1 << BUDDY_MAX_ORDER)) {
        return NULL;
    }
    unsigned int order = buddy_order(n), current = order;
    while (current <= BUDDY_MAX_ORDER && list_empty(&free_lists[current])) {
        current ++;
    }
    if (current > BUDDY_MAX_ORDER) {
        return NULL;
    }

    struct Page *base = le2page(list_next(&free_lists[current]), page_link);
    list_del(&(base->page_link));
    ClearPageProperty(base);
    while (current > order) {
        current --;
        struct Page *buddy = base + ((size_t)1 << current);
        buddy->property = (size_t)1 << current;
        SetPageProperty(buddy);
        list_add(&free_lists[current], &(buddy->page_link));
    }
    base->property = (size_t)1 << order;
    nr_free -= base->property;
    return base;
}

static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0 && n <= ((size_t)1 << BUDDY_MAX_ORDER));
    unsigned int order = buddy_order(n);
    size_t count = (size_t)1 << order;
    assert(base >= pages && base < pages + (npage - nbase));
    assert(count <= npage - page2ppn(base));
    assert((page2ppn(base) & (count - 1)) == 0);
    assert(!PageReserved(base) && !PageProperty(base));
    assert(base->property == count);
    for (struct Page *p = base; p != base + count; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    nr_free += count;
    buddy_insert_free_block(base, order);
}

static size_t
buddy_nr_free_pages(void) {
    return nr_free;
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
