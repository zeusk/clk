/* DEX (legacy version of pcom) interface for HTC wince PDAs.
 *
 * Copyright (c) 2011 htc-linux.org
 * Copyright (c) 2012 Shantanu Gupta <shans95g@gmail.com>
 * Original implementation for linux kernel is copyright (C) maejrep
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * Based on proc_comm.c by Brian Swetland
 */

#include <debug.h>
#include <reg.h>
#include <string.h>
#include <platform/iomap.h>
#include <platform/timer.h>
#include <target/dex_comm.h>

#define MSM_A2M_INT(n) (MSM_CSR_BASE + 0x400 + (n) * 4)

#define DEX_COMMAND			0x00
#define DEX_STATUS			0x04
#define DEX_SERIAL			0x08
#define DEX_SERIAL_CHECK	0x0C
#define DEX_DATA			0x20
#define DEX_DATA_RESULT		0x24

#define DEX_TIMEOUT (10000000) /* 10s in microseconds */

#define DEX_INIT_DONE (MSM_SHARED_BASE + 0xefe3c)

static inline void notify_other_dex_comm(void)
{
	writel(1, MSM_A2M_INT(4));
}

static void wait_dex_ready(void) {
	int status = 0;
	int wait_timeout = 10;
	
	if (readl(DEX_INIT_DONE)) {
		return;
	}

	while ( (wait_timeout--) && (!(status = readl(DEX_INIT_DONE)))) {
		mdelay(500);
	}
	
	if(!(wait_timeout)) dprintf(CRITICAL, "[DEX] ARM9 did not respond for a long time, dex might fail.\n");
}

static void init_dex_locked(void) {
	unsigned base = (unsigned)(MSM_SHARED_BASE + 0xefe00);

	if (readl(DEX_INIT_DONE)) {
		return;
	}

	writel(0, base + DEX_DATA);
	writel(0, base + DEX_DATA_RESULT);
	writel(0, base + DEX_SERIAL);
	writel(0, base + DEX_SERIAL_CHECK);
	writel(0, base + DEX_STATUS);
}

int msm_dex_comm(struct msm_dex_command * in, unsigned *out)
{
	unsigned base = (unsigned)(MSM_SHARED_BASE + 0xefe00);
	unsigned dex_timeout;
	unsigned status;
	unsigned num;
	unsigned base_cmd, base_status;

	wait_dex_ready();

	// dprintf(SPEW, "[DEX] waiting for modem\n\tcommand=0x%02x data=0x%x\n", in->cmd, in->data);

	base_cmd = in->cmd & 0xff;

	writeb(base_cmd, base + DEX_COMMAND);

	if ( in->has_data )
	{
		writel(readl(base + DEX_COMMAND) | DEX_HAS_DATA, base + DEX_COMMAND);
		writel(in->data, base + DEX_DATA);
	} else {
		writel(readl(base + DEX_COMMAND) & ~DEX_HAS_DATA, base + DEX_COMMAND);
		writel(0, base + DEX_DATA);
	}

	num = readl(base + DEX_SERIAL) + 1;
	writel(num, base + DEX_SERIAL);

	// dprintf(SPEW, "[DEX] command and data sent (cntr=0x%x)\n", num);

	notify_other_dex_comm();

	dex_timeout = DEX_TIMEOUT;
	while ( --dex_timeout && readl(base + DEX_SERIAL_CHECK) != num )
		udelay(1);

	if ( ! dex_timeout )
	{
/*
		dprintf(CRITICAL, "[DEX] cmd timed out\n\tstatus=0x%x, A2Mcntr=%x, M2Acntr=%x\n", 
			readl(base + DEX_STATUS), num, readl(base + DEX_SERIAL_CHECK));
*/
		goto end;
	}

	// dprintf(SPEW, "[DEX] command result status = 0x%08x\n", readl(base + DEX_STATUS));
	

	// Read status of command
	status = readl(base + DEX_STATUS);
	writeb(0, base + DEX_STATUS);
	base_status = status & 0xff;

	// dprintf(SPEW, "[DEX] new status: 0x%x @ 0x%x\n", readl(base + DEX_STATUS), base_status);

	if ( base_status == base_cmd )
	{
		if ( status & DEX_STATUS_FAIL )
		{
/*
			dprintf(CRITICAL, "[DEX] cmd failed\n\tstatus:0x%x result:0x%x\n",
				readl(base + DEX_STATUS), readl(base + DEX_DATA_RESULT));
*/
			writel(readl(base + DEX_STATUS) & ~DEX_STATUS_FAIL, base + DEX_STATUS);
		}
		else if ( status & DEX_HAS_DATA )
		{
			writel(readl(base + DEX_STATUS) & ~DEX_HAS_DATA, base + DEX_STATUS);
			if (out)
				*out = readl(base + DEX_DATA_RESULT);
			// dprintf(SPEW, "[DEX] output data: 0x%x\n", readl(base + DEX_DATA_RESULT));
		}
	} else {
/*
		dprintf(CRITICAL, "[DEX] Code not match!\n\ta2m[0x%x], m2a[0x%x], a2m_num[0x%x], m2a_num[0x%x]\n",
			__func__, base_cmd, base_status, num, readl(base + DEX_SERIAL_CHECK));
*/
	}

end:
	writel(0, base + DEX_DATA_RESULT);
	writel(0, base + DEX_STATUS);

	return 0;
}

void msm_dex_comm_init()
{
	init_dex_locked();
}
