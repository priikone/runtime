/*

  silcschedule.c

  Author: Pekka Riikonen <priikone@silcnet.org>

  Copyright (C) 1998 - 2008 Pekka Riikonen

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

*/

#include "silcruntime.h"

/************************** Types and definitions ***************************/

/* Connected event context */
typedef struct SilcScheduleEventConnectionStruct {
  SilcSchedule schedule;
  SilcTaskEventCallback callback;
  void *context;
  struct SilcScheduleEventConnectionStruct *next;
} *SilcScheduleEventConnection;

/* Platform specific implementation */
extern const SilcScheduleOps schedule_ops;

static void silc_schedule_task_remove(SilcSchedule schedule, SilcTask task);
static void silc_schedule_dispatch_fd(SilcSchedule schedule);
static void silc_schedule_dispatch_timeout(SilcSchedule schedule,
					   SilcBool dispatch_all);
SILC_TASK_CALLBACK(silc_schedule_event_del_timeout);

/************************ Static utility functions **************************/

/* Fd task hash table destructor */

static void silc_schedule_fd_destructor(void *key, void *context,
					void *user_context)
{
  silc_free(context);
}

/* Executes file descriptor tasks. Invalid tasks are removed here. */

static void silc_schedule_dispatch_fd(SilcSchedule schedule)
{
  SilcTaskFd task;
  SilcTask t;

  /* The dispatch list includes only valid tasks, and tasks that have
     something to dispatch.  Dispatching is atomic; no matter if another
     thread invalidates a task when we unlock, we dispatch to completion. */
  SILC_SCHEDULE_UNLOCK(schedule);
  silc_list_start(schedule->fd_dispatch);
  while ((task = silc_list_get(schedule->fd_dispatch))) {
    t = (SilcTask)task;

    /* Is the task ready for reading */
    if (task->revents & SILC_TASK_READ)
      t->callback(schedule, schedule->app_context, SILC_TASK_READ,
		  task->fd, t->context);

    /* Is the task ready for writing */
    if (t->valid && task->revents & SILC_TASK_WRITE)
      t->callback(schedule, schedule->app_context, SILC_TASK_WRITE,
		  task->fd, t->context);
  }
  SILC_SCHEDULE_LOCK(schedule);

  /* Remove invalidated tasks */
  silc_list_start(schedule->fd_dispatch);
  while ((task = silc_list_get(schedule->fd_dispatch)))
    if (silc_unlikely(!task->header.valid))
      silc_schedule_task_remove(schedule, (SilcTask)task);
}

/* Executes all tasks whose timeout has expired. The task is removed from
   the task queue after the callback function has returned. Also, invalid
   tasks are removed here. */

static void silc_schedule_dispatch_timeout(SilcSchedule schedule,
					   SilcBool dispatch_all)
{
  SilcTask t;
  SilcTaskTimeout task;
  struct timeval curtime;
  int count = 0;

  SILC_LOG_DEBUG(("Running timeout tasks"));

  silc_gettimeofday(&curtime);

  /* First task in the task queue has always the earliest timeout. */
  silc_list_start(schedule->timeout_queue);
  task = silc_list_get(schedule->timeout_queue);
  if (silc_unlikely(!task))
    return;
  do {
    t = (SilcTask)task;

    /* Remove invalid task */
    if (silc_unlikely(!t->valid)) {
      silc_schedule_task_remove(schedule, t);
      continue;
    }

    /* Execute the task if the timeout has expired */
    if (silc_compare_timeval(&task->timeout, &curtime) > 0 && !dispatch_all)
      break;

    t->valid = FALSE;
    SILC_SCHEDULE_UNLOCK(schedule);
    t->callback(schedule, schedule->app_context, SILC_TASK_EXPIRE, 0,
		t->context);
    SILC_SCHEDULE_LOCK(schedule);

    /* Remove the expired task */
    silc_schedule_task_remove(schedule, t);

    /* Balance when we have lots of small timeouts */
    if (silc_unlikely((++count) > 40))
      break;
  } while (silc_likely((task = silc_list_get(schedule->timeout_queue))));
}

/* Calculates next timeout. This is the timeout value when at earliest some
   of the timeout tasks expire. If this is in the past, they will be
   dispatched now. */

static void silc_schedule_select_timeout(SilcSchedule schedule)
{
  SilcTask t;
  SilcTaskTimeout task;
  struct timeval curtime;
  SilcBool dispatch = TRUE;

  /* Get the current time */
  silc_gettimeofday(&curtime);
  schedule->has_timeout = FALSE;

  /* First task in the task queue has always the earliest timeout. */
  silc_list_start(schedule->timeout_queue);
  task = silc_list_get(schedule->timeout_queue);
  if (silc_unlikely(!task))
    return;
  do {
    t = (SilcTask)task;

    /* Remove invalid task */
    if (silc_unlikely(!t->valid)) {
      silc_schedule_task_remove(schedule, t);
      continue;
    }

    /* If the timeout is in past, we will run the task and all other
       timeout tasks from the past. */
    if (silc_compare_timeval(&task->timeout, &curtime) <= 0 && dispatch) {
      silc_schedule_dispatch_timeout(schedule, FALSE);
      if (silc_unlikely(!schedule->valid))
	return;

      /* Start selecting new timeout again after dispatch */
      silc_list_start(schedule->timeout_queue);
      dispatch = FALSE;
      continue;
    }

    /* Calculate the next timeout */
    curtime.tv_sec = task->timeout.tv_sec - curtime.tv_sec;
    curtime.tv_usec = task->timeout.tv_usec - curtime.tv_usec;
    if (curtime.tv_sec < 0)
      curtime.tv_sec = 0;

    /* We wouldn't want to go under zero, check for it. */
    if (curtime.tv_usec < 0) {
      curtime.tv_sec -= 1;
      if (curtime.tv_sec < 0)
	curtime.tv_sec = 0;
      curtime.tv_usec += 1000000L;
    }
    break;
  } while ((task = silc_list_get(schedule->timeout_queue)));

  /* Save the timeout */
  if (task) {
    schedule->timeout = curtime;
    schedule->has_timeout = TRUE;
    SILC_LOG_DEBUG(("timeout: sec=%d, usec=%d", schedule->timeout.tv_sec,
		    schedule->timeout.tv_usec));
  }
}

