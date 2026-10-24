#include <dtb.h>
#include <stdio.h>
#include <string.h>
#include <defs.h>
#include <memlayout.h>

// 设备树头部结构
struct fdt_header {
    uint32_t magic;          // 0xd00dfeed (big-endian)
    uint32_t totalsize;      // 整个DTB的大小
    uint32_t off_dt_struct;  // 结构体部分的偏移
    uint32_t off_dt_strings; // 字符串部分的偏移
    uint32_t off_mem_rsvmap; // 内存保留映射的偏移
    uint32_t version;        // DTB版本
    uint32_t last_comp_version; // 最后兼容版本
    uint32_t boot_cpuid_phys;   // 启动CPU的物理ID
    uint32_t size_dt_strings;   // 字符串部分大小
    uint32_t size_dt_struct;    // 结构体部分大小
};

// FDT token定义
#define FDT_BEGIN_NODE  0x00000001
#define FDT_END_NODE    0x00000002
#define FDT_PROP        0x00000003
#define FDT_NOP         0x00000004
#define FDT_END         0x00000009

// 字节序转换函数
static uint32_t fdt32_to_cpu(uint32_t x) {
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
}

static uint64_t fdt64_to_cpu(uint64_t x) {
    return ((uint64_t)fdt32_to_cpu(x & 0xffffffff) << 32) | 
           fdt32_to_cpu(x >> 32);
}

// 简化的内存信息提取函数
static int extract_memory_info(uintptr_t dtb_vaddr, const struct fdt_header *header, 
                              uint64_t *mem_base, uint64_t *mem_size) {
    uint32_t struct_offset = fdt32_to_cpu(header->off_dt_struct);
    uint32_t strings_offset = fdt32_to_cpu(header->off_dt_strings);
    
    const char *strings_base = (const char *)(dtb_vaddr + strings_offset);
    const uint32_t *struct_ptr = (const uint32_t *)(dtb_vaddr + struct_offset);
    
    int in_memory_node = 0;
    
    while (1) {
        uint32_t token = fdt32_to_cpu(*struct_ptr++);
        
        switch (token) {
            case FDT_BEGIN_NODE: {
                const char *name = (const char *)struct_ptr;
                int name_len = strlen(name);
                
                // 检查是否是memory节点
                if (strncmp(name, "memory", 6) == 0) {
                    in_memory_node = 1;
                }
                
                // 跳过节点名（4字节对齐）
                struct_ptr = (const uint32_t *)(((uintptr_t)struct_ptr + name_len + 4) & ~3);
                break;
            }
            
            case FDT_END_NODE:
                in_memory_node = 0;
                break;
                
            case FDT_PROP: {
                uint32_t prop_len = fdt32_to_cpu(*struct_ptr++);
                uint32_t prop_nameoff = fdt32_to_cpu(*struct_ptr++);
                const char *prop_name = strings_base + prop_nameoff;
                const void *prop_data = struct_ptr;
                
                // 在memory节点中查找reg属性
                if (in_memory_node && strcmp(prop_name, "reg") == 0 && prop_len >= 16) {
                    const uint64_t *reg_data = (const uint64_t *)prop_data;
                    *mem_base = fdt64_to_cpu(reg_data[0]);
                    *mem_size = fdt64_to_cpu(reg_data[1]);
                    return 0; // 成功找到
                }
                
                // 跳过属性数据（4字节对齐）
                struct_ptr = (const uint32_t *)(((uintptr_t)struct_ptr + prop_len + 3) & ~3);
                break;
            }
            
            case FDT_NOP:
                break;
                
            case FDT_END:
                return -1; // 没有找到
                
            default:
                return -1; // 错误
        }
    }
}

// 保存解析出的系统物理内存信息
static uint64_t memory_base = 0;
static uint64_t memory_size = 0;

void dtb_init(void) {
    cprintf("DTB Init\n");
    cprintf("HartID: %ld\n", boot_hartid);
    cprintf("DTB Address: 0x%lx\n", boot_dtb);
    
    if (boot_dtb == 0) {
        cprintf("Error: DTB address is null\n");
        return;
    }
    
    // 转换为虚拟地址
    uintptr_t dtb_vaddr = boot_dtb + PHYSICAL_MEMORY_OFFSET;
    const struct fdt_header *header = (const struct fdt_header *)dtb_vaddr;
    
    // 验证DTB
    uint32_t magic = fdt32_to_cpu(header->magic);
    if (magic != 0xd00dfeed) {
        cprintf("Error: Invalid DTB magic number: 0x%x\n", magic);
        return;
    }
    
    // 提取内存信息
    uint64_t mem_base, mem_size;
    if (extract_memory_info(dtb_vaddr, header, &mem_base, &mem_size) == 0) {
        cprintf("Physical Memory from DTB:\n");
        cprintf("  Base: 0x%016lx\n", mem_base);
        cprintf("  Size: 0x%016lx (%ld MB)\n", mem_size, mem_size / (1024 * 1024));
        cprintf("  End:  0x%016lx\n", mem_base + mem_size - 1);
        // 保存到全局变量，供 PMM 查询
        memory_base = mem_base;
        memory_size = mem_size;
    } else {
        cprintf("Warning: Could not extract memory info from DTB\n");
    }
    cprintf("DTB init completed\n");
}

uint64_t get_memory_base(void) {
    return memory_base;
}

uint64_t get_memory_size(void) {
    return memory_size;
}