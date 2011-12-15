
#include <reg.h>
#include <dev/fbcon.h>
#include "target/display.h"
#include <debug.h>

void platform_init_interrupts(void);
void platform_init_timer();
void platform_early_init(void)
{
	platform_init_interrupts();
	platform_init_timer();
}

void platform_init(void)
{
	dprintf(INFO, "platform_init()\n");
}

#define FB_FORMAT_RGB565 0
#define LCDC_FB_BPP 16
#define MSM_MDP_BASE1 0xAA200000

static struct fbcon_config fb_cfg = {
	.height		= LCDC_FB_HEIGHT,
	.width		= LCDC_FB_WIDTH,
	.stride		= LCDC_FB_WIDTH,
	.format		= FB_FORMAT_RGB565,
	.bpp		= LCDC_FB_BPP,
	.update_start	= NULL,
	.update_done	= NULL,
};
void display_init(void)
{
	fb_cfg.base = (unsigned*)readl( MSM_MDP_BASE1 + 0x90008);
	fbcon_setup(&fb_cfg);
	fbcon_clear();
}