/* Removes task from the scheduler.  This must be called with scheduler
   locked. */

static void silc_schedule_task_remove(SilcSchedule schedule, SilcTask task)
{
  SilcSchedule parent;

  if (silc_unlikely(task == SILC_ALL_TASKS)) {
    SilcTask task;
    SilcEventTask etask;
    SilcHashTableList htl;
    void *fd;

    /* Delete from fd queue */
    silc_hash_table_list(schedule->fd_queue, &htl);
    while (silc_hash_table_get(&htl, &fd, (void *)&task))
      silc_hash_table_del(schedule->fd_queue, fd);
    silc_hash_table_list_reset(&htl);

    /* Delete from timeout queue */
    silc_list_start(schedule->timeout_queue);
    while ((task = silc_list_get(schedule->timeout_queue))) {
      silc_list_del(schedule->timeout_queue, task);
      silc_free(task);
    }

    /* Delete even tasks */
    parent = silc_schedule_get_parent(schedule);
    if (parent->events) {
      silc_hash_table_list(parent->events, &htl);
      while (silc_hash_table_get(&htl, NULL, (void *)&etask)) {
	silc_hash_table_del_by_context(parent->events, etask->event, etask);
	silc_free(etask->event);
	silc_free(etask);
      }
      silc_hash_table_list_reset(&htl);
    }
    return;
  }

  switch (task->type) {
  case SILC_TASK_FD:
    {
      /* Delete from fd queue */
      SilcTaskFd ftask = (SilcTaskFd)task;
      silc_hash_table_del(schedule->fd_queue, SILC_32_TO_PTR(ftask->fd));
    }
    break;

  case SILC_TASK_TIMEOUT:
    {
      /* Delete from timeout queue */
      silc_list_del(schedule->timeout_queue, task);

      /* Put to free list */
      silc_list_add(schedule->free_tasks, task);
    }
    break;

  case SILC_TASK_EVENT:
    {
      SilcEventTask etask = (SilcEventTask)task;
      SilcScheduleEventConnection conn;

      parent = silc_schedule_get_parent(schedule);

      /* Delete event */
      silc_hash_table_del_by_context(parent->events, etask->event, etask);

      /* Remove all connections */
      silc_list_start(etask->connections);
      while ((conn = silc_list_get(etask->connections)))
	silc_free(conn);

      silc_free(etask->event);
      silc_free(etask);
    }
    break;

  default:
    break;
  }
}

/* Timeout freelist garbage collection */

SILC_TASK_CALLBACK(silc_schedule_timeout_gc)
{
  SilcTaskTimeout t;
  int c;

  if (!schedule->valid)
    return;

  SILC_LOG_DEBUG(("Timeout freelist garbage collection"));

  SILC_SCHEDULE_LOCK(schedule);

  if (silc_list_count(schedule->free_tasks) <= 10) {
    SILC_SCHEDULE_UNLOCK(schedule);
    silc_schedule_task_add_timeout(schedule, silc_schedule_timeout_gc,
				   schedule, 3600, 0);
    return;
  }
  if (silc_list_count(schedule->timeout_queue) >
      silc_list_count(schedule->free_tasks)) {
    SILC_SCHEDULE_UNLOCK(schedule);
    silc_schedule_task_add_timeout(schedule, silc_schedule_timeout_gc,
				   schedule, 3600, 0);
    return;
  }

  c = silc_list_count(schedule->free_tasks) / 2;
  if (c > silc_list_count(schedule->timeout_queue))
    c = (silc_list_count(schedule->free_tasks) -
	 silc_list_count(schedule->timeout_queue));
  if (silc_list_count(schedule->free_tasks) - c < 10)
    c -= (10 - (silc_list_count(schedule->free_tasks) - c));

  SILC_LOG_DEBUG(("Freeing %d unused tasks, leaving %d", c,
		  silc_list_count(schedule->free_tasks) - c));

  silc_list_start(schedule->free_tasks);
  while ((t = silc_list_get(schedule->free_tasks)) && c-- > 0) {
    silc_list_del(schedule->free_tasks, t);
    silc_free(t);
  }
  silc_list_start(schedule->free_tasks);

  SILC_SCHEDULE_UNLOCK(schedule);

  silc_schedule_task_add_timeout(schedule, silc_schedule_timeout_gc,
				 schedule, 3600, 0);
}

#ifdef SILC_DIST_INPLACE
/* Print schedule statistics to stdout */

void silc_schedule_stats(SilcSchedule schedule)
{
  SilcTaskFd ftask;
  fprintf(stdout, "Schedule %p statistics:\n\n", schedule);
  fprintf(stdout, "Num FD tasks         : %d (%lu bytes allocated)\n",
	  silc_hash_table_count(schedule->fd_queue),
	  sizeof(*ftask) * silc_hash_table_count(schedule->fd_queue));
  fprintf(stdout, "Num Timeout tasks    : %d (%lu bytes allocated)\n",
	  silc_list_count(schedule->timeout_queue),
	  sizeof(struct SilcTaskTimeoutStruct) *
	  silc_list_count(schedule->timeout_queue));
  fprintf(stdout, "Num Timeout freelist : %d (%lu bytes allocated)\n",
	  silc_list_count(schedule->free_tasks),
	  sizeof(struct SilcTaskTimeoutStruct) *
	  silc_list_count(schedule->free_tasks));
}
#endif /* SILC_DIST_INPLACE */

/****************************** Public API **********************************/

/* Initializes the scheduler. This returns the scheduler context that
   is given as arugment usually to all silc_schedule_* functions.
   The `max_tasks' indicates the number of maximum tasks that the
   scheduler can handle. The `app_context' is application specific
   context that is delivered to task callbacks. */

