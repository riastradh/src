/*	$NetBSD$	*/

/*
 * SPDX-License-Identifier: MIT
 *
 * i915_sw_fence.h - library routines for N:M synchronisation points
 *
 * Copyright (C) 2016 Intel Corporation
 */

#ifndef _I915_SW_FENCE_H_
#define _I915_SW_FENCE_H_

#include <linux/dma-fence.h>
#include <linux/gfp.h>
#include <linux/kref.h>
#include <linux/notifier.h> /* for NOTIFY_DONE */
#include <linux/wait.h>
#include <drm/drm_wait_netbsd.h>

struct completion;
struct dma_resv;

struct i915_sw_fence {
#ifdef __linux__
	wait_queue_head_t wait;
#else
	drm_waitqueue_t wait;
	spinlock_t lock;
#endif
	unsigned long flags;
	atomic_t pending;
	int error;
};

#define I915_SW_FENCE_CHECKED_BIT	0 /* used internally for DAG checking */
#define I915_SW_FENCE_PRIVATE_BIT	1 /* available for use by owner */
#define I915_SW_FENCE_MASK		(~3)

enum i915_sw_fence_notify {
	FENCE_COMPLETE,
	FENCE_FREE
};

typedef int (*i915_sw_fence_notify_t)(struct i915_sw_fence *,
				      enum i915_sw_fence_notify state);
#define __i915_sw_fence_call __aligned(4)

void __i915_sw_fence_init(struct i915_sw_fence *fence,
			  i915_sw_fence_notify_t fn,
			  const char *name,
			  struct lock_class_key *key);
#if IS_ENABLED(CONFIG_LOCKDEP)
#define i915_sw_fence_init(fence, fn)				\
do {								\
	static struct lock_class_key __key;			\
								\
	__i915_sw_fence_init((fence), (fn), #fence, &__key);	\
} while (0)
#else
#define i915_sw_fence_init(fence, fn)				\
	__i915_sw_fence_init((fence), (fn), NULL, NULL)
#endif

void i915_sw_fence_reinit(struct i915_sw_fence *fence);

#ifdef CONFIG_DRM_I915_SW_FENCE_DEBUG_OBJECTS
void i915_sw_fence_fini(struct i915_sw_fence *fence);
#else
static inline void i915_sw_fence_fini(struct i915_sw_fence *fence) {}
#endif

void i915_sw_fence_commit(struct i915_sw_fence *fence);

int i915_sw_fence_await_sw_fence(struct i915_sw_fence *fence,
				 struct i915_sw_fence *after,
#ifdef __linux__
				 wait_queue_entry_t *wq);
#else
				 drm_waitqueue_t wq);
#endif
int i915_sw_fence_await_sw_fence_gfp(struct i915_sw_fence *fence,
				     struct i915_sw_fence *after,
				     gfp_t gfp);

struct i915_sw_dma_fence_cb {
	struct dma_fence_cb base;
	struct i915_sw_fence *fence;
};

int __i915_sw_fence_await_dma_fence(struct i915_sw_fence *fence,
				    struct dma_fence *dma,
				    struct i915_sw_dma_fence_cb *cb);
int i915_sw_fence_await_dma_fence(struct i915_sw_fence *fence,
				  struct dma_fence *dma,
				  unsigned long timeout,
				  gfp_t gfp);

int i915_sw_fence_await_reservation(struct i915_sw_fence *fence,
				    struct dma_resv *resv,
				    const struct dma_fence_ops *exclude,
				    bool write,
				    unsigned long timeout,
				    gfp_t gfp);

void i915_sw_fence_await(struct i915_sw_fence *fence);
void i915_sw_fence_complete(struct i915_sw_fence *fence);

static inline bool i915_sw_fence_signaled(const struct i915_sw_fence *fence)
{
	return atomic_read(&fence->pending) <= 0;
}

static inline bool i915_sw_fence_done(const struct i915_sw_fence *fence)
{
	return atomic_read(&fence->pending) < 0;
}

static inline void i915_sw_fence_wait(struct i915_sw_fence *fence)
{
#ifdef __linux__
	wait_event(fence->wait, i915_sw_fence_done(fence));
#else
	if (cold) {
		unsigned timo = 1000;
		while (timo --> 0) {
			if (!i915_sw_fence_done(fence))
				return;
			DELAY(100);
		}
		printf("%s: spinout", __func__);
		return;
	}
	int ret;
	spin_lock(&fence->lock);
	DRM_SPIN_WAIT_NOINTR_UNTIL(ret, &fence->wait, &fence->lock,
	    i915_sw_fence_done(fence));
	spin_unlock(&fence->lock);
	KASSERT(ret == 0);
#endif
}

static inline void
i915_sw_fence_set_error_once(struct i915_sw_fence *fence, int error)
{
	if (unlikely(error))
		cmpxchg(&fence->error, 0, error);
}

#endif /* _I915_SW_FENCE_H_ */
