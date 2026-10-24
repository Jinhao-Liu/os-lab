#include <buddy_pmm.h>
#include <list.h>
#include <stdio.h>

void
buddy_check(void) {
    size_t initial = nr_free_pages();
    assert(initial >= 64);
    assert(alloc_pages(initial + 1) == NULL);
    assert(alloc_pages((size_t)-1) == NULL);
    assert(nr_free_pages() == initial);

    const size_t requests[] = {1, 2, 3, 5, 8, 17};
    const size_t sizes[] = {1, 2, 4, 8, 8, 32};
    struct Page *blocks[6];
    size_t used = 0;
    for (size_t i = 0; i < 6; i ++) {
        blocks[i] = alloc_pages(requests[i]);
        assert(blocks[i] != NULL);
        assert((page2ppn(blocks[i]) & (sizes[i] - 1)) == 0);
        assert(page2ppn(blocks[i]) + sizes[i] <= npage);
        for (size_t j = 0; j < sizes[i]; j ++) {
            assert(!PageReserved(blocks[i] + j));
            assert(!PageProperty(blocks[i] + j));
            assert(page_ref(blocks[i] + j) == 0);
        }
        for (size_t j = 0; j < i; j ++) {
            assert(blocks[i] + sizes[i] <= blocks[j] ||
                   blocks[j] + sizes[j] <= blocks[i]);
        }
        used += sizes[i];
        assert(nr_free_pages() == initial - used);
    }
    for (size_t i = 6; i > 0; i --) {
        free_pages(blocks[i - 1], requests[i - 1]);
        used -= sizes[i - 1];
        assert(nr_free_pages() == initial - used);
    }
    cprintf("buddy_check: rounding, alignment and non-overlap passed\n");

    // Hold every available page, using the allocated pages' unused list links.
    list_entry_t allocated;
    list_init(&allocated);
    size_t count = 0;
    struct Page *p;
    while ((p = alloc_page()) != NULL) {
        assert(page2ppn(p) >= nbase && page2ppn(p) < npage);
        assert(!PageReserved(p) && !PageProperty(p) && page_ref(p) == 0);
        set_page_ref(p, 1);
        list_add(&allocated, &(p->page_link));
        count ++;
        assert(count <= initial);
    }
    assert(count == initial && nr_free_pages() == 0);
    assert(alloc_pages(2) == NULL);
    cprintf("buddy_check: exhaustion and complete pool coverage passed\n");

    // Find eight owned pages aligned to an eight-page physical boundary.
    struct Page *region = NULL;
    for (list_entry_t *le = list_next(&allocated); le != &allocated;
         le = list_next(le)) {
        p = le2page(le, page_link);
        if ((page2ppn(p) & 7) != 0 || page2ppn(p) + 8 > npage) {
            continue;
        }
        size_t i = 0;
        while (i < 8 && page_ref(p + i) == 1 && !PageReserved(p + i)) {
            i ++;
        }
        if (i == 8) {
            region = p;
            break;
        }
    }
    assert(region != NULL);
    for (size_t i = 0; i < 8; i ++) {
        list_del(&(region[i].page_link));
        set_page_ref(region + i, 0);
    }

    // Pages 1 and 2 are adjacent, but their order-0 buddies are 0 and 3.
    free_page(region + 1);
    free_page(region + 2);
    assert(nr_free_pages() == 2);
    assert(alloc_pages(2) == NULL);
    assert(nr_free_pages() == 2);
    free_page(region);
    free_page(region + 3);
    for (size_t i = 4; i < 8; i ++) {
        free_page(region + i);
    }
    assert(nr_free_pages() == 8);
    assert(alloc_pages(8) == region);
    assert(nr_free_pages() == 0);
    cprintf("buddy_check: non-buddies stay separate and buddies coalesce passed\n");

    free_pages(region, 8);
    struct Page *first = alloc_pages(3), *second = alloc_pages(3);
    assert(first != NULL && second != NULL);
    assert((first == region && second == region + 4) ||
           (second == region && first == region + 4));
    assert(nr_free_pages() == 0);
    free_pages(second, 3);
    assert(nr_free_pages() == 4);
    free_pages(first, 3);
    assert(nr_free_pages() == 8);
    assert(alloc_pages(8) == region);
    free_pages(region, 8);
    cprintf("buddy_check: recursive splitting and rounded release passed\n");

    while (!list_empty(&allocated)) {
        list_entry_t *le = list_next(&allocated);
        p = le2page(le, page_link);
        list_del(le);
        set_page_ref(p, 0);
        free_page(p);
    }
    assert(nr_free_pages() == initial);
    cprintf("buddy_check() succeeded!\n");
}