SilcSchedule silc_schedule_init(int max_tasks, void *app_context,
				SilcStack stack, SilcSchedule parent)
{
  SilcSchedule schedule;

  /* Initialize Tls, in case it hasn't been done yet */
  silc_thread_tls_init();

  stack = silc_stack_alloc(0, stack);
  if (!stack)
    return NULL;

  /* Allocate scheduler from the stack */
  schedule = silc_scalloc(stack, 1, sizeof(*schedule));
  if (!schedule)
    return NULL;

  SILC_LOG_DEBUG(("Initializing scheduler %p", schedule));

  /* Allocate Fd task hash table dynamically */
  schedule->fd_queue =
    silc_hash_table_alloc(NULL, 0, silc_hash_uint, NULL, NULL, NULL,
			  silc_schedule_fd_destructor, NULL, TRUE);
  if (!schedule->fd_queue) {
    silc_stack_free(stack);
    return NULL;
  }

  silc_list_init(schedule->timeout_queue, struct SilcTaskStruct, next);
  silc_list_init(schedule->free_tasks, struct SilcTaskStruct, next);

  /* Get the parent */
  if (parent && parent->parent)
    parent = parent->parent;

  schedule->stack = stack;
  schedule->app_context = app_context;
  schedule->valid = TRUE;
  schedule->max_tasks = max_tasks;
  schedule->parent = parent;

  /* Allocate scheduler lock */
  silc_mutex_alloc(&schedule->lock);

  /* Initialize the platform specific scheduler. */
  schedule->internal = schedule_ops.init(schedule, app_context);
  if (!schedule->internal) {
    silc_hash_table_free(schedule->fd_queue);
    silc_mutex_free(schedule->lock);
    silc_stack_free(stack);
    return NULL;
  }

  /* Timeout freelist garbage collection */
  silc_schedule_task_add_timeout(schedule, silc_schedule_timeout_gc,
				 schedule, 3600, 0);

  return schedule;
}

/* Uninitializes the schedule. This is called when the program is ready
   to end. This removes all tasks and task queues. Returns FALSE if the
   scheduler could not be uninitialized. This happens when the scheduler
   is still valid and silc_schedule_stop has not been called. */

SilcBool silc_schedule_uninit(SilcSchedule schedule)
{
  SilcTask task;

  SILC_VERIFY(schedule);

  SILC_LOG_DEBUG(("Uninitializing scheduler %p", schedule));

  if (schedule->valid == TRUE)
    return FALSE;

  /* Dispatch all timeouts before going away */
  SILC_SCHEDULE_LOCK(schedule);
  silc_schedule_dispatch_timeout(schedule, TRUE);
  SILC_SCHEDULE_UNLOCK(schedule);

  /* Deliver signals before going away */
  if (schedule->signal_tasks) {
    schedule_ops.signals_call(schedule, schedule->internal);
    schedule->signal_tasks = FALSE;
  }

  /* Unregister all tasks */
  silc_schedule_task_del(schedule, SILC_ALL_TASKS);
  silc_schedule_task_remove(schedule, SILC_ALL_TASKS);

  /* Delete timeout task freelist */
  silc_list_start(schedule->free_tasks);
  while ((task = silc_list_get(schedule->free_tasks)))
    silc_free(task);

  /* Unregister all task queues */
  silc_hash_table_free(schedule->fd_queue);

  /* Uninit the platform specific scheduler. */
  schedule_ops.uninit(schedule, schedule->internal);

  silc_mutex_free(schedule->lock);
  silc_stack_free(schedule->stack);

  return TRUE;
}

/* Stops the schedule even if it is not supposed to be stopped yet.
   After calling this, one should call silc_schedule_uninit (after the
   silc_schedule has returned). */

void silc_schedule_stop(SilcSchedule schedule)
{
  SILC_LOG_DEBUG(("Stopping scheduler"));
  SILC_VERIFY(schedule);
  SILC_SCHEDULE_LOCK(schedule);
  schedule->valid = FALSE;
  SILC_SCHEDULE_UNLOCK(schedule);
}

/* Runs the scheduler once and then returns.   Must be called locked. */

static SilcBool silc_schedule_iterate(SilcSchedule schedule, int timeout_usecs)
{
  struct timeval timeout;
  int ret;

  do {
    SILC_LOG_DEBUG(("In scheduler loop"));

    /* Deliver signals if any has been set to be called */
    if (silc_unlikely(schedule->signal_tasks)) {
      SILC_SCHEDULE_UNLOCK(schedule);
      schedule_ops.signals_call(schedule, schedule->internal);
      schedule->signal_tasks = FALSE;
      SILC_SCHEDULE_LOCK(schedule);
    }

    /* Check if scheduler is valid */
    if (silc_unlikely(schedule->valid == FALSE)) {
      SILC_LOG_DEBUG(("Scheduler not valid anymore, exiting"));
      return FALSE;
    }

    /* Calculate next timeout for silc_select().  This is the timeout value
       when at earliest some of the timeout tasks expire.  This may dispatch
       already expired timeouts. */
    silc_schedule_select_timeout(schedule);

    /* Check if scheduler is valid */
    if (silc_unlikely(schedule->valid == FALSE)) {
      SILC_LOG_DEBUG(("Scheduler not valid anymore, exiting"));
      return FALSE;
    }

    if (timeout_usecs >= 0) {
      timeout.tv_sec = 0;
      timeout.tv_usec = timeout_usecs;
      schedule->timeout = timeout;
      schedule->has_timeout = TRUE;
    }

    /* This is the main silc_select(). The program blocks here until some
       of the selected file descriptors change status or the selected
       timeout expires. */
    SILC_LOG_DEBUG(("Select"));
    ret = schedule_ops.schedule(schedule, schedule->internal);

    if (silc_likely(ret == 0)) {
      /* Timeout */
      SILC_LOG_DEBUG(("Running timeout tasks"));
      if (silc_likely(silc_list_count(schedule->timeout_queue)))
	silc_schedule_dispatch_timeout(schedule, FALSE);
      continue;

    } else if (silc_likely(ret > 0)) {
      /* There is some data available now */
      SILC_LOG_DEBUG(("Running fd tasks"));
      silc_schedule_dispatch_fd(schedule);

      /* If timeout was very short, dispatch also timeout tasks */
      if (schedule->has_timeout && schedule->timeout.tv_sec == 0 &&
	  schedule->timeout.tv_usec < 50000)
	silc_schedule_dispatch_timeout(schedule, FALSE);
      continue;

    } else {
      /* Error or special case handling */
      if (errno == EINTR)
	continue;
      if (ret == -2)
	break;

      SILC_LOG_ERROR(("Error in select()/poll(): %s", strerror(errno)));
      continue;
    }
  } while (timeout_usecs == -1);

  return TRUE;
}

