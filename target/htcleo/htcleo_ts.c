/* board-htcleo-ts.c
 *
 * Copyright (c) 2010 Cotulla
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

#include <array.h>
#include <bits.h>
#include <debug.h>
#include <reg.h>
#include <target.h>
#include <malloc.h>
#include <dev/gpio.h>
#include <dev/keys.h>
#include <kernel/timer.h>
#include <kernel/thread.h>
#include <target/clock.h>
#include <platform/interrupts.h>
#include <platform/irqs.h>
#include <platform/gpio.h>
#include <platform/timer.h>
#include <msm_i2c.h>
#include <pcom.h>
#include <platform/iomap.h>
#include <string.h>
#include <target.h>
#include <compiler.h>
#include <target/board_htcleo.h>

#define TOUCH_TYPE_UNKNOWN  0
#define TOUCH_TYPE_B8       1
#define TOUCH_TYPE_68       2
#define TOUCH_TYPE_2A       3

int ts_type;

#define TYPE_2A_DEVID	(0x2A >> 1)
#define TYPE_B8_DEVID	(0xB8 >> 1)
#define TYPE_68_DEVID	(0x68 >> 1)

#define MAKEWORD(a, b)  ((uint16_t)(((uint8_t)(a)) | ((uint16_t)((uint8_t)(b))) << 8))

static struct timer poll_timer;
static wait_queue_t ts_work __UNUSED;

static int ts_i2c_read_sec(uint8_t dev, uint8_t addr, size_t count, uint8_t *buf)
{
	struct i2c_msg msg[2];
	
	msg[0].addr  = dev;
	msg[0].flags = 0;
	msg[0].len   = 1;
	msg[0].buf   = &addr;

	msg[1].addr  = dev;
	msg[1].flags = I2C_M_RD;
	msg[1].len   = count;
	msg[1].buf   = buf;
	
	if (msm_i2c_xfer(msg, 2) < 0) {
		dprintf(INFO, "TS: ts_i2c_read_sec FAILED!\n");
		return 0;
	}
	
	return 1;
}

static int ts_i2c_read_master(size_t count, uint8_t *buf)
{
	struct i2c_msg msg[1];
	
	msg[0].addr  = TYPE_2A_DEVID;
	msg[0].flags = I2C_M_RD;
	msg[0].len   = count;
	msg[0].buf   = buf;
	
	if (msm_i2c_xfer(msg, 1) < 0) {
		dprintf(INFO, "TS: ts_i2c_read_master FAILED!\n");
		return 0;
	}
	
	return 1;
}

static void htcleo_ts_reset(void)
{
	while (gpio_get(HTCLEO_GPIO_TS_POWER)) {
		gpio_set(HTCLEO_GPIO_TS_SEL, 1);
		gpio_set(HTCLEO_GPIO_TS_POWER, 0);
		gpio_set(HTCLEO_GPIO_TS_MULT, 0);
		gpio_set(HTCLEO_GPIO_H2W_CLK, 0);
		mdelay(10);
	}
	gpio_set(HTCLEO_GPIO_TS_MULT, 1);
	gpio_set(HTCLEO_GPIO_H2W_CLK, 1);

	while (!gpio_get(HTCLEO_GPIO_TS_POWER)) {
		gpio_set(HTCLEO_GPIO_TS_POWER, 1);
	}
	mdelay(20);
	while (gpio_get(HTCLEO_GPIO_TS_IRQ)) {
		mdelay(10);
	}

	while (gpio_get(HTCLEO_GPIO_TS_SEL)) {
		gpio_set(HTCLEO_GPIO_TS_SEL, 0);
	}
	mdelay(300);
	dprintf(INFO, "TS: reset done\n");
}

static void htcleo_ts_detect_type(void)
{
	uint8_t bt[4];
	/* if (ts_i2c_read_sec(TYPE_68_DEVID, 0x00, 1, bt)) {
		dprintf(INFO, "TS: DETECTED TYPE 68\n");
		ts_type = TOUCH_TYPE_68;
		return;
	}
	if (ts_i2c_read_sec(TYPE_B8_DEVID, 0x00, 1, bt)) {
		dprintf(INFO, "TS: DETECTED TYPE B8\n");
		ts_type = TOUCH_TYPE_B8;
		return;
	} */
	if (ts_i2c_read_master(4, bt) && bt[0] == 0x55 ) {
		dprintf(INFO, "TS: DETECTED TYPE 2A\n");
		ts_type = TOUCH_TYPE_2A;
		return;
	}
	ts_type = TOUCH_TYPE_UNKNOWN;
}

static void htcleo_init_ts(void)
{
	uint8_t bt[6];
	struct i2c_msg msg;
	
	bt[0] = 0xD0;
	bt[1] = 0x00;
	bt[2] = 0x01;
		
	msg.addr  = TYPE_2A_DEVID;
	msg.flags = 0;
	msg.len   = 3;
	msg.buf   = bt;
	
	msm_i2c_xfer(&msg, 1);
	dprintf(INFO, "TS: init\n");
}

void htcleo_ts_deinit(void) {
	gpio_set(HTCLEO_GPIO_TS_POWER, 0);
}

