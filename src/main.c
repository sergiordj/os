/*==================[inclusions]=============================================*/

#include "os.h"
#include "board.h"

/*==================[macros and definitions]=================================*/

/** tamaño de pila para los threads */
#define STACK_SIZE 512
#define ROJORGB 0
#define AMARILLO 3
#define ROJO 4
#define VERDE 5

/*==================[internal data declaration]==============================*/

/*==================[internal functions declaration]=========================*/

static void * tarea1(void * param);
static void * tarea2(void * param);
static void * tarea3(void * param);
static void * tarea4(void * param);

/*==================[internal data definition]===============================*/

/*==================[external data definition]===============================*/

/* pilas de cada tarea */
uint8_t stack1[STACK_SIZE];
uint8_t stack2[STACK_SIZE];
uint8_t stack3[STACK_SIZE];
uint8_t stack4[STACK_SIZE];

const taskDefinition task_list[] = {
		{stack1, STACK_SIZE, tarea1, 1, (void *)0xAAAAAAAA},
		{stack2, STACK_SIZE, tarea2, 3, (void *)0xBBBBBBBB},
		{stack3, STACK_SIZE, tarea3, 4, (void *)0xBBBBBBBB},
		{stack4, STACK_SIZE, tarea4, 2, (void *)0xBBBBBBBB}
};


/*==================[internal functions definition]==========================*/

static void * tarea1(void * param)
{
	float i = 1;
	int32_t a;
	while (1) {
		Board_LED_Toggle(ROJO);
		a = i*500;
		delay(a);
		//for (a=0; a<0xAAAAAA; a++);
		i += 0.1;
	}
	return (void *)0; /* a dónde va? */
}

static void * tarea2(void * param)
{
	float i = 1;
	int32_t a;
	while (1) {
		Board_LED_Toggle(VERDE);
		a = i*100;
		delay(a);
		//for (i=0; i<0xFFFFFF; i++);
		i += 0.1;
	}
	return (void *)4; /* a dónde va? */
}

static void * tarea3(void * param)
{
	float i = 1;
	int32_t a;
	while (1) {
		Board_LED_Toggle(ROJORGB);
		a = i*200;
		delay(a);
		//for (i=0; i<0xFFFFFF; i++);
		i += 0.1;
	}
	return (void *)4; /* a dónde va? */
}

static void * tarea4(void * param)
{
	float i = 1;
	int32_t a;
	while (1) {
		Board_LED_Toggle(AMARILLO);
		a = i*300;
		delay(a);
		//for (i=0; i<0xFFFFFF; i++);
		i += 0.1;
	}
	return (void *)4; /* a dónde va? */
}

/*==================[external functions definition]==========================*/

int main(void)
{
	/* Inicialización del MCU */
	Board_Init();

	/* Inicio OS */
	start_os();

	/* no deberíamos volver acá */
	while(1);
}

/*==================[end of file]============================================*/