/* Runs the scheduler once and then returns. */

SilcBool silc_schedule_one(SilcSchedule schedule, int timeout_usecs)
{
  SilcBool ret;
  SILC_SCHEDULE_LOCK(schedule);
  ret = silc_schedule_iterate(schedule, timeout_usecs);
  SILC_SCHEDULE_UNLOCK(schedule);
  return ret;
}

/* Runs the scheduler and blocks here.  When this returns the scheduler
   has ended. */

#ifndef SILC_SYMBIAN
void silc_schedule(SilcSchedule schedule)
{
  SILC_LOG_DEBUG(("Running scheduler"));

  /* Start the scheduler loop */
  SILC_SCHEDULE_LOCK(schedule);
  silc_schedule_iterate(schedule, -1);
  SILC_SCHEDULE_UNLOCK(schedule);
}
#endif /* !SILC_SYMBIAN */

/* Wakes up the scheduler. This is used only in multi-threaded
   environments where threads may add new tasks or remove old tasks
   from task queues. This is called to wake up the scheduler in the
   main thread so that it detects the changes in the task queues.
   If threads support is not compiled in this function has no effect.
   Implementation of this function is platform specific. */

void silc_schedule_wakeup(SilcSchedule schedule)
{
#ifdef SILC_THREADS
  SILC_LOG_DEBUG(("Wakeup scheduler"));
  SILC_SCHEDULE_LOCK(schedule);
  schedule_ops.wakeup(schedule, schedule->internal);
  SILC_SCHEDULE_UNLOCK(schedule);
#endif
}

/* Returns parent scheduler */

SilcSchedule silc_schedule_get_parent(SilcSchedule schedule)
{
  return schedule->parent ? schedule->parent : schedule;
}

/* Returns the application specific context that was saved into the
   scheduler in silc_schedule_init function.  The context is also
   returned to application in task callback functions, but this function
   may be used to get it as well if needed. */

void *silc_schedule_get_context(SilcSchedule schedule)
{
  return schedule->app_context;
}

/* Return the stack of the scheduler */

SilcStack silc_schedule_get_stack(SilcSchedule schedule)
{
  return schedule->stack;
}

/* Set notify callback */

void silc_schedule_set_notify(SilcSchedule schedule,
			      SilcTaskNotifyCb notify, void *context)
{
  schedule->notify = notify;
  schedule->notify_context = context;
}

/* Set global scheduler */

void silc_schedule_set_global(SilcSchedule schedule)
{
  SilcTls tls = silc_thread_get_tls();

  if (!tls) {
    /* Try to initialize Tls */
    tls = silc_thread_tls_init();
    SILC_VERIFY(tls);
    if (!tls)
      return;
  }

  SILC_LOG_DEBUG(("Setting global scheduler %p", schedule));

  tls->schedule = schedule;
}

/* Return global scheduler */

SilcSchedule silc_schedule_get_global(void)
{
  SilcTls tls = silc_thread_get_tls();

  if (!tls)
    return NULL;

  SILC_LOG_DEBUG(("Return global scheduler %p", tls->schedule));

  return tls->schedule;
}

/* Add new task to the scheduler */

