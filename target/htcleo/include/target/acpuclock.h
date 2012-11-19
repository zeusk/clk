/*
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __QSD8K_PLATFORM_ACPUCLOCK_H_
#define __QSD8K_PLATFORM_ACPUCLOCK_H_


enum setrate_reason {
	SETRATE_CPUFREQ = 0,
	SETRATE_SWFI,
};
unsigned acpuclk_init_done;
void clock_config(uint32_t ns, uint32_t md, uint32_t ns_addr, uint32_t md_addr);
int acpuclk_set_rate(unsigned long rate, enum setrate_reason reason);
unsigned long acpuclk_get_rate(void);
void msm_acpu_clock_init(int freq_num);
#endif //__QSD8K_PLATFORM_ACPUCLOCK_H_

#include <platform/iomap.h>




