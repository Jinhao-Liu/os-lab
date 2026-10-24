#include <buddy_pmm.h>
#include <stdio.h>
#include <string.h>

extern int puts(const char *);
extern int vprintf(const char *, va_list);
extern void exit(int) __attribute__((noreturn));

#undef offsetof
#define offsetof(type, member) __builtin_offsetof(type, member)
#include "../kern/mm/buddy_pmm.c"
#include "../kern/mm/buddy_pmm_test.c"

static struct Page descriptors[1 << BUDDY_MAX_ORDER];
struct Page *pages = descriptors;
const size_t nbase = DRAM_BASE / PGSIZE;
size_t npage;
uint64_t va_pa_offset;
const struct pmm_manager *pmm_manager = &buddy_pmm_manager;

int cprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    return vprintf(fmt, ap);
}
void __panic(const char *file, int line, const char *fmt, ...) {
    cprintf("Assertion at %s:%d: ", file, line);
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    puts("");
    exit(77);
}
void __warn(const char *file, int line, const char *fmt, ...) {}
struct Page *alloc_pages(size_t n) { return pmm_manager->alloc_pages(n); }
void free_pages(struct Page *base, size_t n) { pmm_manager->free_pages(base, n); }
size_t nr_free_pages(void) { return pmm_manager->nr_free_pages(); }

static void reset_pool(size_t n) {
    npage = nbase + n;
    memset(descriptors, 0, sizeof(descriptors));
    for (size_t i = 0; i < n; i++) SetPageReserved(pages + i);
    pmm_manager->init();
}

static unsigned int random_state = 9876543;
static unsigned int next_random(void) {
    random_state = random_state * 1664525U + 1013904223U;
    return random_state;
}

static void verify_state(const unsigned char *state) {
    unsigned char covered[513] = {0};
    size_t total = 0;
    for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; order++) {
        size_t count = (size_t)1 << order, blocks = 0;
        list_entry_t *previous = &free_lists[order];
        for (list_entry_t *le = list_next(previous); le != &free_lists[order];
             le = list_next(le)) {
            assert(++blocks <= 513);
            assert(list_prev(le) == previous);
            struct Page *p = le2page(le, page_link);
            assert(page2ppn(p) + count <= npage);
            assert((page2ppn(p) & (count - 1)) == 0);
            assert(PageProperty(p) && p->property == count);
            for (size_t i = 0; i < count; i++) {
                size_t index = p - pages + i;
                assert(index < 513 && state[index] == 0 && !covered[index]);
                covered[index] = 1;
            }
            total += count;
            previous = le;
        }
        assert(list_prev(&free_lists[order]) == previous);
    }
    size_t expected = 0;
    for (size_t i = 0; i < 513; i++) {
        assert(covered[i] == (state[i] == 0));
        assert(PageReserved(pages + i) == (state[i] == 1));
        expected += state[i] == 0;
    }
    assert(total == expected && nr_free_pages() == expected);
}

int main(int argc, char **argv) {
    reset_pool(513);
    pmm_manager->init_memmap(pages + 3, 509);
    if (argc > 1) {
        struct Page *p = alloc_pages(3);
        assert(p != NULL);
        if (strcmp(argv[1], "zero-alloc") == 0) alloc_pages(0);
        else if (strcmp(argv[1], "wrong-size") == 0) free_pages(p, 5);
        else if (strcmp(argv[1], "interior-free") == 0) free_page(p + 1);
        else if (strcmp(argv[1], "reserved-free") == 0) free_page(pages);
        else if (strcmp(argv[1], "double-free") == 0) {
            free_pages(p, 3);
            free_pages(p, 3);
        }
        return 1;
    }
    pmm_manager->check();
    assert(nr_free_pages() == 509);
    puts("PASS: kernel self-tests on a misaligned non-power-of-two pool");

    reset_pool(513);
    pmm_manager->init_memmap(pages + 3, 13);
    pmm_manager->init_memmap(pages + 16, 32);
    pmm_manager->init_memmap(pages + 100, 37);
    assert(nr_free_pages() == 82 && alloc_pages(32) == NULL);
    pmm_manager->init_memmap(pages + 48, 16);
    assert(alloc_pages(32) == pages + 32);
    free_pages(pages + 32, 32);
    assert(nr_free_pages() == 98);
    reset_pool(513);
    pmm_manager->init_memmap(pages + 512, 1);
    assert(alloc_page() == pages + 512);
    free_page(pages + 512);
    pmm_manager->init_memmap(pages, 1);
    pmm_manager->init_memmap(pages + 1, 1);
    assert(alloc_pages(2) == pages);
    free_pages(pages, 2);
    assert(nr_free_pages() == 3);
    puts("PASS: disjoint ranges, reserved holes, adjacent registration and bounds");

    size_t maximum = (size_t)1 << BUDDY_MAX_ORDER;
    reset_pool(maximum);
    pmm_manager->init_memmap(pages, maximum);
    assert(alloc_pages(maximum + 1) == NULL);
    assert(alloc_pages(maximum) == pages && nr_free_pages() == 0);
    free_pages(pages, maximum);
    struct Page *small = alloc_page(), *rounded = alloc_pages(5);
    assert(nr_free_pages() == maximum - 9);
    free_pages(rounded, 5);
    free_page(small);
    assert(alloc_pages(maximum) == pages);
    free_pages(pages, maximum);
    puts("PASS: highest order, deep splitting and complete merging");

    reset_pool(513);
    pmm_manager->init_memmap(pages + 3, 97);
    pmm_manager->init_memmap(pages + 120, 392);
    unsigned char state[513];
    for (size_t i = 0; i < 513; i++)
        state[i] = (i >= 3 && i < 100) || (i >= 120 && i < 512) ? 0 : 1;
    struct {struct Page *page; size_t request, count;} live[513];
    size_t live_count = 0;
    for (size_t step = 0; step < 4000; step++) {
        if (live_count && next_random() % 3 == 0) {
            size_t slot = next_random() % live_count;
            struct Page *p = live[slot].page;
            free_pages(p, live[slot].request);
            for (size_t i = 0; i < live[slot].count; i++) state[p - pages + i] = 0;
            live[slot] = live[--live_count];
        } else {
            size_t request = next_random() % 20 + 1, count = 1;
            while (count < request) count *= 2;
            int possible = 0;
            for (size_t start = 0; start + count <= 513; start++) {
                if ((nbase + start) % count) continue;
                size_t i = 0;
                while (i < count && state[start + i] == 0) i++;
                if (i == count) {possible = 1; break;}
            }
            struct Page *p = alloc_pages(request);
            assert((p != NULL) == possible);
            if (p != NULL) {
                size_t start = p - pages;
                assert(start + count <= 513 && page2ppn(p) % count == 0);
                for (size_t i = 0; i < count; i++) {
                    assert(state[start + i] == 0);
                    state[start + i] = 2;
                }
                live[live_count].page = p;
                live[live_count].request = request;
                live[live_count++].count = count;
            }
        }
        verify_state(state);
    }
    while (live_count) {
        live_count--;
        free_pages(live[live_count].page, live[live_count].request);
    }
    assert(nr_free_pages() == 489);
    puts("PASS: 4000 allocations/releases against an independent page bitmap");
    return 0;
}