SilcTask silc_schedule_task_add(SilcSchedule schedule, SilcUInt32 fd,
				SilcTaskCallback callback, void *context,
				long seconds, long useconds,
				SilcTaskType type)
{
  SilcTask task = NULL;

  if (!schedule) {
    schedule = silc_schedule_get_global();
    SILC_VERIFY(schedule);
    if (!schedule) {
      silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
      return NULL;
    }
  }

  if (silc_unlikely(!schedule->valid)) {
    silc_set_errno(SILC_ERR_NOT_VALID);
    return NULL;
  }

  SILC_SCHEDULE_LOCK(schedule);

  if (silc_likely(type == SILC_TASK_TIMEOUT)) {
    SilcTaskTimeout tmp, prev, ttask;
    SilcList list;

    silc_list_start(schedule->free_tasks);
    ttask = silc_list_get(schedule->free_tasks);
    if (silc_unlikely(!ttask)) {
      ttask = silc_calloc(1, sizeof(*ttask));
      if (silc_unlikely(!ttask))
	goto out;
    } else
      silc_list_del(schedule->free_tasks, ttask);

    ttask->header.type = 1;
    ttask->header.callback = callback;
    ttask->header.context = context;
    ttask->header.valid = TRUE;

    /* Add timeout */
    silc_gettimeofday(&ttask->timeout);
    if ((seconds + useconds) > 0) {
      ttask->timeout.tv_sec += seconds + (useconds / 1000000L);
      ttask->timeout.tv_usec += (useconds % 1000000L);
      if (ttask->timeout.tv_usec >= 1000000L) {
	ttask->timeout.tv_sec += 1;
	ttask->timeout.tv_usec -= 1000000L;
      }
    }

    SILC_LOG_DEBUG(("New timeout task %p: sec=%d, usec=%d", ttask,
		    seconds, useconds));

    /* Add task to correct spot so that the first task in the list has
       the earliest timeout. */
    list = schedule->timeout_queue;
    silc_list_start(list);
    prev = NULL;
    while ((tmp = silc_list_get(list)) != SILC_LIST_END) {
      /* If we have shorter timeout, we have found our spot */
      if (silc_compare_timeval(&ttask->timeout, &tmp->timeout) < 0) {
	silc_list_insert(schedule->timeout_queue, prev, ttask);
	break;
      }
      prev = tmp;
    }
    if (!tmp)
      silc_list_add(schedule->timeout_queue, ttask);

    task = (SilcTask)ttask;

    /* Call notify callback */
    if (schedule->notify)
      schedule->notify(schedule, TRUE, task, FALSE, 0, 0, seconds, useconds,
		       schedule->notify_context);

  } else if (silc_likely(type == SILC_TASK_FD)) {
    SilcTaskFd ftask;

    /* Check if fd is already added */
    if (silc_unlikely(silc_hash_table_find(schedule->fd_queue,
					   SILC_32_TO_PTR(fd),
					   NULL, (void *)&task))) {
      if (task->valid)
        goto out;

      /* Remove invalid task.  We must have unique fd key to hash table. */
      silc_schedule_task_remove(schedule, task);
    }

    /* Check max tasks */
    if (silc_unlikely(schedule->max_tasks > 0 &&
		      silc_hash_table_count(schedule->fd_queue) >=
		      schedule->max_tasks)) {
      SILC_LOG_WARNING(("Scheduler task limit reached: cannot add new task"));
      task = NULL;
      silc_set_errno(SILC_ERR_LIMIT);
      goto out;
    }

    ftask = silc_calloc(1, sizeof(*ftask));
    if (silc_unlikely(!ftask)) {
      task = NULL;
      goto out;
    }

    SILC_LOG_DEBUG(("New fd task %p fd=%d", ftask, fd));

    ftask->header.type = 0;
    ftask->header.callback = callback;
    ftask->header.context = context;
    ftask->header.valid = TRUE;
    ftask->events = SILC_TASK_READ;
    ftask->fd = fd;

    /* Add task and schedule it */
    if (!silc_hash_table_add(schedule->fd_queue, SILC_32_TO_PTR(fd), ftask)) {
      silc_free(ftask);
      task = NULL;
      goto out;
    }
    if (!schedule_ops.schedule_fd(schedule, schedule->internal,
				  ftask, ftask->events)) {
      silc_hash_table_del(schedule->fd_queue, SILC_32_TO_PTR(fd));
      task = NULL;
      goto out;
    }

    task = (SilcTask)ftask;

    /* Call notify callback */
    if (schedule->notify)
      schedule->notify(schedule, TRUE, task, TRUE, ftask->fd,
		       SILC_TASK_READ, 0, 0, schedule->notify_context);

  } else if (silc_unlikely(type == SILC_TASK_SIGNAL)) {
    SILC_SCHEDULE_UNLOCK(schedule);
    schedule_ops.signal_register(schedule, schedule->internal, fd,
				 callback, context);
    return NULL;
  }

 out:
  SILC_SCHEDULE_UNLOCK(schedule);

#ifdef SILC_SYMBIAN
  /* On symbian we wakeup scheduler immediately after adding timeout task
     in case the task is added outside the scheduler loop (in some active
     object). */
  if (task && task->type == 1)
    silc_schedule_wakeup(schedule);
#endif /* SILC_SYMBIAN */

  return task;
}

/* Invalidates task */

SilcBool silc_schedule_task_del(SilcSchedule schedule, SilcTask task)
{
  SilcSchedule parent;

  if (!schedule) {
    schedule = silc_schedule_get_global();
    SILC_VERIFY(schedule);
    if (!schedule) {
      silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
      return FALSE;
    }
  }

  if (silc_unlikely(task == SILC_ALL_TASKS)) {
    SilcHashTableList htl;

    SILC_LOG_DEBUG(("Unregister all tasks"));

    SILC_SCHEDULE_LOCK(schedule);

    /* Delete from fd queue */
    silc_hash_table_list(schedule->fd_queue, &htl);
    while (silc_hash_table_get(&htl, NULL, (void *)&task)) {
      task->valid = FALSE;

      /* Call notify callback */
      if (schedule->notify)
	schedule->notify(schedule, FALSE, task, TRUE,
			 ((SilcTaskFd)task)->fd, 0, 0, 0,
			 schedule->notify_context);
    }
    silc_hash_table_list_reset(&htl);

    /* Delete from timeout queue */
    silc_list_start(schedule->timeout_queue);
    while ((task = (SilcTask)silc_list_get(schedule->timeout_queue))) {
      task->valid = FALSE;

      /* Call notify callback */
      if (schedule->notify)
	schedule->notify(schedule, FALSE, task, FALSE, 0, 0, 0, 0,
			 schedule->notify_context);
    }

    /* Delete even tasks */
    parent = silc_schedule_get_parent(schedule);
    if (parent->events) {
      silc_hash_table_list(parent->events, &htl);
      while (silc_hash_table_get(&htl, NULL, (void *)&task))
	task->valid = FALSE;
      silc_hash_table_list_reset(&htl);
    }

    SILC_SCHEDULE_UNLOCK(schedule);
    return TRUE;
  }

  SILC_LOG_DEBUG(("Unregistering task %p, type %d", task, task->type));
  SILC_SCHEDULE_LOCK(schedule);
  task->valid = FALSE;

  /* Call notify callback */
  if (schedule->notify && task->type != SILC_TASK_EVENT)
    schedule->notify(schedule, FALSE, task, task->type == SILC_TASK_FD,
		     0, 0, 0, 0, schedule->notify_context);
  SILC_SCHEDULE_UNLOCK(schedule);

  if (task->type == SILC_TASK_EVENT) {
    /* Schedule removal of deleted event task */
    parent = silc_schedule_get_parent(schedule);
    silc_schedule_task_add_timeout(parent, silc_schedule_event_del_timeout,
				   task, 0, 1);
  }

  return TRUE;
}

/* Invalidate task by fd */

