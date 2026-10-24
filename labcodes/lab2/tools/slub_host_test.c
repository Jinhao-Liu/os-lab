#include <buddy_pmm.h>
#include <slub.h>
#include <stdio.h>
#include <string.h>

extern int puts(const char *);
extern int vprintf(const char *, va_list);
extern void exit(int) __attribute__((noreturn));

#undef offsetof
#define offsetof(type, member) __builtin_offsetof(type, member)
#include "../kern/mm/buddy_pmm.c"
#include "../kern/mm/buddy_pmm_test.c"
#include "../kern/mm/slub.c"
#include "../kern/mm/slub_test.c"

#define HOST_PAGES 1024
static struct Page descriptors[HOST_PAGES];
static unsigned char memory[HOST_PAGES * PGSIZE]
    __attribute__((aligned(PGSIZE)));
struct Page *pages = descriptors;
const size_t nbase = DRAM_BASE / PGSIZE;
size_t npage = DRAM_BASE / PGSIZE + HOST_PAGES;
uint64_t va_pa_offset;
const struct pmm_manager *pmm_manager = &buddy_pmm_manager;

int cprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    return vprintf(fmt, ap);
}
void __panic(const char *file, int line, const char *fmt, ...) {
    cprintf("FAIL at %s:%d: ", file, line);
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    puts("");
    exit(1);
}
void __warn(const char *file, int line, const char *fmt, ...) {}
struct Page *alloc_pages(size_t n) { return pmm_manager->alloc_pages(n); }
void free_pages(struct Page *base, size_t n) { pmm_manager->free_pages(base, n); }
size_t nr_free_pages(void) { return pmm_manager->nr_free_pages(); }

static unsigned int random_state = 135792468;
static unsigned int next_random(void) {
    random_state = random_state * 1664525U + 1013904223U;
    return random_state;
}

int main(void) {
    assert((uintptr_t)memory % PGSIZE == 0);
    va_pa_offset = (uintptr_t)memory - DRAM_BASE;
    for (size_t i = 0; i < HOST_PAGES; i++) SetPageReserved(pages + i);
    pmm_manager->init();
    pmm_manager->init_memmap(pages + 3, HOST_PAGES - 4);
    pmm_manager->check();
    slub_init();
    slub_check();
    size_t initial = nr_free_pages();
    assert(initial == HOST_PAGES - 4);

    const size_t large_sizes[] = {2049, 4032, 4033, 4097, 8193};
    const size_t actual_pages[] = {1, 1, 2, 2, 4};
    for (size_t i = 0; i < 5; i++) {
        void *object = kmalloc(large_sizes[i]);
        assert(object != NULL);
        assert(nr_free_pages() == initial - actual_pages[i]);
        memset(object, 0x5a, large_sizes[i]);
        check_pattern(object, large_sizes[i], 0x5a);
        kfree(object);
        assert(nr_free_pages() == initial);
    }
    assert(kmalloc((size_t)-1 - 32) == NULL);
    assert(kmalloc((size_t)-1 - 128) == NULL);
    assert(nr_free_pages() == initial);
    puts("PASS: large-object page accounting, payload boundaries and overflow");

    const size_t sizes[] = {
        1, 15, 16, 17, 24, 31, 32, 33, 63, 64, 65, 127, 128,
        129, 255, 256, 257, 511, 512, 513, 1023, 1024, 1025,
        2047, 2048, 2049, 4032, 4033, 4096, 4097, 8193, 16385,
    };
    struct {void *ptr; size_t size; unsigned char pattern;} live[64] = {0};
    for (size_t step = 0; step < 2000; step++) {
        size_t slot = next_random() % 64;
        if (live[slot].ptr != NULL) {
            check_pattern(live[slot].ptr, live[slot].size, live[slot].pattern);
            kfree(live[slot].ptr);
            live[slot].ptr = NULL;
        }
        if (next_random() % 3 != 0) {
            size_t size = sizes[next_random() % (sizeof(sizes) / sizeof(sizes[0]))];
            void *ptr = kmalloc(size);
            assert(ptr != NULL && (uintptr_t)ptr % SLUB_ALIGN == 0);
            assert((uintptr_t)ptr >= (uintptr_t)memory + 3 * PGSIZE);
            assert((uintptr_t)ptr + size <= (uintptr_t)memory + (HOST_PAGES - 1) * PGSIZE);
            for (size_t i = 0; i < 64; i++) {
                if (live[i].ptr != NULL) {
                    assert((uintptr_t)ptr + size <= (uintptr_t)live[i].ptr ||
                           (uintptr_t)live[i].ptr + live[i].size <= (uintptr_t)ptr);
                }
            }
            live[slot].ptr = ptr;
            live[slot].size = size;
            live[slot].pattern = next_random() >> 24;
            memset(ptr, (char)live[slot].pattern, size);
        }
        for (size_t i = 0; i < 64; i++) {
            if (live[i].ptr != NULL)
                check_pattern(live[i].ptr, live[i].size, live[i].pattern);
        }
    }
    for (size_t i = 0; i < 64; i++) kfree(live[i].ptr);
    assert(nr_free_pages() == initial);
    puts("PASS: 2000 mixed object operations with byte-by-byte data checks");
    puts("PASS: all object and backing-page allocations reclaimed");
    return 0;
}
