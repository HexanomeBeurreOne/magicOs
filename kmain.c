#include "syscall.h"
#include "sched.h"
#include "util.h"
#include "hw.h"
#include "keyboard.h"

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


void test_led_on()
{
	led_on();
}

void test_led_blink()
{
	led_blink();
}

void test_led_off()
{
	led_off();
}

void test_clavier()
{
	UsbInitialise();
	int nb_clavier;
	
	while (nb_clavier == 0)
	{
		UsbCheckForChange();
		nb_clavier = KeyboardCount();
	}
	
	char input_char = 0;


	while (input_char == 0) {
		KeyboardUpdate();
		input_char = KeyboardGetChar();
	}
	
	if (input_char == 'e')
	{
		led_on();
	}
	if (input_char == 'a')
	{
		led_off();
	}
	input_char = 0;
}

void kmain()
{
	hw_init();
	sched_init();
	//create_process((func_t*)&user_process_1);
	//create_process((func_t*)&user_process_2);
	
	//create_process((func_t*)&test_led_on);
	//create_process((func_t*)&test_led_on);
	
	create_process((func_t*)&test_clavier);

	// Switch to user mode
	__asm("cps 0x10");

	while (1)
	{
		sys_yield();
	}
}
