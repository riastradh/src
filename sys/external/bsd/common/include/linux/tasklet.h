/*	$NetBSD$	*/

/*-
 * Copyright (c) 2018 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Taylor R. Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_LINUX_TASKLET_H_
#define	_LINUX_TASKLET_H_

#include <linux/atomic.h>

/* namespace */
#define	tasklet_disable			linux_tasklet_disable
#define	tasklet_disable_sync_once	linux_tasklet_disable_sync_once
#define	tasklet_enable			linux_tasklet_enable
#define	tasklet_enable_sync_once	linux_tasklet_enable_sync_once
#define	tasklet_hi_schedule		linux_tasklet_hi_schedule
#define	tasklet_init			linux_tasklet_init
#define	tasklet_is_enabled		linux_tasklet_is_enabled
#define	tasklet_kill			linux_tasklet_kill
#define	tasklet_schedule		linux_tasklet_schedule
#define	tasklet_struct			linux_tasklet_struct

struct tasklet_struct {
	SIMPLEQ_ENTRY(tasklet_struct)	tl_entry;
	/* begin Linux API */
	void				(*func)(unsigned long);
	unsigned long			data;
	atomic_t			count;
	volatile unsigned long		state;
	/* end Linux API */
};

#define TASKLET_STATE_SCHED	1
#define TASKLET_STATE_RUN	0

#define	DEFINE_TASKLET(name, func, data)				      \
	struct tasklet_struct name = {					      \
	    .state = 0,							      \
	    .count = 0,							      \
	    .func = (func),						      \
	    .data = (data),						      \
	}

#define	DEFINE_TASKLET_DISABLED(name, func, data)			      \
	struct tasklet_struct name = {					      \
	    .state = 0,							      \
	    .count = 1,							      \
	    .func = (func),						      \
	    .data = (data),						      \
	}

static inline int
tasklet_trylock(struct tasklet_struct *ts)
{
	return !test_and_set_bit(TASKLET_STATE_RUN, &ts->state);
}

static inline void
tasklet_unlock(struct tasklet_struct *ts)
{
	smp_mb__before_atomic();
	clear_bit(TASKLET_STATE_RUN, &ts->state);
}

static inline void
tasklet_unlock_wait(struct tasklet_struct *ts)
{
	while (test_bit(TASKLET_STATE_RUN, &ts->state))
		barrier();
}

int	linux_tasklets_init(void);
void	linux_tasklets_fini(void);

void	tasklet_init(struct tasklet_struct *, void (*)(unsigned long),
	    unsigned long);
void	tasklet_disable(struct tasklet_struct *);
void	tasklet_enable(struct tasklet_struct *);
void	tasklet_schedule(struct tasklet_struct *);
void	tasklet_hi_schedule(struct tasklet_struct *);
void	tasklet_kill(struct tasklet_struct *);

/* i915drmkms hack */
void	tasklet_disable_sync_once(struct tasklet_struct *);
void	tasklet_enable_sync_once(struct tasklet_struct *);
bool	tasklet_is_enabled(const struct tasklet_struct *);

#endif	/* _LINUX_TASKLET_H_ */
