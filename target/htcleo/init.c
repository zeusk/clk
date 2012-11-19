/*
 * author: cedesmith
 * license: GPL
 * Added inhouse partitioning code: dan1j3l
 * Optimized by zeus; inspired, consulted by cotulla, my bitch :D
 */
// koko : (TODO) Clean up headers
#include <string.h>
#include <board.h>
#include <debug.h>
#include <arch/mtype.h>
#include <arch/ops.h>
#include <dev/keys.h>
#include <dev/gpio_keypad.h>
#include <kernel/thread.h>
#include <kernel/timer.h>
#include <lib/ptable.h>
#include <lib/devinfo.h>
#include <dev/fbcon.h>
#include <dev/flash.h>
#include <dev/udc.h>
#include <dev/power_supply.h>
#include <target/hsusb.h>
#include <smem.h>
#include <platform.h>
#include <platform/iomap.h>
#include <platform/timer.h>
#include <platform/interrupts.h>
#include <platform/gpio.h>
#include <reg.h>
#include <target.h>
#include <pcom.h>
#include <target/acpuclock.h>
#include <target/board_htcleo.h>
#include <target/gpio_keys.h>
#include <bootreason.h>

/******************************************************************************
 * target_ ...
 *****************************************************************************/
unsigned boot_reason = 0xFFFFFFFF;
unsigned android_reboot_reason = 0;

void target_shutdown(void)
{
	enter_critical_section();
	
	if(fbcon_display())
		htcleo_display_shutdown();
	platform_exit();
	msm_proc_comm(PCOM_POWER_DOWN, 0, 0);
	for (;;) ;
}

void reboot(unsigned reboot_reason)
{
    msm_proc_comm(PCOM_RESET_CHIP, &reboot_reason, 0);
    for (;;) ;
}

void target_reboot(unsigned reboot_reason)
{
	enter_critical_section();
	
	if(fbcon_display())
		htcleo_display_shutdown();
	platform_exit();
	writel(reboot_reason, LK_BOOTREASON_ADDR);
	writel(reboot_reason^MARK_LK_TAG, LK_BOOTREASON_ADDR + 4);
	reboot(reboot_reason);
}

unsigned get_boot_reason(void)
{
	if (boot_reason == 0xFFFFFFFF) {
		boot_reason = readl(MSM_SHARED_BASE + 0xef244);
		if (boot_reason != 2) {
			if (readl(LK_BOOTREASON_ADDR) == (readl(LK_BOOTREASON_ADDR + 4) ^ MARK_LK_TAG)) {
				android_reboot_reason = readl(LK_BOOTREASON_ADDR);
				writel(0, LK_BOOTREASON_ADDR);
			}
		}
	}
	
	return boot_reason;
}

unsigned target_check_reboot_mode(void)
{
	get_boot_reason();
	return android_reboot_reason;
}

unsigned target_pause_for_battery_charge(void)
{
    if (get_boot_reason() == 2) 
		return 1;

    return 0;
}

void target_early_init(void)
{
	target_board->early_init();
}

void target_init(void)
{
	target_board->init();
}

void target_exit(void)
{
#if HTCLEO_USE_CRAP_FADE_EFFECT
	htcleo_display_shutdown();
#endif
	target_board->exit();
}

void* target_atag(unsigned* ptr)
{
	return (void *)target_board->atag(ptr);
}

unsigned target_machtype(void)
{
	return target_board->mach_type;
}

char* target_get_cmdline(void)
{
	return target_board->cmdline;
}

void* target_get_scratch_address(void)
{
	return target_board->scratch_address;
}

unsigned target_get_scratch_size(void)
{
	return target_board->scratch_size;
}

unsigned target_support_flashlight(void)
{
	return 1;
}

void* target_flashlight(void *arg)
{
	if(target_board && target_board->flashlight)
		return (void *)target_board->flashlight(arg);
		
	return 0;
}
