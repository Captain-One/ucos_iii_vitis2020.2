/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * Copyright (C) 2007 - 2019 Xilinx, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
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
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

#ifndef __SYS_XILINX_ARCH_H__
#define __SYS_XILINX_ARCH_H__

#ifdef __cplusplus
extern "C" {
#endif
#include "lwipopts.h"

#ifdef OS_IS_XILKERNEL

#include "arch/cc.h"
#include "semaphore.h"
#include "os_config.h"

#define SYS_MBOX_NULL NULL
#define SYS_SEM_NULL  NULL
#define SYS_MBOX_SIZE   500
#define SYS_SEM_MAX     MAX_SEM
#define SYS_MBOX_MAX    (MAX_SEM/2)
#define SYS_THREAD_MAX  MAX_PTHREADS

#define SEM_FREE   0xffffffff
#define TID_FREE   0xffffffff

struct sys_mbox_msg {
	struct sys_mbox_msg *next;
	void *msg;
};

struct sys_mbox_s {
	u8_t  used;
	u16_t first, last;
	void *msgs[SYS_MBOX_SIZE];
	sem_t mail;
	sem_t mutex;
};


typedef sem_t sys_sem_t;

struct sys_mbox_s;
typedef struct sys_mbox_s sys_mbox_t;

struct sys_thread;
typedef struct sys_thread *sys_thread_t;

typedef u32_t sys_prot_t;
#endif

#ifdef OS_IS_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"

#define SYS_MBOX_NULL					( ( xQueueHandle ) NULL )
#define SYS_SEM_NULL					( ( xSemaphoreHandle ) NULL )
#define SYS_DEFAULT_THREAD_STACK_DEPTH	configMINIMAL_STACK_SIZE

typedef xSemaphoreHandle sys_sem_t;
typedef xSemaphoreHandle sys_mutex_t;
typedef xQueueHandle sys_mbox_t;
typedef xTaskHandle sys_thread_t;

typedef unsigned long sys_prot_t;

#define sys_mbox_valid( x ) ( ( ( *x ) == NULL) ? pdFALSE : pdTRUE )
#define sys_mbox_set_invalid( x ) ( ( *x ) = NULL )
#define sys_sem_valid( x ) ( ( ( *x ) == NULL) ? pdFALSE : pdTRUE )
#define sys_sem_set_invalid( x ) ( ( *x ) = NULL )
#endif

#ifdef OS_IS_UCOSIII
#include "os.h"

#define SYS_MBOX_NULL					( ( OS_Q *) NULL )
#define SYS_SEM_NULL					( ( OS_SEM *) NULL )
#define SYS_DEFAULT_THREAD_STACK_DEPTH	OS_CFG_IDLE_TASK_STK_SIZE

#ifndef container_of
#define container_of(ptr, type, member) ({\
			const typeof(((type *)0)->member) *__mptr = (ptr); \
			(type *)((char *)__mptr - offsetof(type, member));})
#endif

typedef struct sys_arch_tcb_{
	void *p_base_stack; //base stack address，release use
	OS_TCB os_tcb; //ucos os tcb
}SysArchTCB_t;

typedef OS_SEM* sys_sem_t;
typedef OS_MUTEX* sys_mutex_t;
typedef OS_Q* sys_mbox_t;
typedef OS_TCB* sys_thread_t;

typedef unsigned long sys_prot_t;

#define sys_mbox_valid( x ) ( ( ( *x ) == NULL) ? DEF_FALSE : DEF_TRUE )
#define sys_mbox_set_invalid( x ) ( ( *x ) = NULL )
#define sys_sem_valid( x ) ( ( ( *x ) == NULL) ? DEF_FALSE : DEF_TRUE )
#define sys_sem_set_invalid( x ) ( ( *x ) = NULL )
#endif

#ifdef __cplusplus
}
#endif

#endif /* __SYS_XILINX_ARCH_H__ */
