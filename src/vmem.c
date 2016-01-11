#include "vmem.h"
#include "kheap.h"
#include "config.h"
#include <stdint.h>

// 9.2
// 0  (nG : global?)
// 0  (S : partagé?)
// 0  (APX : optimiser le remplacement des pages?)
// 001 (TEX : politique de cache)
// 11 (AP :  lecture/ecriture)
// 0  (C : periph mappé en mém) 
// 0  (B : periph mappé en mém) 
// 1  (extended small page)
// 1  (XN : code non exécutable)
 
// 0b000001110011 

/* FLAGS */
static const uint32_t DEVICE_FLAGS = 0b010000010110;
static const uint32_t KERNEL_FLAGS = 0b000001010010;
static const uint32_t FIRST_LEVEL_FLAGS = 0b0000000001;

// Page table address
static unsigned int MMUTABLEBASE;
static const uint32_t kernel_heap_end = (uint32_t) &__kernel_heap_end__;

// Indicates available frames
uint8_t* frames_occupation_table;

static uint32_t frame_kernel_heap_end = (16777215 / 4096);



/******************************
9.6	    
*******************************/
void vmem_init()
{
	kheap_init();

	MMUTABLEBASE = init_kern_translation_table();
	frames_occupation_table = init_frames_occupation_table();
	
	//config MMU
	configure_mmu_C();
	//active MMU
	vmem_translate(32768, NULL);
	vmem_translate(0x1000001, NULL);
	start_mmu_C();

	//__asm("cps 0b10111");//Activate data abort and interruptions : bit 7-8 of cpsr
	//TODO Later
	uint32_t f = find_available_frame();
	f = f*FRAME_SIZE;
	
	uint32_t* li = list_available_frames(3);
	uint32_t frame_i = 0;
	frame_i = frame_i;
	li = li;
}


/**************************************************************************
9.5	    ALLOCATION PUIS INITIALISATION DE LA TABLE DES PAGES DE L'OS
**************************************************************************/
unsigned int init_kern_translation_table(void)
{

	/* **** Constants **** */
	const uint8_t FIRST_LVL_IDX_BEGIN = 20;
	const uint8_t SECOND_LVL_IDX_BEGIN = 12;
	const uint8_t SECOND_LVL_IDX_LEN = 0xFF;
	
	const uint32_t SECOND_LVL_ADDR_MASK = 0xFFFFFC00; // last 10 bits to 0

	const uint8_t nb_tables_kernel_device = (kernel_heap_end / (PAGE_SIZE * SECON_LVL_TT_COUN)) + 1; // For kernel & device, we have 0xFFFFFF addresse to store, 16 = 0xFFFFFF / (RAME_SIZE[4096] * SECON_LVL_TT_COUN[256])
	const uint16_t device_address_page_table_idx_start = 0x20000000 >> 20;
	const uint16_t device_address_page_table_idx_end = (0x20000000 >> 20) + 16;
	// Alloc page table
	uint32_t** page_table = (uint32_t**) kAlloc_aligned(FIRST_LVL_TT_SIZE, FIRST_LVL_TT_ALIG);
	// Put kern and devices pages for all processes or for kernel
	uint32_t* first_level_descriptor_address;
	uint32_t* second_level_descriptor_address;
	uint32_t first_lvl_idx, first_lvl_desc, second_lvl_idx;
	uint32_t log_addr, i;
	
	// ** Init kernel pages
	// Alloc table pages for kernel (from 0 to 0xffffff (kernel_heap_end, included))
	for(i = 0; i < nb_tables_kernel_device; i++)
	{
		first_level_descriptor_address = (uint32_t*) ((uint32_t)page_table | (i << 2));
		(*first_level_descriptor_address) = (uint32_t) kAlloc_aligned(SECON_LVL_TT_SIZE, SECON_LVL_TT_ALIG) | FIRST_LEVEL_FLAGS;
	}
	// Fill second level tables for kernel
	for (log_addr = 0; log_addr < kernel_heap_end; log_addr += PAGE_SIZE)
	{
		first_lvl_idx = log_addr >> FIRST_LVL_IDX_BEGIN;
		first_level_descriptor_address = (uint32_t*) ((uint32_t)page_table | (first_lvl_idx << 2));

		first_lvl_desc = (*first_level_descriptor_address) & SECOND_LVL_ADDR_MASK;
		second_lvl_idx = (log_addr >> SECOND_LVL_IDX_BEGIN) & SECOND_LVL_IDX_LEN;

		second_level_descriptor_address = (uint32_t*) (first_lvl_desc | (second_lvl_idx << 2));

        *second_level_descriptor_address = (log_addr & 0xFFFFF000) | KERNEL_FLAGS;
	}
	
		
	// ** Init devices pages
	// Alloc table pages for devices (from 0x20000000 to 0x20ffffff)
	for(i = device_address_page_table_idx_start; i < device_address_page_table_idx_end; i++) 
	{
		first_level_descriptor_address = (uint32_t*) ((uint32_t)page_table | (i << 2));
		(*first_level_descriptor_address) = (uint32_t) kAlloc_aligned(SECON_LVL_TT_SIZE, SECON_LVL_TT_ALIG) | FIRST_LEVEL_FLAGS;
	}
	// Fill second level tables for devices I/O
	for(log_addr = 0x20000000; log_addr < 0x20FFFFFF; log_addr += PAGE_SIZE)
    {
        first_lvl_idx = log_addr >> FIRST_LVL_IDX_BEGIN;
		first_level_descriptor_address = (uint32_t*) ((uint32_t)page_table | (first_lvl_idx << 2));

		first_lvl_desc = (*first_level_descriptor_address) & SECOND_LVL_ADDR_MASK;
		second_lvl_idx = (log_addr >> SECOND_LVL_IDX_BEGIN) & SECOND_LVL_IDX_LEN;

		second_level_descriptor_address = (uint32_t*) (first_lvl_desc | (second_lvl_idx << 2));

        *second_level_descriptor_address = (log_addr & 0xFFFFF000) | DEVICE_FLAGS;
    }
	
	return (unsigned int)page_table;
	
}


