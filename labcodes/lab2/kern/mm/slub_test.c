#include <slub.h>
#include <pmm.h>
#include <list.h>
#include <stdio.h>
#include <string.h>

static void
check_pattern(const void *ptr, size_t size, unsigned char pattern) {
    const unsigned char *bytes = ptr;
    for (size_t i = 0; i < size; i ++) {
        assert(bytes[i] == pattern);
    }
}

static void
check_sizes(void) {
    const size_t sizes[] = {
        1, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129,
        255, 256, 257, 511, 512, 513, 1023, 1024, 1025, 2047,
        2048, 2049, 4031, 4032, 4033, 4095, 4096, 4097, 8193,
    };
    void *objects[sizeof(sizes) / sizeof(sizes[0])];
    size_t initial = nr_free_pages();
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i ++) {
        objects[i] = kmalloc(sizes[i]);
        assert(objects[i] != NULL && (uintptr_t)objects[i] % SLUB_ALIGN == 0);
        for (size_t j = 0; j < i; j ++) {
            assert((uintptr_t)objects[i] + sizes[i] <= (uintptr_t)objects[j] ||
                   (uintptr_t)objects[j] + sizes[j] <= (uintptr_t)objects[i]);
        }
        memset(objects[i], (char)(i + 1), sizes[i]);
    }
    for (size_t parity = 0; parity < 2; parity ++) {
        for (size_t i = parity; i < sizeof(sizes) / sizeof(sizes[0]); i += 2) {
            check_pattern(objects[i], sizes[i], (unsigned char)(i + 1));
            kfree(objects[i]);
        }
    }
    assert(nr_free_pages() == initial);
    cprintf("slub_check: size boundaries, alignment and data isolation passed\n");
}

static void
check_reuse(void) {
    size_t initial = nr_free_pages();
    void *first = kmalloc(24), *second = kmalloc(24);
    assert(first != NULL && second != NULL);
    assert(nr_free_pages() == initial - 1);
    memset(second, 0x5a, 24);
    kfree(first);
    void *reused = kmalloc(24);
    assert(reused == first && nr_free_pages() == initial - 1);
    memset(reused, 0x33, 24);
    check_pattern(second, 24, 0x5a);
    kfree(second);
    kfree(reused);
    assert(nr_free_pages() == initial);
    cprintf("slub_check: shared slabs, object reuse and page return passed\n");
}

static void
check_full_slabs(void) {
    void *objects[300];
    size_t initial = nr_free_pages();
    for (size_t i = 0; i < 300; i ++) {
        objects[i] = kmalloc(16);
        assert(objects[i] != NULL);
        memset(objects[i], (char)(i % 251 + 1), 16);
    }
    assert(nr_free_pages() == initial - 2);
    kfree(objects[0]);
    void *reused = kmalloc(16);
    assert(reused == objects[0]);
    assert(nr_free_pages() == initial - 2);
    memset(reused, 1, 16);
    for (size_t i = 0; i < 300; i ++) {
        check_pattern(objects[i], 16, (unsigned char)(i % 251 + 1));
        kfree(objects[i]);
    }
    assert(nr_free_pages() == initial);
    cprintf("slub_check: full-to-partial transitions and multi-slab release passed\n");
}

static void
check_exhaustion(void) {
    size_t initial = nr_free_pages();
    void *anchor = kmalloc(32);
    assert(anchor != NULL);
    memset(anchor, 0x5a, 32);

    list_entry_t held;
    list_init(&held);
    struct Page *page;
    while ((page = alloc_page()) != NULL) {
        list_add(&held, &(page->page_link));
    }
    assert(nr_free_pages() == 0);
    void *shared = kmalloc(32);
    assert(shared != NULL && nr_free_pages() == 0);
    memset(shared, 0x33, 32);
    assert(kmalloc(2048) == NULL);
    assert(kmalloc(PGSIZE + 1) == NULL);
    check_pattern(anchor, 32, 0x5a);
    check_pattern(shared, 32, 0x33);
    kfree(anchor);
    kfree(shared);
    assert(nr_free_pages() == 1);
    void *recovered = kmalloc(2048);
    assert(recovered != NULL && nr_free_pages() == 0);
    memset(recovered, 0x77, 2048);
    check_pattern(recovered, 2048, 0x77);
    kfree(recovered);

    while (!list_empty(&held)) {
        list_entry_t *le = list_next(&held);
        page = le2page(le, page_link);
        list_del(le);
        free_page(page);
    }
    assert(nr_free_pages() == initial);
    cprintf("slub_check: page exhaustion, cached allocation and recovery passed\n");
}

void
slub_check(void) {
    size_t initial = nr_free_pages();
    assert(kmalloc(0) == NULL);
    assert(kmalloc((size_t)-1) == NULL);
    assert(kmalloc((size_t)-1 - PGSIZE) == NULL);
    kfree(NULL);
    assert(nr_free_pages() == initial);
    check_sizes();
    check_reuse();
    check_full_slabs();
    check_exhaustion();
    assert(nr_free_pages() == initial);
    cprintf("slub_check() succeeded!\n");
}
