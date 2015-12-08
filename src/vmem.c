#include "vmem.h"

#define PAGE_SIZE 4096
#define SECON_LVL_TT_COUN 256
#define SECON_LVL_TT_SIZE 256
#define FIRST_LVL_TT_COUN 4096
#define FIRST_LVL_TT_SIZE 4096

#define __kernel_heap_end__ 0x10000000

// champ de bit entree table 2 page read/write for all
// 1XXXXXXXXXXXXXXXXXXX010000110111
// 0x20000437


if(adresse_physique < 0x20FFFFFF && adresse_physique > 0x20000000) {
	uint32_t device_flags = adresse_physique << 3;
}

unsigned int init_kern_translation_table(void) {
	
}
