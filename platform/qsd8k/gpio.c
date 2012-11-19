/*
 * Copyright (C) 2008, Google Inc. All rights reserved.
 * Copyright (C) 2011, htc-linux.org 
 * Copyrught (C) 2012, shantanu gupta <shans95g@gmail.com>
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

#include <array.h>
#include <debug.h>
#include <reg.h>
#include <dev/gpio.h>
#include <kernel/thread.h>
#include <platform/irqs.h>
#include <platform/iomap.h>
#include <platform/interrupts.h>
#include <platform/gpio.h>
#include <pcom.h>
#include <dev/gpio.h>

struct gpioregs_t {
	unsigned out;
	unsigned in;
	unsigned int_status;
	unsigned int_clear;
	unsigned int_en;
	unsigned int_edge;
	unsigned int_pos;
	unsigned oe;
	unsigned owner;
	unsigned start;
	unsigned end;
};

typedef struct gpioregs_t gpioregs;

static gpioregs GPIO_REGS[] = {
	{
	.out 		= GPIO_OUT_0,
	.in 		= GPIO_IN_0,
	.int_status = GPIO_INT_STATUS_0,
	.int_clear 	= GPIO_INT_CLEAR_0,
	.int_en 	= GPIO_INT_EN_0,
	.int_edge 	= GPIO_INT_EDGE_0,
	.int_pos 	= GPIO_INT_POS_0,
	.oe 		= GPIO_OE_0,
	.owner 		= GPIO_OWNER_0,
	.start 		= 0,
	.end 		= 15,
	},
	{
	.out 		= GPIO_OUT_1,
	.in 		= GPIO_IN_1,
	.int_status = GPIO_INT_STATUS_1,
	.int_clear 	= GPIO_INT_CLEAR_1,
	.int_en 	= GPIO_INT_EN_1,
	.int_edge 	= GPIO_INT_EDGE_1,
	.int_pos 	= GPIO_INT_POS_1,
	.oe 		= GPIO_OE_1,
	.owner 		= GPIO_OWNER_1,
	.start	 	= 16,
	.end 		= 42,
	},
	{
	.out 		= GPIO_OUT_2,
	.in 		= GPIO_IN_2,
	.int_status = GPIO_INT_STATUS_2,
	.int_clear 	= GPIO_INT_CLEAR_2,
	.int_en 	= GPIO_INT_EN_2,
	.int_edge 	= GPIO_INT_EDGE_2,
	.int_pos 	= GPIO_INT_POS_2,
	.oe 		= GPIO_OE_2,
	.owner 		= GPIO_OWNER_2,
	.start 		= 43,
	.end 		= 67,
	},
	{
	.out 		= GPIO_OUT_3,
	.in 		= GPIO_IN_3,
	.int_status = GPIO_INT_STATUS_3,
	.int_clear 	= GPIO_INT_CLEAR_3,
	.int_en 	= GPIO_INT_EN_3,
	.int_edge 	= GPIO_INT_EDGE_3,
	.int_pos 	= GPIO_INT_POS_3,
	.oe 		= GPIO_OE_3,
	.owner 		= GPIO_OWNER_3,
	.start 		= 68,
	.end 		= 94
	},
	{
	.out 		= GPIO_OUT_4,
	.in 		= GPIO_IN_4,
	.int_status = GPIO_INT_STATUS_4,
	.int_clear 	= GPIO_INT_CLEAR_4,
	.int_en 	= GPIO_INT_EN_4,
	.int_edge 	= GPIO_INT_EDGE_4,
	.int_pos 	= GPIO_INT_POS_4,
	.oe 		= GPIO_OE_4,
	.owner 		= GPIO_OWNER_4,
	.start 		= 95,
	.end 		= 103,
	},
	{
	.out 		= GPIO_OUT_5,
	.in 		= GPIO_IN_5,
	.int_status = GPIO_INT_STATUS_5,
	.int_clear 	= GPIO_INT_CLEAR_5,
	.int_en 	= GPIO_INT_EN_5,
	.int_edge 	= GPIO_INT_EDGE_5,
	.int_pos 	= GPIO_INT_POS_5,
	.oe 		= GPIO_OE_5,
	.owner 		= GPIO_OWNER_5,
	.start 		= 104,
	.end 		= 121,
	},
	{
	.out        = GPIO_OUT_6,
	.in         = GPIO_IN_6,
	.int_status = GPIO_INT_STATUS_6,
	.int_clear  = GPIO_INT_CLEAR_6,
	.int_en     = GPIO_INT_EN_6,
	.int_edge   = GPIO_INT_EDGE_6,
	.int_pos    = GPIO_INT_POS_6,
	.oe         = GPIO_OE_6,
	.owner      = GPIO_OWNER_6,
	.start      = 122,
	.end        = 152,
	},
	{
	.out        = GPIO_OUT_7,
	.in         = GPIO_IN_7,
	.int_status = GPIO_INT_STATUS_7,
	.int_clear  = GPIO_INT_CLEAR_7,
	.int_en     = GPIO_INT_EN_7,
	.int_edge   = GPIO_INT_EDGE_7,
	.int_pos    = GPIO_INT_POS_7,
	.oe         = GPIO_OE_7,
	.owner      = GPIO_OWNER_7,
	.start      = 153,
	.end        = 164,
	},
};

static struct msm_gpio_isr_handler
{
	 int_handler handler;
	 void *arg;
	 bool pending;
} gpio_irq_handlers[NR_MSM_GPIOS];

static gpioregs *find_gpio(unsigned n, unsigned *bit)
{
	gpioregs *ret = 0;
	if (n > GPIO_REGS[ARRAY_SIZE(GPIO_REGS) - 1].end)
		goto end;

	for (unsigned i = 0; i < ARRAY_SIZE(GPIO_REGS); i++) {
		ret = GPIO_REGS + i;
		if (n >= ret->start && n <= ret->end) {
			*bit = 1 << (n - ret->start);
			break;
		}
	}

end:
	return ret;
}

/*
 * IRQ -> GPIO mapping table
 */
