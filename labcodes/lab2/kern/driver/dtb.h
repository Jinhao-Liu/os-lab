#ifndef __KERN_DRIVER_DTB_H__
#define __KERN_DRIVER_DTB_H__

#include <defs.h>

// Defined in entry.S
extern uint64_t boot_hartid;
extern uint64_t boot_dtb;

void dtb_init(void);
uint64_t get_memory_base(void);
uint64_t get_memory_size(void);

#endif /* !__KERN_DRIVER_DTB_H__ */