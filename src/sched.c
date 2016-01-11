#include <stdint.h>
#include "sched.h"
#include "kheap.h"
#include "vmem.h"
#include "config.h"
#include "hw.h"
#include "asm_tools.h"
#include "syscall.h"

#define STACK_SIZE 10000

static struct pcb_s* current_process;
static struct pcb_s kmain_process;
static int scheduling_type;

void sched_init(int scheduling)
{
	//9.6 Appel vmem_init 
	vmem_init();
	timer_init();

	current_process = &kmain_process;
	current_process->next = current_process;
	current_process->previous = current_process;
	current_process->status = RUNNING;
	// Which priority for kmain_process ???
	current_process->priority = 0;
	// Useless
	current_process->waiting_since = 0;
	
	scheduling_type = scheduling;

	ENABLE_IRQ();
}

void add_process_to_loop(struct pcb_s* process)
{
	struct pcb_s* previousProcess;
	switch(scheduling_type) {
		case ROUND_ROBIN:
			previousProcess = kmain_process.previous;
		break;
		default:
			previousProcess = &kmain_process;
			while(previousProcess->next->priority >= process->priority && previousProcess->next != &kmain_process) {
				previousProcess = previousProcess->next;
			}
	}

	previousProcess->next->previous = process;
	process->next = previousProcess->next;
	previousProcess->next = process;
	process->previous = previousProcess;
}

void remove_process_from_loop(struct pcb_s* process)
{
	// Link the previous element to the next and the next to the previous to delete current process
	process->previous->next = process->next;
	process->next->previous = process->previous;
}

void update_priorities()
{
	// TODO : set maximum priority
	static struct pcb_s* process;
	process = kmain_process.next;
	while(process != &kmain_process) {
		if(process == current_process) {
			process->waiting_since = 0;
		}
		else {
			process->waiting_since++;
			if(process->waiting_since >= INCREMENT_IF_WAITING_SINCE) {
				process->waiting_since = 0;
				process->priority++;
				remove_process_from_loop(process);
				add_process_to_loop(process);
			}
		}
		process = process->next;
	}
}

static void start_current_process()
{
	current_process->entry();
	sys_exit(0);
}

void create_process(func_t* entry)
{
	create_process_with_priority(entry, 0);
}

void create_process_with_priority(func_t* entry, int priority)
{
	struct pcb_s* process = (struct pcb_s*)kAlloc(sizeof(struct pcb_s));
	process->entry = entry;
	process->lr_svc = (uint32_t)&start_current_process;
	process->priority = priority;

	// Allocate stack
	process->sp_user = (uint32_t*)kAlloc(STACK_SIZE) + STACK_SIZE;

	__asm("mrs %0, cpsr" : "=r"(process->cpsr_user)); // TODO : pourquoi nécessaire d'initialiser CPSR
	
	// Add the process in the loop of processes
	add_process_to_loop(process);

	// Initial status
	process->status = WAITING;
}

static void free_process(struct pcb_s* process)
{
	kFree((uint8_t*)process, sizeof(struct pcb_s) + STACK_SIZE);
}

static void elect()
{
	struct pcb_s* next_process;
	switch(scheduling_type) {
		case ROUND_ROBIN:
			next_process = current_process->next;
		break;
		default:
			if(kmain_process.next->priority > current_process->priority || current_process->next->priority < current_process->priority) {
				next_process = kmain_process.next;
			}
			else {
				next_process = current_process->next;
			}
	}
	// Delete current if terminated (so a terminated process does not wait at the end of list)
	if (current_process->status == TERMINATED)
	{
		// If it is the last process
		if (current_process == current_process->next && current_process == current_process->previous)
			terminate_kernel();
		
		// Remove the process from the loop of processes
		remove_process_from_loop(current_process);
		
		free_process(current_process);
	}
	else
	{
		current_process->status = WAITING;
	}
	current_process = next_process;
	if (current_process->status == TERMINATED)
		elect(); // Elect the next one, and delete the current one
	else
		current_process->status = RUNNING; // Else, this one is now running

	// Updates the wait counter used in dynamic priorities scheduling
	if(scheduling_type == DYNAMIC_PRIORITIES) {
		update_priorities();
	}
}

