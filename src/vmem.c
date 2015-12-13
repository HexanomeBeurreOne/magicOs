#include "vmem.h"
#include "kheap.h"
#include "config.h"


// 9.2
// 0  (nG : global?)
// 0  (S : partagé?)
// 0  (APX : optimiser le remplacement des pages?)
// 001 (TEX : politique de cache)
// 11 (AP :  lecture/ecriture)
// 0  (C : periph mappé en mém) 
// 0  (B : periph mappé en mém) 
// 1  (extended small page)
// 0  (XN : code non exécutable)
 
// 0b000001110010 

// 9.3 : Bits d'entree tables 2eme niv
uint32_t device_flags = 0b000001110010;

void vmem_init()
{
	init_kern_translation_table();
	configure_mmu_C();
	//__asm("cps 0b10111");//Activate data abort and interruptions : bit 7-8 of cpsr
	//TODO Later
	start_mmu_C();
}


/**************************************************************************
9.5	    ALLOCATION PUIS INITIALISATION DE LA TABLE DES PAGES DE L'OS
**************************************************************************/
unsigned int init_kern_translation_table(void)
{
	int fl_index; //first level index
	uint32_t * fl_page_entry = (uint32_t*)FIRST_LVL_TABLE_BASE; //first level

	for(fl_index = 0 ; fl_index < FIRST_LVL_TT_COUN ; fl_index++)
	{
		int sl_index; //second level index
		uint32_t* sl_page_entry= (uint32_t*)(SECON_LVL_TABLE_BASE + fl_index* SECON_LVL_TT_SIZE);

		for(sl_index = 0 ; sl_index < SECON_LVL_TT_COUN ; sl_index++)
		{
			uint32_t physical_addr = ((fl_index<<8) + sl_index) <<12; //build the 20 first bits of virtual address

			if(physical_addr < __kernel_heap_end__ && physical_addr > 0)
			{
				*sl_page_entry = physical_addr; //| memory_flags;
			} 
			else if(physical_addr > 0x20000000 && physical_addr < 0x20FFFFFF)
			{
				*sl_page_entry = physical_addr | device_flags;
			} 
			else
			{
				*sl_page_entry = 0 ; // translation fault
			}

			sl_page_entry++;
		}
		uint32_t addr = SECON_LVL_TABLE_BASE + fl_index* SECON_LVL_TT_SIZE; //see fig 9.4
		*fl_page_entry = (uint32_t)((addr) | fl_flags);

		fl_page_entry++;
	}

 	return 0;
}

/**************************************
			ACTIVER LA MMU
**************************************/
void start_mmu_C()
{
register unsigned int control;
__asm("mcr p15, 0, %[zero], c1, c0, 0" : : [zero] "r"(0)); //Disable cache
__asm("mcr p15, 0, r0, c7, c7, 0"); //Invalidate cache (data and instructions) */
__asm("mcr p15, 0, r0, c8, c7, 0"); //Invalidate TLB entries
/* Enable ARMv6 MMU features (disable sub-page AP) */
control = (1<<23) | (1 << 15) | (1 << 4) | 1;
/* Invalidate the translation lookaside buffer (TLB) */
__asm volatile("mcr p15, 0, %[data], c8, c7, 0" : : [data] "r" (0));
/* Write control register */
__asm volatile("mcr p15, 0, %[control], c1, c0, 0" : : [control] "r" (control));
}

/**************************************
			CONFIGURER LA MMU
**************************************/
void configure_mmu_C()
{
register unsigned int pt_addr = FIRST_LVL_TABLE_BASE;
//total++; //why?
/* Translation table 0 */
__asm volatile("mcr p15, 0, %[addr], c2, c0, 0" : : [addr] "r" (pt_addr));
/* Translation table 1 */
__asm volatile("mcr p15, 0, %[addr], c2, c0, 1" : : [addr] "r" (pt_addr));
/* Use translation table 0 for everything */
__asm volatile("mcr p15, 0, %[n], c2, c0, 2" : : [n] "r" (0));
/* Set Domain 0 ACL to "Manager", not enforcing memory permissions
* Every mapped section/page is in domain 0
*/
__asm volatile("mcr p15, 0, %[r], c3, c0, 0" : : [r] "r" (0x3));
}