SilcBool silc_schedule_task_del_by_fd(SilcSchedule schedule, SilcUInt32 fd)
{
  SilcTask task = NULL;
  SilcBool ret = FALSE;

  SILC_LOG_DEBUG(("Unregister task by fd %d", fd));

  if (!schedule) {
    schedule = silc_schedule_get_global();
    SILC_VERIFY(schedule);
    if (!schedule) {
      silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
      return FALSE;
    }
  }

  SILC_SCHEDULE_LOCK(schedule);

  /* fd is unique, so there is only one task with this fd in the table */
  if (silc_likely(silc_hash_table_find(schedule->fd_queue,
				       SILC_32_TO_PTR(fd), NULL,
				       (void *)&task))) {
    SILC_LOG_DEBUG(("Deleting task %p", task));
    task->valid = FALSE;

    /* Call notify callback */
    if (schedule->notify)
      schedule->notify(schedule, FALSE, task, TRUE, fd, 0, 0, 0,
		       schedule->notify_context);
    ret = TRUE;
  }

  SILC_SCHEDULE_UNLOCK(schedule);

  /* If it is signal, remove it */
  if (silc_unlikely(!task)) {
    schedule_ops.signal_unregister(schedule, schedule->internal, fd);
    ret = TRUE;
  }

  if (ret == FALSE)
    silc_set_errno(SILC_ERR_NOT_FOUND);

  return ret;
}

/* Invalidate task by task callback. */

SilcBool silc_schedule_task_del_by_callback(SilcSchedule schedule,
					    SilcTaskCallback callback)
{
  SilcTask task;
  SilcHashTableList htl;
  SilcList list;
  SilcBool ret = FALSE;

  SILC_LOG_DEBUG(("Unregister task by callback"));

  if (!schedule) {
    schedule = silc_schedule_get_global();
    SILC_VERIFY(schedule);
    if (!schedule) {
      silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
      return FALSE;
    }
  }

  SILC_SCHEDULE_LOCK(schedule);

  /* Delete from fd queue */
  silc_hash_table_list(schedule->fd_queue, &htl);
  while (silc_hash_table_get(&htl, NULL, (void *)&task)) {
    if (task->callback == callback) {
      task->valid = FALSE;

      /* Call notify callback */
      if (schedule->notify)
	schedule->notify(schedule, FALSE, task, TRUE,
			 ((SilcTaskFd)task)->fd, 0, 0, 0,
			 schedule->notify_context);
      ret = TRUE;
    }
  }
  silc_hash_table_list_reset(&htl);

  /* Delete from timeout queue */
  list = schedule->timeout_queue;
  silc_list_start(list);
  while ((task = (SilcTask)silc_list_get(list))) {
    if (task->callback == callback) {
      task->valid = FALSE;

      /* Call notify callback */
      if (schedule->notify)
	schedule->notify(schedule, FALSE, task, FALSE, 0, 0, 0, 0,
			 schedule->notify_context);
      ret = TRUE;
    }
  }

  SILC_SCHEDULE_UNLOCK(schedule);

  if (ret == FALSE)
    silc_set_errno(SILC_ERR_NOT_FOUND);

  return ret;
}

/* Invalidate task by context. */

SilcBool silc_schedule_task_del_by_context(SilcSchedule schedule,
					   void *context)
{
  SilcTask task;
  SilcHashTableList htl;
  SilcList list;
  SilcBool ret = FALSE;

  SILC_LOG_DEBUG(("Unregister task by context"));

  if (!schedule) {
    schedule = silc_schedule_get_global();
    SILC_VERIFY(schedule);
    if (!schedule) {
      silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
      return FALSE;
    }
  }

  SILC_SCHEDULE_LOCK(schedule);

  /* Delete from fd queue */
  silc_hash_table_list(schedule->fd_queue, &htl);
  while (silc_hash_table_get(&htl, NULL, (void *)&task)) {
    if (task->context == context) {
      task->valid = FALSE;

      /* Call notify callback */
      if (schedule->notify)
	schedule->notify(schedule, FALSE, task, TRUE,
			 ((SilcTaskFd)task)->fd, 0, 0, 0,
			 schedule->notify_context);
      ret = TRUE;
    }
  }
  silc_hash_table_list_reset(&htl);

  /* Delete from timeout queue */
  list = schedule->timeout_queue;
  silc_list_start(list);
  while ((task = (SilcTask)silc_list_get(list))) {
    if (task->context == context) {
      task->valid = FALSE;

      /* Call notify callback */
      if (schedule->notify)
	schedule->notify(schedule, FALSE, task, FALSE, 0, 0, 0, 0,
			 schedule->notify_context);
      ret = TRUE;
    }
  }

  SILC_SCHEDULE_UNLOCK(schedule);

  if (ret == FALSE)
    silc_set_errno(SILC_ERR_NOT_FOUND);

  return ret;
}

/* Invalidate task by all */

SilcBool silc_schedule_task_del_by_all(SilcSchedule schedule, int fd,
				       SilcTaskCallback callback,
				       void *context)
{
  SilcTask task;
  SilcList list;
  SilcBool ret = FALSE;

  SILC_LOG_DEBUG(("Unregister task by fd, callback and context"));

  /* For fd task, callback and context is irrelevant as fd is unique */
  if (fd)
    return silc_schedule_task_del_by_fd(schedule, fd);

  if (!schedule) {
    schedule = silc_schedule_get_global();
    SILC_VERIFY(schedule);
    if (!schedule) {
      silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
      return FALSE;
    }
  }

  SILC_SCHEDULE_LOCK(schedule);

  /* Delete from timeout queue */
  list = schedule->timeout_queue;
  silc_list_start(list);
  while ((task = (SilcTask)silc_list_get(list))) {
    if (task->callback == callback && task->context == context) {
      task->valid = FALSE;

      /* Call notify callback */
      if (schedule->notify)
	schedule->notify(schedule, FALSE, task, FALSE, 0, 0, 0, 0,
			 schedule->notify_context);
      ret = TRUE;
    }
  }

  SILC_SCHEDULE_UNLOCK(schedule);

  if (ret == FALSE)
    silc_set_errno(SILC_ERR_NOT_FOUND);

  return TRUE;
}

/* Sets a file descriptor to be listened by scheduler. One can call this
   directly if wanted. This can be called multiple times for one file
   descriptor to set different iomasks. */

