#include <debug.h>
#include <target.h>
#include <board.h>
#include <pcom.h>
#include <msm_i2c.h>
#include <arch/ops.h>
#include <platform/interrupts.h>
#include <platform/timer.h>
#include <platform/gpio.h>
#include <target/board_htcleo.h>
#include <target/dex_comm.h>

void platform_early_init(void)
{
	board_init();
	platform_init_interrupts();
	platform_init_timer();
	msm_gpio_init();
}

void platform_init(void)
{
	msm_dex_comm_init();	
    msm_pcom_init();
}

void platform_exit(void)
{
	msm_i2c_remove();
	msm_prepare_clocks();
	msm_gpio_deinit();
	platform_deinit_timer();
	arch_disable_cache(UCACHE);
	arch_disable_mmu();
	platform_deinit_interrupts();
}

void platform_init_mmu_mappings(void)
{
	//dprintf(SPEW, "platform_init_mmu_mappings()\n");
}