static signed char irq2gpio[32] = {
	-1, -1, -1, -1, -1, -1,  0,  1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1,  2,  3,  4,  5,  6,
	 7,  8,  9, 10, 11, 12, -1, -1,
};

int gpio_to_irq(int gpio)
{
	int irq;

	for (irq = 0; irq < 32; irq++) {
		if (irq2gpio[irq] == gpio)
			return irq;
	}
	return -2;
}

int gpio_config(unsigned n, unsigned flags)
{
	gpioregs *r;
	unsigned b = 0;
	unsigned v;

	if ((r = find_gpio(n, &b)) == 0)
		return -1;

	v = readl(r->oe);
	if (flags & GPIO_OUTPUT) {
		writel(v | b, r->oe);
	} else {
		writel(v & (~b), r->oe);
	}
	
	return 0;
}

void gpio_set(unsigned n, unsigned on)
{
	gpioregs *r;
	unsigned b = 0;
	unsigned v;

	if ((r = find_gpio(n, &b)) == 0)
		return;
		
	gpio_config(n, GPIO_OUTPUT);

	v = readl(r->out);
	if (on) {
		writel(v | b, r->out);
	} else {
		writel(v & (~b), r->out);
	}
}

int gpio_get(unsigned n)
{
	gpioregs *r;
	unsigned b = 0;

	if ((r = find_gpio(n, &b)) == 0)
		return 0;
		
	gpio_config(n, GPIO_INPUT);

	return (readl(r->in) & b) ? 1 : 0;
}

void config_gpio_table(uint32_t *table, int len)
{
	int n;
	unsigned id;
	for (n = 0; n < len; n++) {
		id = table[n];
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
	}
}

void msm_gpio_set_owner(unsigned gpio, unsigned owner)
{

	gpioregs *r;
	unsigned b = 0;
	unsigned v;

	if ((r = find_gpio(gpio, &b)) == 0)
		return;

	v = readl(r->owner);
	if (owner == MSM_GPIO_OWNER_ARM11) {
		writel(v | b, r->owner);
	} else {
		writel(v & (~b), r->owner);
	}
}

