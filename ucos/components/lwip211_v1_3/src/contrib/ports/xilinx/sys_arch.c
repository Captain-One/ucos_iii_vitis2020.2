/* sys_arch.c -
 *	provide sys_arch functionality as required for lwIP
 *	most of this functionality is obtained from xilkernel
 *	this file acts as a wrapper around the xilkernel functions
 */

/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * Copyright (C) 2007 - 2019 Xilinx, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *         Simon Goldschmidt
 *
 */

#include "lwipopts.h"
#include "xil_printf.h"

#ifdef OS_IS_XILKERNEL
#include "xmk.h"
#include "sys/timer.h"
#include "sys/process.h"

#include "lwip/sys.h"
#include "lwip/opt.h"
#include "lwip/stats.h"

#include "arch/sys_arch.h"
#include "lwipopts.h"
#include "lwip/debug.h"

#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include "os_config.h"
#include "errno.h"

struct thread_start_param {
	struct sys_thread *thread;
	void (*function)(void*);
	void *arg;
};

struct sys_thread {
	pthread_t tid;
	struct thread_start_param tp;
};



/* statically allocate required structures */
struct sys_mbox_s      	lwip_mbox[SYS_MBOX_MAX];
struct sys_thread    	lwip_thread[SYS_THREAD_MAX];

static int SemaphoreCnt = 0;

void
sys_init ()
{
	int i;

	/* Initialize mailboxes */
	for (i = 0; i < SYS_MBOX_MAX; i++)
		lwip_mbox[i].used = 0;

	/* Initialize threads */
	for (i = 0; i < SYS_THREAD_MAX; i++)
		lwip_thread[i].tid = TID_FREE;
}

err_t
sys_sem_new(sys_sem_t *sem, u8_t count)
{
	int i;
	int shared = 0;

	if (SemaphoreCnt >= SYS_SEM_MAX)
		LWIP_DEBUGF(SYS_DEBUG, ("sys_sem_new: Out of semaphore resources"));
	if (sem_init(sem, shared, count) < 0) {
		LWIP_DEBUGF(SYS_DEBUG, ("sys_sem_new: Error while initializing semaphore 1: %d", errno));
		return ERR_MEM;
	}
	SemaphoreCnt++;

#if SYS_STATS
	lwip_stats.sys.sem.used++;
	if (lwip_stats.sys.sem.used > lwip_stats.sys.sem.max) {
		lwip_stats.sys.sem.max = lwip_stats.sys.sem.used;
	}
#endif
	return ERR_OK;
}

u32_t
sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
#define SYS_MSPERTICK (SYSTMR_INTERVAL/SYSTMR_CLK_FREQ_KHZ)
#define TICKS_TO_MS(x) ((x) * SYS_MSPERTICK)


	if (timeout) {	/* Try to acquire the semaphore within timeout. If not return */
		u32_t ticks = xget_clock_ticks();
		u32_t nticks = 0;
		if (!sem_timedwait(sem, timeout)) {	/* sem_timedwait returns 0 on success */
			nticks = xget_clock_ticks();
			if (nticks >= ticks)
				return TICKS_TO_MS(nticks-ticks);
			else {
				/* overflow condition */
				/* we'll assume that this has overflowed just once */
				return TICKS_TO_MS((0xffffffff - ticks) + nticks);
			}
		} else {
			return SYS_ARCH_TIMEOUT;
		}
	} else  {
		sem_wait(sem);
	}

	return 0;
}

void
sys_sem_signal(sys_sem_t *sem)
{
	sem_post(sem);
}

void
sys_sem_free(sys_sem_t *sem)
{

#if SYS_STATS
	lwip_stats.sys.sem.used--;
#endif
	sem_destroy(sem);
	*sem = SEM_FREE;
	if (SemaphoreCnt > 0)
		SemaphoreCnt--;

}

err_t
sys_mbox_new(sys_mbox_t* mbox, int size)
{
	int i;

	/* The size parameter is new in lwIP-1.3.0 (compared to lwIP-1.2.0.
	 * For now, we just make sure that our default size is bigger than the requested size
	 */
	if (SemaphoreCnt >= (SYS_SEM_MAX - 1))
		LWIP_DEBUGF(SYS_DEBUG, ("sys_sem_new: Out of semaphore resources"));

	if (size > SYS_MBOX_SIZE) {
		LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_new: Error - requested mailbox size (%d) > SYS_MBOX_SIZE", size));
		return ERR_MEM;
	}

	for (i = 0; i < SYS_MBOX_MAX; i++)
		if (!lwip_mbox[i].used)
			break;

	if (i == SYS_MBOX_MAX) {
		LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_new: Error - Out of mailbox resources."));
		return ERR_MEM;
	}

	memcpy((void *)mbox, (void *)&(lwip_mbox[i]), sizeof(sys_mbox_t));
	mbox->first = mbox->last = 0;

	/* mbox->mail indicates whether mail is available */
	if (sem_init(&(mbox->mail), 0, 0) < 0 ) {
		LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_new: Error - While initializing semaphore 'mail': %d.", errno));
		return ERR_MEM;
	}
	SemaphoreCnt++;

	/* mbox->mutex controls access to the mbox */
	if (sem_init(&(mbox->mutex), 0, 1) < 0 ) {
		LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_new: Error - While initializing semaphore 'mutex': %d.", errno));
		return ERR_MEM;
	}
	SemaphoreCnt++;
	mbox->used = 1;

#if SYS_STATS
	lwip_stats.sys.mbox.used++;
	lwip_stats.sys.sem.used += 2;
	if (lwip_stats.sys.mbox.used > lwip_stats.sys.mbox.max) {
		lwip_stats.sys.mbox.max = lwip_stats.sys.mbox.used;
	}
#endif /* SYS_STATS */

	return ERR_OK;
}

void
sys_mbox_free (sys_mbox_t *mbox)
{
	if(mbox != SYS_MBOX_NULL) {
#if SYS_STATS
		lwip_stats.sys.mbox.used--;
		lwip_stats.sys.sem.used -= 2;
#endif /* SYS_STATS */
		sem_wait (&(mbox->mutex));
		sem_destroy (&(mbox->mail));
		sem_destroy (&(mbox->mutex));
		mbox->used = 0;
		if (SemaphoreCnt > 1)
			SemaphoreCnt = SemaphoreCnt - 2;
	}
}

void
sys_mbox_post (sys_mbox_t *mbox, void *msg)
{
	u8_t first;

	/* first obtain mutex to access mbox */
	if (sem_wait(&mbox->mutex)) {
		LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_post: Error - While locking mutex for mbox: %d", errno));
		return;
	}

	/* post the message */
	mbox->msgs[mbox->last] = msg;

	if (mbox->last == mbox->first)
		first = 1;
	else
		first = 0;

	/* ignores overflow conditions (cannot post > SYS_MBOX_SIZE messages) */
	mbox->last++;
	if (mbox->last == SYS_MBOX_SIZE)
		mbox->last = 0;

	sem_post(&mbox->mail);

	sem_post (&mbox->mutex);
}

err_t
sys_mbox_trypost (sys_mbox_t *mbox, void *msg)
{
	u8_t first;

	/* first obtain mutex to access mbox */
	if (sem_trywait(&mbox->mutex) < 0) {
		return ERR_MEM;
	}

	/* post the message */
	mbox->msgs[mbox->last] = msg;

	if (mbox->last == mbox->first)
		first = 1;
	else
		first = 0;

	/* ignores overflow conditions (cannot post > SYS_MBOX_SIZE messages) */
	mbox->last++;
	if (mbox->last == SYS_MBOX_SIZE)
		mbox->last = 0;

	sem_post(&mbox->mail);

	sem_post (&mbox->mutex);

	return ERR_OK;
}

