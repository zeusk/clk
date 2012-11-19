/* board.h
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __PLATFORM_MSM_SHARED_BOARD_H__
#define __PLATFORM_MSM_SHARED_BOARD_H__

struct board_mach {
	void (*early_init)(void);
	void (*init)(void);
	void (*exit)(void);
	int (*flashlight)(void *arg);
	unsigned (*atag)(unsigned *ptr);
	unsigned mach_type;
	char *cmdline;
	void *scratch_address;
	unsigned scratch_size;
}( *target_board);

void board_init(void);

#endif // __PLATFORM_MSM_SHARED_BOARD_H__