void msm_gpio_config(unsigned config, unsigned disable)
{
	unsigned gpio = MSM_GPIO_PIN(config);
	unsigned cfg = (MSM_GPIO_DRVSTR(config) << 6)
					| (MSM_GPIO_FUNC(config) << 2)
					| (MSM_GPIO_PULL(config));

	if (gpio < 16 || gpio > 42) {
		writel(gpio, MSM_GPIOCFG1_BASE + 0x20);
		writel(cfg, MSM_GPIOCFG1_BASE + 0x24);
		if (readl(MSM_GPIOCFG1_BASE + 0x20) != gpio) {
			dprintf(CRITICAL, "[GPIO]: could not set alt func %u => %u\n",
			 gpio, MSM_GPIO_FUNC(config));
		}
	} else {
		writel(gpio, MSM_GPIOCFG2_BASE + 0x410);
		writel(cfg, MSM_GPIOCFG2_BASE + 0x414);
		if (readl(MSM_GPIOCFG2_BASE + 0x410) != gpio) {
			dprintf(CRITICAL, "[GPIO]: could not set alt func %u => %u\n",
			 gpio, MSM_GPIO_FUNC(config));
		}
	}

	if (MSM_GPIO_DIR(config))
		gpio_set(gpio, !disable);
	else
		gpio_config(gpio, GPIO_INPUT);
}

int msm_gpio_configure(unsigned int gpio, unsigned long flags)
{
	gpioregs *r;
	unsigned b = 0;
	unsigned v;
	
	if ((r = find_gpio(gpio, &b)) == 0)
		return -1;

	if (flags & (GPIOF_OUTPUT_LOW | GPIOF_OUTPUT_HIGH))
		gpio_set(gpio, flags & GPIOF_OUTPUT_HIGH);

	if (flags & (GPIOF_INPUT | GPIOF_DRIVE_OUTPUT)) {
		v = readl(r->oe);
		if (flags & GPIOF_DRIVE_OUTPUT) {
			writel(v | b, r->oe);
		} else {
			writel(v & (~b), r->oe);
		}
	}

	/* Interrupt part stripped */

	return 0;
}

static void msm_gpio_update_both_edge_detect(unsigned gpio)
{
	gpioregs *r;
	unsigned b = 0;

	if ((r = find_gpio(gpio, &b)) == 0)
		return;

	int loop_limit = 100;
	unsigned pol, val, val2, intstat;
	do {
		val = readl(r->in);
		pol = readl(r->int_pos);
		pol |= ~val;
		writel(pol, r->int_pos);
		intstat = readl(r->int_status);
		val2 = readl(r->in);
		if (((val ^ val2) & ~intstat) == 0)
			return;
	} while (loop_limit-- > 0);
	dprintf(INFO, "%s, failed to reach stable state %x != %x\n", __func__,val, val2);
}

static int msm_gpio_irq_clear(unsigned gpio)
{
	gpioregs *r;
	unsigned b = 0;

	if ((r = find_gpio(gpio, &b)) == 0)
		return -1;

	writel(b, r->int_clear);
	
	return 0;
}

static void msm_gpio_irq_ack(unsigned gpio)
{
	msm_gpio_irq_clear(gpio);
	msm_gpio_update_both_edge_detect(gpio);
}

static void msm_set_gpio_irq_trigger(unsigned gpio, enum gpio_irq_trigger flags)
{
	gpioregs *r;
	unsigned b = 0;
	unsigned v;

	if ((r = find_gpio(gpio, &b)) == 0)
		return;

	v = readl(r->int_edge);
	if (flags & (GPIO_IRQF_FALLING | GPIO_IRQF_RISING)) {
		writel(v | b, r->int_edge);
	} else {
		writel(v & (~b), r->int_edge);
	}
	
	if ((flags & (GPIO_IRQF_FALLING | GPIO_IRQF_RISING)) ==
	(GPIO_IRQF_FALLING | GPIO_IRQF_RISING)) {
		msm_gpio_update_both_edge_detect(gpio);
	} else {
		v = readl(r->int_pos);
		if (flags & (GPIO_IRQF_RISING)) {
			writel(v | b, r->int_pos);
		} else {
			writel(v & (~b), r->int_pos);
		}
	}
}