/**
* Renvoie la table des pages de second niveau à partir d'un index
* du premier niveau et d'une table de pages ou l'initialise si elle
* n'existe pas
*/
uint32_t* get_second_level_page_table(uint32_t* page_table, uint32_t fl_index)
{
	// tables de niv 1 et 2
	uint32_t* first_level_page_table;
	uint32_t* second_level_page_table;

    // cf 9.4 on récupère la Translation base
    first_level_page_table = (uint32_t*) (*page_table & 0xFFFFC000);

    // First level descriptors
	uint32_t first_level_descriptor;
    uint32_t* first_level_descriptor_address;

    first_level_descriptor_address = (uint32_t*) (*first_level_page_table | (fl_index << 2));
    first_level_descriptor = *(first_level_descriptor_address);

    /* Translation fault*/
	if (!(first_level_descriptor & 0x3))  
	{
		// Il faut allouer une nouvelle table
		second_level_page_table = (uint32_t*)kAlloc_aligned(SECON_LVL_TT_SIZE, SECON_LVL_TT_ALIG);

		// Initialisons les entrées de la table
		for (int i = 0; i < SECON_LVL_TT_COUN; ++i)
		{
			second_level_page_table[i]=0;
		}

		// On indique l'emplacement de cette nouvelle table au 1er niveau
		page_table[fl_index] = (uint32_t)second_level_page_table | FIRST_LVL_FLAGS;
	}

	second_level_page_table = (uint32_t*) (first_level_descriptor & 0xFFFFFC00);

	return second_level_page_table;
}


/**
* Trouve un espace contigue de blocs libres parmi toutes les pages virtuelles
* possible sur 32 bits et renvoie le num de la premiere page de l'espace trouvé
* renvoie uint32_max si impossible
*/
uint32_t find_contiguous_pages(uint32_t* page_table, uint32_t nb_page)
{
	const uint32_t MAX_PAGE = 0x1FFFFFF;

	uint32_t current_contiguous_pages = 0;

	uint32_t first_level_index;
	uint32_t second_level_index;

	for (int page_i = 0x1000000; page_i < MAX_PAGE; page_i+=PAGE_SIZE)
	{
		// On récupère les indexes
		first_level_index = (page_i >> 20);
		second_level_index = ((page_i << 12) >> 24);

		if (is_available(page_table, first_level_index, second_level_index))
		{
		    current_contiguous_pages++;
		}
		else
		{
		    current_contiguous_pages = 0;
		}

		// On a le bon espace contigu
		if (current_contiguous_pages >= nb_page)
		{
		    return page_i + 1 - nb_page;
		}
	}

	// Il est impossible de trouver un tel espace !
	return UINT32_MAX;
}


