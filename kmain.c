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
	//hw_init();
	for(;;) {
		UsbCheckForChange();
		int nb_clavier = KeyboardCount();
		for( int i = 0; i < nb_clavier; i++) {
		
			int keyboard_address = KeyboardGetAddress(i);
			int keydown_count = KeyboardGetKeyDownCount(keyboard_address);
			
			//int key_stored[] = {-1,-1,-1,-1,-1,-1};
			int key_stored = -1;
			
			for( int j = 0; j<keydown_count; j++) {
			
				int key_number = 4;
				
				while(key_number<130) {
							
					int scan_code = KeyboardGetKeyDown(keyboard_address, key_number);
					key_number++;
					
					if(key_stored == -1) {
						key_stored = key_number;
					} // endif
					
					/*
					for (int k = 0; k<6; k++) {
						if(key_stored[k] == -1) {
							key_stored[k] = key_number;
							break;
						} // endif
					} // endfor
					*/
					
				} // endwhile
				
				if(KeyboardGetKeyDown(keyboard_address, key_stored) == 0 && key_stored != -1) {
					key_stored = -1;
					//led_on();
				} // endif
				
				/*
				for( int l=0; l<6; l++) {
					if(KeyboardGetKeyDown(keyboard_address, key_stored[l])==0) {
						//led_off();
						key_stored[l] = -1;
						led_on()
						break;
					} // endif
				
				} // endfor
				*/
				
			} // endfor
		
		} // endfor
			
	
	} // endwhile
}

void kmain()
{
	hw_init();

   led_blink();
	sched_init();
	
	
	//create_process((func_t*)&user_process_1);
	//create_process((func_t*)&user_process_2);
	
	//create_process((func_t*)&test_led_on);
	//create_process((func_t*)&test_led_on);
	
	

	// Switch to user mode
	__asm("cps 0x10");

	while (1)
	{
		sys_yield();
	}
}
