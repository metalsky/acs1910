/*
 * RT-Mutexes: blocking mutual exclusion locks with PI support
 *
 * started by Ingo Molnar and Thomas Gleixner:
 *
 *  Copyright (C) 2004-2006 Red Hat, Inc., Ingo Molnar <mingo@redhat.com>
 *  Copyright (C) 2006 Timesys Corp., Thomas Gleixner <tglx@timesys.com>
 *
 * This code is based on the rt.c implementation in the preempt-rt tree.
 * Portions of said code are
 *
 *  Copyright (C) 2004  LynuxWorks, Inc., Igor Manyilov, Bill Huey
 *  Copyright (C) 2006  Esben Nielsen
 *  Copyright (C) 2006  Kihon Technologies Inc.,
 *			Steven Rostedt <rostedt@goodmis.org>
 *
 * See rt.c in preempt-rt for proper credits and further information
 */
#include <linux/config.h>
#include <linux/rt_lock.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/kallsyms.h>
#include <linux/syscalls.h>
#include <linux/interrupt.h>
#include <linux/plist.h>
#include <linux/fs.h>
#include <linux/debug_locks.h>

#include "rtmutex_common.h"

#ifdef CONFIG_DEBUG_RT_MUTEXES
# include "rtmutex-debug.h"
#else
# include "rtmutex.h"
#endif

static void printk_task(struct task_struct *p)
{
	if (p)
		printk("%16s:%5d [%p, %3d]", p->comm, p->pid, p, p->prio);
	else
		printk("<none>");
}

static void printk_lock(struct rt_mutex *lock, int print_owner)
{
	if (lock->name)
		printk(" [%p] {%s}\n",
			lock, lock->name);
	else
		printk(" [%p] {%s:%d}\n",
			lock, lock->file, lock->line);

	if (print_owner && rt_mutex_owner(lock)) {
		printk(".. ->owner: %p\n", lock->owner);
		printk(".. held by:  ");
		printk_task(rt_mutex_owner(lock));
		printk("\n");
	}
}

void rt_mutex_debug_task_free(struct task_struct *task)
{
	DEBUG_LOCKS_WARN_ON(!plist_head_empty(&task->pi_waiters));
	DEBUG_LOCKS_WARN_ON(task->pi_blocked_on);
}

/*
 * We fill out the fields in the waiter to store the information about
 * the deadlock. We print when we return. act_waiter can be NULL in
 * case of a remove waiter operation.
 */
void debug_rt_mutex_deadlock(int detect, struct rt_mutex_waiter *act_waiter,
			     struct rt_mutex *lock)
{
	struct task_struct *task;

	if (!debug_locks || detect || !act_waiter)
		return;

	task = rt_mutex_owner(act_waiter->lock);
	if (task && task != current) {
		act_waiter->deadlock_task_pid = task->pid;
		act_waiter->deadlock_lock = lock;
	}
}

void debug_rt_mutex_print_deadlock(struct rt_mutex_waiter *waiter)
{
	struct task_struct *task;

	if (!waiter->deadlock_lock || !debug_locks)
		return;

	task = find_task_by_pid(waiter->deadlock_task_pid);
	if (!task)
		return;

	if (!debug_locks_off())
		return;

	printk("\n============================================\n");
	printk(  "[ BUG: circular locking deadlock detected! ]\n");
	printk(  "--------------------------------------------\n");
	printk("%s/%d is deadlocking current task %s/%d\n\n",
	       task->comm, task->pid, current->comm, current->pid);

	printk("\n1) %s/%d is trying to acquire this lock:\n",
	       current->comm, current->pid);
	printk_lock(waiter->lock, 1);

	printk("\n2) %s/%d is blocked on this lock:\n", task->comm, task->pid);
	printk_lock(waiter->deadlock_lock, 1);

	debug_show_held_locks(current);
	debug_show_held_locks(task);

	printk("\n%s/%d's [blocked] stackdump:\n\n", task->comm, task->pid);
	show_stack(task, NULL);
	printk("\n%s/%d's [current] stackdump:\n\n",
	       current->comm, current->pid);
	dump_stack();
	debug_show_all_locks();

	printk("[ turning off deadlock detection."
	       "Please report this trace. ]\n\n");
}

void debug_rt_mutex_lock(struct rt_mutex *lock)
{
}

void debug_rt_mutex_unlock(struct rt_mutex *lock)
{
	if (debug_locks)
		DEBUG_LOCKS_WARN_ON(rt_mutex_owner(lock) != current);
}

void
debug_rt_mutex_proxy_lock(struct rt_mutex *lock, struct task_struct *powner)
{
}

void debug_rt_mutex_proxy_unlock(struct rt_mutex *lock)
{
	DEBUG_LOCKS_WARN_ON(!rt_mutex_owner(lock));
}

void debug_rt_mutex_init_waiter(struct rt_mutex_waiter *waiter)
{
	memset(waiter, 0x11, sizeof(*waiter));
	plist_node_init(&waiter->list_entry, MAX_PRIO);
	plist_node_init(&waiter->pi_list_entry, MAX_PRIO);
}

void debug_rt_mutex_free_waiter(struct rt_mutex_waiter *waiter)
{
	DEBUG_LOCKS_WARN_ON(!plist_node_empty(&waiter->list_entry));
	DEBUG_LOCKS_WARN_ON(!plist_node_empty(&waiter->pi_list_entry));
	DEBUG_LOCKS_WARN_ON(waiter->task);
	memset(waiter, 0x22, sizeof(*waiter));
}

void debug_rt_mutex_init(struct rt_mutex *lock, const char *name)
{
	/*
	 * Make sure we are not reinitializing a held lock:
	 */
	debug_check_no_locks_freed((void *)lock, sizeof(*lock));
	lock->name = name;
}

void
rt_mutex_deadlock_account_lock(struct rt_mutex *lock, struct task_struct *task)
{
#ifdef CONFIG_DEBUG_PREEMPT
	trace_special(0, 0, task->lock_count);
	if (task->lock_count >= MAX_LOCK_STACK) {
		if (!debug_locks_off())
			return;
		printk("BUG: %s/%d: lock count overflow!\n",
			task->comm, task->pid);
		dump_stack();
		return;
	}
#ifdef CONFIG_PREEMPT_RT
	task->owned_lock[task->lock_count] = lock;
#endif
	task->lock_count++;
#endif
}

void rt_mutex_deadlock_account_unlock(struct task_struct *task)
{
#ifdef CONFIG_DEBUG_PREEMPT
	trace_special(0, 0, task->lock_count);
	if (!task->lock_count) {
		if (!debug_locks_off())
			return;
		printk("BUG: %s/%d: lock count underflow!\n",
			task->comm, task->pid);
		dump_stack();
		return;
	}
	task->lock_count--;
#ifdef CONFIG_PREEMPT_RT
	task->owned_lock[task->lock_count] = NULL;
#endif
#endif
}