u32_t
sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
	u32_t start_ticks = 0;
	u32_t stop_ticks = 0;
	u32_t ticks = 0;

	/* The mutex lock is quick so we don't bother with the timeout stuff here. */
	sem_wait(&mbox->mutex);

	while (mbox->first == mbox->last) {
		/* no messages in mailbox, relinqush control */
		sem_post(&mbox->mutex);

		/* Block w/ timeout while waiting for a mail to arrive in the mailbox. */
		if (timeout) {    /* Try to acquire the semaphore within timeout. If not return */
			int pid;

			start_ticks = xget_clock_ticks();
			pid = get_currentPID();
			if (sem_timedwait(&mbox->mail, timeout)) {
				return SYS_ARCH_TIMEOUT;
			}
			stop_ticks = xget_clock_ticks();
		} else {
			sem_wait (&mbox->mail);
		}

		/* now that there is a message, regain control of mailbox */
		sem_wait (&mbox->mutex);
	}

	/* obtain the first message */
	if (msg != NULL) {
		*msg = mbox->msgs[mbox->first];
	}

	mbox->first++;
	if(mbox->first == SYS_MBOX_SIZE) {
		mbox->first = 0;
	}

	/* relinqush control of the mailbox */
	sem_post(&mbox->mutex);

	/* find out how much time it took us */
	if (stop_ticks >= start_ticks)
		ticks = stop_ticks - start_ticks;
	else
		ticks = (0xffffffff - start_ticks) + stop_ticks;

	return TICKS_TO_MS(ticks);
}

u32_t
sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
	/* required in lwIP-1.3.0. Initial naive implementation: */
	return sys_arch_mbox_fetch(mbox, msg, 1);
}


static struct sys_thread *
current_thread(void)
{
	int i;
	pthread_t me;

	me = pthread_self();

	for (i = 0; i < SYS_THREAD_MAX; i++) {
		if (lwip_thread[i].tid == me)
			return &lwip_thread[i];
	}

	LWIP_DEBUGF(SYS_DEBUG, ("current_thread: Error - could not find current thread"));
	abort();
}

static void *
thread_start(void *arg)
{
	struct thread_start_param *tp = arg;
	tp->function(tp->arg);
	tp->thread->tid = TID_FREE;      /* Free up the thread structure */
	return NULL;
}

sys_thread_t
sys_thread_new(const char *name, void (* function)(void *arg), void *arg, int stacksize, int prio)
{
	int i, ret;
	sys_thread_t thread = NULL;
	pthread_attr_t attr;
#if SCHED_TYPE == SCHED_PRIO
	struct sched_param sched;
#endif

	/* stacksize & name parameters are new in lwIP-1.3.0
	 * for stacksize, we just make sure that xilkernel default thread stack size is >
	 * than the request stack size.
	 * the name parameter is ignored.
	 */

	if (stacksize > PTHREAD_STACK_SIZE) {
		LWIP_DEBUGF(SYS_DEBUG, ("sys_thread_new: requested stack size (%d) \
					> xilkernel PTHREAD_STACK_SIZE (%d)",
					stacksize, PTHREAD_STACK_SIZE));
		return NULL;
	}

	for (i = 0; i < SYS_THREAD_MAX; i++)
		if (lwip_thread[i].tid == TID_FREE)
			break;

	if (i == SYS_THREAD_MAX) {
		LWIP_DEBUGF(SYS_DEBUG, ("sys_thread_new: Out of lwip thread structures"));
		return NULL;
	}


	thread = &lwip_thread[i];

	thread->tp.function = function;
	thread->tp.arg = arg;
	thread->tp.thread = thread;

	pthread_attr_init (&attr);
#if SCHED_TYPE == SCHED_PRIO
	sched.sched_priority = prio;
	pthread_attr_setschedparam(&attr, &sched);
#endif
	if((ret = pthread_create(&(thread->tid), &attr, thread_start, &(thread->tp))) != 0) {
		LWIP_DEBUGF(SYS_DEBUG, ("sys_thread_new: Error in pthread_create: %d", ret));
		abort();
	}

	return thread;
}

int sys_mbox_valid(sys_mbox_t *mbox)
{
	if (mbox->used == 0)
		return 0;
	return 1;
}

void sys_mbox_set_invalid(sys_mbox_t *mbox)
{

}

int sys_sem_valid(sys_sem_t *sem)
{
	if (*sem == SEM_FREE)
		return 0;
	return 1;
}

void sys_sem_set_invalid(sys_sem_t *sem)
{

}
#endif

#ifdef OS_IS_FREERTOS


#include "arch/sys_arch.h"

/* ------------------------ lwIP includes --------------------------------- */
#include "lwip/opt.h"

#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/sys.h"
#include "lwip/mem.h"
#include "lwip/stats.h"

/* Very crude mechanism used to determine if the critical section handling
functions are being called from an interrupt context or not.  This relies on
the interrupt handler setting this variable manually. */
extern u32 xInsideISR;

/*---------------------------------------------------------------------------*
 * Routine:  sys_mbox_new
 *---------------------------------------------------------------------------*
 * Description:
 *      Creates a new mailbox
 * Inputs:
 *      int size                -- Size of elements in the mailbox
 * Outputs:
 *      sys_mbox_t              -- Handle to new mailbox
 *---------------------------------------------------------------------------*/
err_t sys_mbox_new( sys_mbox_t *pxMailBox, int iSize )
{
err_t xReturn = ERR_MEM;

	*pxMailBox = xQueueCreate( iSize, sizeof( void * ) );

	if( *pxMailBox != NULL )
	{
		xReturn = ERR_OK;
		SYS_STATS_INC_USED( mbox );
	}
	return xReturn;
}


/*---------------------------------------------------------------------------*
 * Routine:  sys_mbox_free
 *---------------------------------------------------------------------------*
 * Description:
 *      Deallocates a mailbox. If there are messages still present in the
 *      mailbox when the mailbox is deallocated, it is an indication of a
 *      programming error in lwIP and the developer should be notified.
 * Inputs:
 *      sys_mbox_t mbox         -- Handle of mailbox
 * Outputs:
 *      sys_mbox_t              -- Handle to new mailbox
 *---------------------------------------------------------------------------*/