static void msm_gpio_irq_mask(unsigned gpio)
{
	gpioregs *r;
	unsigned b = 0;
	unsigned v;

	if ((r = find_gpio(gpio, &b)) == 0)
		return;

	v = readl(r->int_edge);
	/* level triggered interrupts are also latched */
	if (!(v & b))
		msm_gpio_irq_clear(gpio);
		
	writel(readl(r->int_en) & ~b, r->int_en);
}

static void msm_gpio_irq_unmask(unsigned gpio)
{
	gpioregs *r;
	unsigned b = 0;
	unsigned v;

	if ((r = find_gpio(gpio, &b)) == 0)
		return;

	v = readl(r->int_edge);
	/* level triggered interrupts are also latched */
	if (!(v & b))
		msm_gpio_irq_clear(gpio);
		
	writel(readl(r->int_en) | b, r->int_en);
}

static enum handler_return msm_gpio_isr(void *arg)
{
	unsigned s, e;
	gpioregs *r;

	enter_critical_section();
	for (unsigned i = 0; i < ARRAY_SIZE(GPIO_REGS); i++) {
		r = GPIO_REGS + i;
		s = readl(r->int_status);
		e = readl(r->int_en);
		for (int j = 0; j < 32; j++)
			if (e & s & (1 << j)) {
				msm_gpio_irq_ack(r->start + j);
				gpio_irq_handlers[r->start + j].pending = 1;
		}
	}
	
	for (unsigned i = 0; i < ARRAY_SIZE(gpio_irq_handlers); i++) {
		if (gpio_irq_handlers[i].pending) {
			gpio_irq_handlers[i].pending = 0;
			if (gpio_irq_handlers[i].handler)
				gpio_irq_handlers[i].handler(gpio_irq_handlers[i].arg);
		}
	}
	exit_critical_section();
	
	return INT_RESCHEDULE;
}

void msm_gpio_init(void)
{
	enter_critical_section();
	for (unsigned i = 0; i < ARRAY_SIZE(GPIO_REGS); i++) {
		writel(-1, GPIO_REGS[i].int_clear);
		writel(0, GPIO_REGS[i].int_en);
	}

	register_int_handler(INT_GPIO_GROUP1, msm_gpio_isr, NULL);
	register_int_handler(INT_GPIO_GROUP2, msm_gpio_isr, NULL);

	unmask_interrupt(INT_GPIO_GROUP1);
	unmask_interrupt(INT_GPIO_GROUP2);
	exit_critical_section();
}

void msm_gpio_deinit(void)
{
	enter_critical_section();
	for (unsigned i = 0; i < ARRAY_SIZE(GPIO_REGS); i++) {
		writel(-1, GPIO_REGS[i].int_clear);
		writel(0, GPIO_REGS[i].int_en);
	}
	
	mask_interrupt(INT_GPIO_GROUP1);
	mask_interrupt(INT_GPIO_GROUP2);
	exit_critical_section();
}

void register_gpio_int_handler(unsigned gpio, int_handler handler, void *arg)
{
	if (gpio > NR_MSM_GPIOS) {
		dprintf(CRITICAL, "%s: gpio %d is out of supported range\n", __func__, gpio);
	}
	
	enter_critical_section();
	gpio_irq_handlers[gpio].handler = handler;
	gpio_irq_handlers[gpio].arg = arg;
	exit_critical_section();
}

status_t mask_gpio_interrupt(unsigned gpio)
{
	msm_gpio_irq_mask(gpio);
	return 0;
}

status_t unmask_gpio_interrupt(unsigned gpio, enum gpio_irq_trigger detect)
{
	msm_set_gpio_irq_trigger(gpio, detect);
	msm_gpio_irq_unmask(gpio);
	return 0;
}