SilcBool silc_schedule_set_listen_fd(SilcSchedule schedule, SilcUInt32 fd,
				     SilcTaskEvent mask, SilcBool send_events)
{
  SilcTaskFd task;

  if (!schedule) {
    schedule = silc_schedule_get_global();
    SILC_VERIFY(schedule);
    if (!schedule) {
      silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
      return FALSE;
    }
  }

  if (silc_unlikely(!schedule->valid)) {
    silc_set_errno(SILC_ERR_NOT_VALID);
    return FALSE;
  }

  SILC_SCHEDULE_LOCK(schedule);

  if (silc_hash_table_find(schedule->fd_queue, SILC_32_TO_PTR(fd),
			   NULL, (void *)&task)) {
    if (!schedule_ops.schedule_fd(schedule, schedule->internal, task, mask)) {
      SILC_SCHEDULE_UNLOCK(schedule);
      return FALSE;
    }
    task->events = mask;
    if (silc_unlikely(send_events) && mask) {
      task->revents = mask;
      silc_schedule_dispatch_fd(schedule);
    }

    /* Call notify callback */
    if (schedule->notify)
      schedule->notify(schedule, TRUE, (SilcTask)task,
		       TRUE, task->fd, mask, 0, 0,
		       schedule->notify_context);
  }

  SILC_SCHEDULE_UNLOCK(schedule);

  return TRUE;
}

/* Returns the file descriptor's current requested event mask. */

SilcTaskEvent silc_schedule_get_fd_events(SilcSchedule schedule,
					  SilcUInt32 fd)
{
  SilcTaskFd task;
  SilcTaskEvent event = 0;

  if (!schedule) {
    schedule = silc_schedule_get_global();
    SILC_VERIFY(schedule);
    if (!schedule) {
      silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
      return 0;
    }
  }

  if (silc_unlikely(!schedule->valid)) {
    silc_set_errno(SILC_ERR_NOT_VALID);
    return 0;
  }

  SILC_SCHEDULE_LOCK(schedule);
  if (silc_hash_table_find(schedule->fd_queue, SILC_32_TO_PTR(fd),
			   NULL, (void *)&task))
    event = task->events;
  SILC_SCHEDULE_UNLOCK(schedule);

  return event;
}

/* Removes a file descriptor from listen list. */

void silc_schedule_unset_listen_fd(SilcSchedule schedule, SilcUInt32 fd)
{
  silc_schedule_set_listen_fd(schedule, fd, 0, FALSE);
}

/*************************** Asynchronous Events ****************************/

/* Add event */

SilcTask silc_schedule_task_add_event(SilcSchedule schedule,
				      const char *event, ...)
{
  SilcEventTask task;
  SilcSchedule parent;

  if (!schedule) {
    schedule = silc_schedule_get_global();
    SILC_VERIFY(schedule);
    if (!schedule) {
      silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
      return NULL;
    }
  }

  /* Get parent scheduler */
  parent = silc_schedule_get_parent(schedule);

  SILC_LOG_DEBUG(("Adding event '%s' to scheduler %p", event, parent));

  SILC_SCHEDULE_LOCK(parent);

  /* Create events hash table if not already done */
  if (!parent->events) {
    parent->events = silc_hash_table_alloc(NULL, 3,
					   silc_hash_string, NULL,
					   silc_hash_string_compare, NULL,
					   NULL, NULL, FALSE);
    if (!parent->events) {
      SILC_SCHEDULE_UNLOCK(parent);
      return NULL;
    }
  }

  /* Check if this event is added already */
  if (silc_hash_table_find(parent->events, (void *)event, NULL, NULL)) {
    SILC_SCHEDULE_UNLOCK(parent);
    return NULL;
  }

  /* Add new event */
  task = silc_calloc(1, sizeof(*task));
  if (!task) {
    SILC_SCHEDULE_UNLOCK(parent);
    return NULL;
  }

  task->header.type = SILC_TASK_EVENT;
  task->header.valid = TRUE;
  task->event = silc_strdup(event);
  if (!task->event) {
    SILC_SCHEDULE_UNLOCK(parent);
    silc_free(task);
    return NULL;
  }
  silc_list_init(task->connections, struct SilcScheduleEventConnectionStruct,
		 next);

  if (!silc_hash_table_add(parent->events, task->event, task)) {
    SILC_SCHEDULE_UNLOCK(parent);
    silc_free(task->event);
    silc_free(task);
    return NULL;
  }

  SILC_SCHEDULE_UNLOCK(parent);

  return (SilcTask)task;
}

/* Connect to event task */

SilcBool silc_schedule_event_connect(SilcSchedule schedule,
				     const char *event, SilcTask task,
				     SilcTaskEventCallback callback,
				     void *context)
{
  SilcSchedule parent;
  SilcScheduleEventConnection conn;
  SilcEventTask etask;

  if (!schedule) {
    schedule = silc_schedule_get_global();
    SILC_VERIFY(schedule);
    if (!schedule) {
      silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
      return FALSE;
    }
  }

  if (!event && !task) {
    silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
    return FALSE;
  }

  if (task && task->type != SILC_TASK_EVENT) {
    silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
    return FALSE;
  }

  /* Get parent scheduler */
  parent = silc_schedule_get_parent(schedule);

  SILC_SCHEDULE_LOCK(parent);

  if (!task) {
    /* Get the event task */
    if (!silc_hash_table_find(parent->events, (void *)event, NULL,
			      (void *)&task)) {
      SILC_SCHEDULE_UNLOCK(parent);
      return FALSE;
    }
  }
  etask = (SilcEventTask)task;

  /* See if task is deleted */
  if (task->valid == FALSE) {
    SILC_SCHEDULE_UNLOCK(parent);
    silc_set_errno(SILC_ERR_NOT_VALID);
    return FALSE;
  }

  SILC_LOG_DEBUG(("Connect callback %p with context %p to event '%s'",
		  callback, context, etask->event));

  /* See if already connected */
  silc_list_start(etask->connections);
  while ((conn = silc_list_get(etask->connections))) {
    if (conn->callback == callback && conn->context == context) {
      SILC_SCHEDULE_UNLOCK(parent);
      silc_set_errno(SILC_ERR_ALREADY_EXISTS);
      return FALSE;
    }
  }

  conn = silc_calloc(1, sizeof(*conn));
  if (!conn) {
    SILC_SCHEDULE_UNLOCK(parent);
    return FALSE;
  }

  /* Connect to the event */
  conn->schedule = schedule;
  conn->callback = callback;
  conn->context = context;
  silc_list_add(etask->connections, conn);

  SILC_SCHEDULE_UNLOCK(parent);

  return TRUE;
}