void do_sys_yieldto(uint32_t* sp) // Points on saved r0 in stack
{	
	struct pcb_s* dest = (struct pcb_s*)(*(sp+1));

	__asm("mrs %0, spsr" : "=r"(current_process->cpsr_user)); // Save user CPSR, which has been copied at swi in SPSR_svc

	__asm("cps 0b11111"); // System mode
	__asm("mov %0, lr" : "=r"(current_process->lr_user)); // Save lr_user
	__asm("mov %0, sp" : "=r"(current_process->sp_user)); // Save sp_user
	__asm("cps 0b10011"); // SVC mode

	int i;
	for (i = 0; i < NBREG; ++i)
	{
		current_process->registers[i] = sp[i];
		// Update context in stack
		sp[i] = dest->registers[i];
	}
	current_process->lr_svc = sp[NBREG];
	sp[NBREG] = dest->lr_svc;

	current_process = dest;

	__asm("cps 0b11111"); // System mode
	__asm("mov lr, %0" : : "r"(current_process->lr_user));
	__asm("mov sp, %0" : : "r"(current_process->sp_user));
	__asm("cps 0b10011"); // SVC mode

	__asm("msr spsr, %0" : : "r"(current_process->cpsr_user));

}

void do_sys_yield(uint32_t* sp) // Points on saved r0 in stack
{
	// Save current context
	__asm("cps 0b11111"); // System mode
	__asm("mov %0, lr" : "=r"(current_process->lr_user)); // Save lr_user
	__asm("mov %0, sp" : "=r"(current_process->sp_user)); // Save sp_user
	__asm("cps 0b10011"); // SVC mode

	__asm("mrs %0, spsr" : "=r"(current_process->cpsr_user)); // Save user CPSR, which has been copied at swi in SPSR_svc

	int i;
	for (i = 0; i < NBREG; ++i)
	{
		current_process->registers[i] = sp[i];
	}
	current_process->lr_svc = sp[NBREG];

	// Elects new current process
	elect();
	
	// Update context which will be reloaded
	for (i = 0; i < NBREG; ++i)
	{
		sp[i] = current_process->registers[i];
	}
	sp[NBREG] = current_process->lr_svc;

	__asm("cps 0b11111"); // System mode
	__asm("mov lr, %0" : : "r"(current_process->lr_user));
	__asm("mov sp, %0" : : "r"(current_process->sp_user));
	__asm("cps 0b10011"); // SVC mode

	__asm("msr spsr, %0" : : "r"(current_process->cpsr_user));
	
	//TEST
	__asm("mov r0, %0" : : "r"(vmem_translate(0x1000000,current_process)));
}

void do_sys_exit(uint32_t* sp) // Points on saved r0 in stack
{
	current_process->status = TERMINATED;
	current_process->return_code = *(sp+1);

	sys_yield();
}

static void context_save_to_pcb(uint32_t* sp) // Points to the beginning of process context in stack
{
	__asm("cps 0b11111"); // System mode
	__asm("mov %0, lr" : "=r"(current_process->lr_user)); // Save lr_user
	__asm("mov %0, sp" : "=r"(current_process->sp_user)); // Save sp_user
	__asm("cps 0b10010"); // IRQ mode

	__asm("mrs %0, spsr" : "=r"(current_process->cpsr_user)); // Save user CPSR, which has been copied at swi in SPSR_svc

	int i;
	for (i = 0; i < NBREG; ++i)
	{
		current_process->registers[i] = sp[i];
	}
	current_process->lr_svc = sp[NBREG];
}

static void context_load_from_pcb(uint32_t* sp) // Points to the beginning of process context in stack
{
	int i;
	// Update context which will be reloaded
	for (i = 0; i < NBREG; ++i)
	{
		sp[i] = current_process->registers[i];
	}
	sp[NBREG] = current_process->lr_svc;


	__asm("cps 0b11111"); // System mode
	__asm("mov lr, %0" : : "r"(current_process->lr_user));
	__asm("mov sp, %0" : : "r"(current_process->sp_user));
	__asm("cps 0b10010"); // IRQ mode

	__asm("msr spsr, %0" : : "r"(current_process->cpsr_user));
}

static void handle_irq(uint32_t* sp)
{
	context_save_to_pcb(sp);
	elect();
	context_load_from_pcb(sp);
}

void __attribute__((naked)) irq_handler()
{
	// Already in IRQ mode. So the old cpsr has been copied into spsr_irq. I bit of cpsr already to 0 (deactivated)

	// Save registers and return address (decremented by 4, see Encart 13 + p.42 the "Il faut donc décrémenter...")
	// Save return address lr because there is a function call in this function
	__asm("sub lr, #4");
	__asm("stmfd sp!, {r0-r12, lr}");
	//__asm("push {r0-r12, lr}"); // The same
	
	// Load right handler PC
	// TODO if (timerInterrupt) handle_irq(); ??????
	uint32_t* sp;
	__asm("mov %0, sp" : "=r"(sp));
	handle_irq(sp);

	// Re-arm timer
	set_next_tick_default();
    ENABLE_TIMER_IRQ();

	// Reactivate interruptions (I bit to 0)
	ENABLE_IRQ();


	// Reload registers, put return address (lr_irq) into pc to get back to interrupted code, and load spsr => change back CPU mode
	__asm("ldmfd sp!, {r0-r12, pc}^");
}

