/*	$NetBSD: subr_pserialize.c,v 1.17 2019/12/05 03:21:29 riastradh Exp $	*/

/*-
 * Copyright (c) 2010, 2011 The NetBSD Foundation, Inc.
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

/*
 * Passive serialization.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_pserialize.c,v 1.17 2019/12/05 03:21:29 riastradh Exp $");

#include <sys/param.h>
#include <sys/atomic.h>
#include <sys/cpu.h>
#include <sys/evcnt.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/pserialize.h>
#include <sys/xcall.h>

struct pserialize {
	lwp_t *			psz_owner;
};

static struct {
	kmutex_t		lock;
	kcondvar_t		cv;
	struct lwp *		active;
	uint64_t		gen;
	struct evcnt		perform_evcnt;
	struct evcnt		xc_evcnt;
	struct evcnt		shared_evcnt;
} pserialize __cacheline_aligned = {
	.gen = 0,
	.perform_evcnt = EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "pserialize",
	    "perform"),
	.xc_evcnt = EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "pserialize",
	    "xcall"),
	.shared_evcnt = EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "pserialize",
	    "shared"),
};

/*
 * pserialize_init:
 *
 *	Initialize passive serialization structures.
 */
void
pserialize_init(void)
{

	mutex_init(&pserialize.lock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&pserialize.cv, "pserial");
	evcnt_attach_static(&pserialize.perform_evcnt);
	evcnt_attach_static(&pserialize.xc_evcnt);
	evcnt_attach_static(&pserialize.shared_evcnt);
}

/*
 * pserialize_create:
 *
 *	Create and initialize a passive serialization object.
 */
pserialize_t
pserialize_create(void)
{
	pserialize_t psz;

	psz = kmem_zalloc(sizeof(*psz), KM_SLEEP);
	return psz;
}

/*
 * pserialize_destroy:
 *
 *	Destroy a passive serialization object.
 */
void
pserialize_destroy(pserialize_t psz)
{

	KASSERT(psz->psz_owner == NULL);
	kmem_free(psz, sizeof(*psz));
}

/*
 * pserialize_perform:
 *
 *	Perform the write side of passive serialization.  This operation
 *	MUST be serialized at a caller level (e.g. with a mutex or by a
 *	single-threaded use).
 */
void
pserialize_perform(pserialize_t psz)
{

	KASSERT(!cpu_intr_p());
	KASSERT(!cpu_softintr_p());

	if (__predict_false(panicstr != NULL)) {
		return;
	}
	KASSERT(psz->psz_owner == NULL);

	if (__predict_false(mp_online == false)) {
		pserialize.perform_evcnt.ev_count++;
		return;
	}

	psz->psz_owner = curlwp;

	mutex_enter(&pserialize.lock);
	pserialize.perform_evcnt.ev_count++;

	/* Check whether there's any xcall going on already.  */
	if (pserialize.active) {
		/* Someone else is issuing an xcall.  Wait our turn.  */
		uint64_t gen = pserialize.gen;
		do {
			cv_wait(&pserialize.cv, &pserialize.lock);
			/* Check how many generations have passed.  */
			if (pserialize.gen - gen >= 2) {
				/*
				 * At least two generations have
				 * passed, meaning someone else issued
				 * an xcall since we started, and it
				 * has completed, so our job is done.
				 */
				pserialize.shared_evcnt.ev_count++;
				goto out;
			}
		} while (pserialize.active);
	}

	/*
	 * There are no xcalls pending, and while waiting, we did not
	 * see two generations pass.  Claim the throne and broadcast a
	 * NOP to all CPUs; then wait until all CPUs complete.
	 */
	pserialize.active = curlwp;
	xc_barrier(XC_HIGHPRI);

	/*
	 * Relinquish the throne and advance the generation.  Use
	 * cv_signal so only one thread is woken to perform the next
	 * xcall.
	 */
	pserialize.active = NULL;
	cv_signal(&pserialize.cv);
	pserialize.gen++;

out:	mutex_exit(&pserialize.lock);

	KASSERT(psz->psz_owner == curlwp);
	psz->psz_owner = NULL;
}

int
pserialize_read_enter(void)
{
	int s;

	s = splsoftserial();
	curcpu()->ci_psz_read_depth++;
	__insn_barrier();
	return s;
}

void
pserialize_read_exit(int s)
{

	KASSERT(kpreempt_disabled());

	__insn_barrier();
	if (__predict_false(curcpu()->ci_psz_read_depth-- == 0))
		panic("mismatching pserialize_read_exit()");
	splx(s);
}

/*
 * pserialize_in_read_section:
 *
 *	True if the caller is in a pserialize read section.  To be used
 *	only for diagnostic assertions where we want to guarantee the
 *	condition like:
 *
 *		KASSERT(pserialize_in_read_section());
 */
bool
pserialize_in_read_section(void)
{

	return kpreempt_disabled() && curcpu()->ci_psz_read_depth > 0;
}

/*
 * pserialize_not_in_read_section:
 *
 *	True if the caller is not in a pserialize read section.  To be
 *	used only for diagnostic assertions where we want to guarantee
 *	the condition like:
 *
 *		KASSERT(pserialize_not_in_read_section());
 */
bool
pserialize_not_in_read_section(void)
{
	bool notin;

	kpreempt_disable();
	notin = (curcpu()->ci_psz_read_depth == 0);
	kpreempt_enable();

	return notin;
}
