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


// Page table address
static unsigned int MMUTABLEBASE;


// Indicates available frames
uint8* frames_occupation_table;

/******************************
9.6	    
*******************************/
void vmem_init()
{
	kheap_init();
	MMUTABLEBASE = init_kern_translation_table();

	//config MMU
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
	// allocate first level table
	uint32_t** first_level_table = (uint32_t**) kAlloc_aligned(FIRST_LVL_TT_SIZE, FIRST_LVL_TT_ALIG);
	// second level table
	uint32_t* second_level_table;
	
	// allocate second level table
	uint8_t i;
	for(i = 0; i < SECON_LVL_TT_COUN; i++)
	{
		first_level_table[i] = (uint32_t*) kAlloc_aligned(SECON_LVL_TT_SIZE, SECON_LVL_TT_ALIG);
	}
	
	//9.2
	uint32_t device_flags = 0b010000010110;
	
	uint32_t virtual_addr;
	
	// initialise memoire noyau
	for (virtual_addr = 0; virtual_addr < 0x1000000; virtual_addr++) // TODO 0x1000000 ==? __kernel_heap_end__
	{
		virtual_physical_mirror(virtual_addr, (uint32_t)first_level_table, second_level_table, 0b000001010010);
	}
	
	// initialise memoire peripherique
	for(virtual_addr = 0x20000000; virtual_addr < 0x20FFFFFF; virtual_addr++)
	{
		virtual_physical_mirror(virtual_addr, (uint32_t)first_level_table, second_level_table, device_flags);
	}
	return (unsigned int)first_level_table;
}


void virtual_physical_mirror(uint32_t virtual_addr, uint32_t first_level_table, uint32_t* second_level_table, uint32_t flags) {
	// Indexes
	uint32_t first_level_table_index;
	uint32_t second_level_table_index;
	// Desc
	uint32_t first_level_descriptor;
	uint32_t* first_level_descriptor_address;
	//uint32_t second_level_descriptor;
	uint32_t* second_level_descriptor_address;
	
	//Decalage de 20 bits pour recuperer first_level_table_index
	first_level_table_index = virtual_addr >> 20;
	//Construction du first_level_descriptor_address par concatenation, ajout de 00 a la fin
	first_level_descriptor_address = (uint32_t*) ((uint32_t)first_level_table | (first_level_table_index << 2));
	
	first_level_descriptor = *(first_level_descriptor_address);
	
	// On recupere second_level_table_index
	// Decalage de 12 bits pour retirer page index
	// Puis masque de 8 bits pour retirer first_level_table_index
	second_level_table_index = (virtual_addr >> 12) & 0xFF;
	// Masque pour recuperer les 22 premiers bits de first_level_table
	second_level_table = (uint32_t*)(first_level_descriptor & 0xFFFFFC00);
	// Construction de second_level_descriptor_address
	second_level_descriptor_address = (uint32_t*) ((uint32_t)second_level_table | (second_level_table_index << 2));
	// Construction de second_level_descriptor pour que virtual_addr == physical_addr
	*second_level_descriptor_address = (virtual_addr & 0xFFFFF000) | flags;
}


/**************************************************************************
9.8	    ALLOCATION PUIS INITIALISATION DE LA TABLE DES PAGES DE L'OS
**************************************************************************/


/******************************************************************************
Renvoie une adresse physique a partir d'une adresse virtuelle pour un process
*******************************************************************************/
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


/**************************************
			CONFIGURER LA MMU
**************************************/
void configure_mmu_C()
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
			ACTIVER LA MMU
**************************************/
void start_mmu_C()
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