/**
* Indique si la page virtuelle est dispo pour une table de page donnée
* renvoie 1 si libre, 0 sinon
*/
int is_available(uint32_t* page_table, uint32_t first_level_index, uint32_t second_level_index)
{
	uint32_t* second_level_table = get_second_level_page_table(page_table, first_level_index);

	if(second_level_table[second_level_index] != 0)
	{
		// occupée
		return 0;
	}

	// page libre !
	return 1;
}

/**************************************************************************
9.8	    ALLOCATION PUIS INITIALISATION DE LA TABLE DES PAGES DE L'OS
**************************************************************************/
/**
* Initialise la table d'occupation des frames
* On utilise la figure 3.4, la partie FREE est grisée
*/
uint8_t* init_frames_occupation_table()
{
	//On alloue la table d'occupation des frames.
	uint8_t* frames_table = (uint8_t*) kAlloc(FRAMES_OCCUPATION_TABLE_SIZE);

	//On marque les frames qui sont occupées (1) et celles libres (0)
	uint32_t i;
	uint32_t de = frame_devices_start;
	uint32_t ds = frame_devices_end;
	ds = ds;
	de = de;

	for (i = 0; i <= frame_kernel_heap_end; i++)
	{
		frames_table[i] = FRAME_OCCUPIED;
	}
	for (; i < frame_devices_start; i++)
	{
		frames_table[i] = FRAME_FREE;
	}
	for (i = frame_devices_start; i <= frame_devices_end; i++)
	{
		frames_table[i] = FRAME_OCCUPIED;
	}
	
	return frames_table;
}

/**
* Libère la table d'occupation des frames
*/
void free_frames_occupation_table()
{
	kFree(frames_occupation_table, FRAMES_OCCUPATION_TABLE_SIZE);
}



/**************************************************************************
9.11-13			    ALLOUE PLUSIEURS PAGES A UN PROCESS
**************************************************************************/
/**
 * +size : nb d'octets qu'il faut allouer
 */
uint8_t* vmem_alloc_for_userland(struct pcb_s* process, unsigned int size)
{
	uint32_t first_level_index;
	uint32_t second_level_index;

	// Nb de pages nécessaire arrondi au nb supérieur
	uint32_t nb_page = get_nb_page(size);

	uint32_t* frame_available_list;

	// On cherche un espace contigue correspondant
	uint32_t contiguous_pages = find_contiguous_pages(*(process->page_table), nb_page);

	if (contiguous_pages < UINT32_MAX)
	{
		// Espace trouvé !
		
		// On cherche si on a assez de frames dispos
		frame_available_list = list_available_frames(nb_page);
		if (frame_available_list == NULL)
		{
			//Pas assez de frames...
			return 0;
		}

		// On a assez de frames et de pages virtuelles !
		// On alloue une frame pour chaque page
		uint32_t frame_i = 0;
		for (uint32_t page_i = contiguous_pages; page_i < (contiguous_pages + nb_page); ++page_i)
		{
			// On récupère les indexes d'une page
			first_level_index = page_i / SECON_LVL_TT_COUN;
			second_level_index = page_i - first_level_index * SECON_LVL_TT_COUN;

			// Ajout de l'adresse de la frame
			add_frame_page_table(*(process->page_table), first_level_index, second_level_index, frame_available_list[frame_i]);

			++frame_i;
		}

	}
	else
	{
		// pas d'espace virtuel contigu assez grand
		return 0;
	}
	kFree((uint8_t*)frame_available_list, nb_page);

	return (uint8_t*)(contiguous_pages * PAGE_SIZE);
}

