#ifndef __KERN_MM_BUDDY_PMM_H__
#define __KERN_MM_BUDDY_PMM_H__

#include <pmm.h>

// 128 MiB / 4 KiB = 2^15 pages in the current physical memory layout.
#define BUDDY_MAX_ORDER 15

// Requests round up to a power of two. Free the block with the original n.
// Page.property records the block size at both free and allocated block heads;
// PG_property distinguishes free heads from allocated heads and interior pages.
extern const struct pmm_manager buddy_pmm_manager;

void buddy_check(void);

#endif /* !__KERN_MM_BUDDY_PMM_H__ */
