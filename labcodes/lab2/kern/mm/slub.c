#include <slub.h>
#include <pmm.h>
#include <list.h>

#define SLUB_CACHE_COUNT 8

struct slub_cache {
    size_t object_size;
    size_t objects;
    list_entry_t partial;
};

struct slub_page {
    struct slub_cache *cache;
    struct Page *page;
    void *freelist;
    size_t inuse;
    size_t npages;
    list_entry_t link;
};

#define SLUB_HEADER_SIZE ROUNDUP(sizeof(struct slub_page), SLUB_ALIGN)

static struct slub_cache caches[SLUB_CACHE_COUNT];
static bool initialized;

// LAB2: 2512739 徐乙涵 SLUB 分配算法

void
slub_init(void) {
    assert(!initialized);
    for (size_t i = 0; i < SLUB_CACHE_COUNT; i ++) {
        caches[i].object_size = (size_t)SLUB_ALIGN << i;
        caches[i].objects = (PGSIZE - SLUB_HEADER_SIZE) / caches[i].object_size;
        list_init(&caches[i].partial);
    }
    initialized = 1;
}

void *
kmalloc(size_t size) {
    assert(initialized);
    if (size == 0 || size > (size_t)-1 - SLUB_HEADER_SIZE) {
        return NULL;
    }

    size_t index = 0;
    while (index < SLUB_CACHE_COUNT && caches[index].object_size < size) {
        index ++;
    }
    if (index == SLUB_CACHE_COUNT) {
        size_t total = size + SLUB_HEADER_SIZE;
        size_t npages = total / PGSIZE + (total % PGSIZE != 0);
        struct Page *page = alloc_pages(npages);
        if (page == NULL) {
            return NULL;
        }
        struct slub_page *slab = (void *)(page2pa(page) + va_pa_offset);
        slab->cache = NULL;
        slab->page = page;
        slab->npages = npages;
        return (char *)slab + SLUB_HEADER_SIZE;
    }

    struct slub_cache *cache = &caches[index];
    if (list_empty(&cache->partial)) {
        struct Page *page = alloc_page();
        if (page == NULL) {
            return NULL;
        }
        struct slub_page *slab = (void *)(page2pa(page) + va_pa_offset);
        slab->cache = cache;
        slab->page = page;
        slab->freelist = NULL;
        slab->inuse = 0;
        slab->npages = 1;
        // Free objects store their next pointer in their own payload.
        for (size_t i = cache->objects; i > 0; i --) {
            void *object = (char *)slab + SLUB_HEADER_SIZE +
                           (i - 1) * cache->object_size;
            *(void **)object = slab->freelist;
            slab->freelist = object;
        }
        list_add(&cache->partial, &(slab->link));
    }

    struct slub_page *slab = to_struct(list_next(&cache->partial),
                                       struct slub_page, link);
    void *object = slab->freelist;
    slab->freelist = *(void **)object;
    slab->inuse ++;
    if (slab->freelist == NULL) {
        list_del(&(slab->link));
    }
    return object;
}

void
kfree(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    assert(initialized);
    struct slub_page *slab = (void *)ROUNDDOWN((uintptr_t)ptr, PGSIZE);
    struct slub_cache *cache = slab->cache;
    if (cache == NULL) {
        assert(ptr == (char *)slab + SLUB_HEADER_SIZE);
        free_pages(slab->page, slab->npages);
        return;
    }

    uintptr_t offset = (uintptr_t)ptr - (uintptr_t)slab - SLUB_HEADER_SIZE;
    assert(offset < cache->objects * cache->object_size);
    assert(offset % cache->object_size == 0 && slab->inuse > 0);
    if (slab->freelist == NULL) {
        list_add(&cache->partial, &(slab->link));
    }
    *(void **)ptr = slab->freelist;
    slab->freelist = ptr;
    slab->inuse --;
    if (slab->inuse == 0) {
        list_del(&(slab->link));
        free_page(slab->page);
    }
}
