#ifndef __SCHED__
#define __SCHED__

#define NBREG 13

#define INCREMENT_IF_WAITING_SINCE 1

enum PROCESS_STATUS
{
	WAITING,
	RUNNING,
	TERMINATED
};

enum SCHEDULING_TYPE
{
	ROUND_ROBIN,
	FIXED_PRIORITIES,
	DYNAMIC_PRIORITIES
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

	int priority;
	int waiting_since;
	uint32_t** page_table;

	
	struct pcb_s* next;
	struct pcb_s* previous;
};

void sched_init();
void create_process(func_t* entry);
void create_process_with_priority(func_t* entry, int priority);

#endif // __SCHED__
