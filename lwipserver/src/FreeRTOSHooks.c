/*
 * @brief Common FreeRTOS functions shared among platforms
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2012
 * All rights reserved.
 *
 * @par
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * LPC products.  This software is supplied "AS IS" without any warranties of
 * any kind, and NXP Semiconductors and its licensor disclaim any and
 * all warranties, express or implied, including all implied warranties of
 * merchantability, fitness for a particular purpose and non-infringement of
 * intellectual property rights.  NXP Semiconductors assumes no responsibility
 * or liability for the use of the software, conveys no license or rights under any
 * patent, copyright, mask work right, or any other intellectual property rights in
 * or to any products. NXP Semiconductors reserves the right to make changes
 * in the software without notification. NXP Semiconductors also makes no
 * representation or warranty that such application will be suitable for the
 * specified use without further testing or modification.
 *
 * @par
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, under NXP Semiconductors' and its
 * licensor's relevant copyrights in the software, without fee, provided that it
 * is used in conjunction with NXP Semiconductors microcontrollers.  This
 * copyright, permission, and disclaimer notice must appear in all copies of
 * this code.
 */

#include "FreeRTOS.h"
#include "task.h"

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/*****************************************************************************
 * Public functions
 ****************************************************************************/
/* FreeRTOS malloc fail hook */
void vApplicationMallocFailedHook(void)
{
	/* vApplicationMallocFailedHook() will only be called if
	configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
	function that will get called if a call to pvPortMalloc() fails.
	pvPortMalloc() is called internally by the kernel whenever a task, queue,
	timer or semaphore is created.  It is also called by various parts of the
	demo application.  If heap_1.c or heap_2.c are used, then the size of the
	heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
	FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
	to query the size of free heap space that remains (although it does not
	provide information on how the remaining heap might be fragmented). */
	taskDISABLE_INTERRUPTS();
	for (;;) {}
}

/* FreeRTOS application idle hook */
void vApplicationIdleHook(void)
{
	/* Best to sleep here until next systick */
	__WFI();
}

/* FreeRTOS stack overflow hook */
void vApplicationStackOverflowHook(xTaskHandle pxTask, signed char *pcTaskName)
{
	(void) pxTask;
	(void) pcTaskName;

	/* Run time stack overflow checking is performed if
	   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	   function is called if a stack overflow is detected. */
	taskDISABLE_INTERRUPTS();
	printf("Overflow!");
	for (;;) {}
}

/* FreeRTOS application tick hook */
void vApplicationTickHook(void)
{}

/* vConfigureTimerForRunTimeStats is performed if
   configGENERATE_RUN_TIME_STATS is defined to 1.  This hook
   function is called because we have this additional config in FreeRTOSConfig.h:
   extern void vConfigureTimerForRunTimeStats( void );
   #define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() vConfigureTimerForRunTimeStats()
   #define portGET_RUN_TIME_COUNTER_VALUE() (configCPU_CLOCK_HZ / 10000UL)
*/
void vConfigureTimerForRunTimeStats(void)
{
	uint32_t ulSysTickCounts, ulTickCount, ulReturn;
	const uint32_t ulSysTickReloadValue = ( configCPU_CLOCK_HZ / configTICK_RATE_HZ ) - 1UL;
	volatile uint32_t * const pulCurrentSysTickCount = ( ( volatile uint32_t *) 0xe000e018 );
	volatile uint32_t * const pulInterruptCTRLState = ( ( volatile uint32_t *) 0xe000ed04 );
	const uint32_t ulSysTickPendingBit = 0x04000000UL;

	/* NOTE: There are potentially race conditions here.  However, it is used
	anyway to keep the examples simple, and to avoid reliance on a separate
	timer peripheral. */

	/* The SysTick is a down counter.  How many clocks have passed since it was
	last reloaded? */
	ulSysTickCounts = ulSysTickReloadValue - *pulCurrentSysTickCount;

	/* How many times has it overflowed? */
	ulTickCount = xTaskGetTickCountFromISR();

	/* Is there a SysTick interrupt pending? */
	if( ( *pulInterruptCTRLState & ulSysTickPendingBit ) != 0UL )
	{
		/* There is a SysTick interrupt pending, so the SysTick has overflowed
		but the tick count not yet incremented. */
		ulTickCount++;

		/* Read the SysTick again, as the overflow might have occurred since
		it was read last. */
		ulSysTickCounts = ulSysTickReloadValue - *pulCurrentSysTickCount;
	}

	/* Convert the tick count into tenths of a millisecond.  THIS ASSUMES
	configTICK_RATE_HZ is 1000! */
	ulReturn = ( ulTickCount * 10UL ) ;

	/* Add on the number of tenths of a millisecond that have passed since the
	tick count last got updated. */
	ulReturn += ( ulSysTickCounts / (configCPU_CLOCK_HZ / 10000UL));

	return ulReturn;
}
