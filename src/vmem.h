#ifndef VMEM_H
#define VMEM_H

#include <stdint.h>
#include "sched.h"

#define PAGE_SIZE 4096 // 4KB 
#define TT_ENTRY_SIZE 4 //1 case

#define FIRST_LVL_TT_COUN 4096
#define FIRST_LVL_TT_SIZE (FIRST_LVL_TT_COUN * TT_ENTRY_SIZE) // 16384
#define FIRST_LVL_TT_ALIG 10 //TODO : WHY ?

#define SECON_LVL_TT_COUN 256
#define SECON_LVL_TT_SIZE  (SECON_LVL_TT_COUN * TT_ENTRY_SIZE) // 1024
#define SECON_LVL_TT_ALIG 14 //TODO : WHY ?

#define FIRST_LVL_TABLE_BASE 0X1000000 //start of the user space, end of the kernel heap
#define SECON_LVL_TABLE_BASE (FIRST_LVL_TABLE_BASE + FIRST_LVL_TT_SIZE)


extern uint32_t __kernel_heap_end__;

unsigned int init_kern_translation_table();
void virtual_physical_mirror(uint32_t virtual_addr, uint32_t first_level_table, uint32_t* second_level_table, uint32_t flags);
void vmem_init();
uint32_t vmem_translate(uint32_t va, struct pcb_s* process);
void configure_mmu_C();
void start_mmu_C();

#endif
