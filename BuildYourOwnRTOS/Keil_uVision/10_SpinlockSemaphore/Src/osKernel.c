#include "osKernel.h"

#define NUM_OF_THREADS			3
#define STACKSIZE				400

#define BUS_FREQ				16000000

#define CTRL_ENABLE		(1U<<0)
#define CTRL_TICKINT	(1U<<1)
#define CTRL_CLCKSRC	(1U<<2)
#define CTRL_COUNTFLAG	(1U<<16)
#define SYSTICK_RST		0



#define INTCTRL				(*((volatile uint32_t *)0xE000ED04))
#define PENDSTSET			(1U<<26)


#define TIM2EN			(1U<<0)
#define CR1_CEN			(1U<<0)
#define DIER_UIE		(1U<<0)


uint32_t period_tick;


void osSchedulerLaunch(void);


uint32_t 	MILLIS_PRESCALER;
struct tcb{
	int32_t *stackPt;
	struct tcb *nextPt;
};

typedef struct tcb tcbType;


tcbType tcbs[NUM_OF_THREADS];
tcbType *currentPt;


/*Each thread will have stacksize of 100 i.e. 400bytes*/
int32_t TCB_STACK[NUM_OF_THREADS][STACKSIZE];


void osKernelStackInit(int i)
{
	tcbs[i].stackPt =  &TCB_STACK[i][STACKSIZE - 16]; /*Stack Pointer*/

	/*Set bit21 (T-bit) in PSR to 1, to operate in
	 * Thumb mode*/
	TCB_STACK[i][STACKSIZE - 1] =  (1U<<24); /*PSR*/


  /**@Note : Block below is optional, for debugging purpose only*/
  /**Dummy stack content*/
	TCB_STACK[i][STACKSIZE-3]  = 0xAAAAAAAA;    /*R14 i.e LR*/
	TCB_STACK[i][STACKSIZE-4]  = 0xAAAAAAAA;    /*R12*/
	TCB_STACK[i][STACKSIZE-5]  = 0xAAAAAAAA;    /*R3*/
	TCB_STACK[i][STACKSIZE-6]  = 0xAAAAAAAA;    /*R2*/
	TCB_STACK[i][STACKSIZE-7]  = 0xAAAAAAAA;    /*R1*/
	TCB_STACK[i][STACKSIZE-8]  = 0xAAAAAAAA;    /*R0*/

	TCB_STACK[i][STACKSIZE-9]  = 0xAAAAAAAA;    /*R11*/
	TCB_STACK[i][STACKSIZE-10] = 0xAAAAAAAA;   /*R10*/
	TCB_STACK[i][STACKSIZE-11] = 0xAAAAAAAA;   /*R9*/
	TCB_STACK[i][STACKSIZE-12] = 0xAAAAAAAA;   /*R8*/
	TCB_STACK[i][STACKSIZE-13] = 0xAAAAAAAA;   /*R7*/
	TCB_STACK[i][STACKSIZE-14] = 0xAAAAAAAA;   /*R6*/
	TCB_STACK[i][STACKSIZE-15] = 0xAAAAAAAA;   /*R5*/
	TCB_STACK[i][STACKSIZE-16] = 0xAAAAAAAA;   /*R4*/
}


uint8_t osKernelAddThreads(void(*task0)(void),void(*task1)(void),void(*task2)(void))
{
	/*Disable global interrupts*/
	__disable_irq();
	tcbs[0].nextPt =  &tcbs[1];
	tcbs[1].nextPt =  &tcbs[2];
	tcbs[2].nextPt =  &tcbs[0];

	/*Initial stack for thread0*/
	osKernelStackInit(0);
	/*Initial PC*/
	TCB_STACK[0][STACKSIZE - 2] =  (int32_t)(task0);

	/*Initial stack for thread1*/
	osKernelStackInit(1);
	/*Initial PC*/
	TCB_STACK[1][STACKSIZE - 2] =  (int32_t)(task1);


	/*Initial stack for thread2*/
	osKernelStackInit(2);
	/*Initial PC*/
	TCB_STACK[2][STACKSIZE - 2] =  (int32_t)(task2);

	/*Start from thread0*/
	currentPt  = &tcbs[0];

	/*Enable global interrupts*/
	__enable_irq();

	return 1;
}


void osKernelInit(void)
{
	MILLIS_PRESCALER  = (BUS_FREQ/1000);
}


void osKernelLaunch(uint32_t quanta)
{
	/*Reset systick*/
	SysTick->CTRL = SYSTICK_RST;

	/*Clear systick current value register*/
	SysTick->VAL = 0;

	/*Load quanta*/
	SysTick->LOAD =  (quanta * MILLIS_PRESCALER) - 1;

	/*Set systick to low priority*/
	NVIC_SetPriority(SysTick_IRQn,15);

	/*Enable systick, select internal clock*/
	SysTick->CTRL  = CTRL_CLCKSRC | CTRL_ENABLE;

	/*Enable systick interrupt*/
	SysTick->CTRL  |= CTRL_TICKINT;

	/*Launch scheduler*/
	osSchedulerLaunch();
}

void osThreadYield(void)
{
	/*Clear Systick Current Value register*/
	SysTick->VAL = 0;

	/*Trigger SysTick*/
	INTCTRL = PENDSTSET;
}



void osSchedulerRoundRobin(void)
{
	if((++period_tick)  == PERIOD)
	{
		(*task3)();

		period_tick = 0;
	}

	currentPt = currentPt->nextPt;
}



void tim2_1hz_interrupt_init(void)
{
	/*Enable clock access to tim2*/
	RCC->APB1ENR |=TIM2EN;

	/*Set timer prescaler*/
	TIM2->PSC =  1600 - 1; // 16 000  000 / 1600 = 10 000

	/*Set auto-reload value*/
	TIM2->ARR =  10000 -1;  //10 0000 / 10 000 =  1

	/*Clear counter*/
	TIM2->CNT = 0;

	/*Enable timer*/
	TIM2->CR1 =  CR1_CEN;

	/*Enable timer interrupt*/
	TIM2->DIER |= DIER_UIE;

	/*Enable timer interrupt in NVIC*/
	NVIC_EnableIRQ(TIM2_IRQn);
}


void osSemaphoreInit(int32_t *semaphore, int32_t value)
{
	*semaphore =  value;
}


void osSemaphoreSet(int32_t * semaphore)
{
	__disable_irq();
	*semaphore += 1;
	__enable_irq();

}

void osSemaphoreWait(int32_t * semaphore)
{
	__disable_irq();
	while(*semaphore <= 0)
	{
		__disable_irq();
		__enable_irq();
	}
	*semaphore -=1 ;
	__enable_irq();
}