//static int htcleo_ts_work_func(void *arg)
static enum handler_return ts_poll_fn(struct timer *timer, time_t now, void *arg)
{
	uint8_t buf[9];
	uint32_t ptcount = 0;
	uint32_t ptx[2];
	uint32_t pty[2];

	if (!ts_i2c_read_master(9, buf)) {
		dprintf(INFO, "TS: ReadPos failed\n");
		goto error;
	}
	if (buf[0] != 0x5A) {
		dprintf(INFO, "TS: ReadPos wrmark\n");
		goto error;
	}
	ptcount = (buf[8] >> 1) & 3;
	if (ptcount > 2) {
		ptcount = 2;
	}
	if (ptcount >= 1) {
		ptx[0] = MAKEWORD(buf[2], (buf[1] & 0xF0) >> 4);
		pty[0] = MAKEWORD(buf[3], (buf[1] & 0x0F) >> 0);
	}
	if (ptcount == 2) {
		ptx[1] = MAKEWORD(buf[5], (buf[4] & 0xF0) >> 4);
		pty[1] = MAKEWORD(buf[6], (buf[4] & 0x0F) >> 0);
	}
#if 1 //Set to 0 if ts driver is working(check splash.h too for virtual buttons logo)
	if (ptcount == 0) dprintf(INFO, "TS: not pressed\n");
	else if (ptcount == 1) dprintf(INFO, "TS: pressed1 (%d, %d)\n", ptx[0], pty[0]);
	else if (ptcount == 2) dprintf(INFO, "TS: pressed2 (%d, %d) (%d, %d)\n", ptx[0], pty[0], ptx[1], pty[1]);
	else dprintf(INFO, "TS: BUGGY!\n");
#else
	/*
	 * ...WIP...
	 * koko: Catch on screen buttons events.
	 *		 According to the screen area touched
	 *		 set the corresponding key_code
	 */
	unsigned key_code = 0;
	if (ptcount == 1) {
		if (pty[0] > 723) {
				 if ((ptx[0] < 90)) 					key_code = KEY_HOME;
			else if ((ptx[0] > 90) && (ptx[0] < 180))	key_code = KEY_VOLUMEUP;
			else if ((ptx[0] >180) && (ptx[0] < 300))	key_code = KEY_SEND;
			else if ((ptx[0] >300) && (ptx[0] < 390))	key_code = KEY_VOLUMEDOWN;
			else if ((ptx[0] >390))						key_code = KEY_BACK;
			else 										key_code = 0;
		}
		if (key_code)
			keys_set_state(key_code);
	}
#endif
	enter_critical_section();
	timer_set_oneshot(timer, 125, ts_poll_fn, NULL);
	exit_critical_section();

error:
	enter_critical_section();
	mask_interrupt(gpio_to_irq(HTCLEO_GPIO_TS_IRQ));
	exit_critical_section();
	return INT_RESCHEDULE;
}
/* 
static enum handler_return htcleo_ts_irq_handler(void *arg)
{
	//zzz
	enter_critical_section();
	unmask_interrupt(gpio_to_irq(HTCLEO_GPIO_TS_IRQ));
	wait_queue_wake_one(&ts_work, 1, 0);
	exit_critical_section();
	
	return INT_RESCHEDULE;
}
 */
static uint32_t touch_on_gpio_table[] =
{
	MSM_GPIO_CFG(HTCLEO_GPIO_TS_IRQ, 0, PCOM_GPIO_CFG_INPUT, PCOM_GPIO_CFG_PULL_UP, PCOM_GPIO_CFG_8MA),
};

int htcleo_ts_probe(void)
{
	//wait_queue_init(&ts_work);
	
	config_gpio_table(touch_on_gpio_table, ARRAY_SIZE(touch_on_gpio_table));
	gpio_set(HTCLEO_GPIO_TS_SEL, 1);
	gpio_set(HTCLEO_GPIO_TS_POWER, 0);
	gpio_set(HTCLEO_GPIO_TS_MULT, 0);
	gpio_set(HTCLEO_GPIO_H2W_CLK, 0);
	gpio_config(HTCLEO_GPIO_TS_IRQ, GPIO_INPUT);

	mdelay(100);

	htcleo_ts_reset();
	htcleo_ts_detect_type();
	if (ts_type == TOUCH_TYPE_68 || ts_type == TOUCH_TYPE_B8) {
		dprintf(INFO, "TS: NOT SUPPORTED\n");
		goto error;
	} else
	if (ts_type == TOUCH_TYPE_UNKNOWN) {
		dprintf(INFO, "TS: NOT DETECTED\n");
		goto error;
	}
	htcleo_init_ts();

	enter_critical_section();
	//register_int_handler(gpio_to_irq(HTCLEO_GPIO_TS_IRQ), htcleo_ts_irq_handler, NULL);
	timer_initialize(&poll_timer);
	timer_set_oneshot(&poll_timer, 0, ts_poll_fn, NULL);
	exit_critical_section();
	
	return 1;
	
error:
	htcleo_ts_deinit();
	//wait_queue_destroy(&ts_work);
	return 0;
}
