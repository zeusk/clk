/*
 * aboot.h
 * This file is part of Little Kernel
 *
 * Copyright (C) 2011 - Shantanu Gupta 
 *
 * Little Kernel is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Little Kernel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Little Kernel; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */


#define EXPAND(NAME) #NAME
#define TARGET(NAME) EXPAND(NAME)
#define ROUND_TO_PAGE(x,y) (((x) + (y)) & (~(y)))

#define FASTBOOT_MODE   0x77665500
#define ANDRBOOT_MODE	0x77665501
#define RECOVERY_MODE   0x77665502
#define DEV_FAC_RESET	0x776655EF

unsigned get_page_sz(void);
unsigned get_page_mask(void);

void platform_uninit_timer(void);
unsigned* target_atag_mem(unsigned* ptr);
unsigned board_machtype(void);
unsigned check_reboot_mode(void);
void *target_get_scratch_address(void);
int target_is_emmc_boot(void);
void reboot_device(unsigned);
void target_battery_charging_enable(unsigned enable, unsigned disconnect);
void display_shutdown(void);

void boot_linux(void *kernel, unsigned *tags, 
		const char *cmdline, unsigned machtype,
		void *ramdisk, unsigned ramdisk_size);
int boot_linux_from_flash(void);
