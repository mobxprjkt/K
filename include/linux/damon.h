/* SPDX-License-Identifier: GPL-2.0 */
/*
 * DAMON api
 *
 * Author: SeongJae Park <sjpark@amazon.de>
 */

#ifndef _DAMON_H_
#define _DAMON_H_

#include <linux/mutex.h>
#include <linux/time64.h>
#include <linux/types.h>

struct damon_ctx;

/**
 * struct damon_primitive	Monitoring primitives for given use cases.
 *
 * @init:			Initialize primitive-internal data structures.
 * @update:			Update primitive-internal data structures.
 * @prepare_access_checks:	Prepare next access check of target regions.
 * @check_accesses:		Check the accesses to target regions.
 * @reset_aggregated:		Reset aggregated accesses monitoring results.
 * @target_valid:		Determine if the target is valid.
 * @cleanup:			Clean up the context.
 *
 * DAMON can be extended for various address spaces and usages.  For this,
 * users should register the low level primitives for their target address
 * space and usecase via the &damon_ctx.primitive.  Then, the monitoring thread
 * (&damon_ctx.kdamond) calls @init and @prepare_access_checks before starting
 * the monitoring, @update after each &damon_ctx.primitive_update_interval, and
 * @check_accesses, @target_valid and @prepare_access_checks after each
 * &damon_ctx.sample_interval.  Finally, @reset_aggregated is called after each
 * &damon_ctx.aggr_interval.
 *
 * @init should initialize primitive-internal data structures.  For example,
 * this could be used to construct proper monitoring target regions and link
 * those to @damon_ctx.target.
 * @update should update the primitive-internal data structures.  For example,
 * this could be used to update monitoring target regions for current status.
 * @prepare_access_checks should manipulate the monitoring regions to be
 * prepared for the next access check.
 * @check_accesses should check the accesses to each region that made after the
 * last preparation and update the number of observed accesses of each region.
 * @reset_aggregated should reset the access monitoring results that aggregated
 * by @check_accesses.
 * @target_valid should check whether the target is still valid for the
 * monitoring.
 * @cleanup is called from @kdamond just before its termination.
 */
struct damon_primitive {
	void (*init)(struct damon_ctx *context);
	void (*update)(struct damon_ctx *context);
	void (*prepare_access_checks)(struct damon_ctx *context);
	void (*check_accesses)(struct damon_ctx *context);
	void (*reset_aggregated)(struct damon_ctx *context);
	bool (*target_valid)(void *target);
	void (*cleanup)(struct damon_ctx *context);
};

/*
 * struct damon_callback	Monitoring events notification callbacks.
 *
 * @before_start:	Called before starting the monitoring.
 * @after_sampling:	Called after each sampling.
 * @after_aggregation:	Called after each aggregation.
 * @before_terminate:	Called before terminating the monitoring.
 * @private:		User private data.
 *
 * The monitoring thread (&damon_ctx.kdamond) calls @before_start and
 * @before_terminate just before starting and finishing the monitoring,
 * respectively.  Therefore, those are good places for installing and cleaning
 * @private.
 *
 * The monitoring thread calls @after_sampling and @after_aggregation for each
 * of the sampling intervals and aggregation intervals, respectively.
 * Therefore, users can safely access the monitoring results without additional
 * protection.  For the reason, users are recommended to use these callback for
 * the accesses to the results.
 *
 * If any callback returns non-zero, monitoring stops.
 */
struct damon_callback {
	void *private;

	int (*before_start)(struct damon_ctx *context);
	int (*after_sampling)(struct damon_ctx *context);
	int (*after_aggregation)(struct damon_ctx *context);
	int (*before_terminate)(struct damon_ctx *context);
};

/**
 * struct damon_ctx - Represents a context for each monitoring.  This is the
 * main interface that allows users to set the attributes and get the results
 * of the monitoring.
 *
 * @sample_interval:		The time between access samplings.
 * @aggr_interval:		The time between monitor results aggregations.
 * @primitive_update_interval:	The time between monitoring primitive updates.
 *
 * For each @sample_interval, DAMON checks whether each region is accessed or
 * not.  It aggregates and keeps the access information (number of accesses to
 * each region) for @aggr_interval time.  DAMON also checks whether the target
 * memory regions need update (e.g., by ``mmap()`` calls from the application,
 * in case of virtual memory monitoring) and applies the changes for each
 * @primitive_update_interval.  All time intervals are in micro-seconds.
 * Please refer to &struct damon_primitive and &struct damon_callback for more
 * detail.
 *
 * @kdamond:		Kernel thread who does the monitoring.
 * @kdamond_stop:	Notifies whether kdamond should stop.
 * @kdamond_lock:	Mutex for the synchronizations with @kdamond.
 *
 * For each monitoring context, one kernel thread for the monitoring is
 * created.  The pointer to the thread is stored in @kdamond.
 *
 * Once started, the monitoring thread runs until explicitly required to be
 * terminated or every monitoring target is invalid.  The validity of the
 * targets is checked via the &damon_primitive.target_valid of @primitive.  The
 * termination can also be explicitly requested by writing non-zero to
 * @kdamond_stop.  The thread sets @kdamond to NULL when it terminates.
 * Therefore, users can know whether the monitoring is ongoing or terminated by
 * reading @kdamond.  Reads and writes to @kdamond and @kdamond_stop from
 * outside of the monitoring thread must be protected by @kdamond_lock.
 *
 * Note that the monitoring thread protects only @kdamond and @kdamond_stop via
 * @kdamond_lock.  Accesses to other fields must be protected by themselves.
 *
 * @primitive:	Set of monitoring primitives for given use cases.
 * @callback:	Set of callbacks for monitoring events notifications.
 *
 * @target:	Pointer to the user-defined monitoring target.
 */
struct damon_ctx {
	unsigned long sample_interval;
	unsigned long aggr_interval;
	unsigned long primitive_update_interval;

/* private: internal use only */
	struct timespec64 last_aggregation;
	struct timespec64 last_primitive_update;

/* public: */
	struct task_struct *kdamond;
	bool kdamond_stop;
	struct mutex kdamond_lock;

	struct damon_primitive primitive;
	struct damon_callback callback;

	void *target;
};

#ifdef CONFIG_DAMON

struct damon_ctx *damon_new_ctx(void);
void damon_destroy_ctx(struct damon_ctx *ctx);
int damon_set_attrs(struct damon_ctx *ctx, unsigned long sample_int,
		unsigned long aggr_int, unsigned long primitive_upd_int);

int damon_start(struct damon_ctx **ctxs, int nr_ctxs);
int damon_stop(struct damon_ctx **ctxs, int nr_ctxs);

#endif	/* CONFIG_DAMON */

#endif	/* _DAMON_H */
