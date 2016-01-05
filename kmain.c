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


void test_led()
{
	hw_init();
	led_on();
}

void test_clavier()
{

	UsbInitialise();
	hw_init();
	while(1) {
		UsbCheckForChange();
		int nb_clavier = KeyboardCount();
			for( int i = 0; i < nb_clavier ; i++) {
			
				int keyboard_address = KeyboardGetAddress(i);
				int keydown_count = KeyboardGetKeyDownCount(keyboard_address);
				
				for( int j = 0 ; j < keydown_count ; j++) {
					int key_number = 4;
					
					int key_stored[] = {-1,-1,-1,-1,-1,-1}; 
					
					while( key_number<130) {
								
						int scan_code = KeyboardGetKeyDown(keyboard_address, key_number);
						
						key_number++;
						
						//int key_status = KeyboardGetKeyIsDown(keyboard_address, scan_code);
						
						//if(key_status !=0) {
							led_on();
							for (int k = 0; k<6; k++) {
								if(key_stored[k] == -1) {
									key_stored[k] = key_number;
									break;
								} // endif
							} // endfor
						//} // endif
					} // endwhile
					
					for( int l=0; l<6; l++) {
						//int key_released_status = KeyboardGetKeyIsDown(keyboard_address, key_stored[l]);
						if(KeyboardGetKeyDown(keyboard_address, key_stored[l])==0) {
							led_off();
							key_stored[l] = -1;
							break;
						} // endif
					
					} // endfor
					
				} // endfor
			
			} // endfor
			
	
	} // endwhile
}

void kmain()
{
	sched_init();
	
	//create_process((func_t*)&user_process_1);
	//create_process((func_t*)&user_process_2);
	//create_process((func_t*)&user_process_3);
	
	create_process((func_t*)&test_clavier);

	// Switch to user mode
	__asm("cps 0x10");

	while (1)
	{
		sys_yield();
	}
}
