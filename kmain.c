#include "syscall.h"
#include "sched.h"
#include "util.h"
#include "hw.h"

#define NB_PROCESS 5

void user_process_1()
{
	int v1 = 5;
	int m = 0;
	while (m < 500)
	{
		v1++;
		if(v1 > 1000000) {
			m++;
			v1 = 0;
		}
	}
}

void user_process_2()
{
	int v2 = -12;
	int m = 0;
	while (m < 500)
	{
		v2++;
		if(v2 > 1000000) {
			m++;
			v2 = 0;
		}
	}
}

void user_process_3()
{
	int v3 = 0;
	int m = 0;
	while (m < 500)
	{
		v3++;
		if(v3 > 1000000) {
			m++;
			v3 = 0;
		}
	}
}

void kmain()
{
	/*
	Types of schedulers available :
	ROUND_ROBIN,
	FIXED_PRIORITIES,
	DYNAMIC_PRIORITIES
	*/

	// Need to specify the scheduler
	sched_init(DYNAMIC_PRIORITIES);

	// Default priority : 0
	//create_process((func_t*)&user_process_1);
	//create_process((func_t*)&user_process_2);
	//create_process((func_t*)&user_process_3);

	// Processes with priority
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
