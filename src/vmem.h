#ifndef VMEM_H
#define VMEM_H

#include <stdint.h>
#include "sched.h"

//**************************************************** PAGE TABLE
#define PAGE_SIZE 4096 // 4KB 
#define TT_ENTRY_SIZE 4 //1 case = 32 bits

#define FIRST_LVL_TT_COUN 4096
#define FIRST_LVL_TT_SIZE (FIRST_LVL_TT_COUN * TT_ENTRY_SIZE) // 16384
#define FIRST_LVL_TT_ALIG 10 //TODO : WHY ?

#define SECON_LVL_TT_COUN 256
#define SECON_LVL_TT_SIZE  (SECON_LVL_TT_COUN * TT_ENTRY_SIZE) // 1024
#define SECON_LVL_TT_ALIG 14 //TODO : WHY ?

//**************************************************** FRAME TABLE
#define FRAME_SIZE 4096
#define MEMORY_SPACE 0x20FFFFFF
#define FRAMES_OCCUPATION_TABLE_SIZE (MEMORY_SPACE / FRAME_SIZE) // 135 168

#define frame_kernel_heap_end (__kernel_heap_end__ / FRAME_SIZE)
#define frame_devices_start (0x20000000 / FRAME_SIZE)
#define frame_devices_end (0x20FFFFFF / FRAME_SIZE)

#define FRAME_OCCUPIED 1
#define FRAME_FREE 0

extern uint32_t __kernel_heap_end__;

unsigned int init_kern_translation_table();
void virtual_physical_mirror(uint32_t virtual_addr, uint32_t first_level_table, uint32_t* second_level_table, uint32_t flags);
void vmem_init();
uint32_t vmem_translate(uint32_t va, struct pcb_s* process);
void configure_mmu_C();
void start_mmu_C();

void init_frames_occupation_table(uint32_t FRAMES_OCCUPATION_TABLE_SIZE);

#endif
