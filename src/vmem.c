#include "vmem.h"
#include "kheap.h"
#include "config.h"

// Page table address
static unsigned int MMUTABLEBASE;
static const uint32_t kernel_heap_end = (uint32_t) &__kernel_heap_end__;


/* FLAGS */
static const uint32_t DEVICE_FLAGS = 0b010000010110;
static const uint32_t KERNEL_FLAGS = 0b000001010010;
static const uint32_t FIRST_LEVEL_FLAGS = 0b0000000001;

void vmem_init()
{
	kheap_init();
	MMUTABLEBASE = init_kern_translation_table();
	//config MMU
	configure_mmu_C();
	//active MMU
	//vmem_translate(32768, NULL);
	start_mmu_C();
}

unsigned int init_kern_translation_table(void)
{
	// allocate first level table
	uint32_t* first_level_table = (uint32_t*) kAlloc_aligned(FIRST_LVL_TT_SIZE, FIRST_LVL_TT_ALIG);

	/* Second level table */
	uint32_t* second_level_table;
	
	
	/* Descriptors */
	uint32_t first_level_descriptor;
	uint32_t* first_level_descriptor_address;
	uint32_t second_level_descriptor;
	uint32_t* second_level_descriptor_address;

	/* Indexes */
	uint32_t first_level_table_index;
	uint32_t second_level_table_index;

	/* Debug */
	uint32_t debug_first_level_descriptor = 42;
	uint32_t* debug_first_level_descriptor_address = (uint32_t*)42;
	debug_first_level_descriptor++;
	debug_first_level_descriptor_address= (uint32_t*)debug_first_level_descriptor;
	debug_first_level_descriptor = (uint32_t)debug_first_level_descriptor_address;



	for(first_level_table_index = 0; first_level_table_index < FIRST_LVL_TT_COUN; first_level_table_index++)
	{
		// if we are between 0x0 and 0x1000000 or between 0x2000000 and 0x20FFFFFF
		if(first_level_table_index < 16) {
			// Allocate the second level
			second_level_table = (uint32_t*) kAlloc_aligned(SECON_LVL_TT_SIZE, SECON_LVL_TT_ALIG);

			// Browse the second level table and fill it
			for(second_level_table_index = 0; second_level_table_index < SECON_LVL_TT_COUN; second_level_table_index++) {
				// Build the physical address base on the virtual one
				uint32_t physical_addr = ((first_level_table_index<<8) | second_level_table_index) <<12;

				if(physical_addr <= kernel_heap_end)
				{
					second_level_descriptor = (physical_addr<<12) | KERNEL_FLAGS;
					second_level_descriptor_address = (uint32_t*) ((uint32_t)second_level_table | (second_level_table_index<<2));
					(*second_level_descriptor_address) = second_level_descriptor;
				}
				else
				{
					second_level_descriptor_address = (uint32_t*) ((uint32_t)second_level_table | (second_level_table_index<<2));
					(*second_level_descriptor_address) = 0;
				}
			}

			first_level_descriptor = ((uint32_t)second_level_table<<10) | FIRST_LEVEL_FLAGS;
			first_level_descriptor_address = (uint32_t*) ((uint32_t)first_level_table | (first_level_table_index<<2));
			(*first_level_descriptor_address) = first_level_descriptor;

		}
		else if(first_level_table_index > 512 && first_level_table_index < 528) {
			// Allocate the second level table but not attached to table 1 for now 
			second_level_table = (uint32_t*) kAlloc_aligned(SECON_LVL_TT_SIZE, SECON_LVL_TT_ALIG);

			// Browse the second level table and populate it with pysical adresses
			for(second_level_table_index = 0; second_level_table_index < SECON_LVL_TT_COUN; second_level_table_index++) {
				//build the physical address base on the virtual one
				uint32_t physical_addr = ((first_level_table_index<<8) | second_level_table_index) <<12;

				if(physical_addr >= 0x20000000 && physical_addr < 0x20FFFFFF)
				{
					second_level_descriptor = (physical_addr<<12) | DEVICE_FLAGS;
					second_level_descriptor_address = (uint32_t*) ((uint32_t)second_level_table | (second_level_table_index<<2));
					(*second_level_descriptor_address) = second_level_descriptor;

				} 
				else
				{
					second_level_descriptor_address = (uint32_t*) ((uint32_t)second_level_table | (second_level_table_index<<2));
					(*second_level_descriptor_address) = 0;
				}
			}

			first_level_descriptor = ((uint32_t)second_level_table<<10) | FIRST_LEVEL_FLAGS;
			first_level_descriptor_address = (uint32_t*) ((uint32_t)first_level_table | (first_level_table_index<<2));
			(*first_level_descriptor_address) = first_level_descriptor;

		}
		else {
			// Translation fault
			first_level_descriptor_address = (uint32_t*) ((uint32_t)first_level_table | (first_level_table_index<<2));
			(*first_level_descriptor_address) = 0;

		}
	}
	
	return (unsigned int)first_level_table;
}


