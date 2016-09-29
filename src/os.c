/*==================[inclusions]=============================================*/

#include "board.h"
#include "os.h"

#include <string.h>

/*==================[macros and definitions]=================================*/

/** valor de retorno de excepción a cargar en el LR */
#define EXC_RETURN 0xFFFFFFF9

/** id de tarea inválida */
#define INVALID_TASK ((uint32_t)-1)

/** id de tarea idle */
#define IDLE_TASK (Task_Count)

/** tamaño de stack de tarea idle */
#define STACK_IDLE_SIZE 256

/** Cantidad de prioridades definidad */
#define PRIORITY_COUNT 5

/** estructura interna de control de tareas */
typedef struct taskControlBlock {
	uint32_t sp;
	const taskDefinition * tdef;
	taskState state;
	uint32_t waiting_time;
	uint32_t scheduled_flag;
}taskControlBlock;

/*==================[internal data declaration]==============================*/
static uint32_t PriorityBuffer [PRIORITY_COUNT] [Task_Count+1];
static uint32_t PriorityWritePointer [PRIORITY_COUNT];
static uint32_t PriorityReadPointer [PRIORITY_COUNT];
/*==================[internal functions declaration]=========================*/

__attribute__ ((weak)) void * idle_hook(void * p);

/*==================[internal data definition]===============================*/

/** indice a la tarea actual */
static uint32_t current_task = INVALID_TASK;
static uint32_t current_priority = 0;

/** estructura interna de control de tareas */
static taskControlBlock task_control_list[Task_Count];

/** contexto de la tarea idle */
uint8_t stack_idle[STACK_IDLE_SIZE];
static taskDefinition idle_tdef = {
		stack_idle, STACK_IDLE_SIZE, idle_hook, 0, 0
};
static taskControlBlock idle_task_control;

/*==================[external data definition]===============================*/

/*==================[internal functions definition]==========================*/

/* si una tarea ejecuta return, vengo acá */
static void return_hook(void * returnValue)
{
	while(1);
}

/* task_create sirve para crear un contexto inicial */
static void task_create(
		uint8_t * stack_frame, /* vector de pila (frame) */
		uint32_t stack_frame_size, /* el tamaño expresado en bytes */
		uint32_t * stack_pointer, /* donde guardar el puntero de pila */
		entry_point_t entry_point, /* punto de entrada de la tarea */
		void * parameter, /* parametro de la tarea */
		taskState * state,
		uint32_t * scheduled_flag)
{
	uint32_t * stack = (uint32_t *)stack_frame;

	/* inicializo el frame en cero */
	bzero(stack, stack_frame_size);

	/* último elemento del contexto inicial: xPSR
	 * necesita el bit 24 (T, modo Thumb) en 1
	 */
	stack[stack_frame_size/4 - 1] = 1<<24;

	/* anteúltimo elemento: PC (entry point) */
	stack[stack_frame_size/4 - 2] = (uint32_t)entry_point;

	/* penúltimo elemento: LR (return hook) */
	stack[stack_frame_size/4 - 3] = (uint32_t)return_hook;

	/* elemento -8: R0 (parámetro) */
	stack[stack_frame_size/4 - 8] = (uint32_t)parameter;

	stack[stack_frame_size/4 - 9] = EXC_RETURN;

	/* inicializo stack pointer inicial */
	*stack_pointer = (uint32_t)&(stack[stack_frame_size/4 - 17]);

	/* seteo estado inicial READY */
	*state = TASK_STATE_READY;

	/* inicializo el flag de schedule */
	*scheduled_flag = 0;
}

void task_delay_update(void)
{
	uint32_t i;
	for (i=0; i<Task_Count; i++) {
		if ( (task_control_list[i].state == TASK_STATE_WAITING) &&
				(task_control_list[i].waiting_time > 0)) {
			task_control_list[i].waiting_time--;
			if (task_control_list[i].waiting_time == 0) {
				task_control_list[i].state = TASK_STATE_READY;
			}
		}
	}
}

void priority_buffer_update(void)
{
	uint32_t i;
	for (i=0; i<Task_Count; i++) {

		if ( (task_control_list[i].state == TASK_STATE_READY) && (task_control_list[i].scheduled_flag == 0)) {

			uint32_t Task_Priority = task_control_list[i].tdef->priority;
			PriorityBuffer [Task_Priority][PriorityWritePointer[Task_Priority]]=i;
			task_control_list[i].scheduled_flag = 1;
			PriorityWritePointer[Task_Priority]++;

			if (PriorityWritePointer[Task_Priority] > Task_Count){
				PriorityWritePointer[Task_Priority] = 0;
			}
		}
	}

	if ( (idle_task_control.state == TASK_STATE_READY) && (idle_task_control.scheduled_flag == 0)) {

		uint32_t Task_Priority = idle_task_control.tdef->priority;
		PriorityBuffer [Task_Priority][PriorityWritePointer[Task_Priority]]=IDLE_TASK;
		idle_task_control.scheduled_flag = 1;
		PriorityWritePointer[Task_Priority]++;

		if (PriorityWritePointer[Task_Priority] > Task_Count){
			PriorityWritePointer[Task_Priority] = 0;
		}
	}
}

