#ifndef __SCHED__
#define __SCHED__

#define NBREG 13

enum PROCESS_STATUS
{
	WAITING,
	RUNNING,
	TERMINATED
};

typedef int (func_t) (void);

struct pcb_s
{
	uint32_t registers[NBREG];
	uint32_t lr_svc;
	uint32_t lr_user;
	uint32_t* sp_user;
	uint32_t cpsr_user;
	
	func_t* entry;

	int status;
	int return_code;
	
	struct pcb_s* next;
	struct pcb_s* previous;
};

void sched_init();
void create_process(func_t* entry);

#endif // __SCHED__