uint32_t vmem_translate(uint32_t va, struct pcb_s* process)
{
	uint32_t pa; /* The result */
	
	/* 1st and 2nd table addresses */
	uint32_t table_base;
	uint32_t second_level_table;
	
	/* Indexes */
	uint32_t first_level_index;
	uint32_t second_level_index;
	uint32_t page_index;
	
	/* Descriptors */
	uint32_t first_level_descriptor;
	uint32_t* first_level_descriptor_address;
	uint32_t second_level_descriptor;
	uint32_t* second_level_descriptor_address;
	
	if (process == NULL)
	{
		__asm("mrc p15, 0, %[tb], c2, c0, 0" : [tb] "=r"(table_base));
	}
	else
	{
		table_base = (uint32_t) process->page_table;
	}
	table_base = table_base & 0xFFFFC000;
	
	/* Indexes */
	first_level_index = (va >> 20);
	second_level_index = ((va << 12) >> 24);
	page_index = (va & 0x00000FFF);
	
	/* First level descriptor */
	first_level_descriptor_address = (uint32_t*) (table_base | (first_level_index << 2));
	first_level_descriptor = *(first_level_descriptor_address);
	
	/* Translation fault */
	if(! (first_level_descriptor & 0x3)) {
		return (uint32_t) FORBIDDEN_ADDRESS_TABLE1;
	}
	
	/* Second level descriptor */
	second_level_table = first_level_descriptor & 0xFFFFFC00;
	second_level_descriptor_address = (uint32_t*) (second_level_table | (second_level_index << 2));
	second_level_descriptor = *((uint32_t*) second_level_descriptor_address);
	
	/* Translation fault */
	if (!(second_level_descriptor & 0x3)) {
		return (uint32_t) FORBIDDEN_ADDRESS;
	}
	
	/* Physical address */
	pa = (second_level_descriptor & 0xFFFFF000) | page_index;
	return pa;
}


void configure_mmu_C()
{
	register  unsigned  int  pt_addr  =  MMUTABLEBASE;
	//total++;
	/*  Translation  table  0  */
	__asm  volatile("mcr  p15,  0,  %[addr],  c2,  c0,  0"  :  :  [addr]  "r"  (pt_addr));
	/*  Translation  table  1  */
	__asm  volatile("mcr  p15,  0,  %[addr],  c2,  c0,  1"  :  :  [addr]  "r"  (pt_addr));
	/*  Use  translation  table  0  for  everything  */
	__asm  volatile("mcr  p15,  0,  %[n],  c2,  c0,  2"  :  :  [n]  "r"  (0));
	/*  Set  Domain  0  ACL  to  "Manager",  not  enforcing  memory  permissions
	*  Every  mapped  section/page  is  in  domain  0
	*/
	__asm  volatile("mcr  p15,  0,  %[r],  c3,  c0,  0"  :  :  [r]  "r"  (0x3));
}

void start_mmu_C()
{
	register  unsigned  int  control;
	__asm("mcr  p15,  0,  %[zero],  c1,  c0,  0"  :  :  [zero]  "r"(0)); //Disable  cache
	__asm("mcr  p15,  0,  r0,  c7,  c7,  0"); //Invalidate  cache  (data  and  instructions)  */
	__asm("mcr  p15,  0,  r0,  c8,  c7,  0"); //Invalidate  TLB  entries
	/*  Enable  ARMv6  MMU  features  (disable  sub-page  AP)  */
	control  =  (1<<23)  |  (1  <<  15)  |  (1  <<  4)  |  1;
	/*  Invalidate  the  translation  lookaside  buffer  (TLB)  */
	__asm  volatile("mcr  p15,  0,  %[data],  c8,  c7,  0"  :  :  [data]  "r"  (0));
	/*  Write  control  register  */
	__asm  volatile("mcr  p15,  0,  %[control],  c1,  c0,  0"  :  :  [control]  "r"  (control));
}
