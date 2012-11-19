/*
 * Copyright (c) 2008 Travis Geiselbrecht
 *
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
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
#include <compiler.h>
#include <debug.h>
#include <string.h>
#include <app.h>
#include <arch.h>
#include <platform.h>
#include <target.h>
#include <lib/heap.h>
#include <kernel/thread.h>
#include <kernel/timer.h>
#include <kernel/dpc.h>

extern void *__ctor_list;
extern void *__ctor_end;
extern int __bss_start;
extern int _end;

static int bootstrap2(void *arg);

#if (ENABLE_NANDWRITE)
void bootstrap_nandwrite(void);
#endif

static void call_constructors(void)
{
	void **ctor;
   
	ctor = &__ctor_list;
	while(ctor != &__ctor_end) {
		void (*func)(void);

		func = (void (*)())*ctor;

		func();
		ctor++;
	}
}
	
/* called from crt0.S */
#ifdef HANDLE_LINUX_KERNEL_ARGS
void kmain(unsigned, unsigned, unsigned *) __NO_RETURN __EXTERNALLY_VISIBLE;
void kmain(unsigned zero_arg, unsigned mach_type, unsigned *atags_ptr)
#else
void kmain(void) __NO_RETURN __EXTERNALLY_VISIBLE;
void kmain(void)
#endif
{
	// get us into some sort of thread context
	thread_init_early();

	// early arch stuff
	arch_early_init();

	// do any super early platform initialization
	platform_early_init();
	
	// do any super early target initialization
	target_early_init();
	//dprintf(SPEW, "Initializing LK\n");
	/* dprintf(SPEW,  "\n __________________________________________________________ \
						|                                                          |");
	dprintf(SPEW,    "|  [L] i t t l e - [K] e r n e l      p o o p L o a d e r  |\
						|__________________________________________________________|\n\n"); */
	
	// deal with any static constructors
	//dprintf(SPEW, "Calling constructors\n");
	call_constructors();

	// bring up the kernel heap
	//dprintf(SPEW, "Initializing heap\n");
	heap_init();

	// initialize the threading system
	//dprintf(SPEW, "Initializing threads\n");
	thread_init();

	// initialize the dpc system
	//dprintf(SPEW, "Initializing dpc\n");
	dpc_init();

	// initialize kernel timers
	//dprintf(SPEW, "Initializing timers\n");
	timer_init();

#if (!ENABLE_NANDWRITE)
	// create a thread to complete system initialization
	//dprintf(SPEW, "\nCreating bootstrap completion thread\n");
	
	// enable interrupts
	exit_critical_section();
	
	thread_resume(thread_create("bootstrap2", &bootstrap2, NULL, DEFAULT_PRIORITY, DEFAULT_STACK_SIZE));

	// become the idle thread
	thread_become_idle();
#else
        bootstrap_nandwrite();
#endif
}

int main(void);

static int bootstrap2(void *arg)
{
	//dprintf(SPEW, "Initializing arch\n");
	arch_init();
	
	// initialize the rest of the platform
	//dprintf(SPEW, "Initializing platform\n");
	platform_init();
	
	// initialize the target
	//dprintf(SPEW, "Initializing target\n\n");
	/* dprintf(SPEW,"    __________________________________________________      \
					   |                                                  |     ");
	dprintf(SPEW,"   | Pressing BACK KEY will attempt to load LK's menu |     ");
	dprintf(SPEW,"   | Pressing HOME KEY will attempt to load recovery  |     ");
	dprintf(SPEW,"   | Pressing MENU KEY will attempt to load sboot     |     \
					   |__________________________________________________|     \n\n"); */
	target_init();

	//dprintf(SPEW, "Determining completion according to boot reason\n");
	apps_init();
	
	return 0;
}

#if (ENABLE_NANDWRITE)
void bootstrap_nandwrite(void)
{
	dprintf(SPEW, "Top of bootstrap2()\n");

	arch_init();

	// initialize the rest of the platform
	dprintf(SPEW, "Initializing platform\n");
	platform_init();

	// initialize the target
	dprintf(SPEW, "Initializing target\n");
	target_init();

	dprintf(SPEW, "Calling nandwrite_init()\n");
	nandwrite_init();

	return 0;
}
#endif
