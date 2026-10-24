#ifndef __KERN_MM_SLUB_H__
#define __KERN_MM_SLUB_H__

#include <defs.h>

#define SLUB_ALIGN 16

// Single-hart allocator: initialize once after pmm_init().
void slub_init(void);

// Returns SLUB_ALIGN-aligned storage, or NULL for zero size/failure.
void *kmalloc(size_t size);

// Accepts NULL or the exact address of a live kmalloc allocation.
void kfree(void *ptr);

void slub_check(void);

#endif /* !__KERN_MM_SLUB_H__ */
