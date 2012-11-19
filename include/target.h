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
#ifndef __TARGET_H
#define __TARGET_H

#include <arch/mtype.h>
/* super early platform initialization, before almost everything */
void target_early_init(void);
/* later init, after the kernel has come up */
void target_init(void);
/* prepare the device for (re)boot */
void target_exit(void);
/* get memory address for fastboot image loading */
void* target_get_scratch_address(void);
/* get memory size for fastboot image loading */
unsigned target_get_scratch_size(void);

void* target_atag(unsigned* ptr);
unsigned target_machtype(void);
char* target_get_cmdline(void);
unsigned target_support_flashlight(void);
void* target_flashlight(void *arg);
unsigned target_pause_for_battery_charge(void);
void target_battery_charging_enable(unsigned enable, unsigned disconnect);

void target_shutdown(void);
void target_reboot(unsigned reboot_reason);
unsigned target_check_reboot_mode(void);

#endif