void sys_mbox_free( sys_mbox_t *pxMailBox )
{
unsigned long ulMessagesWaiting;

	ulMessagesWaiting = uxQueueMessagesWaiting( *pxMailBox );
	configASSERT( ( ulMessagesWaiting == 0 ) );

	#if SYS_STATS
	{
		if( ulMessagesWaiting != 0UL )
		{
			SYS_STATS_INC( mbox.err );
		}

		SYS_STATS_DEC( mbox.used );
	}
	#endif /* SYS_STATS */

	vQueueDelete( *pxMailBox );
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_mbox_post
 *---------------------------------------------------------------------------*
 * Description:
 *      Post the "msg" to the mailbox.
 * Inputs:
 *      sys_mbox_t mbox         -- Handle of mailbox
 *      void *data              -- Pointer to data to post
 *---------------------------------------------------------------------------*/
void sys_mbox_post( sys_mbox_t *pxMailBox, void *pxMessageToPost )
{
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
	if( xInsideISR != pdFALSE ) {
		xQueueSendToBackFromISR( *pxMailBox, &pxMessageToPost, &xHigherPriorityTaskWoken );
		if (xHigherPriorityTaskWoken == pdTRUE) {
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
		}
	}
	else
		xQueueSendToBack( *pxMailBox, &pxMessageToPost, portMAX_DELAY );
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_mbox_trypost
 *---------------------------------------------------------------------------*
 * Description:
 *      Try to post the "msg" to the mailbox.  Returns immediately with
 *      error if cannot.
 * Inputs:
 *      sys_mbox_t mbox         -- Handle of mailbox
 *      void *msg               -- Pointer to data to post
 * Outputs:
 *      err_t                   -- ERR_OK if message posted, else ERR_MEM
 *                                  if not.
 *---------------------------------------------------------------------------*/
err_t sys_mbox_trypost( sys_mbox_t *pxMailBox, void *pxMessageToPost )
{
err_t xReturn;
portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

	if( xInsideISR != pdFALSE )
	{
		xReturn = xQueueSendFromISR( *pxMailBox, &pxMessageToPost, &xHigherPriorityTaskWoken );
		if (xHigherPriorityTaskWoken == pdTRUE) {
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
		}
	}
	else
	{
		xReturn = xQueueSend( *pxMailBox, &pxMessageToPost, ( portTickType ) 0 );
	}

	if( xReturn == pdPASS )
	{
		xReturn = ERR_OK;
	}
	else
	{
		xil_printf("Queue is full\r\n");
		/* The queue was already full. */
		xReturn = ERR_MEM;
		SYS_STATS_INC( mbox.err );
	}

	return xReturn;
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_arch_mbox_fetch
 *---------------------------------------------------------------------------*
 * Description:
 *      Blocks the thread until a message arrives in the mailbox, but does
 *      not block the thread longer than "timeout" milliseconds (similar to
 *      the sys_arch_sem_wait() function). The "msg" argument is a result
 *      parameter that is set by the function (i.e., by doing "*msg =
 *      ptr"). The "msg" parameter maybe NULL to indicate that the message
 *      should be dropped.
 *
 *      The return values are the same as for the sys_arch_sem_wait() function:
 *      Number of milliseconds spent waiting or SYS_ARCH_TIMEOUT if there was a
 *      timeout.
 *
 *      Note that a function with a similar name, sys_mbox_fetch(), is
 *      implemented by lwIP.
 * Inputs:
 *      sys_mbox_t mbox         -- Handle of mailbox
 *      void **msg              -- Pointer to pointer to msg received
 *      u32_t timeout           -- Number of milliseconds until timeout
 * Outputs:
 *      u32_t                   -- SYS_ARCH_TIMEOUT if timeout, else number
 *                                  of milliseconds until received.
 *---------------------------------------------------------------------------*/
u32_t sys_arch_mbox_fetch( sys_mbox_t *pxMailBox, void **ppvBuffer, u32_t ulTimeOut )
{
void *pvDummy;
portTickType xStartTime, xEndTime, xElapsed;
unsigned long ulReturn;
portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

	xStartTime = xTaskGetTickCount();

	if( NULL == ppvBuffer )
	{
		ppvBuffer = &pvDummy;
	}

	if( ulTimeOut != 0UL )
	{
		if( xInsideISR != pdFALSE ) {
			if( pdTRUE == xQueueReceiveFromISR( *pxMailBox, &( *ppvBuffer ), &xHigherPriorityTaskWoken ) )
			{
				xEndTime = xTaskGetTickCount();
				xElapsed = ( xEndTime - xStartTime ) * portTICK_RATE_MS;
				ulReturn = xElapsed;
				if (xHigherPriorityTaskWoken == pdTRUE) {
					portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
				}
			}
			else
			{
				*ppvBuffer = NULL;
				ulReturn = SYS_ARCH_TIMEOUT;
			}
		} else {
		if( pdTRUE == xQueueReceive( *pxMailBox, &( *ppvBuffer ), ulTimeOut/ portTICK_RATE_MS ) )
		{
			xEndTime = xTaskGetTickCount();
			xElapsed = ( xEndTime - xStartTime ) * portTICK_RATE_MS;

			ulReturn = xElapsed;
		}
		else
		{
			/* Timed out. */
			*ppvBuffer = NULL;
			ulReturn = SYS_ARCH_TIMEOUT;
		}
	}
	}
	else
	{
		if( xInsideISR != pdFALSE ) {
			xQueueReceiveFromISR( *pxMailBox, &( *ppvBuffer ), &xHigherPriorityTaskWoken );
			if (xHigherPriorityTaskWoken == pdTRUE) {
				portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
			}
		}
		else
			xQueueReceive( *pxMailBox, &( *ppvBuffer ), portMAX_DELAY );
		xEndTime = xTaskGetTickCount();
		xElapsed = ( xEndTime - xStartTime ) * portTICK_RATE_MS;

		if( xElapsed == 0UL )
		{
			xElapsed = 1UL;
		}

		ulReturn = xElapsed;
	}

	return ulReturn;
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_arch_mbox_tryfetch
 *---------------------------------------------------------------------------*
 * Description:
 *      Similar to sys_arch_mbox_fetch, but if message is not ready
 *      immediately, we'll return with SYS_MBOX_EMPTY.  On success, 0 is
 *      returned.
 * Inputs:
 *      sys_mbox_t mbox         -- Handle of mailbox
 *      void **msg              -- Pointer to pointer to msg received
 * Outputs:
 *      u32_t                   -- SYS_MBOX_EMPTY if no messages.  Otherwise,
 *                                  return ERR_OK.
 *---------------------------------------------------------------------------*/
u32_t sys_arch_mbox_tryfetch( sys_mbox_t *pxMailBox, void **ppvBuffer )
{
void *pvDummy;
unsigned long ulReturn;
long lResult;
portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

	if( ppvBuffer== NULL )
	{
		ppvBuffer = &pvDummy;
	}

	if( xInsideISR != pdFALSE )
	{
		lResult = xQueueReceiveFromISR( *pxMailBox, &( *ppvBuffer ), &xHigherPriorityTaskWoken );
		if (xHigherPriorityTaskWoken == pdTRUE) {
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
		}
	}
	else
	{
		lResult = xQueueReceive( *pxMailBox, &( *ppvBuffer ), 0UL );
	}

	if( lResult == pdPASS )
	{
		ulReturn = ERR_OK;
	}
	else
	{
		ulReturn = SYS_MBOX_EMPTY;
	}

	return ulReturn;
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_sem_new
 *---------------------------------------------------------------------------*
 * Description:
 *      Creates and returns a new semaphore. The "ucCount" argument specifies
 *      the initial state of the semaphore.
 *      NOTE: Currently this routine only creates counts of 1 or 0
 * Inputs:
 *      sys_mbox_t mbox         -- Handle of mailbox
 *      u8_t ucCount              -- Initial ucCount of semaphore (1 or 0)
 * Outputs:
 *      sys_sem_t               -- Created semaphore or 0 if could not create.
 *---------------------------------------------------------------------------*/
err_t sys_sem_new( sys_sem_t *pxSemaphore, u8_t ucCount )
{
	(void) ucCount;
err_t xReturn = ERR_MEM;

	*pxSemaphore = xSemaphoreCreateBinary();

	if( *pxSemaphore != NULL )
	{
		xReturn = ERR_OK;
		SYS_STATS_INC_USED( sem );
	}
	else
	{
		xil_printf("Sem creation error\r\n");
		SYS_STATS_INC( sem.err );
	}

	return xReturn;
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_arch_sem_wait
 *---------------------------------------------------------------------------*
 * Description:
 *      Blocks the thread while waiting for the semaphore to be
 *      signaled. If the "timeout" argument is non-zero, the thread should
 *      only be blocked for the specified time (measured in
 *      milliseconds).
 *
 *      If the timeout argument is non-zero, the return value is the number of
 *      milliseconds spent waiting for the semaphore to be signaled. If the
 *      semaphore wasn't signaled within the specified time, the return value is
 *      SYS_ARCH_TIMEOUT. If the thread didn't have to wait for the semaphore
 *      (i.e., it was already signaled), the function may return zero.
 *
 *      Notice that lwIP implements a function with a similar name,
 *      sys_sem_wait(), that uses the sys_arch_sem_wait() function.
 * Inputs:
 *      sys_sem_t sem           -- Semaphore to wait on
 *      u32_t timeout           -- Number of milliseconds until timeout
 * Outputs:
 *      u32_t                   -- Time elapsed or SYS_ARCH_TIMEOUT.
 *---------------------------------------------------------------------------*/
u32_t sys_arch_sem_wait( sys_sem_t *pxSemaphore, u32_t ulTimeout )
{
portTickType xStartTime, xEndTime, xElapsed;
unsigned long ulReturn;
portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

	xStartTime = xTaskGetTickCount();

	if( ulTimeout != 0UL )
	{
		if( xInsideISR != pdFALSE ) {
			if( xSemaphoreTakeFromISR( *pxSemaphore, &xHigherPriorityTaskWoken ) == pdTRUE )
			{
				xEndTime = xTaskGetTickCount();
				xElapsed = (xEndTime - xStartTime) * portTICK_RATE_MS;
				ulReturn = xElapsed;
				if (xHigherPriorityTaskWoken == pdTRUE) {
					portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
				}
			}
			else
			{
				ulReturn = SYS_ARCH_TIMEOUT;
			}
		} else {
		if( xSemaphoreTake( *pxSemaphore, ulTimeout / portTICK_RATE_MS ) == pdTRUE )
		{
			xEndTime = xTaskGetTickCount();
			xElapsed = (xEndTime - xStartTime) * portTICK_RATE_MS;
			ulReturn = xElapsed;
		}
		else
		{
			ulReturn = SYS_ARCH_TIMEOUT;
		}
	}
	}
	else
	{
		if( xInsideISR != pdFALSE ) {
			xSemaphoreTakeFromISR( *pxSemaphore, &xHigherPriorityTaskWoken );
			if (xHigherPriorityTaskWoken == pdTRUE) {
				portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
			}
		}
		else
			xSemaphoreTake( *pxSemaphore, portMAX_DELAY );
		xEndTime = xTaskGetTickCount();
		xElapsed = ( xEndTime - xStartTime ) * portTICK_RATE_MS;

		if( xElapsed == 0UL )
		{
			xElapsed = 1UL;
		}

		ulReturn = xElapsed;
	}

	return ulReturn;
}

/** Create a new mutex
 * @param mutex pointer to the mutex to create
 * @return a new mutex */
err_t sys_mutex_new( sys_mutex_t *pxMutex )
{
err_t xReturn = ERR_MEM;

	*pxMutex = xSemaphoreCreateMutex();

	if( *pxMutex != NULL )
	{
		xReturn = ERR_OK;
		SYS_STATS_INC_USED( mutex );
	}
	else
	{
		xil_printf("Mutex creation error\r\n");
		SYS_STATS_INC( mutex.err );
	}

	return xReturn;
}

/** Lock a mutex
 * @param mutex the mutex to lock */
void sys_mutex_lock( sys_mutex_t *pxMutex )
{
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
	if( xInsideISR != pdFALSE ) {
		xSemaphoreTakeFromISR( *pxMutex, &xHigherPriorityTaskWoken );
		if (xHigherPriorityTaskWoken == pdTRUE) {
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
		}
	}
	else
		xSemaphoreTake( *pxMutex, portMAX_DELAY );
}

/** Unlock a mutex
 * @param mutex the mutex to unlock */
void sys_mutex_unlock(sys_mutex_t *pxMutex )
{
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
	if( xInsideISR != pdFALSE ) {
		xSemaphoreGiveFromISR( *pxMutex, &xHigherPriorityTaskWoken );
		if (xHigherPriorityTaskWoken == pdTRUE)
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
	else
	xSemaphoreGive( *pxMutex );
}


/** Delete a semaphore
 * @param mutex the mutex to delete */
void sys_mutex_free( sys_mutex_t *pxMutex )
{
	SYS_STATS_DEC( mutex.used );
	vQueueDelete( *pxMutex );
}


/*---------------------------------------------------------------------------*
 * Routine:  sys_sem_signal
 *---------------------------------------------------------------------------*
 * Description:
 *      Signals (releases) a semaphore
 * Inputs:
 *      sys_sem_t sem           -- Semaphore to signal
 *---------------------------------------------------------------------------*/
void sys_sem_signal( sys_sem_t *pxSemaphore )
{
portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

	if( xInsideISR != pdFALSE )
	{
		xSemaphoreGiveFromISR( *pxSemaphore, &xHigherPriorityTaskWoken );
		if (xHigherPriorityTaskWoken == pdTRUE) {
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
		}
	}
	else
	{
		xSemaphoreGive( *pxSemaphore );
	}
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_sem_free
 *---------------------------------------------------------------------------*
 * Description:
 *      Deallocates a semaphore
 * Inputs:
 *      sys_sem_t sem           -- Semaphore to free
 *---------------------------------------------------------------------------*/
void sys_sem_free( sys_sem_t *pxSemaphore )
{
	SYS_STATS_DEC(sem.used);
	vQueueDelete( *pxSemaphore );
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_init
 *---------------------------------------------------------------------------*
 * Description:
 *      Initialize sys arch
 *---------------------------------------------------------------------------*/
void sys_init(void)
{
}

u32_t sys_now(void)
{
	return ((xTaskGetTickCount() * 1000) / configTICK_RATE_HZ);
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_thread_new
 *---------------------------------------------------------------------------*
 * Description:
 *      Starts a new thread with priority "prio" that will begin its
 *      execution in the function "thread()". The "arg" argument will be
 *      passed as an argument to the thread() function. The id of the new
 *      thread is returned. Both the id and the priority are system
 *      dependent.
 * Inputs:
 *      char *name              -- Name of thread
 *      void (* thread)(void *arg) -- Pointer to function to run.
 *      void *arg               -- Argument passed into function
 *      int stacksize           -- Required stack amount in bytes
 *      int prio                -- Thread priority
 * Outputs:
 *      sys_thread_t            -- Pointer to per-thread timeouts.
 *---------------------------------------------------------------------------*/
sys_thread_t sys_thread_new( const char *pcName, void( *pxThread )( void *pvParameters ), void *pvArg, int iStackSize, int iPriority )
{
xTaskHandle xCreatedTask;
portBASE_TYPE xResult;
sys_thread_t xReturn;

	xResult = xTaskCreate( pxThread, ( const char * const) pcName, iStackSize, pvArg, iPriority, &xCreatedTask );

	if( xResult == pdPASS )
	{
		xReturn = xCreatedTask;
	}
	else
	{
		xReturn = NULL;
	}

	return xReturn;
}


/*
 * Prints an assertion messages and aborts execution.
 */
void sys_assert( const char *pcMessage )
{
	(void) pcMessage;

	for (;;)
	{
	}
}

#endif

#ifdef OS_IS_UCOSIII
#include "arch/sys_arch.h"

/* ------------------------ lwIP includes --------------------------------- */
#include "lwip/opt.h"

#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/sys.h"
#include "lwip/mem.h"
#include "lwip/stats.h"

#include <math.h>

SysArchTCB_t sys_arch_tcb_pool_buf[SYS_ARCH_TCB_MEMBUF_SIZE]; //tcb memory pool
unsigned char sys_arch_mbox_pool_buf[SYS_ARCH_MBOX_MEMBUF_SIZE]; //mbox memory pool
unsigned char sys_arch_sem_pool_buf[SYS_ARCH_SEM_MEMBUF_SIZE]; //sem memory pool
unsigned char sys_arch_mutex_pool_buf[SYS_ARCH_MUTEX_MEMBUF_SIZE]; //mutex memory pool
CPU_STK thread_stack_pool_buf[SYS_ARCH_THREAD_STACK_MEMBUF_SIZE]; //stack memory pool
OS_MEM	sys_arch_tcb_mem_pool;//tcp  mempool
OS_MEM	sys_arch_mbox_mem_pool;//mbox  mempool
OS_MEM	sys_arch_sem_mem_pool;//sem  mempool
OS_MEM	sys_arch_mutex_mem_pool;//mutex  mempool
OS_MEM	thread_stack_mem_pool;//stack  mempool

/* Very crude mechanism used to determine if the critical section handling
functions are being called from an interrupt context or not.  This relies on
the interrupt handler setting this variable manually. */
//extern u32 xInsideISR;

/*---------------------------------------------------------------------------*
 * Routine:  sys_mbox_new
 *---------------------------------------------------------------------------*
 * Description:
 *      Creates a new mailbox
 * Inputs:
 *      int size                -- Size of elements in the mailbox
 * Outputs:
 *      sys_mbox_t              -- Handle to new mailbox
 *---------------------------------------------------------------------------*/
err_t sys_mbox_new( sys_mbox_t *pxMailBox, int iSize )
{
	err_t xReturn = ERR_MEM;
	OS_ERR err;
	
	*pxMailBox = (sys_mbox_t)OSMemGet(&sys_arch_mbox_mem_pool, &err); //malloc mbox buf
	//*pxMailBox = (sys_mbox_t)malloc(sizeof(OS_Q));
	if(*pxMailBox == NULL){
		return ERR_MEM;
	}
	
    OSQCreate(*pxMailBox, "lwip_q_x", iSize, &err);
    if(err == OS_ERR_NONE){
    	xReturn = ERR_OK;
    	SYS_STATS_INC_USED( mbox );
    }else{
		OSMemPut(&sys_arch_mbox_mem_pool, (void *)(*pxMailBox), &err); 
	}
#if 0
	*pxMailBox = xQueueCreate( iSize, sizeof( void * ) );

	if( *pxMailBox != NULL )
	{
		xReturn = ERR_OK;
		SYS_STATS_INC_USED( mbox );
	}
#endif
	return xReturn;
}


/*---------------------------------------------------------------------------*
 * Routine:  sys_mbox_free
 *---------------------------------------------------------------------------*
 * Description:
 *      Deallocates a mailbox. If there are messages still present in the
 *      mailbox when the mailbox is deallocated, it is an indication of a
 *      programming error in lwIP and the developer should be notified.
 * Inputs:
 *      sys_mbox_t mbox         -- Handle of mailbox
 * Outputs:
 *      sys_mbox_t              -- Handle to new mailbox
 *---------------------------------------------------------------------------*/
void sys_mbox_free( sys_mbox_t *pxMailBox )
{
	unsigned long ulMessagesWaiting;
	OS_ERR      err;

	ulMessagesWaiting = OSQFlush(*pxMailBox, &err);
	LWIP_ASSERT("sys_mbox_free empty", ( ulMessagesWaiting == 0 ) );
#if 0
	ulMessagesWaiting = uxQueueMessagesWaiting( *pxMailBox );
	configASSERT( ( ulMessagesWaiting == 0 ) );
#endif

	#if SYS_STATS
	{
		if( ulMessagesWaiting != 0UL )
		{
			SYS_STATS_INC( mbox.err );
		}

		SYS_STATS_DEC( mbox.used );
	}
	#endif /* SYS_STATS */

	OSQDel(*pxMailBox, OS_OPT_DEL_ALWAYS, &err);
	OSMemPut(&sys_arch_mbox_mem_pool, (void *)(*pxMailBox), &err);
	//free((void *)(*pxMailBox));
#if 0
	vQueueDelete( *pxMailBox );
#endif
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_mbox_post
 *---------------------------------------------------------------------------*
 * Description:
 *      Post the "msg" to the mailbox.
 * Inputs:
 *      sys_mbox_t mbox         -- Handle of mailbox
 *      void *data              -- Pointer to data to post
 *---------------------------------------------------------------------------*/
void sys_mbox_post( sys_mbox_t *pxMailBox, void *pxMessageToPost )
{
	OS_ERR      err;

	OSQPost(*pxMailBox, pxMessageToPost, 0, OS_OPT_POST_FIFO, &err);
#if 0
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
	if( xInsideISR != pdFALSE ) {
		xQueueSendToBackFromISR( *pxMailBox, &pxMessageToPost, &xHigherPriorityTaskWoken );
		if (xHigherPriorityTaskWoken == pdTRUE) {
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
		}
	}
	else
		xQueueSendToBack( *pxMailBox, &pxMessageToPost, portMAX_DELAY );
#endif
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_mbox_trypost
 *---------------------------------------------------------------------------*
 * Description:
 *      Try to post the "msg" to the mailbox.  Returns immediately with
 *      error if cannot.
 * Inputs:
 *      sys_mbox_t mbox         -- Handle of mailbox
 *      void *msg               -- Pointer to data to post
 * Outputs:
 *      err_t                   -- ERR_OK if message posted, else ERR_MEM
 *                                  if not.
 *---------------------------------------------------------------------------*/
err_t sys_mbox_trypost( sys_mbox_t *pxMailBox, void *pxMessageToPost )
{
	OS_ERR      err;
	err_t xReturn;
#if 0
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
#endif

	OSQPost(*pxMailBox, pxMessageToPost, 0, OS_OPT_POST_FIFO, &err);
	if(err != OS_ERR_NONE){
		xil_printf("Fun: sys_mbox_trypost-> Queue is full\r\n");
		xReturn = ERR_MEM;
		SYS_STATS_INC( mbox.err );
	}else{
		xReturn = ERR_OK;
	}

#if 0
	if( xInsideISR != pdFALSE )
	{
		xReturn = xQueueSendFromISR( *pxMailBox, &pxMessageToPost, &xHigherPriorityTaskWoken );
		if (xHigherPriorityTaskWoken == pdTRUE) {
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
		}
	}
	else
	{
		xReturn = xQueueSend( *pxMailBox, &pxMessageToPost, ( portTickType ) 0 );
	}

	if( xReturn == pdPASS )
	{
		xReturn = ERR_OK;
	}
	else
	{
		xil_printf("Queue is full\r\n");
		/* The queue was already full. */
		xReturn = ERR_MEM;
		SYS_STATS_INC( mbox.err );
	}
#endif
	return xReturn;
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_arch_mbox_fetch
 *---------------------------------------------------------------------------*
 * Description:
 *      Blocks the thread until a message arrives in the mailbox, but does
 *      not block the thread longer than "timeout" milliseconds (similar to
 *      the sys_arch_sem_wait() function). The "msg" argument is a result
 *      parameter that is set by the function (i.e., by doing "*msg =
 *      ptr"). The "msg" parameter maybe NULL to indicate that the message
 *      should be dropped.
 *
 *      The return values are the same as for the sys_arch_sem_wait() function:
 *      Number of milliseconds spent waiting or SYS_ARCH_TIMEOUT if there was a
 *      timeout.
 *
 *      Note that a function with a similar name, sys_mbox_fetch(), is
 *      implemented by lwIP.
 * Inputs:
 *      sys_mbox_t mbox         -- Handle of mailbox
 *      void **msg              -- Pointer to pointer to msg received
 *      u32_t timeout           -- Number of milliseconds until timeout
 * Outputs:
 *      u32_t                   -- SYS_ARCH_TIMEOUT if timeout, else number
 *                                  of milliseconds until received.
 *---------------------------------------------------------------------------*/
u32_t sys_arch_mbox_fetch( sys_mbox_t *pxMailBox, void **ppvBuffer, u32_t ulTimeOut )
{
	void *pvDummy;
	OS_TICK  xStartTime, xEndTime, xElapsed;
	OS_ERR err;
	OS_MSG_SIZE msg_size;
#if 0
	portTickType xStartTime, xEndTime, xElapsed;
#endif
	unsigned long ulReturn;
#if 0
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
#endif

	xStartTime = OSTimeGet(&err);

	if( NULL == ppvBuffer )
	{
		ppvBuffer = &pvDummy;
	}

	if( ulTimeOut != 0UL ){ //none block
		if(((ulTimeOut * OS_CFG_TICK_RATE_HZ) / 1000) == 0){ //delay time small one tick time
			*ppvBuffer = OSQPend(*pxMailBox, 1, OS_OPT_PEND_NON_BLOCKING, &msg_size, NULL, &err);
		}else{
			*ppvBuffer = OSQPend(*pxMailBox, (ulTimeOut * OS_CFG_TICK_RATE_HZ) / 1000, \
					OS_OPT_PEND_NON_BLOCKING, &msg_size, NULL, &err);
		}
	}else{ //block
		*ppvBuffer = OSQPend(*pxMailBox, 0, OS_OPT_PEND_BLOCKING, &msg_size, NULL, &err);
	}

	if(*ppvBuffer == NULL){ //timeout
		ulReturn = SYS_ARCH_TIMEOUT;
	}else{
		xEndTime = OSTimeGet(&err);
		if(xEndTime < xStartTime){
			xElapsed = (pow(2, sizeof(OS_TICK)*8) - xStartTime + xEndTime) * (1000 / OS_CFG_TICK_RATE_HZ);
		}else{
			xElapsed = ( xEndTime - xStartTime ) * (1000 / OS_CFG_TICK_RATE_HZ);
		}

		if( xElapsed == 0UL )
		{
			xElapsed = 1UL;
		}

		ulReturn = xElapsed;
	}
#if 0
	xStartTime = xTaskGetTickCount();


	if( NULL == ppvBuffer )
	{
		ppvBuffer = &pvDummy;
	}

	if( ulTimeOut != 0UL )
	{
		if( xInsideISR != pdFALSE ) {
			if( pdTRUE == xQueueReceiveFromISR( *pxMailBox, &( *ppvBuffer ), &xHigherPriorityTaskWoken ) )
			{
				xEndTime = xTaskGetTickCount();
				xElapsed = ( xEndTime - xStartTime ) * portTICK_RATE_MS;
				ulReturn = xElapsed;
				if (xHigherPriorityTaskWoken == pdTRUE) {
					portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
				}
			}
			else
			{
				*ppvBuffer = NULL;
				ulReturn = SYS_ARCH_TIMEOUT;
			}
		} else {
		if( pdTRUE == xQueueReceive( *pxMailBox, &( *ppvBuffer ), ulTimeOut/ portTICK_RATE_MS ) )
		{
			xEndTime = xTaskGetTickCount();
			xElapsed = ( xEndTime - xStartTime ) * portTICK_RATE_MS;

			ulReturn = xElapsed;
		}
		else
		{
			/* Timed out. */
			*ppvBuffer = NULL;
			ulReturn = SYS_ARCH_TIMEOUT;
		}
	}
	}
	else
	{
		if( xInsideISR != pdFALSE ) {
			xQueueReceiveFromISR( *pxMailBox, &( *ppvBuffer ), &xHigherPriorityTaskWoken );
			if (xHigherPriorityTaskWoken == pdTRUE) {
				portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
			}
		}
		else
			xQueueReceive( *pxMailBox, &( *ppvBuffer ), portMAX_DELAY );
		xEndTime = xTaskGetTickCount();
		xElapsed = ( xEndTime - xStartTime ) * portTICK_RATE_MS;

		if( xElapsed == 0UL )
		{
			xElapsed = 1UL;
		}

		ulReturn = xElapsed;
	}
#endif
	return ulReturn;
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_arch_mbox_tryfetch
 *---------------------------------------------------------------------------*
 * Description:
 *      Similar to sys_arch_mbox_fetch, but if message is not ready
 *      immediately, we'll return with SYS_MBOX_EMPTY.  On success, 0 is
 *      returned.
 * Inputs:
 *      sys_mbox_t mbox         -- Handle of mailbox
 *      void **msg              -- Pointer to pointer to msg received
 * Outputs:
 *      u32_t                   -- SYS_MBOX_EMPTY if no messages.  Otherwise,
 *                                  return ERR_OK.
 *---------------------------------------------------------------------------*/
u32_t sys_arch_mbox_tryfetch( sys_mbox_t *pxMailBox, void **ppvBuffer )
{
	void *pvDummy;
	unsigned long ulReturn;
	OS_ERR err;
	OS_MSG_SIZE msg_size;
#if 0
long lResult;
portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
#endif

	if( ppvBuffer== NULL )
	{
		ppvBuffer = &pvDummy;
	}

	*ppvBuffer = OSQPend(*pxMailBox, 0, OS_OPT_PEND_NON_BLOCKING, &msg_size, NULL, &err);
	if(*ppvBuffer == NULL){
		ulReturn = SYS_MBOX_EMPTY;
	}else{
		ulReturn = ERR_OK;
	}

#if 0
	if( ppvBuffer== NULL )
	{
		ppvBuffer = &pvDummy;
	}

	if( xInsideISR != pdFALSE )
	{
		lResult = xQueueReceiveFromISR( *pxMailBox, &( *ppvBuffer ), &xHigherPriorityTaskWoken );
		if (xHigherPriorityTaskWoken == pdTRUE) {
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
		}
	}
	else
	{
		lResult = xQueueReceive( *pxMailBox, &( *ppvBuffer ), 0UL );
	}

	if( lResult == pdPASS )
	{
		ulReturn = ERR_OK;
	}
	else
	{
		ulReturn = SYS_MBOX_EMPTY;
	}
#endif

	return ulReturn;
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_sem_new
 *---------------------------------------------------------------------------*
 * Description:
 *      Creates and returns a new semaphore. The "ucCount" argument specifies
 *      the initial state of the semaphore.
 *      NOTE: Currently this routine only creates counts of 1 or 0
 * Inputs:
 *      sys_mbox_t mbox         -- Handle of mailbox
 *      u8_t ucCount              -- Initial ucCount of semaphore (1 or 0)
 * Outputs:
 *      sys_sem_t               -- Created semaphore or 0 if could not create.
 *---------------------------------------------------------------------------*/
err_t sys_sem_new( sys_sem_t *pxSemaphore, u8_t ucCount )
{
	(void) ucCount;
	err_t xReturn = ERR_MEM;
    OS_ERR err;
	
	*pxSemaphore = (sys_sem_t)OSMemGet(&sys_arch_sem_mem_pool, &err); //malloc sem buf
	//*pxSemaphore = (sys_sem_t)malloc(sizeof(OS_SEM));
	if(*pxSemaphore == NULL){
		return ERR_MEM;
	}

	OSSemCreate(*pxSemaphore, "lwip_sem_x", ucCount, &err);
	if(err == OS_ERR_NONE){
		xReturn = ERR_OK;
		SYS_STATS_INC_USED( sem );
	}else{
		OSMemPut(&sys_arch_sem_mem_pool, (void *)(*pxSemaphore), &err); 
		xil_printf("Sem creation error\r\n");
		SYS_STATS_INC( sem.err );
	}

#if 0
	*pxSemaphore = xSemaphoreCreateBinary();

	if( *pxSemaphore != NULL )
	{
		xReturn = ERR_OK;
		SYS_STATS_INC_USED( sem );
	}
	else
	{
		xil_printf("Sem creation error\r\n");
		SYS_STATS_INC( sem.err );
	}
#endif
	return xReturn;
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_arch_sem_wait
 *---------------------------------------------------------------------------*
 * Description:
 *      Blocks the thread while waiting for the semaphore to be
 *      signaled. If the "timeout" argument is non-zero, the thread should
 *      only be blocked for the specified time (measured in
 *      milliseconds).
 *
 *      If the timeout argument is non-zero, the return value is the number of
 *      milliseconds spent waiting for the semaphore to be signaled. If the
 *      semaphore wasn't signaled within the specified time, the return value is
 *      SYS_ARCH_TIMEOUT. If the thread didn't have to wait for the semaphore
 *      (i.e., it was already signaled), the function may return zero.
 *
 *      Notice that lwIP implements a function with a similar name,
 *      sys_sem_wait(), that uses the sys_arch_sem_wait() function.
 * Inputs:
 *      sys_sem_t sem           -- Semaphore to wait on
 *      u32_t timeout           -- Number of milliseconds until timeout
 * Outputs:
 *      u32_t                   -- Time elapsed or SYS_ARCH_TIMEOUT.
 *---------------------------------------------------------------------------*/
u32_t sys_arch_sem_wait( sys_sem_t *pxSemaphore, u32_t ulTimeout )
{
	unsigned long ulReturn;
#if 0
	portTickType xStartTime, xEndTime, xElapsed;
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
#endif
	OS_TICK  xStartTime, xEndTime, xElapsed;
	OS_ERR err;

	xStartTime = OSTimeGet(&err);

	if( ulTimeout != 0UL ){ //none block
		if(((ulTimeout * OS_CFG_TICK_RATE_HZ) / 1000) == 0){ //delay time small one tick time
			OSSemPend(*pxSemaphore, 1, OS_OPT_PEND_NON_BLOCKING, NULL, &err);
		}else{
			OSSemPend(*pxSemaphore, (ulTimeout * OS_CFG_TICK_RATE_HZ) / 1000, OS_OPT_PEND_NON_BLOCKING, \
					NULL, &err);
		}
	}else{ //block
		OSSemPend(*pxSemaphore, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
	}

	if(err == OS_ERR_NONE){
		xEndTime = OSTimeGet(&err);
		if(xEndTime < xStartTime){
			xElapsed = (pow(2, sizeof(OS_TICK)*8) - xStartTime + xEndTime) * (1000 / OS_CFG_TICK_RATE_HZ);
		}else{
			xElapsed = ( xEndTime - xStartTime ) * (1000 / OS_CFG_TICK_RATE_HZ);
		}

		if( xElapsed == 0UL )
		{
			xElapsed = 1UL;
		}

		ulReturn = xElapsed;
	}else{
		ulReturn = SYS_ARCH_TIMEOUT;
	}

#if 0
	xStartTime = xTaskGetTickCount();

	if( ulTimeout != 0UL )
	{
		if( xInsideISR != pdFALSE ) {
			if( xSemaphoreTakeFromISR( *pxSemaphore, &xHigherPriorityTaskWoken ) == pdTRUE )
			{
				xEndTime = xTaskGetTickCount();
				xElapsed = (xEndTime - xStartTime) * portTICK_RATE_MS;
				ulReturn = xElapsed;
				if (xHigherPriorityTaskWoken == pdTRUE) {
					portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
				}
			}
			else
			{
				ulReturn = SYS_ARCH_TIMEOUT;
			}
		} else {
		if( xSemaphoreTake( *pxSemaphore, ulTimeout / portTICK_RATE_MS ) == pdTRUE )
		{
			xEndTime = xTaskGetTickCount();
			xElapsed = (xEndTime - xStartTime) * portTICK_RATE_MS;
			ulReturn = xElapsed;
		}
		else
		{
			ulReturn = SYS_ARCH_TIMEOUT;
		}
	}
	}
	else
	{
		if( xInsideISR != pdFALSE ) {
			xSemaphoreTakeFromISR( *pxSemaphore, &xHigherPriorityTaskWoken );
			if (xHigherPriorityTaskWoken == pdTRUE) {
				portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
			}
		}
		else
			xSemaphoreTake( *pxSemaphore, portMAX_DELAY );
		xEndTime = xTaskGetTickCount();
		xElapsed = ( xEndTime - xStartTime ) * portTICK_RATE_MS;

		if( xElapsed == 0UL )
		{
			xElapsed = 1UL;
		}

		ulReturn = xElapsed;
	}
#endif
	return ulReturn;
}

/** Create a new mutex
 * @param mutex pointer to the mutex to create
 * @return a new mutex */
err_t sys_mutex_new( sys_mutex_t *pxMutex )
{
	err_t xReturn = ERR_MEM;
    OS_ERR err;

    *pxMutex = (sys_mutex_t)OSMemGet(&sys_arch_mutex_mem_pool, &err); //malloc mutex buf
	//*pxMutex = (sys_mutex_t)malloc(sizeof(OS_MUTEX));
	if(*pxMutex == NULL){
		return ERR_MEM;
	}

	OSMutexCreate(*pxMutex, "lwip_mutex_x", &err);
	if(err == OS_ERR_NONE){
		xReturn = ERR_OK;
		SYS_STATS_INC_USED( mutex );
	}else{
		OSMemPut(&sys_arch_mutex_mem_pool, (void *)(*pxMutex), &err); 
		xil_printf("Mutex creation error\r\n");
		SYS_STATS_INC( mutex.err );
	}

#if 0
	*pxMutex = xSemaphoreCreateMutex();

	if( *pxMutex != NULL )
	{
		xReturn = ERR_OK;
		SYS_STATS_INC_USED( mutex );
	}
	else
	{
		xil_printf("Mutex creation error\r\n");
		SYS_STATS_INC( mutex.err );
	}
#endif
	return xReturn;
}

/** Lock a mutex
 * @param mutex the mutex to lock */
void sys_mutex_lock( sys_mutex_t *pxMutex )
{
	OS_ERR err;

	OSMutexPend(*pxMutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);

#if 0
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
	if( xInsideISR != pdFALSE ) {
		xSemaphoreTakeFromISR( *pxMutex, &xHigherPriorityTaskWoken );
		if (xHigherPriorityTaskWoken == pdTRUE) {
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
		}
	}
	else
		xSemaphoreTake( *pxMutex, portMAX_DELAY );
#endif
}

/** Unlock a mutex
 * @param mutex the mutex to unlock */
void sys_mutex_unlock(sys_mutex_t *pxMutex )
{
	OS_ERR err;

	OSMutexPost(*pxMutex, OS_OPT_POST_NONE, &err);

#if 0
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
	if( xInsideISR != pdFALSE ) {
		xSemaphoreGiveFromISR( *pxMutex, &xHigherPriorityTaskWoken );
		if (xHigherPriorityTaskWoken == pdTRUE)
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
	else
	xSemaphoreGive( *pxMutex );
#endif
}


/** Delete a semaphore
 * @param mutex the mutex to delete */
void sys_mutex_free( sys_mutex_t *pxMutex )
{
	OS_ERR err;

	SYS_STATS_DEC( mutex.used );

	OSMutexDel(*pxMutex, OS_OPT_DEL_NO_PEND, &err);
	OSMemPut(&sys_arch_mutex_mem_pool, (void *)(*pxMutex), &err);
	//free((void *)(*pxMutex));
#if 0
	vQueueDelete( *pxMutex );
#endif
}


/*---------------------------------------------------------------------------*
 * Routine:  sys_sem_signal
 *---------------------------------------------------------------------------*
 * Description:
 *      Signals (releases) a semaphore
 * Inputs:
 *      sys_sem_t sem           -- Semaphore to signal
 *---------------------------------------------------------------------------*/
void sys_sem_signal( sys_sem_t *pxSemaphore )
{
	OS_ERR err;

	OSSemPost(*pxSemaphore, OS_OPT_POST_ALL, &err);

#if 0
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

	if( xInsideISR != pdFALSE )
	{
		xSemaphoreGiveFromISR( *pxSemaphore, &xHigherPriorityTaskWoken );
		if (xHigherPriorityTaskWoken == pdTRUE) {
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
		}
	}
	else
	{
		xSemaphoreGive( *pxSemaphore );
	}
#endif
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_sem_free
 *---------------------------------------------------------------------------*
 * Description:
 *      Deallocates a semaphore
 * Inputs:
 *      sys_sem_t sem           -- Semaphore to free
 *---------------------------------------------------------------------------*/
void sys_sem_free( sys_sem_t *pxSemaphore )
{
	OS_ERR err;

	SYS_STATS_DEC(sem.used);

	OSSemDel(*pxSemaphore, OS_OPT_DEL_NO_PEND, &err);
    OSMemPut(&sys_arch_sem_mem_pool, (void *)(*pxSemaphore), &err);
	//free((void *)(*pxSemaphore));
#if 0
	vQueueDelete( *pxSemaphore );
#endif
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_init
 *---------------------------------------------------------------------------*
 * Description:
 *      Initialize sys arch
 *---------------------------------------------------------------------------*/
void sys_init(void)
{
	OS_ERR err;
	
	//init memory pool
	OSMemCreate(&sys_arch_tcb_mem_pool, "sys_arch_tcb_mem_poll", &sys_arch_tcb_pool_buf[0], \
			SYS_ARCH_TCB_MEMBUF_SIZE/sizeof(SysArchTCB_t), sizeof(SysArchTCB_t), &err);
	if(err != OS_ERR_NONE){
		return ;
	}
	
	OSMemCreate(&sys_arch_mbox_mem_pool, "sys_arch_mbox_pool_buf", &sys_arch_mbox_pool_buf[0], \
			SYS_ARCH_MBOX_MEMBUF_SIZE/sizeof(OS_Q), sizeof(OS_Q), &err);
	if(err != OS_ERR_NONE){
		return ;
	}
	
	OSMemCreate(&sys_arch_sem_mem_pool, "sys_arch_sem_pool_buf", &sys_arch_sem_pool_buf[0], \
			SYS_ARCH_SEM_MEMBUF_SIZE/sizeof(OS_SEM), sizeof(OS_SEM), &err);
	if(err != OS_ERR_NONE){
		return ;
	}
	
	OSMemCreate(&sys_arch_mutex_mem_pool, "sys_arch_mutex_pool_buf", &sys_arch_mutex_pool_buf[0], \
			SYS_ARCH_MUTEX_MEMBUF_SIZE/sizeof(OS_MUTEX), sizeof(OS_MUTEX), &err);
	if(err != OS_ERR_NONE){
		return ;
	}
	
	OSMemCreate(&thread_stack_mem_pool, "thread_stack_pool_buf", &thread_stack_pool_buf[0], \
			SYS_ARCH_THREAD_STACK_MEMBUF_SIZE/SYS_ARCH_THREAD_MAX_STACK_SIZE, \
			SYS_ARCH_THREAD_MAX_STACK_SIZE * sizeof(CPU_STK), &err);
	if(err != OS_ERR_NONE){
		return ;
	}
}

u32_t sys_now(void)
{
	OS_ERR err;

	return ((OSTimeGet(&err) * 1000) / OS_CFG_TICK_RATE_HZ);
#if 0
	return ((xTaskGetTickCount() * 1000) / configTICK_RATE_HZ);
#endif
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_thread_new
 *---------------------------------------------------------------------------*
 * Description:
 *      Starts a new thread with priority "prio" that will begin its
 *      execution in the function "thread()". The "arg" argument will be
 *      passed as an argument to the thread() function. The id of the new
 *      thread is returned. Both the id and the priority are system
 *      dependent.
 * Inputs:
 *      char *name              -- Name of thread
 *      void (* thread)(void *arg) -- Pointer to function to run.
 *      void *arg               -- Argument passed into function
 *      int stacksize           -- Required stack amount in bytes
 *      int prio                -- Thread priority
 * Outputs:
 *      sys_thread_t            -- Pointer to per-thread timeouts.
 *---------------------------------------------------------------------------*/
sys_thread_t sys_thread_new( const char *pcName, void( *pxThread )( void *pvParameters ), void *pvArg, int iStackSize, int iPriority )
{
	OS_ERR err;
	SysArchTCB_t *thread_tcb;
	OS_TCB *task_tcb;
	CPU_STK *p_stack;
	sys_thread_t xReturn;
	
	//malloc OS_TCP space
	thread_tcb = (SysArchTCB_t *)OSMemGet(&sys_arch_tcb_mem_pool, &err); //malloc tcb buf
	//thread_tcb = (OS_TCB *)malloc(sizeof(OS_TCB));
	if(thread_tcb == NULL){
		xReturn = NULL;
		return xReturn;
	}
	
	task_tcb = &(thread_tcb->os_tcb); //get ucos OS_TCB pointer
	
	if(iStackSize > SYS_ARCH_THREAD_MAX_STACK_SIZE){
		OSMemPut(&sys_arch_tcb_mem_pool, (void *)(thread_tcb), &err);
		return NULL;
	}

	//malloc stack space
	p_stack = (CPU_STK *)OSMemGet(&thread_stack_mem_pool, &err); //malloc stack buf
	//p_stack = (CPU_STK *)malloc(iStackSize);
	if(p_stack == NULL){
		OSMemPut(&sys_arch_tcb_mem_pool, (void *)(thread_tcb), &err);
		xReturn = NULL;
		return xReturn;
	}
	
	thread_tcb->p_base_stack = (void *)p_stack; //save stack address
	
	if(iPriority > (OS_CFG_PRIO_MAX - 30)){
		iPriority = OS_CFG_PRIO_MAX - 30;
	}else if(iPriority == 0){
		iPriority = OS_CFG_PRIO_MAX - 3 - iPriority;
	}else{
		iPriority = OS_CFG_PRIO_MAX - 30 - iPriority;
	}

	OSTaskCreate(task_tcb, (CPU_CHAR *)pcName, pxThread, pvArg, iPriority, \
			p_stack, 0, iStackSize, 0, 0, NULL, OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR, &err);
	if(err == OS_ERR_NONE){
		xReturn = task_tcb;
	}else{
		OSMemPut(&sys_arch_tcb_mem_pool, (void *)(thread_tcb), &err);
		OSMemPut(&thread_stack_mem_pool, (void *)(p_stack), &err);
		xReturn = NULL;
	}

#if 0
	xTaskHandle xCreatedTask;
	portBASE_TYPE xResult;
	sys_thread_t xReturn;

	xResult = xTaskCreate( pxThread, ( const char * const) pcName, iStackSize, pvArg, iPriority, &xCreatedTask );

	if( xResult == pdPASS )
	{
		xReturn = xCreatedTask;
	}
	else
	{
		xReturn = NULL;
	}
#endif
	return xReturn;
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_thread_delete
 *---------------------------------------------------------------------------*
 * Description:
 *      delete a thread
 *---------------------------------------------------------------------------*/
void sys_thread_delete(sys_thread_t thread)
{
	OS_ERR err;
	SysArchTCB_t *p_thread_tcb;
	CPU_STK *p_stack;
	OS_TCB  p_tcb;
	
	if(thread == NULL){
		return ;
	}
	
	p_thread_tcb = container_of(thread, SysArchTCB_t, os_tcb);
	p_stack = p_thread_tcb->p_base_stack;
	p_tcb = *((OS_TCB *)thread);
	
	//put tcb buf
	OSMemPut(&sys_arch_tcb_mem_pool, p_thread_tcb, &err);
	
	//put stack buf
	OSMemPut(&thread_stack_mem_pool, p_stack, &err);
	
	OSTaskDel(&p_tcb, &err);
}


/*
 * Prints an assertion messages and aborts execution.
 */
void sys_assert( const char *pcMessage )
{
	(void) pcMessage;

	for (;;)
	{
	}
}
/*-------------------------------------------------------------------------*
 * End of File:  sys_arch.c
 *-------------------------------------------------------------------------*/


#endif