/**
 * Libère les pages allouées par vmem_alloc_for_userland
 */
void vmem_free(uint8_t* vAddress, struct pcb_s* process, unsigned int size)
{
	uint32_t first_level_index;
	uint32_t second_level_index;

	//Calculons le nombre de pages et frames à libérer
	uint32_t nb_page = get_nb_page(size);

	uint32_t contiguous_pages = (uint32_t)(*vAddress / PAGE_SIZE);
	for (uint32_t page_i = contiguous_pages; page_i < (contiguous_pages + nb_page); ++page_i)
	{
		// On récupère les indexes de la page
		first_level_index = page_i / SECON_LVL_TT_COUN;
		second_level_index = page_i - first_level_index * SECON_LVL_TT_COUN;

		// On libère l'entrée correpondante
		remove_frame_page_table(*(process->page_table), first_level_index, second_level_index);
	}
}


/**************************************************************************
			    FRAMES TABLE HELPERS
**************************************************************************/

/**
* Indique si une frame est libre ou non
*/
uint32_t get_frame_state(uint32_t frame)
{
	return frames_occupation_table[frame];
}

/**
* Met à jour l'état d'une frame
* 0 : libre / 1 : occupée
*/
void set_frame_state(uint32_t frame, uint8_t state)
{
	frames_occupation_table[frame] = state;
}

/**
* Calcule le nombre de pages nécessaires pour une taille donnée
*/
uint32_t get_nb_page(unsigned int size)
{
	return (((uint32_t)size - 1) / PAGE_SIZE) + 1;
}


/**
* Renvoie une frame qui est libre
* ou UINT32_MAX sinon
*/
uint32_t find_available_frame()
{
	uint32_t i;

	for (i = 0; i < FRAMES_OCCUPATION_TABLE_SIZE; ++i)
	{
		if (get_frame_state(i) == FRAME_FREE)
		{
			return i;
		}
	}

	//Aucune frame n'est libre
	return UINT32_MAX;
}

/**
* Renvoie une liste de frames disponible pour un nb de pages demandé
* ou null sinon
*/
uint32_t* list_available_frames(uint32_t nb_page)
{
	uint32_t* frame_available_list = (uint32_t*) kAlloc(nb_page);
	uint32_t frame;

	uint32_t frame_i = 0;
	for (; frame_i < nb_page; ++frame_i)
	{
		frame = find_available_frame();
		if (frame != UINT32_MAX)
		{
			// frame dispo !
			frame_available_list [frame_i] = frame;
			// On indique que la frame est maintenant occupée
			set_frame_state(frame, 1);
		}
		else
		{
			// NO MORE RAM
			for(; frame_i >= 0; --frame_i)
			{
				// On indique que la frame est maintenant libre
				set_frame_state(frame_available_list[frame_i], 0);
			}
			kFree((uint8_t*)frame_available_list, nb_page);
			return NULL;
		}
	}
	return frame_available_list;
}

/**
* Ajoute une frame à la table des pages
*/
void add_frame_page_table(uint32_t* page_table, uint32_t fl_index, uint32_t sl_index, uint32_t frame)
{
	// On récupère la table de second niveau
	uint32_t* second_level_page_table = get_second_level_page_table(page_table, fl_index);

	// On ajoute la frame dans la case de niveau 2
	uint32_t frame_address = (frame * FRAME_SIZE);
	second_level_page_table[sl_index] = frame_address | SECOND_LVL_FLAGS;

	// On indique que la frame est maintenant occupée
	set_frame_state(frame, 1);
}

/**
* Retire une frame à la table des pages
*/
void remove_frame_page_table(uint32_t* page_table, uint32_t fl_index, uint32_t sl_index)
{
	// On récupère la table de second niveau
	uint32_t* second_level_page_table = get_second_level_page_table(page_table, fl_index);

	uint32_t frame = second_level_page_table[sl_index] / FRAME_SIZE;
	// On met 0 dans la case de niveau 2
	second_level_page_table[sl_index] = 0;

	// On indique que la frame est maintenant libre
	set_frame_state(frame, 0);
}

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


/**************************************
	ACTIVER LA MMU
**************************************/
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
