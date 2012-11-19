/*
 * Copyright (c) 2008 Travis Geiselbrecht
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <debug.h>
#include <err.h>
#include <kernel/mutex.h>
#include <kernel/thread.h>

void mutex_init(mutex_t *m)
{
#if MUTEX_CHECK
	m->magic = MUTEX_MAGIC;
	m->holder = 0;
#endif

	m->count = 0;
	wait_queue_init(&m->wait);
}

void mutex_destroy(mutex_t *m)
{
	enter_critical_section();

#if MUTEX_CHECK
	ASSERT(m->magic == MUTEX_MAGIC);
	m->magic = 0;
	
	if (m->holder != 0 && current_thread != m->holder)
		panic("mutex_destroy: thread %p (%s) tried to release mutex %p it doesn't own. owned by %p (%s)\n", 
				current_thread, current_thread->name, m, m->holder, m->holder ? m->holder->name : "none");
#endif

	
	m->count = 0;
	wait_queue_destroy(&m->wait, true);
	exit_critical_section();
}

status_t mutex_acquire(mutex_t *m)
{
	status_t ret = NO_ERROR;

#if MUTEX_CHECK
	ASSERT(m->magic == MUTEX_MAGIC);
	
	if (current_thread == m->holder)
		panic("mutex_acquire: thread %p (%s) tried to acquire mutex %p it already owns.\n",
				current_thread, current_thread->name, m);
#endif

	enter_critical_section();

	if (unlikely(++m->count > 1)) {
		/* 
		 * block on the wait queue. If it returns an error, it was likely destroyed
		 * out from underneath us, so make sure we dont scribble thread ownership 
		 * on the mutex.
		 */
		ret = wait_queue_block(&m->wait, INFINITE_TIME);

#if MUTEX_CHECK
		if (ret < 0)
			goto err;
#endif
	}
	
#if MUTEX_CHECK
	m->holder = current_thread;	
err:
#endif

	exit_critical_section();
	return ret;
}

status_t mutex_acquire_timeout(mutex_t *m, time_t timeout)
{
	status_t ret = NO_ERROR;

#if MUTEX_CHECK
	if (timeout == INFINITE_TIME)
		return mutex_acquire(m); // Unecessary overhead for correct calls, this function can handle this anyway

	ASSERT(m->magic == MUTEX_MAGIC);

	if (current_thread == m->holder)
		panic("mutex_acquire_timeout: thread %p (%s) tried to acquire mutex %p it already owns.\n",
		      current_thread, current_thread->name, m);
#endif


	enter_critical_section();

	if (unlikely(++m->count > 1)) {
		ret = wait_queue_block(&m->wait, timeout);
		if (ret < NO_ERROR) {
			/* if the acquisition timed out, back out the acquire and exit */
			if (ret == ERR_TIMED_OUT) {
				/*
				 * XXX race: the mutex may have been destroyed after the timeout,
				 * but before we got scheduled again which makes messing with the
				 * count variable dangerous.
				 */
				m->count--;
			}
#if MUTEX_CHECK
			goto err;
#endif
		}
	}

#if MUTEX_CHECK
	m->holder = current_thread;
err:
#endif

	exit_critical_section();
	return ret;
}

status_t mutex_release(mutex_t *m)
{
#if MUTEX_CHECK
	ASSERT(m->magic == MUTEX_MAGIC);
	
	if (current_thread != m->holder)
		panic("mutex_release: thread %p (%s) tried to release mutex %p it doesn't own. owned by %p (%s)\n", 
				current_thread, current_thread->name, m, m->holder, m->holder ? m->holder->name : "none");
#endif

	enter_critical_section();

#if MUTEX_CHECK
	m->holder = 0;
#endif

	if (unlikely(--m->count != 0)) {
		/* release a thread */
		wait_queue_wake_one(&m->wait, true, NO_ERROR);
	}

	exit_critical_section();

	return NO_ERROR;
}

