/* $NetBSD$ */

/*-
 * Copyright (c) 2019 The NetBSD Foundation, Inc.
 * All rights reserved.
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

#include <sys/cdefs.h>
__COPYRIGHT("@(#) Copyright (c) 2019\
 The NetBSD Foundation, inc. All rights reserved.");
__RCSID("$NetBSD$");

#include <sys/mman.h>
#include <atomic.h>
#include <errno.h>
#include <lwp.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <atf-c.h>

#include <libc/include/futex_private.h>

#define	STACK_SIZE	65536

static volatile int futex_word;

struct lwp_data {
	ucontext_t	context;
	void		*stack_base;
	lwpid_t		lwpid;
	tid_t		threadid;

	int		futex_error;
};

#define	WAITER_LWP		0
#define	NLWPS			1

struct lwp_data lwp_data[NLWPS];

static void
setup_lwp_context(struct lwp_data *d, void (*func)(void *))
{

	memset(d, 0, sizeof(*d));
	d->stack_base = mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_STACK | MAP_PRIVATE, -1, 0);
	ATF_REQUIRE(d->stack_base != MAP_FAILED);
	_lwp_makecontext(&d->context, func, d, NULL, d->stack_base, STACK_SIZE);
	d->threadid = 0;
}

static void
do_cleanup(void)
{
	int i;

	for (i = 0; i < NLWPS; i++) {
		struct lwp_data *d = &lwp_data[i];
		if (d->stack_base != NULL && d->stack_base != MAP_FAILED) {
			(void) munmap(d->stack_base, STACK_SIZE);
		}
	}
	memset(lwp_data, 0, sizeof(lwp_data));
	futex_word = 0;
}

static void
waiter_lwp(void *arg)
{
	struct lwp_data *d = arg;

	d->threadid = _lwp_gettid();

	futex_word = 1;
	membar_sync();

	if (__futex(&futex_word, FUTEX_WAIT | FUTEX_PRIVATE_FLAG,
		    1, NULL, NULL, 0, 0) == -1) {
		d->futex_error = errno;
		_lwp_exit();
	}

	do {
		membar_sync();
		sleep(1);
	} while (futex_word != 0);

	if (__futex(&futex_word, FUTEX_WAIT | FUTEX_PRIVATE_FLAG,
		    0, NULL, NULL, 0, 0) == -1) {
		d->futex_error = errno;
		_lwp_exit();
	}

	futex_word = 2;
	membar_sync();

	_lwp_exit();
}

ATF_TC_WITH_CLEANUP(futex_basic_wait_wake);
ATF_TC_HEAD(futex_basic_wait_wake, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "tests basic futex WAIT + WAKE operations");
}

ATF_TC_BODY(futex_basic_wait_wake, tc)
{
	struct lwp_data *wlwp = &lwp_data[WAITER_LWP];

	setup_lwp_context(wlwp, waiter_lwp);

	printf("futex_basic_wait_wake: creating watier LWP\n");

	ATF_REQUIRE(_lwp_create(&wlwp->context, 0, &wlwp->lwpid) == 0);

	printf("futex_basic_wait_wake: waiting for LWP %d to enter futex\n",
	    wlwp->lwpid);

	do {
		membar_sync();
		sleep(1);
		if (wlwp->futex_error) {
			printf("futex_basic_wait_wake: futex error = %d\n",
			    wlwp->futex_error);
			ATF_REQUIRE(wlwp->futex_error == 0);
		}
	} while (futex_word != 1);

	printf("futex_basic_wait_wake: waking 1 waiter\n");

	futex_word = 0;
	membar_sync();

	ATF_REQUIRE(__futex(&futex_word, FUTEX_WAKE | FUTEX_PRIVATE_FLAG,
			    1, NULL, NULL, 0, 0) == 1);

	do {
		membar_sync();
		sleep(1);
	} while (futex_word != 2);

	printf("futex_basic_wait_wake: reaping LWP %d\n", wlwp->lwpid);

	ATF_REQUIRE(_lwp_wait(wlwp->lwpid, NULL) == 0);
}

ATF_TC_CLEANUP(futex_basic_wait_wake, tc)
{
	do_cleanup();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, futex_basic_wait_wake);

	return atf_no_error();
}