/* Disconnect from event */

SilcBool silc_schedule_event_disconnect(SilcSchedule schedule,
					const char *event, SilcTask task,
					SilcTaskEventCallback callback,
					void *context)
{
  SilcSchedule parent;
  SilcScheduleEventConnection conn;
  SilcEventTask etask;

  if (!schedule) {
    schedule = silc_schedule_get_global();
    SILC_VERIFY(schedule);
    if (!schedule) {
      silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
      return FALSE;
    }
  }

  if (!event && !task) {
    silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
    return FALSE;
  }

  if (task && task->type != SILC_TASK_EVENT) {
    silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
    return FALSE;
  }

  /* Get parent scheduler */
  parent = silc_schedule_get_parent(schedule);

  SILC_SCHEDULE_LOCK(parent);

  if (!task) {
    /* Get the event task */
    if (!silc_hash_table_find(parent->events, (void *)event, NULL,
			      (void *)&task)) {
      SILC_SCHEDULE_UNLOCK(parent);
      return FALSE;
    }
  }
  etask = (SilcEventTask)task;

  /* See if task is deleted */
  if (task->valid == FALSE) {
    SILC_SCHEDULE_UNLOCK(parent);
    silc_set_errno(SILC_ERR_NOT_VALID);
    return FALSE;
  }

  SILC_LOG_DEBUG(("Disconnect callback %p with context %p from event '%s'",
		  callback, context, etask->event));

  /* Disconnect */
  silc_list_start(etask->connections);
  while ((conn = silc_list_get(etask->connections))) {
    if (conn->callback == callback && conn->context == context) {
      silc_list_del(etask->connections, conn);
      silc_free(conn);
      SILC_SCHEDULE_UNLOCK(parent);
      return TRUE;
    }
  }

  SILC_SCHEDULE_UNLOCK(parent);
  silc_set_errno(SILC_ERR_NOT_FOUND);
  return FALSE;
}

/* Signal event */

SilcBool silc_schedule_event_signal(SilcSchedule schedule, const char *event,
				    SilcTask task, ...)
{
  SilcSchedule parent;
  SilcScheduleEventConnection conn;
  SilcEventTask etask;
  SilcBool stop;
  va_list ap, cp;

  if (silc_unlikely(!schedule)) {
    schedule = silc_schedule_get_global();
    SILC_VERIFY(schedule);
    if (!schedule) {
      silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
      return FALSE;
    }
  }

  if (silc_unlikely(!event && !task)) {
    silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
    return FALSE;
  }

  if (silc_unlikely(task && task->type != SILC_TASK_EVENT)) {
    silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
    return FALSE;
  }

  /* Get parent scheduler */
  parent = silc_schedule_get_parent(schedule);

  SILC_SCHEDULE_LOCK(parent);

  if (!task) {
    /* Get the event task */
    if (!silc_hash_table_find(parent->events, (void *)event, NULL,
			      (void *)&task)) {
      SILC_SCHEDULE_UNLOCK(parent);
      return FALSE;
    }
  }
  etask = (SilcEventTask)task;

  /* See if task is deleted */
  if (task->valid == FALSE) {
    SILC_SCHEDULE_UNLOCK(parent);
    silc_set_errno(SILC_ERR_NOT_VALID);
    return FALSE;
  }

  SILC_LOG_DEBUG(("Signal event '%s'", etask->event));

  va_start(ap, task);

  /* Deliver the signal */
  silc_list_start(etask->connections);
  while ((conn = silc_list_get(etask->connections))) {
    SILC_SCHEDULE_UNLOCK(parent);

    silc_va_copy(cp, ap);
    stop = conn->callback(conn->schedule, conn->schedule->app_context,
			  task, conn->context, cp);
    va_end(cp);

    SILC_SCHEDULE_LOCK(parent);

    /* Stop signal if wanted or if the task was deleted */
    if (!stop || !task->valid)
      break;
  }

  va_end(ap);

  SILC_SCHEDULE_UNLOCK(parent);

  return TRUE;
}

/* Delete event */

SilcBool silc_schedule_task_del_event(SilcSchedule schedule, const char *event)
{
  SilcSchedule parent;
  SilcTask task;

  if (!schedule) {
    schedule = silc_schedule_get_global();
    SILC_VERIFY(schedule);
    if (!schedule) {
      silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
      return FALSE;
    }
  }

  if (!event) {
    silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
    return FALSE;
  }

  /* Get parent scheduler */
  parent = silc_schedule_get_parent(schedule);

  SILC_SCHEDULE_LOCK(parent);

  /* Get the event task */
  if (!silc_hash_table_find(parent->events, (void *)event, NULL,
			    (void *)&task)) {
    SILC_SCHEDULE_UNLOCK(parent);
    return FALSE;
  }

  /* See if already deleted */
  if (task->valid == FALSE)
    return TRUE;

  SILC_LOG_DEBUG(("Delete event '%s'", ((SilcEventTask)task)->event));

  SILC_SCHEDULE_UNLOCK(parent);

  silc_schedule_task_del(parent, task);

  return TRUE;
}

/* Timeout to remove deleted event task */

SILC_TASK_CALLBACK(silc_schedule_event_del_timeout)
{
  SILC_SCHEDULE_LOCK(schedule);
  silc_schedule_task_remove(schedule, context);
  SILC_SCHEDULE_UNLOCK(schedule);
}