/*==================[external functions definition]==========================*/

void delay(uint32_t milliseconds)
{
	if (current_task != INVALID_TASK && milliseconds > 0) {
		task_control_list[current_task].state = TASK_STATE_WAITING;
		task_control_list[current_task].waiting_time = milliseconds;
		schedule();
	}
}

/* rutina de selección de próximo contexto a ejecutarse
 * acá definimos la política de scheduling de nuestro os
 */
int32_t getNextContext(int32_t current_context)
{
	uint32_t previous_task = current_task;
	uint32_t returned_stack;
	int32_t i;

	/* guardo contexto actual si es necesario */
	if (current_task == IDLE_TASK) {
		idle_task_control.sp = current_context;
	}
	else if (current_task < Task_Count) {
		task_control_list[current_task].sp = current_context;
	}

	/* Seleccionamos la posible próxima tarea */
	for (i = (PRIORITY_COUNT-1); i >= 0; i--){
		if (PriorityReadPointer[i] != PriorityWritePointer[i]){
			current_task = PriorityBuffer[i][PriorityReadPointer[i]];
			if (current_task != IDLE_TASK){
				task_control_list[current_task].scheduled_flag = 0;
			} else {
				idle_task_control.scheduled_flag = 0;
			}
			PriorityReadPointer[i]++;
			if (PriorityReadPointer[i] > Task_Count){
				PriorityReadPointer[i] = 0;
			}
			break;
		}
	}

	if (previous_task != IDLE_TASK){
		if (current_task != IDLE_TASK){
			if (task_control_list[current_task].tdef->priority >= current_priority){
				if (task_control_list[previous_task].state == TASK_STATE_RUNNING){
					task_control_list[previous_task].state = TASK_STATE_READY;
				}
				task_control_list[current_task].state = TASK_STATE_RUNNING;
				current_priority = task_control_list[current_task].tdef->priority;
				returned_stack = task_control_list[current_task].sp;
			} else {
				if (task_control_list[previous_task].state == TASK_STATE_RUNNING){
					returned_stack = task_control_list[previous_task].sp;
				} else {
					current_priority = task_control_list[current_task].tdef->priority;
					returned_stack = task_control_list[current_task].sp;
				}
			}
		} else {
			if ((task_control_list[previous_task].state != TASK_STATE_RUNNING)){
				idle_task_control.state = TASK_STATE_RUNNING;
				current_priority = idle_task_control.tdef->priority;
				returned_stack = idle_task_control.sp;
			} else {
				returned_stack = task_control_list[previous_task].sp;
			}
		}
	} else {
		if (current_task != IDLE_TASK){
			idle_task_control.state = TASK_STATE_READY;
			task_control_list[current_task].state = TASK_STATE_RUNNING;
			current_priority = task_control_list[current_task].tdef->priority;
			returned_stack = task_control_list[current_task].sp;
		} else {
			returned_stack = idle_task_control.sp;
		}
	}

	return returned_stack;
}

void schedule(void)
{
	/*Lleno las listas de prioridad */
	priority_buffer_update();

	/* activo PendSV para llevar a cabo el cambio de contexto */
	SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;

	/* Instruction Synchronization Barrier: aseguramos que se
	 * ejecuten todas las instrucciones en  el pipeline
	 * */

	__ISB();
	/* Data Synchronization Barrier: aseguramos que se
	 * completen todos los accesos a memoria
	 * */

	__DSB();
}

void SysTick_Handler(void)
{
	task_delay_update();
	schedule();
}

int start_os(void)
{
	uint32_t i;

	/* actualizo SystemCoreClock (CMSIS) */
	SystemCoreClockUpdate();

	/* inicializo contexto idle */
	idle_task_control.tdef = &idle_tdef;
	task_create(idle_task_control.tdef->stack,
			idle_task_control.tdef->stack_size,
			&(idle_task_control.sp),
			idle_task_control.tdef->entry_point,
			idle_task_control.tdef->parameter,
			&(idle_task_control.state),
			&(idle_task_control.scheduled_flag));

	/** inicializo contextos iniciales de cada tarea */
	for (i=0; i<Task_Count; i++) {
		task_control_list[i].tdef = task_list+i;

		task_create(task_control_list[i].tdef->stack,
				task_control_list[i].tdef->stack_size,
				&(task_control_list[i].sp),
				task_control_list[i].tdef->entry_point,
				task_control_list[i].tdef->parameter,
				&(task_control_list[i].state),
				&(task_control_list[i].scheduled_flag));
	}

	/** Inicializo los punteros del buffer de prioridades */
	for (i = 0; i<PRIORITY_COUNT; i++){
		PriorityWritePointer[i]=0;
		PriorityReadPointer[i]=0;
	}

	/* configuro PendSV con la prioridad más baja */
	NVIC_SetPriority(PendSV_IRQn, 255);

	/* Configuro el systick para un tick cada milisegundo */
	SysTick_Config(SystemCoreClock / 1000);

	/* Lleno las listas de prioridad y llamo al scheduler */
	schedule();

	return -1;
}

void * idle_hook(void * p)
{
	while (1) {
		__WFI();
	}
}

/*==================[end of file]============================================*/
