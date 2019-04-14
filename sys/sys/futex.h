/*	$NetBSD$	*/

/*-
 * Copyright (c) 2005 Emmanuel Dreyfus, all rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Emmanuel Dreyfus
 * 4. The name of the author may not be used to endorse or promote 
 *    products derived from this software without specific prior written 
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE THE AUTHOR AND CONTRIBUTORS ``AS IS'' 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS 
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SYS_FUTEX_H_
#define _SYS_FUTEX_H_

#include <sys/timespec.h>

#define FUTEX_WAIT			  0 
#define FUTEX_WAKE			  1
#define FUTEX_FD			  2
#define FUTEX_REQUEUE			  3
#define FUTEX_CMP_REQUEUE		  4
#define FUTEX_WAKE_OP			  5
#define FUTEX_LOCK_PI			  6
#define FUTEX_UNLOCK_PI			  7
#define FUTEX_TRYLOCK_PI		  8
#define FUTEX_WAIT_BITSET		  9
#define FUTEX_WAKE_BITSET		 10
#define FUTEX_WAIT_REQUEUE_PI		 11
#define FUTEX_CMP_REQUEUE_PI		 12

#define FUTEX_PRIVATE_FLAG		128
#define FUTEX_CLOCK_REALTIME		256
#define FUTEX_CMD_MASK		\
    (~(FUTEX_PRIVATE_FLAG|FUTEX_CLOCK_REALTIME))

#define FUTEX_OP_SET		0
#define FUTEX_OP_ADD		1
#define FUTEX_OP_OR		2
#define FUTEX_OP_ANDN		3
#define FUTEX_OP_XOR		4
#define FUTEX_OP_OPARG_SHIFT	8

#define FUTEX_OP_CMP_EQ		0
#define FUTEX_OP_CMP_NE		1
#define FUTEX_OP_CMP_LT		2
#define FUTEX_OP_CMP_LE		3
#define FUTEX_OP_CMP_GT		4
#define FUTEX_OP_CMP_GE		5

#define FUTEX_WAITERS		0x80000000
#define FUTEX_OWNER_DIED	0x40000000
#define FUTEX_TID_MASK		0x3fffffff

#define FUTEX_BITSET_MATCH_ANY  0xffffffff

/*
 * The robust futex ABI consists of an array of 3 longwords, the address
 * of which is registered with the kernel on a per-thread basis:
 *
 *	0: A pointer to a singly-linked list of "lock entries".  If the
 *	   list is empty, this points back to the list itself.
 *
 *	1: An offset from address of the "lock entry" to the 32-bit futex
 *	   word associated with that lock entry (may be negative).
 *
 *	2: A "pending" pointer, for locks are are in the process of being
 *	   acquired or released.
 */
#define _FUTEX_ROBUST_HEAD_LIST		0
#define _FUTEX_ROBUST_HEAD_OFFSET	1
#define _FUTEX_ROBUST_HEAD_PENDING	2
#define _FUTEX_ROBUST_HEAD_NWORDS	3
#define _FUTEX_ROBUST_HEAD_SIZE		(_FUTEX_ROBUST_HEAD_NWORDS * \
					 sizeof(u_long))
#ifdef _LP64
#define _FUTEX_ROBUST_HEAD_SIZE32	(_FUTEX_ROBUST_HEAD_NWORDS * \
					 sizeof(uint32_t))
#endif /* _LP64 */

#ifdef __LIBC_FUTEX_PRIVATE
struct futex_robust_list {
	struct futex_robust_list	*next;
};

struct futex_robust_list_head {
	struct futex_robust_list	list;
	long				futex_offset;
	struct futex_robust_list	*pending_list;
};
#endif /* __LIBC_FUTEX_PRIVATE */

#ifdef _KERNEL
struct lwp;

int	futex_robust_head_lookup(struct lwp *, lwpid_t, void **);
void	futex_release_all_lwp(struct lwp *);
int	futex_func(int *, int, int, const struct timespec *, int *, int,
	    int, register_t *);
void	futex_sys_init(void);
void	futex_sys_fini(void);
#endif /* _KERNEL */

#endif /* ! _SYS_FUTEX_H_ */
