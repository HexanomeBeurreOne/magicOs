#include "syscall.h"
#include "sched.h"
#include "util.h"
#include "hw.h"

#define NB_PROCESS 5

void user_process_1()
{
	int v1 = 5;
	while (1)
	{
		v1++;
	}
}

void user_process_2()
{
	int v2 = -12;
	while (1)
	{
		v2 -= 2;
	}
}

void user_process_3()
{
	int v3 = 0;
	while (1)
	{
		v3 += 5;
	}
}

void kmain()
{
	sched_init();

	// Selection priorités fixes ou dynamiques
	// TODO

	// Priorité par défaut : 0
	//create_process((func_t*)&user_process_1);
	//create_process((func_t*)&user_process_2);
	//create_process((func_t*)&user_process_3);

	// Processus avec priorité
	create_process_with_priority((func_t*)&user_process_1, 3);
	create_process_with_priority((func_t*)&user_process_2, 1);
	create_process_with_priority((func_t*)&user_process_3, 4);

	// Switch CPU to USER mode
	__asm("cps 0x10");

	while (1)
	{
		sys_yield();
	}
}
