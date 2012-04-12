/*
 * Copyright (c) 2008, Google Inc.
 * All rights reserved.
 *
 * Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the 
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <proc_comm.h>
#include <debug.h>
#include <reg.h>
#include <platform/iomap.h>

static inline void notify_other_proc_comm(void)
{
#ifndef PLATFORM_MSM7X30
    writel(1, MSM_A2M_INT(6));
#else
    writel(1<<6, (MSM_GCC_BASE + 0x8));
#endif
}

int msm_proc_comm(unsigned cmd, unsigned *data1, unsigned *data2)
{
	int ret = -1;
	unsigned status;

	while (readl(MDM_STATUS) != PCOM_READY) {
		/* XXX check for A9 reset */
	}

	writel(cmd, APP_COMMAND);
	if (data1)
		writel(*data1, APP_DATA1);
	if (data2)
		writel(*data2, APP_DATA2);

	notify_other_proc_comm();
	while (readl(APP_COMMAND) != PCOM_CMD_DONE) {
		/* XXX check for A9 reset */
	}

	status = readl(APP_STATUS);

	if (status != PCOM_CMD_FAIL) {
		if (data1)
			*data1 = readl(APP_DATA1);
		if (data2)
			*data2 = readl(APP_DATA2);
		ret = 0;
	}

	return ret;
}

int clock_enable(unsigned id)
{
	return msm_proc_comm(PCOM_CLKCTL_RPC_ENABLE, &id, 0);
}

int clock_disable(unsigned id)
{
	return msm_proc_comm(PCOM_CLKCTL_RPC_DISABLE, &id, 0);
}

int clock_set_rate(unsigned id, unsigned rate)
{
	return msm_proc_comm(PCOM_CLKCTL_RPC_SET_RATE, &id, &rate);
}

int clock_get_rate(unsigned id)
{
	if (msm_proc_comm(PCOM_CLKCTL_RPC_RATE, &id, 0)) {
		return -1;
	} else {
		return (int) id;
	}
}

void usb_clock_init()
{
	clock_enable(USB_HS_PCLK);
	clock_enable(USB_HS_CLK);
	clock_enable(P_USB_HS_CORE_CLK);
}

void lcdc_clock_init(unsigned rate)
{
	clock_set_rate(MDP_LCDC_PCLK_CLK, rate);
	clock_enable(MDP_LCDC_PCLK_CLK);
	clock_enable(MDP_LCDC_PAD_PCLK_CLK);
}

void mdp_clock_init (unsigned rate)
{
	clock_set_rate(MDP_CLK, rate);
	clock_enable(MDP_CLK);
	clock_enable(MDP_P_CLK);
}

void uart3_clock_init(void)
{
	clock_enable(UART3_CLK);
	clock_set_rate(UART3_CLK, 19200000 / 4);
}

void uart2_clock_init(void)
{
	clock_enable(UART2_CLK);
	clock_set_rate(UART2_CLK, 19200000);
}

void mddi_clock_init(unsigned num, unsigned rate)
{
	unsigned clock_id;

	if (num == 0)
		clock_id = PMDH_CLK;
	else
		clock_id = EMDH_CLK;

	clock_enable(clock_id);
	clock_set_rate(clock_id, rate);
#ifdef PLATFORM_MSM7X30
	clock_enable (PMDH_P_CLK);
#endif
}

void reboot(unsigned reboot_reason)
{
        msm_proc_comm(PCOM_RESET_CHIP, &reboot_reason, 0);
        for (;;) ;
}

void shutdown(void)
{
		msm_proc_comm(PCOM_POWER_DOWN, 0, 0);
		for (;;) ;
}

/* Apps processor calls this API to tell modem processor that a PC USB
 * is connected return true if the USB HOST PC charger charging is
 * supported */
int charger_usb_is_pc_connected(void)
{
   unsigned charging_supported = 0;
   unsigned m = 0;
   msm_proc_comm(PCOM_CHG_USB_IS_PC_CONNECTED, &charging_supported, &m);
   return charging_supported;
}

/* Apps processor calls this API to tell modem processor that a USB Wall
 * charger is connected returns true if the USB WALL charger charging is
 * supported */
int charger_usb_is_charger_connected(void)
{
   unsigned charging_supported = 0;
   unsigned m = 0;
   msm_proc_comm(PCOM_CHG_USB_IS_CHARGER_CONNECTED, &charging_supported, &m);
   return charging_supported;
}

/* Apps processor calls this API to tell modem processor that a USB cable is
 * disconnected return true if charging is supported in the system */
int charger_usb_disconnected(void)
{
   unsigned charging_supported = 0;
   unsigned m = 0;
   msm_proc_comm(PCOM_CHG_USB_IS_DISCONNECTED, &charging_supported, &m);
   return charging_supported;
}

/* current parameter passed is the amount of current that the charger needs
 * to draw from USB */
int charger_usb_i(unsigned current)
{
   unsigned charging_supported = 0;
   msm_proc_comm(PCOM_CHG_USB_I_AVAILABLE, &current, &charging_supported);
   return charging_supported;
}

int mmc_clock_enable_disable (unsigned id, unsigned enable)
{
	if(enable)
	{
		return clock_enable(id); //Enable mmc clock rate
	}
	else
	{
		return clock_disable(id); //Disable mmc clock rate
	}
}

int mmc_clock_set_rate(unsigned id, unsigned rate)
{
	return clock_set_rate(id, rate); //Set mmc clock rate
}

int mmc_clock_get_rate(unsigned id)
{
	return clock_get_rate(id); //Get mmc clock rate
}

int gpio_tlmm_config(unsigned config, unsigned disable)
{
    return msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, &disable);
}

int vreg_set_level(unsigned id, unsigned mv)
{
    return msm_proc_comm(PCOM_VREG_SET_LEVEL, &id, &mv);
}

int vreg_enable(unsigned id)
{
    unsigned int enable = 1;
    return msm_proc_comm(PCOM_VREG_SWITCH, &id, &enable);

}

int vreg_disable(unsigned id)
{
    unsigned int enable = 0;
    return msm_proc_comm(PCOM_VREG_SWITCH, &id, &enable);
}
